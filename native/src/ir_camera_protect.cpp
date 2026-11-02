#include <ams_mel/abi.h>
#include "internal/ir_camera_protect.hpp"

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

/* IRMEL-ADA-004: C2 CameraProtectCmd completion engine and the opt-in C2
 * CameraProtectCmdResp owner. The C2 claim/send adapter and the owner's open
 * export live in ir_c2.cpp beside the private C2 ChannelState. */

namespace {
using namespace ams::iface;
using ams_mel_common::CompletionKind;

static_assert(static_cast<std::uint32_t>(irmel::CmdResp::OK) == AMS_MEL_IR_CAMERA_PROTECT_OK);
static_assert(static_cast<std::uint32_t>(irmel::CmdResp::Unavailable) ==
              AMS_MEL_IR_CAMERA_PROTECT_UNAVAILABLE);
static_assert(static_cast<std::uint32_t>(irmel::CmdResp::Unsupported) ==
              AMS_MEL_IR_CAMERA_PROTECT_UNSUPPORTED);
static_assert(std::is_same_v<std::underlying_type_t<irmel::CmdResp>, std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<irmel::CameraProtectCmd>().getCommandID()),
                             std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<irmel::CameraProtectCmd>().getResponseTime()),
                             std::chrono::nanoseconds>);
static_assert(std::is_same_v<decltype(std::declval<irmel::CameraProtectCmd>().getProtectFlag()),
                             bool>);
static_assert(std::is_same_v<decltype(std::declval<irmel::CameraProtectCmdResp>().getCommandID()),
                             std::uint32_t>);
static_assert(std::is_same_v<
              decltype(std::declval<irmel::CameraProtectCmdResp>().getResponseTime()),
              std::chrono::nanoseconds>);
static_assert(sizeof(std::chrono::nanoseconds::rep) == 8U &&
              std::is_signed_v<std::chrono::nanoseconds::rep>);

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

void arm_worker(const std::shared_ptr<CameraProtectWorkerInput>& input) noexcept
{
    input->emergency_self = input;
}

/* Allocation-free process-lifetime retention of a graph whose future cannot
 * be consumed safely; the provider library is therefore never unloaded. */
