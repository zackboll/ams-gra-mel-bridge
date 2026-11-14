use ams_mel::{
    ComponentLocation, ControlConfig, CoordinateFrameReference, DegradationMethod, ErrorKind,
    MelErrorCode, MfaMode, MfaState, ModeCommand, ModeResult, ScanParameters, ScanType, Session,
    SessionOptions, UciId,
};
use std::{
    env, fs,
    path::{Path, PathBuf},
    sync::Mutex,
};

static TEST: Mutex<()> = Mutex::new(());

fn provider() -> PathBuf {
    env::var_os("AMS_MEL_TEST_PROVIDER_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| {
            Path::new(env!("CARGO_MANIFEST_DIR")).join("../../native/build-tests/test-providers")
        })
        .join("libmock_ir_provider.so")
}

fn config() -> ControlConfig {
    ControlConfig::new(
        UciId::new([0; 16], "channel").unwrap(),
        UciId::new([0; 16], "platform").unwrap(),
        ComponentLocation::new(1.25, -2.5, 3.75, "station-1", "mock-aircraft").unwrap(),
    )
}

fn operate() -> ModeCommand {
    ModeCommand {
        command_id: 0x89ab_cdef,
        state: MfaState::Operate,
        mode: MfaMode::TaskSched,
        scan_parameters: ScanParameters::default(),
    }
}

#[test]
fn every_published_state_and_mode_including_standby_unused() {
    let _lock = TEST.lock().unwrap();
    let session = Session::open(provider(), "mode-general", "").unwrap();
    let mut channel = session.open_control_channel(&config()).unwrap();
    channel.enable().unwrap();
    for &state in MfaState::ALL {
        for mode in [
            MfaMode::Unused,
            MfaMode::TaskSched,
            MfaMode::ScanVolumeSched,
            MfaMode::ScanBarSched,
        ] {
            let command = ModeCommand {
                command_id: u32::MAX,
                state,
                mode,
                ..operate()
            };
            let mut request = channel.submit_mode(&command).unwrap();
            assert_eq!(request.wait(1000).unwrap(), ModeResult::Success { mode });
            assert_eq!(request.wait(0).unwrap(), ModeResult::Success { mode });
            request.close().unwrap();
        }
    }
    let mut legacy = channel.submit_operate(0x89ab_cdef).unwrap();
    assert_eq!(
        legacy.wait(1000).unwrap(),
        ModeResult::Success {
            mode: MfaMode::TaskSched
        }
    );
    legacy.close().unwrap();
    channel.close().unwrap();
    session.close().unwrap();
}

#[test]
fn complete_scan_payload_is_checked_by_the_existing_provider() {
    let _lock = TEST.lock().unwrap();
    let session = Session::open(provider(), "mode-full", "").unwrap();
    let mut channel = session.open_control_channel(&config()).unwrap();
    channel.enable().unwrap();
    let command = ModeCommand {
        mode: MfaMode::ScanVolumeSched,
        scan_parameters: ScanParameters {
            elevation_defined_with_range_and_altitude: true,
            center_az_rad: 0.25,
            center_el_rad: -0.5,
            center_frame_ref_el: CoordinateFrameReference::Aircraft,
            center_frame_ref_az: CoordinateFrameReference::Inertial,
            scan_width_rad: 1.25,
            scan_height_rad: 0.75,
            scan_type: ScanType {
                continuous_scan: 1,
                returning: 2,
                agile_scan: 3,
            },
            scan_id: 0x89ab_cdef,
            scan_rate_rad_per_second: -0.125,
            preferred_revisit_interval_seconds: 2.5,
            required_revisit_interval_seconds: 3.5,
            max_range_of_interest_m: 123456,
            min_range_of_interest_m: 42,
            elevation_scan_center_altitude_m: 7000,
            elevation_scan_center_range_m: 9000,
            degradation_method: DegradationMethod::Revisit,
        },
        ..operate()
    };
    let mut request = channel.submit_mode(&command).unwrap();
    assert_eq!(
        request.wait(1000).unwrap(),
        ModeResult::Success {
            mode: MfaMode::ScanVolumeSched
        }
    );
    request.close().unwrap();
    channel.close().unwrap();
    session.close().unwrap();
}

#[test]
fn native_boundary_accepts_full_width_and_nonfinite_scan_fields() {
    let _lock = TEST.lock().unwrap();
    let session = Session::open(provider(), "mode-general", "").unwrap();
    let mut channel = session.open_control_channel(&config()).unwrap();
    channel.enable().unwrap();
    let mut command = operate();
    command.command_id = u32::MAX;
    command.scan_parameters = ScanParameters {
        elevation_defined_with_range_and_altitude: true,
        center_az_rad: -0.0,
        center_el_rad: f64::NAN,
        scan_width_rad: f64::INFINITY,
        scan_height_rad: f64::NEG_INFINITY,
        scan_type: ScanType {
            continuous_scan: u32::MAX,
            returning: 0x8000_0000,
            agile_scan: u32::MAX,
        },
        scan_id: u32::MAX,
        scan_rate_rad_per_second: f64::NAN,
        preferred_revisit_interval_seconds: -1.0,
        required_revisit_interval_seconds: -0.0,
        min_range_of_interest_m: u32::MAX,
        max_range_of_interest_m: 1,
        elevation_scan_center_altitude_m: u32::MAX,
        elevation_scan_center_range_m: u32::MAX,
        ..ScanParameters::default()
    };
    let mut request = channel.submit_mode(&command).unwrap();
    assert_eq!(
        request.wait(1000).unwrap(),
        ModeResult::Success {
            mode: MfaMode::TaskSched
        }
    );
}

