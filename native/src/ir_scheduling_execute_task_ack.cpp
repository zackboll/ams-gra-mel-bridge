#include <ams_mel/abi.h>
#include "internal/ir_scheduling_execute_task_ack.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <string_view>
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

/* IRMEL-ADA-012: opt-in @RequiredIfIRSTTrackTasking SchedulingChannel
 * ExecuteTaskAck owner. Independent of the Scheduling foundation: Open, Enable,
 * capabilities, Close and the inherited common Channel view never register
 * this overload, and this owner never calls any Scheduling exec or send. */

namespace {
using namespace ams::iface;
/* This owner needs only the pinned Scheduling include closure, not Image or
 * NUC payload declarations. Diagnostics here are fixed ASCII strings. */
void ack_diagnostic(std::string_view text, char *out, std::size_t capacity,
                    std::size_t *required) noexcept
{
    if (required) *required = text.size() + 1U;
    if (out && capacity) {
        const auto copied = std::min(text.size(), capacity - 1U);
        std::memcpy(out, text.data(), copied);
        out[copied] = '\0';
    }
}

bool ack_map_return(irmel::Return input, ams_mel_ir_return_t& value) noexcept
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

/* Upstream getter types this copy relies on. Getters only, never layouts. */
using Ack = irmel::ExecuteTaskAck;
static_assert(std::is_same_v<decltype(std::declval<const Ack&>().getCommandID()), std::uint32_t>,
              "pinned ExecuteTaskAck::getCommandID() returns std::uint32_t");
static_assert(std::is_same_v<decltype(std::declval<const Ack&>().getReqId()), std::uint32_t>,
              "pinned ExecuteTaskAck::getReqId() returns std::uint32_t");
/* Both C fields are full-width unsigned 32-bit: no narrowing, no sign change. */
static_assert(std::is_same_v<decltype(ams_mel_ir_execute_task_ack_v1::command_id), std::uint32_t>);
static_assert(std::is_same_v<decltype(ams_mel_ir_execute_task_ack_v1::request_id), std::uint32_t>);
static_assert(offsetof(ams_mel_ir_execute_task_ack_v1, command_id) == 0U, "command first");
static_assert(offsetof(ams_mel_ir_execute_task_ack_v1, request_id) == 4U, "request second");
static_assert(sizeof(ams_mel_ir_execute_task_ack_v1) == 8U, "fixed 8-byte record");
/* The existing published metadata capability constant names this callback. */
static_assert(static_cast<std::uint32_t>(irmel::ChannelMetadataCapabilityType::ExecuteTaskAck) ==
              AMS_MEL_IR_METADATA_EXECUTE_TASK_ACK);
static_assert(AMS_MEL_IR_METADATA_EXECUTE_TASK_ACK == 14U);

void increment(std::uint64_t& value) noexcept
{
    if (value != std::numeric_limits<std::uint64_t>::max()) ++value;
}

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
/* Test-only deterministic observation and failure injection, compiled out of
 * production builds. Lock order is always ExecuteTaskAckState::mutex, then
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
std::unique_ptr<ExecuteTaskAckEventData> copy_execute_task_ack(const irmel::ExecuteTaskAck& input)
{
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    copy_attempts.fetch_add(1);
    if (copy_failure_armed.exchange(false)) throw std::bad_alloc{};
#endif
    auto event = std::make_unique<ExecuteTaskAckEventData>();
    /* Plain full-width assignment in upstream order. The identifiers are
     * never swapped, masked, correlated with a local request or validated as
     * nonzero, unique or equal to each other. */
    event->view.command_id = input.getCommandID();
    event->view.request_id = input.getReqId();
    return event;
}

void scheduling_execute_task_ack_stopped(const std::shared_ptr<ExecuteTaskAckState>& state) noexcept
{
    if (state) state->stop();
}

void scheduling_execute_task_ack_failed(const std::shared_ptr<ExecuteTaskAckState>& state) noexcept
{
    if (state) state->fail();
}

void scheduling_execute_task_ack_drained(const std::shared_ptr<ExecuteTaskAckState>& state) noexcept
{
    if (state) state->drain_and_stop();
}
} // namespace ams_mel::internal

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
/* Test-facade-only hook (never in the production export map):
 *   0 park the next bridge callback inside receive(), counted in flight and
 *     with ExecuteTaskAckState::mutex released
 *   1 wait (bounded) until the parked callback has entered
 *   2 release the parked callback
 *   3 wait (bounded) until physical teardown waits for in-flight callbacks
 *   4 return the number of snapshot allocations attempted so far
 *   5 arm (value != 0) or disarm a one-shot snapshot allocation failure
 *   6 return 1 while that allocation failure is still armed
 *   7 wait (bounded) until a Receive blocks waiting for an event (consumes it)
 *   8 forget any earlier Receive wait observation
 * Returns 1 on success/true and 0 on timeout/false or an unknown operation. */
