//! Shared value conversion for C2 ModeCmd and Scheduling ScanTask.

use crate::{sys, ScanParameters};

pub(super) fn raw_scan_parameters(s: &ScanParameters) -> sys::AmsMelIrScanParamV1 {
    sys::AmsMelIrScanParamV1 {
        elevation_defined_with_range_and_altitude: u32::from(
            s.elevation_defined_with_range_and_altitude,
        ),
        center_az_rad: s.center_az_rad,
        center_el_rad: s.center_el_rad,
        center_frame_ref_el: s.center_frame_ref_el as u32,
        center_frame_ref_az: s.center_frame_ref_az as u32,
        scan_width_rad: s.scan_width_rad,
        scan_height_rad: s.scan_height_rad,
        scan_type: sys::AmsMelIrScanTypeV1 {
            continuous_scan: s.scan_type.continuous_scan,
            returning: s.scan_type.returning,
            agile_scan: s.scan_type.agile_scan,
        },
        scan_id: s.scan_id,
        scan_rate_rad_per_second: s.scan_rate_rad_per_second,
        preferred_revisit_interval_seconds: s.preferred_revisit_interval_seconds,
        required_revisit_interval_seconds: s.required_revisit_interval_seconds,
        max_range_of_interest_m: s.max_range_of_interest_m,
        min_range_of_interest_m: s.min_range_of_interest_m,
        elevation_scan_center_altitude_m: s.elevation_scan_center_altitude_m,
        elevation_scan_center_range_m: s.elevation_scan_center_range_m,
        degradation_method: s.degradation_method as u32,
    }
}
