//! The injected opaque values are never dereferenced or passed to native Close.
use super::*;
use std::{cell::Cell, path::PathBuf};

fn config() -> SchedulingConfig {
    SchedulingConfig::new(
        UciId::new(std::array::from_fn(|i| i as u8), "通道 🛰").unwrap(),
        UciId::new(std::array::from_fn(|i| 0xf0 + i as u8), "plateforme é").unwrap(),
        ComponentLocation::new(1.25, -2.5, -0.0, "位置", "航機").unwrap(),
    )
}

const TASK: SchedulingTaskInfo = SchedulingTaskInfo {
    system_time_ns: i64::MIN,
    request_id: 0xf123_4567,
    action: TaskListAction::Normal,
    precedence_within_priority: u32::MAX,
    priority: 0x8000_0001,
};

const STARE: SchedulingStareTask = SchedulingStareTask {
    command_id: u32::MAX,
    parameters: SchedulingStareParameters {
        center_azimuth_rad: -0.0,
        center_elevation_rad: 1.25,
        elevation_frame: CoordinateFrameReference::Inertial,
        azimuth_frame: CoordinateFrameReference::Aircraft,
        duration_ns: i64::MIN,
    },
    task_info: TASK,
};

fn scan_task() -> SchedulingScanTask {
    SchedulingScanTask {
        command_id: u32::MAX,
        parameters: ScanParameters::default(),
        task_info: TASK,
    }
}

// The compiled mock stores the last scan arguments in one process-wide slot.
static SCAN_MOCK: std::sync::Mutex<()> = std::sync::Mutex::new(());

#[test]
fn scan_exact_layout_and_null_success() {
    type Raw = sys::AmsMelIrScanTaskV1;
    if cfg!(all(target_pointer_width = "64", not(target_os = "windows"))) {
        assert_eq!(mem::size_of::<Raw>(), 144);
        assert_eq!(mem::align_of::<Raw>(), 8);
        assert_eq!(mem::offset_of!(Raw, scan_parameters), 0);
        assert_eq!(mem::offset_of!(Raw, task_info), 112);
        assert_eq!(mem::offset_of!(Raw, command_id), 136);
    }
    let mut task = scan_task();
    task.parameters.center_az_rad = -0.0;
    task.parameters.scan_type.continuous_scan = u32::MAX;
    let raw = raw_scan_task(&task);
    assert_eq!(raw.command_id, u32::MAX);
    assert_eq!(
        raw.scan_parameters.center_az_rad.to_bits(),
        (-0.0_f64).to_bits()
    );
    assert_eq!(raw.scan_parameters.center_frame_ref_el, 0);
    assert_eq!(raw.scan_parameters.center_frame_ref_az, 1);
    assert_eq!(raw.scan_parameters.scan_type.continuous_scan, u32::MAX);
    assert_eq!(raw.task_info.system_time_ns, i64::MIN);
    assert_eq!(raw.task_info.request_id, 0xf123_4567);

    let mut calls = 0;
    let error = submit_with("ScanTask", |out, d, _, r| {
        calls += 1;
        assert!(out.is_null());
        // SAFETY: writable diagnostic buffer and length slot.
        unsafe {
            *d = 0;
            *r = 1;
        }
        sys::AMS_MEL_OK
    })
    .unwrap_err();
    assert_eq!(calls, 1);
    assert_eq!(error.kind(), &ErrorKind::ProtocolInconsistency);
    assert_eq!(
        error.diagnostic(),
        Some("Scheduling ScanTask succeeded without a request owner")
    );
}

