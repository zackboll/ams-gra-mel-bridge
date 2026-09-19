#include <ams_mel/abi.h>
#include "internal.hpp"
#include "internal/ir_channel.hpp"

#include <irmel/library/instrumentation/InstrumentationChannel.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
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
{ return value.data ? valid_utf8({value.data, value.size}) : value.size == 0U; }

std::string copy_view(const ams_mel_string_view_v1& value)
{ return value.size ? std::string{value.data, value.size} : std::string{}; }

mel::UCI_ID convert_id(const ams_mel_uci_id_v1& value)
{
    std::array<std::uint8_t, mel::UUID_SIZE> uuid{};
    std::copy(std::begin(value.uuid), std::end(value.uuid), uuid.begin());
    return {uuid, copy_view(value.descriptive_label)};
}

void increment(std::uint64_t& value) noexcept { if (value != UINT64_MAX) ++value; }

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

/* Upstream Priority defines exactly Normal=0 and Debug=1 and no MaxExclusive
 * value, so the façade rejects anything above Debug explicitly. */
bool convert_priority(ams_mel_ir_priority_t value, irmel::Priority& priority) noexcept
{
    if (value > AMS_MEL_IR_PRIORITY_DEBUG) return false;
    priority = static_cast<irmel::Priority>(value);
    return true;
}

bool copy_report(const irmel::InstrumentationReport& value,
                 ams_mel_ir_instrumentation_report_v1& report) noexcept
{
    const auto priority = static_cast<std::uint32_t>(value.getInstrumentationPriority());
    if (priority > AMS_MEL_IR_PRIORITY_DEBUG) return false;
    report.command_id = value.getCommandID();
    report.size = value.getSize();
    report.timestamp_ns = value.getTimestamp().count();
    report.priority = priority;
    return true;
}

struct EventData { ams_mel_ir_instrumentation_metadata_event_v1 view{}; };

enum class MetadataLifecycle { Active, Inactive, Stopped, Failed };

struct MetadataState {
    std::mutex mutex;
    std::condition_variable ready, callbacks_done;
    std::deque<std::unique_ptr<EventData>> queue;
    std::size_t capacity{};
    ams_mel_ir_metadata_counters_v1 counters{};
    MetadataLifecycle lifecycle{MetadataLifecycle::Active};
    std::atomic<std::uint64_t> callbacks{};

    void fail() noexcept
    {
        try {
            std::lock_guard lock{mutex};
            lifecycle = MetadataLifecycle::Failed;
            ready.notify_all();
        } catch (...) {
            /* No exception may cross the provider callback boundary. */
        }
    }
};

struct CallbackGuard {
    std::shared_ptr<MetadataState> state;
    explicit CallbackGuard(std::shared_ptr<MetadataState> value) noexcept
        : state(std::move(value))
    { state->callbacks.fetch_add(1, std::memory_order_acq_rel); }
    ~CallbackGuard() noexcept
    {
        if (state->callbacks.fetch_sub(1, std::memory_order_acq_rel) == 1)
            state->callbacks_done.notify_all();
    }
};

/* Provider callback boundary: validates and copies the complete report into a
 * bounded owned queue. It never calls into Ada and never lets an exception
 * escape into provider code. A null payload is malformed. */
void metadata_callback(const std::shared_ptr<MetadataState>& state,
                       const irmel::InstrumentationReport *value) noexcept
{
    try {
        CallbackGuard guard{state};
        {
            std::lock_guard lock{state->mutex};
            increment(state->counters.events_received);
            if (state->lifecycle != MetadataLifecycle::Active) return;
        }
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
        if (const char *failure =
                std::getenv("AMS_MEL_TEST_INSTRUMENTATION_CALLBACK_FAILURE");
            failure && std::strcmp(failure, "allocation") == 0)
            throw std::bad_alloc{};
#endif
        std::unique_ptr<EventData> event;
        if (value) {
            auto owned = std::make_unique<EventData>();
            owned->view.kind = AMS_MEL_IR_INSTRUMENTATION_METADATA_REPORT;
            if (copy_report(*value, owned->view.report)) event = std::move(owned);
        }
        std::lock_guard lock{state->mutex};
        if (state->lifecycle != MetadataLifecycle::Active) return;
        if (!event) { increment(state->counters.malformed_or_unsupported); return; }
        if (state->queue.size() >= state->capacity) {
            increment(state->counters.events_dropped_queue_full);
            return;
        }
        state->queue.push_back(std::move(event));
        state->ready.notify_one();
    } catch (...) { state->fail(); }
}

