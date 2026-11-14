#include <ams_mel/abi.h>
#include "internal/ir_scheduling_task_executing.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>

/* IRMEL-ADA-013: opt-in @RequiredIfIRSTTrackTasking SchedulingChannel
 * TaskExecutingRep owner. Independent of the Scheduling foundation and of the
 * IRMEL-ADA-012 ExecuteTaskAck owner: Open, Enable, capabilities, Close, the
 * inherited common Channel view and ExecuteTaskAck Open never register this
 * overload, this owner never registers ExecuteTaskAck, and it never calls any
 * Scheduling exec or send. */

namespace {
using namespace ams::iface;
/* Diagnostics here are fixed ASCII strings. */
void rep_diagnostic(std::string_view text, char *out, std::size_t capacity,
                    std::size_t *required) noexcept
{
    if (required) *required = text.size() + 1U;
    if (out && capacity) {
        const auto copied = std::min(text.size(), capacity - 1U);
        std::memcpy(out, text.data(), copied);
        out[copied] = '\0';
    }
}

bool rep_map_return(irmel::Return input, ams_mel_ir_return_t& value) noexcept
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
using Rep = irmel::TaskExecutingRep;
template<class T> using Bare = std::remove_cv_t<std::remove_reference_t<T>>;
static_assert(std::is_same_v<decltype(std::declval<const Rep&>().getCommandID()), std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<const Rep&>().getActivityId()), std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<const Rep&>().getReqId()), std::uint32_t>);
static_assert(std::is_same_v<Bare<decltype(std::declval<const Rep&>().getTask())>, irmel::Task>);
static_assert(std::is_same_v<decltype(std::declval<const Rep&>().getEstimatedDuration()),
                             std::chrono::nanoseconds>);
static_assert(std::is_same_v<decltype(std::declval<const Rep&>().getTrackId()), std::int32_t>);
static_assert(std::is_same_v<decltype(std::declval<const Rep&>().getPrevActivityId()),
                             std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<const Rep&>().getPrevReqId()), std::uint32_t>);
static_assert(std::is_same_v<Bare<decltype(std::declval<const Rep&>().getPrevTask())>,
                             irmel::Task>);
static_assert(std::is_same_v<decltype(std::declval<const Rep&>().getPrevTaskStatus()),
                             irmel::TaskCompStatus>);
/* Both enums are separate std::uint32_t enumerations. */
static_assert(std::is_same_v<std::underlying_type_t<irmel::Task>, std::uint32_t>);
static_assert(std::is_same_v<std::underlying_type_t<irmel::TaskCompStatus>, std::uint32_t>);
static_assert(!std::is_same_v<irmel::Task, irmel::TaskCompStatus>);
/* The nanosecond count is signed 64-bit and copied unconverted. */
static_assert(std::is_same_v<std::chrono::nanoseconds::period, std::nano>);
static_assert(std::is_signed_v<std::chrono::nanoseconds::rep> &&
              sizeof(std::chrono::nanoseconds::rep) == sizeof(std::int64_t));
/* The C fields keep each upstream width and signedness, in getter order. */
using View = ams_mel_ir_task_executing_rep_v1;
static_assert(std::is_same_v<decltype(View::command_id), std::uint32_t>);
static_assert(std::is_same_v<decltype(View::activity_id), std::uint32_t>);
static_assert(std::is_same_v<decltype(View::request_id), std::uint32_t>);
static_assert(std::is_same_v<decltype(View::task), std::uint32_t>);
static_assert(std::is_same_v<decltype(View::estimated_duration_ns), std::int64_t>);
static_assert(std::is_same_v<decltype(View::track_id), std::int32_t>);
static_assert(std::is_same_v<decltype(View::previous_activity_id), std::uint32_t>);
static_assert(std::is_same_v<decltype(View::previous_request_id), std::uint32_t>);
static_assert(std::is_same_v<decltype(View::previous_task), std::uint32_t>);
static_assert(std::is_same_v<decltype(View::previous_task_status), std::uint32_t>);
static_assert(offsetof(View, command_id) == 0U && offsetof(View, activity_id) == 4U &&
              offsetof(View, request_id) == 8U && offsetof(View, task) == 12U &&
              offsetof(View, estimated_duration_ns) == 16U && offsetof(View, track_id) == 24U &&
              offsetof(View, previous_activity_id) == 28U &&
              offsetof(View, previous_request_id) == 32U &&
              offsetof(View, previous_task) == 36U &&
              offsetof(View, previous_task_status) == 40U, "upstream getter order");
