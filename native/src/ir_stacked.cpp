/* IRMEL-ADA-016: required channel details and common communications, even on
 * sensors without stack acquisition. No image buffers or listeners are created.
 * Application-owned buffers are supported separately from acquisition;
 * optional LOS3D is not registered. */
#include <ams_mel/abi.h>
#include "internal.hpp"
#include "internal/ir_channel.hpp"
#include "internal/common_channel_requests.hpp"
#include "internal/channel_disable.hpp"
#include "internal/channel_buffer_registration.hpp"
#include "internal/channel_comms_metadata.hpp"

#include <irmel/library/stacked-image/StackedImageChannel.h>

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

/* The required StackedImage channel is distinct from an acquisition service. */
static_assert(static_cast<std::uint32_t>(irmel::ChannelType::StackedImage) ==
              AMS_MEL_IR_CHANNEL_STACKED_IMAGE);
static_assert(AMS_MEL_IR_CHANNEL_STACKED_IMAGE == 6U);

enum class Lifecycle { Attached, Enabling, Enabled, Disabling, Failed, Closed };

/* Owns the complete Stacked provider graph: the Session state (and thus
 * Control, manager and provider library), the attached generic Channel and its
 * StackedImageChannel view. `requests` counts admitted inherited common
 * Channel requests and temporary registration/query operations; it is never shared with
 * C2, Track, Health, Instrumentation or Image accounting. */
struct StackedState {
    ams_mel_common::BufferRegistrations application_buffers;
    std::shared_ptr<CommsMetadataState> comms_metadata;
    bool comms_metadata_attempted{};
    std::mutex mutex;
    std::shared_ptr<SessionState> session;
    std::shared_ptr<irmel::Channel> channel;
    std::shared_ptr<irmel::StackedImageChannel> stacked;
    Lifecycle lifecycle{Lifecycle::Attached};
    std::size_t requests{};
    bool enable_attempted{};
    bool cleanup_started{};
    bool disable_failed{};
    /* Allocation-free permanent retention root for graphs whose detach
     * ownership could not be proven. */
    std::shared_ptr<StackedState> emergency_self;
    StackedState *emergency_next{};
    std::atomic<bool> emergency_retained{};
};

void retain_failed(const std::shared_ptr<StackedState>& state) noexcept
{
    static std::atomic<StackedState *> head{};
    bool expected = false;
    if (!state->emergency_retained.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) return;
    state->emergency_self = state;
    auto *current = head.load(std::memory_order_relaxed);
    do { state->emergency_next = current; }
    while (!head.compare_exchange_weak(current, state.get(),
        std::memory_order_release, std::memory_order_relaxed));
}

bool has_stacked(const irmel::ChannelCapability& capability)
{
    const auto& types = capability.getChannelTypes();
    return std::find(types.begin(), types.end(), irmel::ChannelType::StackedImage) !=
           types.end();
}

/* Detach exactly once; any exception or non-Success leaves ownership
 * uncertain, which the caller handles by retaining the graph. */
bool detach(const std::shared_ptr<StackedState>& state,
            const std::shared_ptr<irmel::Channel>& channel) noexcept
{
    try {
        return state->session->control->detachChannel(channel) == irmel::Return::Success;
    } catch (...) { return false; }
}
/* Physical teardown. Returns false when the provider disable failed; leaves
 * state->channel non-null when detach could not be proven. Never runs while
 * an admitted common request still owns the graph. */
bool cleanup(const std::shared_ptr<StackedState>& state, bool retain_orphan)
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
    bool ok = !state->disable_failed;
    /* Only an outstanding enable cycle needs a cleanup Disable. */
    if (disable && channel) {
        try { if (channel->disable() != irmel::Return::Success) ok = false; }
        catch (...) { ok = false; }
    }
    state->disable_failed = !ok;
    /* A failed disable never proves ownership safety: detach regardless. */
    const bool detached = !channel || detach(state, channel);
    if (!detached) {
        if (state->comms_metadata) state->comms_metadata->deactivate(true);
        {
            std::lock_guard lock{state->mutex};
            state->cleanup_started = false;
        }
        /* There is no public owner to retry deferred cleanup. Retain the
         * exact graph, including its deleters and library, rather than unload
         * provider code whose ownership has not been established. */
        if (retain_orphan) retain_failed(state);

        return false;
    }
    {
        std::lock_guard lock{state->mutex};
        state->stacked.reset();
        state->channel.reset();
        state->lifecycle = Lifecycle::Closed;
    }
    /* The provider channel is destroyed here while this state still holds the
     * Session graph, so provider code stays loaded until the graph is gone. */
    channel.reset();
    if (state->comms_metadata) {
        if (!ok) state->comms_metadata->deactivate(true);
        state->comms_metadata->drain();
    }
    return ok;
}

