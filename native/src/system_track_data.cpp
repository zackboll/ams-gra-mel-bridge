#include "internal/system_track_data.hpp"
#include "internal/provider_common.hpp"

#include <chrono>
#include <cstdint>
#include <type_traits>
#include <utility>

/* IRMEL-ADA-008: the one SystemTrackDataResponse/CommandStatus value contract,
 * extracted unchanged from the Track adapter so the independent C2 overload
 * reuses it rather than duplicating it. */

namespace {
using namespace ams::iface;

static_assert(std::is_same_v<decltype(std::declval<irmel::SystemTrackDataResponse>().getCommandID()),
                             std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<irmel::SystemTrackDataResponse>().getRequestId()),
                             std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<irmel::SystemTrackDataResponse>().getTrackId()),
                             std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<irmel::SystemTrackDataResponse>().getRange()),
                             double>);
static_assert(std::is_same_v<decltype(AzEl::az), double> && std::is_same_v<decltype(AzEl::el), double>);
static_assert(sizeof(std::chrono::nanoseconds::rep) == sizeof(std::int64_t));
static_assert(std::is_same_v<std::underlying_type_t<irmel::CommandState>, std::uint32_t>);
static_assert(std::is_same_v<std::underlying_type_t<irmel::CannotComply>, std::uint32_t>);
static_assert(static_cast<std::uint32_t>(irmel::CommandState::Cancelled) == AMS_MEL_IR_COMMAND_CANCELLED);
static_assert(static_cast<std::uint32_t>(irmel::CannotComply::AlignmentManeuver) ==
              AMS_MEL_IR_CANNOT_COMPLY_ALIGNMENT_MANEUVER);

/* The repository's established uint8_t representation for published bool
 * values: exactly 0 or 1 is accepted and anything else is malformed input. */
bool convert_bool(std::uint8_t value, bool& result) noexcept
{
    if (value > 1U) return false;
    result = value != 0U;
    return true;
}

AzEl convert_az_el(const ams_mel_ir_az_el_v1& value) noexcept
{
    AzEl result;
    result.az = value.azimuth_rad;
    result.el = value.elevation_rad;
    return result;
}

} // namespace

namespace ams_mel_system_track_data {

bool valid_flags(const ams_mel_ir_system_track_data_response_v1& input) noexcept
{
    bool ignored = false;
    return convert_bool(input.az_el_valid, ignored) && convert_bool(input.range_valid, ignored);
}

/* Builds the complete upstream SystemTrackDataResponse (@Optional) through the
 * published setters only; no provider object layout is assumed. Every published
 * setter is called exactly once with the corresponding C field. The system time
 * stays signed nanoseconds and no range, rate, error, or angle value is clamped
 * or normalized, because the upstream setters perform no such validation. */
bool build_response(const ams_mel_ir_system_track_data_response_v1& input,
                    irmel::SystemTrackDataResponse& output)
{
    bool az_el_valid = false;
    bool range_valid = false;
    if (!convert_bool(input.az_el_valid, az_el_valid) ||
        !convert_bool(input.range_valid, range_valid)) return false;
    irmel::SystemTrackDataResponse value;
    value.setCommandID(input.command_id);
    value.setSystemTime(std::chrono::nanoseconds{input.system_time_ns});
    value.setRequestId(input.request_id);
    value.setTrackId(input.track_id);
    value.setRange(input.range_m);
    value.setRangeRate(input.range_rate_mps);
    value.setRangeError(input.range_error_m);
    value.setRangeRateError(input.range_rate_error_mps);
    value.setAzElValid(az_el_valid);
    value.setInertialAzEl(convert_az_el(input.inertial_az_el));
    value.setAzElError(convert_az_el(input.az_el_error));
    value.setRangeValid(range_valid);
    output = std::move(value);
    return true;
}

/* Upstream CommandState tops out at Cancelled and CannotComply at
 * Alignment_Maneuver; neither declares a MaxExclusive value, so anything above
 * is malformed. The description is copied into request-owned storage only
 * after it is proven valid UTF-8 without an embedded NUL. */
bool copy_command_status(const irmel::CommandStatus& source,
                         ams_mel_ir_command_status_v1& destination,
                         std::string& owned_text)
{
    const auto state = static_cast<std::uint32_t>(source.getState());
    const auto reason = static_cast<std::uint32_t>(source.getReasonID());
    if (state > AMS_MEL_IR_COMMAND_CANCELLED) return false;
    if (reason > AMS_MEL_IR_CANNOT_COMPLY_ALIGNMENT_MANEUVER) return false;
    const std::string& description = source.getReasonDescription();
    if (!ams_mel::internal::valid_utf8(description)) return false;
    owned_text = description;
    destination.command_id = source.getCommandID();
    destination.state = state;
    destination.reason_id = reason;
    return true;
}

} // namespace ams_mel_system_track_data
