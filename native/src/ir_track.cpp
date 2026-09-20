#include <ams_mel/abi.h>
#include "internal.hpp"
#include "internal/ir_channel.hpp"

#include <irmel/library/track/IRSTTrackReport.h>
#include <irmel/library/track/TrackChannel.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
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

/* Complete IRSTTrackReport copy. Every upstream getter is read exactly once and
 * every floating-point value is copied verbatim: no clamping, no normalization,
 * and no narrowing. Upstream IrstTrackState and IrstTrackMode declare no
 * MaxExclusive value, so anything above Dropped or Stare is malformed. */
bool copy_track_report(const irmel::IRSTTrackReport& source,
                       ams_mel_ir_track_report_v1& destination) noexcept
{
    const auto state = static_cast<std::uint32_t>(source.getState());
    const auto mode = static_cast<std::uint32_t>(source.getMode());
    if (state > AMS_MEL_IR_TRACK_STATE_DROPPED) return false;
    if (mode > AMS_MEL_IR_TRACK_MODE_STARE) return false;

    destination.system_time_ns = source.getSystemTime().count();
    destination.activity_id = source.getActivityId();

    const auto& measured = source.getMeasuredNed();
    destination.measured_ned.north = measured.getNorth();
    destination.measured_ned.east = measured.getEast();
    destination.measured_ned.down = measured.getDown();
    destination.measured_intensity = source.getMeasuredIntensity();
    destination.measured_snr = source.getMeasuredSnr();

    const auto& filtered = source.getFilteredNed();
    destination.filtered_ned.north = filtered.getNorth();
    destination.filtered_ned.east = filtered.getEast();
    destination.filtered_ned.down = filtered.getDown();
    destination.filtered_intensity = source.getFilteredIntensity();
    destination.filtered_snr = source.getFilteredSnr();

    destination.range_m = source.getRange();
    destination.range_error_m = source.getRangeError();
    destination.spatial_extent_rad = source.getSpatialExtent();
    destination.track_quality = source.getTrackQuality();
    destination.clutter = source.getClutter();

    destination.age_ns = source.getAge().count();

    destination.state = state;
    destination.mode = mode;
    return true;
}

struct EventData { ams_mel_ir_track_metadata_event_v1 view{}; };

enum class MetadataLifecycle { Active, Inactive, Stopped, Failed };

/* Follows the proven Instrumentation metadata shape: a bounded DROP-INCOMING
 * FIFO of owned events, saturating counters, and an explicit in-flight callback
 * count whose drain is the quiescence proof. */