extern "C" __attribute__((visibility("default"))) int
ams_mel_test_scheduling_execute_task_ack_probe(unsigned operation, std::size_t value)
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

void ExecuteTaskAckState::receive(const irmel::ExecuteTaskAck *input) noexcept
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
                     * public Close, Scheduling Close and physical teardown can
                     * proceed. */
                    lock.unlock();
                    probe_ready.wait(probe, [] { return !park_next; });
                    probe.unlock();
                    lock.lock();
                }
            }
#endif
            active = lifecycle == ExecuteTaskAckLifecycle::Active;
            full = queue.size() >= capacity;
        }
        /* The copy runs without the state mutex, but strictly before this
         * callback returns: the provider payload is never read afterwards. */
        Outcome outcome = Outcome::Ignored;
        std::unique_ptr<ExecuteTaskAckEventData> event;
        if (active) {
            if (!input)
                /* A null payload is malformed even while the queue is full. */
                outcome = Outcome::Malformed;
            else if (full)
                /* Full-queue optimization: never allocate a snapshot that
                 * would only be dropped. */
                outcome = Outcome::Full;
            else {
                event = ams_mel::internal::copy_execute_task_ack(*input);
                outcome = Outcome::Copied;
            }
        }
        std::lock_guard lock{mutex};
        if (outcome != Outcome::Ignored && lifecycle == ExecuteTaskAckLifecycle::Active) {
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

void ExecuteTaskAckState::deactivate() noexcept
{
    try {
        std::lock_guard lock{mutex};
        if (lifecycle == ExecuteTaskAckLifecycle::Active)
            lifecycle = ExecuteTaskAckLifecycle::Inactive;
        ready.notify_all();
    } catch (...) {
    }
}

void ExecuteTaskAckState::stop() noexcept
{
    try {
        std::lock_guard lock{mutex};
        if (lifecycle == ExecuteTaskAckLifecycle::Active)
            lifecycle = ExecuteTaskAckLifecycle::Stopped;
        ready.notify_all();
    } catch (...) {
    }
}

void ExecuteTaskAckState::fail() noexcept
{
    try {
        std::lock_guard lock{mutex};
        if (lifecycle == ExecuteTaskAckLifecycle::Active ||
            lifecycle == ExecuteTaskAckLifecycle::Stopped)
            lifecycle = ExecuteTaskAckLifecycle::Failed;
        ready.notify_all();
    } catch (...) {
    }
}

void ExecuteTaskAckState::drain_and_stop() noexcept
{
    try {
        std::unique_lock lock{mutex};
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
        if (callbacks != 0U) observe(drain_waiting);
#endif
        callbacks_done.wait(lock, [this] { return callbacks == 0U; });
        if (lifecycle == ExecuteTaskAckLifecycle::Active)
            lifecycle = ExecuteTaskAckLifecycle::Stopped;
        ready.notify_all();
    } catch (...) {
    }
}

extern "C" ams_mel_status_t ams_mel_ir_scheduling_execute_task_ack_open(
    ams_mel_ir_scheduling *scheduling, std::size_t queue_capacity,
    ams_mel_ir_scheduling_execute_task_ack_metadata **output, ams_mel_ir_return_t *registration,
    char *out, std::size_t capacity, std::size_t *required) noexcept
{
    ack_diagnostic("", out, capacity, required);
    /* Local argument rejection happens before the one shot is consumed. */
    if (!scheduling || !queue_capacity || !output || *output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    std::shared_ptr<ExecuteTaskAckState> state;
    std::function<void(irmel::Channel&, const irmel::ExecuteTaskAck *const)> callback;
    std::unique_ptr<ams_mel_ir_scheduling_execute_task_ack_metadata> owner;
    try {
        state = std::make_shared<ExecuteTaskAckState>();
        state->capacity = queue_capacity;
        callback = [state](irmel::Channel&, const irmel::ExecuteTaskAck *const value) noexcept {
            /* Keep the state alive for the whole call even if the provider
             * destroys its stored closure concurrently. */
            const auto keep = state;
            keep->receive(value);
        };
        owner = std::make_unique<ams_mel_ir_scheduling_execute_task_ack_metadata>();
        owner->state = state;
    } catch (...) {
        ack_diagnostic("ExecuteTaskAck metadata allocation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    /* Declared before `channel`, so the provider channel reference is released
     * first while the Scheduling graph (Session, provider library) is still
     * retained by `graph`. */
    std::shared_ptr<void> graph;
    std::shared_ptr<irmel::SchedulingChannel> channel;
    if (!ams_mel::internal::claim_scheduling_execute_task_ack(*scheduling, state, channel, graph)) {
        ack_diagnostic("Scheduling owner is not eligible or ExecuteTaskAck was already attempted",
                       out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    irmel::Return result{};
    try {
        /* No bridge lock is held: synchronous delivery during registration is
         * safe. The one shot was already recorded by the claim. This is the
         * exact pinned ExecuteTaskAck overload; no exec or send is called. */
        result = channel->registerMetadataCallback(std::move(callback));
    } catch (...) {
        state->deactivate();
        ack_diagnostic("provider exception during ExecuteTaskAck registration", out, capacity,
                       required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
    ams_mel_ir_return_t mapped = AMS_MEL_IR_RETURN_FAIL;
    const bool known = ack_map_return(result, mapped);
    if (known && registration) *registration = mapped;
    if (!known || mapped != AMS_MEL_IR_RETURN_SUCCESS) {
        state->deactivate();
        ack_diagnostic(!known ? "provider returned an unknown ExecuteTaskAck registration result" :
                       mapped == AMS_MEL_IR_RETURN_NOT_SUPPORTED ?
                           "provider does not support ExecuteTaskAck callback" :
                           "ExecuteTaskAck callback registration failed",
                       out, capacity, required);
        return AMS_MEL_PROVIDER_FAILED;
    }
    *output = owner.release();
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_scheduling_execute_task_ack_receive(
    ams_mel_ir_scheduling_execute_task_ack_metadata *metadata, std::uint32_t timeout_ms,
    ams_mel_ir_scheduling_execute_task_ack_event **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    ack_diagnostic("", out, capacity, required);
    if (!metadata || !metadata->state || !output || *output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        auto& state = *metadata->state;
        auto owner = std::make_unique<ams_mel_ir_scheduling_execute_task_ack_event>();
        std::unique_lock lock{state.mutex};
        if (state.queue.empty() && state.lifecycle == ExecuteTaskAckLifecycle::Active &&
            timeout_ms) {
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
            observe(receive_waiting);
#endif
            state.ready.wait_for(lock, std::chrono::milliseconds{timeout_ms}, [&] {
                return !state.queue.empty() || state.lifecycle != ExecuteTaskAckLifecycle::Active;
            });
        }
        if (!state.queue.empty()) {
            owner->data = std::move(state.queue.front());
            state.queue.pop_front();
            *output = owner.release();
            return AMS_MEL_OK;
        }
        if (state.lifecycle == ExecuteTaskAckLifecycle::Failed) {
            ack_diagnostic("Scheduling channel cleanup failed; ExecuteTaskAck owner failed", out,
                           capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        return state.lifecycle == ExecuteTaskAckLifecycle::Active ? AMS_MEL_TIMEOUT
                                                                  : AMS_MEL_STREAM_STOPPED;
    } catch (...) {
        ack_diagnostic("ExecuteTaskAck metadata receive failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_scheduling_execute_task_ack_get_counters(
    const ams_mel_ir_scheduling_execute_task_ack_metadata *metadata,
    ams_mel_ir_metadata_counters_v1 *output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    ack_diagnostic("", out, capacity, required);
    if (!metadata || !metadata->state || !output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::lock_guard lock{metadata->state->mutex};
        *output = metadata->state->counters;
        return AMS_MEL_OK;
    } catch (...) {
        ack_diagnostic("ExecuteTaskAck counter query failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_scheduling_execute_task_ack_close(
    ams_mel_ir_scheduling_execute_task_ack_metadata **metadata, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    ack_diagnostic("", out, capacity, required);
    if (!metadata || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    auto *owner = std::exchange(*metadata, nullptr);
    /* Deactivates consumption only: the provider keeps the registered callback
     * (upstream has no unregister) and later callbacks never enqueue. */
    if (owner && owner->state) owner->state->deactivate();
    delete owner;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_scheduling_execute_task_ack_event_view(
    const ams_mel_ir_scheduling_execute_task_ack_event *event,
    const ams_mel_ir_execute_task_ack_v1 **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    ack_diagnostic("", out, capacity, required);
    if (!event || !event->data || !output || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    *output = &event->data->view;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_scheduling_execute_task_ack_event_close(
    ams_mel_ir_scheduling_execute_task_ack_event **event, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    ack_diagnostic("", out, capacity, required);
    if (!event || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    delete std::exchange(*event, nullptr);
    return AMS_MEL_OK;
}
