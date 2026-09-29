/* Task 033D RF ProductRxEndpoint ComplexINT16 receive.
 *
 * Ownership graph (never exposed through C):
 *
 *   ams_mel_rf_product_rx_request --shared_ptr--> CreateCompletion
 *   detached worker (sole future.get() caller) --> CreateWorkerInput
 *        CreateCompletion owns: cached outcome, unclaimed endpoint, RfChildClaim
 *
 *   ams_mel_rf_product_rx (claimed) owns: provider endpoint, the SAME
 *        RfChildClaim, and a reference to its RfRxCallbackState
 *
 *   PermanentRxRegistration (process lifetime, intrusive root, never freed):
 *        the exact std::function lvalue passed to setDataReadyCallback,
 *        shared_ptr<RfRxCallbackState>, shared_ptr<SharedLibrary>
 *        -- and NOT the DataMEL, the endpoint, the public owner, or events.
 *
 *   ams_mel_rf_product_rx_event: immutable plain copies only. */
#include <ams_mel/abi.h>
#include "internal/provider_common.hpp"
#include "internal/rf_product_rx.hpp"
#include "internal/shared_library.hpp"

#include <rfmel/data/DataMEL.h>
#include <rfmel/data/ProductRxEndpoint.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace mel = ams::iface::mel;
namespace rfmel = ams::iface::rfmel;
using namespace ams_mel::internal;

/* The exact pinned callback type; setDataReadyCallback takes it by NON-CONST
 * lvalue reference, so a provider may copy it, move from it, or keep a
 * reference to the exact object. */
using DataReadyCallback =
    std::function<void(std::shared_ptr<rfmel::ProductRxMetadata>, rfmel::JobDataPointer,
                       std::size_t)>;
using ComplexI16 = rfmel::MELComplex<std::int16_t>;
using CreateFuture = mel::RequestFor<rfmel::ProductRxEndpoint>;

struct ams_mel_rf_product_rx_event {
    std::vector<ams_mel_rf_complex_i16_v1> samples;
    std::vector<std::uint32_t> stream_ids;
    ams_mel_rf_product_rx_event_v1 view{};
};

