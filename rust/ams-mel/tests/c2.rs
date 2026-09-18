use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Mutex;
use std::thread;
use std::time::Duration;

use ams_mel::{
    CommandReturn, ComponentLocation, ControlConfig, ErrorKind, ImageConfig, MelErrorCode, MfaMode,
    ModeResult, ReturnResult, Session, UciId,
};

static NEXT_LOG: AtomicU64 = AtomicU64::new(0);
static C2_TEST: Mutex<()> = Mutex::new(());

#[test]
fn success_requires_enable_and_repeats_cached_result() {
    let _guard = C2_TEST.lock().expect("C2 test lock");
    let session = open("c2-command-id");
    let mut c2 = session
        .open_control_channel(&control_config())
        .expect("open C2");
    assert_eq!(
        c2.submit_operate(0x89ab_cdef)
            .expect_err("submit before enable")
            .kind(),
        &ErrorKind::ProviderFailed
    );
    c2.enable().expect("enable");
    c2.enable().expect("repeated enable");
    let mut request = c2.submit_operate(0x89ab_cdef).expect("submit");
    let expected = ModeResult::Success {
        mode: MfaMode::TaskSched,
    };
    assert_eq!(request.wait(1_000).expect("wait"), expected);
    assert_eq!(request.wait(0).expect("cached poll"), expected);
    request.close().expect("request close");
    c2.close().expect("C2 close");
    assert!(!c2.is_open());
    c2.close().expect("repeated C2 close");
    session.close().expect("session close");
}

#[test]
fn bit_success_requires_enable_and_repeats_cached_result() {
    let _guard = C2_TEST.lock().expect("C2 test lock");
    let session = open("bit-command-id");
    let mut c2 = session
        .open_control_channel(&control_config())
        .expect("open C2");
    assert_eq!(
        c2.submit_bit_noop(0x89ab_cdef)
            .expect_err("submit before enable")
            .kind(),
        &ErrorKind::ProviderFailed
    );
    c2.enable().expect("enable");
    let mut request = c2.submit_bit_noop(0x89ab_cdef).expect("submit BIT");
    let expected = ReturnResult::Completed {
        value: CommandReturn::Success,
    };
    assert_eq!(request.wait(1_000).expect("wait"), expected);
    assert_eq!(request.wait(0).expect("cached poll"), expected);
    request.close().expect("request close");
    c2.close().expect("C2 close");
    session.close().expect("session close");
}

#[test]
fn bit_return_fail_is_a_normal_completion() {
    let _guard = C2_TEST.lock().expect("C2 test lock");
    let session = open("bit-fail");
    let mut c2 = session
        .open_control_channel(&control_config())
        .expect("open C2");
    c2.enable().expect("enable");
    let mut request = c2.submit_bit_noop(1).expect("submit BIT");
    assert_eq!(
        request.wait(1_000).expect("normal Return::Fail completion"),
        ReturnResult::Completed {
            value: CommandReturn::Fail
        }
    );
}

#[test]
fn bit_timeout_does_not_cancel_and_request_outlives_parents() {
    let _guard = C2_TEST.lock().expect("C2 test lock");
    let session = open("bit-delayed");
    let mut c2 = session
        .open_control_channel(&control_config())
        .expect("open C2");
    c2.enable().expect("enable");
    let mut request = c2.submit_bit_noop(7).expect("submit BIT");
    assert_eq!(
        request.wait(0).expect_err("poll timeout").kind(),
        &ErrorKind::Timeout
    );
    session.close().expect("parent close");
    c2.close().expect("channel close");
    assert_eq!(
        request.wait(1_000).expect("request outlives parents"),
        ReturnResult::Completed {
            value: CommandReturn::Success
        }
    );
}

