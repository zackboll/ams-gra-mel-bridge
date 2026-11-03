use ams_mel_sys::*;
use std::mem::{align_of, offset_of, size_of};
use std::path::Path;
use std::process::Command;
#[test]
fn erase_abi() {
    let _: unsafe extern "C" fn(
        *mut AmsMelIrC2,
        *const AmsMelIrEraseCommandV1,
        *mut *mut AmsMelIrEraseRequest,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_c2_submit_erase;
    let _: unsafe extern "C" fn(
        *const AmsMelIrEraseRequest,
        u32,
        *mut AmsMelIrEraseResultV1,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_erase_request_wait;
    let _: unsafe extern "C" fn(
        *mut *mut AmsMelIrEraseRequest,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_erase_request_close;
    assert_eq!(
        [
            AMS_MEL_IR_ERASE_ZEROIZE,
            AMS_MEL_IR_ERASE_CLASSIFIED_DATA_ERASE,
            AMS_MEL_IR_ERASE_DISABLE,
            AMS_MEL_IR_ERASE_CDE_AND_ZEROIZE
        ],
        [0, 1, 2, 3]
    );
    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("../..");
    let executable = std::env::temp_dir().join(format!("ams-mel-erase-{}", std::process::id()));
    assert!(
        Command::new(std::env::var("CC").unwrap_or_else(|_| "cc".into()))
            .args(["-std=c11", "-Wall", "-Wextra", "-Werror"])
            .arg(format!("-I{}", root.join("native/include").display()))
            .arg(root.join("native/tests/erase_layout_probe.c"))
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
        vec![
            size_of::<AmsMelIrEraseCommandV1>(),
            align_of::<AmsMelIrEraseCommandV1>(),
            offset_of!(AmsMelIrEraseCommandV1, command_id),
            offset_of!(AmsMelIrEraseCommandV1, erase_type),
            size_of::<AmsMelIrEraseResultV1>(),
            align_of::<AmsMelIrEraseResultV1>(),
            offset_of!(AmsMelIrEraseResultV1, erase_type),
            offset_of!(AmsMelIrEraseResultV1, error_code)
        ]
    );
}