namespace {

enum class RxLifecycle { Receiving, Closed };

struct RfRxCallbackState {
    std::mutex mutex;
    std::condition_variable ready;   /* Receive wake-ups */
    std::condition_variable drained; /* in_flight reached zero */
    RxLifecycle lifecycle{RxLifecycle::Receiving};
    std::deque<std::unique_ptr<ams_mel_rf_product_rx_event>> queue;
    std::size_t queue_capacity{};
    std::size_t max_samples_per_event{};
    std::size_t in_flight{};
    std::size_t waiters{}; /* Receive calls currently blocked (observation only) */
    std::size_t drain_waiters{}; /* Close/cleanup blocked in the drain (observation only) */
    ams_mel_rf_product_rx_counters_v1 counters{};
    std::uint64_t endpoint_id{};
};

void saturating_increment(std::uint64_t& value) noexcept
{
    if (value != std::numeric_limits<std::uint64_t>::max()) ++value;
}

/* One heap-stable registration per setDataReadyCallback call. Once linked it
 * is never freed and `callback` never moves: the provider may hold a
 * reference to that exact object. */
struct PermanentRxRegistration {
    std::shared_ptr<SharedLibrary> library;
    std::shared_ptr<RfRxCallbackState> callback_state;
    DataReadyCallback callback;
    PermanentRxRegistration *next{};
};

std::atomic<PermanentRxRegistration *> permanent_registrations{};

/* Allocation-free: the link is a raw pointer inside the registration. */
void retain_registration_forever(PermanentRxRegistration *registration) noexcept
{
    PermanentRxRegistration *head = permanent_registrations.load(std::memory_order_relaxed);
    do { registration->next = head; }
    while (!permanent_registrations.compare_exchange_weak(
        head, registration, std::memory_order_release, std::memory_order_relaxed));
}

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
/* Test-only seams; none is reachable from a production build. */
enum : unsigned { FailEvent = 1U, FailStreamIds = 2U, FailWorkerLaunch = 3U };
enum : unsigned {
    ControlNone = 0U, ControlNoLibraryPin = 1U, ControlNoExactLvalue = 2U,
    ControlOldCloseOrder = 3U
};
std::atomic<unsigned> test_failpoint{};
std::atomic<unsigned> test_negative_control{};
std::atomic<int> test_hold_notify{-1};
std::atomic<int> test_hold_release{-1};

bool take_failpoint(unsigned which) noexcept
{
    unsigned expected = which;
    return test_failpoint.compare_exchange_strong(expected, 0U);
}

/* Appends one line to AMS_MEL_TEST_LIFETIME_LOG (opened without O_CREAT). */
void test_record(const char *event) noexcept
{
    const char *const path = std::getenv("AMS_MEL_TEST_LIFETIME_LOG");
    if (path == nullptr) return;
    const int fd = open(path, O_WRONLY | O_APPEND | O_CLOEXEC);
    if (fd < 0) return;
    char line[128];
    const std::size_t size = std::strlen(event);
    if (size + 1U <= sizeof line) {
        std::memcpy(line, event, size);
        line[size] = '\n';
        ssize_t written;
        do { written = write(fd, line, size + 1U); } while (written < 0 && errno == EINTR);
    }
    (void)close(fd);
}

/* One-shot deterministic hold after in_flight was incremented and Receiving
 * was observed. Returns true if THIS callback was the held one. */
bool test_hold() noexcept
{
    const int notify = test_hold_notify.exchange(-1);
    if (notify < 0) return false;
    const int release = test_hold_release.exchange(-1);
    char byte = 'h';
    ssize_t result;
    do { result = write(notify, &byte, 1U); } while (result < 0 && errno == EINTR);
    do { result = read(release, &byte, 1U); } while (result < 0 && errno == EINTR);
    test_record("rf_rx_held_callback_released");
    return true;
}
#endif

enum class BuildOutcome { Accepted, Malformed, AllocationFailure };

static_assert(std::is_same_v<rfmel::StreamID, std::uint32_t>);
static_assert(std::is_same_v<rfmel::EndpointID, std::uint64_t>);
static_assert(std::is_same_v<rfmel::VirtualApertureDefinitionID, std::uint32_t>);
static_assert(std::is_same_v<rfmel::VirtualApertureInstanceID, std::uint32_t>);
static_assert(std::is_same_v<rfmel::LocalFunctionTypeID, std::uint32_t>);
static_assert(std::is_same_v<ams::util::math::Femtoseconds::rep, std::int64_t>);
static_assert(sizeof(std::chrono::seconds::rep) == sizeof(std::int64_t) &&
              std::is_signed_v<std::chrono::seconds::rep>);

/* Validates and copies one callback product. Every value read from the
 * provider is copied into event-owned storage before this returns; no
 * provider pointer or reference survives. */
BuildOutcome build_event(const RfRxCallbackState& state,
                         const rfmel::ProductRxMetadata *metadata,
                         const rfmel::JobDataPointer& data, std::size_t count,
                         std::unique_ptr<ams_mel_rf_product_rx_event>& output)
{
    if (metadata == nullptr) return BuildOutcome::Malformed;
    ComplexI16 *const *const alternative = std::get_if<ComplexI16 *>(&data);
    if (alternative == nullptr) return BuildOutcome::Malformed;
    const ComplexI16 *const source = *alternative;
    if (count != 0U && source == nullptr) return BuildOutcome::Malformed;
    if (count > state.max_samples_per_event) return BuildOutcome::Malformed;
    if (count > std::vector<ams_mel_rf_complex_i16_v1>{}.max_size())
        return BuildOutcome::Malformed;

    /* Fail closed: unsupported nonempty metadata is never silently dropped. */
    if (metadata->getUserDefinedData().has_value()) return BuildOutcome::Malformed;
    if (!metadata->getStabPoints().empty()) return BuildOutcome::Malformed;
    if (!metadata->getReceiveEvents().empty()) return BuildOutcome::Malformed;
    {
        const auto associations = metadata->getReceiveEventAssociations();
        if (associations.has_value() && !associations->empty())
            return BuildOutcome::Malformed;
    }

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    if (take_failpoint(FailEvent)) throw std::bad_alloc{};
#endif
    auto event = std::make_unique<ams_mel_rf_product_rx_event>();
    /* Element-wise: MELComplex<int16_t> is not trivially copyable, and no
     * layout compatibility with ams_mel_rf_complex_i16_v1 is assumed. */
    event->samples.resize(count);
    for (std::size_t index = 0; index < count; ++index) {
        event->samples[index].real = source[index].real();
        event->samples[index].imag = source[index].imag();
    }
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    if (take_failpoint(FailStreamIds)) throw std::bad_alloc{};
#endif
    const std::vector<rfmel::StreamID> ids = metadata->getRxStreamIDs();
    event->stream_ids.assign(ids.begin(), ids.end());
    const auto start = metadata->getFirstReceiveEventStart();

    ams_mel_rf_product_rx_event_v1& view = event->view;
    view.endpoint_id = state.endpoint_id;
    view.data_format = AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT16;
    view.samples = {event->samples.empty() ? nullptr : event->samples.data(),
                    event->samples.size()};
    ams_mel_rf_product_rx_metadata_v1& out = view.metadata;
    out.mel_protocol_version_id = metadata->getMelProtocolVersionID();
    out.va_definition_id = metadata->getVaDefinitionID();
    out.va_instance_id = metadata->getVaInstanceID();
    out.job_details_id = metadata->getJobDetailsID();
    out.job_interval_id = metadata->getJobIntervalID();
    out.lf_type_id = metadata->getLfTypeID();
    out.lf_instance_id = metadata->getLfInstanceID();
    out.phase_coherence_with_prior = metadata->getPhaseCoherenceWithPrior() ? 1U : 0U;
    /* Verbatim: no nanosecond conversion, no normalization. */
    out.first_rx_event_start_s = start.getIntegralSeconds().count();
    out.first_rx_event_start_fs = start.getFractionalFemtoseconds().count();
    out.rx_stream_ids = {event->stream_ids.empty() ? nullptr : event->stream_ids.data(),
                         event->stream_ids.size()};
    output = std::move(event);
    return BuildOutcome::Accepted;
}

/* The bridge callback body. noexcept: nothing escapes into provider code. */
void on_data_ready(RfRxCallbackState& state,
                   std::shared_ptr<rfmel::ProductRxMetadata> metadata,
                   const rfmel::JobDataPointer& data, std::size_t count) noexcept
{
    {
        /* Lifecycle is checked BEFORE any metadata, variant payload, or
         * sample access and before any allocation. */
        std::lock_guard lock{state.mutex};
        saturating_increment(state.counters.callbacks_received);
        ++state.in_flight;
        if (state.lifecycle == RxLifecycle::Closed) {
            saturating_increment(state.counters.callbacks_after_close);
            if (--state.in_flight == 0U) state.drained.notify_all();
            return;
        }
    }
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    const bool held = test_hold();
#endif
    /* Validation and copy allocation run outside the queue mutex. This is
     * where callback-scoped provider data (metadata, JobDataPointer samples,
     * possibly ProductRxEndpoint-owned storage) is read, so Close must drain
     * this body BEFORE it destroys the ProductRxEndpoint. */
    std::unique_ptr<ams_mel_rf_product_rx_event> event;
    BuildOutcome outcome;
    try {
        outcome = build_event(state, metadata.get(), data, count, event);
    } catch (const std::bad_alloc&) {
        outcome = BuildOutcome::AllocationFailure;
    } catch (...) {
        outcome = BuildOutcome::Malformed;
    }
    if (outcome != BuildOutcome::Accepted) event.reset();

    std::unique_ptr<ams_mel_rf_product_rx_event> discarded;
    {
        std::lock_guard lock{state.mutex};
        if (state.lifecycle == RxLifecycle::Closed) {
            /* Close won while this callback was copying: publish nothing. */
            saturating_increment(state.counters.callbacks_after_close);
            discarded = std::move(event);
        } else if (outcome == BuildOutcome::Malformed) {
            saturating_increment(state.counters.malformed_or_unsupported);
        } else if (outcome == BuildOutcome::AllocationFailure) {
            saturating_increment(state.counters.allocation_failures);
        } else if (state.queue.size() >= state.queue_capacity) {
            /* DROP-INCOMING: queued (older) products are never replaced. */
            saturating_increment(state.counters.products_dropped_queue_full);
            discarded = std::move(event);
        } else {
            try {
                state.queue.push_back(std::move(event));
                saturating_increment(state.counters.products_queued);
                state.ready.notify_all();
            } catch (...) {
                saturating_increment(state.counters.allocation_failures);
                discarded = std::move(event);
            }
        }
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
        /* Recorded under the state mutex, so it precedes the drain's return
         * and therefore any post-drain ProductRxEndpoint destruction. */
        if (held) test_record("rf_rx_held_callback_left_in_flight");
#endif
        if (--state.in_flight == 0U) state.drained.notify_all();
    }
    /* `discarded` and `metadata` are released after the lock: owned plain
     * copies and the provider's shared_ptr only. */
}

/* Logical Close followed by a drain of CURRENT bridge callback bodies.
 *
 * Shared by endpoint Close and by registration-throw cleanup. It never
 * destroys the ProductRxEndpoint, never releases the child claim, and never
 * calls the provider: the caller does those only after this returns true.
 *
 * Why drain BEFORE endpoint destruction: a callback admitted while the
 * endpoint was Receiving may still be reading callback-scoped provider data
 * (JobDataPointer samples, ProductRxMetadata) that a provider is allowed to
 * back with ProductRxEndpoint-owned storage. Returning true proves only that
 * no such callback is still inside the bridge body. It is NOT provider
 * callback quiescence and never authorizes a DSO unload: later callbacks may
 * still start, and they take the Closed fast path before touching any
 * payload (the permanent registration keeps callback, state, and DSO).
 *
 * Returns false if Closed + drained could not be established; the caller
 * must then retain the endpoint and child claim forever.
 *
 * `endpoint` is untouched in production. Only the TEST-build destructive
 * negative control drops it between the two phases (the old unsafe order). */
bool logical_close_and_drain(RfRxCallbackState& state,
                             [[maybe_unused]] std::shared_ptr<rfmel::ProductRxEndpoint>& endpoint)
    noexcept
{
    /* 1. Logical close: Closed, queued products discarded (so permanent
     *    callback-state retention never retains payloads), Receive woken. */
    std::deque<std::unique_ptr<ams_mel_rf_product_rx_event>> discarded;
    try {
        std::lock_guard lock{state.mutex};
        state.lifecycle = RxLifecycle::Closed;
        discarded.swap(state.queue);
        state.ready.notify_all();
    } catch (...) {
        return false;
    }
    discarded.clear(); /* facade events only, outside the lock */

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    if (test_negative_control.load() == ControlOldCloseOrder)
        endpoint.reset(); /* NEGATIVE CONTROL (destructive): destroy, then drain */
#endif

    /* 2. Drain every CURRENT bridge callback body. */
    try {
        std::unique_lock lock{state.mutex};
        ++state.drain_waiters;
        while (state.in_flight != 0U) state.drained.wait(lock);
        --state.drain_waiters;
    } catch (...) {
        return false;
    }
    return true;
}

/* ------------------------------------------------------------ create path */

enum class CreateKind { Pending, Succeeded, ProviderFailed, ProviderException, InternalError };

struct CreateCompletion {
    std::mutex mutex;
    std::condition_variable changed;
    CreateKind kind{CreateKind::Pending};
    ams_mel_error_code_t error_code{AMS_MEL_ERROR_NONE};
    std::string message;