struct MetadataState {
    std::mutex mutex;
    std::condition_variable ready, callbacks_done;
    std::deque<std::unique_ptr<EventData>> queue;
    std::size_t capacity{};
    ams_mel_ir_metadata_counters_v1 counters{};
    MetadataLifecycle lifecycle{MetadataLifecycle::Active};
    std::atomic<std::uint64_t> callbacks_in_flight{};

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

/* Every provider callback increments this count before touching callback state
 * and decrements it on return, including on an exceptional path. */
struct CallbackGuard {
    std::shared_ptr<MetadataState> state;
    explicit CallbackGuard(std::shared_ptr<MetadataState> value) noexcept
        : state(std::move(value))
    { state->callbacks_in_flight.fetch_add(1, std::memory_order_acq_rel); }
    ~CallbackGuard() noexcept
    {
        if (state->callbacks_in_flight.fetch_sub(1, std::memory_order_acq_rel) == 1)
            state->callbacks_done.notify_all();
    }
};

/* Provider callback boundary for the @RequiredIfTrack IRSTTrackReport. It never
 * calls into Ada, never lets an exception escape into provider code, and always
 * accounts for the event. A null payload is malformed. The queue drops the
 * INCOMING report when full so the earliest reports survive. */
void metadata_callback(const std::shared_ptr<MetadataState>& state,
                       const irmel::IRSTTrackReport *value) noexcept
{
    try {
        CallbackGuard guard{state};
        {
            std::lock_guard lock{state->mutex};
            increment(state->counters.events_received);
            if (state->lifecycle != MetadataLifecycle::Active) return;
        }
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
        if (const char *failure = std::getenv("AMS_MEL_TEST_TRACK_CALLBACK_FAILURE");
            failure && std::strcmp(failure, "allocation") == 0)
            throw std::bad_alloc{};
#endif
        std::unique_ptr<EventData> event;
        if (value) {
            auto owned = std::make_unique<EventData>();
            owned->view.kind = AMS_MEL_IR_TRACK_METADATA_IRST_TRACK_REPORT;
            if (copy_track_report(*value, owned->view.track_report))
                event = std::move(owned);
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

/* Track channel ownership foundation, extended only as far as the
 * @RequiredIfTrack IRSTTrackReport callback requires. There is still no request
 * count: no Track send is implemented. */
struct TrackState {
    std::mutex mutex;
    std::shared_ptr<SessionState> session;
    std::shared_ptr<irmel::Channel> channel;
    std::shared_ptr<irmel::TrackChannel> track;
    /* Callback-accessible state belongs to the channel, not to the public
     * metadata wrapper: upstream declares no unregister operation. */
    std::shared_ptr<MetadataState> metadata;
    Lifecycle lifecycle{Lifecycle::Attached};
    bool enable_attempted{};
    bool metadata_attempted{};
    bool cleanup_started{};
    /* Allocation-free permanent retention root for graphs whose detach
     * ownership could not be proven. */
    std::shared_ptr<TrackState> emergency_self;
    TrackState *emergency_next{};
    std::atomic<bool> emergency_retained{};
};

void retain_failed(const std::shared_ptr<TrackState>& state) noexcept
{
    static std::atomic<TrackState *> head{};
    bool expected = false;
    if (!state->emergency_retained.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) return;
    state->emergency_self = state;
    auto *current = head.load(std::memory_order_relaxed);
    do { state->emergency_next = current; }
    while (!head.compare_exchange_weak(current, state.get(),
        std::memory_order_release, std::memory_order_relaxed));
}

bool has_track(const irmel::ChannelCapability& capability)
{
    const auto& types = capability.getChannelTypes();
    return std::find(types.begin(), types.end(), irmel::ChannelType::IRSTTrack) !=
           types.end();
}

/* Returns false when the provider disable failed; leaves state->channel
 * non-null when detach could not be proven. */
bool cleanup(const std::shared_ptr<TrackState>& state, bool retain_orphan)
{
    std::shared_ptr<irmel::Channel> channel;
    std::shared_ptr<MetadataState> metadata;
    bool disable = false;
    {
        std::lock_guard lock{state->mutex};
        if (state->cleanup_started) return !state->channel;
        state->cleanup_started = true;
        channel = state->channel;
        metadata = state->metadata;
        disable = state->enable_attempted;
    }
    /* Public consumption stops before provider teardown begins, and any waiting
     * receiver is woken immediately. */
    if (metadata) {
        std::lock_guard lock{metadata->mutex};
        if (metadata->lifecycle == MetadataLifecycle::Active)
            metadata->lifecycle = MetadataLifecycle::Inactive;
        metadata->ready.notify_all();
    }
    bool ok = true;
    if (disable && channel) {
        try { if (channel->disable() != irmel::Return::Success) ok = false; }
        catch (...) { ok = false; }
    }
    /* A failed disable never proves ownership safety, so detach is attempted
     * regardless of the disable outcome. */
    bool detached = !channel;
    if (channel) {
        try {
            detached = state->session->control->detachChannel(channel) ==
                       irmel::Return::Success;
        } catch (...) { detached = false; }
    }
    if (!detached) {
        /* The complete callback/provider graph stays alive so a later Close can
         * retry; the metadata is failed rather than stopped because quiescence
         * was never established. */
        if (metadata) {
            std::lock_guard lock{metadata->mutex};
            metadata->lifecycle = MetadataLifecycle::Failed;
            metadata->ready.notify_all();
        }
        {
            std::lock_guard lock{state->mutex};
            state->cleanup_started = false;
        }
        if (retain_orphan) retain_failed(state);
        return false;
    }
    {
        std::lock_guard lock{state->mutex};
        state->track.reset();
        state->channel.reset();
        state->enable_attempted = false;
        state->lifecycle = Lifecycle::Closed;
    }
    /* Destroying the provider channel while this state still holds the session
     * graph keeps provider code loaded until the whole graph is released. This
     * destruction is the provider callback-quiescence boundary. */
    channel.reset();
    /* Only after provider channel destruction is waiting for the in-flight
     * callback count to drain meaningful. */
    if (metadata) {
        std::unique_lock lock{metadata->mutex};
        metadata->callbacks_done.wait(lock, [&] {
            return metadata->callbacks_in_flight.load(std::memory_order_acquire) == 0U; });
        if (metadata->lifecycle != MetadataLifecycle::Failed)
            metadata->lifecycle = MetadataLifecycle::Stopped;
        metadata->ready.notify_all();
    }
    return ok;
}
} // namespace

struct ams_mel_ir_track { std::shared_ptr<TrackState> state; };
/* Public consumption wrapper only. Deleting it never unregisters the provider
 * callback and never destroys the callback-accessible state. */
struct ams_mel_ir_track_metadata {
    std::shared_ptr<MetadataState> state;
    std::shared_ptr<TrackState> track;
};
struct ams_mel_ir_track_metadata_event { std::unique_ptr<EventData> data; };

extern "C" ams_mel_status_t ams_mel_ir_track_open(
    const ams_mel_session *session, const ams_mel_ir_track_config_v1 *config,
    ams_mel_ir_track **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!session || !session->state || !config || !output || *output ||
        (!out && capacity) ||
        config->channel_type != AMS_MEL_IR_CHANNEL_IRST_TRACK ||
        !valid_view(config->channel_id.descriptive_label) ||
        !valid_view(config->platform_id.descriptive_label) ||
        !valid_view(config->sensor_location.key) ||
        !valid_view(config->sensor_location.system_name)) {
        diagnostic("invalid Track configuration", out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    std::shared_ptr<TrackState> state;
    try {
        state = std::make_shared<TrackState>();
        state->session = session->state;
        mel::ForeignKey key{copy_view(config->sensor_location.key),
                            copy_view(config->sensor_location.system_name)};
        mel::ComponentLocation location{config->sensor_location.offset_x_m,
            config->sensor_location.offset_y_m, config->sensor_location.offset_z_m,
            key};
        irmel::Config upstream{convert_id(config->channel_id),
            irmel::ChannelType::IRSTTrack, convert_id(config->platform_id),
            std::move(location), {}, false, false};
        state->channel = state->session->control->attachChannel(upstream);
        if (!state->channel) {
            diagnostic("attachChannel returned null", out, capacity, required);
            return AMS_MEL_FACTORY_FAILED;
        }
        state->track = std::dynamic_pointer_cast<irmel::TrackChannel>(state->channel);
        bool compatible = static_cast<bool>(state->track);
        const char *reason = "attached channel is not a TrackChannel";
        if (compatible) {
            compatible = has_track(state->channel->getCapabilities());
            if (!compatible) reason = "Track capability omits IRSTTrack";
        }
        if (!compatible) {
            bool detached = false;
            try {
                detached = state->session->control->detachChannel(state->channel) ==
                           irmel::Return::Success;
            } catch (...) {}
            if (!detached) retain_failed(state);
            diagnostic(detached ? reason :
                       "incompatible Track channel and detach failed",
                       out, capacity, required);
            return detached ? AMS_MEL_INITIALIZATION_FAILED : AMS_MEL_PROVIDER_FAILED;
        }
        auto owner = std::make_unique<ams_mel_ir_track>();
        owner->state = std::move(state);
        *output = owner.release();
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        if (state && state->channel) retain_failed(state);
        diagnostic("allocation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        /* Includes a throwing getCapabilities. The provider graph is never
         * destructed while detach ownership remains uncertain. */
        if (state && state->channel) {
            bool detached = false;
            try {
                detached = state->session->control->detachChannel(state->channel) ==
                           irmel::Return::Success;
            } catch (...) {}
            if (!detached) {
                retain_failed(state);
                diagnostic("provider exception during Track open and detach failed",
                           out, capacity, required);
                return AMS_MEL_PROVIDER_FAILED;
            }
            {
                std::lock_guard lock{state->mutex};
                state->track.reset();
                state->channel.reset();
                state->lifecycle = Lifecycle::Closed;
            }
            diagnostic("provider exception during Track open",
                       out, capacity, required);
            return AMS_MEL_INITIALIZATION_FAILED;
        }
        diagnostic("provider exception during Track open", out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_track_enable(
    ams_mel_ir_track *track, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!track || !track->state || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    auto state = track->state;
    try {
        std::lock_guard lock{state->mutex};
        if (state->lifecycle == Lifecycle::Enabled) return AMS_MEL_OK;
        if (state->lifecycle != Lifecycle::Attached) {
            diagnostic("Track channel is not attached", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        state->enable_attempted = true;
        if (state->channel->enable() != irmel::Return::Success) {
            state->lifecycle = Lifecycle::Failed;
            diagnostic("Track enable failed", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        state->lifecycle = Lifecycle::Enabled;
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        diagnostic("allocation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        try {
            std::lock_guard lock{state->mutex};
            state->lifecycle = Lifecycle::Failed;
        } catch (...) {}
        diagnostic("provider exception during Track enable", out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_track_get_capabilities(
    ams_mel_ir_track *track, ams_mel_ir_channel_capability **output, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!track || !track->state || !output || *output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::lock_guard lock{track->state->mutex};
        if (track->state->lifecycle != Lifecycle::Attached &&
            track->state->lifecycle != Lifecycle::Enabled)
            return AMS_MEL_PROVIDER_FAILED;
        /* Reuses the one shared native ChannelCapability snapshot. */
        return ams_mel::internal::snapshot_capability(
            *track->state->track, output, out, capacity, required);
    } catch (...) {
        diagnostic("Track capability query failed", out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_track_metadata_open(
    ams_mel_ir_track *track, std::size_t queue_capacity,
    ams_mel_ir_track_metadata **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!track || !track->state || !queue_capacity || !output || *output ||
        (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    std::shared_ptr<MetadataState> state;
    std::shared_ptr<irmel::TrackChannel> channel;
    try {
        /* Publish and retain the callback state first, then release the
         * lifecycle lock, then register. The provider may invoke the
         * IRSTTrackReport callback synchronously from inside
         * registerMetadataCallback, so TrackState::mutex is never held across
         * the registration call. */
        {
            std::lock_guard lock{track->state->mutex};
            /* Registration is one-shot: upstream has no unregister. */
            if (track->state->metadata_attempted ||
                (track->state->lifecycle != Lifecycle::Attached &&
                 track->state->lifecycle != Lifecycle::Enabled))
                return AMS_MEL_INVALID_ARGUMENT;
            track->state->metadata_attempted = true;
            state = std::make_shared<MetadataState>();
            state->capacity = queue_capacity;
            track->state->metadata = state;
            channel = track->state->track;
        }
        auto owner = std::make_unique<ams_mel_ir_track_metadata>();
        owner->state = state;
        owner->track = track->state;
        const auto result = channel->registerMetadataCallback(
            std::function<void(irmel::Channel&, const irmel::IRSTTrackReport *const)>{
                [state](irmel::Channel&,
                        const irmel::IRSTTrackReport *value) noexcept {
                    metadata_callback(state, value);
                }});
        if (result != irmel::Return::Success) {
            /* No owner escapes, but the callback state stays retained by the
             * Track channel because registration may have partially taken. */
            std::lock_guard lock{state->mutex};
            state->lifecycle = MetadataLifecycle::Inactive;
            state->ready.notify_all();
            diagnostic("IRSTTrackReport callback registration failed",
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
        diagnostic("Track metadata allocation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        if (state) {
            std::lock_guard lock{state->mutex};
            state->lifecycle = MetadataLifecycle::Inactive;
            state->ready.notify_all();
        }
        diagnostic("provider exception during Track metadata registration",
                   out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_track_metadata_receive(
    ams_mel_ir_track_metadata *metadata, std::uint32_t timeout_ms,
    ams_mel_ir_track_metadata_event **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
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
        /* A queued event is always delivered first, whatever the lifecycle. */
        if (!metadata->state->queue.empty()) {
            auto owner = std::make_unique<ams_mel_ir_track_metadata_event>();
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
        diagnostic("Track metadata receive failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_track_metadata_get_counters(
    const ams_mel_ir_track_metadata *metadata,
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

extern "C" ams_mel_status_t ams_mel_ir_track_metadata_close(
    ams_mel_ir_track_metadata **metadata, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!metadata || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try {
        auto *owner = std::exchange(*metadata, nullptr);
        if (owner) {
            {
                /* Deactivates public consumption only. The retained provider
                 * callback keeps using channel-owned state until provider
                 * channel destruction; this call never blocks on it. */
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

extern "C" ams_mel_status_t ams_mel_ir_track_metadata_event_view(
    const ams_mel_ir_track_metadata_event *event,
    const ams_mel_ir_track_metadata_event_v1 **output, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!event || !event->data || !output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    *output = &event->data->view;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_track_metadata_event_close(
    ams_mel_ir_track_metadata_event **event, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!event || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try { delete std::exchange(*event, nullptr); return AMS_MEL_OK; }
    catch (...) { return AMS_MEL_INTERNAL_ERROR; }
}


extern "C" ams_mel_status_t ams_mel_ir_track_close(
    ams_mel_ir_track **track, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!track || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    auto *owner = *track;
    if (!owner) return AMS_MEL_OK;
    try {
        auto state = owner->state;
        std::shared_ptr<MetadataState> metadata;
        {
            std::lock_guard lock{state->mutex};
            state->lifecycle = Lifecycle::Closed;
            metadata = state->metadata;
        }
        /* Deactivate public metadata consumption and wake receivers before any
         * provider teardown begins. */
        if (metadata) {
            std::lock_guard lock{metadata->mutex};
            if (metadata->lifecycle == MetadataLifecycle::Active)
                metadata->lifecycle = MetadataLifecycle::Inactive;
            metadata->ready.notify_all();
        }
        const bool ok = cleanup(state, false);
        if (state->channel) {
            /* Detach could not be proven: the complete graph and the caller's
             * owner are retained so a later Close can retry. */
            diagnostic("Track detach failed; provider state retained",
                       out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        *track = nullptr;
        delete owner;
        if (!ok) {
            diagnostic("Track disable failed", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        return AMS_MEL_OK;
    } catch (...) {
        diagnostic("Track close failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}
