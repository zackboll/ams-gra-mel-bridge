#include <ams_mel/abi.h>
#include "internal/ir_stream.hpp"
#include "internal/completion_probe.hpp"

#include <irmel/library/image/ImageChannel.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>

namespace {
using namespace ams::iface;

bool valid_utf8(std::string_view value) noexcept;

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

bool convert_state(ams_mel_position_solution_state_t value, mel::PositionSolutionState& state) noexcept
{
    if (value >= AMS_MEL_POSITION_SOLUTION_MAX_EXCLUSIVE) return false;
    state = static_cast<mel::PositionSolutionState>(value);
    return true;
}

mel::Euler convert_euler(const ams_mel_euler_v1& value) noexcept
{ return {value.roll, value.pitch, value.yaw}; }

mel::NorthEastDown convert_ned(const ams_mel_north_east_down_v1& value) noexcept
{ return {value.north, value.east, value.down}; }

mel::AttitudeRate convert_attitude_rate(const ams_mel_attitude_rate_v1& value) noexcept
{
    return {convert_euler(value.attitude_rate),
            std::chrono::nanoseconds{value.attitude_rate_time_ns}};
}

mel::PositionVelocityCovariance convert_covariance(
    const ams_mel_position_velocity_covariance_v1& value) noexcept
{
    return {value.position_position_pn_pn, value.position_position_pn_pe,
            value.position_position_pn_pd, value.position_position_pe_pe,
            value.position_position_pe_pd, value.position_position_pd_pd,
            value.position_velocity_pn_vn, value.position_velocity_pn_ve,
            value.position_velocity_pn_vd, value.position_velocity_pe_ve,
            value.position_velocity_pe_vd, value.position_velocity_pd_vd,
            value.velocity_velocity_vn_vn, value.velocity_velocity_vn_ve,
            value.velocity_velocity_vn_vd, value.velocity_velocity_ve_ve,
            value.velocity_velocity_ve_vd, value.velocity_velocity_vd_vd};
}

mel::NavigationReport convert_report(const ams_mel_navigation_report_v1& value,
                                     mel::PositionSolutionState state)
{
    return {std::chrono::nanoseconds{value.system_time_ns}, state,
            value.latitude_rad, value.longitude_rad, value.altitude_m,
            convert_euler(value.attitude), convert_attitude_rate(value.attitude_rate),
            convert_ned(value.speed), convert_ned(value.acceleration),
            value.wander_angle_rad, value.magnetic_heading, value.altitude_msl,
            convert_covariance(value.position_velocity_covariance_uncertainty)};
}

ams_mel_ir_navigation_response_v1 convert_response(const irmel::NavigationReportResp& value) noexcept
{
    return {value.getSystemTime().count(), value.getCommandID(), value.getReqId()};
}

enum class CompletionKind { Pending, Success, Rejected, ProviderException, ProviderFailure, InternalError };

/* Never retains a raw ams_mel_ir_stream *; the stream state graph keeps this
 * request alive independently of the public Image_Stream and Session owners. */
struct Completion {
    std::mutex mutex;
    std::condition_variable ready;
    CompletionKind kind{CompletionKind::Pending};
    ams_mel_ir_navigation_result_v1 result{};
    std::string message;
    std::shared_ptr<ImageStreamState> stream;
};

struct WorkerInput {
    AMS_MEL_PROBE_OWNER(Navigation)
    CompletionPermit admission;
    std::shared_ptr<Completion> completion;
    mel::RequestFor<irmel::NavigationReportResp> future;
    std::shared_ptr<WorkerInput> emergency_self;
    WorkerInput *emergency_next{};
    std::atomic<bool> emergency_retained{};
    std::atomic<unsigned> launch_state{};
};

void arm_worker(const std::shared_ptr<WorkerInput>& input) noexcept
{
    input->emergency_self = input;
}

void retain_worker(const std::shared_ptr<WorkerInput>& input) noexcept
{
    static std::atomic<WorkerInput *> retained{};
    bool expected = false;
    if (!input->emergency_retained.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) return;
    WorkerInput *head = retained.load(std::memory_order_relaxed);
    do { input->emergency_next = head; }
    while (!retained.compare_exchange_weak(
        head, input.get(), std::memory_order_release, std::memory_order_relaxed));
}
/* Decrements the request count; if this was the final request, attempts
 * deferred physical cleanup (NotRequired unless the stream has already begun
 * logical Stop/Close). Never called with the frame callback mutex held.
 *
 * The cleanup call itself is what synchronizes this completion thread against
 * a concurrent public Stop/Close: ownership of physical teardown is claimed
 * under ImageStreamState's teardown lock, and this thread never touches
 * channel/image_channel outside it. Only an attempted-and-failed teardown is
 * reported as a request failure; NotRequired is not a failure. */
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
/* Test-only park between the final request-count decrement and the cleanup
 * ownership claim. This is exactly the window in which requests == 0,
 * cleanup_in_progress == false, and channel is still attached, so a regression
 * can run a public Close against that transient state deterministically.
 * Blocks only when the test arms the stage; production behavior never depends
 * on it and the whole body is compiled out of a non-test build. */
void finish_stream_test_barrier() noexcept
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
        /* A barrier failure must never change teardown behavior. */
    }
}
#endif

bool finish_stream(const std::shared_ptr<ImageStreamState>& stream)
{
    release_navigation_submission(*stream);
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    finish_stream_test_barrier();
#endif
    return finish_deferred_cleanup_from_external_owner(stream) != ImageCleanupOutcome::Failed;
}

