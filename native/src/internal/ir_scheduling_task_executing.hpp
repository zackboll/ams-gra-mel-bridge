#pragma once

/* IRMEL-ADA-013 private opt-in Scheduling TaskExecutingRep reception.
 *
 * One independently registered owner per Scheduling owner, never registered by
 * ams_mel_ir_scheduling_open/_enable/_get_capabilities/_close, by the
 * inherited common Channel view or by the ExecuteTaskAck owner, and never
 * registering ExecuteTaskAck itself. Provider callbacks only ever touch
 * TaskExecutingState through the shared_ptr captured by the registered
 * closure, so a late callback after the public owner, the Scheduling owner or
 * the Session is closed remains memory-safe. Every non-null payload is read,
 * validated and copied by value before the provider callback returns; no
 * provider-owned object, std::chrono value or borrowed pointer survives
 * callback return.
 *
 * TaskExecutingState::mutex is a leaf lock, independent of
 * ExecuteTaskAckState::mutex. It is never held across provider code and never
 * acquired while SchedulingState::mutex is held. */

#include <ams_mel/abi.h>

#include <irmel/library/scheduling/SchedulingChannel.h>
#include <irmel/library/scheduling/TaskExecutingRep.h>

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>

/* Event-owned storage: one fixed-size C record, no indirection. */
struct TaskExecutingEventData {
    ams_mel_ir_task_executing_rep_v1 view{};
};

enum class TaskExecutingLifecycle { Active, Inactive, Stopped, Failed };

struct TaskExecutingState {
    std::mutex mutex;
    std::condition_variable ready;
    std::condition_variable callbacks_done;
    std::deque<std::unique_ptr<TaskExecutingEventData>> queue;
    std::size_t capacity{};
    ams_mel_ir_metadata_counters_v1 counters{};
    TaskExecutingLifecycle lifecycle{TaskExecutingLifecycle::Active};
    /* Bridge callbacks currently inside receive(); guarded by mutex. */
    std::size_t callbacks{};

    /* Provider callback entry point. Never throws. */
    void receive(const ams::iface::irmel::TaskExecutingRep *input) noexcept;
    /* Active -> Inactive (public close or failed registration). */
    void deactivate() noexcept;
    /* Active -> Stopped (owning Scheduling owner logically closed). */
    void stop() noexcept;
    /* Uncertain provider ownership after failed detach; Receive drains, then
     * reports PROVIDER_FAILED. */
    void fail() noexcept;
    /* After provider channel destruction: wait for bridge callbacks already in
     * flight, then stop. Never called from inside a TaskExecutingRep callback. */
    void drain_and_stop() noexcept;
};

struct ams_mel_ir_scheduling_task_executing_metadata {
    std::shared_ptr<TaskExecutingState> state;
};
struct ams_mel_ir_scheduling_task_executing_event {
    std::unique_ptr<TaskExecutingEventData> data;
};

namespace ams_mel::internal {

/* Reads each of the ten published getters exactly once, in upstream getter
 * order, into `output` by plain full-width assignment (the duration is the
 * exact signed nanoseconds count()). Returns false, leaving the caller to
 * reject the whole report, when the current Task or previous Task is above
 * Reserved6 (NumTaskTypes 17 and every unknown value) or the previous-task
 * TaskCompStatus is above Skipped. Identifiers, the signed track identifier
 * and the signed duration have no structural validation. Allocates nothing. */
bool read_task_executing_rep(const ams::iface::irmel::TaskExecutingRep& input,
                             ams_mel_ir_task_executing_rep_v1& output) noexcept;

/* Allocates the owned event for an already validated record. Throws
 * std::bad_alloc on allocation failure, in which case nothing is published. */
std::unique_ptr<TaskExecutingEventData> snapshot_task_executing_rep(
    const ams_mel_ir_task_executing_rep_v1& value);

/* Scheduling lifecycle hooks, defined with TaskExecutingState. Each accepts a
 * null state (no TaskExecutingRep owner was ever opened) and never throws.
 * They are called without SchedulingState::mutex held. */
void scheduling_task_executing_stopped(
    const std::shared_ptr<TaskExecutingState>& state) noexcept;
void scheduling_task_executing_failed(
    const std::shared_ptr<TaskExecutingState>& state) noexcept;
void scheduling_task_executing_drained(
    const std::shared_ptr<TaskExecutingState>& state) noexcept;

/* Defined in ir_scheduling.cpp. Under SchedulingState::mutex only: requires an
 * Attached or Enabled owner whose TaskExecutingRep one-shot (independent of
 * the ExecuteTaskAck one-shot) is unused, records the attempt, publishes
 * `state` to the Scheduling lifecycle and returns the provider
 * SchedulingChannel plus a type-erased strong reference to the Scheduling
 * graph for the duration of the unlocked provider registration. Returns false
 * (nothing recorded) for an ineligible owner or a repeated attempt. */
bool claim_scheduling_task_executing(
    ams_mel_ir_scheduling& owner, const std::shared_ptr<TaskExecutingState>& state,
    std::shared_ptr<ams::iface::irmel::SchedulingChannel>& channel,
    std::shared_ptr<void>& graph) noexcept;

} // namespace ams_mel::internal
