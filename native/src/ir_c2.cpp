#include <ams_mel/abi.h>
#include "internal.hpp"

#include <irmel/library/c2/C2Channel.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <future>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace ams::iface;

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

bool valid_view(const ams_mel_string_view_v1& value) noexcept
{
    return value.data ? valid_utf8({value.data, value.size}) : value.size == 0U;
}

std::string copy_view(const ams_mel_string_view_v1& value)
{ return value.size ? std::string{value.data, value.size} : std::string{}; }

mel::UCI_ID convert_id(const ams_mel_uci_id_v1& value)
{
    std::array<std::uint8_t, mel::UUID_SIZE> uuid{};
    std::copy(std::begin(value.uuid), std::end(value.uuid), uuid.begin());
    return {uuid, copy_view(value.descriptive_label)};
}

bool has_c2(const irmel::ChannelCapability& capability)
{
    const auto& types = capability.getChannelTypes();
    return std::find(types.begin(), types.end(), irmel::ChannelType::CommandAndControl) !=
           types.end();
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

bool map_mode(irmel::MFA_Mode mode, ams_mel_ir_mfa_mode_t& value) noexcept
{
    switch (mode) {
    case irmel::MFA_Mode::Unused: value = AMS_MEL_IR_MFA_MODE_UNUSED; return true;
    case irmel::MFA_Mode::TaskSched: value = AMS_MEL_IR_MFA_MODE_TASK_SCHED; return true;
    case irmel::MFA_Mode::ScanVolumeSched: value = AMS_MEL_IR_MFA_MODE_SCAN_VOLUME_SCHED; return true;
    case irmel::MFA_Mode::ScanBarSched: value = AMS_MEL_IR_MFA_MODE_SCAN_BAR_SCHED; return true;
    }
    return false;
}

bool convert_state(ams_mel_ir_mfa_state_t value, mel::MFA_State& state) noexcept
{
    if (value >= AMS_MEL_IR_MFA_STATE_MAX_EXCLUSIVE) return false;
    state = static_cast<mel::MFA_State>(value);
    return true;
}

bool convert_mode(ams_mel_ir_mfa_mode_t value, irmel::MFA_Mode& mode) noexcept
{
    if (value > AMS_MEL_IR_MFA_MODE_SCAN_BAR_SCHED) return false;
    mode = static_cast<irmel::MFA_Mode>(value);
    return true;
}

bool valid_scan(const ams_mel_ir_scan_param_v1& value) noexcept
{
    return value.elevation_defined_with_range_and_altitude <= 1U &&
           value.center_frame_ref_el <= AMS_MEL_IR_COORD_FRAME_AIRCRAFT &&
           value.center_frame_ref_az <= AMS_MEL_IR_COORD_FRAME_AIRCRAFT &&
           value.degradation_method <= AMS_MEL_IR_DEGRADATION_REVISIT;
}

irmel::ScanParam convert_scan(const ams_mel_ir_scan_param_v1& value)
{
    irmel::ScanType type;
    type.setContinuousScan(value.scan_type.continuous_scan);
    type.setReturning(value.scan_type.returning);
    type.setAgileScan(value.scan_type.agile_scan);
    return {value.elevation_defined_with_range_and_altitude != 0U,
            AzEl{value.center_az_rad, value.center_el_rad},
            static_cast<irmel::CoordFrameRef>(value.center_frame_ref_el),
            static_cast<irmel::CoordFrameRef>(value.center_frame_ref_az),
            value.scan_width_rad, value.scan_height_rad, std::move(type),
            value.scan_id, value.scan_rate_rad_per_second,
            value.preferred_revisit_interval_seconds,
            value.required_revisit_interval_seconds,
            value.max_range_of_interest_m, value.min_range_of_interest_m,
            value.elevation_scan_center_altitude_m,
            value.elevation_scan_center_range_m,
            static_cast<irmel::DegradationMethod>(value.degradation_method)};
}

bool valid_span(const ams_mel_u32_span_v1& span) noexcept
{ return span.data != nullptr || span.size == 0U; }

bool valid_span(const ams_mel_string_view_span_v1& span) noexcept
{
    if (!span.data && span.size != 0U) return false;
    for (std::size_t index = 0; index < span.size; ++index)
        if (!valid_view(span.data[index])) return false;
    return true;
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

enum class C2Lifecycle { Attached, Enabled, Failed, Closed };

struct ChannelState {
    std::mutex mutex;
    std::shared_ptr<SessionState> session;
    std::shared_ptr<irmel::Channel> channel;
    std::shared_ptr<irmel::C2Channel> c2;
    C2Lifecycle lifecycle{C2Lifecycle::Attached};
    bool enabled{};
    bool enable_attempted{};
    std::size_t requests{};
    bool cleanup_started{};
    std::shared_ptr<ChannelState> emergency_self;
    ChannelState *emergency_next{};
    std::atomic<bool> emergency_retained{};
};

/* A failed deferred detach cannot safely destroy its graph. Keep it for process
 * lifetime rather than unload provider code that may still own the channel.
 * The intrusive root and pre-existing shared_ptr cycle require no allocation. */
void retain_failed(const std::shared_ptr<ChannelState>& state) noexcept
{
    static std::atomic<ChannelState *> retained{};
    bool expected = false;
    if (!state->emergency_retained.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) return;
    state->emergency_self = state;
    ChannelState *head = retained.load(std::memory_order_relaxed);
    do { state->emergency_next = head; }
    while (!retained.compare_exchange_weak(
        head, state.get(), std::memory_order_release, std::memory_order_relaxed));
}

bool cleanup(const std::shared_ptr<ChannelState>& state,
             bool retain_if_orphaned)
{
    std::shared_ptr<irmel::Channel> channel;
    bool disable = false;
    {
        std::lock_guard lock{state->mutex};
        if (state->cleanup_started || state->requests != 0U) return true;
        state->cleanup_started = true;
        channel = state->channel;
        disable = state->enable_attempted;
    }
    bool ok = true;
    if (channel && disable) {
        try { if (channel->disable() != irmel::Return::Success) ok = false; }
        catch (...) { ok = false; }
    }
    bool detached = !channel;
    if (channel) {
        try {
            detached = state->session->control->detachChannel(channel) ==
                       irmel::Return::Success;
        } catch (...) { detached = false; }
    }
    if (!detached) {
        {
            std::lock_guard lock{state->mutex};
            state->cleanup_started = false;
        }
        if (retain_if_orphaned) retain_failed(state);
        return false;
    }
    {
        std::lock_guard lock{state->mutex};
        state->c2.reset();
        state->channel.reset();
        state->enabled = false;
        state->enable_attempted = false;
        state->lifecycle = C2Lifecycle::Closed;
    }
    return ok;
}

enum class CompletionKind { Pending, Success, Rejected, ProviderException, ProviderFailure, InternalError };

struct Completion {
    std::mutex mutex;
    std::condition_variable ready;
    CompletionKind kind{CompletionKind::Pending};
    ams_mel_ir_mode_result_v1 result{};
    std::string message;
    std::shared_ptr<ChannelState> channel;
};

struct WorkerInput {
    std::shared_ptr<Completion> completion;
    mel::RequestFor<irmel::MFA_Mode> future;
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

bool finish_channel(const std::shared_ptr<ChannelState>& channel)
{
    bool close = false;
    {
        std::lock_guard lock{channel->mutex};
        if (channel->requests) --channel->requests;
        close = channel->requests == 0U && channel->lifecycle == C2Lifecycle::Closed;
    }
    return !close || cleanup(channel, true);
}

void complete(const std::shared_ptr<Completion>& state,
              mel::RequestFor<irmel::MFA_Mode>& future)
{
    CompletionKind kind = CompletionKind::ProviderException;
    ams_mel_ir_mode_result_v1 result{};
    std::string message;
    try {
        auto outcome = future.get();
        if (outcome) {
            const auto& value = outcome.get();
            if (!value) {
                kind = CompletionKind::ProviderFailure;
                message = "provider returned null successful mode result";
            } else if (!map_mode(*value, result.mode)) {
                kind = CompletionKind::ProviderFailure;
                message = "provider returned unknown MFA mode";
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
    auto channel = state->channel;
    const bool cleanup_ok = finish_channel(channel);
    if (!cleanup_ok) {
        kind = CompletionKind::ProviderFailure;
        try { message = "deferred C2 cleanup failed"; }
        catch (...) { message.clear(); }
    }
    {
        std::lock_guard lock{state->mutex};
        state->kind = kind;
        state->result = result;
        state->message = std::move(message);
        state->channel.reset();
    }
    channel.reset();
    state->ready.notify_all();
}

void run_worker(const std::shared_ptr<WorkerInput>& input) noexcept
{
    while (input->launch_state.load(std::memory_order_acquire) == 0U)
        std::this_thread::yield();
    try {
        complete(input->completion, input->future);
    } catch (...) {
        /* A mutex/system failure must neither escape the detached thread nor
         * destroy an unaccounted future/provider graph. */
        arm_worker(input);
        retain_worker(input);
    }
}

struct ReturnCompletion {
    std::mutex mutex;
    std::condition_variable ready;
    CompletionKind kind{CompletionKind::Pending};
    ams_mel_ir_return_result_v1 result{};
    std::string message;
    std::shared_ptr<ChannelState> channel;
};

struct ReturnWorkerInput {
    std::shared_ptr<ReturnCompletion> completion;
    mel::RequestFor<irmel::Return> future;
    std::shared_ptr<ReturnWorkerInput> emergency_self;
    ReturnWorkerInput *emergency_next{};
    std::atomic<bool> emergency_retained{};
    std::atomic<unsigned> launch_state{};
};

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
        auto outcome = future.get();
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
    auto channel = state->channel;
    if (!finish_channel(channel)) {
        kind = CompletionKind::ProviderFailure;
        try { message = "deferred C2 cleanup failed"; }
        catch (...) { message.clear(); }
    }
    {
        std::lock_guard lock{state->mutex};
        state->kind = kind;
        state->result = result;
        state->message = std::move(message);
        state->channel.reset();
    }
    channel.reset();
    state->ready.notify_all();
}

void run_return_worker(const std::shared_ptr<ReturnWorkerInput>& input) noexcept
{
    while (input->launch_state.load(std::memory_order_acquire) == 0U)
        std::this_thread::yield();
    try {
        complete_return(input->completion, input->future);
    } catch (...) {
        arm_return_worker(input);
        retain_return_worker(input);
    }
}

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
enum class SubmitFailpoint { None, Allocation, WorkerLaunch };

SubmitFailpoint submit_failpoint() noexcept
{
    const char *value = std::getenv("AMS_MEL_TEST_C2_POST_SEND_FAILURE");
    if (value && std::strcmp(value, "allocation") == 0)
        return SubmitFailpoint::Allocation;
    if (value && std::strcmp(value, "worker-launch") == 0)
        return SubmitFailpoint::WorkerLaunch;
    return SubmitFailpoint::None;
}
#endif
} // namespace

struct ams_mel_ir_c2 { std::shared_ptr<ChannelState> state; };
struct ams_mel_ir_mode_request { std::shared_ptr<Completion> state; };
struct ams_mel_ir_return_request { std::shared_ptr<ReturnCompletion> state; };

namespace {
ams_mel_status_t submit_mode_command(
    ams_mel_ir_c2 *c2, irmel::ModeCmd command,
    ams_mel_ir_mode_request **out_request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    std::shared_ptr<Completion> completion;
    std::shared_ptr<WorkerInput> input;
    std::unique_ptr<ams_mel_ir_mode_request> owner;
    std::unique_ptr<std::thread> worker;
    try {
        completion = std::make_shared<Completion>();
        completion->channel = c2->state;
        input = std::make_shared<WorkerInput>();
        input->completion = completion;
        owner = std::make_unique<ams_mel_ir_mode_request>();
        owner->state = completion;
        worker = std::make_unique<std::thread>();
    } catch (...) {
        diagnostic("allocation failed before provider send", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    std::unique_lock<std::mutex> lock;
    try { lock = std::unique_lock<std::mutex>{c2->state->mutex}; }
    catch (...) { diagnostic("C2 submission lock failed", out, capacity, required); return AMS_MEL_INTERNAL_ERROR; }
    if (c2->state->lifecycle != C2Lifecycle::Enabled) {
        diagnostic("C2 channel is not enabled", out, capacity, required);
        return AMS_MEL_PROVIDER_FAILED;
    }
    try {
        try {
            input->future = c2->state->c2->send(std::move(command));
            arm_worker(input);
            ++c2->state->requests;
        } catch (const std::exception& error) {
            const char *what = error.what();
            const std::string_view text = what ? std::string_view{what} : std::string_view{};
            diagnostic(!text.empty() && valid_utf8(text) ? text : "provider send exception", out, capacity, required);
            return AMS_MEL_PROVIDER_EXCEPTION;
        } catch (...) {
            diagnostic("unknown provider send exception", out, capacity, required);
            return AMS_MEL_PROVIDER_EXCEPTION;
        }
        lock.unlock();
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

template<typename Command>
ams_mel_status_t submit_return_command(
    ams_mel_ir_c2 *c2, Command command,
    ams_mel_ir_return_request **out_request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    std::shared_ptr<ReturnCompletion> completion;
    std::shared_ptr<ReturnWorkerInput> input;
    std::unique_ptr<ams_mel_ir_return_request> owner;
    std::unique_ptr<std::thread> worker;
    try {
        completion = std::make_shared<ReturnCompletion>();
        completion->channel = c2->state;
        input = std::make_shared<ReturnWorkerInput>();
        input->completion = completion;
        owner = std::make_unique<ams_mel_ir_return_request>();
        owner->state = completion;
        worker = std::make_unique<std::thread>();
    } catch (...) {
        diagnostic("allocation failed before provider send", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    std::unique_lock<std::mutex> lock;
    try { lock = std::unique_lock<std::mutex>{c2->state->mutex}; }
    catch (...) { diagnostic("C2 submission lock failed", out, capacity, required); return AMS_MEL_INTERNAL_ERROR; }
    if (c2->state->lifecycle != C2Lifecycle::Enabled) {
        diagnostic("C2 channel is not enabled", out, capacity, required);
        return AMS_MEL_PROVIDER_FAILED;
    }
    try {
        try {
            input->future = c2->state->c2->send(std::move(command));
            arm_return_worker(input);
            ++c2->state->requests;
        } catch (const std::exception& error) {
            const char *what = error.what();
            const std::string_view text = what ? std::string_view{what} : std::string_view{};
            diagnostic(!text.empty() && valid_utf8(text) ? text : "provider send exception", out, capacity, required);
            return AMS_MEL_PROVIDER_EXCEPTION;
        } catch (...) {
            diagnostic("unknown provider send exception", out, capacity, required);
            return AMS_MEL_PROVIDER_EXCEPTION;
        }
        lock.unlock();
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
        const SubmitFailpoint failpoint = submit_failpoint();
        if (failpoint == SubmitFailpoint::Allocation) throw std::bad_alloc{};
        if (failpoint == SubmitFailpoint::WorkerLaunch)
            throw std::system_error{std::make_error_code(std::errc::resource_unavailable_try_again)};
#endif
        *worker = std::thread{[input] { run_return_worker(input); }};
        worker->detach();
        input->emergency_self.reset();
        input->launch_state.store(1U, std::memory_order_release);
        *out_request = owner.release();
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        retain_return_worker(input);
        diagnostic("facade allocation failed after provider send", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        retain_return_worker(input);
        if (worker && worker->joinable()) (void)worker.release();
        input->launch_state.store(2U, std::memory_order_release);
        diagnostic("facade worker launch failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}
} // namespace

extern "C" ams_mel_status_t ams_mel_ir_c2_open(
    const ams_mel_session *session, const ams_mel_ir_c2_config_v1 *config,
    ams_mel_ir_c2 **out_c2, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!session || !session->state || !config || !out_c2 || *out_c2 ||
        (!out && capacity) ||
        config->channel_type != AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL ||
        !valid_view(config->channel_id.descriptive_label) ||
        !valid_view(config->platform_id.descriptive_label) ||
        !valid_view(config->sensor_location.key) ||
        !valid_view(config->sensor_location.system_name)) {
        diagnostic("invalid C2 configuration", out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    std::shared_ptr<ChannelState> state;
    try {
        bool advertised = false;
        for (const auto& capability : session->state->control->getCapabilities())
            if (has_c2(capability)) { advertised = true; break; }
        if (!advertised) {
            diagnostic("provider does not advertise CommandAndControl", out, capacity, required);
            return AMS_MEL_INITIALIZATION_FAILED;
        }
        state = std::make_shared<ChannelState>();
        state->session = session->state;
        mel::ForeignKey key{copy_view(config->sensor_location.key),
                            copy_view(config->sensor_location.system_name)};
        mel::ComponentLocation location{config->sensor_location.offset_x_m,
            config->sensor_location.offset_y_m, config->sensor_location.offset_z_m,
            key};
        irmel::Config upstream{convert_id(config->channel_id),
            irmel::ChannelType::CommandAndControl, convert_id(config->platform_id),
            std::move(location), {}, false, false};
        state->channel = state->session->control->attachChannel(upstream);
        if (!state->channel) {
            diagnostic("attachChannel returned null", out, capacity, required);
            return AMS_MEL_FACTORY_FAILED;
        }
        state->c2 = std::dynamic_pointer_cast<irmel::C2Channel>(state->channel);
        bool compatible = state->c2 && has_c2(state->channel->getCapabilities());
        if (!compatible) {
            bool detached = false;
            try { detached = state->session->control->detachChannel(state->channel) == irmel::Return::Success; }
            catch (...) {}
            if (!detached) retain_failed(state);
            diagnostic(detached ? "attached channel is not compatible C2" :
                       "incompatible C2 channel and detach failed", out, capacity, required);
            return detached ? AMS_MEL_INITIALIZATION_FAILED : AMS_MEL_PROVIDER_FAILED;
        }
        auto owner = std::make_unique<ams_mel_ir_c2>();
        owner->state = std::move(state);
        *out_c2 = owner.release();
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        if (state && state->channel) {
            bool detached = false;
            try { detached = state->session->control->detachChannel(state->channel) == irmel::Return::Success; }
            catch (...) {}
            if (!detached) retain_failed(state);
        }
        diagnostic("allocation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        if (state && state->channel) {
            bool detached = false;
            try { detached = state->session->control->detachChannel(state->channel) == irmel::Return::Success; }
            catch (...) {}
            if (!detached) {
                retain_failed(state);
                diagnostic("C2 open exception and detach failed", out, capacity, required);
                return AMS_MEL_PROVIDER_FAILED;
            }
        }
        diagnostic("provider exception during C2 open", out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_c2_enable(
    ams_mel_ir_c2 *c2, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!c2 || !c2->state || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::lock_guard lock{c2->state->mutex};
        if (c2->state->lifecycle == C2Lifecycle::Enabled) return AMS_MEL_OK;
        if (c2->state->lifecycle != C2Lifecycle::Attached) {
            diagnostic("C2 channel is not attachable", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        c2->state->enable_attempted = true;
        if (c2->state->channel->enable() != irmel::Return::Success) {
            c2->state->lifecycle = C2Lifecycle::Failed;
            diagnostic("C2 enable failed", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        c2->state->lifecycle = C2Lifecycle::Enabled;
        c2->state->enabled = true;
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        diagnostic("allocation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        c2->state->lifecycle = C2Lifecycle::Failed;
        diagnostic("provider exception during C2 enable", out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_c2_submit_operate(
    ams_mel_ir_c2 *c2, std::uint32_t command_id,
    ams_mel_ir_mode_request **out_request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!c2 || !c2->state || !out_request || *out_request || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    irmel::ModeCmd command;
    command.setCommandID(command_id);
    command.setState(mel::MFA_State::Operate);
    command.setMode(irmel::MFA_Mode::TaskSched);
    return submit_mode_command(c2, std::move(command), out_request, out, capacity, required);
}

extern "C" ams_mel_status_t ams_mel_ir_c2_submit_mode(
    ams_mel_ir_c2 *c2, const ams_mel_ir_mode_command_v1 *input,
    ams_mel_ir_mode_request **out_request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    mel::MFA_State state{};
    irmel::MFA_Mode mode{};
    if (!c2 || !c2->state || !input || !out_request || *out_request ||
        (!out && capacity) || !convert_state(input->state, state) ||
        !convert_mode(input->mode, mode) || !valid_scan(input->scan_parameters)) {
        diagnostic("invalid ModeCmd input", out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    try {
        irmel::ModeCmd command;
        command.setCommandID(input->command_id);
        command.setState(state);
        command.setMode(mode);
        command.setScanParameters(convert_scan(input->scan_parameters));
        return submit_mode_command(c2, std::move(command), out_request, out, capacity, required);
    } catch (...) {
        diagnostic("ModeCmd preparation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_mode_request_wait(
    const ams_mel_ir_mode_request *request, std::uint32_t timeout_ms,
    ams_mel_ir_mode_result_v1 *result, char *out, std::size_t capacity,
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
        diagnostic("mode request wait failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    return AMS_MEL_INTERNAL_ERROR;
}

extern "C" ams_mel_status_t ams_mel_ir_mode_request_close(
    ams_mel_ir_mode_request **request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!request || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try { delete std::exchange(*request, nullptr); return AMS_MEL_OK; }
    catch (...) { diagnostic("request close failed", out, capacity, required); return AMS_MEL_INTERNAL_ERROR; }
}

extern "C" ams_mel_status_t ams_mel_ir_c2_submit_bit_noop(
    ams_mel_ir_c2 *c2, std::uint32_t command_id,
    ams_mel_ir_return_request **out_request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!c2 || !c2->state || !out_request || *out_request || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    irmel::BIT_Command command;
    command.setCommandID(command_id);
    return submit_return_command(c2, std::move(command), out_request, out, capacity, required);
}

extern "C" ams_mel_status_t ams_mel_ir_c2_submit_bit(
    ams_mel_ir_c2 *c2, const ams_mel_ir_bit_command_v1 *input,
    ams_mel_ir_return_request **out_request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!c2 || !c2->state || !input || !out_request || *out_request || (!out && capacity) ||
        !valid_span(input->initiate_bit_ids) || !valid_span(input->cancel_bit_ids) ||
        !valid_span(input->clear_fault_codes)) {
        diagnostic("invalid BIT command input", out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    try {
        std::vector<std::uint32_t> initiate;
        std::vector<std::uint32_t> cancel;
        if (input->initiate_bit_ids.size != 0U)
            initiate.assign(input->initiate_bit_ids.data,
                input->initiate_bit_ids.data + input->initiate_bit_ids.size);
        if (input->cancel_bit_ids.size != 0U)
            cancel.assign(input->cancel_bit_ids.data,
                input->cancel_bit_ids.data + input->cancel_bit_ids.size);
        std::vector<std::string> faults;
        faults.reserve(input->clear_fault_codes.size);
        for (std::size_t index = 0; index < input->clear_fault_codes.size; ++index)
            faults.push_back(copy_view(input->clear_fault_codes.data[index]));
        irmel::BIT_Command command{input->command_id, std::move(initiate),
                                   std::move(cancel), std::move(faults)};
        return submit_return_command(c2, std::move(command), out_request, out, capacity, required);
    } catch (...) {
        diagnostic("BIT command preparation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_c2_submit_config_set(
    ams_mel_ir_c2 *c2, const ams_mel_ir_config_set_command_v1 *input,
    ams_mel_ir_return_request **out_request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!c2 || !c2->state || !input || !out_request || *out_request || (!out && capacity) ||
        !valid_view(input->config)) {
        diagnostic("invalid ConfigSet command input", out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    try {
        irmel::ConfigSetCommand command;
        command.setCommandID(input->command_id);
        command.setSystemTime(std::chrono::nanoseconds{input->system_time_ns});
        command.setConfig(copy_view(input->config));
        return submit_return_command(c2, std::move(command), out_request, out, capacity, required);
    } catch (...) {
        diagnostic("ConfigSet command preparation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
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

extern "C" ams_mel_status_t ams_mel_ir_c2_close(
    ams_mel_ir_c2 **c2, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!c2 || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    ams_mel_ir_c2 *owner = *c2;
    if (!owner) return AMS_MEL_OK;
    try {
        auto state = owner->state;
        bool now = false;
        {
            std::lock_guard lock{state->mutex};
            state->lifecycle = C2Lifecycle::Closed;
            now = state->requests == 0U;
        }
        bool ok = !now || cleanup(state, false);
        if (now && state->channel) {
            diagnostic("C2 detach failed; provider state retained", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        *c2 = nullptr;
        delete owner;
        if (!ok) {
            diagnostic("C2 disable failed", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        return AMS_MEL_OK;
    } catch (...) {
        diagnostic("C2 close failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}
