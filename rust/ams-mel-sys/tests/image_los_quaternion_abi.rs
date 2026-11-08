use ams_mel_sys::*;
use std::mem::{align_of, offset_of, size_of};
use std::path::Path;
use std::process::Command;

/// IRMEL-ADA-010: private raw Image LineOfSightQuaternion mirror only; exact
/// signatures and every layout value compared with the compiled C11 probe.
#[test]
fn image_los_quaternion_abi() {
    type Text = *mut std::ffi::c_char;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrStream,
        usize,
        *mut *mut AmsMelIrImageLosQuaternionMetadata,
        *mut AmsMelIrReturn,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_image_los_quaternion_open;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrImageLosQuaternionMetadata,
        u32,
        *mut *mut AmsMelIrImageLosQuaternionEvent,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_image_los_quaternion_receive;
    let _: unsafe extern "C" fn(
        *const AmsMelIrImageLosQuaternionMetadata,
        *mut AmsMelIrC2MetadataCountersV1,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_image_los_quaternion_get_counters;
    let _: unsafe extern "C" fn(
        *mut *mut AmsMelIrImageLosQuaternionMetadata,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_image_los_quaternion_close;
    let _: unsafe extern "C" fn(
        *const AmsMelIrImageLosQuaternionEvent,
        *mut *const AmsMelIrLineOfSightQuaternionV1,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_image_los_quaternion_event_view;
    let _: unsafe extern "C" fn(
        *mut *mut AmsMelIrImageLosQuaternionEvent,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_image_los_quaternion_event_close;
    // The record reuses the canonical quaternion and FRD mirrors by type.
    let value = AmsMelIrLineOfSightQuaternionV1 {
        system_time_ns: i64::MIN,
        q_xyzw: AmsMelIrQuaternionV1 {
            x: 1.0,
            y: 2.0,
            z: 3.0,
            w: 4.0,
        },
        pqr_rps: AmsMelIrForwardRightDownV1 {
            forward: 5.0,
            right: 6.0,
            down: 7.0,
        },
    };
    assert_eq!(value.system_time_ns, i64::MIN);
    assert_eq!(value.q_xyzw.w, 4.0);
    assert_eq!(value.pqr_rps.down, 7.0);

    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("../..");
    let executable = std::env::temp_dir().join(format!("ams-mel-losq-sys-{}", std::process::id()));
    assert!(
        Command::new(std::env::var("CC").unwrap_or_else(|_| "cc".into()))
            .args(["-std=c11", "-Wall", "-Wextra", "-Werror"])
            .arg(format!("-I{}", root.join("native/include").display()))
            .arg(root.join("native/tests/image_los_quaternion_layout_probe.c"))
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
    type L = AmsMelIrLineOfSightQuaternionV1;
    type Q = AmsMelIrQuaternionV1;
    type F = AmsMelIrForwardRightDownV1;
    type C = AmsMelIrC2MetadataCountersV1;
    assert_eq!(
        actual,
        vec![
            size_of::<L>(),
            align_of::<L>(),
            offset_of!(L, system_time_ns),
            offset_of!(L, q_xyzw),
            offset_of!(L, pqr_rps),
            size_of::<Q>(),
            offset_of!(Q, x),
            offset_of!(Q, y),
            offset_of!(Q, z),
            offset_of!(Q, w),
            size_of::<F>(),
            offset_of!(F, forward),
            offset_of!(F, right),
            offset_of!(F, down),
            size_of::<C>(),
            offset_of!(C, events_received),
            offset_of!(C, events_dropped_queue_full),
            offset_of!(C, malformed_or_unsupported),
            align_of::<C>(),
        ]
    );
}
