use ams_mel_sys::*;
use std::mem::{align_of, offset_of, size_of};
use std::path::Path;
use std::process::Command;

#[test]
fn scheduling_scan_performance_abi() {
    type Text = *mut std::ffi::c_char;
    type Stream = AmsMelIrSchedulingScanPerformanceMetadata;
    type Event = AmsMelIrSchedulingScanPerformanceEvent;
    type View = AmsMelIrScanPerformanceReportV1;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrScheduling,
        usize,
        *mut *mut Stream,
        *mut AmsMelIrReturn,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_scan_performance_open;
    let _: unsafe extern "C" fn(
        *mut Stream,
        u32,
        *mut *mut Event,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_scan_performance_receive;
    let _: unsafe extern "C" fn(
        *const Stream,
        *mut AmsMelIrC2MetadataCountersV1,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_scan_performance_get_counters;
    let _: unsafe extern "C" fn(*mut *mut Stream, Text, usize, *mut usize) -> AmsMelStatus =
        ams_mel_ir_scheduling_scan_performance_close;
    let _: unsafe extern "C" fn(
        *const Event,
        *mut *const View,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_scan_performance_event_view;
    let _: unsafe extern "C" fn(*mut *mut Event, Text, usize, *mut usize) -> AmsMelStatus =
        ams_mel_ir_scheduling_scan_performance_event_close;

    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("../..");
    let executable =
        std::env::temp_dir().join(format!("ams-mel-scan-performance-sys-{}", std::process::id()));
    assert!(
        Command::new(std::env::var("CC").unwrap_or_else(|_| "cc".into()))
            .args([
                "-std=c11",
                "-pedantic-errors",
                "-Wall",
                "-Wextra",
                "-Werror"
            ])
            .arg(format!("-I{}", root.join("native/include").display()))
            .arg(root.join("native/tests/scheduling_scan_performance_layout_probe.c"))
            .arg("-o")
            .arg(&executable)
            .status()
            .unwrap()
            .success()
    );
    let output = Command::new(&executable).output().unwrap();
    assert!(output.status.success());
    std::fs::remove_file(executable).unwrap();
    let actual: Vec<usize> = String::from_utf8(output.stdout)
        .unwrap()
        .split_whitespace()
        .map(|v| v.parse().unwrap())
        .collect();
    assert_eq!(
        actual,
        [
            size_of::<View>(),
            align_of::<View>(),
            offset_of!(View, commanded),
            offset_of!(View, achieved),
            size_of::<AmsMelIrScanParamV1>(),
            align_of::<AmsMelIrScanParamV1>(),
            offset_of!(AmsMelIrScanParamV1, elevation_defined_with_range_and_altitude),
            offset_of!(AmsMelIrScanParamV1, center_az_rad),
            offset_of!(AmsMelIrScanParamV1, center_el_rad),
            offset_of!(AmsMelIrScanParamV1, center_frame_ref_el),
            offset_of!(AmsMelIrScanParamV1, center_frame_ref_az),
            offset_of!(AmsMelIrScanParamV1, scan_width_rad),
            offset_of!(AmsMelIrScanParamV1, scan_height_rad),
            offset_of!(AmsMelIrScanParamV1, scan_type),
            offset_of!(AmsMelIrScanParamV1, scan_id),
            offset_of!(AmsMelIrScanParamV1, scan_rate_rad_per_second),
            offset_of!(AmsMelIrScanParamV1, preferred_revisit_interval_seconds),
            offset_of!(AmsMelIrScanParamV1, required_revisit_interval_seconds),
            offset_of!(AmsMelIrScanParamV1, max_range_of_interest_m),
            offset_of!(AmsMelIrScanParamV1, min_range_of_interest_m),
            offset_of!(AmsMelIrScanParamV1, elevation_scan_center_altitude_m),
            offset_of!(AmsMelIrScanParamV1, elevation_scan_center_range_m),
            offset_of!(AmsMelIrScanParamV1, degradation_method),
            size_of::<AmsMelIrScanTypeV1>(),
            align_of::<AmsMelIrScanTypeV1>(),
            offset_of!(AmsMelIrScanTypeV1, continuous_scan),
            offset_of!(AmsMelIrScanTypeV1, returning),
            offset_of!(AmsMelIrScanTypeV1, agile_scan),
            size_of::<*mut Stream>(),
            align_of::<*mut Stream>(),
            size_of::<*mut Event>(),
            align_of::<*mut Event>(),
            AMS_MEL_IR_METADATA_SCAN_PERFORMANCE_REPORT as usize,
        ]
    );
}
