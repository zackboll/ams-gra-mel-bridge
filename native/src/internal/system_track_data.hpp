#pragma once

/* IRMEL-ADA-008: pure, state-free SystemTrackDataResponse/CommandStatus value
 * conversion shared by the two independent upstream overloads
 * TrackChannel::send(SystemTrackDataResponse) and
 * C2Channel::send(SystemTrackDataResponse). Nothing here touches channel
 * state, request ownership, admission or lifecycle accounting. */

#include <ams_mel/abi.h>
#include <irmel/library/irmel-types/CommandStatus.h>
#include <irmel/library/irmel-types/SystemTrackDataResponse.h>

#include <string>

namespace ams_mel_system_track_data {

/* True only when az_el_valid and range_valid are each exactly 0 or 1. Lets
 * callers refuse malformed input before any allocation or provider work. */
bool valid_flags(const ams_mel_ir_system_track_data_response_v1& input) noexcept;

/* Validates both published bool values (exactly 0 or 1, independently) and,
 * only when both are valid, builds the complete upstream response through the
 * twelve published setters, each called exactly once. No value is clamped,
 * normalized, transformed or zeroed when a validity flag is false. Returns
 * false without touching output for an invalid Boolean encoding. Upstream
 * setters are not declared noexcept, so callers keep their existing guard. */
bool build_response(const ams_mel_ir_system_track_data_response_v1& input,
                    ams::iface::irmel::SystemTrackDataResponse& output);

/* Validates a provider CommandStatus completely: CommandState 0..4,
 * CannotComply 0..46 and a UTF-8 reason description without embedded NUL.
 * Only a fully valid value is copied; owned_text receives the description. */
bool copy_command_status(const ams::iface::irmel::CommandStatus& source,
                         ams_mel_ir_command_status_v1& destination,
                         std::string& owned_text);

} // namespace ams_mel_system_track_data