#[test]
fn scan_native_validation_diagnostic() {
    let _fixture = SCAN_MOCK.lock().unwrap();
    let session = Session::open(mock_provider(), "sched-scan-success", "").unwrap();
    let mut channel = session.open_scheduling_channel(&config()).unwrap();
    channel.enable().unwrap();
    // These invalid discriminators cannot be constructed with the public Rust types.
    for (selector, el, az, degradation, action) in [
        (2, 0, 1, 0, 0),
        (0, 2, 1, 0, 0),
        (0, 0, 2, 0, 0),
        (0, 0, 1, 4, 0),
        (0, 0, 1, 0, 4),
    ] {
        let mut raw = raw_scan_task(&scan_task());
        raw.scan_parameters
            .elevation_defined_with_range_and_altitude = selector;
        raw.scan_parameters.center_frame_ref_el = el;
        raw.scan_parameters.center_frame_ref_az = az;
        raw.scan_parameters.degradation_method = degradation;
        raw.task_info.action = action;
        let error = submit_with("ScanTask", |out, d, c, r| {
            assert!(out.is_null());
            // SAFETY: live channel, initialized input and writable outputs.
            unsafe { sys::ams_mel_ir_scheduling_exec_scan(channel.raw, &raw, out, d, c, r) }
        })
        .unwrap_err();
        assert_eq!(error.kind(), &ErrorKind::InvalidArgument);
        assert_eq!(error.diagnostic(), Some("invalid Scheduling ScanTask"));
    }
    channel.close().unwrap();
    session.close().unwrap();
}

#[test]
fn scan_contradictory_owner_and_diagnostics() {
    let _fixture = SCAN_MOCK.lock().unwrap();
    contradictory_owner_and_diagnostics("ScanTask", "sched-scan-success", |channel| {
        channel.exec_scan(&scan_task())
    });
}

#[test]
fn scan_is_independent_of_actual_matching_ack_delivery() {
    let _fixture = SCAN_MOCK.lock().unwrap();
    let task = scan_task();
    independent_ack(
        "sched-scan-delayed",
        task.command_id,
        task.task_info.request_id,
        17,
        |channel| channel.exec_scan(&task),
    );
}

#[test]
fn abort_exact_fields_and_null_success() {
    for (index, (action, time, request_id, precedence, priority)) in [
        (
            TaskListAction::Normal,
            i64::MIN,
            0xf123_4567,
            u32::MAX,
            0x8000_0001,
        ),
        (TaskListAction::Next, i64::MAX, u32::MAX, 0x8000_0002, 0),
        (TaskListAction::Immediate, -42, 0x8000_0000, 37, u32::MAX),
        (TaskListAction::Abort, 0, 0, 0, 19),
    ]
    .into_iter()
    .enumerate()
    {
        let info = SchedulingTaskInfo {
            system_time_ns: time,
            request_id,
            action,
            precedence_within_priority: precedence,
            priority,
        };
        let mut calls = 0;
        let err = submit_with("AbortTask", |out, d, _, r| {
            let raw = raw_task_info(&info);
            calls += 1;
            assert!(out.is_null());
            assert_eq!(raw.system_time_ns, time);
            assert_eq!(raw.request_id, request_id);
            assert_eq!(raw.action, index as u32);
            assert_eq!(raw.precedence_within_priority, precedence);
            assert_eq!(raw.priority, priority);
            // SAFETY: writable diagnostic buffer and size out slot.
            unsafe {
                *d = 0;
                *r = 1;
            }
            sys::AMS_MEL_OK
        })
        .unwrap_err();
        assert_eq!(calls, 1);
        assert_eq!(err.kind(), &ErrorKind::ProtocolInconsistency);
        assert_eq!(
            err.diagnostic(),
            Some("Scheduling AbortTask succeeded without a request owner")
        );
    }
}

