/* IRMEL-ADA-011: conditionally required (@RequiredIfIRSTTrackTasking)
 * SchedulingChannel ownership/lifecycle foundation.
 *
 * Implemented: Open (attach + validation), explicit Enable, owned
 * ChannelCapability snapshots, Close (inherited disable + detach), and the
 * inherited common Channel view adapter. NOT implemented: the four
 * SchedulingChannel::exec overloads, send(PrioritizedTrackListCmd) and the six
 * Scheduling registerMetadataCallback overloads. No translation unit in the
 * bridge calls any of them. */
#include <ams_mel/abi.h>
#include "internal.hpp"
#include "internal/ir_channel.hpp"
#include "internal/common_channel_requests.hpp"

#include <irmel/library/scheduling/SchedulingChannel.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
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
{ return value.data ? valid_utf8({value.data, value.size}) : value.size == 0U; }

std::string copy_view(const ams_mel_string_view_v1& value)
{ return value.size ? std::string{value.data, value.size} : std::string{}; }

mel::UCI_ID convert_id(const ams_mel_uci_id_v1& value)
{
    std::array<std::uint8_t, mel::UUID_SIZE> uuid{};
    std::copy(std::begin(value.uuid), std::end(value.uuid), uuid.begin());
    return {uuid, copy_view(value.descriptive_label)};
}

/* Pinned ChannelType::Scheduling is the fourth alternative; the C constant
 * reuses the existing published representation. */
static_assert(static_cast<std::uint32_t>(irmel::ChannelType::Scheduling) ==
              AMS_MEL_IR_CHANNEL_SCHEDULING);
static_assert(AMS_MEL_IR_CHANNEL_SCHEDULING == 3U);

enum class Lifecycle { Attached, Enabled, Failed, Closed };

/* Owns the complete Scheduling provider graph: the Session state (and thus
 * Control, manager and provider library), the attached generic Channel and its
 * SchedulingChannel view. `requests` counts ONLY admitted inherited common
 * Channel requests issued through a Scheduling view; it is never shared with
 * C2, Track, Health, Instrumentation or Image accounting. */
struct SchedulingState {
    std::mutex mutex;
    std::shared_ptr<SessionState> session;
    std::shared_ptr<irmel::Channel> channel;
    std::shared_ptr<irmel::SchedulingChannel> scheduling;
    Lifecycle lifecycle{Lifecycle::Attached};
    std::size_t requests{};
    bool enable_attempted{};
    bool cleanup_started{};
    /* Allocation-free permanent retention root for graphs whose detach
     * ownership could not be proven. */
    std::shared_ptr<SchedulingState> emergency_self;
    SchedulingState *emergency_next{};
    std::atomic<bool> emergency_retained{};
};

void retain_failed(const std::shared_ptr<SchedulingState>& state) noexcept
{
    static std::atomic<SchedulingState *> head{};
    bool expected = false;
    if (!state->emergency_retained.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) return;
    state->emergency_self = state;
    auto *current = head.load(std::memory_order_relaxed);
    do { state->emergency_next = current; }
    while (!head.compare_exchange_weak(current, state.get(),
        std::memory_order_release, std::memory_order_relaxed));
}

bool has_scheduling(const irmel::ChannelCapability& capability)
{
    const auto& types = capability.getChannelTypes();
    return std::find(types.begin(), types.end(), irmel::ChannelType::Scheduling) !=
           types.end();
}

/* Detach exactly once; any exception or non-Success leaves ownership
 * uncertain, which the caller handles by retaining the graph. */
bool detach(const std::shared_ptr<SchedulingState>& state,
            const std::shared_ptr<irmel::Channel>& channel) noexcept
{
    try {
        return state->session->control->detachChannel(channel) == irmel::Return::Success;
    } catch (...) { return false; }
}
/* Physical teardown. Returns false when the provider disable failed; leaves
 * state->channel non-null when detach could not be proven. Never runs while
 * an admitted common request still owns the graph. */
