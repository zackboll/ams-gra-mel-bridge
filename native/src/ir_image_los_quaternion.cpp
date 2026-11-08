#include <ams_mel/abi.h>
#include "internal/ir_image_los_quaternion.hpp"
#include "internal/ir_nuc.hpp"
#include "internal/ir_stream.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <type_traits>
#include <utility>

/* IRMEL-ADA-010: opt-in @RequiredIfLOSQuaternion ImageChannel
 * LineOfSightQuaternion owner. Independent of ams_mel_ir_image_metadata_open
 * and of the NUC_TempData, CameraCommandResp and OpticalDistortionMap owners:
 * none of their one-shots affects this one and none of them registers this
 * overload. */

namespace {
using namespace ams::iface;
using ams_mel::internal::nuc_diagnostic;

/* Upstream getter types this copy relies on. Getters only, never layouts. */
using Los = irmel::LineOfSightQuaternion;
static_assert(std::is_same_v<decltype(std::declval<Los>().getSystemTime()), std::chrono::nanoseconds>);
static_assert(std::is_same_v<decltype(std::declval<Los>().getQ_xyzw()), const irmel::Quaternion&>);
static_assert(std::is_same_v<decltype(std::declval<Los>().getPqr_rps()),
                             const irmel::ForwardRightDown&>);
static_assert(std::is_same_v<decltype(std::declval<irmel::Quaternion>().getQuaternionX()), double>);
static_assert(std::is_same_v<decltype(std::declval<irmel::Quaternion>().getQuaternionY()), double>);
static_assert(std::is_same_v<decltype(std::declval<irmel::Quaternion>().getQuaternionZ()), double>);
static_assert(std::is_same_v<decltype(std::declval<irmel::Quaternion>().getQuaternionW()), double>);
static_assert(std::is_same_v<decltype(std::declval<irmel::ForwardRightDown>().getForward()), double>);
static_assert(std::is_same_v<decltype(std::declval<irmel::ForwardRightDown>().getRight()), double>);
static_assert(std::is_same_v<decltype(std::declval<irmel::ForwardRightDown>().getDown()), double>);
/* Every chrono::nanoseconds count is representable in the signed 64-bit C
 * field without narrowing, so INT64_MIN..INT64_MAX survive exactly. */
using Rep = std::chrono::nanoseconds::rep;
static_assert(std::is_integral_v<Rep> && std::is_signed_v<Rep>, "signed nanosecond count");
static_assert(std::numeric_limits<Rep>::min() >= std::numeric_limits<std::int64_t>::min() &&
              std::numeric_limits<Rep>::max() <= std::numeric_limits<std::int64_t>::max(),
              "nanosecond count fits int64_t");
static_assert(std::is_same_v<decltype(ams_mel_ir_line_of_sight_quaternion_v1::system_time_ns),
                             std::int64_t>);
static_assert(std::is_same_v<decltype(ams_mel_ir_line_of_sight_quaternion_v1::q_xyzw),
                             ams_mel_ir_quaternion_v1>, "canonical quaternion record reused");
static_assert(std::is_same_v<decltype(ams_mel_ir_line_of_sight_quaternion_v1::pqr_rps),
                             ams_mel_ir_forward_right_down_v1>, "canonical FRD record reused");
static_assert(std::numeric_limits<double>::is_iec559, "IEEE doubles are forwarded unchanged");

void increment(std::uint64_t& value) noexcept
{
    if (value != std::numeric_limits<std::uint64_t>::max()) ++value;
}

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
/* Test-only deterministic observation and failure injection, compiled out of
 * production builds. Lock order is always LosQuaternionState::mutex, then
 * probe_mutex. */
std::mutex probe_mutex;
std::condition_variable probe_ready;
bool park_next{}, parked_entered{}, drain_waiting{}, receive_waiting{};
std::atomic<int> copy_attempts{0};
std::atomic<bool> copy_failure_armed{false};

void observe(bool& flag) noexcept
{
    try {
        std::lock_guard probe{probe_mutex};
        flag = true;
        probe_ready.notify_all();
    } catch (...) {
    }
}
#endif
} // namespace