#[test]
fn bit_preserves_complete_long_rejection_and_cached_result() {
    let _guard = C2_TEST.lock().expect("C2 test lock");
    let session = open("bit-reject-long");
    let mut c2 = session
        .open_control_channel(&control_config())
        .expect("open C2");
    c2.enable().expect("enable");
    let mut request = c2.submit_bit_noop(1).expect("submit BIT");
    let expected = ReturnResult::Rejected {
        code: MelErrorCode::InvalidParameters,
        description: format!("{}€{}", "x".repeat(510), "y".repeat(100)),
    };
    assert_eq!(request.wait(1_000).expect("rejection"), expected);
    assert_eq!(request.wait(0).expect("cached rejection"), expected);
}

#[test]
fn bit_preserves_terminal_and_submission_failure_kinds() {
    let _guard = C2_TEST.lock().expect("C2 test lock");
    for (scenario, expected) in [
        ("bit-null-result", ErrorKind::ProviderFailed),
        ("bit-future-throw", ErrorKind::ProviderException),
        ("bit-unknown-return", ErrorKind::ProviderFailed),
    ] {
        let session = open(scenario);
        let mut c2 = session
            .open_control_channel(&control_config())
            .expect("open C2");
        c2.enable().expect("enable");
        let mut request = c2.submit_bit_noop(1).expect("request was published");
        assert_eq!(
            request.wait(1_000).expect_err("terminal failure").kind(),
            &expected
        );
    }

    let session = open("bit-send-throw");
    let mut c2 = session
        .open_control_channel(&control_config())
        .expect("open C2");
    c2.enable().expect("enable");
    assert_eq!(
        c2.submit_bit_noop(1).expect_err("send exception").kind(),
        &ErrorKind::ProviderException
    );
}

#[test]
fn dropping_pending_bit_request_is_non_cancelling_and_cleanup_is_ordered() {
    let _guard = C2_TEST.lock().expect("C2 test lock");
    let log = LifetimeLog::new("bit-pending");
    let session = open("bit-lifetime");
    let mut c2 = session
        .open_control_channel(&control_config())
        .expect("open C2");
    c2.enable().expect("enable");
    let request = c2.submit_bit_noop(8).expect("submit BIT");
    drop(request);
    c2.close().expect("C2 close remains nonblocking");
    session.close().expect("session close remains nonblocking");

    let events = log.wait_for("library_unloaded");
    assert_order(&events, "bit_completed", "c2_channel_destroyed");
    assert_order(&events, "c2_channel_destroyed", "library_unloaded");
}

#[test]
fn timeout_does_not_cancel_and_owners_are_independent() {
    let _guard = C2_TEST.lock().expect("C2 test lock");
    let session = open("c2-delayed");
    let mut c2 = session
        .open_control_channel(&control_config())
        .expect("open C2");
    c2.enable().expect("enable");
    let mut request = c2.submit_operate(7).expect("submit");
    assert_eq!(
        request.wait(0).expect_err("poll timeout").kind(),
        &ErrorKind::Timeout
    );
    session.close().expect("parent close");
    c2.close().expect("channel close");
    assert!(!c2.is_open());
    assert_eq!(
        request.wait(1_000).expect("request outlives parents"),
        ModeResult::Success {
            mode: MfaMode::TaskSched
        }
    );
}

#[test]
fn preserves_rejection_variants_and_cached_results() {
    let _guard = C2_TEST.lock().expect("C2 test lock");
    for (scenario, expected) in [
        ("c2-reject", "invalid task schedule".to_owned()),
        ("c2-reject-empty", String::new()),
        (
            "c2-reject-invalid-utf8",
            "provider rejection description was invalid UTF-8 or contained NUL".to_owned(),
        ),
        (
            "c2-reject-long",
            format!("{}€{}", "x".repeat(510), "y".repeat(100)),
        ),
    ] {
        let session = open(scenario);
        let mut c2 = session
            .open_control_channel(&control_config())
            .expect("open C2");
        c2.enable().expect("enable");
        let mut request = c2.submit_operate(1).expect("submit");
        let expected = ModeResult::Rejected {
            code: MelErrorCode::InvalidParameters,
            description: expected,
        };
        assert_eq!(request.wait(1_000).expect("rejection"), expected);
        assert_eq!(request.wait(0).expect("cached rejection"), expected);
    }
}