static_assert(sizeof(View) == 48U && alignof(View) == 8U, "fixed 48-byte record");
/* Every published C constant is checked against the pinned enumerators. */
#define AMS_MEL_TASK_IS(name, value) \
    static_assert(static_cast<std::uint32_t>(irmel::Task::name) == (value), #name)
AMS_MEL_TASK_IS(NoTask, AMS_MEL_IR_TASK_NO_TASK);
AMS_MEL_TASK_IS(AbortTask, AMS_MEL_IR_TASK_ABORT_TASK);
AMS_MEL_TASK_IS(IRSTCamCal, AMS_MEL_IR_TASK_IRST_CAM_CAL);
AMS_MEL_TASK_IS(CamCal, AMS_MEL_IR_TASK_CAM_CAL);
AMS_MEL_TASK_IS(IRSTBoresight, AMS_MEL_IR_TASK_IRST_BORESIGHT);
AMS_MEL_TASK_IS(Idle, AMS_MEL_IR_TASK_IDLE);
AMS_MEL_TASK_IS(IRSTScan, AMS_MEL_IR_TASK_IRST_SCAN);
AMS_MEL_TASK_IS(IRSTPosition, AMS_MEL_IR_TASK_IRST_POSITION);
AMS_MEL_TASK_IS(IRSTSTT, AMS_MEL_IR_TASK_IRST_STT);
AMS_MEL_TASK_IS(IRSTMTT, AMS_MEL_IR_TASK_IRST_MTT);
AMS_MEL_TASK_IS(IRSTUpdate, AMS_MEL_IR_TASK_IRST_UPDATE);
AMS_MEL_TASK_IS(Reserved1, AMS_MEL_IR_TASK_RESERVED_1);
AMS_MEL_TASK_IS(Reserved2, AMS_MEL_IR_TASK_RESERVED_2);
AMS_MEL_TASK_IS(Reserved3, AMS_MEL_IR_TASK_RESERVED_3);
AMS_MEL_TASK_IS(Reserved4, AMS_MEL_IR_TASK_RESERVED_4);
AMS_MEL_TASK_IS(Reserved5, AMS_MEL_IR_TASK_RESERVED_5);
AMS_MEL_TASK_IS(Reserved6, AMS_MEL_IR_TASK_RESERVED_6);
#undef AMS_MEL_TASK_IS
/* NumTaskTypes is the first value that is not a defined Task. */
static_assert(static_cast<std::uint32_t>(irmel::Task::NumTaskTypes) ==
              AMS_MEL_IR_TASK_RESERVED_6 + 1U);
#define AMS_MEL_STATUS_IS(name, value) \
    static_assert(static_cast<std::uint32_t>(irmel::TaskCompStatus::name) == (value), #name)
AMS_MEL_STATUS_IS(NoTask, AMS_MEL_IR_TASK_COMP_STATUS_NO_TASK);
AMS_MEL_STATUS_IS(Complete, AMS_MEL_IR_TASK_COMP_STATUS_COMPLETE);
AMS_MEL_STATUS_IS(Timeout, AMS_MEL_IR_TASK_COMP_STATUS_TIMEOUT);
AMS_MEL_STATUS_IS(Abort, AMS_MEL_IR_TASK_COMP_STATUS_ABORT);
AMS_MEL_STATUS_IS(Skipped, AMS_MEL_IR_TASK_COMP_STATUS_SKIPPED);
#undef AMS_MEL_STATUS_IS
/* The existing published metadata capability constant names this callback. */
static_assert(static_cast<std::uint32_t>(
                  irmel::ChannelMetadataCapabilityType::TaskExecutingRep) ==
              AMS_MEL_IR_METADATA_TASK_EXECUTING_REP);
static_assert(AMS_MEL_IR_METADATA_TASK_EXECUTING_REP == 12U);

/* Reserved1..Reserved6 are defined (preserved, non-operational) values;
 * NumTaskTypes and everything above it are not. */
bool defined_task(std::uint32_t value) noexcept { return value <= AMS_MEL_IR_TASK_RESERVED_6; }
bool defined_status(std::uint32_t value) noexcept
{ return value <= AMS_MEL_IR_TASK_COMP_STATUS_SKIPPED; }

void increment(std::uint64_t& value) noexcept
{
    if (value != std::numeric_limits<std::uint64_t>::max()) ++value;
}

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
/* Test-only deterministic observation and failure injection, compiled out of
 * production builds, separate from the ExecuteTaskAck probe state. Lock order
 * is always TaskExecutingState::mutex, then probe_mutex. */
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
bool read_task_executing_rep(const irmel::TaskExecutingRep& input,
                             ams_mel_ir_task_executing_rep_v1& output) noexcept
{
    /* Plain full-width assignment in upstream getter order. Identifiers are
     * never swapped, masked or correlated; current and previous values stay
     * independent; the signed nanosecond count is not converted. */
    output.command_id = input.getCommandID();
    output.activity_id = input.getActivityId();
    output.request_id = input.getReqId();
    output.task = static_cast<std::uint32_t>(input.getTask());
    output.estimated_duration_ns = input.getEstimatedDuration().count();
    output.track_id = input.getTrackId();
    output.previous_activity_id = input.getPrevActivityId();
    output.previous_request_id = input.getPrevReqId();
    output.previous_task = static_cast<std::uint32_t>(input.getPrevTask());
    output.previous_task_status = static_cast<std::uint32_t>(input.getPrevTaskStatus());
    /* Each enum domain is checked independently; one bad value rejects all. */
    if (!defined_task(output.task)) return false;
    if (!defined_task(output.previous_task)) return false;
    if (!defined_status(output.previous_task_status)) return false;
    return true;
}

std::unique_ptr<TaskExecutingEventData> snapshot_task_executing_rep(
    const ams_mel_ir_task_executing_rep_v1& value)
{
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    copy_attempts.fetch_add(1);
    if (copy_failure_armed.exchange(false)) throw std::bad_alloc{};
#endif
    auto event = std::make_unique<TaskExecutingEventData>();
    event->view = value;
    return event;
}

void scheduling_task_executing_stopped(const std::shared_ptr<TaskExecutingState>& state) noexcept
{
    if (state) state->stop();
}

void scheduling_task_executing_failed(const std::shared_ptr<TaskExecutingState>& state) noexcept
{
    if (state) state->fail();
}

void scheduling_task_executing_drained(const std::shared_ptr<TaskExecutingState>& state) noexcept
{
    if (state) state->drain_and_stop();
}
} // namespace ams_mel::internal

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
/* Test-facade-only hook (never in the production export map), with the same
 * operation numbering as the ExecuteTaskAck probe but independent state:
 *   0 park the next TaskExecutingRep bridge callback inside receive(), counted
 *     in flight and with TaskExecutingState::mutex released
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
ams_mel_test_scheduling_task_executing_probe(unsigned operation, std::size_t value)
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


void TaskExecutingState::receive(const irmel::TaskExecutingRep *input) noexcept
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
            active = lifecycle == TaskExecutingLifecycle::Active;
            full = queue.size() >= capacity;
        }
        /* Validation and the copy run without the state mutex, but strictly
         * before this callback returns: the provider payload is never read
         * afterwards. Domain validation precedes the queue-full decision, so a
         * malformed report is never misclassified as merely dropped. */
        Outcome outcome = Outcome::Ignored;
        std::unique_ptr<TaskExecutingEventData> event;
        if (active) {
            ams_mel_ir_task_executing_rep_v1 value{};
            if (!input)
                outcome = Outcome::Malformed;
            else if (!ams_mel::internal::read_task_executing_rep(*input, value))
                outcome = Outcome::Malformed;
            else if (full)
                /* Full-queue optimization: never allocate a snapshot that
                 * would only be dropped. */
                outcome = Outcome::Full;
            else {
                event = ams_mel::internal::snapshot_task_executing_rep(value);
                outcome = Outcome::Copied;
            }
        }
        std::lock_guard lock{mutex};
        if (outcome != Outcome::Ignored && lifecycle == TaskExecutingLifecycle::Active) {
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
         * allocation failure rejects this whole report only; nothing partial
         * was published and earlier queued reports are untouched. */
        try {
            std::lock_guard lock{mutex};
            increment(counters.malformed_or_unsupported);
            if (counted && --callbacks == 0U) callbacks_done.notify_all();
        } catch (...) {
        }
    }
}

