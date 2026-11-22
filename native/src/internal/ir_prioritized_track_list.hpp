#pragma once

#include <ams_mel/abi.h>
#include <irmel/library/irmel-types/PrioritizedTrackListCmd.h>

#include <bit>
#include <limits>
#include <utility>

namespace ams_mel_prioritized_tracks {
using namespace ams::iface::irmel;

static_assert(sizeof(double) == sizeof(std::uint64_t) &&
              std::numeric_limits<double>::is_iec559 &&
              std::numeric_limits<double>::digits == 53);
static_assert(std::numeric_limits<std::chrono::nanoseconds::rep>::is_signed &&
              std::numeric_limits<std::chrono::nanoseconds::rep>::digits >= 63);
static_assert(static_cast<std::uint32_t>(Task::NumTaskTypes) == AMS_MEL_IR_TASK_RESERVED_6 + 1U);

inline bool same_double(double left, double right) noexcept
{
    return std::bit_cast<std::uint64_t>(left) == std::bit_cast<std::uint64_t>(right);
}

inline bool matches(const ams_mel_ir_range_az_el_v1& input, const RangeAzEl& value) noexcept
{
    return same_double(input.range_m, value.range) && same_double(input.azimuth_rad, value.az) &&
           same_double(input.elevation_rad, value.el) && same_double(input.range_rate_mps, value.rangerate);
}

inline RangeAzEl convert(const ams_mel_ir_range_az_el_v1& input) noexcept
{
    return {input.range_m, input.azimuth_rad, input.elevation_rad, input.range_rate_mps};
}

inline bool matches(const ams_mel_ir_track_data_v1& input, const TrackData& value)
{
    return input.timestamp_ns == value.getTimestamp().count() && input.track_id == value.getTrackId() &&
           input.irst_priority == value.getIrstPriority() && input.reserved_priority == value.getReservedPriority() &&
           input.irst_precedence == value.getIrstPrecedence() && input.reserved_precedence == value.getReservedPrecedence() &&
           input.is_skippable == static_cast<std::uint32_t>(value.getIsSkippable()) &&
           input.maintain_outside_fos == static_cast<std::uint32_t>(value.getMaintainOutsideFOS()) &&
           input.irst_track_id == value.getIrstTrackId() && matches(input.position, value.getPosition()) &&
           matches(input.velocity, value.getVelocity()) && input.task == static_cast<std::uint32_t>(value.getTask()) &&
           input.command_id == value.getCommandID();
}

inline TrackData convert(const ams_mel_ir_track_data_v1& input)
{
    TrackData value;
    value.setTimestamp(std::chrono::nanoseconds{input.timestamp_ns});
    value.setTrackId(input.track_id);
    value.setIrstPriority(input.irst_priority);
    value.setReservedPriority(input.reserved_priority);
    value.setIrstPrecedence(input.irst_precedence);
    value.setReservedPrecedence(input.reserved_precedence);
    value.setIsSkippable(input.is_skippable != 0U);
    value.setMaintainOutsideFOS(input.maintain_outside_fos != 0U);
    value.setIrstTrackId(input.irst_track_id);
    value.setPosition(convert(input.position));
    value.setVelocity(convert(input.velocity));
    value.setTask(static_cast<Task>(input.task));
    value.setCommandID(input.command_id);
    return value;
}

inline ams_mel_status_t convert(const ams_mel_ir_prioritized_track_list_cmd_v1& input,
                                PrioritizedTrackListCmd& command, bool fail_allocation = false)
{
    const auto [data, size] = input.tracks;
    std::vector<TrackData> tracks;
    if ((!data && size != 0U) || size > std::numeric_limits<std::ptrdiff_t>::max() / sizeof(*data) ||
        size > tracks.max_size()) return AMS_MEL_INVALID_ARGUMENT;
    for (std::size_t index = 0; index < size; ++index) {
        const auto& entry = data[index];
        if (entry.is_skippable > 1U || entry.maintain_outside_fos > 1U ||
            entry.task > AMS_MEL_IR_TASK_RESERVED_6) return AMS_MEL_INVALID_ARGUMENT;
    }
    if (fail_allocation) throw std::bad_alloc{};
    tracks.reserve(size);
    for (std::size_t index = 0; index < size; ++index) tracks.push_back(convert(data[index]));
    command = PrioritizedTrackListCmd{std::chrono::nanoseconds{input.schedule_time_ns},
        std::chrono::nanoseconds{input.time_to_live_ns}, input.command_id, input.track_count, std::move(tracks)};
    const auto& copied = command.getTrackData();
    if (command.getScheduleTime().count() != input.schedule_time_ns ||
        command.getTimeToLive().count() != input.time_to_live_ns || command.getCommandID() != input.command_id ||
        command.getTrackCount() != input.track_count || copied.size() != size) return AMS_MEL_INTERNAL_ERROR;
    for (std::size_t index = 0; index < size; ++index) {
        if (!matches(data[index], copied[index])) return AMS_MEL_INTERNAL_ERROR;
    }
    return AMS_MEL_OK;
}
} // namespace ams_mel_prioritized_tracks