#[test]
fn preserves_terminal_and_submission_failure_kinds() {
    let _guard = C2_TEST.lock().expect("C2 test lock");
    for (scenario, expected) in [
        ("c2-future-throw", ErrorKind::ProviderException),
        ("c2-null-result", ErrorKind::ProviderFailed),
    ] {
        let session = open(scenario);
        let mut c2 = session
            .open_control_channel(&control_config())
            .expect("open C2");
        c2.enable().expect("enable");
        let mut request = c2.submit_operate(1).expect("request was published");
        assert_eq!(
            request.wait(1_000).expect_err("terminal failure").kind(),
            &expected
        );
    }

    let session = open("c2-enable-fail");
    let mut c2 = session
        .open_control_channel(&control_config())
        .expect("open C2");
    assert_eq!(
        c2.enable().expect_err("enable failure").kind(),
        &ErrorKind::ProviderFailed
    );
    assert_eq!(
        c2.submit_operate(1).expect_err("still disabled").kind(),
        &ErrorKind::ProviderFailed
    );

    let session = open("c2-send-throw");
    let mut c2 = session
        .open_control_channel(&control_config())
        .expect("open C2");
    c2.enable().expect("enable");
    assert_eq!(
        c2.submit_operate(1).expect_err("send exception").kind(),
        &ErrorKind::ProviderException
    );
}

#[test]
fn dropping_pending_request_is_non_cancelling_and_cleanup_is_ordered() {
    let _guard = C2_TEST.lock().expect("C2 test lock");
    let log = LifetimeLog::new("c2-pending");
    let session = open("c2-lifetime");
    let mut c2 = session
        .open_control_channel(&control_config())
        .expect("open C2");
    c2.enable().expect("enable");
    let request = c2.submit_operate(8).expect("submit");
    drop(request);
    c2.close().expect("C2 close remains nonblocking");
    session.close().expect("session close remains nonblocking");

    let events = log.wait_for("library_unloaded");
    assert_order(&events, "mode_completed", "c2_channel_destroyed");
    assert_order(&events, "c2_channel_destroyed", "control_destroyed");
    assert_order(&events, "control_destroyed", "manager_destroyed");
    assert_order(&events, "manager_destroyed", "library_unloaded");
}

#[test]
fn retryable_channel_close_retains_then_clears_owner() {
    let _guard = C2_TEST.lock().expect("C2 test lock");
    let session = open("c2-detach-fail");
    let mut c2 = session
        .open_control_channel(&control_config())
        .expect("open C2");
    assert_eq!(
        c2.close().expect_err("first detach fails").kind(),
        &ErrorKind::ProviderFailed
    );
    assert!(c2.is_open());
    c2.close().expect("retry succeeds");
    assert!(!c2.is_open());
    session.close().expect("session close");
}

#[test]
fn safe_config_selects_c2_and_rejects_nul_before_open() {
    let _guard = C2_TEST.lock().expect("C2 test lock");
    let session = open("c2-config");
    let mut c2 = session
        .open_control_channel(&control_config())
        .expect("mock validates C2 type and complete config");
    c2.close().expect("C2 close");
    session.close().expect("session close");

    for error in [
        UciId::new([0; 16], "bad\0label").expect_err("label NUL"),
        ComponentLocation::new(0.0, 0.0, 0.0, "bad\0key", "system").expect_err("key NUL"),
        ComponentLocation::new(0.0, 0.0, 0.0, "key", "bad\0system").expect_err("system NUL"),
    ] {
        assert_eq!(error.kind(), &ErrorKind::InvalidArgument);
    }
}