    /* Successful creation, owned here until claim or abandonment. */
    std::shared_ptr<rfmel::ProductRxEndpoint> endpoint;
    ams_mel_rf_product_rx_info_v1 info{};
    RfChildClaim claim;
    bool claimed{};
    bool abandoned{};

    /* A failed claim is cached; registration is never attempted twice. */
    bool claim_failed{};
    ams_mel_status_t claim_status{AMS_MEL_OK};
    std::string claim_message;

    std::size_t queue_capacity{};
    std::size_t max_samples_per_event{};
};

/* Owned by the detached worker. Allocated before the provider call so that a
 * valid returned future always has an owner. */
struct CreateWorkerInput {
    std::shared_ptr<CreateCompletion> completion;
    CreateFuture future;
    /* Allocation-free permanent retention if the worker cannot launch. */
    std::shared_ptr<CreateWorkerInput> emergency_self;
    CreateWorkerInput *emergency_next{};
};

/* The worker could not launch around a VALID provider future. Nobody can
 * consume it, so the future, its eventual provider endpoint, and the RF child
 * claim are kept forever: the parent DataMEL is then never shut down or
 * destroyed and its DSO never unloaded. */
void retain_input_forever(const std::shared_ptr<CreateWorkerInput>& input) noexcept
{
    static std::atomic<CreateWorkerInput *> root{};
    input->emergency_self = input;
    CreateWorkerInput *head = root.load(std::memory_order_relaxed);
    do { input->emergency_next = head; }
    while (!root.compare_exchange_weak(head, input.get(), std::memory_order_release,
                                       std::memory_order_relaxed));
}

bool map_error_code(mel::ErrorCode code, ams_mel_error_code_t& output) noexcept
{
    switch (code) {
    case mel::ErrorCode::None: output = AMS_MEL_ERROR_NONE; return true;
    case mel::ErrorCode::InvalidId: output = AMS_MEL_ERROR_INVALID_ID; return true;
    case mel::ErrorCode::InvalidState: output = AMS_MEL_ERROR_INVALID_STATE; return true;
    case mel::ErrorCode::InvalidParameters:
        output = AMS_MEL_ERROR_INVALID_PARAMETERS; return true;
    case mel::ErrorCode::InsufficientPermissions:
        output = AMS_MEL_ERROR_INSUFFICIENT_PERMISSIONS; return true;
    case mel::ErrorCode::InsufficientResources:
        output = AMS_MEL_ERROR_INSUFFICIENT_RESOURCES; return true;
    case mel::ErrorCode::InsufficientLocalResources:
        output = AMS_MEL_ERROR_INSUFFICIENT_LOCAL_RESOURCES; return true;
    case mel::ErrorCode::InsufficientRemoteResources:
        output = AMS_MEL_ERROR_INSUFFICIENT_REMOTE_RESOURCES; return true;
    case mel::ErrorCode::Unsupported: output = AMS_MEL_ERROR_UNSUPPORTED; return true;
    }
    output = AMS_MEL_ERROR_NONE;
    return false;
}

/* Must be called from a catch handler. */
std::string current_exception_text(std::string_view fallback, std::string_view unknown)
{
    try {
        throw;
    } catch (const std::exception& error) {
        const char *const what = error.what();
        const std::string_view text = what == nullptr ? std::string_view{}
                                                      : std::string_view{what};
        return std::string{!text.empty() && valid_utf8(text) ? text : fallback};
    } catch (...) {
        return std::string{unknown};
    }
}

struct WorkerOutcome {
    CreateKind kind{CreateKind::InternalError};
    ams_mel_error_code_t error_code{AMS_MEL_ERROR_NONE};
    std::string message;
    std::shared_ptr<rfmel::ProductRxEndpoint> endpoint;
    ams_mel_rf_product_rx_info_v1 info{};
};

/* The ONE future.get() in the bridge. Any provider-owned object that is not
 * the accepted endpoint is destroyed before this returns. */
void settle(CreateWorkerInput& input, WorkerOutcome& outcome) noexcept
{
    std::optional<mel::ErrorOr<std::shared_ptr<rfmel::ProductRxEndpoint>>> result;
    try {
        result.emplace(input.future.get());
    } catch (const std::bad_alloc&) {
        outcome.kind = CreateKind::InternalError;
        return;
    } catch (...) {
        outcome.kind = CreateKind::ProviderException;
        try {
            outcome.message = current_exception_text("provider create future exception",
                                                     "unknown provider create future exception");
        } catch (...) { outcome.message.clear(); }
        return;
    }
    try {
        if (!*result) {
            const mel::Error& error = result->getError();
            outcome.kind = CreateKind::ProviderFailed;
            if (!map_error_code(error.getCode(), outcome.error_code)) {
                outcome.message =
                    "malformed provider ProductRx creation result: unknown MEL ErrorCode " +
                    std::to_string(static_cast<long long>(
                        static_cast<std::underlying_type_t<mel::ErrorCode>>(error.getCode())));
                return;
            }
            const std::string& description = error.getDescription();
            outcome.message = valid_utf8(description)
                ? description
                : std::string{"provider ProductRx creation error (invalid UTF-8 description)"};
            return;
        }
        std::shared_ptr<rfmel::ProductRxEndpoint> endpoint = std::move(result->get());
        result.reset();
        if (!endpoint) {
            outcome.kind = CreateKind::ProviderFailed;
            outcome.message = "malformed provider ProductRx creation result: null endpoint";
            return;
        }
        rfmel::EndpointID id{};
        rfmel::JobDataFormat format{};
        try {
            /* Each value is read exactly once, before any registration. */
            id = endpoint->getEndpointID();
            format = endpoint->getAssignedDataFormat();
        } catch (...) {
            outcome.kind = CreateKind::ProviderException;
            try {
                outcome.message = current_exception_text(
                    "provider ProductRxEndpoint getter exception",
                    "unknown provider ProductRxEndpoint getter exception");
            } catch (...) { outcome.message.clear(); }
            return; /* endpoint destroyed here, on the worker */
        }
        if (format != rfmel::JobDataFormat::ComplexINT16) {
            outcome.kind = CreateKind::ProviderFailed;
            outcome.message = "provider assigned JobDataFormat " +
                std::to_string(static_cast<long long>(
                    static_cast<std::underlying_type_t<rfmel::JobDataFormat>>(format))) +
                " to a ComplexINT16 ProductRxEndpoint request";
            return;
        }
        outcome.kind = CreateKind::Succeeded;
        outcome.info = {id, AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT16};
        outcome.endpoint = std::move(endpoint);
    } catch (...) {
        /* Only bridge-owned string building can throw here. */
        outcome.kind = CreateKind::InternalError;
        outcome.message.clear();
        outcome.endpoint.reset();
    }
}

void run_create_worker(std::shared_ptr<CreateWorkerInput> input) noexcept
{
    WorkerOutcome outcome;
    settle(*input, outcome);
    /* The consumed future (and its provider shared state) is released on the
     * worker, while the RF child claim still keeps the DSO loaded. */
    input->future = CreateFuture{};
    CreateCompletion& completion = *input->completion;

    std::shared_ptr<rfmel::ProductRxEndpoint> abandoned_endpoint;
    RfChildClaim released;
    if (outcome.kind != CreateKind::Succeeded) {
        /* Release the child claim BEFORE publishing the terminal failure, so
         * a caller that observes it knows the claim is already gone. A
         * deferred shutdown this triggers runs here, on the worker. */
        {
            std::lock_guard lock{completion.mutex};
            released = std::move(completion.claim);
        }
        released.release_detached();
    }
    {
        std::unique_lock lock{completion.mutex};
        completion.kind = outcome.kind;
        completion.error_code = outcome.error_code;
        completion.message.swap(outcome.message);
        if (outcome.kind != CreateKind::Succeeded) {
            completion.changed.notify_all();
        } else {
            completion.endpoint = std::move(outcome.endpoint);
            completion.info = outcome.info;
            completion.changed.notify_all();
            /* future success != callback registered. Wait for Claim (which
             * registers and takes the endpoint and claim) or for request
             * Close (abandonment). */
            while (!completion.claimed && !completion.abandoned) completion.changed.wait(lock);
            if (!completion.claimed) {
                abandoned_endpoint = std::move(completion.endpoint);
                released = std::move(completion.claim);
            }
        }
    }
    /* Unclaimed provider endpoint destroyed on the worker, outside every
     * bridge lock, BEFORE the claim can trigger a deferred parent shutdown. */
    abandoned_endpoint.reset();
    /* A deferred shutdown failure here has no public caller: the complete
     * RfDataState graph is retained, never retried. */
    released.release_detached();
}

} // namespace

