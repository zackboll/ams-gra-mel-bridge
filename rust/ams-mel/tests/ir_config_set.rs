use ams_mel::{
    C2MetadataEvent, CommandReturn, CommandState, ComponentLocation, ConfigSetCommand,
    ControlConfig, ErrorKind, MelErrorCode, ReturnRequest, ReturnResult, Session, SessionOptions,
    UciId,
};
use std::{
    env, fs,
    path::{Path, PathBuf},
    sync::Mutex,
};

static TEST: Mutex<()> = Mutex::new(());
const SUCCESS: ReturnResult = ReturnResult::Completed {
    value: CommandReturn::Success,
};

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

#[test]
fn public_construction_and_compiled_provider_field_fidelity() {
    let _lock = TEST.lock().unwrap();
    let default = ConfigSetCommand::new(301);
    assert_eq!(default.command_id, 301);
    assert_eq!(default.system_time_ns, 0);
    assert_eq!(default.config, "");
    assert_eq!(default, default.clone());
    for (scenario, command) in [
        ("config-empty", ConfigSetCommand::new(0)),
        (
            "config-full",
            ConfigSetCommand {
                command_id: 0xfedc_ba98,
                system_time_ns: -1234567890123,
                config: "configuration-€".into(),
            },
        ),
        (
            "config-positive",
            ConfigSetCommand {
                command_id: 0x8000_0000,
                system_time_ns: 9876543210,
                config: "positive".into(),
            },
        ),
    ] {
        let session = Session::open(provider(), scenario, "").unwrap();
        let mut channel = session.open_control_channel(&config()).unwrap();
        channel.enable().unwrap();
        let mut request: ReturnRequest = channel.submit_config_set(&command).unwrap();
        assert_eq!(request.wait(1000).unwrap(), SUCCESS);
        assert_eq!(request.wait(0).unwrap(), SUCCESS);
        request.close().unwrap();
        channel.close().unwrap();
        session.close().unwrap();
    }
}

#[test]
fn native_submission_exception_is_not_a_completed_return() {
    let _lock = TEST.lock().unwrap();
    let session = Session::open(provider(), "config-full", "").unwrap();
    let mut channel = session.open_control_channel(&config()).unwrap();
    channel.enable().unwrap();
    let error = channel
        .submit_config_set(&ConfigSetCommand::new(0))
        .unwrap_err();
    assert_eq!(error.kind(), &ErrorKind::ProviderException);
    assert!(error
        .diagnostic()
        .unwrap()
        .contains("ConfigSet conversion mismatch"));
    let command = ConfigSetCommand {
        command_id: 0xfedc_ba98,
        system_time_ns: -1234567890123,
        config: "configuration-€".into(),
    };
    let mut request = channel.submit_config_set(&command).unwrap();
    assert_eq!(request.wait(1000).unwrap(), SUCCESS);
    request.close().unwrap();
    channel.close().unwrap();
    session.close().unwrap();
}

#[test]
fn opaque_unicode_and_full_numeric_ranges_are_accepted() {
    let _lock = TEST.lock().unwrap();
    let session = Session::open(provider(), "success", "").unwrap();
    let mut channel = session.open_control_channel(&config()).unwrap();
    channel.enable().unwrap();
    for system_time_ns in [i64::MIN, i64::MAX] {
        for config in ["", "not a structured configuration", "é e\u{301} € 🛰️\r\n"] {
            let command = ConfigSetCommand {
                command_id: u32::MAX,
                system_time_ns,
                config: config.into(),
            };
            let mut request = channel.submit_config_set(&command).unwrap();
            drop(command); // no Rust string borrow is retained by the request
            assert_eq!(request.wait(1000).unwrap(), SUCCESS);
            request.close().unwrap();
        }
    }
}

#[test]
fn embedded_nul_does_not_send_or_poison_channel() {
    let _lock = TEST.lock().unwrap();
    let log = env::temp_dir().join(format!("rust-ir-010-nul-{}.log", std::process::id()));
    let _ = fs::remove_file(&log);
    env::set_var("AMS_MEL_TEST_LIFETIME_LOG", &log);
    let session = Session::open(provider(), "config-empty", "").unwrap();
    let mut channel = session.open_control_channel(&config()).unwrap();
    channel.enable().unwrap();
    for config in ["\0", "before\0after", "€\0"] {
        assert_eq!(
            channel
                .submit_config_set(&ConfigSetCommand {
                    config: config.into(),
                    ..ConfigSetCommand::new(0)
                })
                .unwrap_err()
                .kind(),
            &ErrorKind::InvalidArgument
        );
    }
    assert!(!fs::read_to_string(&log)
        .unwrap()
        .contains("config_set_sent"));
    channel
        .submit_config_set(&ConfigSetCommand::new(0))
        .unwrap()
        .wait(1000)
        .unwrap();
    channel.close().unwrap();
    session.close().unwrap();
    assert_eq!(
        fs::read_to_string(&log)
            .unwrap()
            .matches("config_set_sent")
            .count(),
        1
    );
    env::remove_var("AMS_MEL_TEST_LIFETIME_LOG");
    fs::remove_file(log).unwrap();
}

