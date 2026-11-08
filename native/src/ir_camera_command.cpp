#include <ams_mel/abi.h>
#include "internal/ir_camera_command.hpp"
#include "internal/ir_nuc.hpp"
#include "internal/ir_stream.hpp"

#include <irmel/library/image/ImageChannel.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <limits>
#include <new>
#include <string_view>
#include <system_error>
#include <thread>
#include <type_traits>
#include <utility>

/* IRMEL-ADA-003: one CameraCommand completion engine for two independent
 * upstream sends, plus the opt-in Image CameraCommandResp owner. */

namespace {
using namespace ams::iface;
using ams_mel_common::CommonRequestClaim;
using ams_mel_common::CompletionKind;

static_assert(static_cast<std::uint32_t>(irmel::ImageType::Staring) == AMS_MEL_IR_IMAGE_STARING);
static_assert(static_cast<std::uint32_t>(irmel::ImageType::Scanning) == AMS_MEL_IR_IMAGE_SCANNING);
static_assert(static_cast<std::uint32_t>(irmel::ImageType::Reserved13) ==
              AMS_MEL_IR_IMAGE_RESERVED13);
static_assert(static_cast<std::uint32_t>(irmel::ScanDir::NegToPosAz) ==
              AMS_MEL_IR_SCAN_DIR_NEG_TO_POS_AZ);
static_assert(static_cast<std::uint32_t>(irmel::ScanDir::PosToNegAz) ==
              AMS_MEL_IR_SCAN_DIR_POS_TO_NEG_AZ);
static_assert(std::is_same_v<std::underlying_type_t<irmel::ImageType>, std::uint32_t>);
static_assert(std::is_same_v<std::underlying_type_t<irmel::ScanDir>, std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<irmel::CameraCommand>().getSystemTime()),
                             std::chrono::nanoseconds>);
static_assert(std::is_same_v<decltype(std::declval<irmel::CameraCommand>().getIntegrationTime()),
                             std::chrono::nanoseconds>);
static_assert(std::is_same_v<decltype(std::declval<irmel::CameraCommandResp>().getTimestamp()),
                             std::chrono::nanoseconds>);
static_assert(sizeof(std::chrono::nanoseconds::rep) == 8U &&
              std::is_signed_v<std::chrono::nanoseconds::rep>);
static_assert(std::is_same_v<decltype(std::declval<irmel::CameraCommandResp>().getCommandSuccess()),
                             bool>);

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

ams_mel_ir_camera_command_response_v1 convert_response(const irmel::CameraCommandResp& value) noexcept
{
    ams_mel_ir_camera_command_response_v1 result{};
    result.command_id = value.getCommandID();
    result.size = value.getSize();
    result.timestamp_ns = static_cast<std::int64_t>(value.getTimestamp().count());
    result.command_success = value.getCommandSuccess() ? 1U : 0U;
    return result;
}

void arm_worker(const std::shared_ptr<CameraWorkerInput>& input) noexcept
{
    input->emergency_self = input;
}

/* Allocation-free process-lifetime retention of a graph whose future cannot
 * be consumed safely; the provider library is therefore never unloaded. */
void retain_worker(const std::shared_ptr<CameraWorkerInput>& input) noexcept
{
    static std::atomic<CameraWorkerInput *> retained{};
    bool expected = false;
    if (!input->emergency_retained.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) return;
    CameraWorkerInput *head = retained.load(std::memory_order_relaxed);
    do { input->emergency_next = head; }
    while (!retained.compare_exchange_weak(
        head, input.get(), std::memory_order_release, std::memory_order_relaxed));
}

