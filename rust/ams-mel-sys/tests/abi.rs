use std::env;
use std::mem::{align_of, offset_of, size_of};
use std::path::{Path, PathBuf};
use std::process::Command;

use ams_mel_sys::*;

macro_rules! layout {
    ($values:expr, $type:ty, $($field:ident),+ $(,)?) => {{
        $values.push(size_of::<$type>());
        $values.push(align_of::<$type>());
        $($values.push(offset_of!($type, $field));)+
    }};
}

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
        "{}",
        String::from_utf8_lossy(&compile.stderr)
    );
    let probe = Command::new(&output)
        .output()
        .expect("C ABI probe must run");
    let _ = std::fs::remove_file(&output);
    assert!(
        probe.status.success(),
        "{}",
        String::from_utf8_lossy(&probe.stderr)
    );
    let actual: Vec<usize> = String::from_utf8(probe.stdout)
        .expect("probe output is UTF-8")
        .split_whitespace()
        .map(|value| value.parse().expect("numeric probe value"))
        .collect();

    let mut expected = vec![
        AMS_MEL_OK as usize,
        AMS_MEL_INVALID_ARGUMENT as usize,
        AMS_MEL_LIBRARY_LOAD_FAILED as usize,
        AMS_MEL_SYMBOL_NOT_FOUND as usize,
        AMS_MEL_FACTORY_FAILED as usize,
        AMS_MEL_INITIALIZATION_FAILED as usize,
        AMS_MEL_PROVIDER_EXCEPTION as usize,
        AMS_MEL_BUFFER_TOO_SMALL as usize,
        AMS_MEL_INTERNAL_ERROR as usize,
        AMS_MEL_TIMEOUT as usize,
        AMS_MEL_STREAM_STOPPED as usize,
        AMS_MEL_PROVIDER_FAILED as usize,
        AMS_MEL_IR_CHANNEL_IRST_IMAGE as usize,
        AMS_MEL_IR_PIXEL_MONO as usize,
        AMS_MEL_IR_IMAGE_STARING as usize,
        AMS_MEL_IR_IMAGE_SCANNING as usize,
        AMS_MEL_IR_FLIP_NONE as usize,
        AMS_MEL_IR_FLIP_VERTICAL as usize,
        AMS_MEL_IR_FLIP_HORIZONTAL as usize,
        AMS_MEL_IR_FLIP_BOTH as usize,
    ];
    layout!(expected, AmsMelAbiVersionV1, major, minor);
    layout!(
        expected,
        AmsMelProviderVersionV1,
        api_version,
        library_version,
        vendor,
        vendor_capacity,
        vendor_required,
        description,
        description_capacity,
        description_required
    );
    layout!(expected, AmsMelStringViewV1, data, size);
    layout!(expected, AmsMelUciIdV1, uuid, descriptive_label);
    layout!(
        expected,
        AmsMelComponentLocationV1,
        offset_x_m,
        offset_y_m,
        offset_z_m,
        key,
        system_name
    );
    layout!(
        expected,
        AmsMelIrStreamConfigV1,
        channel_type,
        channel_id,
        platform_id,
        sensor_location,
        buffer_count,
        buffer_size,
        queue_capacity
    );
    layout!(
        expected,
        AmsMelIrFrameV1,
        system_time_ns,
        integration_time_ns,
        width,
        height,
        bits_per_pixel,
        number_of_bands,
        horizontal_fov_rad,
        vertical_fov_rad,
        pixel_format,
        frame_id,
        subframe_id,
        subframe_total,
        image_type,
        image_flip,
        image_flags,
        dither_row,
        dither_column,
        row_offset,
        column_offset,
        band_index,
        reserved,
        pixels,
        pixel_capacity,
        pixel_required
    );
    layout!(
        expected,
        AmsMelIrStreamCountersV1,
        frames_received,
        frames_dropped_queue_full,
        malformed_or_unsupported_frames
    );
    expected.extend([
        AMS_MEL_OK as usize,
        AMS_MEL_ABI_VERSION_MAJOR as usize,
        AMS_MEL_ABI_VERSION_MINOR as usize,
    ]);
    assert_eq!(actual, expected);
}

fn absolute(path: &Path) -> PathBuf {
    if path.is_absolute() {
        path.to_owned()
    } else {
        env::current_dir().expect("current directory").join(path)
    }
}