#[test]
fn stare_exact_layout_bits_and_null_success() {
    use CoordinateFrameReference::{Aircraft, Inertial};
    use TaskListAction::{Abort, Immediate, Next, Normal};

    if cfg!(all(target_pointer_width = "64", not(target_os = "windows"))) {
        assert_eq!(mem::size_of::<sys::AmsMelIrStareTaskV1>(), 64);
        assert_eq!(mem::align_of::<sys::AmsMelIrStareTaskV1>(), 8);
        assert_eq!(mem::offset_of!(sys::AmsMelIrStareTaskV1, task_info), 40);
    }
    for (index, (el, az, duration, action, azimuth, elevation)) in [
        (Inertial, Aircraft, i64::MIN, Normal, -0.0, 0.0),
        (Aircraft, Inertial, i64::MAX, Next, 0.0, -0.0),
        (Inertial, Inertial, -1, Immediate, -1.23456789012345, 2.75),
        (Aircraft, Aircraft, 0, Abort, 1.23456789012345, -2.75),
    ]
    .into_iter()
    .enumerate()
    {
        let task = SchedulingStareTask {
            command_id: u32::MAX - index as u32,
            parameters: SchedulingStareParameters {
                center_azimuth_rad: azimuth,
                center_elevation_rad: elevation,
                elevation_frame: el,
                azimuth_frame: az,
                duration_ns: duration,
            },
            task_info: SchedulingTaskInfo { action, ..TASK },
        };
        let mut calls = 0;
        let error = submit_with("StareTask", |out, d, _, r| {
            calls += 1;
            assert!(out.is_null());
            let raw = raw_stare_task(&task);
            assert_eq!(raw.center_azimuth_rad.to_bits(), azimuth.to_bits());
            assert_eq!(raw.center_elevation_rad.to_bits(), elevation.to_bits());
            assert_eq!(raw.duration_ns, duration);
            assert_eq!(raw.elevation_frame, [0, 1, 0, 1][index]);
            assert_eq!(raw.azimuth_frame, [1, 0, 0, 1][index]);
            assert_eq!(raw.command_id, task.command_id);
            assert_eq!(raw.task_info.system_time_ns, i64::MIN);
            assert_eq!(raw.task_info.request_id, 0xf123_4567);
            assert_eq!(raw.task_info.action, index as u32);
            assert_eq!(raw.task_info.precedence_within_priority, u32::MAX);
            assert_eq!(raw.task_info.priority, 0x8000_0001);
            // SAFETY: writable diagnostic and required-size slots.
            unsafe {
                *d = 0;
                *r = 1;
            }
            sys::AMS_MEL_OK
        })
        .unwrap_err();
        assert_eq!(calls, 1);
        assert_eq!(error.kind(), &ErrorKind::ProtocolInconsistency);
        assert_eq!(
            error.diagnostic(),
            Some("Scheduling StareTask succeeded without a request owner")
        );
    }
}

#[test]
fn stare_native_validation_diagnostic() {
    let session = Session::open(mock_provider(), "sched-stare-success", "").unwrap();
    let mut channel = session.open_scheduling_channel(&config()).unwrap();
    channel.enable().unwrap();
    // A bad discriminator cannot be constructed by safe clients. Inject it
    // into the ABI record, not into a Rust enum, to check native validation.
    for (el, az, action) in [(2, 1, 0), (0, u32::MAX, 0), (0, 1, u32::MAX)] {
        let mut raw = raw_stare_task(&STARE);
        raw.elevation_frame = el;
        raw.azimuth_frame = az;
        raw.task_info.action = action;
        let error = submit_with("StareTask", |out, d, c, r| {
            assert!(out.is_null());
            // SAFETY: live channel and a valid C record with invalid scalar
            // values; output and diagnostic storage are valid through the call.
            unsafe { sys::ams_mel_ir_scheduling_exec_stare(channel.raw, &raw, out, d, c, r) }
        })
        .unwrap_err();
        assert_eq!(error.kind(), &ErrorKind::InvalidArgument);
        assert_eq!(error.diagnostic(), Some("invalid Scheduling StareTask"));
    }
    channel.close().unwrap();
    session.close().unwrap();
}

fn mock_provider() -> PathBuf {
    std::env::var_os("AMS_MEL_TEST_PROVIDER_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| {
            Path::new(env!("CARGO_MANIFEST_DIR")).join("../../native/build-tests/test-providers")
        })
        .join("libmock_ir_provider.so")
}

#[test]
fn abort_contradictory_owner_and_diagnostics() {
    contradictory_owner_and_diagnostics("AbortTask", "sched-abort-success", |channel| {
        channel.exec_abort(&TASK)
    });
}

#[test]
fn stare_contradictory_owner_and_diagnostics() {
    contradictory_owner_and_diagnostics("StareTask", "sched-stare-success", |channel| {
        channel.exec_stare(&STARE)
    });
}

