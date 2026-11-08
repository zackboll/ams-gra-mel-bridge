use ams_mel_sys::*;
use std::mem::{align_of, offset_of, size_of};
use std::path::Path;
use std::process::Command;

/// IRMEL-ADA-008: private raw C2 SystemTrackDataResponse mirror only.
#[test]
fn c2_system_response_abi() {
    let _: unsafe extern "C" fn(
        *mut AmsMelIrC2,
        *const AmsMelIrSystemTrackDataResponseV1,
        *mut *mut AmsMelIrC2SystemResponseRequest,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_c2_submit_system_track_data_response;
    let _: unsafe extern "C" fn(
        *const AmsMelIrC2SystemResponseRequest,
        u32,
        *mut AmsMelIrC2SystemResponseResultV1,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_c2_system_response_request_wait;
    let _: unsafe extern "C" fn(
        *mut *mut AmsMelIrC2SystemResponseRequest,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_c2_system_response_request_close;
    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("../..");
    let executable = std::env::temp_dir().join(format!("ams-mel-c2-sys-{}", std::process::id()));
    assert!(
        Command::new(std::env::var("CC").unwrap_or_else(|_| "cc".into()))
            .args(["-std=c11", "-Wall", "-Wextra", "-Werror"])
            .arg(format!("-I{}", root.join("native/include").display()))
            .arg(root.join("native/tests/c2_system_response_layout_probe.c"))
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
    type R = AmsMelIrSystemTrackDataResponseV1;
    type C = AmsMelIrC2SystemResponseResultV1;
    assert_eq!(
        actual,
        vec![
            size_of::<R>(),
            align_of::<R>(),
            offset_of!(R, system_time_ns),
            offset_of!(R, command_id),
            offset_of!(R, request_id),
            offset_of!(R, track_id),
            offset_of!(R, range_m),
            offset_of!(R, range_rate_mps),
            offset_of!(R, range_error_m),
            offset_of!(R, range_rate_error_mps),
            offset_of!(R, az_el_valid),
            offset_of!(R, range_valid),
            offset_of!(R, inertial_az_el),
            offset_of!(R, az_el_error),
            size_of::<C>(),
            align_of::<C>(),
            offset_of!(C, status),
            offset_of!(C, error_code),
            size_of::<AmsMelIrCommandStatusV1>(),
            offset_of!(AmsMelIrCommandStatusV1, reason_description),
        ]
    );
}