bool cleanup(const std::shared_ptr<SchedulingState>& state, bool retain_orphan)
{
    std::shared_ptr<irmel::Channel> channel;
    bool disable = false;
    {
        std::lock_guard lock{state->mutex};
        if (state->cleanup_started) return !state->channel;
        if (state->requests != 0U) return true;
        state->cleanup_started = true;
        channel = state->channel;
        disable = state->enable_attempted;
        /* disable() is attempted at most once: a Close retried after an
         * unproven detach only retries the detach. */
        state->enable_attempted = false;
    }
    bool ok = true;
    /* Inherited Channel::disable() is invoked only when Enable was attempted;
     * this foundation has no separate public Disable. */
    if (disable && channel) {
        try { if (channel->disable() != irmel::Return::Success) ok = false; }
        catch (...) { ok = false; }
    }
    /* A failed disable never proves ownership safety: detach regardless. */
    const bool detached = !channel || detach(state, channel);
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
        state->scheduling.reset();
        state->channel.reset();
        state->lifecycle = Lifecycle::Closed;
    }
    /* The provider channel is destroyed here while this state still holds the
     * Session graph, so provider code stays loaded until the graph is gone. */
    channel.reset();
    return ok;
}

/* Called by the final admitted common request (never under the mutex). */
bool finish_request(const std::shared_ptr<SchedulingState>& state)
{
    bool close = false;
    {
        std::lock_guard lock{state->mutex};
        if (state->requests) --state->requests;
        close = state->requests == 0U && state->lifecycle == Lifecycle::Closed;
    }
    return !close || cleanup(state, true);
}

/* The one Scheduling capability policy, shared by the typed export and the
 * common Channel adapter. */
