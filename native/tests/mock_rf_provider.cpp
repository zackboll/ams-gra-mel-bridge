/* Task 033B/033D deterministic mock RF MEL provider. It exports exactly
 * createDataMEL plus TEST-only observation/control functions, and is
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
#include <rfmel/data/ProductRxEndpoint.h>
#include <rfmel/endpoints/RDMAMemoryRegionParams.h>
#include <rfmel/factory/RFCreateFunctions.h>
#include <rfmel/mfa/RFMFAInfo.h>

#include <any>
#include <atomic>
#include <cerrno>
#include <chrono>
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
