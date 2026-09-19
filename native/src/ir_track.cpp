#include <ams_mel/abi.h>
#include "internal.hpp"
#include "internal/ir_channel.hpp"

#include <irmel/library/track/TrackChannel.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
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

enum class Lifecycle { Attached, Enabled, Failed, Closed };

/* Track channel ownership foundation. No metadata state and no request count
 * exist in this release; both belong to the deferred Track report slice. */
struct TrackState {
    std::mutex mutex;
    std::shared_ptr<SessionState> session;
    std::shared_ptr<irmel::Channel> channel;
    std::shared_ptr<irmel::TrackChannel> track;
    Lifecycle lifecycle{Lifecycle::Attached};
    bool enable_attempted{};
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
    bool disable = false;
    {
        std::lock_guard lock{state->mutex};
        if (state->cleanup_started) return !state->channel;
        state->cleanup_started = true;
        channel = state->channel;
        disable = state->enable_attempted;
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
     * graph keeps provider code loaded until the whole graph is released. */
    channel.reset();
    return ok;
}
} // namespace

struct ams_mel_ir_track { std::shared_ptr<TrackState> state; };

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
        {
            std::lock_guard lock{state->mutex};
            state->lifecycle = Lifecycle::Closed;
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
