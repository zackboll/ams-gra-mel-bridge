#include <ams_mel/abi.h>
#include "internal/ir_c2_system_response.hpp"
#include "internal/provider_common.hpp"
#include "internal/system_track_data.hpp"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>

/* IRMEL-ADA-008: optional C2Channel::send(SystemTrackDataResponse) request
 * engine. Independent of the Track overload: C2 lifecycle, C2 request
 * accounting, its own opaque request and its own probe family. No callbacks,
 * no retries, no replay. */

namespace {
using namespace ams::iface;
using ams_mel_common::CompletionKind;
using ams_mel::internal::valid_utf8;

void write(std::string_view text, char *out, std::size_t capacity, std::size_t *required) noexcept
{
    ams_mel::internal::write_diagnostic(text, out, capacity, required);
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

void arm_worker(const std::shared_ptr<C2SystemResponseWorkerInput>& input) noexcept
{
    input->emergency_self = input;
}

/* Allocation-free process-lifetime retention of a graph whose future cannot
 * be consumed safely; the provider library is therefore never unloaded. */
void retain_worker(const std::shared_ptr<C2SystemResponseWorkerInput>& input) noexcept
{
    static std::atomic<C2SystemResponseWorkerInput *> retained{};
    bool expected = false;
    if (!input->emergency_retained.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) return;
    C2SystemResponseWorkerInput *head = retained.load(std::memory_order_relaxed);
    do { input->emergency_next = head; }
    while (!retained.compare_exchange_weak(
        head, input.get(), std::memory_order_release, std::memory_order_relaxed));
}

/* Successful or rejected ErrorOr outcome classification. Returns the kind and
 * fills result/reason_text/message. Exceptions propagate to complete(). */
CompletionKind classify(mel::ErrorOr<std::shared_ptr<irmel::CommandStatus>>& outcome,
                        ams_mel_ir_c2_system_response_result_v1& result,
                        std::string& reason_text, std::string& message)
{
    if (outcome) {
        const auto& value = outcome.get();
        if (!value) {
            message = "provider returned null successful CommandStatus";
            return CompletionKind::ProviderFailure;
        }
        if (!ams_mel_system_track_data::copy_command_status(*value, result.status, reason_text)) {
            /* Never remapped to a known alternative or truncated. */
            result = {};
            reason_text.clear();
            message = "provider returned malformed C2 CommandStatus";
            return CompletionKind::ProviderFailure;
        }
        /* A CommandStatus whose own state is Rejected is still a successful
         * future outcome and stays AMS_MEL_OK. */
        result.error_code = AMS_MEL_ERROR_NONE;
        return CompletionKind::Success;
    }
    const mel::Error& error = outcome.getError();
    bool known = false;
    result.error_code = map_error(error.getCode(), known);
    if (!known) {
        result.error_code = AMS_MEL_ERROR_NONE;
        message = "provider returned unknown MEL error code";
        return CompletionKind::ProviderFailure;
    }
    const std::string& description = error.getDescription();
    if (!valid_utf8(description)) {
        /* Never report a rejection with a silently replaced or truncated
         * diagnostic. It is an invalid provider payload. */
        message = "provider rejection description was invalid UTF-8 or contained NUL";
        return CompletionKind::ProviderFailure;
    }
    message = description;
    return CompletionKind::Rejected;
}

/* The single future::get() caller. No other thread may call it. */
void complete(const std::shared_ptr<C2SystemResponseCompletion>& state,
              mel::RequestFor<irmel::CommandStatus>& future)
{
    CompletionKind kind = CompletionKind::ProviderException;
    ams_mel_ir_c2_system_response_result_v1 result{};
    std::string reason_text;
    std::string message;
    try {
        auto outcome = AMS_MEL_PROBE_GET(C2SystemResponse, future.get());
        kind = classify(outcome, result, reason_text, message);
    } catch (const std::bad_alloc&) {
        kind = CompletionKind::InternalError;
        result = {};
        reason_text.clear();
        message.clear();
    } catch (const std::exception& error) {
        kind = CompletionKind::ProviderException;
        result = {};
        reason_text.clear();
        const char *what = error.what();
        const std::string_view text = what ? std::string_view{what} : std::string_view{};
        try { message = !text.empty() && valid_utf8(text) ? text : "provider future exception"; }
        catch (...) { message.clear(); }
    } catch (...) {
        kind = CompletionKind::ProviderException;
        result = {};
        reason_text.clear();
        try { message = "unknown provider future exception"; } catch (...) {}
    }
    AMS_MEL_PROBE_BOUNDARY(C2SystemResponse);
    /* The future and provider result are consumed; only now may the C2
     * request accounting and any deferred physical teardown be released. */
    const char *failure = state->claim.failure_message();
    if (!state->claim.finish()) {
        kind = CompletionKind::ProviderFailure;
        result = {};
        reason_text.clear();
        try { message = failure ? failure : "deferred channel cleanup failed"; }
        catch (...) { message.clear(); }
    }
    {
        std::lock_guard lock{state->mutex};
        state->kind = kind;
        state->result = result;
        state->reason_text = std::move(reason_text);
        /* The public view always points into request-owned cached storage. */
        state->result.status.reason_description.data =
            state->reason_text.empty() ? nullptr : state->reason_text.data();
        state->result.status.reason_description.size = state->reason_text.size();
        state->message = std::move(message);
        AMS_MEL_PROBE_GRAPH_EMPTY(C2SystemResponse, state->claim.empty());
    }
    state->ready.notify_all();
}

void run_worker(const std::shared_ptr<C2SystemResponseWorkerInput>& input) noexcept
{
    while (input->launch_state.load(std::memory_order_acquire) == 0U)
        std::this_thread::yield();
    AMS_MEL_PROBE_WORKER(C2SystemResponse);
    try {
        complete(input->completion, input->future); AMS_MEL_PROBE_RETURN(C2SystemResponse);
    } catch (...) {
        arm_worker(input);
        retain_worker(input);
    }
}

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
enum class SubmitFailpoint { None, Allocation, WorkerLaunch };
SubmitFailpoint submit_failpoint() noexcept
{
    const char *value = std::getenv("AMS_MEL_TEST_C2_SYSTEM_RESPONSE_POST_SEND_FAILURE");
    if (value && std::strcmp(value, "allocation") == 0) return SubmitFailpoint::Allocation;
    if (value && std::strcmp(value, "worker-launch") == 0) return SubmitFailpoint::WorkerLaunch;
    return SubmitFailpoint::None;
}
#endif

} // namespace

namespace ams_mel_c2_system_response {

void diagnostic(const char *text, char *out, std::size_t capacity, std::size_t *required) noexcept
{
    write(text ? std::string_view{text} : std::string_view{}, out, capacity, required);
}

ams_mel_status_t submit(const Family& family, const void *context,
                        const ams_mel_ir_system_track_data_response_v1 *response,
                        ams_mel_ir_c2_system_response_request **output, char *out,
                        std::size_t capacity, std::size_t *required) noexcept
{
    /* Malformed input (including any Boolean encoding other than 0/1) is
     * refused before allocation, admission, accounting or provider code. */
    if (!response || !output || *output || (!out && capacity) ||
        !ams_mel_system_track_data::valid_flags(*response)) {
        diagnostic("invalid SystemTrackDataResponse input", out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    std::shared_ptr<C2SystemResponseCompletion> completion;
    std::shared_ptr<C2SystemResponseWorkerInput> input;
    std::unique_ptr<ams_mel_ir_c2_system_response_request> owner;
    std::unique_ptr<std::thread> worker;
    try {
        completion = std::make_shared<C2SystemResponseCompletion>();
        input = std::make_shared<C2SystemResponseWorkerInput>();
        input->completion = completion;
        owner = std::make_unique<ams_mel_ir_c2_system_response_request>();
        owner->state = completion;
        worker = std::make_unique<std::thread>();
    } catch (...) {
        diagnostic("allocation failed before provider send", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    /* The complete response is converted and copied here; nothing borrowed
     * outlives this call. */
    irmel::SystemTrackDataResponse upstream;
    try {
        if (!ams_mel_system_track_data::build_response(*response, upstream))
            throw std::logic_error{"validated SystemTrackDataResponse flags changed"};
    } catch (...) {
        diagnostic("SystemTrackDataResponse preparation failed", out, capacity, required);
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
        diagnostic("SystemTrackDataResponse submission lock failed", out, capacity, required);
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
            diagnostic("provider returned an invalid C2 SystemTrackDataResponse future", out,
                       capacity, required);
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

} // namespace ams_mel_c2_system_response

extern "C" ams_mel_status_t ams_mel_ir_c2_system_response_request_wait(
    const ams_mel_ir_c2_system_response_request *request, std::uint32_t timeout_ms,
    ams_mel_ir_c2_system_response_result_v1 *result, char *out, std::size_t capacity,
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
            return AMS_MEL_TIMEOUT; /* the caller's record stays untouched */
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
        write("C2 SystemTrackDataResponse request wait failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    return AMS_MEL_INTERNAL_ERROR;
}

extern "C" ams_mel_status_t ams_mel_ir_c2_system_response_request_close(
    ams_mel_ir_c2_system_response_request **request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    write("", out, capacity, required);
    if (!request || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    /* Drops the public observer only; pending provider work is not cancelled. */
    try { delete std::exchange(*request, nullptr); return AMS_MEL_OK; }
    catch (...) { write("request close failed", out, capacity, required); return AMS_MEL_INTERNAL_ERROR; }
}
