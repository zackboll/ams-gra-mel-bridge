#include <ams_mel/abi.h>
#include "internal/ir_image_odm.hpp"
#include "internal/ir_nuc.hpp"
#include "internal/ir_stream.hpp"

#include <algorithm>
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
#include <vector>

/* IRMEL-ADA-009: opt-in @Optional ImageChannel OpticalDistortionMap owner.
 * Independent of ams_mel_ir_image_metadata_open, the NUC_TempData owner and
 * the CameraCommandResp owner: none of their one-shots affects this one and
 * none of them registers this overload. */

namespace {
using namespace ams::iface;
using ams_mel::internal::nuc_diagnostic;

/* Upstream getter types this copy relies on. Getters only, never layouts. */
using Map = irmel::OpticalDistortionMap;
static_assert(std::is_same_v<decltype(std::declval<Map>().getWidth()), std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<Map>().getHeight()), std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<Map>().getRightSize()), std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<Map>().getDownSize()), std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<Map>().getPixelsPerOdm()), std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<Map>().getRightMin()), double>);
static_assert(std::is_same_v<decltype(std::declval<Map>().getRightMax()), double>);
static_assert(std::is_same_v<decltype(std::declval<Map>().getDownMin()), double>);
static_assert(std::is_same_v<decltype(std::declval<Map>().getDownMax()), double>);
static_assert(std::is_same_v<decltype(std::declval<Map>().getFrd()),
                             const std::vector<irmel::ForwardRightDown>&>);
static_assert(std::is_same_v<decltype(std::declval<Map>().getRowCol()),
                             const std::vector<irmel::RowCol>&>);
static_assert(std::is_same_v<decltype(std::declval<irmel::ForwardRightDown>().getForward()), double>);
static_assert(std::is_same_v<decltype(std::declval<irmel::ForwardRightDown>().getRight()), double>);
static_assert(std::is_same_v<decltype(std::declval<irmel::ForwardRightDown>().getDown()), double>);
static_assert(std::is_same_v<decltype(std::declval<irmel::RowCol>().getRow()), double>);
static_assert(std::is_same_v<decltype(std::declval<irmel::RowCol>().getCol()), double>);
static_assert(std::numeric_limits<double>::is_iec559, "IEEE doubles are forwarded unchanged");

void increment(std::uint64_t& value) noexcept
{
    if (value != std::numeric_limits<std::uint64_t>::max()) ++value;
}

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
/* Test-only deterministic observation and failure injection, compiled out of
 * production builds. Lock order is always OdmState::mutex, then probe_mutex. */
std::mutex probe_mutex;
std::condition_variable probe_ready;
bool park_next{}, parked_entered{}, drain_waiting{}, receive_waiting{};
std::atomic<std::size_t> count_limit_override{0U};
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

std::size_t count_limit() noexcept
{
    constexpr auto limit = static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max());
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    /* INT32_MAX + 1 elements cannot be allocated in a test, so the boundary is
     * exercised through a test-only lowered limit using the same comparison. */
    if (const auto value = count_limit_override.load(); value != 0U) return std::min(value, limit);
#endif
    return limit;
}

/* Count bounded by INT32_MAX (Ada Natural span traversal) and checked element
 * byte size, so neither native nor Ada pointer arithmetic can overflow. */
template<class Element> bool checked_count(std::size_t count) noexcept
{
    return count <= count_limit() &&
           count <= static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()) /
                        sizeof(Element);
}
} // namespace

namespace ams_mel::internal {
bool valid_optical_distortion_map(const irmel::OpticalDistortionMap& input) noexcept
{
    /* Allocation-free. Each collection is checked independently; equal
     * lengths and products of the scalar dimensions are deliberately NOT
     * required, because the pinned upstream API establishes neither. */
    return checked_count<ams_mel_ir_forward_right_down_v1>(input.getFrd().size()) &&
           checked_count<ams_mel_ir_row_col_v1>(input.getRowCol().size());
}

std::unique_ptr<OdmEventData> copy_optical_distortion_map(const irmel::OpticalDistortionMap& input)
{
    if (!valid_optical_distortion_map(input)) return {};
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    copy_attempts.fetch_add(1);
    if (copy_failure_armed.exchange(false)) throw std::bad_alloc{};
#endif
    const auto& frd = input.getFrd();
    const auto& row_col = input.getRowCol();
    auto event = std::make_unique<OdmEventData>();
    /* Both vectors reach their final size before any view pointer is bound,
     * so a bad_alloc anywhere here publishes nothing. */
    event->frd.reserve(frd.size());
    for (const auto& value : frd)
        event->frd.push_back({value.getForward(), value.getRight(), value.getDown()});
    event->row_col.reserve(row_col.size());
    for (const auto& value : row_col)
        event->row_col.push_back({value.getRow(), value.getCol()});
    auto& view = event->view;
    view.width = input.getWidth();
    view.height = input.getHeight();
    view.right_size = input.getRightSize();
    view.down_size = input.getDownSize();
    view.pixels_per_odm = input.getPixelsPerOdm();
    /* Plain IEEE assignment: signed zero, extremes, infinities and NaN are
     * forwarded unchanged; nothing is clamped, normalized or replaced. */
    view.right_min = input.getRightMin();
    view.right_max = input.getRightMax();
    view.down_min = input.getDownMin();
    view.down_max = input.getDownMax();
    view.frd = {event->frd.empty() ? nullptr : event->frd.data(), event->frd.size()};
    view.row_col = {event->row_col.empty() ? nullptr : event->row_col.data(),
                    event->row_col.size()};
    return event;
}
} // namespace ams_mel::internal

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
/* Test-facade-only hook (never in the production export map):
 *   0 park the next bridge callback inside receive(), counted in flight and
 *     with OdmState::mutex released
 *   1 wait (bounded) until the parked callback has entered
 *   2 release the parked callback
 *   3 wait (bounded) until physical teardown waits for in-flight callbacks
 *   4 lower the per-vector element-count limit to value (0 restores INT32_MAX)
 *   5 return the number of snapshot copies attempted so far
 *   6 arm (value != 0) or disarm a one-shot snapshot-copy allocation failure
 *   7 return 1 while that copy allocation failure is still armed
 *   8 wait (bounded) until a Receive blocks waiting for an event (consumes it)
 *   9 forget any earlier Receive wait observation
 * Returns 1 on success/true and 0 on timeout/false or an unknown operation. */
