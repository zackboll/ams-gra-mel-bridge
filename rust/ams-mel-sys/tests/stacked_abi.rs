use ams_mel_sys::*;
use std::mem::{align_of, offset_of, size_of};
use std::path::Path;
use std::process::Command;

/// IRMEL-ADA-016: private raw StackedChannel foundation mirror only; exact
/// signatures and every layout value compared with the compiled C11 probe.
#[test]
fn stacked_abi() {
    type Text = *mut std::ffi::c_char;
    let _: unsafe extern "C" fn(
        *const AmsMelSession,
        *const AmsMelIrStackedConfigV1,
        *mut *mut AmsMelIrStacked,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_stacked_open;
    let _: unsafe extern "C" fn(*mut AmsMelIrStacked, Text, usize, *mut usize) -> AmsMelStatus =
        ams_mel_ir_stacked_enable;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrStacked,
        *mut *mut AmsMelIrChannelCapability,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_stacked_get_capabilities;
    let _: unsafe extern "C" fn(
        *mut *mut AmsMelIrStacked,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_stacked_close;
    let _: unsafe extern "C" fn(
        *const AmsMelIrStacked,
        *mut *mut AmsMelIrChannel,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_channel_from_stacked;
    assert_eq!(AMS_MEL_IR_CHANNEL_STACKED_IMAGE, 6);

    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("../..");
    let executable =
        std::env::temp_dir().join(format!("ams-mel-stacked-sys-{}", std::process::id()));
    assert!(
        Command::new(std::env::var("CC").unwrap_or_else(|_| "cc".into()))
            .args(["-std=c11", "-Wall", "-Wextra", "-Werror"])
            .arg(format!("-I{}", root.join("native/include").display()))
            .arg(root.join("native/tests/stacked_layout_probe.c"))
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
    type S = AmsMelIrStackedConfigV1;
    assert_eq!(
        actual,
        vec![
            size_of::<S>(),
            align_of::<S>(),
            offset_of!(S, channel_id),
            offset_of!(S, channel_type),
            offset_of!(S, platform_id),
            offset_of!(S, sensor_location),
            size_of::<*mut AmsMelIrStacked>(),
            AMS_MEL_IR_CHANNEL_STACKED_IMAGE as usize,
        ]
    );
    // Same convention as the Track configuration it mirrors.
    assert_eq!(size_of::<S>(), size_of::<AmsMelIrTrackConfigV1>());
}
