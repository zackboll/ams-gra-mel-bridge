/* Task 033B/033D/034B1A/034B1B1 deterministic mock RF MEL provider. It exports
 * createDataMEL, createAdminMEL and createC2MEL plus TEST-only observation/control functions, and is
 * deliberately separate from the IR mock provider.
 *
 * Task 033D adds a CONCRETE ProductRxEndpoint. Being concrete, it must define
 * the pure virtual getRDMAMemoryRegionParams(), whose return type the
 * consumer closure only forward-declares; that is the sole reason this file
 * includes rfmel/endpoints/RDMAMemoryRegionParams.h (test-provider
 * implementation support; never included by production). Production must
 * never call it: any call is recorded as rf_forbidden_call.
 *
 * The configuration string passed to createDataMEL selects the scenario. The
 * lifetime log (AMS_MEL_TEST_LIFETIME_LOG, opened without O_CREAT) records:
 *   rf_factory_called, rf_shutdown, rf_mfa_info_destroyed, rf_data_destroyed,
 *   library_unloaded, and rf_forbidden_call / rf_call_after_shutdown /
 *   rf_unknown_face for any out-of-scope or invalid provider use. */
#include <rfmel/data/DataMEL.h>
#include <rfmel/admin/AdminMEL.h>
#include <rfmel/admin/StatusControl.h>
#include <rfmel/c2/C2MEL.h>
#include <rfmel/c2/JobDetail.h>
#include <rfmel/data/ProductRxEndpoint.h>
#include <rfmel/endpoints/RDMAMemoryRegionParams.h>
#include <rfmel/factory/RFCreateFunctions.h>
#include <rfmel/mfa/RFMFAInfo.h>
#include <rfmel/mfa/PhysicalData.h>

#include <any>
#include <cmath>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <future>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>
#include <variant>
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
std::atomic<unsigned> quantize_calls{};
std::atomic<unsigned> physical_calls{};
std::atomic<std::uint32_t> physical_face{};
std::atomic<unsigned> tx_collection_calls{}, tx_direct_calls{};
std::atomic<std::uint32_t> tx_face{}, tx_requested_id{};

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
    { enter(); return Femtoseconds{scenario_->name == "quantize" ? 10 : 12345}; }
    Femtoseconds quantizeDuration(Femtoseconds input) const override
    {
        enter();
        quantize_calls.fetch_add(1U);
        if (scenario_->name == "quantize-throw")
            throw std::runtime_error{"mock RF quantize exception"};
        if (scenario_->name == "quantize-unknown") throw 42;
        if (scenario_->name == "quantize-alloc") throw std::bad_alloc{};
        // Provider-only rule: signed integer division truncates toward zero.
        return Femtoseconds{(input.count() / 10) * 10};
    }
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
    const rfmel::PhysicalData& getPhysicalData(FaceID face) const override
    {
        enter();
        physical_calls.fetch_add(1U);
        physical_face.store(face);
        const auto& name = scenario_->name;
        if (name == "physical-throw") throw std::runtime_error("mock PhysicalData exception");
        if (name == "physical-unknown") throw 42;
        if (name == "physical-alloc") throw std::bad_alloc{};
        const bool second = name == "physical-changing" && physical_generation_++ != 0U;
        std::string key = second ? "second-bay-µ-18" : "bay-µ-17";
        std::string system = second ? "second/β-installation" : "mock/β-installation";
        if (name == "physical-key-utf8") key = "\xff";
        if (name == "physical-key-nul") key = std::string{"a\0b", 3};
        if (name == "physical-system-utf8") system = "\xff";
        if (name == "physical-system-nul") system = std::string{"a\0b", 3};
        if (name == "physical-empty") { key.clear(); system.clear(); }
        const double delta = second ? 1.0 : 0.0;
        mel::ForeignKey id{key, system};
        physical_.setAntennaHeight(1.25 + delta);
        physical_.setAntennaWidth(2.5 + delta);
        physical_.setLatticeAngle(-0.375 + delta);
        physical_.setInstallationDetails(mel::InstallationDetails{
            mel::ComponentLocation{10.125 + delta, -20.25 + delta, 30.5 + delta, id},
            mel::Euler{0.125 + delta, -0.25 + delta, 0.5 + delta},
            mel::Euler{-0.75 + delta, 1.0 + delta, -1.25 + delta}});
        if (name == "physical-special") {
            physical_.setAntennaHeight(-0.0);
            physical_.setAntennaWidth(std::numeric_limits<double>::infinity());
            physical_.setLatticeAngle(std::numeric_limits<double>::quiet_NaN());
        }
        return physical_;
    }
    const std::vector<rfmel::TxPowerModeData>& getTxPowerModeCharacteristics(
        FaceID face) const override
    {
        enter();
        tx_collection_calls.fetch_add(1U);
        tx_face.store(face);
        tx_failure();
        modes_.clear();
        if (scenario_->name == "tx-empty") return modes_;
        const bool changed = scenario_->name == "tx-changing" && tx_generation_++ != 0U;
        modes_.push_back(make_mode(false, changed));
        modes_.push_back(make_mode(true, changed));
        return modes_;
    }
    const rfmel::TxPowerModeData& getTxPowerModeCharacteristics(
        rfmel::TxPowerModeID id, FaceID face) const override
    {
        enter();
        tx_direct_calls.fetch_add(1U);
        tx_face.store(face);
        tx_requested_id.store(id);
        tx_failure();
        direct_mode_ = make_mode(false, false);
        if (scenario_->name != "tx-mismatch") direct_mode_.setTxPowerModeID(id);
        return direct_mode_;
    }
    /* Non-contiguous, inserted out of order: std::set order is {7, 42}. */
    std::set<FaceID> getFaceIDs() const override
    { enter(); return {42U, 7U}; }

private:
    void tx_failure() const
    {
        if (scenario_->name == "tx-throw") throw std::runtime_error("mock TxPowerModeData exception");
        if (scenario_->name == "tx-unknown") throw 42;
        if (scenario_->name == "tx-alloc") throw std::bad_alloc{};
    }
    rfmel::TxPowerModeData make_mode(bool second, bool changed) const
    {
        rfmel::TxPowerModeData mode;
        mode.setTxPowerModeID(second ? UINT32_MAX : UINT32_C(0x80000001));
        mode.setIsLinearOperation(!second);
        mode.setTxPowerLevel(second ? 0U : UINT32_C(0xF0E1D2C3));
        const double delta = changed ? 10.0 : 0.0;
        if (!second) mode.setTxFrequencyRanges({
            FrequencyRange{1000000.25 + delta, 2000000.5 + delta},
            FrequencyRange{987654321.125 + delta, 987654322.875 + delta}});
        mode.setMaxTxDutyFactor((second ? 0.375 : 0.625) + delta);
        mode.setMaxTxPulseWidth(std::chrono::nanoseconds{second ? INT64_C(9876543210) : INT64_C(-123456789)});
        mode.setMaxTxAtten(second ? 12.25 : 63.5);
        mode.setTxAttenStepSize(second ? 0.5 : 0.125);
        if (scenario_->name == "tx-special") {
            mode.setMaxTxDutyFactor(-0.0);
            mode.setMaxTxAtten(std::numeric_limits<double>::infinity());
            mode.setTxAttenStepSize(std::numeric_limits<double>::quiet_NaN());
        }
        return mode;
    }
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
    mutable rfmel::PhysicalData physical_;
    mutable unsigned physical_generation_{};
    mutable unsigned tx_generation_{};
    mutable std::vector<rfmel::TxPowerModeData> modes_;
    mutable rfmel::TxPowerModeData direct_mode_;
};

/* ---------------------------------------------------------------------------
 * Task 033D ProductRxEndpoint mock.
 *
 * createDataMEL configuration "rx:<create>[:<callback-mode>]" selects:
 *   create: ok, error-known, error-long, error-unknown-code, invalid-future,
 *     future-throw, sync-throw, delayed (released by mock_rf_rx_release),
 *     never, format-mismatch, null-endpoint, getter-throw
 *   callback-mode (how setDataReadyCallback retains the callback):
 *     copy (default); reference (a pointer to the EXACT lvalue only); move
 *     (move-from, leaving the caller's object empty); throw-before (throws,
 *     retaining nothing); throw-after (retains a REFERENCE, then throws);
 *     sync (copies, then invokes synchronously before returning).
 * -------------------------------------------------------------------------*/

using DataReadyCallback =
    std::function<void(std::shared_ptr<rfmel::ProductRxMetadata>, rfmel::JobDataPointer,
                       std::size_t)>;
using ComplexI16 = rfmel::MELComplex<std::int16_t>;
using CreateResult = mel::ErrorOr<std::shared_ptr<rfmel::ProductRxEndpoint>>;

std::atomic<unsigned> rdma_calls{};
std::atomic<unsigned> registrations{};
std::atomic<unsigned> endpoints_destroyed{};
std::atomic<unsigned> creates{};
std::atomic<std::uint64_t> next_endpoint_id{UINT64_C(0x8000000000000011)};

