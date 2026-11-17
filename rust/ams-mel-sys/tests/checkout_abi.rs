use ams_mel_sys::*;
use std::ffi::c_char;
use std::mem::{align_of, offset_of, size_of};
use std::path::Path;
use std::process::Command;

#[test]
fn checkout_abi() {
    let _: unsafe extern "C" fn(
        *mut AmsMelIrStream,
        usize,
        usize,
        *mut *mut AmsMelIrImageCheckout,
        *mut c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_image_checkout_open;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrImageCheckout,
        u32,
        *mut *mut AmsMelIrImageCheckoutLease,
        *mut c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_image_checkout_receive;
    let _: unsafe extern "C" fn(
        *const AmsMelIrImageCheckout,
        *mut AmsMelIrImageCheckoutCountersV1,
        *mut c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_image_checkout_get_counters;
    let _: unsafe extern "C" fn(
        *mut *mut AmsMelIrImageCheckout,
        *mut c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_image_checkout_close;
    let _: unsafe extern "C" fn(
        *mut *mut AmsMelIrImageCheckoutLease,
        *mut u32,
        *mut c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_image_checkout_lease_close;
    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("../..");
    let executable =
        std::env::temp_dir().join(format!("ams-mel-checkout-sys-{}", std::process::id()));
    assert!(Command::new("cc")
        .args([
            "-std=c11",
            "-pedantic-errors",
            "-Wall",
            "-Wextra",
            "-Werror"
        ])
        .arg(format!("-I{}", root.join("native/include").display()))
        .arg(root.join("native/tests/checkout_layout_probe.c"))
        .arg("-o")
        .arg(&executable)
        .status()
        .unwrap()
        .success());
    let output = Command::new(&executable).output().unwrap();
    std::fs::remove_file(executable).unwrap();
    assert!(output.status.success());
    let actual: Vec<usize> = String::from_utf8(output.stdout)
        .unwrap()
        .split_whitespace()
        .map(|s| s.parse().unwrap())
        .collect();
    type S = AmsMelIrImageCheckoutCountersV1;
    assert_eq!(
        actual,
        vec![
            size_of::<S>(),
            align_of::<S>(),
            offset_of!(S, callbacks_received),
            offset_of!(S, checkouts_accepted),
            offset_of!(S, queue_overflow),
            offset_of!(S, callbacks_rejected),
            offset_of!(S, releases_succeeded),
            offset_of!(S, releases_uncertain)
        ]
    );
}