namespace ams_mel::internal {
std::unique_ptr<LosQuaternionEventData> copy_line_of_sight_quaternion(
    const irmel::LineOfSightQuaternion& input)
{
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    copy_attempts.fetch_add(1);
    if (copy_failure_armed.exchange(false)) throw std::bad_alloc{};
#endif
    auto event = std::make_unique<LosQuaternionEventData>();
    auto& view = event->view;
    /* Signed count copied as-is: no epoch, clock or unit conversion. */
    view.system_time_ns = static_cast<std::int64_t>(input.getSystemTime().count());
    /* Plain IEEE assignment in upstream x, y, z, w order: no normalization,
     * sign canonicalization, reordering or Euler conversion. */
    const auto& q = input.getQ_xyzw();
    view.q_xyzw.x = q.getQuaternionX();
    view.q_xyzw.y = q.getQuaternionY();
    view.q_xyzw.z = q.getQuaternionZ();
    view.q_xyzw.w = q.getQuaternionW();
    /* Image-frame rotation rates (rad/s), forward/right/down, unclamped. */
    const auto& rates = input.getPqr_rps();
    view.pqr_rps.forward = rates.getForward();
    view.pqr_rps.right = rates.getRight();
    view.pqr_rps.down = rates.getDown();
    return event;
}
} // namespace ams_mel::internal

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
/* Test-facade-only hook (never in the production export map):
 *   0 park the next bridge callback inside receive(), counted in flight and
 *     with LosQuaternionState::mutex released
 *   1 wait (bounded) until the parked callback has entered
 *   2 release the parked callback
 *   3 wait (bounded) until physical teardown waits for in-flight callbacks
 *   4 return the number of snapshot allocations attempted so far
 *   5 arm (value != 0) or disarm a one-shot snapshot allocation failure
 *   6 return 1 while that allocation failure is still armed
 *   7 wait (bounded) until a Receive blocks waiting for an event (consumes it)
 *   8 forget any earlier Receive wait observation
 * Returns 1 on success/true and 0 on timeout/false or an unknown operation. */
extern "C" __attribute__((visibility("default"))) int ams_mel_test_image_los_quaternion_probe(
    unsigned operation, std::size_t value)
{
    const auto bounded = std::chrono::seconds{15};
    switch (operation) {
    case 0U: {
        std::lock_guard probe{probe_mutex};
        park_next = true; parked_entered = false; drain_waiting = false;
        return 1;
    }
    case 1U: {
        std::unique_lock probe{probe_mutex};
        return probe_ready.wait_for(probe, bounded, [] { return parked_entered; }) ? 1 : 0;
    }
    case 2U: {
        std::lock_guard probe{probe_mutex};
        park_next = false;
        probe_ready.notify_all();
        return 1;
    }
    case 3U: {
        std::unique_lock probe{probe_mutex};
        return probe_ready.wait_for(probe, bounded, [] { return drain_waiting; }) ? 1 : 0;
    }
    case 4U: return copy_attempts.load();
    case 5U: copy_failure_armed.store(value != 0U); return 1;
    case 6U: return copy_failure_armed.load() ? 1 : 0;
    case 7U: {
        std::unique_lock probe{probe_mutex};
        return probe_ready.wait_for(probe, bounded, [] {
            const bool seen = receive_waiting; receive_waiting = false; return seen;
        }) ? 1 : 0;
    }
    case 8U: {
        std::lock_guard probe{probe_mutex};
        receive_waiting = false;
        return 1;
    }
    default: return 0;
    }
}
#endif

