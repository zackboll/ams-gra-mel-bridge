use ams_mel_sys::*;
use std::mem::{align_of, offset_of, size_of};
use std::path::Path;
use std::process::Command;

/// IRMEL-ADA-012: private raw Scheduling ExecuteTaskAck mirror only; exact
/// signatures and every layout value compared with the compiled C11 probe.
#[test]
fn scheduling_execute_task_ack_abi() {
    type Text = *mut std::ffi::c_char;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrScheduling,
        usize,
        *mut *mut AmsMelIrSchedulingAckMetadata,
        *mut AmsMelIrReturn,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_execute_task_ack_open;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrSchedulingAckMetadata,
        u32,
        *mut *mut AmsMelIrSchedulingAckEvent,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_execute_task_ack_receive;
    let _: unsafe extern "C" fn(
        *const AmsMelIrSchedulingAckMetadata,
        *mut AmsMelIrC2MetadataCountersV1,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_execute_task_ack_get_counters;
    let _: unsafe extern "C" fn(
        *mut *mut AmsMelIrSchedulingAckMetadata,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_execute_task_ack_close;
    let _: unsafe extern "C" fn(
        *const AmsMelIrSchedulingAckEvent,
        *mut *const AmsMelIrExecuteTaskAckV1,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_execute_task_ack_event_view;
    let _: unsafe extern "C" fn(
        *mut *mut AmsMelIrSchedulingAckEvent,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_execute_task_ack_event_close;
    let value = AmsMelIrExecuteTaskAckV1 {
        command_id: 0x80000001,
        request_id: u32::MAX,
    };
    assert_eq!(value.command_id, 0x80000001);
    assert_eq!(value.request_id, u32::MAX);
    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("../..");
    let executable =
        std::env::temp_dir().join(format!("ams-mel-sched-ack-sys-{}", std::process::id()));
    assert!(
        Command::new(std::env::var("CC").unwrap_or_else(|_| "cc".into()))
            .args(["-std=c11", "-Wall", "-Wextra", "-Werror"])
            .arg(format!("-I{}", root.join("native/include").display()))
            .arg(root.join("native/tests/scheduling_execute_task_ack_layout_probe.c"))
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
    type L = AmsMelIrExecuteTaskAckV1;
    type C = AmsMelIrC2MetadataCountersV1;
    assert_eq!(size_of::<L>(), 8);
    assert_eq!(
        actual,
        vec![
            size_of::<L>(),
            align_of::<L>(),
            offset_of!(L, command_id),
            offset_of!(L, request_id),
            size_of::<C>(),
            offset_of!(C, events_received),
            offset_of!(C, events_dropped_queue_full),
            offset_of!(C, malformed_or_unsupported),
            align_of::<C>(),
            AMS_MEL_IR_METADATA_EXECUTE_TASK_ACK as usize
        ]
    );
}
