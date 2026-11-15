use std::env;
use std::path::{Path, PathBuf};

fn main() {
    println!("cargo:rerun-if-env-changed=AMS_MEL_NATIVE_LIB_DIR");

    let manifest_dir = PathBuf::from(
        env::var_os("CARGO_MANIFEST_DIR").expect("Cargo provides CARGO_MANIFEST_DIR"),
    );
    let default_dir = manifest_dir.join("../../native/build/lib");
    let library_dir = env::var_os("AMS_MEL_NATIVE_LIB_DIR")
        .map(PathBuf::from)
        .unwrap_or(default_dir);
    let library_dir = absolute(&library_dir);

    println!("cargo:rustc-link-search=native={}", library_dir.display());
    if cfg!(target_family = "unix") {
        println!("cargo:rustc-link-arg=-Wl,-rpath,{}", library_dir.display());
    }
}

fn absolute(path: &Path) -> PathBuf {
    if path.is_absolute() {
        path.to_owned()
    } else {
        env::current_dir()
            .expect("current directory is available")
            .join(path)
    }
}