/* Called by the final admitted common request (never under the mutex). */
bool finish_request(const std::shared_ptr<StackedState>& state)
{
    bool close = false;
    {
        std::lock_guard lock{state->mutex};
        if (state->requests) --state->requests;
        close = state->requests == 0U && state->lifecycle == Lifecycle::Closed;
    }
    return !close || cleanup(state, true);
}

/* The one Stacked capability policy, shared by the typed export and the
 * common Channel adapter. */
ams_mel_status_t stacked_capability(const std::shared_ptr<StackedState>& state,
    ams_mel_ir_channel_capability **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    try {
        std::shared_ptr<irmel::StackedImageChannel> channel;
        {
            std::lock_guard lock{state->mutex};
            if ((state->lifecycle != Lifecycle::Attached &&
                 state->lifecycle != Lifecycle::Enabled) || !state->stacked) {
                diagnostic("Stacked channel is not attached", out, capacity, required);
                return AMS_MEL_PROVIDER_FAILED;
            }
            channel = state->stacked;
            ++state->requests;
        }
        /* Reuses the one shared native ChannelCapability snapshot; all fields,
         * including zero image layout and empty nested collections, are copied.
         * Provider code runs without the lifecycle mutex. The temporary claim
         * also prevents a reentrant Close from physically detaching mid-query. */
        auto status = ams_mel::internal::snapshot_capability(
            *channel, output, out, capacity, required);
        channel.reset();
        if (!finish_request(state)) {
            ams_mel_ir_channel_capability_close(output, nullptr, 0, nullptr);
            diagnostic("deferred Stacked cleanup failed", out, capacity, required);
            status = AMS_MEL_PROVIDER_FAILED;
        }
        return status;
    } catch (...) {
        diagnostic("Stacked capability query failed", out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}
} // namespace

struct ams_mel_ir_stacked { std::shared_ptr<StackedState> state; };
namespace {
bool reconcile_application_buffer(ams_mel_ir_buffer& buffer) noexcept
{
    auto state = std::static_pointer_cast<StackedState>(buffer.channel_owner);
    return ams_mel_common::reconcile_buffer(buffer, state->mutex,
        state->application_buffers, [] { return true; });
}
} // namespace

namespace ams_mel_common {
CommonChannelAccess common_from_stacked(const ams_mel_ir_stacked *owner)
{
    return {owner->state,
        [](const std::shared_ptr<void>& value) noexcept {
            return std::static_pointer_cast<StackedState>(value)->session->admission;
        },
        [](const std::shared_ptr<void>& value, std::shared_ptr<irmel::Channel>& channel,
           CommonRequestClaim& claim) {
            auto state = std::static_pointer_cast<StackedState>(value);
            std::lock_guard lock{state->mutex};
            if ((state->lifecycle != Lifecycle::Attached &&
                 state->lifecycle != Lifecycle::Enabled) ||
                !state->channel || !state->stacked) return false;
            channel = state->channel;
            /* Stacked-only accounting domain. */
            ++state->requests;
            claim = CommonRequestClaim{state,
                [](const std::shared_ptr<void>& erased) noexcept {
                    auto claimed = std::static_pointer_cast<StackedState>(erased);
                    try { return finish_request(claimed); }
                    catch (...) { retain_failed(claimed); return false; }
                }, "deferred Stacked cleanup failed"};
            return true;
        },
        [](const std::shared_ptr<void>& erased, ams_mel_ir_channel_capability **output,
           char *out, std::size_t capacity, std::size_t *required) noexcept {
            return stacked_capability(std::static_pointer_cast<StackedState>(erased),
                                         output, out, capacity, required);
        },
        [](const std::shared_ptr<void>& erased, const std::shared_ptr<CommsMetadataState>& queue) {
            auto state = std::static_pointer_cast<StackedState>(erased);
            std::lock_guard lock{state->mutex};
            if (state->comms_metadata_attempted ||
                (state->lifecycle != Lifecycle::Attached && state->lifecycle != Lifecycle::Enabled))
                return false;
            state->comms_metadata_attempted = true;
            state->comms_metadata = queue;
            return true;
        },
        [](const std::shared_ptr<void>& erased, ams_mel_ir_return_t *result,
           char *out, std::size_t capacity, std::size_t *required) noexcept {
            auto state = std::static_pointer_cast<StackedState>(erased);
            const auto status = disable_attached(state, state->mutex, state->lifecycle,
                Lifecycle::Attached, Lifecycle::Enabled, Lifecycle::Disabling,
                Lifecycle::Failed, true, state->enable_attempted, state->disable_failed,
                state->requests, state->channel,
                [](const std::shared_ptr<StackedState>& claimed) noexcept {
                    try { return finish_request(claimed); }
                    catch (...) { retain_failed(claimed); return false; }
                }, retain_failed, result, &state->application_buffers.operation);
            diagnostic(status == AMS_MEL_OK ? "" : "Stacked Disable failed or unavailable",
                       out, capacity, required);
            return status;
        },
        [](const std::shared_ptr<void>& erased, ams_mel_ir_buffer& buffer,
           bool removing, ams_mel_ir_return_t *result) noexcept {
            auto state = std::static_pointer_cast<StackedState>(erased);
            return buffer_registration(state, state->mutex, state->lifecycle,
                Lifecycle::Attached, true, state->application_buffers, state->channel,
                buffer, removing, result, reconcile_application_buffer, [] { return true; });
        }};
}
} // namespace ams_mel_common

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
/* Test-facade-only observation of the Stacked request accounting. The
 * observer holds only a weak reference, so it also proves that no hidden
 * strong owner (for example a weak Channel view) outlives cleanup. */
struct ams_mel_test_stacked_observer { std::weak_ptr<StackedState> state; };
extern "C" __attribute__((visibility("default"))) int ams_mel_test_stacked_requests(
    const ams_mel_ir_stacked *owner, std::size_t *requests) noexcept
{
    try {
        if (!owner || !owner->state || !requests) return 0;
        std::lock_guard lock{owner->state->mutex};
        *requests = owner->state->requests;
        return 1;
    } catch (...) { return 0; }
}
extern "C" __attribute__((visibility("default"))) int ams_mel_test_stacked_observer_from(
    const ams_mel_ir_stacked *owner, ams_mel_test_stacked_observer **output) noexcept
{
    if (!owner || !output || *output) return 0;
    try { *output = new ams_mel_test_stacked_observer{owner->state}; return 1; }
    catch (...) { return 0; }
}
/* Returns 1 with *alive = 0/1 and, when alive, the Stacked request count. */
extern "C" __attribute__((visibility("default"))) int ams_mel_test_stacked_observer_state(
    const ams_mel_test_stacked_observer *observer, int *alive, std::size_t *requests) noexcept
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
extern "C" __attribute__((visibility("default"))) void ams_mel_test_stacked_observer_close(
    ams_mel_test_stacked_observer **observer) noexcept
{ if (observer) { delete *observer; *observer = nullptr; } }
#endif
extern "C" ams_mel_status_t ams_mel_ir_stacked_open(
    const ams_mel_session *session, const ams_mel_ir_stacked_config_v1 *config,
    ams_mel_ir_stacked **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!session || !session->state || !config || !output || *output ||
        (!out && capacity) ||
        config->channel_type != AMS_MEL_IR_CHANNEL_STACKED_IMAGE ||
        !valid_view(config->channel_id.descriptive_label) ||
        !valid_view(config->platform_id.descriptive_label) ||
        !valid_view(config->sensor_location.key) ||
        !valid_view(config->sensor_location.system_name)) {
        diagnostic("invalid Stacked configuration", out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    std::shared_ptr<StackedState> state;
    try {
        /* Required channel discovery and acquisition support are independent. A
         * provider that does not publish it is an expected unsupported path,
         * reported before any attachment; the Session remains usable. */
        bool advertised = false;
        for (const auto& capability : session->state->control->getCapabilities())
            if (has_stacked(capability)) { advertised = true; break; }
        if (!advertised) {
            diagnostic("provider does not advertise Stacked", out, capacity, required);
            return AMS_MEL_INITIALIZATION_FAILED;
        }
        state = std::make_shared<StackedState>();
        state->session = session->state;
        mel::ForeignKey key{copy_view(config->sensor_location.key),
                            copy_view(config->sensor_location.system_name)};
        mel::ComponentLocation location{config->sensor_location.offset_x_m,
            config->sensor_location.offset_y_m, config->sensor_location.offset_z_m, key};
        irmel::Config upstream{convert_id(config->channel_id),
            irmel::ChannelType::StackedImage, convert_id(config->platform_id),
            std::move(location), {}, false, false};
        /* Exactly one attachment attempt. */
        state->channel = state->session->control->attachChannel(upstream);
        if (!state->channel) {
            diagnostic("attachChannel returned null", out, capacity, required);
            return AMS_MEL_FACTORY_FAILED;
        }
        state->stacked =
            std::dynamic_pointer_cast<irmel::StackedImageChannel>(state->channel);
        /* An ImageChannel can advertise the same category without implementing
         * StackedImageChannel. The dynamic cast is a separate mandatory gate. */
        bool compatible = static_cast<bool>(state->stacked);
        const char *reason = "attached channel is not a StackedImageChannel";
        if (compatible) {
            compatible = has_stacked(state->channel->getCapabilities());
            if (!compatible) reason = "Stacked channel capability omits Stacked";
        }
        if (!compatible) {
            const bool detached = detach(state, state->channel);
            if (!detached) retain_failed(state);
            diagnostic(detached ? reason : "incompatible Stacked channel and detach failed",
                       out, capacity, required);
            return detached ? AMS_MEL_INITIALIZATION_FAILED : AMS_MEL_PROVIDER_FAILED;
        }
        /* Publish the owner only after every validation succeeded. */
        auto owner = std::make_unique<ams_mel_ir_stacked>();
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
                diagnostic("provider exception during Stacked open and detach failed",
                           out, capacity, required);
                return AMS_MEL_PROVIDER_FAILED;
            }
            diagnostic("provider exception during Stacked open", out, capacity, required);
            return AMS_MEL_INITIALIZATION_FAILED;
        }
        diagnostic("provider exception during Stacked open", out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_stacked_enable(
    ams_mel_ir_stacked *stacked, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!stacked || !stacked->state || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    auto state = stacked->state;
    std::shared_ptr<irmel::Channel> channel;
    {
        std::lock_guard lock{state->mutex};
        if (state->application_buffers.operation || state->application_buffers.uncertain)
                return AMS_MEL_PROVIDER_FAILED;
        if (state->lifecycle == Lifecycle::Enabled) return AMS_MEL_OK;
        if (state->lifecycle != Lifecycle::Attached) {
            diagnostic("Stacked channel is not attached", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        state->enable_attempted = true;
        state->lifecycle = Lifecycle::Enabling;
        ++state->requests;
        channel = state->channel;
    }
    /* Reserve an operation before leaving the lock: synchronous reentry may
     * logically close the owner, but cannot detach during this provider call. */
    ams_mel_status_t status = AMS_MEL_OK;
    try {
        if (channel->enable() != irmel::Return::Success) {
            diagnostic("Stacked enable failed", out, capacity, required);
            status = AMS_MEL_PROVIDER_FAILED;
        }
    } catch (...) {
        diagnostic("provider exception during Stacked enable", out, capacity, required);
        status = AMS_MEL_PROVIDER_EXCEPTION;
    }
    {
        std::lock_guard lock{state->mutex};
        if (state->lifecycle == Lifecycle::Enabling)
            state->lifecycle = status == AMS_MEL_OK ? Lifecycle::Enabled : Lifecycle::Failed;
    }
    channel.reset();
    if (!finish_request(state)) {
        diagnostic("deferred Stacked cleanup failed", out, capacity, required);
        return AMS_MEL_PROVIDER_FAILED;
    }
    return status;
}
extern "C" ams_mel_status_t ams_mel_ir_stacked_get_capabilities(
    ams_mel_ir_stacked *stacked, ams_mel_ir_channel_capability **output, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!stacked || !stacked->state || !output || *output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    /* A synchronous provider reentry can consume the public owner. Never lend
     * a reference to its shared_ptr member across that provider boundary. */
    auto state = stacked->state;
    return stacked_capability(state, output, out, capacity, required);
}

extern "C" ams_mel_status_t ams_mel_ir_stacked_close(
    ams_mel_ir_stacked **stacked, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!stacked || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    auto *owner = *stacked;
    if (!owner) return AMS_MEL_OK;
    try {
        auto state = owner->state;
        bool now = false;
        {
            std::lock_guard lock{state->mutex};
            if (state->application_buffers.count) return AMS_MEL_PROVIDER_FAILED;
            /* New common requests are refused from here on; admitted ones
             * keep their claim and own the graph until completion. */
            state->lifecycle = Lifecycle::Closed;
            if (state->comms_metadata) state->comms_metadata->deactivate();
            now = state->requests == 0U;
        }

        const bool ok = !now || cleanup(state, false);
        bool attached;
        { std::lock_guard lock{state->mutex}; attached = static_cast<bool>(state->channel); }
        if (now && attached) {
            /* Detach could not be proven: the graph and the caller's owner are
             * retained so a later Close can retry. */
            diagnostic("Stacked detach failed; provider state retained",
                       out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        *stacked = nullptr;
        delete owner;
        if (!ok) {
            diagnostic("Stacked disable failed", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        return AMS_MEL_OK;
    } catch (...) {
        diagnostic("Stacked close failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}
