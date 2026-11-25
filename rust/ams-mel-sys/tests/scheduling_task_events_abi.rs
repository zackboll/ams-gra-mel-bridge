use ams_mel_sys::*;
use std::mem::{align_of, offset_of, size_of};
use std::path::Path;
use std::process::Command;

#[test]
fn scheduling_task_events_abi() {
    type Text = *mut std::ffi::c_char;
    type Stream = AmsMelIrSchedulingTaskEventsMetadata;
    type Event = AmsMelIrSchedulingTaskEventsEvent;
    type View = AmsMelIrTaskEventsV1;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrScheduling,
        usize,
        *mut *mut Stream,
        *mut AmsMelIrReturn,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_task_events_open;
    let _: unsafe extern "C" fn(
        *mut Stream,
        u32,
        *mut *mut Event,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_task_events_receive;
    let _: unsafe extern "C" fn(
        *const Stream,
        *mut AmsMelIrC2MetadataCountersV1,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_task_events_get_counters;
    let _: unsafe extern "C" fn(*mut *mut Stream, Text, usize, *mut usize) -> AmsMelStatus =
        ams_mel_ir_scheduling_task_events_close;
    let _: unsafe extern "C" fn(
        *const Event,
        *mut *const View,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_task_events_event_view;
    let _: unsafe extern "C" fn(*mut *mut Event, Text, usize, *mut usize) -> AmsMelStatus =
        ams_mel_ir_scheduling_task_events_event_close;

    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("../..");
    let executable =
        std::env::temp_dir().join(format!("ams-mel-sched-events-sys-{}", std::process::id()));
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
            .arg(root.join("native/tests/scheduling_task_events_layout_probe.c"))
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
            offset_of!(View, event_type),
            offset_of!(View, scan_bar),
            offset_of!(View, stop_number),
            offset_of!(View, system_time_ns),
            offset_of!(View, track_id),
            size_of::<*mut Stream>(),
            align_of::<*mut Stream>(),
            size_of::<*mut Event>(),
            align_of::<*mut Event>(),
            AMS_MEL_IR_TASK_EVENT_NOT_SET as usize,
            AMS_MEL_IR_TASK_EVENT_SLEW_START as usize,
            AMS_MEL_IR_TASK_EVENT_SCAN_START as usize,
            AMS_MEL_IR_TASK_EVENT_BREAK_SCAN as usize,
            AMS_MEL_IR_TASK_EVENT_RESERVED_11 as usize,
            AMS_MEL_IR_TASK_EVENT_RESERVED_12 as usize,
            AMS_MEL_IR_TASK_EVENT_ACCEL_START as usize,
            AMS_MEL_IR_TASK_EVENT_ACCEL_COMPLETE as usize,
            AMS_MEL_IR_METADATA_TASK_EVENTS as usize,
        ]
    );
}