extern "C" __attribute__((visibility("default"))) int ams_mel_test_image_odm_probe(
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
    case 4U: count_limit_override.store(value); return 1;
    case 5U: return copy_attempts.load();
    case 6U: copy_failure_armed.store(value != 0U); return 1;
    case 7U: return copy_failure_armed.load() ? 1 : 0;
    case 8U: {
        std::unique_lock probe{probe_mutex};
        return probe_ready.wait_for(probe, bounded, [] {
            const bool seen = receive_waiting; receive_waiting = false; return seen;
        }) ? 1 : 0;
    }
    case 9U: {
        std::lock_guard probe{probe_mutex};
        receive_waiting = false;
        return 1;
    }
    default: return 0;
    }
}
#endif

void OdmState::receive(const irmel::OpticalDistortionMap *input) noexcept
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
            active = lifecycle == OdmLifecycle::Active;
            full = queue.size() >= capacity;
        }
        /* Classification and the deep copy run without the state mutex, so a
         * large map never blocks Receive, Close or counter queries. */
        Outcome outcome = Outcome::Ignored;
        std::unique_ptr<OdmEventData> event;
        if (active) {
            if (!input)
                outcome = Outcome::Malformed;
            else if (full)
                /* Full-queue optimization: validate without allocating and
                 * never construct a snapshot that would be dropped. Malformed
                 * data is still classified malformed, never queue-full. */
                outcome = ams_mel::internal::valid_optical_distortion_map(*input) ?
                              Outcome::Full : Outcome::Malformed;
            else {
                event = ams_mel::internal::copy_optical_distortion_map(*input);
                outcome = event ? Outcome::Copied : Outcome::Malformed;
            }
        }
        std::lock_guard lock{mutex};
        if (outcome != Outcome::Ignored && lifecycle == OdmLifecycle::Active) {
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
        /* No exception crosses the provider callback boundary. A copy
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

void OdmState::deactivate() noexcept
{
    try {
        std::lock_guard lock{mutex};
        if (lifecycle == OdmLifecycle::Active) lifecycle = OdmLifecycle::Inactive;
        ready.notify_all();
    } catch (...) {
    }
}

void OdmState::stop() noexcept
{
    try {
        std::lock_guard lock{mutex};
        if (lifecycle == OdmLifecycle::Active) lifecycle = OdmLifecycle::Stopped;
        ready.notify_all();
    } catch (...) {
    }
}

void OdmState::fail() noexcept
{
    try {
        std::lock_guard lock{mutex};
        lifecycle = OdmLifecycle::Failed;
        ready.notify_all();
    } catch (...) {
    }
}

void OdmState::drain_and_stop() noexcept
{
    try {
        std::unique_lock lock{mutex};
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
        if (callbacks != 0U) observe(drain_waiting);
#endif
        callbacks_done.wait(lock, [this] { return callbacks == 0U; });
        if (lifecycle == OdmLifecycle::Active) lifecycle = OdmLifecycle::Stopped;
        ready.notify_all();
    } catch (...) {
    }
}

/* Image stream lifecycle hooks; never called with CallbackState::mutex held. */
void image_odm_stream_stopped(const std::shared_ptr<OdmState>& state) noexcept
{
    if (state) state->stop();
}

void image_odm_stream_drained(const std::shared_ptr<OdmState>& state) noexcept
{
    if (state) state->drain_and_stop();
}

void image_odm_stream_failed(const std::shared_ptr<OdmState>& state) noexcept
{
    if (state) state->fail();
}

extern "C" ams_mel_status_t ams_mel_ir_image_odm_open(
    ams_mel_ir_stream *stream, std::size_t queue_capacity,
    ams_mel_ir_image_odm_metadata **output, ams_mel_ir_return_t *registration,
    char *out, std::size_t capacity, std::size_t *required) noexcept
{
    nuc_diagnostic("", out, capacity, required);
    if (!stream || !stream->state || !queue_capacity || !output || *output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    std::shared_ptr<OdmState> state;
    std::function<void(irmel::Channel&, const irmel::OpticalDistortionMap *const)> callback;
    std::unique_ptr<ams_mel_ir_image_odm_metadata> owner;
    try {
        state = std::make_shared<OdmState>();
        state->capacity = queue_capacity;
        callback = [state](irmel::Channel&, const irmel::OpticalDistortionMap *const value) noexcept {
            /* Keep the state alive for the whole call even if the provider
             * destroys its stored closure concurrently. */
            const auto keep = state;
            keep->receive(value);
        };
        owner = std::make_unique<ams_mel_ir_image_odm_metadata>();
        owner->state = state;
    } catch (...) {
        nuc_diagnostic("OpticalDistortionMap metadata allocation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    std::shared_ptr<irmel::ImageChannel> channel;
    if (!claim_image_odm(*stream->state, state, channel)) {
        nuc_diagnostic("Image stream is not eligible or OpticalDistortionMap was already attempted",
                       out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    irmel::Return result{};
    try {
        /* No bridge lock is held: synchronous delivery during registration is
         * safe. The one-shot was already recorded by claim_image_odm. */
        result = channel->registerMetadataCallback(std::move(callback));
    } catch (...) {
        state->deactivate();
        nuc_diagnostic("provider exception during OpticalDistortionMap registration", out,
                       capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
    ams_mel_ir_return_t mapped = AMS_MEL_IR_RETURN_FAIL;
    const bool known = ams_mel::internal::nuc_map_return(result, mapped);
    if (known && registration) *registration = mapped;
    if (!known || mapped != AMS_MEL_IR_RETURN_SUCCESS) {
        state->deactivate();
        nuc_diagnostic(!known ? "provider returned an unknown OpticalDistortionMap registration result" :
                       mapped == AMS_MEL_IR_RETURN_NOT_SUPPORTED ?
                           "provider does not support optional OpticalDistortionMap callback" :
                           "OpticalDistortionMap callback registration failed",
                       out, capacity, required);
        return AMS_MEL_PROVIDER_FAILED;
    }
    *output = owner.release();
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_image_odm_receive(
    ams_mel_ir_image_odm_metadata *metadata, std::uint32_t timeout_ms,
    ams_mel_ir_image_odm_event **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    nuc_diagnostic("", out, capacity, required);
    if (!metadata || !metadata->state || !output || *output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        auto& state = *metadata->state;
        auto owner = std::make_unique<ams_mel_ir_image_odm_event>();
        std::unique_lock lock{state.mutex};
        if (state.queue.empty() && state.lifecycle == OdmLifecycle::Active && timeout_ms) {
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
            observe(receive_waiting);
#endif
            state.ready.wait_for(lock, std::chrono::milliseconds{timeout_ms}, [&] {
                return !state.queue.empty() || state.lifecycle != OdmLifecycle::Active;
            });
        }
        if (!state.queue.empty()) {
            owner->data = std::move(state.queue.front());
            state.queue.pop_front();
            *output = owner.release();
            return AMS_MEL_OK;
        }
        if (state.lifecycle == OdmLifecycle::Failed) {
            nuc_diagnostic("Image channel cleanup failed; OpticalDistortionMap owner failed", out,
                           capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        return state.lifecycle == OdmLifecycle::Active ? AMS_MEL_TIMEOUT : AMS_MEL_STREAM_STOPPED;
    } catch (...) {
        nuc_diagnostic("OpticalDistortionMap metadata receive failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_image_odm_get_counters(
    const ams_mel_ir_image_odm_metadata *metadata, ams_mel_ir_metadata_counters_v1 *output,
    char *out, std::size_t capacity, std::size_t *required) noexcept
{
    nuc_diagnostic("", out, capacity, required);
    if (!metadata || !metadata->state || !output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::lock_guard lock{metadata->state->mutex};
        *output = metadata->state->counters;
        return AMS_MEL_OK;
    } catch (...) {
        nuc_diagnostic("OpticalDistortionMap counter query failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_image_odm_close(
    ams_mel_ir_image_odm_metadata **metadata, char *out, std::size_t capacity,
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

extern "C" ams_mel_status_t ams_mel_ir_image_odm_event_view(
    const ams_mel_ir_image_odm_event *event, const ams_mel_ir_optical_distortion_map_v1 **output,
    char *out, std::size_t capacity, std::size_t *required) noexcept
{
    nuc_diagnostic("", out, capacity, required);
    if (!event || !event->data || !output || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    *output = &event->data->view;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_image_odm_event_close(
    ams_mel_ir_image_odm_event **event, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    nuc_diagnostic("", out, capacity, required);
    if (!event || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    delete std::exchange(*event, nullptr);
    return AMS_MEL_OK;
}
