#pragma once

#include <ams_mel/abi.h>
#include <irmel/library/irmel-types/TaskInfo.h>
#include <chrono>
#include <type_traits>

namespace ams_mel::internal {
using ams::iface::irmel::TaskInfo;
using ams::iface::irmel::TaskListAction;
static_assert(static_cast<unsigned>(TaskListAction::Normal) == 0U);
static_assert(static_cast<unsigned>(TaskListAction::Next) == 1U);
static_assert(static_cast<unsigned>(TaskListAction::Immediate) == 2U);
static_assert(static_cast<unsigned>(TaskListAction::Abort) == 3U);
static_assert(std::is_same_v<std::chrono::nanoseconds::rep, std::int64_t>);

/* Every bit pattern of the signed time and unsigned fields is valid. Only
 * the enum has a restricted domain. No epoch, priority or identity rules. */
inline bool convert_task_info(const ams_mel_ir_task_info_v1& input, TaskInfo& task)
{
    if (input.action > 3U) return false;
    task.setSystemTime(std::chrono::nanoseconds{input.system_time_ns});
    task.setReqId(input.request_id);
    task.setAction(static_cast<TaskListAction>(input.action));
    task.setPrecedenceWithinPriority(input.precedence_within_priority);
    task.setPriority(input.priority);
    return task.getSystemTime().count() == input.system_time_ns &&
        task.getReqId() == input.request_id &&
        static_cast<std::uint32_t>(task.getAction()) == input.action &&
        task.getPrecedenceWithinPriority() == input.precedence_within_priority &&
        task.getPriority() == input.priority;
}
} // namespace ams_mel::internal