enum class Lifecycle { Attached, Enabled, Failed, Closed };

struct ChannelState {
    std::mutex mutex;
    std::shared_ptr<SessionState> session;
    std::shared_ptr<irmel::Channel> channel;
    std::shared_ptr<irmel::InstrumentationChannel> instrumentation;
    std::shared_ptr<MetadataState> metadata;
    Lifecycle lifecycle{Lifecycle::Attached};
    bool enable_attempted{};
    bool metadata_attempted{};
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

bool has_instrumentation(const irmel::ChannelCapability& capability)
{
    const auto& types = capability.getChannelTypes();
    return std::find(types.begin(), types.end(),
                     irmel::ChannelType::Instrumentation) != types.end();
}

bool cleanup(const std::shared_ptr<ChannelState>& state, bool retain_if_orphaned)
{
    std::shared_ptr<irmel::Channel> channel;
    std::shared_ptr<MetadataState> metadata;
    bool disable = false;
    {
        std::lock_guard lock{state->mutex};
        if (state->cleanup_started || state->requests != 0U) return true;
        state->cleanup_started = true;
        channel = state->channel;
        metadata = state->metadata;
        disable = state->enable_attempted;
    }
    if (metadata) {
        std::lock_guard lock{metadata->mutex};
        if (metadata->lifecycle == MetadataLifecycle::Active)
            metadata->lifecycle = MetadataLifecycle::Inactive;
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
        if (metadata) {
            std::lock_guard lock{metadata->mutex};
            metadata->lifecycle = MetadataLifecycle::Failed;
            metadata->ready.notify_all();
        }
        {
            std::lock_guard lock{state->mutex};
            state->cleanup_started = false;
        }
        if (retain_if_orphaned) retain_failed(state);
        return false;
    }
    {
        std::lock_guard lock{state->mutex};
        state->instrumentation.reset();
        state->channel.reset();
        state->enable_attempted = false;
        state->lifecycle = Lifecycle::Closed;
    }
    /* Destroying the provider channel is the callback-quiescence boundary. */
    channel.reset();
    if (metadata) {
        std::unique_lock lock{metadata->mutex};
        metadata->callbacks_done.wait(lock, [&] {
            return metadata->callbacks.load(std::memory_order_acquire) == 0U; });
        if (metadata->lifecycle != MetadataLifecycle::Failed)
            metadata->lifecycle = MetadataLifecycle::Stopped;
        metadata->ready.notify_all();
    }
    return ok;
}

enum class CompletionKind {
    Pending, Success, Rejected, ProviderException, ProviderFailure, InternalError
};

/* Never retains a raw ams_mel_ir_instrumentation *; the channel state graph
 * keeps this request alive independently of the public owners. */
struct Completion {
    std::mutex mutex;
    std::condition_variable ready;
    CompletionKind kind{CompletionKind::Pending};
    ams_mel_ir_instrumentation_result_v1 result{};
    std::string message;
    std::shared_ptr<ChannelState> channel;
};

struct WorkerInput {
    std::shared_ptr<Completion> completion;
    mel::RequestFor<irmel::InstrumentationReport> future;
    std::shared_ptr<WorkerInput> emergency_self;
    WorkerInput *emergency_next{};
    std::atomic<bool> emergency_retained{};
    std::atomic<unsigned> launch_state{};
};

void arm_worker(const std::shared_ptr<WorkerInput>& input) noexcept
{ input->emergency_self = input; }

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

/* Decrements the request count; if this was the final request of an already
 * logically closed channel, performs deferred physical cleanup. Never called
 * with the channel lifecycle mutex held. */
bool finish_channel(const std::shared_ptr<ChannelState>& channel)
{
    bool close = false;
    {
        std::lock_guard lock{channel->mutex};
        if (channel->requests) --channel->requests;
        close = channel->requests == 0U && channel->lifecycle == Lifecycle::Closed;
    }
    return !close || cleanup(channel, true);
}

void complete(const std::shared_ptr<Completion>& state,
              mel::RequestFor<irmel::InstrumentationReport>& future)
{
    CompletionKind kind = CompletionKind::ProviderException;
    ams_mel_ir_instrumentation_result_v1 result{};
    std::string message;
    try {
        auto outcome = future.get();
        if (outcome) {
            const auto& value = outcome.get();
            if (!value) {
                kind = CompletionKind::ProviderFailure;
                message = "provider returned null successful InstrumentationReport";
            } else if (!copy_report(*value, result.report)) {
                kind = CompletionKind::ProviderFailure;
                message = "provider returned unknown Instrumentation Priority";
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
        try { message = "deferred Instrumentation cleanup failed"; }
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

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
enum class SubmitFailpoint { None, Allocation, WorkerLaunch };

SubmitFailpoint submit_failpoint() noexcept
{
    const char *value = std::getenv("AMS_MEL_TEST_INSTRUMENTATION_POST_SEND_FAILURE");
    if (value && std::strcmp(value, "allocation") == 0) return SubmitFailpoint::Allocation;
    if (value && std::strcmp(value, "worker-launch") == 0) return SubmitFailpoint::WorkerLaunch;
    return SubmitFailpoint::None;
}
#endif
} // namespace

struct ams_mel_ir_instrumentation { std::shared_ptr<ChannelState> state; };
struct ams_mel_ir_instrumentation_request { std::shared_ptr<Completion> state; };
struct ams_mel_ir_instrumentation_metadata {
    std::shared_ptr<MetadataState> state;
    std::weak_ptr<ChannelState> channel;
};
struct ams_mel_ir_instrumentation_metadata_event { std::unique_ptr<EventData> data; };

extern "C" ams_mel_status_t ams_mel_ir_instrumentation_open(
    const ams_mel_session *session,
    const ams_mel_ir_instrumentation_config_v1 *config,
    ams_mel_ir_instrumentation **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!session || !session->state || !config || !output || *output ||
        (!out && capacity) ||
        config->channel_type != AMS_MEL_IR_CHANNEL_INSTRUMENTATION ||
        !valid_view(config->channel_id.descriptive_label) ||
        !valid_view(config->platform_id.descriptive_label) ||
        !valid_view(config->sensor_location.key) ||
        !valid_view(config->sensor_location.system_name)) {
        diagnostic("invalid Instrumentation configuration", out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    std::shared_ptr<ChannelState> state;
    try {
        state = std::make_shared<ChannelState>();
        state->session = session->state;
        mel::ForeignKey key{copy_view(config->sensor_location.key),
                            copy_view(config->sensor_location.system_name)};
        mel::ComponentLocation location{config->sensor_location.offset_x_m,
            config->sensor_location.offset_y_m, config->sensor_location.offset_z_m,
            key};
        irmel::Config upstream{convert_id(config->channel_id),
            irmel::ChannelType::Instrumentation, convert_id(config->platform_id),
            std::move(location), {}, false, false};
        state->channel = state->session->control->attachChannel(upstream);
        if (!state->channel) {
            diagnostic("attachChannel returned null", out, capacity, required);
            return AMS_MEL_FACTORY_FAILED;
        }
        state->instrumentation =
            std::dynamic_pointer_cast<irmel::InstrumentationChannel>(state->channel);
        if (!state->instrumentation ||
            !has_instrumentation(state->channel->getCapabilities())) {
            bool detached = false;
            try {
                detached = state->session->control->detachChannel(state->channel) ==
                           irmel::Return::Success;
            } catch (...) {}
            if (!detached) retain_failed(state);
            diagnostic(detached ? "attached channel is not compatible Instrumentation" :
                       "incompatible Instrumentation channel and detach failed",
                       out, capacity, required);
            return detached ? AMS_MEL_INITIALIZATION_FAILED : AMS_MEL_PROVIDER_FAILED;
        }
        auto owner = std::make_unique<ams_mel_ir_instrumentation>();
        owner->state = std::move(state);
        *output = owner.release();
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        if (state && state->channel) retain_failed(state);
        diagnostic("allocation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        if (state && state->channel) retain_failed(state);
        diagnostic("provider exception during Instrumentation open",
                   out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_instrumentation_enable(
    ams_mel_ir_instrumentation *instrumentation, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!instrumentation || !instrumentation->state || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    auto state = instrumentation->state;
    try {
        std::lock_guard lock{state->mutex};
        if (state->lifecycle == Lifecycle::Enabled) return AMS_MEL_OK;
        if (state->lifecycle != Lifecycle::Attached) {
            diagnostic("Instrumentation channel is not attachable",
                       out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        state->enable_attempted = true;
        if (state->channel->enable() != irmel::Return::Success) {
            state->lifecycle = Lifecycle::Failed;
            diagnostic("Instrumentation enable failed", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        state->lifecycle = Lifecycle::Enabled;
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        diagnostic("allocation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        try { std::lock_guard lock{state->mutex}; state->lifecycle = Lifecycle::Failed; }
        catch (...) {}
        diagnostic("provider exception during Instrumentation enable",
                   out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_instrumentation_get_capabilities(
    ams_mel_ir_instrumentation *instrumentation,
    ams_mel_ir_channel_capability **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!instrumentation || !instrumentation->state || !output || *output ||
        (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::lock_guard lock{instrumentation->state->mutex};
        if (instrumentation->state->lifecycle != Lifecycle::Attached &&
            instrumentation->state->lifecycle != Lifecycle::Enabled)
            return AMS_MEL_PROVIDER_FAILED;
        /* Reuses the one shared native ChannelCapability snapshot. */
        return ams_mel::internal::snapshot_capability(
            *instrumentation->state->instrumentation, output, out, capacity, required);
    } catch (...) {
        diagnostic("Instrumentation capability query failed", out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_instrumentation_submit_level(
    ams_mel_ir_instrumentation *instrumentation,
    const ams_mel_ir_instrumentation_level_command_v1 *command,
    ams_mel_ir_instrumentation_request **out_request, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    irmel::Priority priority{};
    if (!instrumentation || !instrumentation->state || !command || !out_request ||
        *out_request || (!out && capacity) ||
        !convert_priority(command->priority, priority)) {
        diagnostic("invalid InstrumentationLevelCmd input", out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }

    /* Everything the request needs is allocated before provider send(). */
    std::shared_ptr<Completion> completion;
    std::shared_ptr<WorkerInput> input;
    std::unique_ptr<ams_mel_ir_instrumentation_request> owner;
    std::unique_ptr<std::thread> worker;
    try {
        completion = std::make_shared<Completion>();
        completion->channel = instrumentation->state;
        input = std::make_shared<WorkerInput>();
        input->completion = completion;
        owner = std::make_unique<ams_mel_ir_instrumentation_request>();
        owner->state = completion;
        worker = std::make_unique<std::thread>();
    } catch (...) {
        diagnostic("allocation failed before provider send", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }

    irmel::InstrumentationLevelCmd upstream;
    try {
        upstream.setCommandID(command->command_id);
        upstream.setInstrumentationPriority(priority);
    } catch (...) {
        diagnostic("InstrumentationLevelCmd preparation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }

    /* Account the request and copy the shared InstrumentationChannel locally
     * under the lifecycle mutex, then release it: provider send() may
     * synchronously invoke the InstrumentationReport metadata callback, which
     * independently locks the metadata mutex. */
    std::shared_ptr<irmel::InstrumentationChannel> channel;
    try {
        std::lock_guard lock{instrumentation->state->mutex};
        if (instrumentation->state->lifecycle != Lifecycle::Enabled) {
            diagnostic("Instrumentation channel is not enabled", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        channel = instrumentation->state->instrumentation;
        ++instrumentation->state->requests;
    } catch (...) {
        diagnostic("Instrumentation submission lock failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }

    auto state = instrumentation->state;
    try {
        try {
            input->future = channel->send(std::move(upstream));
            arm_worker(input);
        } catch (const std::exception& error) {
            (void)finish_channel(state);
            const char *what = error.what();
            const std::string_view text = what ? std::string_view{what} : std::string_view{};
            diagnostic(!text.empty() && valid_utf8(text) ? text : "provider send exception",
                       out, capacity, required);
            return AMS_MEL_PROVIDER_EXCEPTION;
        } catch (...) {
            (void)finish_channel(state);
            diagnostic("unknown provider send exception", out, capacity, required);
            return AMS_MEL_PROVIDER_EXCEPTION;
        }
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
        const SubmitFailpoint failpoint = submit_failpoint();
        if (failpoint == SubmitFailpoint::Allocation) throw std::bad_alloc{};
        if (failpoint == SubmitFailpoint::WorkerLaunch)
            throw std::system_error{
                std::make_error_code(std::errc::resource_unavailable_try_again)};
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

extern "C" ams_mel_status_t ams_mel_ir_instrumentation_request_wait(
    const ams_mel_ir_instrumentation_request *request, std::uint32_t timeout_ms,
    ams_mel_ir_instrumentation_result_v1 *result, char *out, std::size_t capacity,
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
        diagnostic("Instrumentation request wait failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    return AMS_MEL_INTERNAL_ERROR;
}

extern "C" ams_mel_status_t ams_mel_ir_instrumentation_request_close(
    ams_mel_ir_instrumentation_request **request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!request || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try { delete std::exchange(*request, nullptr); return AMS_MEL_OK; }
    catch (...) {
        diagnostic("request close failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_instrumentation_metadata_open(
    ams_mel_ir_instrumentation *instrumentation, std::size_t queue_capacity,
    ams_mel_ir_instrumentation_metadata **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!instrumentation || !instrumentation->state || !queue_capacity || !output ||
        *output || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    std::shared_ptr<MetadataState> state;
    std::shared_ptr<irmel::InstrumentationChannel> channel;
    try {
        /* Publish and retain the callback state first, then release the
         * lifecycle lock, then register: the provider may invoke the callback
         * synchronously from inside registerMetadataCallback. */
        {
            std::lock_guard lock{instrumentation->state->mutex};
            if (instrumentation->state->metadata_attempted ||
                (instrumentation->state->lifecycle != Lifecycle::Attached &&
                 instrumentation->state->lifecycle != Lifecycle::Enabled))
                return AMS_MEL_INVALID_ARGUMENT;
            instrumentation->state->metadata_attempted = true;
            state = std::make_shared<MetadataState>();
            state->capacity = queue_capacity;
            instrumentation->state->metadata = state;
            channel = instrumentation->state->instrumentation;
        }
        auto owner = std::make_unique<ams_mel_ir_instrumentation_metadata>();
        owner->state = state;
        owner->channel = instrumentation->state;
        const auto result = channel->registerMetadataCallback(
            std::function<void(irmel::Channel&, const irmel::InstrumentationReport *const)>{
                [state](irmel::Channel&,
                        const irmel::InstrumentationReport *value) noexcept {
                    metadata_callback(state, value);
                }});
        if (result != irmel::Return::Success) {
            std::lock_guard lock{state->mutex};
            state->lifecycle = MetadataLifecycle::Inactive;
            state->ready.notify_all();
            diagnostic("InstrumentationReport callback registration failed",
                       out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        *output = owner.release();
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        if (state) {
            std::lock_guard lock{state->mutex};
            state->lifecycle = MetadataLifecycle::Failed;
            state->ready.notify_all();
        }
        diagnostic("Instrumentation metadata allocation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        if (state) {
            std::lock_guard lock{state->mutex};
            state->lifecycle = MetadataLifecycle::Inactive;
            state->ready.notify_all();
        }
        diagnostic("provider exception during Instrumentation metadata registration",
                   out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_instrumentation_metadata_receive(
    ams_mel_ir_instrumentation_metadata *metadata, std::uint32_t timeout_ms,
    ams_mel_ir_instrumentation_metadata_event **output, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!metadata || !metadata->state || !output || *output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::unique_lock lock{metadata->state->mutex};
        if (metadata->state->queue.empty() &&
            metadata->state->lifecycle == MetadataLifecycle::Active && timeout_ms)
            metadata->state->ready.wait_for(lock, std::chrono::milliseconds{timeout_ms},
                [&] { return !metadata->state->queue.empty() ||
                             metadata->state->lifecycle != MetadataLifecycle::Active; });
        if (!metadata->state->queue.empty()) {
            auto owner = std::make_unique<ams_mel_ir_instrumentation_metadata_event>();
            owner->data = std::move(metadata->state->queue.front());
            metadata->state->queue.pop_front();
            *output = owner.release();
            return AMS_MEL_OK;
        }
        if (metadata->state->lifecycle == MetadataLifecycle::Failed)
            return AMS_MEL_PROVIDER_FAILED;
        if (metadata->state->lifecycle != MetadataLifecycle::Active)
            return AMS_MEL_STREAM_STOPPED;
        return AMS_MEL_TIMEOUT;
    } catch (...) {
        diagnostic("Instrumentation metadata receive failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_instrumentation_metadata_get_counters(
    const ams_mel_ir_instrumentation_metadata *metadata,
    ams_mel_ir_metadata_counters_v1 *output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!metadata || !metadata->state || !output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::lock_guard lock{metadata->state->mutex};
        *output = metadata->state->counters;
        return AMS_MEL_OK;
    } catch (...) { return AMS_MEL_INTERNAL_ERROR; }
}

extern "C" ams_mel_status_t ams_mel_ir_instrumentation_metadata_close(
    ams_mel_ir_instrumentation_metadata **metadata, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!metadata || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try {
        auto *owner = std::exchange(*metadata, nullptr);
        if (owner) {
            {
                /* Deactivates public consumption only; the provider callback
                 * keeps using channel-owned state until channel destruction. */
                std::lock_guard lock{owner->state->mutex};
                if (owner->state->lifecycle == MetadataLifecycle::Active)
                    owner->state->lifecycle = MetadataLifecycle::Inactive;
                owner->state->ready.notify_all();
            }
            delete owner;
        }
        return AMS_MEL_OK;
    } catch (...) { return AMS_MEL_INTERNAL_ERROR; }
}

extern "C" ams_mel_status_t ams_mel_ir_instrumentation_metadata_event_view(
    const ams_mel_ir_instrumentation_metadata_event *event,
    const ams_mel_ir_instrumentation_metadata_event_v1 **output, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!event || !event->data || !output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    *output = &event->data->view;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_instrumentation_metadata_event_close(
    ams_mel_ir_instrumentation_metadata_event **event, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!event || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try { delete std::exchange(*event, nullptr); return AMS_MEL_OK; }
    catch (...) { return AMS_MEL_INTERNAL_ERROR; }
}

extern "C" ams_mel_status_t ams_mel_ir_instrumentation_close(
    ams_mel_ir_instrumentation **instrumentation, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!instrumentation || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    auto *owner = *instrumentation;
    if (!owner) return AMS_MEL_OK;
    try {
        auto state = owner->state;
        bool now = false;
        {
            std::lock_guard lock{state->mutex};
            state->lifecycle = Lifecycle::Closed;
            now = state->requests == 0U;
        }
        /* Deactivate public metadata consumption immediately, including when
         * provider teardown is deferred behind pending requests. */
        if (auto metadata = state->metadata) {
            std::lock_guard lock{metadata->mutex};
            if (metadata->lifecycle == MetadataLifecycle::Active)
                metadata->lifecycle = MetadataLifecycle::Inactive;
            metadata->ready.notify_all();
        }
        const bool ok = !now || cleanup(state, false);
        if (now && state->channel) {
            diagnostic("Instrumentation detach failed; provider state retained",
                       out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        *instrumentation = nullptr;
        delete owner;
        if (!ok) {
            diagnostic("Instrumentation disable failed", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        return AMS_MEL_OK;
    } catch (...) {
        diagnostic("Instrumentation close failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}