void LosQuaternionState::receive(const irmel::LineOfSightQuaternion *input) noexcept
{
    enum class Outcome { Ignored, Malformed, Full, Copied };
    bool counted = false;
    try {
        bool active;
        bool full;
        {
            std::unique_lock lock{mutex};
            ++callbacks;
            counted = true;
            increment(counters.events_received);
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
            {
                std::unique_lock probe{probe_mutex};
                if (park_next) {
                    parked_entered = true;
                    probe_ready.notify_all();
                    /* Counted in flight with the state mutex released, so
                     * public Close, Stop and physical teardown can proceed. */
                    lock.unlock();
                    probe_ready.wait(probe, [] { return !park_next; });
                    probe.unlock();
                    lock.lock();
                }
            }
#endif
            active = lifecycle == LosQuaternionLifecycle::Active;
            full = queue.size() >= capacity;
        }
        /* The copy runs without the state mutex, but strictly before this
         * callback returns: the provider payload is never read afterwards. */
        Outcome outcome = Outcome::Ignored;
        std::unique_ptr<LosQuaternionEventData> event;
        if (active) {
            if (!input)
                /* A null payload is malformed even while the queue is full. */
                outcome = Outcome::Malformed;
            else if (full)
                /* Full-queue optimization: never allocate a snapshot that
                 * would only be dropped. */
                outcome = Outcome::Full;
            else {
                event = ams_mel::internal::copy_line_of_sight_quaternion(*input);
                outcome = Outcome::Copied;
            }
        }
        std::lock_guard lock{mutex};
        if (outcome != Outcome::Ignored && lifecycle == LosQuaternionLifecycle::Active) {
            if (outcome == Outcome::Malformed)
                increment(counters.malformed_or_unsupported);
            else if (outcome == Outcome::Full || queue.size() >= capacity)
                increment(counters.events_dropped_queue_full);
            else {
                queue.push_back(std::move(event));
                ready.notify_one();
            }
        }
        if (--callbacks == 0U) callbacks_done.notify_all();
    } catch (...) {
        /* No exception crosses the provider callback boundary. A snapshot
         * allocation failure rejects this whole event only; nothing partial
         * was published. */
        try {
            std::lock_guard lock{mutex};
            increment(counters.malformed_or_unsupported);
            if (counted && --callbacks == 0U) callbacks_done.notify_all();
        } catch (...) {
        }
    }
}

void LosQuaternionState::deactivate() noexcept
{
    try {
        std::lock_guard lock{mutex};
        if (lifecycle == LosQuaternionLifecycle::Active) lifecycle = LosQuaternionLifecycle::Inactive;
        ready.notify_all();
    } catch (...) {
    }
}

void LosQuaternionState::stop() noexcept
{
    try {
        std::lock_guard lock{mutex};
        if (lifecycle == LosQuaternionLifecycle::Active) lifecycle = LosQuaternionLifecycle::Stopped;
        ready.notify_all();
    } catch (...) {
    }
}

void LosQuaternionState::fail() noexcept
{
    try {
        std::lock_guard lock{mutex};
        if (lifecycle == LosQuaternionLifecycle::Active ||
            lifecycle == LosQuaternionLifecycle::Stopped)
            lifecycle = LosQuaternionLifecycle::Failed;
        ready.notify_all();
    } catch (...) {
    }
}

void LosQuaternionState::drain_and_stop() noexcept
{
    try {
        std::unique_lock lock{mutex};
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
        if (callbacks != 0U) observe(drain_waiting);
#endif
        callbacks_done.wait(lock, [this] { return callbacks == 0U; });
        if (lifecycle == LosQuaternionLifecycle::Active) lifecycle = LosQuaternionLifecycle::Stopped;
        ready.notify_all();
    } catch (...) {
    }
}

/* Image stream lifecycle hooks; never called with CallbackState::mutex held. */
void image_los_quaternion_stream_stopped(
    const std::shared_ptr<LosQuaternionState>& state) noexcept
{
    if (state) state->stop();
}

void image_los_quaternion_stream_drained(
    const std::shared_ptr<LosQuaternionState>& state) noexcept
{
    if (state) state->drain_and_stop();
}

void image_los_quaternion_stream_failed(
    const std::shared_ptr<LosQuaternionState>& state) noexcept
{
    if (state) state->fail();
}