#[test]
fn completion_fail_and_explicit_rejection_are_independent_of_command_status() {
    let _lock = TEST.lock().unwrap();
    for (scenario, expected, state, description) in [
        ("config-empty", SUCCESS, CommandState::Accepted, ""),
        (
            "config-fail",
            ReturnResult::Completed {
                value: CommandReturn::Fail,
            },
            CommandState::Rejected,
            "invalid configuration payload",
        ),
        (
            "config-reject",
            ReturnResult::Rejected {
                code: MelErrorCode::InvalidParameters,
                description: "invalid configuration".into(),
            },
            CommandState::Accepted,
            "",
        ),
    ] {
        let session = Session::open(provider(), scenario, "").unwrap();
        let mut channel = session.open_control_channel(&config()).unwrap();
        channel.enable().unwrap();
        let mut metadata = channel.open_metadata(4).unwrap();
        let mut request = channel
            .submit_config_set(&ConfigSetCommand::new(0))
            .unwrap();
        assert_eq!(request.wait(1000).unwrap(), expected);
        assert_eq!(request.wait(0).unwrap(), expected);
        let C2MetadataEvent::CommandStatus(status) = metadata.receive(1000).unwrap() else {
            panic!("expected independent CommandStatus metadata");
        };
        assert_eq!(status.command_id, 0);
        assert_eq!(status.state, state);
        assert_eq!(status.reason_description, description);
        metadata.close().unwrap();
        request.close().unwrap();
        channel.close().unwrap();
        session.close().unwrap();
    }
}

#[test]
fn enable_failed_and_closed_lifecycle_boundaries() {
    let _lock = TEST.lock().unwrap();
    for scenario in ["config-empty", "c2-enable-fail"] {
        let session = Session::open(provider(), scenario, "").unwrap();
        let mut channel = session.open_control_channel(&config()).unwrap();
        let command = ConfigSetCommand::new(0);
        assert_eq!(
            channel.submit_config_set(&command).unwrap_err().kind(),
            &ErrorKind::ProviderFailed
        );
        if scenario == "c2-enable-fail" {
            assert_eq!(
                channel.enable().unwrap_err().kind(),
                &ErrorKind::ProviderFailed
            );
            assert_eq!(
                channel.submit_config_set(&command).unwrap_err().kind(),
                &ErrorKind::ProviderFailed
            );
        } else {
            channel.enable().unwrap();
        }
        channel.close().unwrap();
        assert_eq!(
            channel.submit_config_set(&command).unwrap_err().kind(),
            &ErrorKind::InvalidArgument
        );
        session.close().unwrap();
    }
}

// Existing provider promise gate and native FinalOwner observation; no sleeps.
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
        // SAFETY: exact existing fixture signature; DSO retained through calls.
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
                ams_mel_test_completion_boundary(1, 3, 0, counts.as_mut_ptr()),
                1
            );
            assert_eq!(
                ams_mel_test_completion_owner(1, counts[2], &mut observed),
                1
            );
        }
    }
}
impl Drop for Mock {
    fn drop(&mut self) {
        // SAFETY: drops only the helper's DSO reference after synchronized cleanup.
        assert_eq!(unsafe { dlclose(self.handle) }, 0);
    }
}

