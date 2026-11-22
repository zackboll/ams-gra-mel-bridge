#pragma once

#include <ams_mel/abi.h>
#include <irmel/library/scheduling/SchedulingChannel.h>
#include <memory>

struct ScheduleCreatedState;

namespace ams_mel::internal {

/* These hooks run without the Scheduling lifecycle mutex. Stop prevents
 * further payload reads; drain waits before provider detach/destruction for
 * the callbacks that were admitted to read a payload. */
void scheduling_schedule_created_stopped(const std::shared_ptr<ScheduleCreatedState>&) noexcept;
void scheduling_schedule_created_drained(const std::shared_ptr<ScheduleCreatedState>&) noexcept;
void scheduling_schedule_created_failed(const std::shared_ptr<ScheduleCreatedState>&) noexcept;

/* Record the independent one-shot and obtain strong references for unlocked
 * provider registration (including synchronous delivery). */
bool claim_scheduling_schedule_created(
    ams_mel_ir_scheduling&, const std::shared_ptr<ScheduleCreatedState>&,
    std::shared_ptr<ams::iface::irmel::SchedulingChannel>&, std::shared_ptr<void>&) noexcept;

} // namespace ams_mel::internal