void complete(const std::shared_ptr<Completion>& state,
             mel::RequestFor<irmel::NavigationReportResp>& future)
{
    CompletionKind kind = CompletionKind::ProviderException;
    ams_mel_ir_navigation_result_v1 result{};
    std::string message;
    try {
        auto outcome = AMS_MEL_PROBE_GET(Navigation, future.get());
        if (outcome) {
            const auto& value = outcome.get();
            if (!value) {
                kind = CompletionKind::ProviderFailure;
                message = "provider returned null successful NavigationReportResp";
            } else {
                result.response = convert_response(*value);
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
    AMS_MEL_PROBE_BOUNDARY(Navigation);
    auto stream = state->stream;
    const bool cleanup_ok = finish_stream(stream);
    if (!cleanup_ok) {
        kind = CompletionKind::ProviderFailure;
        try { message = "deferred Image cleanup failed"; }
        catch (...) { message.clear(); }
    }
    {
        std::lock_guard lock{state->mutex};
        state->kind = kind;
        state->result = result;
        state->message = std::move(message);
        state->stream.reset();
        AMS_MEL_PROBE_GRAPH(Navigation, stream);
    }
    stream.reset();
    state->ready.notify_all();
}

void run_worker(const std::shared_ptr<WorkerInput>& input) noexcept
{
    while (input->launch_state.load(std::memory_order_acquire) == 0U)
        std::this_thread::yield();
    AMS_MEL_PROBE_WORKER(Navigation);
    try {
        complete(input->completion, input->future); AMS_MEL_PROBE_RETURN(Navigation);
    } catch (...) {
        /* A mutex/system failure must neither escape the detached thread nor
         * destroy an unaccounted future/provider graph. */
        arm_worker(input);
        retain_worker(input);
    }
}

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
enum class SubmitFailpoint { None, Allocation, WorkerLaunch };

SubmitFailpoint submit_failpoint() noexcept
{
    const char *value = std::getenv("AMS_MEL_TEST_IMAGE_NAVIGATION_POST_SEND_FAILURE");
    if (value && std::strcmp(value, "allocation") == 0) return SubmitFailpoint::Allocation;
    if (value && std::strcmp(value, "worker-launch") == 0) return SubmitFailpoint::WorkerLaunch;
    return SubmitFailpoint::None;
}
#endif
} // namespace

struct ams_mel_ir_navigation_request { std::shared_ptr<Completion> state; };

extern "C" ams_mel_status_t ams_mel_ir_stream_submit_navigation_report(
    ams_mel_ir_stream *stream, const ams_mel_navigation_report_v1 *report,
    ams_mel_ir_navigation_request **out_request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    mel::PositionSolutionState state{};
    if (!stream || !stream->state || !report || !out_request || *out_request ||
        (!out && capacity != 0U) || !convert_state(report->state, state)) {
        diagnostic("invalid NavigationReport input", out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }

    std::shared_ptr<Completion> completion;
    std::shared_ptr<WorkerInput> input;
    std::unique_ptr<ams_mel_ir_navigation_request> owner;
    std::unique_ptr<std::thread> worker;
    try {
        completion = std::make_shared<Completion>();
        completion->stream = stream->state;
        input = std::make_shared<WorkerInput>();
        input->completion = completion;
        owner = std::make_unique<ams_mel_ir_navigation_request>();
        owner->state = completion;
        worker = std::make_unique<std::thread>();
    } catch (...) {
        diagnostic("allocation failed before provider send", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }

    mel::NavigationReport upstream;
    try {
        upstream = convert_report(*report, state);
    } catch (...) {
        diagnostic("NavigationReport preparation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }

    std::shared_ptr<irmel::ImageChannel> image_channel;
    if (!acquire_completion_permit(stream->state->session->admission, input->admission)) {
        diagnostic("async request limit reached", out, capacity, required);
        return AMS_MEL_RESOURCE_EXHAUSTED;
    }
    if (!claim_navigation_submission(*stream->state, image_channel)) {
        diagnostic("Image stream is not Attached or Running", out, capacity, required);
        return AMS_MEL_PROVIDER_FAILED;
    }

    /* Provider send() MUST NOT execute while the frame callback mutex is
     * held: it may synchronously invoke the NavigationReportResp metadata
     * callback, which independently locks the metadata mutex. */
    try {
        try {
            input->future = image_channel->send(std::move(upstream));
            arm_worker(input);
        } catch (const std::exception& error) {
            release_navigation_submission(*stream->state);
            const char *what = error.what();
            const std::string_view text = what ? std::string_view{what} : std::string_view{};
            diagnostic(!text.empty() && valid_utf8(text) ? text : "provider send exception",
                       out, capacity, required);
            return AMS_MEL_PROVIDER_EXCEPTION;
        } catch (...) {
            release_navigation_submission(*stream->state);
            diagnostic("unknown provider send exception", out, capacity, required);
            return AMS_MEL_PROVIDER_EXCEPTION;
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
        *out_request = owner.release();
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

extern "C" ams_mel_status_t ams_mel_ir_navigation_request_wait(
    const ams_mel_ir_navigation_request *request, std::uint32_t timeout_ms,
    ams_mel_ir_navigation_result_v1 *result, char *out, std::size_t capacity,
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
        diagnostic("navigation request wait failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    return AMS_MEL_INTERNAL_ERROR;
}

extern "C" ams_mel_status_t ams_mel_ir_navigation_request_close(
    ams_mel_ir_navigation_request **request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!request || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try { delete std::exchange(*request, nullptr); return AMS_MEL_OK; }
    catch (...) { diagnostic("request close failed", out, capacity, required); return AMS_MEL_INTERNAL_ERROR; }
}
