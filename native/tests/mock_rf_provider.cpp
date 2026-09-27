/* Task 033B deterministic mock RF MEL provider. It exports exactly
 * createDataMEL plus TEST-only observation functions, and is deliberately
 * separate from the IR mock provider.
 *
 * The configuration string passed to createDataMEL selects the scenario. The
 * lifetime log (AMS_MEL_TEST_LIFETIME_LOG, opened without O_CREAT) records:
 *   rf_factory_called, rf_shutdown, rf_mfa_info_destroyed, rf_data_destroyed,
 *   library_unloaded, and rf_forbidden_call / rf_call_after_shutdown /
 *   rf_unknown_face for any out-of-scope or invalid provider use. */
#include <rfmel/data/DataMEL.h>
#include <rfmel/factory/RFCreateFunctions.h>
#include <rfmel/mfa/RFMFAInfo.h>

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <future>
#include <limits>
#include <memory>
#include <new>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

namespace {

namespace mel = ams::iface::mel;
namespace rfmel = ams::iface::rfmel;
using rfmel::FaceID;
using rfmel::FrequencyRange;
using rfmel::JobDataFormat;
using Femtoseconds = ams::util::math::Femtoseconds;

std::atomic<unsigned> shutdown_calls{};
std::atomic<unsigned> forbidden_calls{};
std::atomic<unsigned> getter_calls{};

void record(const char *event) noexcept
{
    const char *path = std::getenv("AMS_MEL_TEST_LIFETIME_LOG");
    if (path == nullptr) return;
    const int fd = open(path, O_WRONLY | O_APPEND | O_CLOEXEC);
    if (fd < 0) return;
    char line[128];
    const std::size_t size = std::strlen(event);
    if (size + 1U <= sizeof line) {
        std::memcpy(line, event, size);
        line[size] = '\n';
        ssize_t written;
        do { written = write(fd, line, size + 1U); }
        while (written < 0 && errno == EINTR);
    }
    (void)close(fd);
}

[[noreturn]] void forbidden(const char *operation)
{
    forbidden_calls.fetch_add(1U);
    record("rf_forbidden_call");
    throw std::logic_error(std::string{"Task 033B must not call "} + operation);
}

/* Per-DataMEL scenario state, shared with its RFMFAInfo. */
struct Scenario {
    std::string name;
    bool shut_down{};
    bool mfa_throw_armed{};
};

/* A provider-defined JobDataFormat value that upstream does not publish yet.
 * JobDataFormat is a scoped enum with the fixed underlying type int, so every
 * int value is a valid enumerator value; C++20 integral conversion of
 * 0xF0000001 to int is modular (-268435455). */
constexpr JobDataFormat future_format =
    static_cast<JobDataFormat>(static_cast<int>(0xF0000001U));

class MockRFMFAInfo final : public rfmel::RFMFAInfo {
public:
    explicit MockRFMFAInfo(std::shared_ptr<Scenario> scenario)
        : scenario_{std::move(scenario)} {}
    ~MockRFMFAInfo() override { record("rf_mfa_info_destroyed"); }

    Femtoseconds getAGCProcessingTime(FaceID face) const override
    { return Femtoseconds{face_value(face, 1001, 0)}; }
    std::vector<FrequencyRange> getRxFrequencyRanges(FaceID face) const override
    {
        check_face(face);
        if (face == 7U)
            return {FrequencyRange{100250000.125, 200500000.5},
                    FrequencyRange{1500000000.25, 2750000000.75}};
        return {};
    }
    std::vector<FrequencyRange> getTxFrequencyRanges(FaceID face) const override
    {
        check_face(face);
        if (face == 42U) return {FrequencyRange{433125000.5, 434875000.25}};
        return {};
    }
    bool requiresEndpointAssociation(FaceID face) const override
    { check_face(face); return face == 7U; }
    bool containsOpenAdditions() const override { enter(); return true; }
    bool supportsTransmit(FaceID face) const override
    { check_face(face); return face == 42U; }
    bool supportsReceive(FaceID face) const override
    { check_face(face); return face == 7U; }
    std::size_t getNumFaces() const override
    {
        enter();
        return scenario_->name == "inconsistent-faces" ? 5U : 2U;
    }
    Femtoseconds schedulerResolution() const override
    { enter(); return Femtoseconds{12345}; }
    Femtoseconds quantizeDuration(Femtoseconds) const override
    { forbidden("RFMFAInfo::quantizeDuration"); }
    Femtoseconds minJobRequestLeadTime(FaceID face) const override
    { return Femtoseconds{face_value(face, 1002, 42001)}; }
    Femtoseconds maxJobRequestLeadTime(FaceID face) const override
    {
        return Femtoseconds{face_value(face, INT64_C(1003000000000000),
                                       std::numeric_limits<std::int64_t>::max())};
    }
    Femtoseconds minJobDetailLeadTime(FaceID face) const override
    { return Femtoseconds{face_value(face, 1004, 42003)}; }
    Femtoseconds txRxSwitchingTime(FaceID face) const override
    { return Femtoseconds{face_value(face, 1005, 42004)}; }
    Femtoseconds rxTxSwitchingTime(FaceID face) const override
    { return Femtoseconds{face_value(face, 1006, 42005)}; }
    Femtoseconds txTxSwitchingTime(FaceID face) const override
    { return Femtoseconds{face_value(face, 1007, 42006)}; }
    Femtoseconds rxRxSwitchingTime(FaceID face) const override
    { return Femtoseconds{face_value(face, 1008, -42007)}; }