void retain_worker(const std::shared_ptr<CameraProtectWorkerInput>& input) noexcept
{
    static std::atomic<CameraProtectWorkerInput *> retained{};
    bool expected = false;
    if (!input->emergency_retained.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) return;
    CameraProtectWorkerInput *head = retained.load(std::memory_order_relaxed);
    do { input->emergency_next = head; }
    while (!retained.compare_exchange_weak(
        head, input.get(), std::memory_order_release, std::memory_order_relaxed));
}
void complete(const std::shared_ptr<CameraProtectCompletion>& state,
              mel::RequestFor<irmel::CameraProtectCmdResp>& future)
{
    CompletionKind kind = CompletionKind::ProviderException;
    ams_mel_ir_camera_protect_result_v1 result{};
    std::string message;
    try {
        auto outcome = AMS_MEL_PROBE_GET(CameraProtect, future.get());
        if (outcome) {
            const auto& value = outcome.get();
            if (!value) {
                kind = CompletionKind::ProviderFailure;
                message = "provider returned null successful CameraProtectCmdResp";
            } else if (!ams_mel_camera_protect::convert_response(*value, result.response)) {
                /* Never remapped to a known alternative. */
                result = {};
                kind = CompletionKind::ProviderFailure;
                message = "provider returned unknown CameraProtectCmdResp CmdResp value";
            } else {
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
    AMS_MEL_PROBE_BOUNDARY(CameraProtect);
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
        state->result = result;
        state->message = std::move(message);
        AMS_MEL_PROBE_GRAPH_EMPTY(CameraProtect, state->claim.empty());
    }
    state->ready.notify_all();
}

void run_worker(const std::shared_ptr<CameraProtectWorkerInput>& input) noexcept
{
    while (input->launch_state.load(std::memory_order_acquire) == 0U)
        std::this_thread::yield();
    AMS_MEL_PROBE_WORKER(CameraProtect);
    try {
        complete(input->completion, input->future); AMS_MEL_PROBE_RETURN(CameraProtect);
    } catch (...) {
        arm_worker(input);
        retain_worker(input);
    }
}

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
enum class SubmitFailpoint { None, Allocation, WorkerLaunch };
SubmitFailpoint submit_failpoint() noexcept
{
    const char *value = std::getenv("AMS_MEL_TEST_CAMERA_PROTECT_POST_SEND_FAILURE");
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

namespace ams_mel_camera_protect {

void diagnostic(const char *text, char *out, std::size_t capacity, std::size_t *required) noexcept
{
    write(text ? std::string_view{text} : std::string_view{}, out, capacity, required);
}

bool convert_command(const ams_mel_ir_camera_protect_command_v1& input,
                     irmel::CameraProtectCmd& output) noexcept
{
    /* Exactly the two Boolean representations; never a truthiness test. */
    if (input.protect_flag != 0U && input.protect_flag != 1U) return false;
    output.setCommandID(input.command_id);
    output.setResponseTime(std::chrono::nanoseconds{input.response_time_ns});
    output.setProtectFlag(input.protect_flag == 1U);
    return true;
}

bool convert_response(const irmel::CameraProtectCmdResp& input,
                      ams_mel_ir_camera_protect_response_v1& output) noexcept
{
    const auto value = static_cast<std::uint32_t>(input.getCommandResponse());
    switch (value) {
    case AMS_MEL_IR_CAMERA_PROTECT_OK:
    case AMS_MEL_IR_CAMERA_PROTECT_UNAVAILABLE:
    case AMS_MEL_IR_CAMERA_PROTECT_UNSUPPORTED:
        break;
    default:
        return false;
    }
    output.command_id = input.getCommandID();
    output.response_time_ns = static_cast<std::int64_t>(input.getResponseTime().count());
    output.command_response = value;
    return true;
}
ams_mel_status_t submit(const Family& family, const void *context,
                        const ams_mel_ir_camera_protect_command_v1 *command,
                        ams_mel_ir_camera_protect_request **output, char *out,
                        std::size_t capacity, std::size_t *required) noexcept
{
    irmel::CameraProtectCmd upstream;
    if (!command || !output || *output || (!out && capacity) ||
        !convert_command(*command, upstream)) {
        diagnostic("invalid CameraProtectCmd input", out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    std::shared_ptr<CameraProtectCompletion> completion;
    std::shared_ptr<CameraProtectWorkerInput> input;
    std::unique_ptr<ams_mel_ir_camera_protect_request> owner;
    std::unique_ptr<std::thread> worker;
    try {
        completion = std::make_shared<CameraProtectCompletion>();
        input = std::make_shared<CameraProtectWorkerInput>();
        input->completion = completion;
        owner = std::make_unique<ams_mel_ir_camera_protect_request>();
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
        diagnostic("CameraProtectCmd submission lock failed", out, capacity, required);
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
            diagnostic("provider returned an invalid CameraProtectCmd future", out, capacity,
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

} // namespace ams_mel_camera_protect

extern "C" ams_mel_status_t ams_mel_ir_camera_protect_request_wait(
    const ams_mel_ir_camera_protect_request *request, std::uint32_t timeout_ms,
    ams_mel_ir_camera_protect_result_v1 *result, char *out, std::size_t capacity,
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
        write("CameraProtectCmd request wait failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    return AMS_MEL_INTERNAL_ERROR;
}

extern "C" ams_mel_status_t ams_mel_ir_camera_protect_request_close(
    ams_mel_ir_camera_protect_request **request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    write("", out, capacity, required);
    if (!request || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try { delete std::exchange(*request, nullptr); return AMS_MEL_OK; }
    catch (...) { write("request close failed", out, capacity, required); return AMS_MEL_INTERNAL_ERROR; }
}
/* ---- Opt-in C2 CameraProtectCmdResp owner ------------------------------ */

void CameraProtectState::receive(const irmel::CameraProtectCmdResp *input) noexcept
{
    bool counted = false;
    try {
        std::lock_guard lock{mutex};
        ++callbacks;
        counted = true;
        increment(counters.events_received);
        if (lifecycle == CameraProtectLifecycle::Active) {
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
            if (const char *value = std::getenv("AMS_MEL_TEST_CAMERA_PROTECT_CALLBACK_FAILURE");
                value && std::strcmp(value, "allocation") == 0) {
                (void)unsetenv("AMS_MEL_TEST_CAMERA_PROTECT_CALLBACK_FAILURE");
                throw std::bad_alloc{};
            }
#endif
            /* Complete fixed-size copy before the provider callback returns.
             * Null payloads and unknown CmdResp values are never remapped. */
            ams_mel_ir_camera_protect_response_v1 copied{};
            if (!input || !ams_mel_camera_protect::convert_response(*input, copied))
                increment(counters.malformed_or_unsupported);
            else if (queue.size() >= capacity)
                increment(counters.events_dropped_queue_full);
            else {
                queue.push_back(copied);
                ready.notify_one();
            }
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

void CameraProtectState::deactivate() noexcept
{
    try {
        std::lock_guard lock{mutex};
        if (lifecycle == CameraProtectLifecycle::Active)
            lifecycle = CameraProtectLifecycle::Inactive;
        ready.notify_all();
    } catch (...) {
    }
}

void CameraProtectState::fail() noexcept
{
    try {
        std::lock_guard lock{mutex};
        lifecycle = CameraProtectLifecycle::Failed;
        ready.notify_all();
    } catch (...) {
    }
}

void CameraProtectState::drain_and_stop() noexcept
{
    try {
        std::unique_lock lock{mutex};
        callbacks_done.wait(lock, [this] { return callbacks == 0U; });
        if (lifecycle != CameraProtectLifecycle::Failed)
            lifecycle = CameraProtectLifecycle::Stopped;
        ready.notify_all();
    } catch (...) {
    }
}
extern "C" ams_mel_status_t ams_mel_ir_camera_protect_receive(
    ams_mel_ir_camera_protect_metadata *metadata, std::uint32_t timeout_ms,
    ams_mel_ir_camera_protect_event **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    write("", out, capacity, required);
    if (!metadata || !metadata->state || !output || *output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        auto& state = *metadata->state;
        auto owner = std::make_unique<ams_mel_ir_camera_protect_event>();
        std::unique_lock lock{state.mutex};
        if (state.queue.empty() && state.lifecycle == CameraProtectLifecycle::Active && timeout_ms)
            state.ready.wait_for(lock, std::chrono::milliseconds{timeout_ms}, [&] {
                return !state.queue.empty() || state.lifecycle != CameraProtectLifecycle::Active;
            });
        if (!state.queue.empty()) {
            owner->view = state.queue.front();
            state.queue.pop_front();
            *output = owner.release();
            return AMS_MEL_OK;
        }
        if (state.lifecycle == CameraProtectLifecycle::Failed) {
            write("CameraProtectCmdResp channel cleanup failed", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        return state.lifecycle == CameraProtectLifecycle::Active ? AMS_MEL_TIMEOUT
                                                                 : AMS_MEL_STREAM_STOPPED;
    } catch (...) {
        write("CameraProtectCmdResp metadata receive failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_camera_protect_get_counters(
    const ams_mel_ir_camera_protect_metadata *metadata,
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

extern "C" ams_mel_status_t ams_mel_ir_camera_protect_close(
    ams_mel_ir_camera_protect_metadata **metadata, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    write("", out, capacity, required);
    if (!metadata || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    auto *owner = std::exchange(*metadata, nullptr);
    if (owner && owner->state) owner->state->deactivate();
    delete owner;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_camera_protect_event_view(
    const ams_mel_ir_camera_protect_event *event,
    const ams_mel_ir_camera_protect_response_v1 **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    write("", out, capacity, required);
    if (!event || !output || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    *output = &event->view;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_camera_protect_event_close(
    ams_mel_ir_camera_protect_event **event, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    write("", out, capacity, required);
    if (!event || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    delete std::exchange(*event, nullptr);
    return AMS_MEL_OK;
}
