#pragma once

#include <ams_mel/abi.h>
#include <irmel/library/irmel-types/ScanParam.h>
#include <bit>
#include <limits>
#include <type_traits>
#include <utility>

namespace ams_mel::internal {
using ams::iface::irmel::ScanParam;
using ams::iface::irmel::ScanType;
using ams::iface::irmel::CoordFrameRef;
using ams::iface::irmel::DegradationMethod;
static_assert(static_cast<unsigned>(CoordFrameRef::Inertial) == 0U);
static_assert(static_cast<unsigned>(CoordFrameRef::Aircraft) == 1U);
static_assert(static_cast<unsigned>(DegradationMethod::CAPACITY_DEGRADATION) == 0U);
static_assert(static_cast<unsigned>(DegradationMethod::VOLUME_DEGRADATION) == 1U);
static_assert(static_cast<unsigned>(DegradationMethod::RANGE_DEGRADATION) == 2U);
static_assert(static_cast<unsigned>(DegradationMethod::REVISIT_DEGRADATION) == 3U);
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);
static_assert(std::is_same_v<decltype(AzEl{}.az), double>);
static_assert(std::is_same_v<decltype(AzEl{}.el), double>);
static_assert(std::is_same_v<decltype(ScanType{}.getContinuousScan()), std::uint32_t>);
static_assert(std::is_same_v<decltype(ScanType{}.getReturning()), std::uint32_t>);
static_assert(std::is_same_v<decltype(ScanType{}.getAgileScan()), std::uint32_t>);

/* Extracted unchanged from C2: representation domains only, never feasibility. */
inline bool valid_scan(const ams_mel_ir_scan_param_v1& value) noexcept
{
    return value.elevation_defined_with_range_and_altitude <= 1U &&
           value.center_frame_ref_el <= AMS_MEL_IR_COORD_FRAME_AIRCRAFT &&
           value.center_frame_ref_az <= AMS_MEL_IR_COORD_FRAME_AIRCRAFT &&
           value.degradation_method <= AMS_MEL_IR_DEGRADATION_REVISIT;
}

inline ScanParam convert_scan(const ams_mel_ir_scan_param_v1& value)
{
    ScanType type;
    type.setContinuousScan(value.scan_type.continuous_scan);
    type.setReturning(value.scan_type.returning);
    type.setAgileScan(value.scan_type.agile_scan);
    return {value.elevation_defined_with_range_and_altitude != 0U,
            AzEl{value.center_az_rad, value.center_el_rad},
            static_cast<CoordFrameRef>(value.center_frame_ref_el),
            static_cast<CoordFrameRef>(value.center_frame_ref_az),
            value.scan_width_rad, value.scan_height_rad, std::move(type),
            value.scan_id, value.scan_rate_rad_per_second,
            value.preferred_revisit_interval_seconds,
            value.required_revisit_interval_seconds,
            value.max_range_of_interest_m, value.min_range_of_interest_m,
            value.elevation_scan_center_altitude_m,
            value.elevation_scan_center_range_m,
            static_cast<DegradationMethod>(value.degradation_method)};
}

inline bool same_scan_double(double left, double right) noexcept
{ return std::bit_cast<std::uint64_t>(left) == std::bit_cast<std::uint64_t>(right); }

/* Only published getters: including inactive-but-published altitude/range values. */
inline bool matches_scan(const ams_mel_ir_scan_param_v1& input, const ScanParam& value)
{
    const auto center = value.getCenter();
    const auto& type = value.getScanType();
    return value.getIsDefinedWithRangeAndAltitude() ==
            (input.elevation_defined_with_range_and_altitude != 0U) &&
        same_scan_double(center.az, input.center_az_rad) &&
        same_scan_double(center.el, input.center_el_rad) &&
        static_cast<std::uint32_t>(value.getCentFrameRefEl()) == input.center_frame_ref_el &&
        static_cast<std::uint32_t>(value.getCentFrameRefAz()) == input.center_frame_ref_az &&
        same_scan_double(value.getScanWidth(), input.scan_width_rad) &&
        same_scan_double(value.getScanHeight(), input.scan_height_rad) &&
        type.getContinuousScan() == input.scan_type.continuous_scan &&
        type.getReturning() == input.scan_type.returning &&
        type.getAgileScan() == input.scan_type.agile_scan &&
        value.getScanId() == input.scan_id &&
        same_scan_double(value.getScanRate(), input.scan_rate_rad_per_second) &&
        same_scan_double(value.getPreferredRevisitInterval(), input.preferred_revisit_interval_seconds) &&
        same_scan_double(value.getRequiredRevisitInterval(), input.required_revisit_interval_seconds) &&
        value.getMaxRangeOfInterest() == input.max_range_of_interest_m &&
        value.getMinRangeOfInterest() == input.min_range_of_interest_m &&
        value.getElevationScanCenterAltitude() == input.elevation_scan_center_altitude_m &&
        value.getElevationScanCenterRange() == input.elevation_scan_center_range_m &&
        static_cast<std::uint32_t>(value.getDegredationType()) == input.degradation_method;
}
} // namespace ams_mel::internal
