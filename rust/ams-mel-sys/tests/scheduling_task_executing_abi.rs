use ams_mel_sys::*;
use std::mem::{align_of, offset_of, size_of};
use std::path::Path;
use std::process::Command;

/// IRMEL-ADA-013: private raw Scheduling TaskExecutingRep mirror only; exact
/// signatures, every layout value and every Task/TaskCompStatus constant
/// compared with the compiled C11 probe. No safe Rust reception exists.
#[test]
fn scheduling_task_executing_abi() {
    type Text = *mut std::ffi::c_char;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrScheduling,
        usize,
        *mut *mut AmsMelIrSchedulingRepMetadata,
        *mut AmsMelIrReturn,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_task_executing_open;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrSchedulingRepMetadata,
        u32,
        *mut *mut AmsMelIrSchedulingRepEvent,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_task_executing_receive;
    let _: unsafe extern "C" fn(
        *const AmsMelIrSchedulingRepMetadata,
        *mut AmsMelIrC2MetadataCountersV1,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_task_executing_get_counters;
    let _: unsafe extern "C" fn(
        *mut *mut AmsMelIrSchedulingRepMetadata,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_task_executing_close;
    let _: unsafe extern "C" fn(
        *const AmsMelIrSchedulingRepEvent,
        *mut *const AmsMelIrTaskExecutingRepV1,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_task_executing_event_view;
    let _: unsafe extern "C" fn(
        *mut *mut AmsMelIrSchedulingRepEvent,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_task_executing_event_close;
    let value = AmsMelIrTaskExecutingRepV1 {
        command_id: u32::MAX,
        activity_id: 0x8000_0000,
        request_id: 1,
        task: AMS_MEL_IR_TASK_RESERVED_6,
        estimated_duration_ns: i64::MIN,
        track_id: i32::MIN,
        previous_activity_id: 2,
        previous_request_id: 3,
        previous_task: AMS_MEL_IR_TASK_IRST_UPDATE,
        previous_task_status: AMS_MEL_IR_TASK_COMP_STATUS_SKIPPED,
    };
    assert_eq!(value.estimated_duration_ns, i64::MIN);
    assert_eq!(value.track_id, i32::MIN);
    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("../..");
    let executable =
        std::env::temp_dir().join(format!("ams-mel-sched-rep-sys-{}", std::process::id()));
    assert!(
        Command::new(std::env::var("CC").unwrap_or_else(|_| "cc".into()))
            .args(["-std=c11", "-Wall", "-Wextra", "-Werror"])
            .arg(format!("-I{}", root.join("native/include").display()))
            .arg(root.join("native/tests/scheduling_task_executing_layout_probe.c"))
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
    type L = AmsMelIrTaskExecutingRepV1;
    assert_eq!(size_of::<L>(), 48);
    let mut expected = vec![
        size_of::<L>(),
        align_of::<L>(),
        offset_of!(L, command_id),
        offset_of!(L, activity_id),
        offset_of!(L, request_id),
        offset_of!(L, task),
        offset_of!(L, estimated_duration_ns),
        offset_of!(L, track_id),
        offset_of!(L, previous_activity_id),
        offset_of!(L, previous_request_id),
        offset_of!(L, previous_task),
        offset_of!(L, previous_task_status),
    ];
    expected.extend(
        [
            AMS_MEL_IR_TASK_NO_TASK,
            AMS_MEL_IR_TASK_ABORT_TASK,
            AMS_MEL_IR_TASK_IRST_CAM_CAL,
            AMS_MEL_IR_TASK_CAM_CAL,
            AMS_MEL_IR_TASK_IRST_BORESIGHT,
            AMS_MEL_IR_TASK_IDLE,
            AMS_MEL_IR_TASK_IRST_SCAN,
            AMS_MEL_IR_TASK_IRST_POSITION,
            AMS_MEL_IR_TASK_IRST_STT,
            AMS_MEL_IR_TASK_IRST_MTT,
            AMS_MEL_IR_TASK_IRST_UPDATE,
            AMS_MEL_IR_TASK_RESERVED_1,
            AMS_MEL_IR_TASK_RESERVED_2,
            AMS_MEL_IR_TASK_RESERVED_3,
            AMS_MEL_IR_TASK_RESERVED_4,
            AMS_MEL_IR_TASK_RESERVED_5,
            AMS_MEL_IR_TASK_RESERVED_6,
            AMS_MEL_IR_TASK_COMP_STATUS_NO_TASK,
            AMS_MEL_IR_TASK_COMP_STATUS_COMPLETE,
            AMS_MEL_IR_TASK_COMP_STATUS_TIMEOUT,
            AMS_MEL_IR_TASK_COMP_STATUS_ABORT,
            AMS_MEL_IR_TASK_COMP_STATUS_SKIPPED,
            AMS_MEL_IR_METADATA_TASK_EXECUTING_REP,
        ]
        .map(|v| v as usize),
    );
    assert_eq!(actual, expected);
}
