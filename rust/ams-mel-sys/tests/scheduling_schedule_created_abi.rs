use ams_mel_sys::*;
use std::mem::{align_of, offset_of, size_of};
use std::path::Path;
use std::process::Command;

#[test]
fn scheduling_schedule_created_abi() {
    type Text = *mut std::ffi::c_char;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrScheduling,
        usize,
        *mut *mut AmsMelIrSchedulingCreatedMetadata,
        *mut AmsMelIrReturn,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_schedule_created_open;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrSchedulingCreatedMetadata,
        u32,
        *mut *mut AmsMelIrSchedulingCreatedEvent,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_schedule_created_receive;
    let _: unsafe extern "C" fn(
        *const AmsMelIrSchedulingCreatedMetadata,
        *mut AmsMelIrC2MetadataCountersV1,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_schedule_created_get_counters;
    let _: unsafe extern "C" fn(
        *mut *mut AmsMelIrSchedulingCreatedMetadata,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_schedule_created_close;
    let _: unsafe extern "C" fn(
        *const AmsMelIrSchedulingCreatedEvent,
        *mut *const AmsMelIrSchedCreatedRepV1,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_schedule_created_event_view;
    let _: unsafe extern "C" fn(
        *mut *mut AmsMelIrSchedulingCreatedEvent,
        Text,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_scheduling_schedule_created_event_close;
    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("../..");
    let executable =
        std::env::temp_dir().join(format!("ams-mel-sched-created-sys-{}", std::process::id()));
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
            .arg(root.join("native/tests/scheduling_schedule_created_layout_probe.c"))
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
    macro_rules! layout {
        ($record:ty; $($field:ident),+) => {
            [size_of::<$record>(), align_of::<$record>(), $(offset_of!($record, $field)),+]
        };
    }
    let mut expected = Vec::new();
    expected
        .extend(layout!(AmsMelIrRangeAzElV1; range_m, azimuth_rad, elevation_rad, range_rate_mps));
    expected.extend(
        layout!(AmsMelIrActivityRecordV1; activity_id, task_id, track_id, position, duration_ns),
    );
    expected.extend(layout!(AmsMelIrActivitySpanV1; data, size));
    expected.extend(
        layout!(AmsMelIrSchedCreatedRepV1; command_id, scan_id, estimated_duration_ns,
                            activity_count, activities),
    );
    assert_eq!(actual, expected);
}