#[test]
fn image_and_c2_graphs_coexist_after_parent_close() {
    let _guard = C2_TEST.lock().expect("C2 test lock");
    let log = LifetimeLog::new("coexist");
    let session = open("c2-coexist");
    let mut stream = session
        .open_image_stream(&image_config())
        .expect("open image stream");
    let mut c2 = session
        .open_control_channel(&control_config())
        .expect("open C2");
    c2.enable().expect("enable C2");
    let mut request = c2.submit_operate(42).expect("submit");
    stream.start().expect("start image stream");
    session.close().expect("close parent");
    assert_eq!(stream.receive(1_000).expect("one frame").pixels.len(), 12);
    c2.close().expect("close public C2 while pending");
    assert_eq!(
        request.wait(1_000).expect("request completion"),
        ModeResult::Success {
            mode: MfaMode::TaskSched
        }
    );
    request.close().expect("request close");
    stream.close().expect("stream close");

    let events = log.wait_for("library_unloaded");
    assert_order(&events, "mode_completed", "c2_channel_destroyed");
    assert_order(&events, "c2_channel_destroyed", "library_unloaded");
    assert_order(
        &events,
        "callbacks_quiesced_by_channel_destruction",
        "library_unloaded",
    );
}

fn open(scenario: &str) -> Session {
    Session::open(mock_provider(), scenario, "").expect("open session")
}

fn control_config() -> ControlConfig {
    ControlConfig::new(
        id(0, "IR C2 channel"),
        id(0xf0, "test platform"),
        location(),
    )
}

fn image_config() -> ImageConfig {
    ImageConfig::with_limits(
        id(0, "IR image channel"),
        id(0, "test platform"),
        ComponentLocation::new(0.0, 0.0, 0.0, "station-1", "mock-aircraft")
            .expect("image location"),
        2,
        64,
        2,
    )
    .expect("image config")
}

fn id(start: u8, label: &str) -> UciId {
    let mut uuid = [0; 16];
    for (index, byte) in uuid.iter_mut().enumerate() {
        *byte = start.wrapping_add(index as u8);
    }
    UciId::new(uuid, label).expect("valid ID")
}

fn location() -> ComponentLocation {
    ComponentLocation::new(1.25, -2.5, 3.75, "station-1", "mock-aircraft").expect("location")
}

fn mock_provider() -> PathBuf {
    let repository = Path::new(env!("CARGO_MANIFEST_DIR")).join("../..");
    env::var_os("AMS_MEL_TEST_PROVIDER_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| repository.join("native/build/test-providers"))
        .join("libmock_ir_provider.so")
}

fn assert_order(events: &str, first: &str, second: &str) {
    let first = events.find(first).expect("first event");
    let second = events.find(second).expect("second event");
    assert!(first < second, "events out of order:\n{events}");
}

struct LifetimeLog {
    directory: PathBuf,
    path: PathBuf,
}

impl LifetimeLog {
    fn new(name: &str) -> Self {
        let sequence = NEXT_LOG.fetch_add(1, Ordering::Relaxed);
        let directory = env::temp_dir().join(format!(
            "ams-mel-rust-{name}-{}-{sequence}",
            std::process::id()
        ));
        fs::create_dir(&directory).expect("create lifetime directory");
        let path = directory.join("lifetime.log");
        env::set_var("AMS_MEL_TEST_LIFETIME_LOG", &path);
        Self { directory, path }
    }

    fn contents(&self) -> String {
        fs::read_to_string(&self.path).unwrap_or_default()
    }

    fn wait_for(&self, event: &str) -> String {
        for _ in 0..200 {
            let contents = self.contents();
            if contents.contains(event) {
                return contents;
            }
            thread::sleep(Duration::from_millis(1));
        }
        panic!("timed out waiting for {event}:\n{}", self.contents());
    }
}

impl Drop for LifetimeLog {
    fn drop(&mut self) {
        env::remove_var("AMS_MEL_TEST_LIFETIME_LOG");
        let _ = fs::remove_file(&self.path);
        let _ = fs::remove_dir(&self.directory);
    }
}