/* How the provider holds a registered callback. Deliberately never freed so
 * it can outlive its endpoint (late-callback tests). */
struct RetainedCallback {
    DataReadyCallback copy;
    DataReadyCallback *reference{};
};

std::shared_ptr<rfmel::ProductRxMetadata> rich_metadata()
{
    auto metadata = std::make_shared<rfmel::ProductRxMetadata>();
    metadata->setMelProtocolVersionID(UINT32_C(0xFEDCBA98));
    metadata->setVaDefinitionID(UINT32_C(0x80000001));
    metadata->setVaInstanceID(UINT32_C(0x7FFFFFFE));
    metadata->setJobDetailsID(UINT32_C(0xDEADBEEF));
    metadata->setJobIntervalID(UINT32_C(0x00010002));
    metadata->setLfTypeID(UINT32_C(0xFFFFFFFF));
    metadata->setLfInstanceID(UINT32_C(0x12345678));
    metadata->setPhaseCoherenceWithPrior(true);
    /* UTCTime's constructor normalizes; a negative integral with a positive
     * large fractional stays verbatim after normalization. */
    metadata->setFirstReceiveEventStart(ams::util::math::UTCTime{
        std::chrono::seconds{INT64_C(-4102444801)},
        Femtoseconds{INT64_C(987654321098765)}});
    /* Pinned upstream names the rxStreamIDs setter setReceiveEvents (an
     * overload taking std::vector<StreamID>). */
    metadata->setReceiveEvents(std::vector<rfmel::StreamID>{
        UINT32_C(0xFFFFFFFF), UINT32_C(0), UINT32_C(0x80000000), UINT32_C(42)});
    return metadata;
}

/* ENDPOINT-OWNED sample storage (modes "owned" and "throw-active"): one
 * mmap() page holding constructed MELComplex<int16_t> objects, owned by the
 * MockProductRxEndpoint. The destructor revokes it with PROT_NONE, so any
 * read after endpoint destruction faults deterministically (no reliance on
 * heap reuse). The mapping itself is intentionally never unmapped: the
 * tests are process-isolated. */
constexpr std::size_t owned_page_size = 4096U;
constexpr std::size_t owned_samples = 6U;

void fill_rich(ComplexI16 *samples) noexcept
{
    const std::int16_t values[owned_samples][2] = {{10, 20}, {30, 40}, {-5, 6},
        {INT16_MIN, INT16_MAX}, {INT16_MAX, INT16_MIN}, {-1, 0}};
    for (std::size_t index = 0; index < owned_samples; ++index)
        samples[index] = ComplexI16{values[index][0], values[index][1]};
}

class MockProductRxEndpoint final : public rfmel::ProductRxEndpoint {
public:
    MockProductRxEndpoint(std::uint64_t id, JobDataFormat format, std::string mode)
        : id_{id}, format_{format}, mode_{std::move(mode)}
    {
        if (mode_ == "owned" || mode_ == "throw-active") {
            void *const page = mmap(nullptr, owned_page_size, PROT_READ | PROT_WRITE,
                                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (page == MAP_FAILED) throw std::bad_alloc{};
            static_assert(owned_samples * sizeof(ComplexI16) <= owned_page_size);
            owned_ = static_cast<ComplexI16 *>(page);
            for (std::size_t index = 0; index < owned_samples; ++index)
                new (owned_ + index) ComplexI16{};
            fill_rich(owned_);
        }
    }
    ~MockProductRxEndpoint() override
    {
        endpoints_destroyed.fetch_add(1U);
        if (owned_ != nullptr) {
            /* Stale reads of endpoint-owned storage now fault. */
            if (mprotect(owned_, owned_page_size, PROT_NONE) != 0) std::abort();
            record("rf_rx_endpoint_buffer_revoked");
        }
        record("rf_rx_endpoint_destroyed");
    }

    /* Endpoint-owned samples (valid only while this endpoint lives). */
    ComplexI16 *owned_samples_page() const noexcept { return owned_; }

    rfmel::EndpointID getEndpointID() const override
    {
        if (mode_ == "getter-throw") throw std::runtime_error("mock getEndpointID exception");
        return id_;
    }
    JobDataFormat getAssignedDataFormat() const override { return format_; }
    std::uint64_t id() const noexcept { return id_; }
    rfmel::RDMAMemoryRegionParams getRDMAMemoryRegionParams() const override
    {
        rdma_calls.fetch_add(1U);
        forbidden("ProductRxEndpoint::getRDMAMemoryRegionParams");
    }

    void setDataReadyCallback(DataReadyCallback& callback) override;

    /* Invokes the retained callback on the calling thread (serialized per
     * endpoint only by the caller; the mock never serializes callbacks). */
    void invoke(std::shared_ptr<rfmel::ProductRxMetadata> metadata, rfmel::JobDataPointer data,
                std::size_t count)
    {
        std::shared_ptr<RetainedCallback> held;
        {
            std::lock_guard lock{mutex_};
            held = retained_;
        }
        if (!held) return;
        DataReadyCallback& target = held->reference ? *held->reference : held->copy;
        target(std::move(metadata), data, count);
    }

    std::shared_ptr<RetainedCallback> retained() const
    {
        std::lock_guard lock{mutex_};
        return retained_;
    }

private:
    std::uint64_t id_;
    JobDataFormat format_;
    std::string mode_;
    ComplexI16 *owned_{};
    mutable std::mutex mutex_;
    std::shared_ptr<RetainedCallback> retained_;
};

/* Live endpoints by ID (weak) for the test emitters. */
std::mutex endpoints_mutex;
std::map<std::uint64_t, std::weak_ptr<MockProductRxEndpoint>> *live_endpoints =
    new std::map<std::uint64_t, std::weak_ptr<MockProductRxEndpoint>>;

/* Every provider-held callback by endpoint ID. Leaked on purpose: a provider
 * may keep a callback after the endpoint is gone (late-callback tests). */
std::mutex retained_mutex;
std::map<std::uint64_t, std::shared_ptr<RetainedCallback>> *retained_by_id =
    new std::map<std::uint64_t, std::shared_ptr<RetainedCallback>>;
std::atomic<std::uint64_t> last_endpoint_id{};

void remember(std::uint64_t id, const std::shared_ptr<RetainedCallback>& held)
{
    std::lock_guard lock{retained_mutex};
    (*retained_by_id)[id] = held;
}


/* "throw-active" gate: setDataReadyCallback starts a provider callback
 * thread on the endpoint-owned page, then blocks reading one byte from
 * active_gate_fd before throwing. The thread writes 'K' to active_done_fd
 * after the callback returns. */
std::atomic<int> active_gate_fd{-1};
std::atomic<int> active_done_fd{-1};

void write_byte(int fd, char byte) noexcept;

void MockProductRxEndpoint::setDataReadyCallback(DataReadyCallback& callback)
{
    registrations.fetch_add(1U);
    record("rf_rx_set_callback");
    if (mode_ == "throw-before") throw std::runtime_error("mock registration exception");
    auto held = std::make_shared<RetainedCallback>();
    if (mode_ == "reference" || mode_ == "throw-after") held->reference = &callback;
    else if (mode_ == "move") held->copy = std::move(callback);
    else held->copy = callback;
    {
        std::lock_guard lock{mutex_};
        retained_ = held;
    }
    remember(id_, held);
    if (mode_ == "throw-after")
        throw std::runtime_error("mock registration exception after retaining a reference");
    if (mode_ == "throw-active") {
        const int gate = active_gate_fd.exchange(-1);
        const int done = active_done_fd.exchange(-1);
        if (gate < 0 || done < 0) forbidden("throw-active without a configured gate");
        /* The provider thread holds ONLY the retained callback and a raw
         * pointer to endpoint-owned storage: never the endpoint itself. */
        ComplexI16 *const samples = owned_;
        std::thread{[held, samples, done]() {
            DataReadyCallback& target = held->reference ? *held->reference : held->copy;
            target(rich_metadata(), rfmel::JobDataPointer{samples}, owned_samples);
            write_byte(done, 'K');
        }}.detach();
        char byte;
        ssize_t result;
        do { result = read(gate, &byte, 1U); } while (result < 0 && errno == EINTR);
        throw std::runtime_error("mock registration exception with an active callback");
    }
    if (mode_ == "sync") {
        ComplexI16 samples[2] = {ComplexI16{77, -77}, ComplexI16{-1, 1}};
        invoke(rich_metadata(), rfmel::JobDataPointer{samples}, 2U);
        samples[0] = ComplexI16{0, 0};
    }
}

std::shared_ptr<rfmel::ProductRxEndpoint> make_endpoint(const std::string& mode,
                                                        JobDataFormat format)
{
    auto endpoint = std::make_shared<MockProductRxEndpoint>(
        next_endpoint_id.fetch_add(1U), format, mode);
    last_endpoint_id.store(endpoint->id());
    std::lock_guard lock{endpoints_mutex};
    (*live_endpoints)[endpoint->id()] = endpoint;
    return endpoint;
}

std::shared_ptr<MockProductRxEndpoint> find_endpoint(std::uint64_t id)
{
    std::lock_guard lock{endpoints_mutex};
    const auto found = live_endpoints->find(id);
    return found == live_endpoints->end() ? nullptr : found->second.lock();
}

/* Never destroyed: destroying a promise would break a future that a bridge
 * worker may still be waiting on during process exit. */
struct Pending {
    std::promise<CreateResult> promise;
    std::string mode;
};
std::mutex pending_mutex;
std::vector<Pending> *delayed_creates = new std::vector<Pending>;
std::vector<Pending> *never_creates = new std::vector<Pending>;
std::atomic<std::size_t> last_region_size{};

std::pair<std::string, std::string> split_rx(const std::string& name)
{
    const std::string rest = name.substr(3);
    const auto colon = rest.find(':');
    if (colon == std::string::npos) return {rest, "copy"};
    return {rest.substr(0, colon), rest.substr(colon + 1U)};
}

mel::RequestFor<rfmel::ProductRxEndpoint> rx_create(const std::string& name,
                                                    JobDataFormat format,
                                                    std::size_t region_size, char *region)
{
    creates.fetch_add(1U);
    record("rf_rx_create");
    if (name.rfind("rx:", 0) != 0) forbidden("DataMEL::createProductRxEndpoint");
    if (format != JobDataFormat::ComplexINT16 || region != nullptr)
        forbidden("createProductRxEndpoint other than (ComplexINT16, size, nullptr)");
    last_region_size.store(region_size);
    const auto [create, mode] = split_rx(name);
    std::promise<CreateResult> promise;
    auto future = promise.get_future();
    if (create == "invalid-future") return {};
    if (create == "sync-throw") throw std::runtime_error("mock createProductRxEndpoint exception");
    if (create == "future-throw") {
        promise.set_exception(std::make_exception_ptr(
            std::runtime_error("mock ProductRx future exception")));
    } else if (create == "error-known") {
        promise.set_value(CreateResult{mel::Error{mel::ErrorCode::InsufficientResources,
                                                  "mock ProductRx resources exhausted"}});
    } else if (create == "error-long") {
        promise.set_value(CreateResult{mel::Error{
            mel::ErrorCode::Unsupported,
            std::string(600U, 'r') + "\xE2\x82\xAC" + std::string(3U, 'z')}});
    } else if (create == "error-unknown-code") {
        promise.set_value(CreateResult{
            mel::Error{static_cast<mel::ErrorCode>(77), "mock unknown code"}});
    } else if (create == "null-endpoint") {
        promise.set_value(CreateResult{std::shared_ptr<rfmel::ProductRxEndpoint>{}});
    } else if (create == "format-mismatch") {
        promise.set_value(CreateResult{make_endpoint(mode, JobDataFormat::DirectINT16)});
    } else if (create == "getter-throw") {
        promise.set_value(CreateResult{make_endpoint("getter-throw", JobDataFormat::ComplexINT16)});
    } else if (create == "delayed" || create == "never") {
        std::lock_guard lock{pending_mutex};
        (create == "delayed" ? delayed_creates : never_creates)
            ->push_back(Pending{std::move(promise), mode});
    } else {
        promise.set_value(CreateResult{make_endpoint(mode, JobDataFormat::ComplexINT16)});
    }
    return future;
}
/* The provider-owned sample buffer reused by successive emissions. */
std::mutex buffer_mutex;
std::vector<ComplexI16> *reuse_buffer = new std::vector<ComplexI16>(64U);

/* Invokes a provider-held callback WITHOUT holding the endpoint: like a real
 * provider receive thread, only the retained callback is kept alive. */
std::atomic<unsigned> empty_invocations{};

void invoke_held(const std::shared_ptr<RetainedCallback>& held,
                 std::shared_ptr<rfmel::ProductRxMetadata> metadata, rfmel::JobDataPointer data,
                 std::size_t count)
{
    DataReadyCallback& target = held->reference ? *held->reference : held->copy;
    /* Observed only under the destructive no-exact-lvalue negative control. */
    if (!target) { empty_invocations.fetch_add(1U); return; }
    target(std::move(metadata), data, count);
}

void write_byte(int fd, char byte) noexcept
{
    ssize_t result;
    do { result = write(fd, &byte, 1U); } while (result < 0 && errno == EINTR);
}


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
        JobDataFormat format, std::size_t region_size, char *region) override
    {
        if (scenario_->shut_down) record("rf_call_after_shutdown");
        return rx_create(scenario_->name, format, region_size, region);
    }
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
        if (scenario_->name == "shutdown-throw" || scenario_->name == "rx:ok:shutdown-throw")
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

