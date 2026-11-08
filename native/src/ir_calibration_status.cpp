#include <ams_mel/abi.h>
#include "internal/ir_calibration_status.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <string_view>
#include <system_error>
#include <thread>
#include <type_traits>
#include <utility>

/* IRMEL-ADA-006: C2 CalibrationStatusCmd completion engine and the opt-in C2
 * CalibrationStatus owner. The C2 claim/send adapter and the owner's open
 * export live in ir_c2.cpp beside the private C2 ChannelState. */

namespace {
using namespace ams::iface;
using ams_mel_common::CompletionKind;

/* Pinned SubsystemCalibrationResult values; MaxExclusive is a sentinel only. */
using Result = mel::SubsystemCalibrationResult;
static_assert(static_cast<std::uint32_t>(Result::NotSet) == AMS_MEL_IR_CALIBRATION_RESULT_NOT_SET);
static_assert(static_cast<std::uint32_t>(Result::Completed) == AMS_MEL_IR_CALIBRATION_RESULT_COMPLETED);
static_assert(static_cast<std::uint32_t>(Result::Failed) == AMS_MEL_IR_CALIBRATION_RESULT_FAILED);
static_assert(static_cast<std::uint32_t>(Result::Interrupted) == AMS_MEL_IR_CALIBRATION_RESULT_INTERRUPTED);
static_assert(static_cast<std::uint32_t>(Result::NotCalibrated) == AMS_MEL_IR_CALIBRATION_RESULT_NOT_CALIBRATED);
static_assert(static_cast<std::uint32_t>(Result::MaxExclusive) == 5U);
static_assert(std::is_same_v<std::underlying_type_t<Result>, std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<irmel::CalibrationStatusCmd>().getCommandID()), std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<irmel::CalibrationStatusCmd>().getRequestID()), std::uint32_t>);
/* timeTag is signed nanoseconds and fits int64_t exactly: no unit conversion. */
static_assert(std::is_same_v<decltype(std::declval<mel::SubsystemCompletedCalibration>().getTimeTag()),
                             std::chrono::nanoseconds>);
static_assert(std::is_signed_v<std::chrono::nanoseconds::rep> &&
              sizeof(std::chrono::nanoseconds::rep) <= sizeof(std::int64_t));
static_assert(std::is_same_v<decltype(ams_mel_ir_completed_calibration_v1{}.time_tag_ns), std::int64_t>);
static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<mel::CalibrationStatus>().getActiveCalibrationIDs())>,
                             std::vector<mel::UCI_ID>>);
static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<mel::CalibrationStatus>().getCompletedCalibrations())>,
                             std::vector<mel::SubsystemCompletedCalibration>>);
static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<mel::SubsystemCompletedCalibration>().getCalibrationItems())>,
                             std::vector<mel::SubsystemCompletedCalibrationItemType>>);

bool valid_utf8(std::string_view value) noexcept
{
    std::size_t index = 0;
    while (index < value.size()) {
        const auto lead = static_cast<unsigned char>(value[index]);
        if (lead == 0U) return false;
        if (lead <= 0x7fU) { ++index; continue; }
        std::size_t trailing{};
        std::uint32_t point{};
        if (lead >= 0xc2U && lead <= 0xdfU) { trailing = 1U; point = lead & 0x1fU; }
        else if (lead >= 0xe0U && lead <= 0xefU) { trailing = 2U; point = lead & 0x0fU; }
        else if (lead >= 0xf0U && lead <= 0xf4U) { trailing = 3U; point = lead & 0x07U; }
        else return false;
        if (index + trailing >= value.size()) return false;
        for (std::size_t offset = 1; offset <= trailing; ++offset) {
            const auto byte = static_cast<unsigned char>(value[index + offset]);
            if ((byte & 0xc0U) != 0x80U) return false;
            point = (point << 6U) | (byte & 0x3fU);
        }
        if ((trailing == 2U && point < 0x800U) ||
            (trailing == 3U && point < 0x10000U) || point > 0x10ffffU ||
            (point >= 0xd800U && point <= 0xdfffU)) return false;
        index += trailing + 1U;
    }
    return true;
}