extern "C" ams_mel_status_t ams_mel_ir_image_los_quaternion_open(
    ams_mel_ir_stream *stream, std::size_t queue_capacity,
    ams_mel_ir_image_los_quaternion_metadata **output, ams_mel_ir_return_t *registration,
    char *out, std::size_t capacity, std::size_t *required) noexcept
{
    nuc_diagnostic("", out, capacity, required);
    if (!stream || !stream->state || !queue_capacity || !output || *output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    std::shared_ptr<LosQuaternionState> state;
    std::function<void(irmel::Channel&, const irmel::LineOfSightQuaternion *const)> callback;
    std::unique_ptr<ams_mel_ir_image_los_quaternion_metadata> owner;
    try {
        state = std::make_shared<LosQuaternionState>();
        state->capacity = queue_capacity;
        callback = [state](irmel::Channel&,
                           const irmel::LineOfSightQuaternion *const value) noexcept {
            /* Keep the state alive for the whole call even if the provider
             * destroys its stored closure concurrently. */
            const auto keep = state;
            keep->receive(value);
        };
        owner = std::make_unique<ams_mel_ir_image_los_quaternion_metadata>();
        owner->state = state;
    } catch (...) {
        nuc_diagnostic("LineOfSightQuaternion metadata allocation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    std::shared_ptr<irmel::ImageChannel> channel;
    if (!claim_image_los_quaternion(*stream->state, state, channel)) {
        nuc_diagnostic("Image stream is not eligible or LineOfSightQuaternion was already attempted",
                       out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    irmel::Return result{};
    try {
        /* No bridge lock is held: synchronous delivery during registration is
         * safe. The one-shot was already recorded by the claim. */
        result = channel->registerMetadataCallback(std::move(callback));
    } catch (...) {
        state->deactivate();
        nuc_diagnostic("provider exception during LineOfSightQuaternion registration", out,
                       capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
    ams_mel_ir_return_t mapped = AMS_MEL_IR_RETURN_FAIL;
    const bool known = ams_mel::internal::nuc_map_return(result, mapped);
    if (known && registration) *registration = mapped;
    if (!known || mapped != AMS_MEL_IR_RETURN_SUCCESS) {
        state->deactivate();
        nuc_diagnostic(!known ? "provider returned an unknown LineOfSightQuaternion registration result" :
                       mapped == AMS_MEL_IR_RETURN_NOT_SUPPORTED ?
                           "provider does not support LineOfSightQuaternion callback" :
                           "LineOfSightQuaternion callback registration failed",
                       out, capacity, required);
        return AMS_MEL_PROVIDER_FAILED;
    }
    *output = owner.release();
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_image_los_quaternion_receive(
    ams_mel_ir_image_los_quaternion_metadata *metadata, std::uint32_t timeout_ms,
    ams_mel_ir_image_los_quaternion_event **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    nuc_diagnostic("", out, capacity, required);
    if (!metadata || !metadata->state || !output || *output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        auto& state = *metadata->state;
        auto owner = std::make_unique<ams_mel_ir_image_los_quaternion_event>();
        std::unique_lock lock{state.mutex};
        if (state.queue.empty() && state.lifecycle == LosQuaternionLifecycle::Active &&
            timeout_ms) {
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
            observe(receive_waiting);
#endif
            state.ready.wait_for(lock, std::chrono::milliseconds{timeout_ms}, [&] {
                return !state.queue.empty() || state.lifecycle != LosQuaternionLifecycle::Active;
            });
        }
        if (!state.queue.empty()) {
            owner->data = std::move(state.queue.front());
            state.queue.pop_front();
            *output = owner.release();
            return AMS_MEL_OK;
        }
        if (state.lifecycle == LosQuaternionLifecycle::Failed) {
            nuc_diagnostic("Image channel cleanup failed; LineOfSightQuaternion owner failed", out,
                           capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        return state.lifecycle == LosQuaternionLifecycle::Active ? AMS_MEL_TIMEOUT
                                                                 : AMS_MEL_STREAM_STOPPED;
    } catch (...) {
        nuc_diagnostic("LineOfSightQuaternion metadata receive failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_image_los_quaternion_get_counters(
    const ams_mel_ir_image_los_quaternion_metadata *metadata,
    ams_mel_ir_metadata_counters_v1 *output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    nuc_diagnostic("", out, capacity, required);
    if (!metadata || !metadata->state || !output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::lock_guard lock{metadata->state->mutex};
        *output = metadata->state->counters;
        return AMS_MEL_OK;
    } catch (...) {
        nuc_diagnostic("LineOfSightQuaternion counter query failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_image_los_quaternion_close(
    ams_mel_ir_image_los_quaternion_metadata **metadata, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    nuc_diagnostic("", out, capacity, required);
    if (!metadata || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    auto *owner = std::exchange(*metadata, nullptr);
    /* Deactivates consumption only: the provider keeps the registered callback
     * (upstream has no unregister) and later callbacks never enqueue. */
    if (owner && owner->state) owner->state->deactivate();
    delete owner;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_image_los_quaternion_event_view(
    const ams_mel_ir_image_los_quaternion_event *event,
    const ams_mel_ir_line_of_sight_quaternion_v1 **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    nuc_diagnostic("", out, capacity, required);
    if (!event || !event->data || !output || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    *output = &event->data->view;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_image_los_quaternion_event_close(
    ams_mel_ir_image_los_quaternion_event **event, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    nuc_diagnostic("", out, capacity, required);
    if (!event || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    delete std::exchange(*event, nullptr);
    return AMS_MEL_OK;
}
