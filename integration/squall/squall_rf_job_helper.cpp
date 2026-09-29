/* Task 033D TEST-ONLY Squall RF job activation helper.
 *
 * Pinned Squall drops RF ProductRx data unless an RX job is active, and the
 * production facade deliberately implements no RF C2 or Job API. This helper
 * activates one RX job through the EXACT pinned RF MEL C++ interfaces
 * (AdminMEL commandState(OperateRxOnly); C2MEL requestVirtualAperture,
 * requestJob, finalize), mirroring the supported sequence in pinned Squall's
 * tests/mel-boundary-e2e/mel_data_consumer.cpp.
 *
 * Compiled ONLY by integration/squall/run-rf-rx.sh against the pinned
 * checkout headers, into its own shared object. Never compiled into
 * ams_mel_c, never exposed through the public C ABI, never installed, never
 * in ordinary CTest or hosted CI. The ProductRxEndpoint and every receive
 * assertion go through the production C facade. */
#include <rfmel/admin/AdminMEL.h>
#include <rfmel/admin/StatusControl.h>
#include <rfmel/admin/UCI_Control.h>
#include <rfmel/c2/C2MEL.h>
#include <rfmel/c2/ElementGroupCommand.h>
#include <rfmel/c2/JobDetail.h>
#include <rfmel/c2/JobRequest.h>
#include <rfmel/c2/VirtualAperture.h>

#include <dlfcn.h>

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace rfmel = ams::iface::rfmel;

class RxElementGroupCommand final : public rfmel::ElementGroupCommand {
public:
    rfmel::Mode getMode() const override { return rfmel::Mode::RX; }
    std::string getElementGroupLabel() const override { return "0"; }
    std::vector<rfmel::FrequencyRange> getExpectedCenterFrequencies() const override { return {}; }
    std::vector<rfmel::FrequencyRange>& getRefExpectedCenterFrequencies() override
    { return frequencies_; }
    rfmel::TxPowerLevel getTxPower() const override { return 0; }
    rfmel::DutyFactor getDesiredDutyFactor() const override { return 1.0; }
    rfmel::DataPipeConnections getEndpointIDs() const override { return {}; }
    rfmel::DataPipeConnections& getRefEndpointIDs() override { return connections_; }
    void addExpectedCenterFrequencies(rfmel::FrequencyRange) override {}
    void setTxPower(rfmel::TxPowerLevel) override {}
    void setDesiredDutyFactor(rfmel::DutyFactor) override {}
    void addEndpointIDs(const std::set<rfmel::EndpointID>&, rfmel::DataPipeLabel) override {}
    std::vector<rfmel::PointingType> getExpectedPointingAngles() override { return {}; }
    void addExpectedPointingAngle(rfmel::PointingType const&) override {}
    std::vector<rfmel::PointingType>& getRefExpectedPointingAngles() override { return pointing_; }

private:
    std::vector<rfmel::FrequencyRange> frequencies_;
    rfmel::DataPipeConnections connections_;
    std::vector<rfmel::PointingType> pointing_;
};

/* Harness-owned job graph. The provider handle is the harness's own dlopen
 * reference and is never released (the job graph's code lives there). */
struct JobGraph {
    void *provider{};
    std::shared_ptr<rfmel::AdminMEL> admin;
    std::shared_ptr<rfmel::C2MEL> c2;
    std::shared_ptr<rfmel::VirtualAperture> aperture;
    std::shared_ptr<rfmel::JobDetail> job;
};
JobGraph *graph = nullptr;

int fail(char *error, std::size_t capacity, const std::string& text)
{
    if (error != nullptr && capacity != 0U) std::snprintf(error, capacity, "%s", text.c_str());
    return 0;
}

template<typename Function>
Function resolve(void *handle, const char *name)
{
    void *address = dlsym(handle, name);
    if (address == nullptr) throw std::runtime_error(std::string{"missing symbol "} + name);
    Function function{};
    static_assert(sizeof function == sizeof address);
    std::memcpy(&function, &address, sizeof function);
    return function;
}

} // namespace

/* Returns 1 when an RX job is active. */
extern "C" __attribute__((visibility("default"))) int squall_rf_test_job_start(
    const char *provider, const char *profile, char *error, std::size_t capacity)
{
    if (graph != nullptr) return fail(error, capacity, "job already started");
    auto owned = std::make_unique<JobGraph>();
    owned->provider = dlopen(provider, RTLD_NOW | RTLD_LOCAL);
    if (owned->provider == nullptr) return fail(error, capacity, dlerror());
    try {
        using CreateAdmin = std::shared_ptr<rfmel::AdminMEL> (*)(std::string_view);
        using CreateC2 = std::shared_ptr<rfmel::C2MEL> (*)(std::string_view);
        owned->admin = resolve<CreateAdmin>(owned->provider, "createAdminMEL")(profile);
        if (!owned->admin || !owned->admin->getUCIControl() ||
            !owned->admin->getUCIControl()->getStatusControl())
            return fail(error, capacity, "AdminMEL status control unavailable");
        if (!owned->admin->getUCIControl()->getStatusControl()->commandState(
                ams::iface::mel::MFA_State::OperateRxOnly))
            return fail(error, capacity, "commandState(OperateRxOnly) failed");
        owned->c2 = resolve<CreateC2>(owned->provider, "createC2MEL")(profile);
        if (!owned->c2) return fail(error, capacity, "createC2MEL returned null");
        auto aperture = owned->c2->requestVirtualAperture(0, 1, {}, "", {}).get();
        if (!aperture) return fail(error, capacity, aperture.getError().getDescription());
        owned->aperture = aperture.get();
        rfmel::JobRequest request;
        request.addElementGroup(std::make_shared<RxElementGroupCommand>());
        auto job = owned->aperture->requestJob(request).get();
        if (!job) return fail(error, capacity, job.getError().getDescription());
        owned->job = job.get();
        (void)owned->job->finalize(); /* pinned Squall: sets the job active */
    } catch (const std::exception& exception) {
        return fail(error, capacity, exception.what());
    }
    graph = owned.release();
    return 1;
}

/* Cancels the job and drops the harness job graph; the harness's own provider
 * dlopen reference is intentionally kept for the process lifetime. */
extern "C" __attribute__((visibility("default"))) void squall_rf_test_job_stop(void)
{
    if (graph == nullptr) return;
    try {
        if (graph->job) (void)graph->job->cancelJob();
    } catch (...) {
    }
    graph->job.reset();
    graph->aperture.reset();
    graph->c2.reset();
    graph->admin.reset();
}