ams_mel_status_t scheduling_capability(const std::shared_ptr<SchedulingState>& state,
    ams_mel_ir_channel_capability **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    try {
        std::lock_guard lock{state->mutex};
        if ((state->lifecycle != Lifecycle::Attached &&
             state->lifecycle != Lifecycle::Enabled) || !state->scheduling) {
            diagnostic("Scheduling channel is not attached", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        /* Reuses the one shared native ChannelCapability snapshot; all fields,
         * including TaskScheduleDepth, are copied verbatim. */
        return ams_mel::internal::snapshot_capability(
            *state->scheduling, output, out, capacity, required);
    } catch (...) {
        diagnostic("Scheduling capability query failed", out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}
} // namespace

struct ams_mel_ir_scheduling { std::shared_ptr<SchedulingState> state; };
namespace ams_mel_common {
CommonChannelAccess common_from_scheduling(const ams_mel_ir_scheduling *owner)
{
    return {owner->state,
        [](const std::shared_ptr<void>& value) noexcept {
            return std::static_pointer_cast<SchedulingState>(value)->session->admission;
        },
        [](const std::shared_ptr<void>& value, std::shared_ptr<irmel::Channel>& channel,
           CommonRequestClaim& claim) {
            auto state = std::static_pointer_cast<SchedulingState>(value);
            std::lock_guard lock{state->mutex};
            if ((state->lifecycle != Lifecycle::Attached &&
                 state->lifecycle != Lifecycle::Enabled) ||
                !state->channel || !state->scheduling) return false;
            channel = state->channel;
            /* Scheduling-only accounting domain. */
            ++state->requests;
            claim = CommonRequestClaim{state,
                [](const std::shared_ptr<void>& erased) noexcept {
                    auto claimed = std::static_pointer_cast<SchedulingState>(erased);
                    try { return finish_request(claimed); }
                    catch (...) { retain_failed(claimed); return false; }
                }, "deferred Scheduling cleanup failed"};
            return true;
        },
        [](const std::shared_ptr<void>& erased, ams_mel_ir_channel_capability **output,
           char *out, std::size_t capacity, std::size_t *required) noexcept {
            return scheduling_capability(std::static_pointer_cast<SchedulingState>(erased),
                                         output, out, capacity, required);
        }};
}
} // namespace ams_mel_common

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
/* Test-facade-only observation of the Scheduling request accounting. The
 * observer holds only a weak reference, so it also proves that no hidden
 * strong owner (for example a weak Channel view) outlives cleanup. */
struct ams_mel_test_scheduling_observer { std::weak_ptr<SchedulingState> state; };
extern "C" __attribute__((visibility("default"))) int ams_mel_test_scheduling_requests(
    const ams_mel_ir_scheduling *owner, std::size_t *requests) noexcept
{
    try {
        if (!owner || !owner->state || !requests) return 0;
        std::lock_guard lock{owner->state->mutex};
        *requests = owner->state->requests;
        return 1;
    } catch (...) { return 0; }
}
extern "C" __attribute__((visibility("default"))) int ams_mel_test_scheduling_observer_from(
    const ams_mel_ir_scheduling *owner, ams_mel_test_scheduling_observer **output) noexcept
{
    if (!owner || !output || *output) return 0;
    try { *output = new ams_mel_test_scheduling_observer{owner->state}; return 1; }
    catch (...) { return 0; }
}
/* Returns 1 with *alive = 0/1 and, when alive, the Scheduling request count. */
extern "C" __attribute__((visibility("default"))) int ams_mel_test_scheduling_observer_state(
    const ams_mel_test_scheduling_observer *observer, int *alive, std::size_t *requests) noexcept
{
    if (!observer || !alive || !requests) return 0;
    try {
        auto state = observer->state.lock();
        *alive = state ? 1 : 0;
        *requests = 0U;
        if (state) {
            std::lock_guard lock{state->mutex};
            *requests = state->requests;
        }
        return 1;
    } catch (...) { return 0; }
}
extern "C" __attribute__((visibility("default"))) void ams_mel_test_scheduling_observer_close(
    ams_mel_test_scheduling_observer **observer) noexcept
{ if (observer) { delete *observer; *observer = nullptr; } }
#endif
extern "C" ams_mel_status_t ams_mel_ir_scheduling_open(
    const ams_mel_session *session, const ams_mel_ir_scheduling_config_v1 *config,
    ams_mel_ir_scheduling **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!session || !session->state || !config || !output || *output ||
        (!out && capacity) ||
        config->channel_type != AMS_MEL_IR_CHANNEL_SCHEDULING ||
        !valid_view(config->channel_id.descriptive_label) ||
        !valid_view(config->platform_id.descriptive_label) ||
        !valid_view(config->sensor_location.key) ||
        !valid_view(config->sensor_location.system_name)) {
        diagnostic("invalid Scheduling configuration", out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    std::shared_ptr<SchedulingState> state;
    try {
        /* SchedulingChannel is conditional on provider tasking support. A
         * provider that does not publish it is an expected unsupported path,
         * reported before any attachment; the Session remains usable. */
        bool advertised = false;
        for (const auto& capability : session->state->control->getCapabilities())
            if (has_scheduling(capability)) { advertised = true; break; }
        if (!advertised) {
            diagnostic("provider does not advertise Scheduling", out, capacity, required);
            return AMS_MEL_INITIALIZATION_FAILED;
        }
        state = std::make_shared<SchedulingState>();
        state->session = session->state;
        mel::ForeignKey key{copy_view(config->sensor_location.key),
                            copy_view(config->sensor_location.system_name)};
        mel::ComponentLocation location{config->sensor_location.offset_x_m,
            config->sensor_location.offset_y_m, config->sensor_location.offset_z_m, key};
        irmel::Config upstream{convert_id(config->channel_id),
            irmel::ChannelType::Scheduling, convert_id(config->platform_id),
            std::move(location), {}, false, false};
        /* Exactly one attachment attempt. */
        state->channel = state->session->control->attachChannel(upstream);
        if (!state->channel) {
            diagnostic("attachChannel returned null", out, capacity, required);
            return AMS_MEL_FACTORY_FAILED;
        }
        state->scheduling =
            std::dynamic_pointer_cast<irmel::SchedulingChannel>(state->channel);
        bool compatible = static_cast<bool>(state->scheduling);
        const char *reason = "attached channel is not a SchedulingChannel";
        if (compatible) {
            compatible = has_scheduling(state->channel->getCapabilities());
            if (!compatible) reason = "Scheduling channel capability omits Scheduling";
        }
        if (!compatible) {
            const bool detached = detach(state, state->channel);
            if (!detached) retain_failed(state);
            diagnostic(detached ? reason : "incompatible Scheduling channel and detach failed",
                       out, capacity, required);
            return detached ? AMS_MEL_INITIALIZATION_FAILED : AMS_MEL_PROVIDER_FAILED;
        }
        /* Publish the owner only after every validation succeeded. */
        auto owner = std::make_unique<ams_mel_ir_scheduling>();
        owner->state = std::move(state);
        *output = owner.release();
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        if (state && state->channel && !detach(state, state->channel)) retain_failed(state);
        diagnostic("allocation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        /* Includes a throwing attachChannel or getCapabilities. The graph is
         * never destroyed while detach ownership remains uncertain. */
        if (state && state->channel) {
            if (!detach(state, state->channel)) {
                retain_failed(state);
                diagnostic("provider exception during Scheduling open and detach failed",
                           out, capacity, required);
                return AMS_MEL_PROVIDER_FAILED;
            }
            diagnostic("provider exception during Scheduling open", out, capacity, required);
            return AMS_MEL_INITIALIZATION_FAILED;
        }
        diagnostic("provider exception during Scheduling open", out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_scheduling_enable(
    ams_mel_ir_scheduling *scheduling, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!scheduling || !scheduling->state || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    auto state = scheduling->state;
    try {
        std::lock_guard lock{state->mutex};
        if (state->lifecycle == Lifecycle::Enabled) return AMS_MEL_OK;
        if (state->lifecycle != Lifecycle::Attached) {
            /* Failed or Closed: the provider is never silently retried. */
            diagnostic("Scheduling channel is not attached", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        state->enable_attempted = true;
        if (state->channel->enable() != irmel::Return::Success) {
            state->lifecycle = Lifecycle::Failed;
            diagnostic("Scheduling enable failed", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        state->lifecycle = Lifecycle::Enabled;
        return AMS_MEL_OK;
    } catch (...) {
        /* The lock_guard above was released by unwinding. */
        try { std::lock_guard lock{state->mutex}; state->lifecycle = Lifecycle::Failed; }
        catch (...) {}
        diagnostic("provider exception during Scheduling enable", out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}
extern "C" ams_mel_status_t ams_mel_ir_scheduling_get_capabilities(
    ams_mel_ir_scheduling *scheduling, ams_mel_ir_channel_capability **output, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!scheduling || !scheduling->state || !output || *output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    return scheduling_capability(scheduling->state, output, out, capacity, required);
}

extern "C" ams_mel_status_t ams_mel_ir_scheduling_close(
    ams_mel_ir_scheduling **scheduling, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!scheduling || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    auto *owner = *scheduling;
    if (!owner) return AMS_MEL_OK;
    try {
        auto state = owner->state;
        bool now = false;
        {
            std::lock_guard lock{state->mutex};
            /* New common requests are refused from here on; admitted ones
             * keep their claim and own the graph until completion. */
            state->lifecycle = Lifecycle::Closed;
            now = state->requests == 0U;
        }
        const bool ok = !now || cleanup(state, false);
        if (now && state->channel) {
            /* Detach could not be proven: the graph and the caller's owner are
             * retained so a later Close can retry. */
            diagnostic("Scheduling detach failed; provider state retained",
                       out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        *scheduling = nullptr;
        delete owner;
        if (!ok) {
            diagnostic("Scheduling disable failed", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        return AMS_MEL_OK;
    } catch (...) {
        diagnostic("Scheduling close failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}