std::atomic<unsigned> admin_shutdown_calls{};
std::atomic<unsigned> admin_command_calls{};
std::atomic<std::uint32_t> admin_last_state{UINT32_MAX};

class MockStatusControl final : public rfmel::StatusControl {
public:
    explicit MockStatusControl(std::shared_ptr<Scenario> scenario) : scenario_{std::move(scenario)} {}
    bool commandState(mel::MFA_State state) override
    {
        admin_last_state.store(static_cast<std::uint32_t>(state));
        admin_command_calls.fetch_add(1U);
        record("rf_admin_command");
        if (state == mel::MFA_State::Standby) record("rf_admin_state_6");
        if (state == mel::MFA_State::OperateRxOnly) record("rf_admin_state_8");
        if (state == mel::MFA_State::Degraded) record("rf_admin_state_14");
        if (scenario_->name == "admin:command-throw")
            throw std::runtime_error("mock Admin command exception");
        return scenario_->name != "admin:reject";
    }
    bool commandErase() override { forbidden("StatusControl::commandErase"); }
    mel::MFA_Status getStatus() const override { forbidden("StatusBase::getStatus"); }
    void setCallback(std::function<void(const mel::MFA_Status &)>) override
    { forbidden("StatusBase::setCallback(MFA_Status)"); }
    mel::MFA_StatusDetailed getStatusDetailed() const override
    { forbidden("StatusBase::getStatusDetailed"); }
    void setCallback(std::function<void(const mel::MFA_StatusDetailed &)>) override
    { forbidden("StatusBase::setCallback(MFA_StatusDetailed)"); }
    rfmel::AntennaStatus getAntennaStatus() const override
    { forbidden("StatusBase::getAntennaStatus"); }
    void setCallback(std::function<void(const rfmel::AntennaStatus &)>) override
    { forbidden("StatusBase::setCallback(AntennaStatus)"); }
    mel::DiscreteStatus getDiscreteStatus() const override
    { forbidden("StatusBase::getDiscreteStatus"); }
    void setCallback(std::function<void(const mel::DiscreteStatus &)>) override
    { forbidden("StatusBase::setCallback(DiscreteStatus)"); }
    rfmel::SubsystemConfiguration getSubsystemConfiguration() const override
    { forbidden("StatusBase::getSubsystemConfiguration"); }
    void setCallback(std::function<void(const rfmel::SubsystemConfiguration &)>) override
    { forbidden("StatusBase::setCallback(SubsystemConfiguration)"); }
    mel::MFA_SecurityAuditRecord getSecurityAuditRecord() const override
    { forbidden("StatusBase::getSecurityAuditRecord"); }
    void setCallback(std::function<void(const mel::MFA_SecurityAuditRecord &)>) override
    { forbidden("StatusBase::setCallback(MFA_SecurityAuditRecord)"); }
private:
    std::shared_ptr<Scenario> scenario_;
};

class MockUCIControl final : public rfmel::UCI_Control {
public:
    explicit MockUCIControl(std::shared_ptr<Scenario> scenario) : scenario_{std::move(scenario)} {}
    std::shared_ptr<rfmel::StatusControl> getStatusControl() override
    {
        if (scenario_->name == "admin:no-status") return {};
        return std::make_shared<MockStatusControl>(scenario_);
    }
    std::shared_ptr<rfmel::BIT_Control> getBITControl() override { return {}; }
    std::shared_ptr<rfmel::CalibrationControl> getCalibrationControl() override { return {}; }
    std::shared_ptr<rfmel::SettingsControl> getSettingsControl() override { return {}; }
    std::shared_ptr<rfmel::PNT_Control> getPNTControl() override { return {}; }
    std::shared_ptr<rfmel::EMCON_Control> getEMCONControl() override { return {}; }
private:
    std::shared_ptr<Scenario> scenario_;
};

class MockAdminMEL final : public rfmel::AdminMEL {
public:
    explicit MockAdminMEL(std::shared_ptr<Scenario> scenario) : scenario_{std::move(scenario)} {}
    ~MockAdminMEL() override { record("rf_admin_destroyed"); }
    std::shared_ptr<rfmel::UCI_Control> getUCIControl() override
    {
        if (scenario_->name == "admin:no-uci") return {};
        return std::make_shared<MockUCIControl>(scenario_);
    }
    mel::RequestFor<rfmel::SubsystemConfiguration> requestSubsystemConfiguration() override
    { forbidden("AdminMEL::requestSubsystemConfiguration"); }
    mel::VersionInfo getVersionInfo() const override { return {1, 1, "mock", "Admin"}; }
    void shutdown() override
    {
        admin_shutdown_calls.fetch_add(1U);
        record("rf_admin_shutdown");
        if (scenario_->name == "admin:shutdown-throw")
            throw std::runtime_error("mock Admin shutdown exception");
    }
private:
    std::shared_ptr<Scenario> scenario_;
};

