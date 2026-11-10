#pragma once

/* IRMEL-ADA-012 private opt-in Scheduling ExecuteTaskAck reception.
 *
 * One independently registered owner per Scheduling owner, never registered by
 * ams_mel_ir_scheduling_open/_enable/_get_capabilities/_close or by the
 * inherited common Channel view. Provider callbacks only ever touch
 * ExecuteTaskAckState through the shared_ptr captured by the registered
 * closure, so a late callback after the public owner, the Scheduling owner or
 * the Session is closed remains memory-safe. Every non-null payload is copied
 * by value before the provider callback returns; no provider-owned object or
 * borrowed pointer survives callback return.
 *
 * ExecuteTaskAckState::mutex is a leaf lock. It is never held across provider
 * code and never acquired while SchedulingState::mutex is held. */

#include <ams_mel/abi.h>

#include <irmel/library/scheduling/ExecuteTaskAck.h>
#include <irmel/library/scheduling/SchedulingChannel.h>

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>

/* Event-owned storage: one fixed-size C record, no indirection. */
struct ExecuteTaskAckEventData {
    ams_mel_ir_execute_task_ack_v1 view{};
};

enum class ExecuteTaskAckLifecycle { Active, Inactive, Stopped, Failed };

struct ExecuteTaskAckState {
    std::mutex mutex;
    std::condition_variable ready;
    std::condition_variable callbacks_done;
    std::deque<std::unique_ptr<ExecuteTaskAckEventData>> queue;
    std::size_t capacity{};
    ams_mel_ir_metadata_counters_v1 counters{};
    ExecuteTaskAckLifecycle lifecycle{ExecuteTaskAckLifecycle::Active};
    /* Bridge callbacks currently inside receive(); guarded by mutex. */
    std::size_t callbacks{};

    /* Provider callback entry point. Never throws. */
    void receive(const ams::iface::irmel::ExecuteTaskAck *input) noexcept;
    /* Active -> Inactive (public close or failed registration). */
    void deactivate() noexcept;
    /* Active -> Stopped (owning Scheduling owner logically closed). */
    void stop() noexcept;
    /* Uncertain provider ownership after failed detach; Receive drains, then
     * reports PROVIDER_FAILED. */
    void fail() noexcept;
    /* After provider channel destruction: wait for bridge callbacks already in
     * flight, then stop. Never called from inside an ExecuteTaskAck callback. */
    void drain_and_stop() noexcept;
};

struct ams_mel_ir_scheduling_execute_task_ack_metadata {
    std::shared_ptr<ExecuteTaskAckState> state;
};
struct ams_mel_ir_scheduling_execute_task_ack_event {
    std::unique_ptr<ExecuteTaskAckEventData> data;
};

namespace ams_mel::internal {

/* Copies both published getters exactly once, in upstream order, by plain
 * 32-bit assignment. No structural validation exists: zero, duplicate,
 * unequal and high-bit identifiers are all copied, never rejected or
 * correlated. Throws std::bad_alloc on allocation failure, in which case
 * nothing has been published. */
std::unique_ptr<ExecuteTaskAckEventData> copy_execute_task_ack(
    const ams::iface::irmel::ExecuteTaskAck& input);

/* Scheduling lifecycle hooks, defined with ExecuteTaskAckState. Each accepts
 * a null state (no ExecuteTaskAck owner was ever opened) and never throws.
 * They are called without SchedulingState::mutex held. */
void scheduling_execute_task_ack_stopped(
    const std::shared_ptr<ExecuteTaskAckState>& state) noexcept;
void scheduling_execute_task_ack_failed(
    const std::shared_ptr<ExecuteTaskAckState>& state) noexcept;
void scheduling_execute_task_ack_drained(
    const std::shared_ptr<ExecuteTaskAckState>& state) noexcept;

/* Defined in ir_scheduling.cpp. Under SchedulingState::mutex only: requires an
 * Attached or Enabled owner whose ExecuteTaskAck one-shot is unused, records
 * the attempt, publishes `state` to the Scheduling lifecycle and returns the
 * provider SchedulingChannel plus a type-erased strong reference to the
 * Scheduling graph for the duration of the unlocked provider registration.
 * Returns false (nothing recorded) for an ineligible owner or a repeated
 * attempt. */
bool claim_scheduling_execute_task_ack(
    ams_mel_ir_scheduling& owner, const std::shared_ptr<ExecuteTaskAckState>& state,
    std::shared_ptr<ams::iface::irmel::SchedulingChannel>& channel,
    std::shared_ptr<void>& graph) noexcept;

} // namespace ams_mel::internal