struct ams_mel_rf_product_rx_request {
    std::shared_ptr<CreateCompletion> completion;
};

struct ams_mel_rf_product_rx {
    std::shared_ptr<rfmel::ProductRxEndpoint> endpoint;
    RfChildClaim claim;
    std::shared_ptr<RfRxCallbackState> state;
};

namespace {

bool bad_diagnostic(const char *diagnostic, std::size_t capacity) noexcept
{
    return diagnostic == nullptr && capacity != 0U;
}

ams_mel_status_t status_of(CreateKind kind) noexcept
{
    switch (kind) {
    case CreateKind::Pending: return AMS_MEL_TIMEOUT;
    case CreateKind::Succeeded: return AMS_MEL_OK;
    case CreateKind::ProviderFailed: return AMS_MEL_PROVIDER_FAILED;
    case CreateKind::ProviderException: return AMS_MEL_PROVIDER_EXCEPTION;
    case CreateKind::InternalError: break;
    }
    return AMS_MEL_INTERNAL_ERROR;
}

/* SIZE_MAX-checked conversion; region_size_bytes is otherwise verbatim. */
bool to_size(std::uint64_t value, std::size_t& output) noexcept
{
    if constexpr (std::numeric_limits<std::uint64_t>::max() >
                  std::numeric_limits<std::size_t>::max()) {
        if (value > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
            return false;
    }
    output = static_cast<std::size_t>(value);
    return true;
}

} // namespace

extern "C" ams_mel_status_t ams_mel_rf_data_submit_product_rx(
    ams_mel_rf_data *data, const ams_mel_rf_product_rx_config_v1 *config,
    ams_mel_rf_product_rx_request **out_request, char *diagnostic,
    std::size_t diagnostic_capacity, std::size_t *diagnostic_required) noexcept
{
    clear_diagnostic(diagnostic, diagnostic_capacity, diagnostic_required);
    if (bad_diagnostic(diagnostic, diagnostic_capacity) || data == nullptr || !data->state ||
        config == nullptr || out_request == nullptr || *out_request != nullptr)
        return AMS_MEL_INVALID_ARGUMENT;
    std::size_t region_size{};
    if (config->data_format != AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT16) {
        write_diagnostic("only AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT16 is supported",
                         diagnostic, diagnostic_capacity, diagnostic_required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    if (config->queue_capacity == 0U || config->max_samples_per_event == 0U ||
        !to_size(config->region_size_bytes, region_size)) {
        write_diagnostic("invalid RF ProductRx configuration", diagnostic,
                         diagnostic_capacity, diagnostic_required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    const std::shared_ptr<RfDataState>& parent = data->state;
    if (!parent->data) return AMS_MEL_INVALID_ARGUMENT;

    /* Everything needed to own a returned future exists before the call. */
    std::shared_ptr<CreateCompletion> completion;
    std::shared_ptr<CreateWorkerInput> input;
    std::unique_ptr<ams_mel_rf_product_rx_request> owner;
    std::unique_ptr<std::thread> worker;
    try {
        completion = std::make_shared<CreateCompletion>();
        completion->queue_capacity = config->queue_capacity;
        completion->max_samples_per_event = config->max_samples_per_event;
        input = std::make_shared<CreateWorkerInput>();
        input->completion = completion;
        owner = std::make_unique<ams_mel_rf_product_rx_request>();
        owner->completion = completion;
        worker = std::make_unique<std::thread>();
    } catch (...) {
        write_diagnostic("allocation failed before provider create", diagnostic,
                         diagnostic_capacity, diagnostic_required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    if (!RfChildClaim::acquire(parent, completion->claim)) {
        write_diagnostic("RF DataMEL is closing", diagnostic, diagnostic_capacity,
                         diagnostic_required);
        return AMS_MEL_INTERNAL_ERROR;
    }

    /* Provider call outside every bridge mutex. A failure below releases the
     * claim while the public parent owner still exists, so it can never be
     * the final child of a closed parent. */
    try {
        input->future = parent->data->createProductRxEndpoint(
            rfmel::JobDataFormat::ComplexINT16, region_size, nullptr);
    } catch (...) {
        completion->claim.release_detached();
        return translate_provider_exception("provider createProductRxEndpoint exception",
                                            "unknown provider createProductRxEndpoint exception",
                                            diagnostic, diagnostic_capacity,
                                            diagnostic_required);
    }
    if (!input->future.valid()) {
        completion->claim.release_detached();
        write_diagnostic("malformed provider result: invalid ProductRxEndpoint future",
                         diagnostic, diagnostic_capacity, diagnostic_required);
        return AMS_MEL_PROVIDER_FAILED;
    }
    try {
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
        if (take_failpoint(FailWorkerLaunch))
            throw std::system_error{std::make_error_code(std::errc::resource_unavailable_try_again)};
#endif
        *worker = std::thread{[input]() mutable noexcept { run_create_worker(std::move(input)); }};
        worker->detach();
    } catch (...) {
        /* A valid future with no consumer: retain it, its completion, and
         * the child claim forever. No request is published. */
        retain_input_forever(input);
        write_diagnostic("RF ProductRx create worker launch failed; provider future retained",
                         diagnostic, diagnostic_capacity, diagnostic_required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    *out_request = owner.release();
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_rf_product_rx_request_wait(
    const ams_mel_rf_product_rx_request *request, std::uint32_t timeout_ms,
    ams_mel_rf_product_rx_request_result_v1 *out_result, char *diagnostic,
    std::size_t diagnostic_capacity, std::size_t *diagnostic_required) noexcept
{
    clear_diagnostic(diagnostic, diagnostic_capacity, diagnostic_required);
    if (bad_diagnostic(diagnostic, diagnostic_capacity) || request == nullptr ||
        !request->completion || out_result == nullptr)
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        CreateCompletion& completion = *request->completion;
        std::unique_lock lock{completion.mutex};
        if (completion.kind == CreateKind::Pending && timeout_ms != 0U)
            completion.changed.wait_for(lock, std::chrono::milliseconds{timeout_ms}, [&] {
                return completion.kind != CreateKind::Pending;
            });
        const ams_mel_status_t status = status_of(completion.kind);
        if (status == AMS_MEL_TIMEOUT) return status;
        write_diagnostic(completion.message, diagnostic, diagnostic_capacity,
                         diagnostic_required);
        if (status == AMS_MEL_OK) out_result->error_code = AMS_MEL_ERROR_NONE;
        else if (status == AMS_MEL_PROVIDER_FAILED) out_result->error_code = completion.error_code;
        return status;
    } catch (...) {
        write_diagnostic("RF ProductRx request wait failed", diagnostic, diagnostic_capacity,
                         diagnostic_required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_rf_product_rx_request_claim(
    ams_mel_rf_product_rx_request *request, ams_mel_rf_product_rx **out_endpoint,
    ams_mel_rf_product_rx_info_v1 *out_info, char *diagnostic,
    std::size_t diagnostic_capacity, std::size_t *diagnostic_required) noexcept
{
    clear_diagnostic(diagnostic, diagnostic_capacity, diagnostic_required);
    if (bad_diagnostic(diagnostic, diagnostic_capacity) || request == nullptr ||
        !request->completion || out_endpoint == nullptr || *out_endpoint != nullptr ||
        out_info == nullptr)
        return AMS_MEL_INVALID_ARGUMENT;
    CreateCompletion& completion = *request->completion;

    /* Every allocation a callback needs exists BEFORE registration, so a
     * provider that invokes the callback synchronously inside
     * setDataReadyCallback is fully supported. */
    std::shared_ptr<RfRxCallbackState> state;
    std::unique_ptr<ams_mel_rf_product_rx> owner;
    std::unique_ptr<PermanentRxRegistration> registration;
    std::shared_ptr<rfmel::ProductRxEndpoint> endpoint;
    ams_mel_rf_product_rx_info_v1 info{};
    try {
        std::unique_lock lock{completion.mutex};
        if (completion.kind != CreateKind::Succeeded) {
            const ams_mel_status_t status = status_of(completion.kind);
            if (status != AMS_MEL_TIMEOUT)
                write_diagnostic(completion.message, diagnostic, diagnostic_capacity,
                                 diagnostic_required);
            return status;
        }
        if (completion.claim_failed) {
            write_diagnostic(completion.claim_message, diagnostic, diagnostic_capacity,
                             diagnostic_required);
            return completion.claim_status;
        }
        if (completion.claimed) {
            write_diagnostic("RF ProductRx endpoint already claimed", diagnostic,
                             diagnostic_capacity, diagnostic_required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        state = std::make_shared<RfRxCallbackState>();
        state->queue_capacity = completion.queue_capacity;
        state->max_samples_per_event = completion.max_samples_per_event;
        state->endpoint_id = completion.info.endpoint_id;
        owner = std::make_unique<ams_mel_rf_product_rx>();
        registration = std::make_unique<PermanentRxRegistration>();
        registration->callback_state = state;
        registration->library = completion.claim.state()->library;
        RfRxCallbackState *const raw = state.get();
        registration->callback = [raw](std::shared_ptr<rfmel::ProductRxMetadata> metadata,
                                       rfmel::JobDataPointer data, std::size_t count) noexcept {
            on_data_ready(*raw, std::move(metadata), data, count);
        };
        /* The claim transfers now (unique): the worker stops owning the
         * endpoint and never destroys it. */
        completion.claimed = true;
        endpoint = std::move(completion.endpoint);
        owner->claim = std::move(completion.claim);
        info = completion.info;
        completion.changed.notify_all();
    } catch (...) {
        write_diagnostic("allocation failed before RF callback registration", diagnostic,
                         diagnostic_capacity, diagnostic_required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    owner->state = state;
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    if (test_negative_control.load() == ControlNoLibraryPin)
        registration->library.reset(); /* NEGATIVE CONTROL: destructive */
#endif

    /* Registration outside every bridge mutex. The provider receives the
     * exact heap-stable lvalue; it is retained forever once this call
     * begins, whether the call returns or throws, because the generic bridge
     * cannot prove what the provider copied, moved, or referenced. */
    PermanentRxRegistration *const permanent = registration.release();
    bool threw = false;
    ams_mel_status_t failure = AMS_MEL_OK;
    std::string failure_text;
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    if (test_negative_control.load() == ControlNoExactLvalue) {
        /* NEGATIVE CONTROL (destructive): the DSO and callback state are
         * still retained, but the provider is handed a temporary copy that
         * is destroyed right after registration. Its storage is scrubbed and
         * deliberately never freed, so a provider that kept a reference to
         * that exact object deterministically finds an EMPTY std::function
         * instead of reused heap memory. */
        auto *temporary = new (std::nothrow) DataReadyCallback(permanent->callback);
        if (temporary == nullptr) std::terminate();
        try { endpoint->setDataReadyCallback(*temporary); }
        catch (...) { threw = true; failure = AMS_MEL_PROVIDER_EXCEPTION; }
        temporary->~DataReadyCallback();
        std::memset(static_cast<void *>(temporary), 0, sizeof(DataReadyCallback));
        retain_registration_forever(permanent);
    } else
#endif
    {
        try {
            endpoint->setDataReadyCallback(permanent->callback);
        } catch (...) {
            threw = true;
            failure = translate_provider_exception(
                "provider setDataReadyCallback exception",
                "unknown provider setDataReadyCallback exception", nullptr, 0U, nullptr);
            try {
                failure_text = current_exception_text(
                    "provider setDataReadyCallback exception",
                    "unknown provider setDataReadyCallback exception");
            } catch (...) { failure_text.clear(); }
        }
        retain_registration_forever(permanent);
    }
    if (!threw) {
        *out_info = info;
        owner->endpoint = std::move(endpoint);
        *out_endpoint = owner.release();
        return AMS_MEL_OK;
    }

    /* Registration threw: no public endpoint. Same order as endpoint Close:
     * Closed (a retained late callback publishes nothing), drain CURRENT
     * callbacks (a provider may have started one that is still reading
     * endpoint-owned data before it threw), only then destroy the provider
     * endpoint on this caller outside every bridge mutex, then release the
     * child claim (possibly the deferred parent shutdown). */
    const bool drained = logical_close_and_drain(*state, endpoint);
    if (failure == AMS_MEL_OK) failure = AMS_MEL_PROVIDER_EXCEPTION;
    {
        std::lock_guard lock{completion.mutex};
        completion.claim_failed = true;
        completion.claim_status = failure;
        completion.claim_message.swap(failure_text);
        write_diagnostic(completion.claim_message, diagnostic, diagnostic_capacity,
                         diagnostic_required);
    }
    if (!drained) {
        /* The callback-data boundary is unproven: keep the provider endpoint
         * and the child claim forever, so neither it nor the parent
         * DataMEL/provider graph can be destroyed across it. */
        owner->endpoint = std::move(endpoint);
        (void)owner.release();
        write_diagnostic("RF ProductRx registration cleanup failed; endpoint retained",
                         diagnostic, diagnostic_capacity, diagnostic_required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    endpoint.reset();
    const RfShutdownOutcome shutdown = owner->claim.release();
    if (shutdown.status != AMS_MEL_OK) {
        write_diagnostic(shutdown.message, diagnostic, diagnostic_capacity, diagnostic_required);
        return shutdown.status;
    }
    return failure;
}

extern "C" ams_mel_status_t ams_mel_rf_product_rx_request_close(
    ams_mel_rf_product_rx_request **request, char *diagnostic,
    std::size_t diagnostic_capacity, std::size_t *diagnostic_required) noexcept
{
    clear_diagnostic(diagnostic, diagnostic_capacity, diagnostic_required);
    if (bad_diagnostic(diagnostic, diagnostic_capacity) || request == nullptr)
        return AMS_MEL_INVALID_ARGUMENT;
    ams_mel_rf_product_rx_request *const owned = *request;
    *request = nullptr;
    if (owned == nullptr) return AMS_MEL_OK;
    std::shared_ptr<CreateCompletion> completion = std::move(owned->completion);
    delete owned;
    if (!completion) return AMS_MEL_OK;
    /* Not cancellation and nonblocking: only marks abandonment. A pending
     * future stays owned by its worker; a cached-but-unclaimed endpoint is
     * destroyed by the worker, never on this caller. */
    try {
        std::lock_guard lock{completion->mutex};
        completion->abandoned = true;
        completion->changed.notify_all();
    } catch (...) {
        /* Abandonment could not be recorded: the worker keeps the endpoint
         * and claim forever (retained, never unloaded). */
        write_diagnostic("RF ProductRx request close failed; provider state retained",
                         diagnostic, diagnostic_capacity, diagnostic_required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_rf_product_rx_receive(
    ams_mel_rf_product_rx *endpoint, std::uint32_t timeout_ms,
    ams_mel_rf_product_rx_event **out_event, char *diagnostic,
    std::size_t diagnostic_capacity, std::size_t *diagnostic_required) noexcept
{
    clear_diagnostic(diagnostic, diagnostic_capacity, diagnostic_required);
    if (bad_diagnostic(diagnostic, diagnostic_capacity) || endpoint == nullptr ||
        !endpoint->state || out_event == nullptr || *out_event != nullptr)
        return AMS_MEL_INVALID_ARGUMENT;
    /* Keep the state alive even if Close races this Receive. */
    const std::shared_ptr<RfRxCallbackState> state = endpoint->state;
    try {
        std::unique_lock lock{state->mutex};
        if (state->queue.empty() && state->lifecycle == RxLifecycle::Receiving &&
            timeout_ms != 0U) {
            ++state->waiters;
            state->ready.wait_for(lock, std::chrono::milliseconds{timeout_ms}, [&] {
                return !state->queue.empty() || state->lifecycle != RxLifecycle::Receiving;
            });
            --state->waiters;
        }
        if (state->lifecycle == RxLifecycle::Closed) return AMS_MEL_STREAM_STOPPED;
        if (state->queue.empty()) return AMS_MEL_TIMEOUT;
        *out_event = state->queue.front().release();
        state->queue.pop_front();
        return AMS_MEL_OK;
    } catch (...) {
        write_diagnostic("RF ProductRx receive failed", diagnostic, diagnostic_capacity,
                         diagnostic_required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_rf_product_rx_get_counters(
    const ams_mel_rf_product_rx *endpoint, ams_mel_rf_product_rx_counters_v1 *out_counters,
    char *diagnostic, std::size_t diagnostic_capacity,
    std::size_t *diagnostic_required) noexcept
{
    clear_diagnostic(diagnostic, diagnostic_capacity, diagnostic_required);
    if (bad_diagnostic(diagnostic, diagnostic_capacity) || endpoint == nullptr ||
        !endpoint->state || out_counters == nullptr)
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::lock_guard lock{endpoint->state->mutex};
        *out_counters = endpoint->state->counters;
        return AMS_MEL_OK;
    } catch (...) {
        write_diagnostic("RF ProductRx counters failed", diagnostic, diagnostic_capacity,
                         diagnostic_required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_rf_product_rx_close(
    ams_mel_rf_product_rx **endpoint, char *diagnostic, std::size_t diagnostic_capacity,
    std::size_t *diagnostic_required) noexcept
{
    clear_diagnostic(diagnostic, diagnostic_capacity, diagnostic_required);
    if (bad_diagnostic(diagnostic, diagnostic_capacity) || endpoint == nullptr)
        return AMS_MEL_INVALID_ARGUMENT;
    ams_mel_rf_product_rx *const owned = *endpoint;
    *endpoint = nullptr;
    if (owned == nullptr) return AMS_MEL_OK;
    std::unique_ptr<ams_mel_rf_product_rx> owner{owned};
    const std::shared_ptr<RfRxCallbackState> state = owner->state;

    /* 1+2. Logical Close, then drain CURRENT bridge callback bodies: a
     *      callback admitted while Receiving may still borrow
     *      ProductRxEndpoint-owned provider data. Not provider quiescence;
     *      never authorizes a DSO unload. */
    if (!logical_close_and_drain(*state, owner->endpoint)) {
        /* Closed + drained could not be established. Keep the endpoint AND
         * its child claim forever: neither the endpoint nor the parent can
         * be destroyed across an unproven callback-data boundary. */
        (void)owner.release();
        write_diagnostic("RF ProductRx close failed; endpoint retained", diagnostic,
                         diagnostic_capacity, diagnostic_required);
        return AMS_MEL_INTERNAL_ERROR;
    }

    /* 3. Only now drop the provider ProductRxEndpoint, outside every bridge
     *    mutex. */
    owner->endpoint.reset();

    /* 4. Release the child claim: possibly the deferred parent shutdown. */
    const RfShutdownOutcome shutdown = owner->claim.release();
    owner.reset();
    if (shutdown.status != AMS_MEL_OK) {
        write_diagnostic(shutdown.message.empty()
                             ? std::string_view{"deferred RF DataMEL shutdown failed"}
                             : std::string_view{shutdown.message},
                         diagnostic, diagnostic_capacity, diagnostic_required);
    }
    return shutdown.status;
}

extern "C" ams_mel_status_t ams_mel_rf_product_rx_event_view(
    const ams_mel_rf_product_rx_event *event, const ams_mel_rf_product_rx_event_v1 **out_view,
    char *diagnostic, std::size_t diagnostic_capacity,
    std::size_t *diagnostic_required) noexcept
{
    clear_diagnostic(diagnostic, diagnostic_capacity, diagnostic_required);
    if (bad_diagnostic(diagnostic, diagnostic_capacity) || event == nullptr ||
        out_view == nullptr)
        return AMS_MEL_INVALID_ARGUMENT;
    *out_view = &event->view;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_rf_product_rx_event_close(
    ams_mel_rf_product_rx_event **event, char *diagnostic, std::size_t diagnostic_capacity,
    std::size_t *diagnostic_required) noexcept
{
    clear_diagnostic(diagnostic, diagnostic_capacity, diagnostic_required);
    if (bad_diagnostic(diagnostic, diagnostic_capacity) || event == nullptr)
        return AMS_MEL_INVALID_ARGUMENT;
    delete std::exchange(*event, nullptr);
    return AMS_MEL_OK;
}

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
/* TEST-ONLY observation/control seams. Exported only through the generated
 * test-exports.map of AMS_MEL_BUILD_TESTS builds; never in exports.map and
 * never declared in an installed header. The observer shares ownership of
 * the callback state (which a permanent registration retains anyway), never
 * of the provider endpoint or DataMEL. */
struct ams_mel_test_rf_rx_observer { std::shared_ptr<RfRxCallbackState> state; };

extern "C" __attribute__((visibility("default"))) int ams_mel_test_rf_rx_observer_from(
    const ams_mel_rf_product_rx *endpoint, ams_mel_test_rf_rx_observer **output) noexcept
{
    if (endpoint == nullptr || !endpoint->state || output == nullptr || *output != nullptr)
        return 0;
    try { *output = new ams_mel_test_rf_rx_observer{endpoint->state}; return 1; }
    catch (...) { return 0; }
}

/* in_flight, callbacks_received, callbacks_after_close, queue length, Closed. */
extern "C" __attribute__((visibility("default"))) int ams_mel_test_rf_rx_observe(
    const ams_mel_test_rf_rx_observer *observer, std::size_t *in_flight,
    std::uint64_t *callbacks_received, std::uint64_t *callbacks_after_close,
    std::size_t *queue_length, int *closed) noexcept
{
    if (observer == nullptr || !observer->state || in_flight == nullptr ||
        callbacks_received == nullptr || callbacks_after_close == nullptr ||
        queue_length == nullptr || closed == nullptr)
        return 0;
    try {
        std::lock_guard lock{observer->state->mutex};
        *closed = observer->state->lifecycle == RxLifecycle::Closed ? 1 : 0;
        *in_flight = observer->state->in_flight;
        *callbacks_received = observer->state->counters.callbacks_received;
        *callbacks_after_close = observer->state->counters.callbacks_after_close;
        *queue_length = observer->state->queue.size();
        return 1;
    } catch (...) { return 0; }
}

/* Observer of the most recently retained permanent registration's callback
 * state; used when a throwing registration published no endpoint owner. */
extern "C" __attribute__((visibility("default"))) int ams_mel_test_rf_rx_observer_from_last_registration(
    ams_mel_test_rf_rx_observer **output) noexcept
{
    if (output == nullptr || *output != nullptr) return 0;
    PermanentRxRegistration *const head = permanent_registrations.load(std::memory_order_acquire);
    if (head == nullptr || !head->callback_state) return 0;
    try { *output = new ams_mel_test_rf_rx_observer{head->callback_state}; return 1; }
    catch (...) { return 0; }
}

extern "C" __attribute__((visibility("default"))) int ams_mel_test_rf_rx_waiters(
    const ams_mel_test_rf_rx_observer *observer, std::size_t *waiters) noexcept
{
    if (observer == nullptr || !observer->state || waiters == nullptr) return 0;
    try {
        std::lock_guard lock{observer->state->mutex};
        *waiters = observer->state->waiters;
        return 1;
    } catch (...) { return 0; }
}

/* Close/cleanup calls currently blocked in the current-callback drain. */
extern "C" __attribute__((visibility("default"))) int ams_mel_test_rf_rx_drain_waiters(
    const ams_mel_test_rf_rx_observer *observer, std::size_t *waiters) noexcept
{
    if (observer == nullptr || !observer->state || waiters == nullptr) return 0;
    try {
        std::lock_guard lock{observer->state->mutex};
        *waiters = observer->state->drain_waiters;
        return 1;
    } catch (...) { return 0; }
}

/* Presets every counter so saturation at UINT64_MAX is observable. */
extern "C" __attribute__((visibility("default"))) int ams_mel_test_rf_rx_preset_counters(
    const ams_mel_test_rf_rx_observer *observer, std::uint64_t value) noexcept
{
    if (observer == nullptr || !observer->state) return 0;
    try {
        std::lock_guard lock{observer->state->mutex};
        ams_mel_rf_product_rx_counters_v1& c = observer->state->counters;
        c.callbacks_received = c.products_queued = c.products_dropped_queue_full = value;
        c.malformed_or_unsupported = c.allocation_failures = c.callbacks_after_close = value;
        return 1;
    } catch (...) { return 0; }
}

extern "C" __attribute__((visibility("default"))) void ams_mel_test_rf_rx_observer_close(
    ams_mel_test_rf_rx_observer **observer) noexcept
{
    if (observer != nullptr) delete std::exchange(*observer, nullptr);
}

/* 1 event allocation, 2 stream-ID copy allocation, 3 worker launch. */
extern "C" __attribute__((visibility("default"))) void ams_mel_test_rf_rx_failpoint(
    unsigned which) noexcept
{ test_failpoint.store(which); }

/* Destructive negative controls: 1 no provider-library pin in the permanent
 * registration, 2 register a stack-local copy instead of the exact lvalue,
 * 3 old unsafe order (Closed, destroy ProductRxEndpoint, then drain). */
extern "C" __attribute__((visibility("default"))) void ams_mel_test_rf_rx_negative_control(
    unsigned which) noexcept
{ test_negative_control.store(which); }

/* The next Receiving callback writes one byte to notify_fd after entering
 * in_flight, then blocks reading one byte from release_fd. */
extern "C" __attribute__((visibility("default"))) void ams_mel_test_rf_rx_hold_next(
    int notify_fd, int release_fd) noexcept
{
    test_hold_release.store(release_fd);
    test_hold_notify.store(notify_fd);
}

extern "C" __attribute__((visibility("default"))) std::size_t
ams_mel_test_rf_rx_permanent_registrations(void) noexcept
{
    std::size_t count = 0;
    for (auto *item = permanent_registrations.load(std::memory_order_acquire); item;
         item = item->next) ++count;
    return count;
}
#endif