std::atomic<unsigned> c2_factory_calls{};
std::atomic<unsigned> c2_shutdown_calls{};
std::mutex va_gate_mutex;
std::vector<std::shared_ptr<std::promise<mel::ErrorOr<std::shared_ptr<rfmel::VirtualAperture>>>>> va_pending;
std::atomic<unsigned> va_get_calls{};
std::mutex job_gate_mutex;
std::vector<std::shared_ptr<std::promise<mel::ErrorOr<std::shared_ptr<rfmel::JobDetail>>>>> job_pending;
std::atomic<unsigned> job_get_calls{};
std::atomic<unsigned> job_finalize_calls{};
std::atomic<unsigned> job_cancel_calls{};
std::atomic<unsigned> job_add_calls{}, job_flush_calls{}, job_remaining_calls{};
std::mutex interval_mutex;
std::vector<rfmel::JobInterval> latest_intervals;

std::mutex finalize_gate_mutex;
std::vector<std::shared_ptr<std::promise<rfmel::JobStatus>>> finalize_pending;
std::mutex job_cleanup_mutex;
std::condition_variable job_cleanup_changed;
unsigned job_cleanup_shutdowns{};

class MockRxCommand final : public rfmel::ElementGroupCommand {
public:
    explicit MockRxCommand(std::string label, bool tx, bool throws)
        : label_{std::move(label)}, tx_{tx}, throws_{throws} {}
    rfmel::ElementGroupLabel getElementGroupLabel() const override { return label_; }
    rfmel::Mode getMode() const override
    { record("rf_job_mode_checked"); return tx_ ? rfmel::Mode::TX : rfmel::Mode::RX; }
    std::vector<rfmel::FrequencyRange> getExpectedCenterFrequencies() const override { return frequencies_; }
    std::vector<rfmel::FrequencyRange>& getRefExpectedCenterFrequencies() override { return frequencies_; }
    rfmel::TxPowerLevel getTxPower() const override { forbidden("Job::getTxPower"); }
    rfmel::DutyFactor getDesiredDutyFactor() const override { return duty_; }
    rfmel::DataPipeConnections getEndpointIDs() const override { return connections_; }
    rfmel::DataPipeConnections& getRefEndpointIDs() override { return connections_; }
    void addExpectedCenterFrequencies(rfmel::FrequencyRange range) override
    { frequencies_.push_back(range); }
    void setTxPower(rfmel::TxPowerLevel) override { forbidden("Job::setTxPower"); }
    void setDesiredDutyFactor(rfmel::DutyFactor duty) override
    { if (throws_) throw std::runtime_error("mock Job setter exception"); duty_ = duty; }
    void addEndpointIDs(const std::set<rfmel::EndpointID>& ids, rfmel::DataPipeLabel pipe) override
    { connections_.insert_or_assign(pipe, ids); }
    std::vector<rfmel::PointingType> getExpectedPointingAngles() override { return {}; }
    void addExpectedPointingAngle(const rfmel::PointingType&) override { forbidden("Job::addPointing"); }
    std::vector<rfmel::PointingType>& getRefExpectedPointingAngles() override { return pointing_; }
private:
    std::string label_;
    bool tx_, throws_;
    double duty_{1.0};
    std::vector<rfmel::FrequencyRange> frequencies_;
    rfmel::DataPipeConnections connections_;
    std::vector<rfmel::PointingType> pointing_;
};

class MockJobDetail final : public rfmel::JobDetail {
public:
    explicit MockJobDetail(bool throws, std::string scenario = {})
        : throws_{throws}, scenario_{std::move(scenario)} {}
    ~MockJobDetail() override { record("rf_job_destroyed"); }
    ams::util::math::UTCTime actualStartTime() const override
    {
        job_get_calls.fetch_add(1U);
        if (throws_) throw std::runtime_error("mock Job getter exception");
        return {std::chrono::seconds{-123456789}, Femtoseconds{999999999999999}};
    }
    const std::function<void(rfmel::JobIntervalStatus)>& getJobIntervalStatusCallback() const override
    { forbidden("Job::getJobIntervalStatusCallback"); }
    Femtoseconds totalJobDuration() const override { return Femtoseconds{7654321098765LL}; }
    rfmel::VirtualApertureInstanceID getVAInstanceID() const override { return 42; }
    rfmel::VirtualApertureDefinitionID getVADefinitionID() const override { return 0xABCDEF01U; }
    uint32_t getJobDetailsID() const override { return 0x10203040U; }
    std::future<rfmel::JobStatus> finalize() override
    {
        job_finalize_calls.fetch_add(1U);
        record("rf_job_finalize");
        if (scenario_ == "c2:finalize-throw") throw std::runtime_error("mock finalize exception");
        if (scenario_ == "c2:finalize-unknown-throw") throw 17;
        if (scenario_ == "c2:finalize-invalid") return {};
        auto gate = std::make_shared<std::promise<rfmel::JobStatus>>();
        auto future = gate->get_future();
        if (scenario_ == "c2:finalize-delayed" || scenario_ == "c2:finalize-cancel" ||
            scenario_ == "c2:finalize-abandon" || scenario_ == "c2:finalize-shutdown-throw") {
            std::lock_guard lock{finalize_gate_mutex};
            finalize_pending.push_back(std::move(gate));
        } else if (scenario_ == "c2:finalize-future-throw")
            gate->set_exception(std::make_exception_ptr(std::runtime_error("mock finalize future exception")));
        else if (scenario_ == "c2:finalize-future-unknown")
            gate->set_exception(std::make_exception_ptr(17));
        else if (scenario_ == "c2:finalize-unknown")
            gate->set_value(static_cast<rfmel::JobStatus>(77));
        else if (scenario_ == "c2:finalize-none") gate->set_value(rfmel::JobStatus::None);
        else if (scenario_ == "c2:finalize-progress") gate->set_value(rfmel::JobStatus::InProgress);
        else if (scenario_ == "c2:finalize-invalid-id") gate->set_value(rfmel::JobStatus::FailedInvalidID);
        else if (scenario_ == "c2:finalize-interrupted") gate->set_value(rfmel::JobStatus::FailedInterrupted);
        else if (scenario_ == "c2:finalize-invalid-state") gate->set_value(rfmel::JobStatus::FailedInvalidState);
        else gate->set_value(rfmel::JobStatus::Complete);
        return future;
    }
    void interval_failure(const char *operation)
    {
        const auto prefix = std::string{"c2:"} + operation;
        if (scenario_ == prefix + "-throw") throw std::runtime_error(std::string(900, 'x') + " µ end");
        if (scenario_ == prefix + "-unknown") throw 42;
        if (scenario_ == prefix + "-alloc") throw std::bad_alloc{};
    }
    void flush() override
    { ++job_flush_calls; record("rf_job_flush"); interval_failure("flush"); }
    void registerJobIntervalStatusCallback(const std::function<void(rfmel::JobIntervalStatus)>&) override
    { forbidden("Job::registerStatus"); }
    void addJobIntervals(const std::vector<rfmel::JobInterval>& intervals) override
    {
        ++job_add_calls; record("rf_job_add_intervals");
        { std::lock_guard lock{interval_mutex}; latest_intervals = intervals; }
        interval_failure("add");
    }
    void cancelRemainingJobIntervals() override
    { ++job_remaining_calls; record("rf_job_cancel_remaining"); interval_failure("remaining"); }
    rfmel::CancelStatus cancelJob() override
    {
        job_cancel_calls.fetch_add(1U);
        record("rf_job_cancel");
        if (scenario_ == "c2:cancel-throw") throw std::runtime_error("mock cancel exception");
        if (scenario_ == "c2:cancel-unknown-throw") throw 17;
        if (scenario_ == "c2:cancel-unknown")
            return rfmel::CancelStatus{static_cast<rfmel::CancelError>(77)};
        if (scenario_ == "c2:finalize-cancel" || scenario_ == "c2:finalize-shutdown-throw") {
            std::shared_ptr<std::promise<rfmel::JobStatus>> gate;
            {
                std::lock_guard lock{finalize_gate_mutex};
                if (!finalize_pending.empty()) {
                    gate = std::move(finalize_pending.back());
                    finalize_pending.pop_back();
                }
            }
            if (gate) gate->set_value(rfmel::JobStatus::Complete);
        }
        if (scenario_ == "c2:cancel-false") return rfmel::CancelStatus{rfmel::CancelError::None};
        return {};
    }
    void extendJobEvent(uint32_t, rfmel::JobEventID, Femtoseconds) override
    { forbidden("Job::extendJobEvent"); }
    std::vector<rfmel::StreamID> getRxStreamIDs(size_t group) const override
    { if (group != 0) forbidden("Job::getRxStreamIDs(nonzero)"); return {0, 3, UINT32_MAX}; }
    uint32_t getJobRequestId() const override { return 0xFEDCBA98U; }
    Femtoseconds getLookAheadTime() const override { return Femtoseconds{-12345}; }
private:
    bool throws_;
    std::string scenario_;
};