    std::vector<FrequencyRange> getSampleFrequencyRange(FaceID face) const override
    {
        check_face(face);
        if (face == 42U && scenario_->mfa_throw_armed) {
            /* Thrown after face 7 was completely copied, so the bridge must
             * discard a partial snapshot. One-shot: a later snapshot on the
             * same DataMEL succeeds. */
            scenario_->mfa_throw_armed = false;
            if (scenario_->name == "mfa-throw-unknown-once") throw 42;
            throw std::runtime_error("mock RFMFAInfo exception");
        }
        if (face == 7U) return {FrequencyRange{1024000.5, 2048000.25}};
        /* Deliberately unordered and inverted: the bridge never normalizes. */
        return {FrequencyRange{3000000.75, 1000000.125}, FrequencyRange{0.5, 0.25}};
    }
    std::size_t getMaxNumUserDefinedContextBytes() const override
    { enter(); return 9876U; }
    std::set<JobDataFormat> getSupportedDataFormats() const override
    {
        enter();
        std::set<JobDataFormat> formats{JobDataFormat::LFType3, JobDataFormat::DirectINT8,
                                        JobDataFormat::AMSVitaLarge,
                                        JobDataFormat::ComplexINT16};
        if (scenario_->name == "future-format") formats.insert(future_format);
        return formats;
    }
    const rfmel::PhysicalData& getPhysicalData(FaceID) const override
    { forbidden("RFMFAInfo::getPhysicalData"); }
    const std::vector<rfmel::TxPowerModeData>& getTxPowerModeCharacteristics(
        FaceID) const override
    { forbidden("RFMFAInfo::getTxPowerModeCharacteristics(face)"); }
    const rfmel::TxPowerModeData& getTxPowerModeCharacteristics(
        rfmel::TxPowerModeID, FaceID) const override
    { forbidden("RFMFAInfo::getTxPowerModeCharacteristics(mode, face)"); }
    /* Non-contiguous, inserted out of order: std::set order is {7, 42}. */
    std::set<FaceID> getFaceIDs() const override
    { enter(); return {42U, 7U}; }

private:
    void enter() const
    {
        getter_calls.fetch_add(1U);
        if (scenario_->shut_down) record("rf_call_after_shutdown");
    }
    void check_face(FaceID face) const
    {
        enter();
        if (face != 7U && face != 42U) {
            record("rf_unknown_face");
            throw std::out_of_range("mock RF face ID not reported by getFaceIDs");
        }
    }
    std::int64_t face_value(FaceID face, std::int64_t seven, std::int64_t forty_two) const
    {
        check_face(face);
        return face == 7U ? seven : forty_two;
    }

    std::shared_ptr<Scenario> scenario_;
};

class MockDataMEL final : public rfmel::DataMEL {
public:
    explicit MockDataMEL(std::shared_ptr<Scenario> scenario)
        : scenario_{std::move(scenario)}, info_{scenario_}
    {
        scenario_->mfa_throw_armed = scenario_->name == "mfa-throw-once" ||
                                     scenario_->name == "mfa-throw-unknown-once";
    }
    /* info_ (a member) is destroyed after this body runs, so the log order is
     * rf_data_destroyed then rf_mfa_info_destroyed. */
    ~MockDataMEL() override { record("rf_data_destroyed"); }