fn contradictory_owner_and_diagnostics(
    operation: &str,
    scenario: &str,
    exec: impl Fn(&mut SchedulingChannel) -> Result<ReturnRequest, Error>,
) {
    let session = Session::open(mock_provider(), scenario, "").unwrap();
    let mut channel = session.open_scheduling_channel(&config()).unwrap();
    channel.enable().unwrap();
    for (status, bytes, size, kind) in [
        (
            sys::AMS_MEL_PROVIDER_EXCEPTION,
            b"exec threw\0".as_slice(),
            11,
            ErrorKind::ProviderException,
        ),
        (
            sys::AMS_MEL_OK,
            b"unexpected\0".as_slice(),
            11,
            ErrorKind::ProtocolInconsistency,
        ),
        (
            sys::AMS_MEL_OK,
            b"".as_slice(),
            0,
            ErrorKind::ProtocolInconsistency,
        ),
        (
            sys::AMS_MEL_OK,
            b"".as_slice(),
            DIAGNOSTIC_CAPACITY + 1,
            ErrorKind::ProtocolInconsistency,
        ),
        (
            sys::AMS_MEL_PROVIDER_FAILED,
            b"unterminated".as_slice(),
            12,
            ErrorKind::ProtocolInconsistency,
        ),
        (
            sys::AMS_MEL_PROVIDER_FAILED,
            b"a\0b\0".as_slice(),
            4,
            ErrorKind::ProtocolInconsistency,
        ),
        (
            sys::AMS_MEL_PROVIDER_FAILED,
            b"\xff\0".as_slice(),
            2,
            ErrorKind::InvalidUtf8,
        ),
    ] {
        let mut real = exec(&mut channel).unwrap();
        assert_eq!(
            real.wait(5_000).unwrap(),
            ReturnResult::Completed {
                value: CommandReturn::Success
            }
        );
        let mut calls = 0;
        let err = submit_with(operation, |out, d, c, r| {
            calls += 1;
            assert!(out.is_null());
            // Transfer a real unique handle to exercise ReturnRequest Drop;
            // never send an invented or already released pointer to native Close.
            *out = mem::replace(&mut real.raw, ptr::null_mut());
            assert!(bytes.len() <= c);
            // SAFETY: writable diagnostic storage for this call.
            unsafe {
                ptr::copy_nonoverlapping(bytes.as_ptr(), d.cast(), bytes.len());
                *r = size;
            }
            status
        })
        .unwrap_err();
        assert_eq!(calls, 1);
        assert_eq!(err.kind(), &kind);
        if kind == ErrorKind::ProviderException {
            assert_eq!(err.diagnostic(), Some("exec threw"));
        }
    }
    channel.close().unwrap();
    session.close().unwrap();
}

#[test]
fn shared_task_return_boundary_for_bad_pointer_and_long_utf8_rejection() {
    // The existing Scheduling mock does not return BadPointer or non-ASCII
    // rejection text. The facade rejects unknown MEL codes before this ABI.
    for (status, value, code, message, expected) in [
        (
            sys::AMS_MEL_OK,
            sys::AMS_MEL_IR_RETURN_BAD_POINTER,
            sys::AMS_MEL_ERROR_NONE,
            "".to_owned(),
            ReturnResult::Completed {
                value: CommandReturn::BadPointer,
            },
        ),
        (
            sys::AMS_MEL_COMMAND_REJECTED,
            0,
            sys::AMS_MEL_ERROR_UNSUPPORTED,
            "rejected: é🛰".repeat(150),
            ReturnResult::Rejected {
                code: MelErrorCode::Unsupported,
                description: "rejected: é🛰".repeat(150),
            },
        ),
        (
            sys::AMS_MEL_COMMAND_REJECTED,
            0,
            u32::MAX,
            "rejected: é🛰".repeat(150),
            ReturnResult::Rejected {
                code: MelErrorCode::Unknown(u32::MAX),
                description: "rejected: é🛰".repeat(150),
            },
        ),
    ] {
        let calls = Cell::new(0);
        let result = wait_for_return_with(100, |timeout, result, d, capacity, required| {
            assert_eq!(timeout, if calls.get() == 0 { 100 } else { 0 });
            calls.set(calls.get() + 1);
            *result = sys::AmsMelIrReturnResultV1 {
                value,
                error_code: code,
            };
            *required = message.len() + 1;
            if *required <= capacity {
                // SAFETY: the complete message and terminator fit.
                unsafe {
                    ptr::copy_nonoverlapping(message.as_ptr(), d.cast(), message.len());
                    *d.add(message.len()) = 0;
                }
            }
            status
        })
        .unwrap();
        assert_eq!(result, expected);
        assert_eq!(calls.get(), if message.is_empty() { 1 } else { 2 });
    }
}