void complete(const std::shared_ptr<CameraCompletion>& state,
              mel::RequestFor<irmel::CameraCommandResp>& future)
{
    CompletionKind kind = CompletionKind::ProviderException;
    ams_mel_ir_camera_command_result_v1 result{};
    std::string message;
    try {
        auto outcome = AMS_MEL_PROBE_GET(Camera, future.get());
        if (outcome) {
            const auto& value = outcome.get();
            if (!value) {
                kind = CompletionKind::ProviderFailure;
                message = "provider returned null successful CameraCommandResp";
            } else {
                result.response = convert_response(*value);
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
    AMS_MEL_PROBE_BOUNDARY(Camera);
    /* The future and provider result are consumed; only now may the family
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
        AMS_MEL_PROBE_GRAPH_EMPTY(Camera, state->claim.empty());
    }
    state->ready.notify_all();
}

void run_worker(const std::shared_ptr<CameraWorkerInput>& input) noexcept
{
    while (input->launch_state.load(std::memory_order_acquire) == 0U)
        std::this_thread::yield();
    AMS_MEL_PROBE_WORKER(Camera);
    try {
        complete(input->completion, input->future); AMS_MEL_PROBE_RETURN(Camera);
    } catch (...) {
        arm_worker(input);
        retain_worker(input);
    }
}

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
enum class SubmitFailpoint { None, Allocation, WorkerLaunch };
SubmitFailpoint submit_failpoint() noexcept
{
    const char *value = std::getenv("AMS_MEL_TEST_CAMERA_POST_SEND_FAILURE");
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

namespace ams_mel_camera {

void diagnostic(const char *text, char *out, std::size_t capacity, std::size_t *required) noexcept
{
    write(text ? std::string_view{text} : std::string_view{}, out, capacity, required);
}

bool convert_command(const ams_mel_ir_camera_command_v1& input,
                     irmel::CameraCommand& output) noexcept
{
    irmel::ImageType type{};
    switch (input.image_type) {
    case AMS_MEL_IR_IMAGE_STARING: type = irmel::ImageType::Staring; break;
    case AMS_MEL_IR_IMAGE_SCANNING: type = irmel::ImageType::Scanning; break;
    default: return false; /* Reserved13 and unknown values are not imaging modes. */
    }
    irmel::ScanDir direction{};
    switch (input.direction) {
    case AMS_MEL_IR_SCAN_DIR_NEG_TO_POS_AZ: direction = irmel::ScanDir::NegToPosAz; break;
    case AMS_MEL_IR_SCAN_DIR_POS_TO_NEG_AZ: direction = irmel::ScanDir::PosToNegAz; break;
    default: return false;
    }
    output.setCommandID(input.command_id);
    output.setSystemTime(std::chrono::nanoseconds{input.system_time_ns});
    output.setImageType(type);
    output.setWidth(input.width);
    output.setHeight(input.height);
    output.setFrameRate(input.frame_rate_hz);
    output.setIntegrationTime(std::chrono::nanoseconds{input.integration_time_ns});
    output.setRowOffset(input.row_offset);
    output.setColumnOffset(input.column_offset);
    output.setDirection(direction);
    output.setNumberOfStages(input.number_of_stages);
    return true;
}

ams_mel_status_t submit(const CameraFamily& family, const void *context,
                        const ams_mel_ir_camera_command_v1 *command,
                        ams_mel_ir_camera_command_request **output, char *out,
                        std::size_t capacity, std::size_t *required) noexcept
{
    irmel::CameraCommand upstream;
    if (!command || !output || *output || (!out && capacity) ||
        !convert_command(*command, upstream)) {
        diagnostic("invalid CameraCommand input", out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    std::shared_ptr<CameraCompletion> completion;
    std::shared_ptr<CameraWorkerInput> input;
    std::unique_ptr<ams_mel_ir_camera_command_request> owner;
    std::unique_ptr<std::thread> worker;
    try {
        completion = std::make_shared<CameraCompletion>();
        input = std::make_shared<CameraWorkerInput>();
        input->completion = completion;
        owner = std::make_unique<ams_mel_ir_camera_command_request>();
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
        diagnostic("CameraCommand submission lock failed", out, capacity, required);
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
            diagnostic("provider returned an invalid CameraCommand future", out, capacity,
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

} // namespace ams_mel_camera

/* ---- Image family adapter (the C2 adapter lives in ir_c2.cpp) ---------- */
namespace {
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
/* Same post-decrement observation stage as Navigation/common Image requests. */
void camera_image_post_decrement_barrier() noexcept
{
    const char *base = std::getenv("AMS_MEL_TEST_IMAGE_CLEANUP_BARRIER");
    if (!base) return;
    try {
        const std::string prefix = std::string{base} + ".post-decrement";
        if (!std::ifstream{prefix + ".arm"}.good()) return;
        { std::ofstream marker{prefix + ".reached"}; marker << "reached\n"; }
        const std::string release = prefix + ".release";
        for (unsigned attempt = 0; attempt < 15000U; ++attempt) {
            if (std::ifstream{release}.good()) return;
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
    } catch (...) {
    }
}
#endif

bool finish_image_camera(const std::shared_ptr<void>& erased) noexcept
{
    const auto stream = std::static_pointer_cast<ImageStreamState>(erased);
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    return finish_image_request(stream, camera_image_post_decrement_barrier);
#else
    return finish_image_request(stream);
#endif
}

const ams_mel_camera::CameraFamily image_family{
    [](const void *context) noexcept {
        return static_cast<const ams_mel_ir_stream *>(context)->state->session->admission;
    },
    [](const void *context, std::shared_ptr<void>& channel, CommonRequestClaim& claim) {
        const auto& stream = static_cast<const ams_mel_ir_stream *>(context)->state;
        std::shared_ptr<irmel::ImageChannel> image;
        /* Attached/Running check, channel copy and ++requests under the lock. */
        if (!claim_navigation_submission(*stream, image)) return false;
        claim = CommonRequestClaim{stream, finish_image_camera, "deferred Image cleanup failed"};
        channel = std::move(image);
        return true;
    },
    [](const std::shared_ptr<void>& channel, irmel::CameraCommand command) {
        /* ImageChannel::send(CameraCommand); never routed through C2. */
        return std::static_pointer_cast<irmel::ImageChannel>(channel)->send(std::move(command));
    },
    "Image stream is not Attached or Running"};
} // namespace

extern "C" ams_mel_status_t ams_mel_ir_stream_submit_camera_command(
    ams_mel_ir_stream *stream, const ams_mel_ir_camera_command_v1 *command,
    ams_mel_ir_camera_command_request **out_request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    ams_mel_camera::diagnostic("", out, capacity, required);
    if (!stream || !stream->state) {
        ams_mel_camera::diagnostic("invalid CameraCommand input", out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    return ams_mel_camera::submit(image_family, stream, command, out_request, out, capacity,
                                  required);
}

extern "C" ams_mel_status_t ams_mel_ir_camera_command_request_wait(
    const ams_mel_ir_camera_command_request *request, std::uint32_t timeout_ms,
    ams_mel_ir_camera_command_result_v1 *result, char *out, std::size_t capacity,
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
        write("CameraCommand request wait failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    return AMS_MEL_INTERNAL_ERROR;
}

extern "C" ams_mel_status_t ams_mel_ir_camera_command_request_close(
    ams_mel_ir_camera_command_request **request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    write("", out, capacity, required);
    if (!request || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try { delete std::exchange(*request, nullptr); return AMS_MEL_OK; }
    catch (...) { write("request close failed", out, capacity, required); return AMS_MEL_INTERNAL_ERROR; }
}

/* ---- Opt-in Image CameraCommandResp owner ------------------------------ */

void CameraResponseState::receive(const irmel::CameraCommandResp *input) noexcept
{
    try {
        std::lock_guard lock{mutex};
        increment(counters.events_received);
        if (lifecycle != CameraResponseLifecycle::Active) return;
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
        if (const char *value = std::getenv("AMS_MEL_TEST_CAMERA_CALLBACK_FAILURE");
            value && std::strcmp(value, "allocation") == 0) {
            (void)unsetenv("AMS_MEL_TEST_CAMERA_CALLBACK_FAILURE");
            throw std::bad_alloc{};
        }
#endif
        if (!input) { increment(counters.malformed_or_unsupported); return; }
        /* Complete fixed-size copy before the provider callback returns. */
        const auto copied = convert_response(*input);
        if (queue.size() >= capacity) { increment(counters.events_dropped_queue_full); return; }
        queue.push_back(copied);
        ready.notify_one();
    } catch (...) {
        /* No exception crosses the provider callback boundary. */
        try {
            std::lock_guard lock{mutex};
            increment(counters.malformed_or_unsupported);
        } catch (...) {
        }
    }
}

void CameraResponseState::deactivate() noexcept
{
    try {
        std::lock_guard lock{mutex};
        if (lifecycle == CameraResponseLifecycle::Active)
            lifecycle = CameraResponseLifecycle::Inactive;
        ready.notify_all();
    } catch (...) {
    }
}

void CameraResponseState::stop() noexcept
{
    try {
        std::lock_guard lock{mutex};
        if (lifecycle == CameraResponseLifecycle::Active)
            lifecycle = CameraResponseLifecycle::Stopped;
        ready.notify_all();
    } catch (...) {
    }
}

void image_camera_response_stream_stopped(const std::shared_ptr<CameraResponseState>& state) noexcept
{
    if (state) state->stop();
}

extern "C" ams_mel_status_t ams_mel_ir_image_camera_response_open(
    ams_mel_ir_stream *stream, std::size_t queue_capacity,
    ams_mel_ir_camera_response_metadata **output, ams_mel_ir_return_t *registration,
    char *out, std::size_t capacity, std::size_t *required) noexcept
{
    write("", out, capacity, required);
    if (!stream || !stream->state || !queue_capacity || !output || *output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    std::shared_ptr<CameraResponseState> state;
    std::function<void(irmel::Channel&, const irmel::CameraCommandResp *const)> callback;
    std::unique_ptr<ams_mel_ir_camera_response_metadata> owner;
    try {
        state = std::make_shared<CameraResponseState>();
        state->capacity = queue_capacity;
        callback = [state](irmel::Channel&, const irmel::CameraCommandResp *const value) noexcept {
            state->receive(value);
        };
        owner = std::make_unique<ams_mel_ir_camera_response_metadata>();
        owner->state = state;
    } catch (...) {
        write("CameraCommandResp metadata allocation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    std::shared_ptr<irmel::ImageChannel> channel;
    if (!claim_image_camera_response(*stream->state, state, channel)) {
        write("Image stream is not eligible or CameraCommandResp was already attempted",
              out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    irmel::Return result{};
    try {
        /* No bridge lock held: synchronous delivery during registration is safe. */
        result = channel->registerMetadataCallback(std::move(callback));
    } catch (...) {
        state->deactivate();
        write("provider exception during CameraCommandResp registration", out, capacity,
              required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
    ams_mel_ir_return_t mapped = AMS_MEL_IR_RETURN_FAIL;
    const bool known = ams_mel::internal::nuc_map_return(result, mapped);
    if (known && registration) *registration = mapped;
    if (!known || mapped != AMS_MEL_IR_RETURN_SUCCESS) {
        state->deactivate();
        write(known && mapped == AMS_MEL_IR_RETURN_NOT_SUPPORTED ?
                  "provider does not support programmable camera control CameraCommandResp" :
                  "CameraCommandResp callback registration failed",
              out, capacity, required);
        return AMS_MEL_PROVIDER_FAILED;
    }
    *output = owner.release();
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_camera_response_receive(
    ams_mel_ir_camera_response_metadata *metadata, std::uint32_t timeout_ms,
    ams_mel_ir_camera_response_event **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    write("", out, capacity, required);
    if (!metadata || !metadata->state || !output || *output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        auto& state = *metadata->state;
        auto owner = std::make_unique<ams_mel_ir_camera_response_event>();
        std::unique_lock lock{state.mutex};
        if (state.queue.empty() && state.lifecycle == CameraResponseLifecycle::Active && timeout_ms)
            state.ready.wait_for(lock, std::chrono::milliseconds{timeout_ms}, [&] {
                return !state.queue.empty() || state.lifecycle != CameraResponseLifecycle::Active;
            });
        if (!state.queue.empty()) {
            owner->view = state.queue.front();
            state.queue.pop_front();
            *output = owner.release();
            return AMS_MEL_OK;
        }
        return state.lifecycle == CameraResponseLifecycle::Active ? AMS_MEL_TIMEOUT
                                                                   : AMS_MEL_STREAM_STOPPED;
    } catch (...) {
        write("CameraCommandResp metadata receive failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_camera_response_get_counters(
    const ams_mel_ir_camera_response_metadata *metadata,
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

extern "C" ams_mel_status_t ams_mel_ir_camera_response_close(
    ams_mel_ir_camera_response_metadata **metadata, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    write("", out, capacity, required);
    if (!metadata || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    auto *owner = std::exchange(*metadata, nullptr);
    if (owner && owner->state) owner->state->deactivate();
    delete owner;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_camera_response_event_view(
    const ams_mel_ir_camera_response_event *event,
    const ams_mel_ir_camera_command_response_v1 **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    write("", out, capacity, required);
    if (!event || !output || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    *output = &event->view;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_camera_response_event_close(
    ams_mel_ir_camera_response_event **event, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    write("", out, capacity, required);
    if (!event || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    delete std::exchange(*event, nullptr);
    return AMS_MEL_OK;
}