    mel::RequestFor<rfmel::ProductRxEndpoint> createProductRxEndpoint(
        JobDataFormat, std::size_t, char *) override
    { forbidden("DataMEL::createProductRxEndpoint"); }
    mel::RequestFor<rfmel::ExternalEndpoint> registerExternalRxEndpoint(
        JobDataFormat, const rfmel::RDMAExternalEndpointParams&) override
    { forbidden("DataMEL::registerExternalRxEndpoint"); }

    const rfmel::RFMFAInfo& getRFMFAInfo() const override
    {
        if (scenario_->shut_down) record("rf_call_after_shutdown");
        if (scenario_->name == "mfa-reference-throw")
            throw std::runtime_error("mock getRFMFAInfo exception");
        return info_;
    }
    mel::VersionInfo getVersionInfo() const override
    {
        if (scenario_->shut_down) record("rf_call_after_shutdown");
        const std::string& name = scenario_->name;
        if (name == "version-throw") throw std::runtime_error("mock RF version exception");
        if (name == "version-throw-unknown") throw 7;
        if (name == "version-bad-alloc") throw std::bad_alloc{};
        if (name == "version-invalid-utf8")
            return {3, 4, std::string{"bad\xC3\x28", 5}, "unchanged"};
        if (name == "version-nul")
            return {3, 4, "vendor", std::string{"bad\0description", 15}};
        if (name == "version-long")
            return {UINT32_C(0xfedcba98), UINT32_C(0x01234567),
                    std::string(300U, 'v') + "\xE2\x82\xAC",
                    std::string(1000U, 'd') + "\xF0\x9F\x93\xA1"};
        return {UINT32_C(0x0000A5A5), UINT32_C(0x5A5A0000), "Mock RF \xC2\xB5Vendor",
                "Deterministic Task 033B RF DataMEL"};
    }
    void shutdown() override
    {
        shutdown_calls.fetch_add(1U);
        if (scenario_->shut_down) record("rf_shutdown_repeated");
        scenario_->shut_down = true;
        record("rf_shutdown");
        if (scenario_->name == "shutdown-throw")
            throw std::runtime_error("mock RF shutdown exception");
        if (scenario_->name == "shutdown-throw-unknown") throw 99;
    }

private:
    std::shared_ptr<Scenario> scenario_;
    MockRFMFAInfo info_;
};

struct UnloadRecorder {
    ~UnloadRecorder() { record("library_unloaded"); }
} unload_recorder;

class FactoryFailure final : public std::exception {
public:
    const char *what() const noexcept override { return "mock RF factory exception"; }
};

} // namespace

/* The one production RF symbol: C linkage, C++ signature (rfmel::fnDataMEL).
 * That shape IS the pinned provider contract, so Clang's
 * -Wreturn-type-c-linkage is inherent to it. Pinned RF MEL publishes only the
 * fnDataMEL pointer type, not a createDataMEL declaration, so this scoped
 * pragma mirrors exactly how pinned upstream IR MEL declares its own
 * C-linkage factories (CommonIR_MEL.h getAPI_Manager, Control.h getControl).
 * It covers this one declaration only. */
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
#endif
extern "C" __attribute__((visibility("default")))
std::shared_ptr<ams::iface::rfmel::DataMEL> createDataMEL(std::string_view configuration);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

std::shared_ptr<ams::iface::rfmel::DataMEL> createDataMEL(std::string_view configuration)
{
    record("rf_factory_called");
    if (configuration == "factory-null") return {};
    if (configuration == "factory-throw") throw FactoryFailure{};
    if (configuration == "factory-throw-unknown") throw 5;
    if (configuration == "factory-bad-alloc") throw std::bad_alloc{};
    auto scenario = std::make_shared<Scenario>();
    scenario->name = std::string{configuration};
    return std::make_shared<MockDataMEL>(std::move(scenario));
}

static_assert(std::is_same_v<decltype(&createDataMEL), ams::iface::rfmel::fnDataMEL>);

/* TEST-only observations; not part of any MEL interface. */
extern "C" __attribute__((visibility("default"))) unsigned mock_rf_shutdown_calls(void)
{ return shutdown_calls.load(); }
extern "C" __attribute__((visibility("default"))) unsigned mock_rf_forbidden_calls(void)
{ return forbidden_calls.load(); }
extern "C" __attribute__((visibility("default"))) unsigned mock_rf_getter_calls(void)
{ return getter_calls.load(); }