#[test]
fn abort_is_independent_of_actual_ack_delivery() {
    independent_ack("sched-abort-delayed", 17, TASK.request_id, 15, |channel| {
        channel.exec_abort(&TASK)
    });
}

#[test]
fn stare_is_independent_of_actual_matching_ack_delivery() {
    independent_ack(
        "sched-stare-delayed",
        STARE.command_id,
        STARE.task_info.request_id,
        16,
        |channel| channel.exec_stare(&STARE),
    );
}

fn independent_ack(
    scenario: &str,
    command_id: u32,
    request_id: u32,
    family: u32,
    exec: impl FnOnce(&mut SchedulingChannel) -> Result<ReturnRequest, Error>,
) {
    use std::ffi::{c_void, CString};
    use std::sync::Mutex;

    // The mock has one process-wide ACK callback slot for both scenarios.
    static ACK_FIXTURE: Mutex<()> = Mutex::new(());
    let _fixture = ACK_FIXTURE.lock().unwrap();

    unsafe extern "C" {
        fn dlopen(path: *const c_char, flags: i32) -> *mut c_void;
        fn dlsym(handle: *mut c_void, name: *const c_char) -> *mut c_void;
        fn dlclose(handle: *mut c_void) -> i32;
    }
    struct Pin(*mut c_void);
    impl Drop for Pin {
        fn drop(&mut self) {
            // SAFETY: releases one test-only loader reference.
            unsafe { dlclose(self.0) };
        }
    }

    let provider = mock_provider();
    let session = Session::open(&provider, scenario, "").unwrap();
    let mut channel = session.open_scheduling_channel(&config()).unwrap();
    channel.enable().unwrap();
    let mut request = exec(&mut channel).unwrap();
    assert_eq!(request.wait(0).unwrap_err().kind(), &ErrorKind::Timeout);
    let path = CString::new(provider.as_os_str().as_encoded_bytes()).unwrap();
    let mut ack = ptr::null_mut();
    let mut event = ptr::null_mut();
    // SAFETY: all raw owners and inputs are live and used serially. ACK is
    // registered deliberately, for this test only, on the same channel; the
    // fixture retains the provider through delivery and release calls.
    unsafe {
        let pin = Pin(dlopen(path.as_ptr(), 2 | 4)); // RTLD_NOW | RTLD_NOLOAD
        assert!(!pin.0.is_null());
        let deliver = dlsym(pin.0, c"mock_scheduling_ack_deliver".as_ptr());
        let release = dlsym(pin.0, c"mock_completion_release_family".as_ptr());
        assert!(!deliver.is_null() && !release.is_null());
        let deliver: unsafe extern "C" fn(u32, u32, u32) -> i32 = mem::transmute(deliver);
        let release: unsafe extern "C" fn(u32, u32) -> i32 = mem::transmute(release);
        let d = ptr::null_mut();
        let r = ptr::null_mut();
        assert_eq!(
            deliver(command_id, request_id, 0),
            0,
            "no implicit ACK registration"
        );
        assert_eq!(
            sys::ams_mel_ir_scheduling_execute_task_ack_open(
                channel.raw,
                2,
                &mut ack,
                ptr::null_mut(),
                d,
                0,
                r
            ),
            sys::AMS_MEL_OK
        );
        assert_eq!(
            sys::ams_mel_ir_scheduling_execute_task_ack_receive(ack, 0, &mut event, d, 0, r),
            sys::AMS_MEL_TIMEOUT
        );
        assert_eq!(deliver(command_id, request_id, 0), 1);
        assert_eq!(request.wait(0).unwrap_err().kind(), &ErrorKind::Timeout);
        assert_eq!(
            sys::ams_mel_ir_scheduling_execute_task_ack_receive(ack, 0, &mut event, d, 0, r),
            sys::AMS_MEL_OK
        );
        let mut snapshot = ptr::null();
        assert_eq!(
            sys::ams_mel_ir_scheduling_execute_task_ack_event_view(event, &mut snapshot, d, 0, r),
            sys::AMS_MEL_OK
        );
        let snapshot = snapshot.as_ref().unwrap();
        assert_eq!(
            (snapshot.command_id, snapshot.request_id),
            (command_id, request_id)
        );
        assert_eq!(
            sys::ams_mel_ir_scheduling_execute_task_ack_event_close(&mut event, d, 0, r),
            sys::AMS_MEL_OK
        );
        assert_eq!(
            sys::ams_mel_ir_scheduling_execute_task_ack_close(&mut ack, d, 0, r),
            sys::AMS_MEL_OK
        );
        assert_eq!(release(family, 1), 1);
    }
    assert_eq!(
        request.wait(5_000).unwrap(),
        ReturnResult::Completed {
            value: CommandReturn::Success
        }
    );
    request.close().unwrap();
    channel.close().unwrap();
    session.close().unwrap();
}

