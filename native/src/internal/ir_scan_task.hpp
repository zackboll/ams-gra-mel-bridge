#pragma once

#include "ir_scan_param.hpp"
#include "ir_task_info.hpp"
#include <irmel/library/irmel-types/IRSTScanTask.h>

namespace ams_mel::internal {
using ams::iface::irmel::IRSTScanTask;
static_assert(std::is_same_v<decltype(IRSTScanTask{}.getCommandID()), std::uint32_t>);
static_assert(std::is_same_v<decltype(IRSTScanTask{}.getParams()), ScanParam>);
static_assert(std::is_same_v<decltype(IRSTScanTask{}.getTaskInfo()), TaskInfo>);

inline bool matches_scan_task(const ams_mel_ir_scan_task_v1& input, const IRSTScanTask& task)
{
    const auto info = task.getTaskInfo();
    return task.getCommandID() == input.command_id &&
        matches_scan(input.scan_parameters, task.getParams()) &&
        info.getSystemTime().count() == input.task_info.system_time_ns &&
        info.getReqId() == input.task_info.request_id &&
        static_cast<std::uint32_t>(info.getAction()) == input.task_info.action &&
        info.getPrecedenceWithinPriority() == input.task_info.precedence_within_priority &&
        info.getPriority() == input.task_info.priority;
}

inline bool convert_scan_task(const ams_mel_ir_scan_task_v1& input, IRSTScanTask& task)
{
    if (!valid_scan(input.scan_parameters)) return false;
    TaskInfo info;
    if (!convert_task_info(input.task_info, info)) return false;
    task = IRSTScanTask{input.command_id, convert_scan(input.scan_parameters), info};
    return matches_scan_task(input, task);
}
} // namespace ams_mel::internal
