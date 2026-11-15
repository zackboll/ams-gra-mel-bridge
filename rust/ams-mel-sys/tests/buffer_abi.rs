use ams_mel_sys::*;
use std::mem::{align_of, offset_of, size_of};
use std::path::Path;
use std::process::Command;

#[test]
fn buffer_abi() {
    type Text = *mut std::ffi::c_char;
    let _: unsafe extern "C" fn(
        *mut AmsMelSession,
        u64,
        i64,
        *mut *mut AmsMelIrBuffer,
        *mut u32,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_buffer_open;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrBuffer,
        *mut AmsMelIrBufferInfoV1,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_buffer_get_info;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrBuffer,
        *mut u32,
        usize,
        *mut usize,
        *mut u32,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_buffer_get_flags;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrBuffer,
        *const u32,
        usize,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_buffer_set_flags;
    let _: unsafe extern "C" fn(*mut AmsMelIrBuffer, u32, Text, usize, *mut usize) -> AmsMelStatus =
        ams_mel_ir_buffer_add_flag;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrBuffer,
        u64,
        *mut u8,
        usize,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_buffer_read_storage;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrBuffer,
        u64,
        *const u8,
        usize,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_buffer_write_storage;
    let _: unsafe extern "C" fn(*mut *mut AmsMelIrBuffer, Text, usize, *mut usize) -> AmsMelStatus =
        ams_mel_ir_buffer_close;
    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("../..");
    let executable =
        std::env::temp_dir().join(format!("ams-mel-buffer-sys-{}", std::process::id()));
    assert!(Command::new("cc")
        .args([
            "-std=c11",
            "-pedantic-errors",
            "-Wall",
            "-Wextra",
            "-Werror"
        ])
        .arg(format!("-I{}", root.join("native/include").display()))
        .arg(root.join("native/tests/buffer_layout_probe.c"))
        .arg("-o")
        .arg(&executable)
        .status()
        .unwrap()
        .success());
    let output = Command::new(&executable).output().unwrap();
    assert!(output.status.success());
    std::fs::remove_file(executable).unwrap();
    let actual: Vec<usize> = String::from_utf8(output.stdout)
        .unwrap()
        .split_whitespace()
        .map(|s| s.parse().unwrap())
        .collect();
    type S = AmsMelIrBufferInfoV1;
    assert_eq!(
        actual,
        vec![
            size_of::<S>(),
            align_of::<S>(),
            offset_of!(S, storage_size),
            offset_of!(S, initialized_context),
            offset_of!(S, provider_size),
            offset_of!(S, provider_context),
            offset_of!(S, buffer_address_matches),
            offset_of!(S, image_location),
            AMS_MEL_IR_BUFFER_HOST as usize,
            AMS_MEL_IR_BUFFER_GPU as usize,
            AMS_MEL_IR_BUFFER_RDMA as usize
        ]
    );
}