#[test]
fn exact_input_and_null_success() {
    let calls = Cell::new(0);
    let error = open_with(
        &config(),
        |raw, out, d, _, r| {
            calls.set(calls.get() + 1);
            assert_eq!(raw.channel_type, 3);
            assert_eq!(raw.channel_id.uuid, std::array::from_fn(|i| i as u8));
            assert_eq!(
                raw.platform_id.uuid,
                std::array::from_fn(|i| 0xf0 + i as u8)
            );
            assert_eq!(raw.sensor_location.offset_x_m, 1.25);
            assert_eq!(raw.sensor_location.offset_y_m, -2.5);
            assert_eq!(
                raw.sensor_location.offset_z_m.to_bits(),
                (-0.0_f64).to_bits()
            );
            for (view, expected) in [
                (raw.channel_id.descriptive_label, "通道 🛰"),
                (raw.platform_id.descriptive_label, "plateforme é"),
                (raw.sensor_location.key, "位置"),
                (raw.sensor_location.system_name, "航機"),
            ] {
                // SAFETY: the raw strings borrow the live config for this call.
                let bytes = unsafe { slice::from_raw_parts(view.data.cast::<u8>(), view.size) };
                assert_eq!(bytes, expected.as_bytes());
            }
            // SAFETY: helper-provided writable slots, no handle dereference.
            unsafe {
                assert!((*out).is_null());
                *d = 0;
                *r = 1;
            }
            sys::AMS_MEL_OK
        },
        |_| panic!("nothing to release after null-success Open"),
    )
    .unwrap_err();
    assert_eq!(calls.get(), 1);
    assert_eq!(error.kind(), &ErrorKind::ProtocolInconsistency);
    assert_eq!(
        error.diagnostic(),
        Some("Scheduling open succeeded without a channel owner")
    );
}

#[test]
fn unexpected_owner_gets_one_cleanup_without_retrying_open() {
    for (status, diagnostic, kind) in [
        (
            sys::AMS_MEL_PROVIDER_EXCEPTION,
            b"attach threw\0".as_slice(),
            ErrorKind::ProviderException,
        ),
        (
            sys::AMS_MEL_PROVIDER_FAILED,
            b"".as_slice(),
            ErrorKind::ProtocolInconsistency,
        ),
        (
            sys::AMS_MEL_OK,
            b"not empty\0".as_slice(),
            ErrorKind::ProtocolInconsistency,
        ),
        (
            sys::AMS_MEL_PROVIDER_FAILED,
            b"\xff\0".as_slice(),
            ErrorKind::InvalidUtf8,
        ),
    ] {
        let token = ptr::dangling_mut();
        let cleanups = Cell::new(0);
        let error = open_with(
            &config(),
            |_, out, d, _, r| {
                // SAFETY: exact writable slots; diagnostic fits and token is opaque.
                unsafe {
                    *out = token;
                    ptr::copy_nonoverlapping(diagnostic.as_ptr(), d.cast(), diagnostic.len());
                    *r = diagnostic.len();
                }
                status
            },
            |raw| {
                assert_eq!(raw, token);
                cleanups.set(cleanups.get() + 1);
                // Model cleanup retaining the owner; Open must never offer it again.
            },
        )
        .unwrap_err();
        assert_eq!(cleanups.get(), 1);
        assert_eq!(error.kind(), &kind);
        if kind == ErrorKind::ProviderException {
            assert_eq!(error.diagnostic(), Some("attach threw"));
        }
    }
}