#[test]
fn pending_requests_survive_parents_and_close_drop_do_not_cancel() {
    let _lock = TEST.lock().unwrap();
    for session_first in [false, true] {
        let log = env::temp_dir().join(format!(
            "rust-ir-010-{}-{session_first}.log",
            std::process::id()
        ));
        let _ = fs::remove_file(&log);
        env::set_var("AMS_MEL_TEST_LIFETIME_LOG", &log);
        let mock = Mock::open();
        let session = Session::open(provider(), "completion-scale", "").unwrap();
        let mut channel = session.open_control_channel(&config()).unwrap();
        channel.enable().unwrap();
        let mut first = channel
            .submit_config_set(&ConfigSetCommand::new(1))
            .unwrap();
        let second = channel
            .submit_config_set(&ConfigSetCommand::new(2))
            .unwrap();
        let third = channel
            .submit_config_set(&ConfigSetCommand::new(3))
            .unwrap();
        mock.control(1, 3);
        assert_eq!(first.wait(0).unwrap_err().kind(), &ErrorKind::Timeout);
        assert_eq!(first.wait(1).unwrap_err().kind(), &ErrorKind::Timeout);
        second.close().unwrap();
        drop(third);
        if session_first {
            session.close().unwrap();
            channel.close().unwrap();
        } else {
            channel.close().unwrap();
            session.close().unwrap();
        }
        drop(channel);
        assert!(!fs::read_to_string(&log)
            .unwrap()
            .contains("c2_channel_destroyed"));
        mock.control(2, 1);
        assert_eq!(first.wait(1000).unwrap(), SUCCESS);
        assert_eq!(first.wait(0).unwrap(), SUCCESS);
        first.close().unwrap();
        assert!(!fs::read_to_string(&log)
            .unwrap()
            .contains("c2_channel_destroyed"));
        mock.control(2, 2);
        mock.reclaimed();
        let events = fs::read_to_string(&log).unwrap();
        assert_eq!(events.matches("config_set_sent").count(), 3);
        assert_eq!(events.matches("c2_channel_destroyed").count(), 1);
        assert!(!events.contains("library_unloaded"));
        drop(mock);
        let events = fs::read_to_string(&log).unwrap();
        assert_eq!(events.matches("library_unloaded").count(), 1);
        assert!(
            events.find("c2_channel_destroyed").unwrap() < events.find("library_unloaded").unwrap()
        );
        env::remove_var("AMS_MEL_TEST_LIFETIME_LOG");
        fs::remove_file(log).unwrap();
    }
}

#[test]
fn shared_session_admission_is_not_released_by_pending_close() {
    let _lock = TEST.lock().unwrap();
    let mock = Mock::open();
    let session = Session::open_with_options(
        provider(),
        "completion-scale",
        "",
        SessionOptions {
            max_async_requests: 2,
        },
    )
    .unwrap();
    let mut first_channel = session.open_control_channel(&config()).unwrap();
    let mut second_channel = session.open_control_channel(&config()).unwrap();
    first_channel.enable().unwrap();
    second_channel.enable().unwrap();
    let first = first_channel
        .submit_config_set(&ConfigSetCommand::new(1))
        .unwrap();
    let mut second = second_channel.submit_bit_noop(2).unwrap();
    assert_eq!(
        second_channel
            .submit_config_set(&ConfigSetCommand::new(3))
            .unwrap_err()
            .kind(),
        &ErrorKind::ResourceExhausted
    );
    assert_eq!(
        first_channel.submit_operate(4).unwrap_err().kind(),
        &ErrorKind::ResourceExhausted
    );
    first.close().unwrap();
    assert_eq!(
        first_channel
            .submit_config_set(&ConfigSetCommand::new(5))
            .unwrap_err()
            .kind(),
        &ErrorKind::ResourceExhausted
    );
    mock.control(2, 2);
    assert_eq!(second.wait(1000).unwrap(), SUCCESS);
    mock.reclaimed();
    second.close().unwrap();
    let mut next = second_channel
        .submit_config_set(&ConfigSetCommand::new(6))
        .unwrap();
    mock.control(2, 1);
    assert_eq!(next.wait(1000).unwrap(), SUCCESS);
    mock.reclaimed();
    next.close().unwrap();
    first_channel.close().unwrap();
    second_channel.close().unwrap();
    session.close().unwrap();
}

#[test]
fn admitted_provider_exception_is_cached() {
    let _lock = TEST.lock().unwrap();
    let mock = Mock::open();
    mock.control(3, 0);
    let session = Session::open(provider(), "completion-scale", "").unwrap();
    let mut channel = session.open_control_channel(&config()).unwrap();
    channel.enable().unwrap();
    let mut request = channel
        .submit_config_set(&ConfigSetCommand::new(0))
        .unwrap();
    assert_eq!(request.wait(0).unwrap_err().kind(), &ErrorKind::Timeout);
    mock.control(2, 1);
    for timeout in [1000, 0] {
        let error = request.wait(timeout).unwrap_err();
        assert_eq!(error.kind(), &ErrorKind::ProviderException);
        assert!(error
            .diagnostic()
            .unwrap()
            .contains("held future exception"));
    }
    mock.reclaimed();
    request.close().unwrap();
    channel.close().unwrap();
    session.close().unwrap();
}
