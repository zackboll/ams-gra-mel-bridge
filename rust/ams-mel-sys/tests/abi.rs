use std::env;
use std::mem::{align_of, offset_of, size_of};
use std::path::{Path, PathBuf};
use std::process::Command;

use ams_mel_sys::{
    AmsMelAbiVersionV1, AmsMelProviderVersionV1, AMS_MEL_ABI_VERSION_MAJOR,
    AMS_MEL_ABI_VERSION_MINOR, AMS_MEL_BUFFER_TOO_SMALL, AMS_MEL_FACTORY_FAILED,
    AMS_MEL_INITIALIZATION_FAILED, AMS_MEL_INTERNAL_ERROR, AMS_MEL_INVALID_ARGUMENT,
    AMS_MEL_LIBRARY_LOAD_FAILED, AMS_MEL_OK, AMS_MEL_PROVIDER_EXCEPTION, AMS_MEL_PROVIDER_FAILED,
    AMS_MEL_SYMBOL_NOT_FOUND,
};

#[test]
fn declarations_match_the_c_header() {
    let manifest = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let repository = manifest.join("../..");
    let output = env::temp_dir().join(format!("ams-mel-abi-probe-{}", std::process::id()));
    let library_dir = env::var_os("AMS_MEL_NATIVE_LIB_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| repository.join("native/build/lib"));
    let library_dir = absolute(&library_dir);

    let compile = Command::new(env::var_os("CC").unwrap_or_else(|| "cc".into()))
        .arg("-std=c11")
        .arg("-Wall")
        .arg("-Wextra")
        .arg("-Wpedantic")
        .arg("-Werror")
        .arg(format!("-I{}", repository.join("native/include").display()))
        .arg(manifest.join("tests/abi_probe.c"))
        .arg(format!("-L{}", library_dir.display()))
        .arg("-lams_mel_c")
        .arg(format!("-Wl,-rpath,{}", library_dir.display()))
        .arg("-o")
        .arg(&output)
        .output()
        .expect("C compiler must run");
    assert!(
        compile.status.success(),
        "C ABI probe failed to compile: {}",
        String::from_utf8_lossy(&compile.stderr)
    );

    let probe = Command::new(&output)
        .output()
        .expect("C ABI probe must run");
    let _ = std::fs::remove_file(&output);
    assert!(
        probe.status.success(),
        "C ABI probe failed: {}",
        String::from_utf8_lossy(&probe.stderr)
    );
    let actual = String::from_utf8(probe.stdout).expect("probe output is UTF-8");
    let expected = format!(
        "statuses {AMS_MEL_OK} {AMS_MEL_INVALID_ARGUMENT} {AMS_MEL_LIBRARY_LOAD_FAILED} \
         {AMS_MEL_SYMBOL_NOT_FOUND} {AMS_MEL_FACTORY_FAILED} \
         {AMS_MEL_INITIALIZATION_FAILED} {AMS_MEL_PROVIDER_EXCEPTION} \
         {AMS_MEL_BUFFER_TOO_SMALL} {AMS_MEL_INTERNAL_ERROR} {AMS_MEL_PROVIDER_FAILED}\n\
         abi_layout {} {} {} {}\n\
         provider_layout {} {} {} {} {} {} {} {} {} {}\n\
         abi_result {AMS_MEL_OK} {AMS_MEL_ABI_VERSION_MAJOR} {AMS_MEL_ABI_VERSION_MINOR}\n",
        size_of::<AmsMelAbiVersionV1>(),
        align_of::<AmsMelAbiVersionV1>(),
        offset_of!(AmsMelAbiVersionV1, major),
        offset_of!(AmsMelAbiVersionV1, minor),
        size_of::<AmsMelProviderVersionV1>(),
        align_of::<AmsMelProviderVersionV1>(),
        offset_of!(AmsMelProviderVersionV1, api_version),
        offset_of!(AmsMelProviderVersionV1, library_version),
        offset_of!(AmsMelProviderVersionV1, vendor),
        offset_of!(AmsMelProviderVersionV1, vendor_capacity),
        offset_of!(AmsMelProviderVersionV1, vendor_required),
        offset_of!(AmsMelProviderVersionV1, description),
        offset_of!(AmsMelProviderVersionV1, description_capacity),
        offset_of!(AmsMelProviderVersionV1, description_required),
    );
    assert_eq!(actual, expected);
}

fn absolute(path: &Path) -> PathBuf {
    if path.is_absolute() {
        path.to_owned()
    } else {
        env::current_dir().expect("current directory").join(path)
    }
}
