#include "internal/common_channel_requests.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <new>
#include <system_error>
#include <string_view>
#include <thread>
#include <utility>

namespace {
using namespace ams::iface;
using namespace ams_mel_common;
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

void diagnostic(std::string_view text, char *out, std::size_t capacity,
                std::size_t *required) noexcept
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

bool map_return(irmel::Return input, ams_mel_ir_return_t& value) noexcept
{
    switch (input) {
    case irmel::Return::Success: value = AMS_MEL_IR_RETURN_SUCCESS; return true;
    case irmel::Return::BadPointer: value = AMS_MEL_IR_RETURN_BAD_POINTER; return true;
    case irmel::Return::Fail: value = AMS_MEL_IR_RETURN_FAIL; return true;
    case irmel::Return::NotSupported: value = AMS_MEL_IR_RETURN_NOT_SUPPORTED; return true;
    case irmel::Return::NotImplemented: value = AMS_MEL_IR_RETURN_NOT_IMPLEMENTED; return true;
    }
    return false;
}

} // namespace

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
struct ams_mel_test_common_channel {
    explicit ams_mel_test_common_channel(CommonChannelAccess value) noexcept : access{std::move(value)} {}
    CommonChannelAccess access;
};
#endif

namespace {
using namespace ams::iface;
using namespace ams_mel_common;
enum class CommonFailure { None, Allocation, WorkerLaunch };
CommonFailure common_failure() noexcept
{
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    const char *value = std::getenv("AMS_MEL_TEST_COMMON_POST_SEND_FAILURE");
    if (value && std::strcmp(value, "allocation") == 0) return CommonFailure::Allocation;
    if (value && std::strcmp(value, "worker-launch") == 0) return CommonFailure::WorkerLaunch;
#endif
    return CommonFailure::None;
}

template<typename Completion, typename Input, typename Owner, typename Send,
         typename Arm, typename Retain, typename Run>
ams_mel_status_t submit_common(const CommonChannelAccess& access, Owner **output,
    Send send, Arm arm, Retain retain, Run run,
    char *out, std::size_t capacity, std::size_t *required) noexcept
{
    std::shared_ptr<Completion> completion;
    std::shared_ptr<Input> input;
    std::unique_ptr<Owner> owner;
    std::unique_ptr<std::thread> worker;
    try {
        completion = std::make_shared<Completion>();
        input = std::make_shared<Input>();
        input->completion = completion;
        owner = std::make_unique<Owner>();
        owner->state = completion;
        worker = std::make_unique<std::thread>();
    } catch (...) {
        diagnostic("allocation failed before provider send", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    auto state = access.lock();
    if (!state) {
        diagnostic("common channel is no longer available", out, capacity, required);
        return AMS_MEL_PROVIDER_FAILED;
    }
    std::shared_ptr<irmel::Channel> channel;
    try {
        if (!acquire_completion_permit(access.admission(state), input->admission)) {
            diagnostic("async request limit reached", out, capacity, required);
            return AMS_MEL_RESOURCE_EXHAUSTED;
        }
        if (!access.claim(state, channel, completion->claim)) {
            diagnostic("common channel is not available", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
    } catch (...) {
        /* An adapter throwing after claim cannot be unwound as a normal
         * refusal: keep the claim and provider graph forever instead. */
        if (!completion->claim.empty()) {
            arm(input);
            retain(input);
        }
        diagnostic("common channel submission lock failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    try {
        try {
            input->future = send(*channel);
            arm(input);
        } catch (const std::exception& error) {
            channel.reset();
            (void)completion->claim.finish();
            const char *what = error.what();
            const std::string_view text = what ? std::string_view{what} : std::string_view{};
            diagnostic(!text.empty() && valid_utf8(text) ? text : "provider send exception",
                       out, capacity, required);
            return AMS_MEL_PROVIDER_EXCEPTION;
        } catch (...) {
            channel.reset();
            (void)completion->claim.finish();
            diagnostic("unknown provider send exception", out, capacity, required);
            return AMS_MEL_PROVIDER_EXCEPTION;
        }
        channel.reset();
        const auto failure = common_failure();
        if (failure == CommonFailure::Allocation) throw std::bad_alloc{};
        if (failure == CommonFailure::WorkerLaunch)
            throw std::system_error{std::make_error_code(std::errc::resource_unavailable_try_again)};
        *worker = std::thread{[input, run] { run(input); }};
        worker->detach();
        input->emergency_self.reset();
        input->launch_state.store(1U, std::memory_order_release);
        *output = owner.release();
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        retain(input);
        diagnostic("facade allocation failed after provider send", out, capacity, required);
    } catch (...) {
        retain(input);
        if (worker && worker->joinable()) (void)worker.release();
        input->launch_state.store(2U, std::memory_order_release);
        diagnostic("facade worker launch failed", out, capacity, required);
    }
    return AMS_MEL_INTERNAL_ERROR;
}
} // namespace

namespace ams_mel_common {
using namespace ams::iface;
ams_mel_status_t submit_common_keepalive(const CommonChannelAccess& access,
    ams_mel_ir_return_request **request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!request || *request || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    return submit_common<ReturnCompletion, ReturnWorkerInput>(access, request,
        [](irmel::Channel& channel) { return channel.sendKeepAliveRep(); },
        arm_return_worker, retain_return_worker, run_return_worker, out, capacity, required);
}

ams_mel_status_t submit_common_comms(const CommonChannelAccess& access,
    const ams_mel_ir_channel_comms_test_request_v1 *command,
    ams_mel_ir_channel_comms_request **request, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!command || !request || *request || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try {
        irmel::ChannelCommsTestReq upstream;
        upstream.setCommandID(command->command_id);
        upstream.setChannelID(command->channel_id);
        upstream.setRequestID(command->request_id);
        return submit_common<CommsCompletion, CommsWorkerInput>(access, request,
            [value = std::move(upstream)](irmel::Channel& channel) mutable {
                return channel.send(std::move(value));
            }, arm_comms_worker, retain_comms_worker, run_comms_worker, out, capacity, required);
    } catch (...) {
        diagnostic("CommsTest command preparation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}
} // namespace ams_mel_common

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
using namespace ams_mel_common;
extern "C" __attribute__((visibility("default"))) ams_mel_status_t
ams_mel_test_common_from_c2(const ams_mel_ir_c2 *owner,
    ams_mel_test_common_channel **output) noexcept
{
    if (!owner || !output || *output) return AMS_MEL_INVALID_ARGUMENT;
    try { *output = new ams_mel_test_common_channel{common_from_c2(owner)}; return AMS_MEL_OK; }
    catch (...) { return AMS_MEL_INTERNAL_ERROR; }
}
extern "C" __attribute__((visibility("default"))) ams_mel_status_t
ams_mel_test_common_from_stream(const ams_mel_ir_stream *owner,
    ams_mel_test_common_channel **output) noexcept
{
    if (!owner || !output || *output) return AMS_MEL_INVALID_ARGUMENT;
    try { *output = new ams_mel_test_common_channel{common_from_stream(owner)}; return AMS_MEL_OK; }
    catch (...) { return AMS_MEL_INTERNAL_ERROR; }
}
extern "C" __attribute__((visibility("default"))) ams_mel_status_t
ams_mel_test_common_send_keepalive(const ams_mel_test_common_channel *owner,
    ams_mel_ir_return_request **request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    if (!owner) return AMS_MEL_INVALID_ARGUMENT;
    return submit_common_keepalive(owner->access, request, out, capacity, required);
}
extern "C" __attribute__((visibility("default"))) ams_mel_status_t
ams_mel_test_common_submit_comms_test(const ams_mel_test_common_channel *owner,
    const ams_mel_ir_channel_comms_test_request_v1 *command,
    ams_mel_ir_channel_comms_request **request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    if (!owner) return AMS_MEL_INVALID_ARGUMENT;
    return submit_common_comms(owner->access, command, request, out, capacity, required);
}
extern "C" __attribute__((visibility("default"))) void
ams_mel_test_common_close(ams_mel_test_common_channel **owner) noexcept
{ if (owner) { delete *owner; *owner = nullptr; } }
#endif

namespace ams_mel_common {
void arm_return_worker(const std::shared_ptr<ReturnWorkerInput>& input) noexcept
{
    input->emergency_self = input;
}

void retain_return_worker(const std::shared_ptr<ReturnWorkerInput>& input) noexcept
{
    static std::atomic<ReturnWorkerInput *> retained{};
    bool expected = false;
    if (!input->emergency_retained.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) return;
    ReturnWorkerInput *head = retained.load(std::memory_order_relaxed);
    do { input->emergency_next = head; }
    while (!retained.compare_exchange_weak(
        head, input.get(), std::memory_order_release, std::memory_order_relaxed));
}

void complete_return(const std::shared_ptr<ReturnCompletion>& state,
                     mel::RequestFor<irmel::Return>& future)
{
    CompletionKind kind = CompletionKind::ProviderException;
    ams_mel_ir_return_result_v1 result{};
    std::string message;
    try {
        auto outcome = AMS_MEL_PROBE_GET(Return, future.get());
        if (outcome) {
            const auto& value = outcome.get();
            if (!value) {
                kind = CompletionKind::ProviderFailure;
                message = "provider returned null successful Return result";
            } else if (!map_return(*value, result.value)) {
                kind = CompletionKind::ProviderFailure;
                message = "provider returned unknown IR Return value";
            } else {
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
    AMS_MEL_PROBE_BOUNDARY(Return);
    const char *failure = state->claim.failure_message();
    if (!state->claim.finish()) {
        kind = CompletionKind::ProviderFailure;
        try { message = failure; }
        catch (...) { message.clear(); }
    }
    {
        std::lock_guard lock{state->mutex};
        state->kind = kind;
        state->result = result;
        state->message = std::move(message);
        AMS_MEL_PROBE_GRAPH_EMPTY(Return, state->claim.empty());
    }
    state->ready.notify_all();
}

void run_return_worker(const std::shared_ptr<ReturnWorkerInput>& input) noexcept
{
    while (input->launch_state.load(std::memory_order_acquire) == 0U)
        std::this_thread::yield();
    AMS_MEL_PROBE_WORKER(Return);
    try {
        complete_return(input->completion, input->future); AMS_MEL_PROBE_RETURN(Return);
    } catch (...) {
        arm_return_worker(input);
        retain_return_worker(input);
    }
}

void arm_comms_worker(const std::shared_ptr<CommsWorkerInput>& input) noexcept
{ input->emergency_self = input; }
void retain_comms_worker(const std::shared_ptr<CommsWorkerInput>& input) noexcept
{
    static std::atomic<CommsWorkerInput *> retained{};
    bool expected = false;
    if (!input->emergency_retained.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) return;
    CommsWorkerInput *head = retained.load(std::memory_order_relaxed);
    do { input->emergency_next = head; }
    while (!retained.compare_exchange_weak(
        head, input.get(), std::memory_order_release, std::memory_order_relaxed));
}
void complete_comms(const std::shared_ptr<CommsCompletion>& state,
                    mel::RequestFor<irmel::ChannelCommsTestRep>& future)
{
    CompletionKind kind = CompletionKind::ProviderException;
    ams_mel_ir_channel_comms_test_result_v1 result{};
    std::string message;
    try {
        auto outcome = AMS_MEL_PROBE_GET(Comms, future.get());
        if (outcome) {
            const auto& value = outcome.get();
            if (!value) {
                kind = CompletionKind::ProviderFailure;
                message = "provider returned null successful CommsTest result";
            } else {
                result.command_id = value->getCommandID();
                result.request_id = value->getRequestID();
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
    AMS_MEL_PROBE_BOUNDARY(Comms);
    const char *failure = state->claim.failure_message();
    if (!state->claim.finish()) {
        kind = CompletionKind::ProviderFailure;
        try { message = failure; } catch (...) { message.clear(); }
    }
    {
        std::lock_guard lock{state->mutex};
        state->kind = kind; state->result = result;
        state->message = std::move(message);
        AMS_MEL_PROBE_GRAPH_EMPTY(Comms, state->claim.empty());
    }
    state->ready.notify_all();
}
void run_comms_worker(const std::shared_ptr<CommsWorkerInput>& input) noexcept
{
    while (input->launch_state.load(std::memory_order_acquire) == 0U)
        std::this_thread::yield();
    AMS_MEL_PROBE_WORKER(Comms);
    try { complete_comms(input->completion, input->future); AMS_MEL_PROBE_RETURN(Comms); }
    catch (...) { arm_comms_worker(input); retain_comms_worker(input); }
}
} // namespace ams_mel_common

extern "C" ams_mel_status_t ams_mel_ir_channel_comms_request_wait(
    const ams_mel_ir_channel_comms_request *request, std::uint32_t timeout_ms,
    ams_mel_ir_channel_comms_test_result_v1 *result, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!request || !request->state || !result || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::unique_lock lock{request->state->mutex};
        if (request->state->kind == CompletionKind::Pending &&
            !request->state->ready.wait_for(lock, std::chrono::milliseconds{timeout_ms},
                [&] { return request->state->kind != CompletionKind::Pending; }))
            return AMS_MEL_TIMEOUT;
        *result = request->state->result;
        diagnostic(request->state->message, out, capacity, required);
        switch (request->state->kind) {
        case CompletionKind::Success: return AMS_MEL_OK;
        case CompletionKind::Rejected: return AMS_MEL_COMMAND_REJECTED;
        case CompletionKind::ProviderException: return AMS_MEL_PROVIDER_EXCEPTION;
        case CompletionKind::ProviderFailure: return AMS_MEL_PROVIDER_FAILED;
        case CompletionKind::InternalError: return AMS_MEL_INTERNAL_ERROR;
        case CompletionKind::Pending: return AMS_MEL_TIMEOUT;
        }
    } catch (...) {
        diagnostic("CommsTest request wait failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    return AMS_MEL_INTERNAL_ERROR;
}

extern "C" ams_mel_status_t ams_mel_ir_channel_comms_request_close(
    ams_mel_ir_channel_comms_request **request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!request || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try { delete std::exchange(*request, nullptr); return AMS_MEL_OK; }
    catch (...) { return AMS_MEL_INTERNAL_ERROR; }
}
extern "C" ams_mel_status_t ams_mel_ir_return_request_wait(
    const ams_mel_ir_return_request *request, std::uint32_t timeout_ms,
    ams_mel_ir_return_result_v1 *result, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!request || !request->state || !result || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::unique_lock lock{request->state->mutex};
        if (request->state->kind == CompletionKind::Pending &&
            !request->state->ready.wait_for(lock, std::chrono::milliseconds{timeout_ms},
                [&] { return request->state->kind != CompletionKind::Pending; }))
            return AMS_MEL_TIMEOUT;
        *result = request->state->result;
        diagnostic(request->state->message, out, capacity, required);
        switch (request->state->kind) {
        case CompletionKind::Success: return AMS_MEL_OK;
        case CompletionKind::Rejected: return AMS_MEL_COMMAND_REJECTED;
        case CompletionKind::ProviderException: return AMS_MEL_PROVIDER_EXCEPTION;
        case CompletionKind::ProviderFailure: return AMS_MEL_PROVIDER_FAILED;
        case CompletionKind::InternalError: return AMS_MEL_INTERNAL_ERROR;
        case CompletionKind::Pending: return AMS_MEL_TIMEOUT;
        }
    } catch (...) {
        diagnostic("return request wait failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    return AMS_MEL_INTERNAL_ERROR;
}

extern "C" ams_mel_status_t ams_mel_ir_return_request_close(
    ams_mel_ir_return_request **request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!request || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try { delete std::exchange(*request, nullptr); return AMS_MEL_OK; }
    catch (...) { diagnostic("request close failed", out, capacity, required); return AMS_MEL_INTERNAL_ERROR; }
}
