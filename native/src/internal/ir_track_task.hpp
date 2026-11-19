#pragma once

#include "ir_task_info.hpp"
#include <irmel/library/irmel-types/IRSTTrackTask.h>
#include <bit>
#include <limits>

namespace ams_mel::internal {
using ams::iface::irmel::IRSTTrackTask;
using ams::iface::irmel::TrackTaskParam;
using ams::iface::irmel::TrackTaskData;
static_assert(ams::iface::irmel::MAX_TRACKS == AMS_MEL_IR_TRACK_TASK_CAPACITY);
static_assert(std::is_same_v<decltype(TrackTaskData{}.getTargetID()), std::int32_t>);
static_assert(std::is_same_v<decltype(TrackTaskData{}.getTargetLocation()), AzEl>);
static_assert(std::is_same_v<decltype(TrackTaskData{}.getTargetRate()), AzEl>);
static_assert(std::is_same_v<decltype(AzEl{}.az), double>);
static_assert(std::is_same_v<decltype(AzEl{}.el), double>);
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);
static_assert(std::is_same_v<decltype(TrackTaskParam{}.getCount()), std::uint32_t>);
static_assert(std::is_same_v<decltype(TrackTaskParam{}.getDurationNanoseconds()), std::chrono::nanoseconds>);
static_assert(std::is_same_v<decltype(TrackTaskParam{}.getTargetData()),
    const std::array<TrackTaskData, 255>&>);
static_assert(std::is_same_v<decltype(IRSTTrackTask{}.getCommandID()), std::uint32_t>);
static_assert(std::is_same_v<decltype(IRSTTrackTask{}.getParams()), TrackTaskParam>);
static_assert(std::is_same_v<decltype(IRSTTrackTask{}.getTaskInfo()), TaskInfo>);

inline bool matches_track_task(const ams_mel_ir_track_task_v1& input, const IRSTTrackTask& task)
{
    const auto params = task.getParams();
    const auto info = task.getTaskInfo();
    if (task.getCommandID() != input.command_id ||
        params.getCount() != input.track_parameters.count ||
        params.getDurationNanoseconds().count() != input.track_parameters.duration_ns ||
        info.getSystemTime().count() != input.task_info.system_time_ns ||
        info.getReqId() != input.task_info.request_id ||
        static_cast<std::uint32_t>(info.getAction()) != input.task_info.action ||
        info.getPrecedenceWithinPriority() != input.task_info.precedence_within_priority ||
        info.getPriority() != input.task_info.priority) return false;
    const auto same = [](double left, double right) {
        return std::bit_cast<std::uint64_t>(left) == std::bit_cast<std::uint64_t>(right);
    };
    for (std::size_t index = 0; index < AMS_MEL_IR_TRACK_TASK_CAPACITY; ++index) {
        const auto& expected = input.track_parameters.targets[index];
        const auto& actual = params.getTargetData()[index];
        const auto location = actual.getTargetLocation();
        const auto rate = actual.getTargetRate();
        if (actual.getTargetID() != expected.target_id ||
            !same(location.az, expected.location_azimuth_rad) ||
            !same(location.el, expected.location_elevation_rad) ||
            !same(rate.az, expected.rate_azimuth_rad_per_second) ||
            !same(rate.el, expected.rate_elevation_rad_per_second)) return false;
    }
    return true;
}

inline bool convert_track_task(const ams_mel_ir_track_task_v1& input, IRSTTrackTask& task)
{
    if (input.track_parameters.count > AMS_MEL_IR_TRACK_TASK_CAPACITY) return false;
    TaskInfo info;
    if (!convert_task_info(input.task_info, info)) return false;
    std::array<TrackTaskData, AMS_MEL_IR_TRACK_TASK_CAPACITY> targets;
    for (std::size_t index = 0; index < targets.size(); ++index) {
        const auto& source = input.track_parameters.targets[index];
        targets[index].setTargetID(source.target_id);
        targets[index].setTargetLocation(AzEl{source.location_azimuth_rad, source.location_elevation_rad});
        targets[index].setTargetRate(AzEl{source.rate_azimuth_rad_per_second, source.rate_elevation_rad_per_second});
    }
    TrackTaskParam params;
    params.setDurationNanoseconds(std::chrono::nanoseconds{input.track_parameters.duration_ns});
    params.setCount(input.track_parameters.count);
    params.setTargetData(targets);
    task = IRSTTrackTask{input.command_id, params, info};
    return matches_track_task(input, task);
}
} // namespace ams_mel::internal
