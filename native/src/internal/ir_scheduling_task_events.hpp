#pragma once

#include <ams_mel/abi.h>
#include <irmel/library/scheduling/SchedulingChannel.h>
#include <memory>

struct TaskEventsState;

namespace ams_mel::internal {

// Called without the Scheduling mutex. Drain stops new payload reads and
// waits for admitted reads before physical provider teardown.
void scheduling_task_events_stopped(const std::shared_ptr<TaskEventsState>&) noexcept;
void scheduling_task_events_drained(const std::shared_ptr<TaskEventsState>&) noexcept;
void scheduling_task_events_failed(const std::shared_ptr<TaskEventsState>&) noexcept;

bool claim_scheduling_task_events(
    ams_mel_ir_scheduling&, const std::shared_ptr<TaskEventsState>&,
    std::shared_ptr<ams::iface::irmel::SchedulingChannel>&, std::shared_ptr<void>&) noexcept;

} // namespace ams_mel::internal
