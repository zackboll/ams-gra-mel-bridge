use ams_mel::{
    BitAction, BitCommand, C2MetadataEvent, CommandReturn, CommandState, ComponentLocation,
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

fn command(command_id: u32) -> BitCommand {
    BitCommand {
        command_id,
        action: BitAction::NoOp,
    }
}

#[test]
fn public_payloads_and_input_storage_release() {
    let _lock = TEST.lock().unwrap();
    let session = Session::open(provider(), "bit-ada", "").unwrap();
    let mut channel = session.open_control_channel(&config()).unwrap();
    channel.enable().unwrap();
    let mut metadata = channel.open_metadata(4).unwrap();
    // bit-ada accepts one-choice payloads but does not inspect their fields.
    // Exact field/byte fidelity is checked in private conversion tests.
    for action in [
        BitAction::NoOp,
        BitAction::Initiate(vec![7]),
        BitAction::Initiate(vec![0, u32::MAX, 0x8000_0001, 7, 0, 7]),
        BitAction::Cancel(vec![7]),
        BitAction::Cancel(vec![0, u32::MAX, 0x8000_0001, 7, 0, 7]),
        BitAction::ClearFaultCodes(vec!["fault-€".into()]),
        BitAction::ClearFaultCodes(vec![
            "".into(),
            "é".into(),
            "e\u{301}".into(),
            "🛰️".into(),
            "é".into(),
        ]),
    ] {
        let command = BitCommand {
            command_id: u32::MAX,
            action,
        };
        let mut request: ReturnRequest = channel.submit_bit(&command).unwrap();
        drop(command); // all input/descriptor pointers must already have been copied
        assert_eq!(request.wait(1000).unwrap(), SUCCESS);
        assert_eq!(request.wait(0).unwrap(), SUCCESS);
        let C2MetadataEvent::CommandStatus(status) = metadata.receive(1000).unwrap() else {
            panic!("expected independent payload-command status");
        };
        assert_eq!(status.command_id, u32::MAX);
        assert_eq!(status.state, CommandState::Accepted);
        request.close().unwrap();
    }
    metadata.close().unwrap();
    channel.close().unwrap();
    session.close().unwrap();
}

#[test]
fn invalid_actions_never_reach_provider_and_channel_remains_usable() {
    let _lock = TEST.lock().unwrap();
    let log = env::temp_dir().join(format!("rust-ir-011-invalid-{}.log", std::process::id()));
    let _ = fs::remove_file(&log);
    env::set_var("AMS_MEL_TEST_LIFETIME_LOG", &log);
    let session = Session::open(provider(), "bit-ada", "").unwrap();
    let mut channel = session.open_control_channel(&config()).unwrap();
    channel.enable().unwrap();
    for action in [
        BitAction::Initiate(vec![]),
        BitAction::Cancel(vec![]),
        BitAction::ClearFaultCodes(vec![]),
        BitAction::ClearFaultCodes(vec!["\0".into()]),
        BitAction::ClearFaultCodes(vec!["valid".into(), "€\0after".into()]),
    ] {
        assert_eq!(
            channel
                .submit_bit(&BitCommand {
                    command_id: 0,
                    action
                })
                .unwrap_err()
                .kind(),
            &ErrorKind::InvalidArgument
        );
    }
    assert!(!fs::read_to_string(&log).unwrap().contains("bit_sent"));
    let mut request = channel
        .submit_bit(&BitCommand {
            command_id: 1,
            action: BitAction::Initiate(vec![1]),
        })
        .unwrap();
    assert_eq!(request.wait(1000).unwrap(), SUCCESS);
    request.close().unwrap();
    channel.close().unwrap();
    session.close().unwrap();
    assert_eq!(
        fs::read_to_string(&log)
            .unwrap()
            .matches("bit_sent")
            .count(),
        1
    );
    env::remove_var("AMS_MEL_TEST_LIFETIME_LOG");
    fs::remove_file(log).unwrap();
}

#[test]
fn noop_equivalence_return_rejection_and_independent_metadata() {
    let _lock = TEST.lock().unwrap();
    for (scenario, expected) in [
        ("bit-command-id", SUCCESS),
        (
            "bit-fail",
            ReturnResult::Completed {
                value: CommandReturn::Fail,
            },
        ),
        (
            "bit-reject",
            ReturnResult::Rejected {
                code: MelErrorCode::InvalidParameters,
                description: "invalid BIT command".into(),
            },
        ),
    ] {
        let session = Session::open(provider(), scenario, "").unwrap();
        let mut channel = session.open_control_channel(&config()).unwrap();
        channel.enable().unwrap();
        let mut metadata = channel.open_metadata(4).unwrap();
        for legacy in [false, true] {
            let mut request = if legacy {
                channel.submit_bit_noop(0x89ab_cdef)
            } else {
                channel.submit_bit(&command(0x89ab_cdef))
            }
            .unwrap();
            assert_eq!(request.wait(1000).unwrap(), expected);
            assert_eq!(request.wait(0).unwrap(), expected);
            let C2MetadataEvent::CommandStatus(status) = metadata.receive(1000).unwrap() else {
                panic!("expected independent CommandStatus");
            };
            assert_eq!(status.command_id, 0x89ab_cdef);
            // Even Fail and explicit future rejection have Accepted metadata.
            assert_eq!(status.state, CommandState::Accepted);
            assert_eq!(status.reason_description, "");
            request.close().unwrap();
        }
        metadata.close().unwrap();
        channel.close().unwrap();
        session.close().unwrap();
    }
}

#[test]
fn native_long_utf8_rejection_is_cached() {
    let _lock = TEST.lock().unwrap();
    let session = Session::open(provider(), "bit-reject-long", "").unwrap();
    let mut channel = session.open_control_channel(&config()).unwrap();
    channel.enable().unwrap();
    let mut request = channel.submit_bit(&command(1)).unwrap();
    let result = request.wait(1000).unwrap();
    let ReturnResult::Rejected { code, description } = &result else {
        panic!("expected rejection")
    };
    assert_eq!(*code, MelErrorCode::InvalidParameters);
    assert_eq!(
        description,
        &format!("{}€{}", "x".repeat(510), "y".repeat(100))
    );
    assert_eq!(request.wait(0).unwrap(), result);
    request.close().unwrap();
    channel.close().unwrap();
    session.close().unwrap();
}

#[test]
fn provider_exceptions_null_results_and_malformed_return_fail_closed() {
    let _lock = TEST.lock().unwrap();
    for (scenario, expected) in [
        ("bit-send-throw", ErrorKind::ProviderException),
        ("bit-future-throw", ErrorKind::ProviderException),
        ("bit-null-result", ErrorKind::ProviderFailed),
        // The native completion engine rejects unpublished provider Return values.
        ("bit-unknown-return", ErrorKind::ProviderFailed),
    ] {
        let session = Session::open(provider(), scenario, "").unwrap();
        let mut channel = session.open_control_channel(&config()).unwrap();
        channel.enable().unwrap();
        match channel.submit_bit(&command(0)) {
            Err(error) => {
                assert_eq!(scenario, "bit-send-throw");
                assert_eq!(error.kind(), &expected);
                assert!(error
                    .diagnostic()
                    .unwrap()
                    .contains("mock BIT send exception"));
            }
            Ok(mut request) => {
                for timeout in [1000, 0] {
                    assert_eq!(request.wait(timeout).unwrap_err().kind(), &expected);
                }
                request.close().unwrap();
            }
        }
        channel.close().unwrap();
        session.close().unwrap();
    }
}

#[test]
fn enable_failed_and_closed_lifecycle_boundaries() {
    let _lock = TEST.lock().unwrap();
    for scenario in ["bit-empty", "c2-enable-fail"] {
        let session = Session::open(provider(), scenario, "").unwrap();
        let mut channel = session.open_control_channel(&config()).unwrap();
        let command = command(0);
        assert_eq!(
            channel.submit_bit(&command).unwrap_err().kind(),
            &ErrorKind::ProviderFailed
        );
        if scenario == "c2-enable-fail" {
            assert_eq!(
                channel.enable().unwrap_err().kind(),
                &ErrorKind::ProviderFailed
            );
            assert_eq!(
                channel.submit_bit(&command).unwrap_err().kind(),
                &ErrorKind::ProviderFailed
            );
        } else {
            channel.enable().unwrap();
        }
        channel.close().unwrap();
        assert_eq!(
            channel.submit_bit(&command).unwrap_err().kind(),
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
            "rust-ir-011-{}-{session_first}.log",
            std::process::id()
        ));
        let _ = fs::remove_file(&log);
        env::set_var("AMS_MEL_TEST_LIFETIME_LOG", &log);
        let mock = Mock::open();
        let session = Session::open(provider(), "completion-scale", "").unwrap();
        let mut channel = session.open_control_channel(&config()).unwrap();
        channel.enable().unwrap();
        let mut first = channel.submit_bit(&command(1)).unwrap();
        let second = channel.submit_bit(&command(2)).unwrap();
        let third = channel.submit_bit(&command(3)).unwrap();
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
        assert_eq!(events.matches("bit_sent").count(), 3);
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
    let first = first_channel.submit_bit(&command(1)).unwrap();
    let mut second = second_channel.submit_bit_noop(2).unwrap();
    assert_eq!(
        second_channel.submit_bit(&command(3)).unwrap_err().kind(),
        &ErrorKind::ResourceExhausted
    );
    assert_eq!(
        first_channel.submit_operate(4).unwrap_err().kind(),
        &ErrorKind::ResourceExhausted
    );
    first.close().unwrap();
    assert_eq!(
        first_channel.submit_bit(&command(5)).unwrap_err().kind(),
        &ErrorKind::ResourceExhausted
    );
    mock.control(2, 2);
    assert_eq!(second.wait(1000).unwrap(), SUCCESS);
    mock.reclaimed();
    second.close().unwrap();
    let mut next = second_channel.submit_bit(&command(6)).unwrap();
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
    let mut request = channel.submit_bit(&command(0)).unwrap();
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
