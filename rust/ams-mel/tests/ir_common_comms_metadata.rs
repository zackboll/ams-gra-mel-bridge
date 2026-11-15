//! Real native callbacks with process-isolated provider globals and lifetime logs.
use ams_mel::*;
use std::{
    env,
    ffi::{c_char, c_void, CString},
    fs,
    path::{Path, PathBuf},
    process::Command,
};

const CHILD: &str = "AMS_MEL_RUST_COMMON_METADATA_CHILD";
const SYNC: CommsTestReport = CommsTestReport {
    command_id: 0xabcdef01,
    request_id: 0xfedcba98,
};

unsafe extern "C" {
    fn dlopen(path: *const c_char, flags: i32) -> *mut c_void;
    fn dlsym(handle: *mut c_void, name: *const c_char) -> *mut c_void;
    fn dlclose(handle: *mut c_void) -> i32;
    fn ams_mel_test_completion_boundary(family: u32, op: u32, target: u64, out: *mut u64) -> i32;
    fn ams_mel_test_completion_owner(family: u32, target: u64, out: *mut u64) -> i32;
}

fn reclaimed() {
    {
        let family = 2;
        let mut counts = [0; 4];
        let mut observed = 0;
        // SAFETY: exact established test-only completion/reclamation probe signatures.
        unsafe {
            assert_eq!(
                ams_mel_test_completion_boundary(family, 3, 0, counts.as_mut_ptr()),
                1
            );
            assert_eq!(
                ams_mel_test_completion_owner(family, counts[2], &mut observed),
                1
            );
        }
    }
}

fn provider() -> PathBuf {
    env::var_os("AMS_MEL_TEST_PROVIDER_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| {
            Path::new(env!("CARGO_MANIFEST_DIR")).join("../../native/build-tests/test-providers")
        })
        .join("libmock_ir_provider.so")
}

struct Gate(*mut c_void);
impl Gate {
    fn new() -> Self {
        let path = CString::new(provider().as_os_str().as_encoded_bytes()).unwrap();
        // SAFETY: test-only extra DSO pin, released before checking unload.
        let pin = unsafe { dlopen(path.as_ptr(), 2) };
        assert!(!pin.is_null());
        Self(pin)
    }
    fn release(&self, family: u32) {
        // SAFETY: exact mock completion gate signature, symbol valid through the pin.
        unsafe {
            let symbol = dlsym(self.0, c"mock_completion_release_family".as_ptr());
            assert!(!symbol.is_null());
            let release: unsafe extern "C" fn(u32, u32) -> i32 = std::mem::transmute(symbol);
            assert_eq!(release(family, 1), 1);
        }
    }
}
impl Drop for Gate {
    fn drop(&mut self) {
        // SAFETY: unique dlopen reference, not a facade owner.
        assert_eq!(unsafe { dlclose(self.0) }, 0);
    }
}

enum Typed {
    Health(HealthStatusChannel),
    Image(Option<ImageStream>),
    Stacked(StackedChannel),
}
impl Typed {
    fn view(&mut self) -> ChannelView {
        match self {
            Self::Health(c) => c.channel_view(),
            Self::Image(c) => c.as_mut().unwrap().channel_view(),
            Self::Stacked(c) => c.channel_view(),
        }
        .unwrap()
    }
    fn enable(&mut self) {
        match self {
            Self::Health(c) => c.enable(),
            Self::Image(c) => c.as_mut().unwrap().start(),
            Self::Stacked(c) => c.enable(),
        }
        .unwrap();
    }
    fn close(&mut self) -> Result<(), Error> {
        match self {
            Self::Health(c) => c.close(),
            Self::Image(c) => c.take().map_or(Ok(()), ImageStream::close),
            Self::Stacked(c) => c.close(),
        }
    }
}

fn identities() -> (UciId, UciId, ComponentLocation) {
    (
        UciId::new([0; 16], "").unwrap(),
        UciId::new([0; 16], "").unwrap(),
        ComponentLocation::new(0., 0., 0., "", "").unwrap(),
    )
}
fn attach(session: &Session, family: &str) -> (Typed, u32) {
    let (channel, platform, location) = identities();
    match family {
        "health" => (
            Typed::Health(
                session
                    .open_health_channel(&HealthConfig::new(channel, platform, location))
                    .unwrap(),
            ),
            4,
        ),
        "stacked" => (
            Typed::Stacked(
                session
                    .open_stacked_channel(&StackedConfig::new(channel, platform, location))
                    .unwrap(),
            ),
            6,
        ),
        "acquisition" => (
            Typed::Image(Some(
                session
                    .open_image_stream(
                        &ImageConfig::with_limits(channel, platform, location, 3, 1024, 4).unwrap(),
                    )
                    .unwrap(),
            )),
            1,
        ),
        "control" => (
            Typed::Image(Some(
                session
                    .open_image_control(&ImageControlConfig::new(channel, platform, location))
                    .unwrap(),
            )),
            1,
        ),
        _ => panic!("unknown family"),
    }
}

fn assert_error<T: std::fmt::Debug>(result: Result<T, Error>, kind: ErrorKind) {
    assert_eq!(result.unwrap_err().kind(), &kind);
}

