use ams_mel_sys::*;
use std::mem::{align_of, offset_of, size_of};
use std::path::Path;
use std::process::Command;

/// IRMEL-ADA-009: private raw Image OpticalDistortionMap mirror only; exact
/// signatures and every layout value compared with the compiled C11 probe.
#[test]
fn image_odm_abi() {
    type Text = *mut std::ffi::c_char;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrStream,
        usize,
        *mut *mut AmsMelIrImageOdmMetadata,
        *mut AmsMelIrReturn,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_image_odm_open;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrImageOdmMetadata,
        u32,
        *mut *mut AmsMelIrImageOdmEvent,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_image_odm_receive;
    let _: unsafe extern "C" fn(
        *const AmsMelIrImageOdmMetadata,
        *mut AmsMelIrC2MetadataCountersV1,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_image_odm_get_counters;
    let _: unsafe extern "C" fn(
        *mut *mut AmsMelIrImageOdmMetadata,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_image_odm_close;
    let _: unsafe extern "C" fn(
        *const AmsMelIrImageOdmEvent,
        *mut *const AmsMelIrOpticalDistortionMapV1,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_image_odm_event_view;
    let _: unsafe extern "C" fn(
        *mut *mut AmsMelIrImageOdmEvent,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_image_odm_event_close;
    // The RowCol span element is the one canonical RowCol mirror.
    let span = AmsMelIrRowColSpanV1 {
        data: std::ptr::null::<AmsMelIrRowColV1>(),
        size: 0,
    };
    assert!(span.data.is_null() && span.size == 0);

    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("../..");
    let executable = std::env::temp_dir().join(format!("ams-mel-odm-sys-{}", std::process::id()));
    assert!(
        Command::new(std::env::var("CC").unwrap_or_else(|_| "cc".into()))
            .args(["-std=c11", "-Wall", "-Wextra", "-Werror"])
            .arg(format!("-I{}", root.join("native/include").display()))
            .arg(root.join("native/tests/image_odm_layout_probe.c"))
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
    type F = AmsMelIrForwardRightDownV1;
    type M = AmsMelIrOpticalDistortionMapV1;
    type C = AmsMelIrC2MetadataCountersV1;
    assert_eq!(
        actual,
        vec![
            size_of::<F>(),
            align_of::<F>(),
            offset_of!(F, forward),
            offset_of!(F, right),
            offset_of!(F, down),
            size_of::<AmsMelIrForwardRightDownSpanV1>(),
            offset_of!(AmsMelIrForwardRightDownSpanV1, size),
            size_of::<AmsMelIrRowColSpanV1>(),
            offset_of!(AmsMelIrRowColSpanV1, size),
            size_of::<M>(),
            align_of::<M>(),
            offset_of!(M, width),
            offset_of!(M, height),
            offset_of!(M, right_size),
            offset_of!(M, down_size),
            offset_of!(M, pixels_per_odm),
            offset_of!(M, right_min),
            offset_of!(M, right_max),
            offset_of!(M, down_min),
            offset_of!(M, down_max),
            offset_of!(M, frd),
            offset_of!(M, row_col),
            size_of::<AmsMelIrRowColV1>(),
            offset_of!(AmsMelIrRowColV1, column),
            size_of::<C>(),
            offset_of!(C, events_dropped_queue_full),
            offset_of!(C, malformed_or_unsupported),
        ]
    );
}