class MockVirtualAperture final : public rfmel::VirtualAperture {
public:
    explicit MockVirtualAperture(bool fail_getter, std::string scenario = {})
        : fail_getter_{fail_getter}, scenario_{std::move(scenario)} {}
    ~MockVirtualAperture() override { record("rf_va_destroyed"); }
    rfmel::VirtualApertureDefinitionID getID() const override { return 0; }
    std::size_t addStatusCallback(const std::function<void(rfmel::BaseVirtualAperture&)>&) override
    { forbidden("VA::addStatusCallback"); }
    void removeStatusCallback(std::size_t) override { forbidden("VA::removeStatusCallback"); }
    rfmel::VirtualApertureStatus getStatus() const override { forbidden("VA::getStatus"); }
    rfmel::VirtualApertureStatus getInstanceStatus(rfmel::VirtualApertureInstanceID) const override
    { forbidden("VA::getInstanceStatus"); }
    std::vector<rfmel::VirtualApertureInstanceID> getAllInstances() const override
    { forbidden("VA::getAllInstances"); }
    std::vector<rfmel::VirtualApertureInstanceID> getInstances(rfmel::FaceID) const override
    { forbidden("VA::getInstances"); }
    rfmel::VirtualApertureInstanceStatusReport getInstanceStatusReport(rfmel::VirtualApertureInstanceID) const override
    { forbidden("VA::getInstanceStatusReport"); }
    std::set<rfmel::VirtualApertureInstanceID> getVAInstanceIDs() const override
    {
        va_get_calls.fetch_add(1U);
        record("rf_va_get_ids");
        if (fail_getter_) throw std::runtime_error("mock VA getter exception");
        return {9, 0, 3};
    }
    mel::RequestFor<rfmel::JobDetail> requestJob(rfmel::JobRequest& request) override
    {
        record("rf_job_requested");
        if (scenario_ == "c2:job-submit-throw") throw std::runtime_error("mock Job submit exception");
        const auto& groups = request.getElementGroups();
        if (groups.size() != 1 || groups[0]->getMode() != rfmel::Mode::RX ||
            groups[0]->getElementGroupLabel() != "rx/µ-main" ||
            groups[0]->getDesiredDutyFactor() != 0.625 ||
            request.getRequestId() != 0xFEDCBA98U || request.getPriority() != 0x80000001U ||
            request.getPrecedenceWithinPriority() != 0x7FFFFFFEU ||
            !request.getIsInterruptable() ||
            request.getInstanceSelection() != std::vector<uint32_t>{0, 42, UINT32_MAX} ||
            request.getTxPowerModeIDs() != std::set<rfmel::TxPowerModeID>{0} ||
            request.getCapabilityId() != std::vector<uint8_t>{0} ||
            request.getActivityId() != std::vector<uint8_t>{0} ||
            request.getDuration().count() != 0 || request.getLookAheadTime().count() != 0 ||
            request.getNumJIBs() != 0 || request.getSendNextJIBatchCallback() ||
            request.getRequestRejectedCallback())
            throw std::runtime_error("Job request fields mismatched");
        const auto frequencies = groups[0]->getExpectedCenterFrequencies();
        if (frequencies.size() != 2 ||
            frequencies[0].getMinFrequency() != 1000000.25 ||
            frequencies[0].getMaxFrequency() != 2000000.5 ||
            frequencies[1].getMinFrequency() != 987654321.125 ||
            frequencies[1].getMaxFrequency() != 987654322.875)
            throw std::runtime_error("Job frequencies mismatched");
        const auto pipes = groups[0]->getEndpointIDs();
        auto it = pipes.begin();
        if (pipes.size() != 1 || it->first != "products/β" ||
            it->second != std::set<rfmel::EndpointID>{0, UINT64_C(0x8000000000000000), UINT64_MAX})
            throw std::runtime_error("Job endpoint association mismatched");
        if (scenario_ == "c2:job-invalid-future") return {};
        auto gate = std::make_shared<std::promise<mel::ErrorOr<std::shared_ptr<rfmel::JobDetail>>>>();
        auto future = gate->get_future();
        if (scenario_ == "c2:job-delayed" || scenario_ == "c2:job-shutdown-throw") {
            std::lock_guard lock{job_gate_mutex};
            job_pending.push_back(std::move(gate));
        } else if (scenario_ == "c2:job-future-throw")
            gate->set_exception(std::make_exception_ptr(std::runtime_error("mock Job future exception")));
        else if (scenario_ == "c2:job-future-unknown")
            gate->set_exception(std::make_exception_ptr(17));
        else if (scenario_ == "c2:job-failure" || scenario_ == "c2:job-long-failure" ||
                 scenario_ == "c2:job-unknown-error")
            gate->set_value(mel::ErrorOr<std::shared_ptr<rfmel::JobDetail>>{
                mel::Error{scenario_ == "c2:job-unknown-error" ? static_cast<mel::ErrorCode>(99) :
                    mel::ErrorCode::InvalidParameters, scenario_ == "c2:job-long-failure" ?
                    std::string(800, 'X') + "µ end" : "mock Job rejected"}});
        else gate->set_value(mel::ErrorOr<std::shared_ptr<rfmel::JobDetail>>{
            scenario_ == "c2:job-null" ? std::shared_ptr<rfmel::JobDetail>{} :
            std::make_shared<MockJobDetail>(scenario_ == "c2:job-getter-throw", scenario_)});
        return future;
    }
    rfmel::ElementGroupDescriptorLookupMap getElementGroups() const override
    { forbidden("VA::getElementGroups"); }
    std::vector<rfmel::ElementGroupLabel> getElementGroupLabels() const override
    { record("rf_va_get_labels"); return {"group/β", "", "0"}; }
    rfmel::ElementGroupConnections getDataPipes() override { forbidden("VA::getDataPipes"); }
    std::shared_ptr<rfmel::ElementGroupCommand> createElementGroupCommand(
        std::shared_ptr<rfmel::ElementGroupDescriptor>) override
    { forbidden("VA::createElementGroupCommand(descriptor)"); }
    std::shared_ptr<rfmel::ElementGroupCommand> createElementGroupCommand(rfmel::ElementGroupLabel label) override
    {
        record("rf_job_command_created");
        if (scenario_ == "c2:job-command-throw") throw std::runtime_error("mock Job command exception");
        if (scenario_ == "c2:job-command-null") return {};
        return std::make_shared<MockRxCommand>(std::move(label), scenario_ == "c2:job-command-tx",
                                               scenario_ == "c2:job-setter-throw");
    }
    bool isCachedWaveformSupported() const override { forbidden("VA::isCachedWaveformSupported"); }
    std::shared_ptr<rfmel::Weights> getStaticWeights(const std::string&) const override
    { forbidden("VA::getStaticWeights"); }
    bool dynamicWeightsSupported() const override { forbidden("VA::dynamicWeightsSupported"); }
    std::shared_ptr<rfmel::Weights> createWeights(const std::string&, rfmel::WeightType) override
    { forbidden("VA::createWeights"); }
    bool isSingleGroup() const override { record("rf_va_is_single"); return true; }
    double getTxRadiatedPower(std::size_t, rfmel::TxPowerModeID, double, rfmel::WeightType,
        double, rfmel::AnglePair, rfmel::VirtualApertureInstanceID) const override
    { forbidden("VA::getTxRadiatedPower"); }
    double getTxPeakRadiatedPower(std::size_t, rfmel::TxPowerModeID, double, double,
        rfmel::VirtualApertureInstanceID) const override
    { forbidden("VA::getTxPeakRadiatedPower"); }
    double getTxApertureGain(std::size_t, rfmel::TxPowerModeID, rfmel::WeightType, double,
        rfmel::AnglePair, rfmel::VirtualApertureInstanceID) const override
    { forbidden("VA::getTxApertureGain"); }
    double getMaxTxAttenuation(std::size_t, rfmel::TxPowerModeID, rfmel::VirtualApertureInstanceID) const override
    { forbidden("VA::getMaxTxAttenuation"); }
    std::map<rfmel::LocalFunctionTypeID, std::size_t> getLocalFunctions() const override
    { forbidden("VA::getLocalFunctions"); }
    std::vector<rfmel::VirtualApertureStatus> getLocalFunctionStatus(
        rfmel::VirtualApertureInstanceID, rfmel::LocalFunctionTypeID) const override
    { forbidden("VA::getLocalFunctionStatus"); }
private:
    bool fail_getter_{};
    std::string scenario_;
};