fn submit(view: &mut ChannelView, route: u32, index: u32) -> (CommsRequest, CommsTestReport) {
    let ids = CommsTestRequest {
        command_id: u32::MAX - index,
        channel_id: route,
        request_id: 0x80000000 + index,
    };
    (
        view.submit_comms_test(ids).unwrap(),
        CommsTestReport {
            command_id: ids.command_id,
            request_id: ids.request_id,
        },
    )
}
fn complete(mut request: CommsRequest, report: CommsTestReport) {
    assert_eq!(
        request.wait(1000).unwrap(),
        CommsTestResult::Completed { report }
    );
    request.close().unwrap();
}

fn run(family: &str, case: &str) {
    let directory = env::temp_dir().join(format!("rust-common-metadata-{}", std::process::id()));
    fs::create_dir(&directory).unwrap();
    let log = directory.join("events");
    env::set_var("AMS_MEL_TEST_LIFETIME_LOG", &log);
    let scenario = match case {
        "zero" => "zero",
        "held-drain" => "held",
        "weak" => "sync",
        "owner-first" => "parity",
        "drain" => "parity",
        _ => case,
    };
    let mut session =
        Some(Session::open(provider(), format!("common-meta-{scenario}"), "").unwrap());
    let (mut typed, route) = attach(session.as_ref().unwrap(), family);
    if case == "enabled" || case == "disable-fail" {
        typed.enable();
    }
    let mut view = typed.view();
    let mut second = typed.view();
    assert_eq!(
        view.open_comms_metadata(0).unwrap_err().error().kind(),
        &ErrorKind::InvalidArgument
    );
    if ["refuse", "fail", "partial", "throw"].contains(&case) {
        let error = view.open_comms_metadata(2).unwrap_err();
        assert_eq!(
            error.error().kind(),
            &if case == "throw" {
                ErrorKind::ProviderException
            } else {
                ErrorKind::ProviderFailed
            }
        );
        assert_eq!(
            error.registration(),
            match case {
                "throw" => None,
                "refuse" => Some(CommandReturn::NotSupported),
                _ => Some(CommandReturn::Fail),
            }
        );
        assert!(!error.error().diagnostic().unwrap().is_empty());
        assert!(view.open_comms_metadata(2).is_err());
        assert!(second.open_comms_metadata(2).is_err());
        view.close().unwrap();
        second.close().unwrap();
        typed.close().unwrap();
        session.take().unwrap().close().unwrap();
        let text = fs::read_to_string(&log).unwrap();
        assert_eq!(
            text.lines()
                .filter(|s| *s == "common_comms_registration")
                .count(),
            1
        );
        if case == "partial" || case == "throw" {
            assert!(text.contains("common_comms_partial_callback_at_destruction"));
        }
    } else {
        let mut metadata = view.open_comms_metadata(2).unwrap();
        if case == "zero" {
            let cap = second.capabilities().unwrap();
            assert_eq!((cap.width, cap.height, cap.buffer_size), (0, 0, 0));
        }
        assert!(metadata.is_open());
        assert!(view.open_comms_metadata(2).is_err());
        assert!(second.open_comms_metadata(2).is_err());
        match case {
            "sync" | "weak" => {
                view.close().unwrap();
                assert!(second.capabilities().is_ok()); // weak Close does not close typed channel
                if case == "weak" {
                    typed.close().unwrap();
                    session.take().unwrap().close().unwrap();
                    // Queue-only native owner and weak Views do not pin provider code.
                    assert!(fs::read_to_string(&log)
                        .unwrap()
                        .contains("library_unloaded"));
                }
                assert_eq!(metadata.receive(0).unwrap(), SYNC);
                assert_error(
                    metadata.receive(0),
                    if case == "weak" {
                        ErrorKind::StreamStopped
                    } else {
                        ErrorKind::Timeout
                    },
                );
            }
            "overflow" => {
                assert_eq!(
                    metadata.counters().unwrap(),
                    CommsMetadataCounters {
                        events_received: 6,
                        events_dropped_queue_full: 4,
                        malformed_or_unsupported: 0
                    }
                );
                for index in 0..2 {
                    assert_eq!(
                        metadata.receive(0).unwrap(),
                        CommsTestReport {
                            command_id: 0x80000000 + index,
                            request_id: 0xf0000000 + index
                        }
                    );
                }
                assert_error(metadata.receive(0), ErrorKind::Timeout);
                assert_eq!(metadata.counters().unwrap().events_received, 6);
            }
            "null" => {
                assert_eq!(
                    metadata.counters().unwrap(),
                    CommsMetadataCounters {
                        events_received: 1,
                        events_dropped_queue_full: 0,
                        malformed_or_unsupported: 1
                    }
                );
                assert_error(metadata.receive(0), ErrorKind::Timeout);
            }
            "held" | "held-drain" => {
                let gate = Gate::new();
                let (mut pending, report) = submit(&mut second, route, 0);
                assert_eq!(metadata.receive(0).unwrap(), report); // callback independent of future
                assert_error(pending.wait(0), ErrorKind::Timeout);
                assert_error(metadata.receive(1), ErrorKind::Timeout); // finite wait does not cancel
                if case == "held" {
                    metadata.close().unwrap();
                }
                view.close().unwrap();
                second.close().unwrap();
                typed.close().unwrap();
                session.take().unwrap().close().unwrap();
                assert!(!fs::read_to_string(&log)
                    .unwrap()
                    .contains("library_unloaded"));
                gate.release(if route == 6 { 14 } else { 2 });
                complete(pending, report);
                if case == "held-drain" {
                    assert_error(metadata.receive(0), ErrorKind::StreamStopped);
                }
                reclaimed();
                drop(gate);
            }
            "detach-fail" | "disable-fail" => {
                let (pending, report) = submit(&mut second, route, 0);
                complete(pending, report);
                reclaimed();
                assert_error(typed.close(), ErrorKind::ProviderFailed);
                assert_eq!(metadata.receive(0).unwrap(), report);
                assert_error(metadata.receive(0), ErrorKind::ProviderFailed);
                // Native typed owners have their own documented detach retry policy.
                let _ = typed.close();
            }
            _ => {
                assert_eq!(
                    metadata.counters().unwrap(),
                    CommsMetadataCounters {
                        events_received: 0,
                        events_dropped_queue_full: 0,
                        malformed_or_unsupported: 0
                    }
                );
                assert_error(metadata.receive(0), ErrorKind::Timeout);
                assert_error(metadata.receive(1), ErrorKind::Timeout);
                if case == "owner-first" {
                    metadata.close().unwrap();
                }
                let mut reports = Vec::new();
                for index in 0..2 {
                    let (pending, report) = submit(&mut second, route, index);
                    complete(pending, report);
                    reports.push(report);
                }
                reclaimed();
                if case == "drain" {
                    typed.close().unwrap();
                }
                if case != "owner-first" && case != "future-only" {
                    for report in reports {
                        assert_eq!(metadata.receive(0).unwrap(), report);
                    }
                    assert_eq!(metadata.counters().unwrap().events_received, 2);
                }
                if case == "future-only" {
                    assert_eq!(metadata.counters().unwrap().events_received, 0);
                    assert_error(metadata.receive(0), ErrorKind::Timeout);
                }
                if case == "drain" {
                    assert_error(metadata.receive(0), ErrorKind::StreamStopped);
                }
            }
        }
        metadata.close().unwrap();
        metadata.close().unwrap();
        assert!(!metadata.is_open());
        second.close().unwrap();
        view.close().unwrap();
        assert_eq!(
            view.open_comms_metadata(2).unwrap_err().error().kind(),
            &ErrorKind::ProviderFailed
        );
        // Idempotent after successful/native-consumed closure.
        typed.close().unwrap();
        if let Some(session) = session {
            session.close().unwrap();
        }
    }
    reclaimed();
    let text = fs::read_to_string(&log).unwrap();
    assert_eq!(
        text.lines().filter(|s| *s == "library_unloaded").count(),
        1,
        "{text}"
    );
    if family == "control" {
        for forbidden in [
            "buffer_factory_called",
            "buffer_initialized",
            "buffer_registered",
            "buffer_unregistered",
        ] {
            assert!(!text.lines().any(|s| s == forbidden), "{text}");
        }
    }
    fs::remove_dir_all(directory).unwrap();
}