#[test]
fn close_only_retries_well_formed_retained_owner() {
    let original = ptr::dangling_mut();
    let replacement = std::ptr::without_provenance_mut(4096);
    let null = ptr::null_mut();
    let failed = sys::AMS_MEL_PROVIDER_FAILED;
    let ok = sys::AMS_MEL_OK;
    for (status, output, required, kind, retained) in [
        (ok, null, 1, None, false),
        (
            ok,
            original,
            1,
            Some(ErrorKind::ProtocolInconsistency),
            false,
        ),
        (
            ok,
            replacement,
            1,
            Some(ErrorKind::ProtocolInconsistency),
            false,
        ),
        (failed, original, 1, Some(ErrorKind::ProviderFailed), true),
        (failed, null, 1, Some(ErrorKind::ProviderFailed), false),
        (
            failed,
            replacement,
            1,
            Some(ErrorKind::ProviderFailed),
            false,
        ),
        (
            failed,
            original,
            0,
            Some(ErrorKind::ProtocolInconsistency),
            false,
        ),
        (
            failed,
            null,
            0,
            Some(ErrorKind::ProtocolInconsistency),
            false,
        ),
        (
            failed,
            original,
            usize::MAX,
            Some(ErrorKind::ProviderFailed),
            false,
        ),
        (
            sys::AMS_MEL_INTERNAL_ERROR,
            original,
            1,
            Some(ErrorKind::InternalError),
            false,
        ),
    ] {
        let mut owner = original;
        let result = close_with(&mut owner, |out, d, _, r| {
            // SAFETY: exact writable slots; injected tokens never reach the ABI.
            unsafe {
                assert_eq!(*out, original);
                *out = output;
                *d = 0;
                *r = required;
            }
            status
        });
        assert_eq!(result.err().map(|error| error.kind().clone()), kind);
        assert_eq!(owner, if retained { original } else { null });
        if !retained {
            close_with(&mut owner, |_, _, _, _| panic!("owner was disarmed")).unwrap();
        }
    }
}

#[test]
fn shared_conversion_keeps_reserved_and_unknown_tags() {
    // SAFETY: all-zero C snapshot contains valid scalar values and empty spans.
    let mut raw: sys::AmsMelIrChannelCapabilityV1 = unsafe { mem::zeroed() };
    let types = [
        sys::AMS_MEL_IR_CHANNEL_SCHEDULING,
        sys::AMS_MEL_IR_CHANNEL_RESERVED_1,
        u32::MAX,
    ];
    let metadata = [sys::AMS_MEL_IR_METADATA_RESERVED_3, u32::MAX];
    raw.channel_types = sys::AmsMelU32SpanV1 {
        data: types.as_ptr(),
        size: types.len(),
    };
    raw.metadata_capabilities = sys::AmsMelU32SpanV1 {
        data: metadata.as_ptr(),
        size: metadata.len(),
    };
    raw.pixel_format = u32::MAX;
    raw.task_schedule_depth = u32::MAX;
    let capabilities = capability_from_raw(&raw).unwrap();
    assert_eq!(
        capabilities.channel_types,
        [
            ChannelType::Scheduling,
            ChannelType::Reserved1,
            ChannelType::Unknown(u32::MAX)
        ]
    );
    assert_eq!(
        capabilities.metadata_capabilities,
        [
            MetadataCapability::Reserved3,
            MetadataCapability::Unknown(u32::MAX)
        ]
    );
    assert_eq!(capabilities.pixel_format, PixelFormat::Unknown(u32::MAX));
    assert_eq!(capabilities.task_schedule_depth, u32::MAX);
}
