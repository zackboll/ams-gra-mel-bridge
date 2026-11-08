#include <ams_mel/abi.h>
#include "internal/ir_erase.hpp"

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

/* IRMEL-ADA-007: optional C2 EraseCommand request engine; no callbacks or retries. */

namespace {
using namespace ams::iface;
using ams_mel_common::CompletionKind;

static_assert(std::is_same_v<std::underlying_type_t<irmel::EraseCommandType>, std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<irmel::EraseCommand>().getCommandID()), std::uint32_t>);
static_assert(static_cast<std::uint32_t>(irmel::EraseCommandType::Zeroize) == AMS_MEL_IR_ERASE_ZEROIZE);
static_assert(static_cast<std::uint32_t>(irmel::EraseCommandType::Classified_data_erase) == AMS_MEL_IR_ERASE_CLASSIFIED_DATA_ERASE);
static_assert(static_cast<std::uint32_t>(irmel::EraseCommandType::Disable) == AMS_MEL_IR_ERASE_DISABLE);
static_assert(static_cast<std::uint32_t>(irmel::EraseCommandType::CDE_AND_Zeroize) == AMS_MEL_IR_ERASE_CDE_AND_ZEROIZE);

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

void arm_worker(const std::shared_ptr<EraseWorkerInput>& input) noexcept
{
    input->emergency_self = input;
}

/* Allocation-free process-lifetime retention of a graph whose future cannot
 * be consumed safely; the provider library is therefore never unloaded. */
void retain_worker(const std::shared_ptr<EraseWorkerInput>& input) noexcept
{
    static std::atomic<EraseWorkerInput *> retained{};
    bool expected = false;
    if (!input->emergency_retained.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) return;
    EraseWorkerInput *head = retained.load(std::memory_order_relaxed);
    do { input->emergency_next = head; }
    while (!retained.compare_exchange_weak(
        head, input.get(), std::memory_order_release, std::memory_order_relaxed));
}
void complete(const std::shared_ptr<EraseCompletion>& state,
              mel::RequestFor<irmel::EraseCommandType>& future)
{
    CompletionKind kind = CompletionKind::ProviderException;
    ams_mel_ir_erase_result_v1 result{};
    std::string message;
    try {
        auto outcome = AMS_MEL_PROBE_GET(Erase, future.get());
        if (outcome) {
            const auto& value = outcome.get();
            if (!value) {
                kind = CompletionKind::ProviderFailure;
                message = "provider returned null successful EraseCommandType";
            } else if (!ams_mel_erase::convert_response(*value, result.erase_type)) {
                /* Never remapped to a known alternative. */
                result = {};
                kind = CompletionKind::ProviderFailure;
                message = "provider returned unknown EraseCommandType value";
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
                const std::string& description = error.getDescription();
                if (valid_utf8(description)) {
                    kind = CompletionKind::Rejected;
                    message = description;
                } else {
                    /* Never report a rejection with a silently replaced or
                     * truncated diagnostic. It is an invalid provider payload. */
                    kind = CompletionKind::ProviderFailure;
                    message = "provider rejection description was invalid UTF-8 or contained NUL";
                }
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
    AMS_MEL_PROBE_BOUNDARY(Erase);
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
        AMS_MEL_PROBE_GRAPH_EMPTY(Erase, state->claim.empty());
    }
    state->ready.notify_all();
}

void run_worker(const std::shared_ptr<EraseWorkerInput>& input) noexcept
{
    while (input->launch_state.load(std::memory_order_acquire) == 0U)
        std::this_thread::yield();
    AMS_MEL_PROBE_WORKER(Erase);
    try {
        complete(input->completion, input->future); AMS_MEL_PROBE_RETURN(Erase);
    } catch (...) {
        arm_worker(input);
        retain_worker(input);
    }
}

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
enum class SubmitFailpoint { None, Allocation, WorkerLaunch };
SubmitFailpoint submit_failpoint() noexcept
{
    const char *value = std::getenv("AMS_MEL_TEST_ERASE_POST_SEND_FAILURE");
    if (value && std::strcmp(value, "allocation") == 0) return SubmitFailpoint::Allocation;
    if (value && std::strcmp(value, "worker-launch") == 0) return SubmitFailpoint::WorkerLaunch;
    return SubmitFailpoint::None;
}
#endif

} // namespace

namespace ams_mel_erase {

void diagnostic(const char *text, char *out, std::size_t capacity, std::size_t *required) noexcept
{
    write(text ? std::string_view{text} : std::string_view{}, out, capacity, required);
}

bool convert_command(const ams_mel_ir_erase_command_v1& input,
                     irmel::EraseCommand& output) noexcept
{
    if (input.erase_type > AMS_MEL_IR_ERASE_CDE_AND_ZEROIZE) return false;
    output.setCommandID(input.command_id);
    output.setEraseCommandType(static_cast<irmel::EraseCommandType>(input.erase_type));
    return true;
}

bool convert_response(const irmel::EraseCommandType& input,
                      ams_mel_ir_erase_type_t& output) noexcept
{
    const auto value = static_cast<std::uint32_t>(input);
    if (value > AMS_MEL_IR_ERASE_CDE_AND_ZEROIZE) return false;
    output = value;
    return true;
}
ams_mel_status_t submit(const Family& family, const void *context,
                        const ams_mel_ir_erase_command_v1 *command,
                        ams_mel_ir_erase_request **output, char *out,
                        std::size_t capacity, std::size_t *required) noexcept
{
    irmel::EraseCommand upstream;
    if (!command || !output || *output || (!out && capacity) ||
        !convert_command(*command, upstream)) {
        diagnostic("invalid EraseCommand input", out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    std::shared_ptr<EraseCompletion> completion;
    std::shared_ptr<EraseWorkerInput> input;
    std::unique_ptr<ams_mel_ir_erase_request> owner;
    std::unique_ptr<std::thread> worker;
    try {
        completion = std::make_shared<EraseCompletion>();
        input = std::make_shared<EraseWorkerInput>();
        input->completion = completion;
        owner = std::make_unique<ams_mel_ir_erase_request>();
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
        diagnostic("EraseCommand submission lock failed", out, capacity, required);
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
            diagnostic("provider returned an invalid EraseCommand future", out, capacity,
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

} // namespace ams_mel_erase

extern "C" ams_mel_status_t ams_mel_ir_erase_request_wait(
    const ams_mel_ir_erase_request *request, std::uint32_t timeout_ms,
    ams_mel_ir_erase_result_v1 *result, char *out, std::size_t capacity,
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
        write("EraseCommand request wait failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    return AMS_MEL_INTERNAL_ERROR;
}

extern "C" ams_mel_status_t ams_mel_ir_erase_request_close(
    ams_mel_ir_erase_request **request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    write("", out, capacity, required);
    if (!request || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try { delete std::exchange(*request, nullptr); return AMS_MEL_OK; }
    catch (...) { write("request close failed", out, capacity, required); return AMS_MEL_INTERNAL_ERROR; }
}