void write(std::string_view text, char *out, std::size_t capacity, std::size_t *required) noexcept
{
    if (required) *required = text.size() + 1U;
    if (out && capacity) {
        std::size_t copied = std::min(text.size(), capacity - 1U);
        while (copied && !valid_utf8(text.substr(0U, copied))) --copied;
        std::memcpy(out, text.data(), copied);
        out[copied] = '\0';
    }
}

ams_mel_error_code_t map_error(mel::ErrorCode code, bool& known) noexcept
{
    known = true;
    switch (code) {
    case mel::ErrorCode::None: return AMS_MEL_ERROR_NONE;
    case mel::ErrorCode::InvalidId: return AMS_MEL_ERROR_INVALID_ID;
    case mel::ErrorCode::InvalidState: return AMS_MEL_ERROR_INVALID_STATE;
    case mel::ErrorCode::InvalidParameters: return AMS_MEL_ERROR_INVALID_PARAMETERS;
    case mel::ErrorCode::InsufficientPermissions: return AMS_MEL_ERROR_INSUFFICIENT_PERMISSIONS;
    case mel::ErrorCode::InsufficientResources: return AMS_MEL_ERROR_INSUFFICIENT_RESOURCES;
    case mel::ErrorCode::InsufficientLocalResources: return AMS_MEL_ERROR_INSUFFICIENT_LOCAL_RESOURCES;
    case mel::ErrorCode::InsufficientRemoteResources: return AMS_MEL_ERROR_INSUFFICIENT_REMOTE_RESOURCES;
    case mel::ErrorCode::Unsupported: return AMS_MEL_ERROR_UNSUPPORTED;
    }
    known = false;
    return AMS_MEL_ERROR_NONE;
}

void arm_worker(const std::shared_ptr<CalibrationStatusWorkerInput>& input) noexcept
{
    input->emergency_self = input;
}

/* Allocation-free process-lifetime retention of a graph whose future cannot
 * be consumed safely; the provider library is therefore never unloaded. */