#[test]
fn unknown_input_is_not_remapped_and_does_not_poison_channel() {
    let _lock = TEST.lock().unwrap();
    let session = Session::open(provider(), "mode-general", "").unwrap();
    let mut channel = session.open_control_channel(&config()).unwrap();
    channel.enable().unwrap();
    for raw in [0, 1, 2, 3, 4, u32::MAX] {
        assert_eq!(
            channel
                .submit_mode(&ModeCommand {
                    mode: MfaMode::Unknown(raw),
                    ..operate()
                })
                .unwrap_err()
                .kind(),
            &ErrorKind::InvalidArgument
        );
    }
    assert!(MfaState::try_from(15).is_err());
    assert!(CoordinateFrameReference::try_from(2).is_err());
    assert!(DegradationMethod::try_from(4).is_err());
    channel.submit_mode(&operate()).unwrap().wait(1000).unwrap();
}

#[test]
fn enable_failed_and_closed_channel_boundaries() {
    let _lock = TEST.lock().unwrap();
    for scenario in ["mode-general", "c2-enable-fail"] {
        let session = Session::open(provider(), scenario, "").unwrap();
        let mut channel = session.open_control_channel(&config()).unwrap();
        assert_eq!(
            channel.submit_mode(&operate()).unwrap_err().kind(),
            &ErrorKind::ProviderFailed
        );
        if scenario == "c2-enable-fail" {
            assert_eq!(
                channel.enable().unwrap_err().kind(),
                &ErrorKind::ProviderFailed
            );
            assert_eq!(
                channel.submit_mode(&operate()).unwrap_err().kind(),
                &ErrorKind::ProviderFailed
            );
        } else {
            channel.enable().unwrap();
        }
        channel.close().unwrap();
        assert_eq!(
            channel.submit_mode(&operate()).unwrap_err().kind(),
            &ErrorKind::InvalidArgument
        );
        session.close().unwrap();
    }
}

#[test]
fn provider_rejections_retain_actual_description_and_code() {
    let _lock = TEST.lock().unwrap();
    for (scenario, description) in [
        ("c2-reject", "invalid task schedule".to_owned()),
        ("c2-reject-empty", String::new()),
        (
            "c2-reject-long",
            format!("{}€{}", "x".repeat(510), "y".repeat(100)),
        ),
    ] {
        let session = Session::open(provider(), scenario, "").unwrap();
        let mut channel = session.open_control_channel(&config()).unwrap();
        channel.enable().unwrap();
        let mut request = channel.submit_mode(&operate()).unwrap();
        let expected = ModeResult::Rejected {
            code: MelErrorCode::InvalidParameters,
            description,
        };
        assert_eq!(request.wait(1000).unwrap(), expected);
        assert_eq!(request.wait(0).unwrap(), expected);
    }
}

#[test]
fn failed_send_and_malformed_provider_future_are_errors() {
    let _lock = TEST.lock().unwrap();
    for (scenario, kind) in [
        ("c2-null-result", ErrorKind::ProviderFailed),
        ("c2-future-throw", ErrorKind::ProviderException),
        ("c2-send-throw", ErrorKind::ProviderException),
    ] {
        let session = Session::open(provider(), scenario, "").unwrap();
        let mut channel = session.open_control_channel(&config()).unwrap();
        channel.enable().unwrap();
        if scenario == "c2-send-throw" {
            assert_eq!(channel.submit_mode(&operate()).unwrap_err().kind(), &kind);
        } else {
            let mut request = channel.submit_mode(&operate()).unwrap();
            assert_eq!(request.wait(1000).unwrap_err().kind(), &kind);
            assert_eq!(request.wait(0).unwrap_err().kind(), &kind);
        }
    }
}