class MockC2MEL final : public rfmel::C2MEL {
public:
    explicit MockC2MEL(std::string configuration) : configuration_{std::move(configuration)} {}
    ~MockC2MEL() override { record("rf_c2_destroyed"); }
    mel::RequestFor<rfmel::VirtualAperture> requestVirtualAperture(
        rfmel::VirtualApertureDefinitionID id, rfmel::Priority priority,
        const std::vector<std::string>& local, const std::string& file,
        const std::vector<mel::UCI_ID>& capabilities) override
    {
        record("rf_va_requested");
        if (id != 0xFEDCBA98U || priority != 0x80000001U ||
            local != std::vector<std::string>{"alpha", "µ-local", ""} ||
            file != "definition/β.json" || capabilities.size() != 3U)
            throw std::runtime_error("VA input mismatch");
        std::array<std::uint8_t, mel::UUID_SIZE> first_expected{};
        std::array<std::uint8_t, mel::UUID_SIZE> second_expected{};
        std::array<std::uint8_t, mel::UUID_SIZE> third_expected{};
        first_expected[1] = 0x80U;
        first_expected[2] = 0xffU;
        second_expected[0] = 0xffU;
        third_expected[15] = 0x80U;
        if (capabilities[0].getUUID() != first_expected ||
            capabilities[1].getUUID() != second_expected ||
            capabilities[2].getUUID() != third_expected ||
            capabilities[0].getDescriptiveLabel() != "first" ||
            capabilities[1].getDescriptiveLabel() != "" ||
            capabilities[2].getDescriptiveLabel() != "µ-third")
            throw std::runtime_error("VA capability mismatch");
        if (configuration_ == "c2:va-invalid-future") return {};
        if (configuration_ == "c2:va-delayed") {
            auto gate = std::make_shared<std::promise<mel::ErrorOr<std::shared_ptr<rfmel::VirtualAperture>>>>();
            auto future = gate->get_future();
            std::lock_guard lock{va_gate_mutex};
            va_pending.push_back(std::move(gate));
            return future;
        }
        if (configuration_ == "c2:va-submit-throw") throw std::runtime_error("mock VA submit exception");
        std::promise<mel::ErrorOr<std::shared_ptr<rfmel::VirtualAperture>>> promise;
        auto future = promise.get_future();
        if (configuration_ == "c2:va-failure" || configuration_ == "c2:va-long-failure" ||
            configuration_ == "c2:va-unknown-error")
            promise.set_value(mel::ErrorOr<std::shared_ptr<rfmel::VirtualAperture>>{
                mel::Error{configuration_ == "c2:va-unknown-error" ?
                    static_cast<mel::ErrorCode>(99) : mel::ErrorCode::InvalidParameters,
                    configuration_ == "c2:va-long-failure" ?
                    std::string(800, 'X') + "µ end" : "mock VA rejected"}});
        else if (configuration_ == "c2:va-null")
            promise.set_value(mel::ErrorOr<std::shared_ptr<rfmel::VirtualAperture>>{
                std::shared_ptr<rfmel::VirtualAperture>{}});
        else if (configuration_ == "c2:va-future-throw")
            promise.set_exception(std::make_exception_ptr(std::runtime_error("mock VA future exception")));
        else if (configuration_ == "c2:va-future-unknown")
            promise.set_exception(std::make_exception_ptr(17));
        else
            promise.set_value(mel::ErrorOr<std::shared_ptr<rfmel::VirtualAperture>>{
                std::make_shared<MockVirtualAperture>(configuration_ == "c2:va-getter-throw", configuration_)});
        return future;
    }
    mel::RequestFor<rfmel::CachedWaveform> requestCachedWaveform(
        rfmel::Priority, const std::vector<std::complex<double>>&, rfmel::Frequency) override
    { forbidden("C2MEL::requestCachedWaveform"); }
    mel::RequestFor<rfmel::WaveformTxEndpoint> createWaveformTxEndpoint(
        rfmel::JobDataFormat, std::size_t, char *) override
    { forbidden("C2MEL::createWaveformTxEndpoint"); }
    mel::RequestFor<rfmel::ExternalEndpoint> registerExternalTxEndpoint(
        rfmel::JobDataFormat, const rfmel::RDMAExternalEndpointParams&) override
    { forbidden("C2MEL::registerExternalTxEndpoint"); }
    const rfmel::RFMFAInfo& getRFMFAInfo() const override
    { forbidden("C2MEL::getRFMFAInfo"); }
    mel::VersionInfo getVersionInfo() const override { return {1, 1, "mock", "C2"}; }
    void shutdown() override
    {
        c2_shutdown_calls.fetch_add(1U);
        record("rf_c2_shutdown");
        {
            std::lock_guard lock{job_cleanup_mutex};
            ++job_cleanup_shutdowns;
        }
        job_cleanup_changed.notify_all();
        if (configuration_ == "c2:shutdown-throw" || configuration_ == "c2:va-shutdown-throw" ||
            configuration_ == "c2:job-shutdown-throw" ||
            configuration_ == "c2:finalize-shutdown-throw")
            throw std::runtime_error("mock C2 shutdown exception");
    }
private:
    std::string configuration_;
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

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
#endif
extern "C" __attribute__((visibility("default")))
std::shared_ptr<ams::iface::rfmel::AdminMEL> createAdminMEL(std::string_view configuration);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
std::shared_ptr<ams::iface::rfmel::AdminMEL> createAdminMEL(std::string_view configuration)
{
    record("rf_admin_factory_called");
    if (configuration == "admin:factory-null") return {};
    if (configuration == "admin:factory-throw") throw FactoryFailure{};
    if (configuration == "admin:factory-throw-unknown") throw 5;
    auto scenario = std::make_shared<Scenario>();
    scenario->name = std::string{configuration};
    return std::make_shared<MockAdminMEL>(std::move(scenario));
}
static_assert(std::is_same_v<decltype(&createAdminMEL), rfmel::fnAdminMEL>);

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
#endif
extern "C" __attribute__((visibility("default")))
std::shared_ptr<ams::iface::rfmel::C2MEL> createC2MEL(std::string_view configuration);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
std::shared_ptr<ams::iface::rfmel::C2MEL> createC2MEL(std::string_view configuration)
{
    c2_factory_calls.fetch_add(1U);
    record("rf_c2_factory_called");
    if (configuration == "c2:factory-null") return {};
    if (configuration == "c2:factory-throw") throw FactoryFailure{};
    if (configuration == "c2:factory-throw-unknown") throw 5;
    if (configuration != "c2:ok" && configuration != "c2:shutdown-throw" &&
        configuration != "c2:va-ok" && configuration != "c2:va-failure" &&
        configuration != "c2:va-null" && configuration != "c2:va-future-throw" &&
        configuration != "c2:va-future-unknown" && configuration != "c2:va-submit-throw" &&
        configuration != "c2:va-delayed" && configuration != "c2:va-long-failure" &&
        configuration != "c2:va-unknown-error" &&
        configuration != "c2:va-invalid-future" && configuration != "c2:va-getter-throw" &&
        configuration != "c2:va-shutdown-throw" &&
        configuration != "c2:job-ok" && configuration != "c2:job-delayed" &&
        configuration.substr(0, 12) != "c2:finalize-" &&
        configuration.substr(0, 10) != "c2:cancel-" &&
        configuration.substr(0, 7) != "c2:add-" &&
        configuration.substr(0, 9) != "c2:flush-" &&
        configuration.substr(0, 13) != "c2:remaining-" &&
        configuration != "c2:job-shutdown-throw" && configuration != "c2:job-failure" &&
        configuration != "c2:job-long-failure" && configuration != "c2:job-unknown-error" &&
        configuration != "c2:job-future-throw" && configuration != "c2:job-future-unknown" &&
        configuration != "c2:job-invalid-future" && configuration != "c2:job-null" &&
        configuration != "c2:job-getter-throw" && configuration != "c2:job-command-null" &&
        configuration != "c2:job-command-throw" && configuration != "c2:job-command-tx" &&
        configuration != "c2:job-setter-throw" && configuration != "c2:job-submit-throw")
        throw std::invalid_argument("mock C2 received unexpected configuration");
    return std::make_shared<MockC2MEL>(std::string{configuration});
}
static_assert(std::is_same_v<decltype(&createC2MEL), rfmel::fnC2MEL>);

extern "C" __attribute__((visibility("default"))) unsigned mock_rf_va_release_one(void)
{
    std::shared_ptr<std::promise<mel::ErrorOr<std::shared_ptr<rfmel::VirtualAperture>>>> gate;
    {
        std::lock_guard lock{va_gate_mutex};
        if (va_pending.empty()) return 0;
        gate = std::move(va_pending.back());
        va_pending.pop_back();
    }
    gate->set_value(mel::ErrorOr<std::shared_ptr<rfmel::VirtualAperture>>{
        std::make_shared<MockVirtualAperture>(false)});
    return 1;
}
extern "C" __attribute__((visibility("default"))) unsigned mock_rf_va_get_calls(void)
{ return va_get_calls.load(); }
extern "C" __attribute__((visibility("default"))) unsigned mock_rf_job_release_one(void)
{
    std::shared_ptr<std::promise<mel::ErrorOr<std::shared_ptr<rfmel::JobDetail>>>> gate;
    {
        std::lock_guard lock{job_gate_mutex};
        if (job_pending.empty()) return 0;
        gate = std::move(job_pending.back());
        job_pending.pop_back();
    }
    gate->set_value(mel::ErrorOr<std::shared_ptr<rfmel::JobDetail>>{
        std::make_shared<MockJobDetail>(false)});
    return 1;
}
extern "C" __attribute__((visibility("default"))) unsigned mock_rf_job_get_calls(void)
{ return job_get_calls.load(); }
extern "C" __attribute__((visibility("default"))) unsigned mock_rf_job_finalize_calls(void)
{ return job_finalize_calls.load(); }
extern "C" __attribute__((visibility("default"))) unsigned mock_rf_job_cancel_calls(void)
{ return job_cancel_calls.load(); }
extern "C" __attribute__((visibility("default"))) unsigned mock_rf_job_resolve_finalize(void)
{
    std::shared_ptr<std::promise<rfmel::JobStatus>> gate;
    {
        std::lock_guard lock{finalize_gate_mutex};
        if (finalize_pending.empty()) return 0;
        gate = std::move(finalize_pending.back());
        finalize_pending.pop_back();
    }
    gate->set_value(rfmel::JobStatus::Complete);
    return 1;
}
extern "C" __attribute__((visibility("default"))) unsigned mock_rf_job_shutdown_count(void)
{
    std::lock_guard lock{job_cleanup_mutex};
    return job_cleanup_shutdowns;
}
extern "C" __attribute__((visibility("default"))) unsigned mock_rf_job_wait_shutdown_after(unsigned baseline)
{
    std::unique_lock lock{job_cleanup_mutex};
    return job_cleanup_changed.wait_for(lock, std::chrono::seconds{3},
        [&] { return job_cleanup_shutdowns > baseline; }) ? 1U : 0U;
}

extern "C" __attribute__((visibility("default"))) unsigned mock_rf_c2_factory_calls(void)
{ return c2_factory_calls.load(); }
extern "C" __attribute__((visibility("default"))) unsigned mock_rf_c2_shutdown_calls(void)
{ return c2_shutdown_calls.load(); }

extern "C" __attribute__((visibility("default"))) unsigned mock_rf_admin_shutdown_calls(void)
{ return admin_shutdown_calls.load(); }
extern "C" __attribute__((visibility("default"))) unsigned mock_rf_admin_command_calls(void)
{ return admin_command_calls.load(); }
extern "C" __attribute__((visibility("default"))) std::uint32_t mock_rf_admin_last_state(void)
{ return admin_last_state.load(); }

/* TEST-only observations; not part of any MEL interface. */
extern "C" __attribute__((visibility("default"))) unsigned mock_rf_shutdown_calls(void)
{ return shutdown_calls.load(); }
extern "C" __attribute__((visibility("default"))) unsigned mock_rf_forbidden_calls(void)
{ return forbidden_calls.load(); }
extern "C" __attribute__((visibility("default"))) unsigned mock_rf_getter_calls(void)
{ return getter_calls.load(); }
extern "C" __attribute__((visibility("default"))) unsigned mock_rf_quantize_calls(void)
{ return quantize_calls.load(); }

/* ---------------------------------------------------------------------------
 * Task 033D TEST-only mock controls. Not part of any MEL interface.
 * -------------------------------------------------------------------------*/
extern "C" __attribute__((visibility("default"))) unsigned mock_rf_rx_rdma_calls(void)
{ return rdma_calls.load(); }
extern "C" __attribute__((visibility("default"))) unsigned mock_rf_rx_registrations(void)
{ return registrations.load(); }
extern "C" __attribute__((visibility("default"))) unsigned mock_rf_rx_endpoints_destroyed(void)
{ return endpoints_destroyed.load(); }
extern "C" __attribute__((visibility("default"))) unsigned mock_rf_rx_creates(void)
{ return creates.load(); }
extern "C" __attribute__((visibility("default"))) std::size_t mock_rf_rx_last_region_size(void)
{ return last_region_size.load(); }
extern "C" __attribute__((visibility("default"))) std::uint64_t mock_rf_rx_last_endpoint_id(void)
{ return last_endpoint_id.load(); }

extern "C" __attribute__((visibility("default"))) unsigned mock_rf_rx_empty_invocations(void)
{ return empty_invocations.load(); }

/* Completes every delayed create with a ComplexINT16 endpoint. */
extern "C" __attribute__((visibility("default"))) unsigned mock_rf_rx_release(void)
{
    std::lock_guard lock{pending_mutex};
    unsigned released = 0;
    for (auto& item : *delayed_creates) {
        if (item.mode.empty()) continue;
        item.promise.set_value(CreateResult{make_endpoint(item.mode, JobDataFormat::ComplexINT16)});
        item.mode.clear();
        ++released;
    }
    return released;
}

/* Emission kinds (all invoked synchronously on the calling thread). */
enum : int {
    EmitRich = 0, EmitNullMetadata = 1, EmitWrongVariant = 2, EmitNullPointer = 3,
    EmitCount = 4, EmitZeroNull = 5, EmitUserData = 6, EmitStabPoints = 7,
    EmitReceiveEvents = 8, EmitAssociations = 9, EmitEmptyAssociations = 10,
    EmitSparse = 11, EmitRichAlternate = 12
};

/* Returns 1 if the endpoint's retained callback was invoked. */
extern "C" __attribute__((visibility("default"))) int mock_rf_rx_emit(
    std::uint64_t id, int kind, std::size_t count)
{
    std::shared_ptr<RetainedCallback> held;
    {
        const auto endpoint = find_endpoint(id);
        if (!endpoint || !(held = endpoint->retained())) return 0;
    }
    auto metadata = rich_metadata();
    switch (kind) {
    case EmitRich:
    case EmitRichAlternate: {
        std::lock_guard lock{buffer_mutex};
        std::vector<ComplexI16>& buffer = *reuse_buffer;
        if (kind == EmitRich) {
            const std::int16_t values[6][2] = {{10, 20}, {30, 40}, {-5, 6},
                {INT16_MIN, INT16_MAX}, {INT16_MAX, INT16_MIN}, {-1, 0}};
            for (std::size_t index = 0; index < 6U; ++index)
                buffer[index] = ComplexI16{values[index][0], values[index][1]};
            invoke_held(held, metadata, rfmel::JobDataPointer{buffer.data()}, 6U);
        } else {
            for (std::size_t index = 0; index < 3U; ++index)
                buffer[index] = ComplexI16{static_cast<std::int16_t>(1000 + index),
                                           static_cast<std::int16_t>(-1000 - index)};
            metadata->setPhaseCoherenceWithPrior(false);
            invoke_held(held, metadata, rfmel::JobDataPointer{buffer.data()}, 3U);
        }
        /* The provider overwrites its storage immediately after return. */
        for (auto& sample : buffer) sample = ComplexI16{0x5A5A, -0x5A5B};
        metadata->setMelProtocolVersionID(0U);
        metadata->setReceiveEvents(std::vector<rfmel::StreamID>{7U});
        return 1;
    }
    case EmitNullMetadata: {
        ComplexI16 sample{1, 2};
        invoke_held(held, nullptr, rfmel::JobDataPointer{&sample}, 1U);
        return 1;
    }
    case EmitWrongVariant: {
        std::int16_t direct[2] = {1, 2};
        invoke_held(held, metadata, rfmel::JobDataPointer{direct}, 2U);
        return 1;
    }
    case EmitNullPointer:
        invoke_held(held, metadata, rfmel::JobDataPointer{static_cast<ComplexI16 *>(nullptr)},
                         count == 0U ? 1U : count);
        return 1;
    case EmitCount: {
        std::vector<ComplexI16> samples(count);
        for (std::size_t index = 0; index < count; ++index)
            samples[index] = ComplexI16{static_cast<std::int16_t>(index),
                                        static_cast<std::int16_t>(-static_cast<int>(index))};
        invoke_held(held, metadata, rfmel::JobDataPointer{samples.data()}, count);
        return 1;
    }
    case EmitZeroNull:
        invoke_held(held, metadata, rfmel::JobDataPointer{static_cast<ComplexI16 *>(nullptr)}, 0U);
        return 1;
    default: break;
    }
    ComplexI16 sample{3, 4};
    if (kind == EmitUserData) metadata->setUserDefinedData(std::any{42});
    else if (kind == EmitStabPoints) metadata->addStabPoints(rfmel::PointingType{rfmel::FaceRelativePointing{}});
    else if (kind == EmitReceiveEvents) metadata->addReceiveEvents(rfmel::ReceiveEvent{});
    else if (kind == EmitAssociations)
        metadata->setReceiveEventAssociations(std::vector<rfmel::JobEventID>{9U});
    else if (kind == EmitEmptyAssociations)
        metadata->setReceiveEventAssociations(std::vector<rfmel::JobEventID>{});
    else if (kind == EmitSparse) metadata = std::make_shared<rfmel::ProductRxMetadata>();
    else return 0;
    invoke_held(held, metadata, rfmel::JobDataPointer{&sample}, 1U);
    return 1;
}

/* Arms one provider-owned late callback for endpoint `id`: a detached
 * PROVIDER thread (its code lives in this DSO) that keeps the retained
 * callback, blocks on gate_fd, then invokes it with metadata == NULL and a
 * PROT_NONE sample page (count 1024). It writes to done_fd: 'K' invoked, 'E'
 * the retained callback object is empty, 'X' the invocation threw. */
extern "C" __attribute__((visibility("default"))) int mock_rf_rx_arm_late(
    std::uint64_t id, int gate_fd, int done_fd)
{
    std::shared_ptr<RetainedCallback> held;
    {
        std::lock_guard lock{retained_mutex};
        const auto found = retained_by_id->find(id);
        if (found == retained_by_id->end()) return 0;
        held = found->second;
    }
    void *const page = mmap(nullptr, 4096U, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page == MAP_FAILED) return 0;
    /* Handshake: arm returns only after the provider thread is running, so
     * the late thread is parked inside read() before the test may drop its
     * pin (and, under the no-library-pin negative control, before the DSO
     * can be unmapped under a thread that has not started yet). */
    std::promise<void> started;
    std::future<void> running = started.get_future();
    try {
        std::thread{[held, gate_fd, done_fd, page, started = std::move(started)]() mutable {
            char byte;
            ssize_t result;
            started.set_value();
            do { result = read(gate_fd, &byte, 1U); } while (result < 0 && errno == EINTR);
            DataReadyCallback& target = held->reference ? *held->reference : held->copy;
            if (!target) { write_byte(done_fd, 'E'); return; }
            try {
                target(nullptr, rfmel::JobDataPointer{static_cast<ComplexI16 *>(page)}, 1024U);
            } catch (...) {
                write_byte(done_fd, 'X');
                return;
            }
            write_byte(done_fd, 'K');
        }}.detach();
        running.wait();
    } catch (...) {
        return 0;
    }
    return 1;
}

/* Invokes endpoint `id`'s retained callback with its ENDPOINT-OWNED sample
 * page. The temporary endpoint shared_ptr is dropped BEFORE the invocation,
 * so during the callback the bridge endpoint owner is the only thing keeping
 * the endpoint, and therefore the page, alive. */
extern "C" __attribute__((visibility("default"))) int mock_rf_rx_emit_owned(std::uint64_t id)
{
    std::shared_ptr<RetainedCallback> held;
    ComplexI16 *samples = nullptr;
    {
        auto endpoint = find_endpoint(id);
        if (!endpoint || !(held = endpoint->retained())) return 0;
        samples = endpoint->owned_samples_page();
        endpoint.reset();
    }
    if (samples == nullptr) return 0;
    invoke_held(held, rich_metadata(), rfmel::JobDataPointer{samples}, owned_samples);
    return 1;
}

/* Configures the next "throw-active" registration's gate and done pipes. */
extern "C" __attribute__((visibility("default"))) void mock_rf_rx_set_active_throw(
    int gate_fd, int done_fd)
{
    active_done_fd.store(done_fd);
    active_gate_fd.store(gate_fd);
}

extern "C" __attribute__((visibility("default"))) unsigned mock_rf_physical_calls(void)
{ return physical_calls.load(); }
extern "C" __attribute__((visibility("default"))) unsigned mock_rf_tx_collection_calls(void)
{ return tx_collection_calls.load(); }
extern "C" __attribute__((visibility("default"))) unsigned mock_rf_tx_direct_calls(void)
{ return tx_direct_calls.load(); }
extern "C" __attribute__((visibility("default"))) std::uint32_t mock_rf_tx_face(void)
{ return tx_face.load(); }
extern "C" __attribute__((visibility("default"))) std::uint32_t mock_rf_tx_requested_id(void)
{ return tx_requested_id.load(); }
extern "C" __attribute__((visibility("default"))) std::uint32_t mock_rf_physical_face(void)
{ return physical_face.load(); }

extern "C" __attribute__((visibility("default"))) unsigned mock_rf_job_add_calls(void)
{ return job_add_calls.load(); }
extern "C" __attribute__((visibility("default"))) unsigned mock_rf_job_flush_calls(void)
{ return job_flush_calls.load(); }
extern "C" __attribute__((visibility("default"))) unsigned mock_rf_job_remaining_calls(void)
{ return job_remaining_calls.load(); }
extern "C" __attribute__((visibility("default"))) unsigned mock_rf_job_interval_fidelity(void)
{
    std::lock_guard lock{interval_mutex};
    if (latest_intervals.size() != 2) return 0;
    const auto& a = latest_intervals[0];
    const auto& b = latest_intervals[1];
    const auto& seq = a.getSequence();
    if (a.getIntervalStart().count() != 0 || a.getIntervalID() != 0x10203040U ||
        a.getIntervalStartingGap().count() != -111 || seq.getDuration().count() != 9876543210123LL ||
        a.getSequenceRepeatCount() != 0x100000003ULL || a.getCalDuration().count() != 222 ||
        a.getIntervalEndingGap().count() != -333 || !a.getPhaseCoherenceWithPrior() ||
        a.getIterationsPerSignal() != 0x100000005ULL || a.getMaxDataRateBps() != 123456789.25 ||
        a.getMaxSampleRateHZ() != 2500000.5 || a.getJobDetailsID() != 0xABCDEF01U ||
        seq.getRxEvents().size() != 2 || !seq.getTxEvents().empty()) return 0;
    const auto& x = seq.getRxEvents()[0];
    const auto& y = seq.getRxEvents()[1];
    if (x.getEventID() != 0x80000001U || x.getElementGroupLabel() != "rx/µ-main" ||
        x.getStart().count() != -123 || x.getDuration().count() != 456789 ||
        x.getCenterFrequency() != 987654321.125 || x.getSampleFrequency() != 2000000.5 ||
        x.getNumIterationProcessingAGC() != 0x100000007ULL || x.getNumIterationIgnoredPostAGC() != 3 ||
        x.getMaxExtensionDuration().count() != -999) return 0;
    if (y.getEventID() != UINT32_MAX || y.getElementGroupLabel() != "β-secondary" ||
        y.getStart().count() != 777 || y.getDuration().count() != -888 ||
        y.getCenterFrequency() != 0 || !std::signbit(y.getCenterFrequency()) ||
        !std::isinf(y.getSampleFrequency()) || std::signbit(y.getSampleFrequency()) ||
        y.getNumIterationProcessingAGC() != 0 || y.getNumIterationIgnoredPostAGC() != 0x100000009ULL ||
        y.getMaxExtensionDuration().count() != INT64_MAX - 1) return 0;
    if (b.getIntervalStart().count() != -1 || b.getIntervalID() != 42 ||
        b.getIntervalStartingGap().count() != 12 || b.getSequence().getDuration().count() != -13 ||
        b.getSequenceRepeatCount() != 2 || b.getCalDuration().count() != -14 ||
        b.getIntervalEndingGap().count() != 15 || b.getPhaseCoherenceWithPrior() ||
        b.getIterationsPerSignal() != 3 || !std::isnan(b.getMaxDataRateBps()) ||
        !std::isinf(b.getMaxSampleRateHZ()) || b.getJobDetailsID() != 43 ||
        !b.getSequence().getRxEvents().empty()) return 0;
    for (const auto& interval : latest_intervals) {
        if (interval.getApplicableElementGroups().size() != 0 || interval.getEndpoints().size() != 0 ||
            !interval.getStabPoints().empty() || !interval.getLfCommands().empty() ||
            interval.getJobIntervalStatusEnable() != rfmel::JobIntervalStatusEnable::Never ||
            !interval.getActivityId().empty() || interval.getTxPowerModeID() != 0 ||
            interval.getExecutionType() != rfmel::ExecutionType::Normal ||
            !interval.getModulations().empty() || !interval.getSequence().getTxEvents().empty()) return 0;
        for (const auto& event : interval.getSequence().getRxEvents()) {
            if (event.getDirection() != rfmel::JobEvent::Direction::Receive ||
                !event.getPolarization().empty() || event.getPolarizationBeemSteerCorrection() ||
                event.getPhaseOffset() != 0 || event.getStabPointIndex() != 0 || !event.getWeights().empty() ||
                event.getExecutionType() != rfmel::ExecutionType::Normal ||
                event.getEventTerminationType() != rfmel::JobEvent::EventTerminationType::InhibitEvent ||
                event.getAllowDelayStart() || event.getIterationHoldCount() != 0 ||
                event.getIterationTerminationCount() != 0 || event.getChannelizationEnabled() ||
                !event.getApplicableRxElementGroups().empty()) return 0;
        }
    }
    return 1;
}
extern "C" __attribute__((visibility("default"))) unsigned mock_rf_job_start_boundary(void)
{
    std::lock_guard lock{interval_mutex};
    if (latest_intervals.size() != 4) return 0;
    const std::int64_t starts[]{0, 1, -1, INT64_MAX};
    for (std::size_t i = 0; i < 4; ++i)
        if (latest_intervals[i].getIntervalStart().count() != starts[i]) return 0;
    return 1;
}