void retain_worker(const std::shared_ptr<CalibrationStatusWorkerInput>& input) noexcept
{
    static std::atomic<CalibrationStatusWorkerInput *> retained{};
    bool expected = false;
    if (!input->emergency_retained.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) return;
    CalibrationStatusWorkerInput *head = retained.load(std::memory_order_relaxed);
    do { input->emergency_next = head; }
    while (!retained.compare_exchange_weak(
        head, input.get(), std::memory_order_release, std::memory_order_relaxed));
}
void complete(const std::shared_ptr<CalibrationStatusCompletion>& state,
              mel::RequestFor<mel::CalibrationStatus>& future)
{
    CompletionKind kind = CompletionKind::ProviderException;
    ams_mel_ir_calibration_status_result_v1 result{};
    std::string message;
    std::shared_ptr<const ams_mel_calibration_status::Snapshot> snapshot;
    try {
        auto outcome = AMS_MEL_PROBE_GET(CalibrationStatus, future.get());
        if (outcome) {
            const auto& value = outcome.get();
            if (!value) {
                kind = CompletionKind::ProviderFailure;
                message = "provider returned null successful CalibrationStatus";
            } else if (!(snapshot = ams_mel_calibration_status::copy_status(*value))) {
                /* Never remapped to a known alternative. */
                result = {};
                kind = CompletionKind::ProviderFailure;
                message = "provider returned malformed CalibrationStatus data";
            } else {
                result.status = snapshot->view;
                result.error_code = AMS_MEL_ERROR_NONE;
                kind = CompletionKind::Success;
            }
        } else {
            const mel::Error& error = outcome.getError();
            bool known = false;
            result.error_code = map_error(error.getCode(), known);
            if (!known) {
                kind = CompletionKind::ProviderFailure;
                message = "provider returned unknown MEL error code";
            } else {
                kind = CompletionKind::Rejected;
                const std::string& description = error.getDescription();
                message = valid_utf8(description) ? description :
                          "provider rejection description was invalid UTF-8 or contained NUL";
            }
        }
    } catch (const std::bad_alloc&) {
        kind = CompletionKind::InternalError;
        message.clear();
    } catch (const std::exception& error) {
        kind = CompletionKind::ProviderException;
        const char *what = error.what();
        const std::string_view text = what ? std::string_view{what} : std::string_view{};
        try { message = !text.empty() && valid_utf8(text) ? text : "provider future exception"; }
        catch (...) { message.clear(); }
    } catch (...) {
        kind = CompletionKind::ProviderException;
        try { message = "unknown provider future exception"; } catch (...) {}
    }
    AMS_MEL_PROBE_BOUNDARY(CalibrationStatus);
    /* The future and provider result are consumed; only now may the C2
     * request accounting and any deferred physical teardown be released. */
    const char *failure = state->claim.failure_message();
    if (!state->claim.finish()) {
        kind = CompletionKind::ProviderFailure;
        try { message = failure ? failure : "deferred channel cleanup failed"; }
        catch (...) { message.clear(); }
    }
    {
        std::lock_guard lock{state->mutex};
        state->kind = kind;
        state->snapshot = std::move(snapshot);
        state->result = result;
        state->message = std::move(message);
        AMS_MEL_PROBE_GRAPH_EMPTY(CalibrationStatus, state->claim.empty());
    }
    state->ready.notify_all();
}

void run_worker(const std::shared_ptr<CalibrationStatusWorkerInput>& input) noexcept
{
    while (input->launch_state.load(std::memory_order_acquire) == 0U)
        std::this_thread::yield();
    AMS_MEL_PROBE_WORKER(CalibrationStatus);
    try {
        complete(input->completion, input->future); AMS_MEL_PROBE_RETURN(CalibrationStatus);
    } catch (...) {
        arm_worker(input);
        retain_worker(input);
    }
}

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
enum class SubmitFailpoint { None, Allocation, WorkerLaunch };
SubmitFailpoint submit_failpoint() noexcept
{
    const char *value = std::getenv("AMS_MEL_TEST_CALIBRATION_STATUS_POST_SEND_FAILURE");
    if (value && std::strcmp(value, "allocation") == 0) return SubmitFailpoint::Allocation;
    if (value && std::strcmp(value, "worker-launch") == 0) return SubmitFailpoint::WorkerLaunch;
    return SubmitFailpoint::None;
}
#endif

void increment(std::uint64_t& value) noexcept
{
    if (value != std::numeric_limits<std::uint64_t>::max()) ++value;
}
} // namespace