// The provider's existing promise gate and native FinalOwner barrier avoid
// sleep-based decisions about pending work, admission release, and teardown.
type Gate = unsafe extern "C" fn(u32, u64, *mut u64, *mut u64) -> i32;
#[link(name = "dl")]
extern "C" {
    fn dlopen(path: *const std::ffi::c_char, flags: i32) -> *mut std::ffi::c_void;
    fn dlsym(
        handle: *mut std::ffi::c_void,
        symbol: *const std::ffi::c_char,
    ) -> *mut std::ffi::c_void;
    fn dlclose(handle: *mut std::ffi::c_void) -> i32;
}
extern "C" {
    fn ams_mel_test_completion_boundary(
        family: u32,
        operation: u32,
        target: u64,
        counts: *mut u64,
    ) -> i32;
    fn ams_mel_test_completion_owner(family: u32, target: u64, observed: *mut u64) -> i32;
}
struct Mock {
    handle: *mut std::ffi::c_void,
    gate: Gate,
}
impl Mock {
    fn open() -> Self {
        use std::{ffi::CString, os::unix::ffi::OsStrExt};
        let path = CString::new(provider().as_os_str().as_bytes()).unwrap();
        // SAFETY: exact existing test-provider signature; DSO retained through calls.
        unsafe {
            let handle = dlopen(path.as_ptr(), 2);
            assert!(!handle.is_null());
            let symbol = CString::new("mock_completion_gate").unwrap();
            let address = dlsym(handle, symbol.as_ptr());
            assert!(!address.is_null());
            Self {
                handle,
                gate: std::mem::transmute::<*mut std::ffi::c_void, Gate>(address),
            }
        }
    }
    fn control(&self, operation: u32, count: u64) {
        // SAFETY: exact fixture signature, optional output pointers.
        assert_eq!(
            unsafe { (self.gate)(operation, count, std::ptr::null_mut(), std::ptr::null_mut()) },
            1
        );
    }
    fn reclaimed(&self) {
        let mut counts = [0; 4];
        let mut observed = 0;
        // SAFETY: exact test-only signatures, sized writable output buffers.
        unsafe {
            assert_eq!(
                ams_mel_test_completion_boundary(0, 3, 0, counts.as_mut_ptr()),
                1
            );
            assert_eq!(
                ams_mel_test_completion_owner(0, counts[2], &mut observed),
                1
            );
        }
    }
}
impl Drop for Mock {
    fn drop(&mut self) {
        // SAFETY: this helper's DSO reference only; tests synchronize FinalOwner first.
        unsafe {
            dlclose(self.handle);
        }
    }
}

#[test]
fn pending_requests_outlive_both_parent_close_orders_and_drop_is_not_cancel() {
    let _lock = TEST.lock().unwrap();
    for session_first in [false, true] {
        let log = env::temp_dir().join(format!(
            "rust-ir-009-{}-{session_first}.log",
            std::process::id()
        ));
        let _ = fs::remove_file(&log);
        env::set_var("AMS_MEL_TEST_LIFETIME_LOG", &log);
        let mock = Mock::open();
        let session = Session::open(provider(), "completion-scale", "").unwrap();
        let mut channel = session.open_control_channel(&config()).unwrap();
        channel.enable().unwrap();
        let mut first = channel.submit_mode(&operate()).unwrap();
        let second = channel.submit_mode(&operate()).unwrap();
        mock.control(1, 2);
        assert_eq!(first.wait(0).unwrap_err().kind(), &ErrorKind::Timeout);
        drop(second);
        if session_first {
            session.close().unwrap();
            channel.close().unwrap();
        } else {
            channel.close().unwrap();
            session.close().unwrap();
        }
        assert!(!fs::read_to_string(&log)
            .unwrap_or_default()
            .contains("c2_channel_destroyed"));
        mock.control(2, 1);
        assert_eq!(
            first.wait(1000).unwrap(),
            ModeResult::Success {
                mode: MfaMode::TaskSched
            }
        );
        assert_eq!(
            first.wait(0).unwrap(),
            ModeResult::Success {
                mode: MfaMode::TaskSched
            }
        );
        first.close().unwrap();
        // The second request's dropped observer did not cancel its held future.
        assert!(!fs::read_to_string(&log)
            .unwrap_or_default()
            .contains("c2_channel_destroyed"));
        mock.control(2, 1);
        mock.reclaimed();
        let events = fs::read_to_string(&log).unwrap();
        assert!(events.contains("c2_channel_destroyed"));
        assert!(!events.contains("library_unloaded")); // helper still owns DSO
        drop(mock);
        let events = fs::read_to_string(&log).unwrap();
        assert!(
            events.find("c2_channel_destroyed").unwrap() < events.find("library_unloaded").unwrap()
        );
        env::remove_var("AMS_MEL_TEST_LIFETIME_LOG");
        fs::remove_file(log).unwrap();
    }
}

#[test]
fn general_mode_shares_admission_and_pending_close_does_not_free_capacity() {
    let _lock = TEST.lock().unwrap();
    let mock = Mock::open();
    let session = Session::open_with_options(
        provider(),
        "completion-scale",
        "",
        SessionOptions {
            max_async_requests: 1,
        },
    )
    .unwrap();
    let mut channel = session.open_control_channel(&config()).unwrap();
    channel.enable().unwrap();
    let request = channel.submit_mode(&operate()).unwrap();
    assert_eq!(
        channel.submit_operate(1).unwrap_err().kind(),
        &ErrorKind::ResourceExhausted
    );
    request.close().unwrap();
    assert_eq!(
        channel.submit_mode(&operate()).unwrap_err().kind(),
        &ErrorKind::ResourceExhausted
    );
    mock.control(2, 1);
    mock.reclaimed();
    let mut request = channel.submit_mode(&operate()).unwrap();
    mock.control(2, 1);
    request.wait(1000).unwrap();
    mock.reclaimed();
    request.close().unwrap();
    channel.close().unwrap();
    session.close().unwrap();
}