macro_rules! cases {
    ($($name:ident => $case:literal),+ $(,)?) => { $(#[test] fn $name() {
        for family in ["health", "control", "acquisition", "stacked"] {
            if $case == "zero" && family != "control" { continue; }
            // External deadline is a failure bound, never a race synchronization mechanism.
            let output = Command::new("timeout").args(["30s"])
                .arg(env::current_exe().unwrap()).args(["--exact", "scenario_child", "--ignored", "--nocapture"])
                .env(CHILD, format!("{family}:{}", $case)).output().unwrap();
            assert!(output.status.success(), "{family}:{}\n{}\n{}", $case,
                String::from_utf8_lossy(&output.stdout), String::from_utf8_lossy(&output.stderr));
            assert!(String::from_utf8_lossy(&output.stdout).contains("COMMON_METADATA_COMPLETE"));
        }
    })+ };
}
cases! {
    attached_fifo => "parity", enabled_registration => "enabled", synchronous_registration => "sync",
    weak_view_close => "weak", refused_registration => "refuse", failed_registration => "fail",
    partial_retained_registration => "partial", throwing_registration => "throw",
    incoming_overflow => "overflow", null_report => "null", future_only_negative_control => "future-only",
    drain_after_typed_close => "drain", close_with_pending => "held", consumer_first => "owner-first",
    failed_detach_after_drain => "detach-fail", failed_disable_after_drain => "disable-fail",
    zero_layout_registration => "zero", pending_survives_typed_close => "held-drain",
}

#[test]
#[ignore = "isolated subprocess entry, exercised by parent cases"]
fn scenario_child() {
    let value = env::var(CHILD).unwrap();
    let (family, case) = value.split_once(':').unwrap();
    run(family, case);
    println!("COMMON_METADATA_COMPLETE {value}");
}