namespace ams_mel_calibration_status {

void diagnostic(const char *text, char *out, std::size_t capacity, std::size_t *required) noexcept
{
    write(text ? std::string_view{text} : std::string_view{}, out, capacity, required);
}

bool convert_command(const ams_mel_ir_calibration_status_command_v1& input,
                     irmel::CalibrationStatusCmd& output) noexcept
{
    output.setCommandID(input.command_id);
    output.setRequestID(input.request_id);
    return true;
}

namespace {
template<class T> bool checked_count(std::size_t size) noexcept
{
    return size <= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()) &&
           size <= static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()) / sizeof(T);
}
bool valid_id(const mel::UCI_ID& id) noexcept
{
    return checked_count<char>(id.getDescriptiveLabel().size()) && valid_utf8(id.getDescriptiveLabel());
}
ams_mel_uci_id_v1 id_view(const mel::UCI_ID& id) noexcept
{
    ams_mel_uci_id_v1 value{};
    std::copy(id.getUUID().begin(), id.getUUID().end(), value.uuid);
    value.descriptive_label = {id.getDescriptiveLabel().data(), id.getDescriptiveLabel().size()};
    return value;
}
bool valid_text(const std::string& text) noexcept
{
    return checked_count<char>(text.size()) && valid_utf8(text);
}
bool valid_result(mel::SubsystemCalibrationResult value) noexcept
{
    /* Exactly 0..4; MaxExclusive (5) and unknown values are never remapped. */
    return static_cast<std::uint32_t>(value) < static_cast<std::uint32_t>(Result::MaxExclusive);
}
ams_mel_string_view_v1 text_view(const std::string& text) noexcept
{
    return {text.data(), text.size()};
}
bool valid_status(const mel::CalibrationStatus& input) noexcept
{
    /* Validate both independent outer collections and every nested item without
     * allocating. No equal lengths, correspondence, uniqueness or ordering
     * relationship between active and completed is assumed. */
    const auto& active = input.getActiveCalibrationIDs();
    const auto& completed = input.getCompletedCalibrations();
    if (!checked_count<ams_mel_uci_id_v1>(active.size()) ||
        !checked_count<ams_mel_ir_completed_calibration_v1>(completed.size())) return false;
    for (const auto& id : active) if (!valid_id(id)) return false;
    for (const auto& entry : completed) {
        if (!valid_id(entry.getCalibrationID()) || !valid_result(entry.getResult()) ||
            !valid_text(entry.getFailReason()) ||
            !checked_count<ams_mel_ir_completed_calibration_item_v1>(entry.getCalibrationItems().size()))
            return false;
        for (const auto& item : entry.getCalibrationItems())
            if (!valid_text(item.getItemName()) || !valid_result(item.getResult()) ||
                !valid_text(item.getFailReason())) return false;
    }
    return true;
}
}
std::shared_ptr<const Snapshot> copy_status(const mel::CalibrationStatus& input)
{
    if (!valid_status(input)) return {};
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    if (const char *value = std::getenv("AMS_MEL_TEST_CALIBRATION_STATUS_COPY_FAILURE");
        value && std::strcmp(value, "allocation") == 0) {
        (void)unsetenv("AMS_MEL_TEST_CALIBRATION_STATUS_COPY_FAILURE");
        throw std::bad_alloc{};
    }
#endif
    auto result = std::make_shared<Snapshot>();
    result->values = input; // Deep copy: no provider object or deleter retained.
    /* Views reference result->values only. Every view vector is sized before
     * any pointer into it is published, so no later reallocation can occur. */
    const auto& owned_active = result->values.getActiveCalibrationIDs();
    const auto& owned_completed = result->values.getCompletedCalibrations();
    result->active.reserve(owned_active.size());
    for (const auto& id : owned_active) result->active.push_back(id_view(id));
    result->items.resize(owned_completed.size());
    result->completed.resize(owned_completed.size());
    for (std::size_t i = 0; i < owned_completed.size(); ++i) {
        const auto& source = owned_completed[i];
        auto& items = result->items[i];
        items.reserve(source.getCalibrationItems().size());
        for (const auto& item : source.getCalibrationItems())
            items.push_back({text_view(item.getItemName()),
                             static_cast<std::uint32_t>(item.getResult()),
                             text_view(item.getFailReason())});
        result->completed[i] = {id_view(source.getCalibrationID()),
            static_cast<std::int64_t>(source.getTimeTag().count()),
            static_cast<std::uint32_t>(source.getResult()),
            text_view(source.getFailReason()),
            {items.data(), items.size()}};
    }
    result->view = {{result->active.data(), result->active.size()},
                    {result->completed.data(), result->completed.size()}};
    return result;
}
ams_mel_status_t submit(const Family& family, const void *context,
                        const ams_mel_ir_calibration_status_command_v1 *command,
                        ams_mel_ir_calibration_status_request **output, char *out,
                        std::size_t capacity, std::size_t *required) noexcept
{
    irmel::CalibrationStatusCmd upstream;
    if (!command || !output || *output || (!out && capacity) ||
        !convert_command(*command, upstream)) {
        diagnostic("invalid CalibrationStatusCmd input", out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    std::shared_ptr<CalibrationStatusCompletion> completion;
    std::shared_ptr<CalibrationStatusWorkerInput> input;
    std::unique_ptr<ams_mel_ir_calibration_status_request> owner;
    std::unique_ptr<std::thread> worker;
    try {
        completion = std::make_shared<CalibrationStatusCompletion>();
        input = std::make_shared<CalibrationStatusWorkerInput>();
        input->completion = completion;
        owner = std::make_unique<ams_mel_ir_calibration_status_request>();
        owner->state = completion;
        worker = std::make_unique<std::thread>();
    } catch (...) {
        diagnostic("allocation failed before provider send", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    if (!acquire_completion_permit(family.admission(context), input->admission)) {
        diagnostic("async request limit reached", out, capacity, required);
        return AMS_MEL_RESOURCE_EXHAUSTED;
    }
    std::shared_ptr<void> channel;
    try {
        if (!family.claim(context, channel, completion->claim)) {
            diagnostic(family.ineligible, out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
    } catch (...) {
        if (!completion->claim.empty()) {
            /* Accounting was published but cannot be unwound as a refusal. */
            arm_worker(input);
            retain_worker(input);
        }
        diagnostic("CalibrationStatusCmd submission lock failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    try {
        try {
            /* No bridge lock is held: synchronous provider callbacks are safe. */
            input->future = family.send(channel, std::move(upstream));
            arm_worker(input);
        } catch (const std::exception& error) {
            channel.reset();
            (void)completion->claim.finish();
            const char *what = error.what();
            const std::string_view text = what ? std::string_view{what} : std::string_view{};
            write(!text.empty() && valid_utf8(text) ? text : "provider send exception",
                  out, capacity, required);
            return AMS_MEL_PROVIDER_EXCEPTION;
        } catch (...) {
            channel.reset();
            (void)completion->claim.finish();
            diagnostic("unknown provider send exception", out, capacity, required);
            return AMS_MEL_PROVIDER_EXCEPTION;
        }
        /* Only the accounted graph may own the channel when cleanup runs. */
        channel.reset();
        if (!input->future.valid()) {
            /* A future without shared state can never complete and owns no
             * provider result; release the claim normally, start no worker. */
            input->emergency_self.reset();
            (void)completion->claim.finish();
            diagnostic("provider returned an invalid CalibrationStatusCmd future", out, capacity,
                       required);
            return AMS_MEL_PROVIDER_FAILED;
        }
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
        const SubmitFailpoint failpoint = submit_failpoint();
        if (failpoint == SubmitFailpoint::Allocation) throw std::bad_alloc{};
        if (failpoint == SubmitFailpoint::WorkerLaunch)
            throw std::system_error{std::make_error_code(std::errc::resource_unavailable_try_again)};
#endif
        *worker = std::thread{[input] { run_worker(input); }};
        worker->detach();
        input->emergency_self.reset();
        input->launch_state.store(1U, std::memory_order_release);
        *output = owner.release();
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        retain_worker(input);
        diagnostic("facade allocation failed after provider send", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        retain_worker(input);
        if (worker && worker->joinable()) (void)worker.release();
        input->launch_state.store(2U, std::memory_order_release);
        diagnostic("facade worker launch failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

} // namespace ams_mel_calibration_status

extern "C" ams_mel_status_t ams_mel_ir_calibration_status_request_wait(
    const ams_mel_ir_calibration_status_request *request, std::uint32_t timeout_ms,
    ams_mel_ir_calibration_status_result_v1 *result, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    write("", out, capacity, required);
    if (!request || !request->state || !result || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        auto& state = *request->state;
        std::unique_lock lock{state.mutex};
        if (state.kind == CompletionKind::Pending &&
            !state.ready.wait_for(lock, std::chrono::milliseconds{timeout_ms},
                [&] { return state.kind != CompletionKind::Pending; }))
            return AMS_MEL_TIMEOUT;
        *result = state.result;
        write(state.message, out, capacity, required);
        switch (state.kind) {
        case CompletionKind::Success: return AMS_MEL_OK;
        case CompletionKind::Rejected: return AMS_MEL_COMMAND_REJECTED;
        case CompletionKind::ProviderException: return AMS_MEL_PROVIDER_EXCEPTION;
        case CompletionKind::ProviderFailure: return AMS_MEL_PROVIDER_FAILED;
        case CompletionKind::InternalError: return AMS_MEL_INTERNAL_ERROR;
        case CompletionKind::Pending: return AMS_MEL_TIMEOUT;
        }
    } catch (...) {
        write("CalibrationStatusCmd request wait failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    return AMS_MEL_INTERNAL_ERROR;
}

extern "C" ams_mel_status_t ams_mel_ir_calibration_status_request_close(
    ams_mel_ir_calibration_status_request **request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    write("", out, capacity, required);
    if (!request || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try { delete std::exchange(*request, nullptr); return AMS_MEL_OK; }
    catch (...) { write("request close failed", out, capacity, required); return AMS_MEL_INTERNAL_ERROR; }
}
/* ---- Opt-in C2 CalibrationStatus owner ------------------------------ */

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
namespace {
std::mutex status_callback_probe_mutex;
std::condition_variable status_callback_probe_ready;
bool status_callback_parked{}, status_callback_entered{};
}
extern "C" __attribute__((visibility("default"))) int ams_mel_test_calibration_status_callback(unsigned operation)
{
    std::unique_lock lock{status_callback_probe_mutex};
    if (operation == 0U) { status_callback_parked = true; status_callback_entered = false; }
    else if (operation == 1U) return status_callback_probe_ready.wait_for(lock, std::chrono::seconds{15},
        [] { return status_callback_entered; });
    else if (operation == 2U) { status_callback_parked = false; status_callback_probe_ready.notify_all(); }
    else return 0;
    return 1;
}
#endif

void CalibrationStatusState::receive(const mel::CalibrationStatus *input) noexcept
{
    bool counted = false;
    try {
        std::unique_lock lock{mutex};
        ++callbacks;
        counted = true;
        increment(counters.events_received);
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
        {
            std::unique_lock probe{status_callback_probe_mutex};
            if (status_callback_parked) {
                status_callback_entered = true;
                status_callback_probe_ready.notify_all();
                lock.unlock(); // Close can deactivate while this callback is counted in flight.
                if (!status_callback_probe_ready.wait_for(probe, std::chrono::seconds{15},
                        [] { return !status_callback_parked; })) std::abort();
                probe.unlock();
                lock.lock();
            }
        }
#endif
        if (lifecycle == CalibrationStatusLifecycle::Active) {
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
            if (const char *value = std::getenv("AMS_MEL_TEST_CALIBRATION_STATUS_CALLBACK_FAILURE");
                value && std::strcmp(value, "allocation") == 0) {
                (void)unsetenv("AMS_MEL_TEST_CALIBRATION_STATUS_CALLBACK_FAILURE");
                throw std::bad_alloc{};
            }
#endif
            /* A full queue still validates malformed data, but never constructs
             * a snapshot. With space, copy_status validates exactly once and
             * completes the deep copy before the provider callback returns. */
            if (!input)
                increment(counters.malformed_or_unsupported);
            else if (queue.size() >= capacity) {
                if (ams_mel_calibration_status::valid_status(*input))
                    increment(counters.events_dropped_queue_full);
                else
                    increment(counters.malformed_or_unsupported);
            } else if (auto copied = ams_mel_calibration_status::copy_status(*input)) {
                queue.push_back(copied);
                ready.notify_one();
            } else
                increment(counters.malformed_or_unsupported);
        }
        if (--callbacks == 0U) callbacks_done.notify_all();
    } catch (...) {
        /* No exception crosses the provider callback boundary. A copy or
         * allocation failure rejects this event only. */
        try {
            std::lock_guard lock{mutex};
            increment(counters.malformed_or_unsupported);
            if (counted && --callbacks == 0U) callbacks_done.notify_all();
        } catch (...) {
        }
    }
}

void CalibrationStatusState::deactivate() noexcept
{
    try {
        std::lock_guard lock{mutex};
        if (lifecycle == CalibrationStatusLifecycle::Active)
            lifecycle = CalibrationStatusLifecycle::Inactive;
        ready.notify_all();
    } catch (...) {
    }
}

void CalibrationStatusState::fail() noexcept
{
    try {
        std::lock_guard lock{mutex};
        lifecycle = CalibrationStatusLifecycle::Failed;
        ready.notify_all();
    } catch (...) {
    }
}

void CalibrationStatusState::drain_and_stop() noexcept
{
    try {
        std::unique_lock lock{mutex};
        callbacks_done.wait(lock, [this] { return callbacks == 0U; });
        if (lifecycle != CalibrationStatusLifecycle::Failed)
            lifecycle = CalibrationStatusLifecycle::Stopped;
        ready.notify_all();
    } catch (...) {
    }
}
extern "C" ams_mel_status_t ams_mel_ir_calibration_status_receive(
    ams_mel_ir_calibration_status_metadata *metadata, std::uint32_t timeout_ms,
    ams_mel_ir_calibration_status_event **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    write("", out, capacity, required);
    if (!metadata || !metadata->state || !output || *output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        auto& state = *metadata->state;
        auto owner = std::make_unique<ams_mel_ir_calibration_status_event>();
        std::unique_lock lock{state.mutex};
        if (state.queue.empty() && state.lifecycle == CalibrationStatusLifecycle::Active && timeout_ms)
            state.ready.wait_for(lock, std::chrono::milliseconds{timeout_ms}, [&] {
                return !state.queue.empty() || state.lifecycle != CalibrationStatusLifecycle::Active;
            });
        if (!state.queue.empty()) {
            owner->snapshot = state.queue.front();
            state.queue.pop_front();
            *output = owner.release();
            return AMS_MEL_OK;
        }
        if (state.lifecycle == CalibrationStatusLifecycle::Failed) {
            write("CalibrationStatus channel cleanup failed", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        return state.lifecycle == CalibrationStatusLifecycle::Active ? AMS_MEL_TIMEOUT
                                                                 : AMS_MEL_STREAM_STOPPED;
    } catch (...) {
        write("CalibrationStatus metadata receive failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_calibration_status_get_counters(
    const ams_mel_ir_calibration_status_metadata *metadata,
    ams_mel_ir_metadata_counters_v1 *output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    write("", out, capacity, required);
    if (!metadata || !metadata->state || !output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::lock_guard lock{metadata->state->mutex};
        *output = metadata->state->counters;
        return AMS_MEL_OK;
    } catch (...) {
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_calibration_status_close(
    ams_mel_ir_calibration_status_metadata **metadata, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    write("", out, capacity, required);
    if (!metadata || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    auto *owner = std::exchange(*metadata, nullptr);
    if (owner && owner->state) owner->state->deactivate();
    delete owner;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_calibration_status_event_view(
    const ams_mel_ir_calibration_status_event *event,
    const ams_mel_ir_calibration_status_v1 **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    write("", out, capacity, required);
    if (!event || !output || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    *output = &event->snapshot->view;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_calibration_status_event_close(
    ams_mel_ir_calibration_status_event **event, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    write("", out, capacity, required);
    if (!event || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    delete std::exchange(*event, nullptr);
    return AMS_MEL_OK;
}