void TaskExecutingState::deactivate() noexcept
{
    try {
        std::lock_guard lock{mutex};
        if (lifecycle == TaskExecutingLifecycle::Active)
            lifecycle = TaskExecutingLifecycle::Inactive;
        ready.notify_all();
    } catch (...) {
    }
}

void TaskExecutingState::stop() noexcept
{
    try {
        std::lock_guard lock{mutex};
        if (lifecycle == TaskExecutingLifecycle::Active)
            lifecycle = TaskExecutingLifecycle::Stopped;
        ready.notify_all();
    } catch (...) {
    }
}

void TaskExecutingState::fail() noexcept
{
    try {
        std::lock_guard lock{mutex};
        if (lifecycle == TaskExecutingLifecycle::Active ||
            lifecycle == TaskExecutingLifecycle::Stopped)
            lifecycle = TaskExecutingLifecycle::Failed;
        ready.notify_all();
    } catch (...) {
    }
}

void TaskExecutingState::drain_and_stop() noexcept
{
    try {
        std::unique_lock lock{mutex};
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
        if (callbacks != 0U) observe(drain_waiting);
#endif
        callbacks_done.wait(lock, [this] { return callbacks == 0U; });
        if (lifecycle == TaskExecutingLifecycle::Active)
            lifecycle = TaskExecutingLifecycle::Stopped;
        ready.notify_all();
    } catch (...) {
    }
}


