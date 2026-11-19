#pragma once

#include "ir_task_info.hpp"
#include <irmel/library/irmel-types/IRSTStareTask.h>
#include <bit>
#include <limits>

namespace ams_mel::internal {
using ams::iface::irmel::CoordFrameRef;
using ams::iface::irmel::IRSTStareTask;
using ams::iface::irmel::StareTaskParam;
static_assert(static_cast<unsigned>(CoordFrameRef::Inertial) == 0U);
static_assert(static_cast<unsigned>(CoordFrameRef::Aircraft) == 1U);
static_assert(std::is_same_v<decltype(AzEl{}.az), double>);
static_assert(std::is_same_v<decltype(AzEl{}.el), double>);
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);
static_assert(std::is_same_v<decltype(IRSTStareTask{}.getCommandID()), std::uint32_t>);
static_assert(std::is_same_v<decltype(IRSTStareTask{}.getParams()), StareTaskParam>);
static_assert(std::is_same_v<decltype(IRSTStareTask{}.getTaskInfo()), TaskInfo>);
static_assert(std::is_same_v<decltype(StareTaskParam{}.getCenter()), AzEl>);
static_assert(std::is_same_v<decltype(StareTaskParam{}.getCentFrameRefEl()), CoordFrameRef>);
static_assert(std::is_same_v<decltype(StareTaskParam{}.getCentFrameRefAz()), CoordFrameRef>);
static_assert(std::is_same_v<decltype(TaskInfo{}.getReqId()), std::uint32_t>);
static_assert(std::is_same_v<decltype(TaskInfo{}.getPrecedenceWithinPriority()), std::uint32_t>);
static_assert(std::is_same_v<decltype(TaskInfo{}.getPriority()), std::uint32_t>);
static_assert(std::is_same_v<decltype(StareTaskParam{}.getDurationNanoseconds()), std::chrono::nanoseconds>);

inline bool same_angle(double left, double right) noexcept
{ return std::bit_cast<std::uint64_t>(left) == std::bit_cast<std::uint64_t>(right); }

inline bool matches_stare(const ams_mel_ir_stare_task_v1& input, const IRSTStareTask& task)
{
    const auto params = task.getParams();
    const auto center = params.getCenter();
    const auto info = task.getTaskInfo();
    return task.getCommandID() == input.command_id &&
        same_angle(center.az, input.center_azimuth_rad) &&
        same_angle(center.el, input.center_elevation_rad) &&
        static_cast<std::uint32_t>(params.getCentFrameRefEl()) == input.elevation_frame &&
        static_cast<std::uint32_t>(params.getCentFrameRefAz()) == input.azimuth_frame &&
        params.getDurationNanoseconds().count() == input.duration_ns &&
        info.getSystemTime().count() == input.task_info.system_time_ns &&
        info.getReqId() == input.task_info.request_id &&
        static_cast<std::uint32_t>(info.getAction()) == input.task_info.action &&
        info.getPrecedenceWithinPriority() == input.task_info.precedence_within_priority &&
        info.getPriority() == input.task_info.priority;
}

inline bool convert_stare(const ams_mel_ir_stare_task_v1& input, IRSTStareTask& task)
{
    if (input.elevation_frame > 1U || input.azimuth_frame > 1U) return false;
    TaskInfo info;
    if (!convert_task_info(input.task_info, info)) return false;
    StareTaskParam params;
    params.setCenter(AzEl{input.center_azimuth_rad, input.center_elevation_rad});
    params.setCentFrameRefEl(static_cast<CoordFrameRef>(input.elevation_frame));
    params.setCentFrameRefAz(static_cast<CoordFrameRef>(input.azimuth_frame));
    params.setDurationNanoseconds(std::chrono::nanoseconds{input.duration_ns});
    task = IRSTStareTask{input.command_id, params, info};
    return matches_stare(input, task);
}
} // namespace ams_mel::internal
