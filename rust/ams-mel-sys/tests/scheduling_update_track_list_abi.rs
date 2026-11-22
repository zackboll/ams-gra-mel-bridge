use ams_mel_sys::*;
use std::mem::{align_of, offset_of, size_of};
use std::path::Path;
use std::process::Command;

#[test]
fn scheduling_update_track_list_abi() {
    type Text = *mut std::ffi::c_char;
    type Stream = AmsMelIrSchedulingUpdateTrackListMetadata;
    type Event = AmsMelIrSchedulingUpdateTrackListEvent;
    type View = AmsMelIrUpdateTrackListResponseV1;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrScheduling,
        usize,
        *mut *mut Stream,
        *mut AmsMelIrReturn,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_update_track_list_open;
    let _: unsafe extern "C" fn(
        *mut Stream,
        u32,
        *mut *mut Event,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_update_track_list_receive;
    let _: unsafe extern "C" fn(
        *const Stream,
        *mut AmsMelIrC2MetadataCountersV1,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_update_track_list_get_counters;
    let _: unsafe extern "C" fn(*mut *mut Stream, Text, usize, *mut usize) -> AmsMelStatus =
        ams_mel_ir_scheduling_update_track_list_close;
    let _: unsafe extern "C" fn(
        *const Event,
        *mut *const View,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_update_track_list_event_view;
    let _: unsafe extern "C" fn(*mut *mut Event, Text, usize, *mut usize) -> AmsMelStatus =
        ams_mel_ir_scheduling_update_track_list_event_close;

    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("../..");
    let executable =
        std::env::temp_dir().join(format!("ams-mel-sched-update-sys-{}", std::process::id()));
    assert!(
        Command::new(std::env::var("CC").unwrap_or_else(|_| "cc".into()))
            .args([
                "-std=c11",
                "-pedantic-errors",
                "-Wall",
                "-Wextra",
                "-Werror"
            ])
            .arg(format!("-I{}", root.join("native/include").display()))
            .arg(root.join("native/tests/scheduling_update_track_list_layout_probe.c"))
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
        [
            size_of::<View>(),
            align_of::<View>(),
            offset_of!(View, command_id),
            offset_of!(View, response),
            size_of::<*mut Stream>(),
            align_of::<*mut Stream>(),
            size_of::<*mut Event>(),
            align_of::<*mut Event>(),
            AMS_MEL_IR_TRACK_LIST_RESPONSE_NONE as usize,
            AMS_MEL_IR_TRACK_LIST_RESPONSE_REJECTED as usize,
            AMS_MEL_IR_TRACK_LIST_RESPONSE_REJECTED_EXPIRED as usize,
            AMS_MEL_IR_TRACK_LIST_RESPONSE_ACCEPTED as usize,
            AMS_MEL_IR_METADATA_UPDATE_TRACK_LIST_RESPONSE as usize,
        ]
    );
}