extern "C" ams_mel_status_t ams_mel_ir_scheduling_task_executing_open(
    ams_mel_ir_scheduling *scheduling, std::size_t queue_capacity,
    ams_mel_ir_scheduling_task_executing_metadata **output, ams_mel_ir_return_t *registration,
    char *out, std::size_t capacity, std::size_t *required) noexcept
{
    rep_diagnostic("", out, capacity, required);
    /* Local argument rejection happens before the one shot is consumed. */
    if (!scheduling || !queue_capacity || !output || *output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    std::shared_ptr<TaskExecutingState> state;
    std::function<void(irmel::Channel&, const irmel::TaskExecutingRep *const)> callback;
    std::unique_ptr<ams_mel_ir_scheduling_task_executing_metadata> owner;
    try {
        state = std::make_shared<TaskExecutingState>();
        state->capacity = queue_capacity;
        callback = [state](irmel::Channel&, const irmel::TaskExecutingRep *const value) noexcept {
            /* Keep the state alive for the whole call even if the provider
             * destroys its stored closure concurrently. */
            const auto keep = state;
            keep->receive(value);
        };
        owner = std::make_unique<ams_mel_ir_scheduling_task_executing_metadata>();
        owner->state = state;
    } catch (...) {
        rep_diagnostic("TaskExecutingRep metadata allocation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    /* Declared before `channel`, so the provider channel reference is released
     * first while the Scheduling graph (Session, provider library) is still
     * retained by `graph`. */
    std::shared_ptr<void> graph;
    std::shared_ptr<irmel::SchedulingChannel> channel;
    if (!ams_mel::internal::claim_scheduling_task_executing(*scheduling, state, channel, graph)) {
        rep_diagnostic(
            "Scheduling owner is not eligible or TaskExecutingRep was already attempted", out,
            capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    irmel::Return result{};
    try {
        /* No bridge lock is held: synchronous delivery during registration is
         * safe. The one shot was already recorded by the claim. This is the
         * exact pinned TaskExecutingRep overload; no exec, send or other
         * Scheduling registration is called. */
        result = channel->registerMetadataCallback(std::move(callback));
    } catch (...) {
        state->deactivate();
        rep_diagnostic("provider exception during TaskExecutingRep registration", out, capacity,
                       required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
    ams_mel_ir_return_t mapped = AMS_MEL_IR_RETURN_FAIL;
    const bool known = rep_map_return(result, mapped);
    if (known && registration) *registration = mapped;
    if (!known || mapped != AMS_MEL_IR_RETURN_SUCCESS) {
        state->deactivate();
        rep_diagnostic(!known ? "provider returned an unknown TaskExecutingRep registration result" :
                       mapped == AMS_MEL_IR_RETURN_NOT_SUPPORTED ?
                           "provider does not support TaskExecutingRep callback" :
                           "TaskExecutingRep callback registration failed",
                       out, capacity, required);
        return AMS_MEL_PROVIDER_FAILED;
    }
    *output = owner.release();
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_scheduling_task_executing_receive(
    ams_mel_ir_scheduling_task_executing_metadata *metadata, std::uint32_t timeout_ms,
    ams_mel_ir_scheduling_task_executing_event **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    rep_diagnostic("", out, capacity, required);
    if (!metadata || !metadata->state || !output || *output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        auto& state = *metadata->state;
        auto owner = std::make_unique<ams_mel_ir_scheduling_task_executing_event>();
        std::unique_lock lock{state.mutex};
        if (state.queue.empty() && state.lifecycle == TaskExecutingLifecycle::Active &&
            timeout_ms) {
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
            observe(receive_waiting);
#endif
            state.ready.wait_for(lock, std::chrono::milliseconds{timeout_ms}, [&] {
                return !state.queue.empty() || state.lifecycle != TaskExecutingLifecycle::Active;
            });
        }
        if (!state.queue.empty()) {
            owner->data = std::move(state.queue.front());
            state.queue.pop_front();
            *output = owner.release();
            return AMS_MEL_OK;
        }
        if (state.lifecycle == TaskExecutingLifecycle::Failed) {
            rep_diagnostic("Scheduling channel cleanup failed; TaskExecutingRep owner failed", out,
                           capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        return state.lifecycle == TaskExecutingLifecycle::Active ? AMS_MEL_TIMEOUT
                                                                 : AMS_MEL_STREAM_STOPPED;
    } catch (...) {
        rep_diagnostic("TaskExecutingRep metadata receive failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_scheduling_task_executing_get_counters(
    const ams_mel_ir_scheduling_task_executing_metadata *metadata,
    ams_mel_ir_metadata_counters_v1 *output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    rep_diagnostic("", out, capacity, required);
    if (!metadata || !metadata->state || !output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::lock_guard lock{metadata->state->mutex};
        *output = metadata->state->counters;
        return AMS_MEL_OK;
    } catch (...) {
        rep_diagnostic("TaskExecutingRep counter query failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_scheduling_task_executing_close(
    ams_mel_ir_scheduling_task_executing_metadata **metadata, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    rep_diagnostic("", out, capacity, required);
    if (!metadata || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    auto *owner = std::exchange(*metadata, nullptr);
    /* Deactivates consumption only: the provider keeps the registered callback
     * (upstream has no unregister) and later callbacks never enqueue. The
     * ExecuteTaskAck owner, if any, is unaffected. */
    if (owner && owner->state) owner->state->deactivate();
    delete owner;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_scheduling_task_executing_event_view(
    const ams_mel_ir_scheduling_task_executing_event *event,
    const ams_mel_ir_task_executing_rep_v1 **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    rep_diagnostic("", out, capacity, required);
    if (!event || !event->data || !output || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    *output = &event->data->view;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_scheduling_task_executing_event_close(
    ams_mel_ir_scheduling_task_executing_event **event, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    rep_diagnostic("", out, capacity, required);
    if (!event || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    delete std::exchange(*event, nullptr);
    return AMS_MEL_OK;
}
