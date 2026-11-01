//! Mock-only RUST-IR-003 qualification. Each scenario/family runs in an isolated
//! child: the mock's environment, DSO lifetime log, failpoints and barriers are
//! process-global. Barriers use observed files, never sleeps.

use std::{
    env, fs,
    path::{Path, PathBuf},
    process::Command,
    time::{Duration, Instant},
};

use ams_mel::{
    BandInfo, BandType, CommandReturn, ComponentLocation, ErrorKind, HealthConfig,
    HealthStatusChannel, ImageConfig, ImageStream, NucCounters, NucOpenError, NucPixelValue,
    NucPointType, NucStream, NucTempData, NucTempPoint, Session, UciId,
};

const CHILD: &str = "AMS_MEL_RUST_NUC_CHILD";
const BARRIER: &str = "AMS_MEL_TEST_NUC_CALLBACK_BARRIER";

struct SessionOwner(Option<Session>);

impl SessionOwner {
    fn close(&mut self) -> Result<(), ams_mel::Error> {
        if let Some(session) = self.0.take() {
            session.close()
        } else {
            Ok(())
        }
    }
}

impl std::ops::Deref for SessionOwner {
    type Target = Session;
    fn deref(&self) -> &Session {
        self.0.as_ref().expect("live test session")
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

fn id(label: &str) -> UciId {
    UciId::new([0; 16], label).unwrap()
}

fn location() -> ComponentLocation {
    ComponentLocation::new(0.0, 0.0, 0.0, "key", "system").unwrap()
}

enum Parent {
    Image(ImageStream),
    Health(HealthStatusChannel),
    Closed,
}

impl Parent {
    fn open(session: &Session, image: bool) -> Self {
        if image {
            Self::Image(
                session
                    .open_image_stream(
                        &ImageConfig::with_limits(
                            id("image"),
                            id("platform"),
                            location(),
                            3,
                            64,
                            2,
                        )
                        .unwrap(),
                    )
                    .unwrap(),
            )
        } else {
            Self::Health(
                session
                    .open_health_channel(&HealthConfig::new(
                        id("health"),
                        id("platform"),
                        location(),
                    ))
                    .unwrap(),
            )
        }
    }

    fn nuc(&mut self, capacity: usize) -> Result<NucStream, NucOpenError> {
        match self {
            Self::Image(parent) => parent.open_nuc(capacity),
            Self::Health(parent) => parent.open_nuc(capacity),
            Self::Closed => panic!("closed test parent"),
        }
    }

    fn close(&mut self) {
        match std::mem::replace(self, Self::Closed) {
            Self::Image(parent) => parent.close().unwrap(),
            Self::Health(mut parent) => parent.close().unwrap(),
            Self::Closed => (),
        }
    }

    fn usable(&mut self) {
        match self {
            Self::Image(parent) => {
                parent.start().unwrap();
                // Ordinary frame reception is still available; the scenario
                // determines whether enable publishes a frame or not.
                match parent.receive(0) {
                    Ok(frame) => assert!(!frame.pixels.is_empty()),
                    Err(error) => assert_eq!(error.kind(), &ErrorKind::Timeout),
                }
            }
            Self::Health(parent) => {
                parent.enable().unwrap();
                assert!(!parent.capabilities().unwrap().channel_types.is_empty());
            }
            Self::Closed => panic!("closed test parent"),
        }
    }
}

fn counters(received: u64, dropped: u64, malformed: u64) -> NucCounters {
    NucCounters {
        events_received: received,
        events_dropped_queue_full: dropped,
        malformed_or_unsupported: malformed,
    }
}

fn rich() -> Vec<NucTempData> {
    let point = |timestamp_ns,
                 kind,
                 min_wavelength_m,
                 max_wavelength_m,
                 pixel,
                 temperature_k,
                 point_type| NucTempPoint {
        timestamp_ns,
        frame_band: BandInfo {
            kind,
            min_wavelength_m,
            max_wavelength_m,
        },
        pixel,
        temperature_k,
        point_type,
    };
    vec![
        NucTempData { points: vec![] },
        NucTempData {
            points: vec![point(
                i64::MIN,
                BandType::IrMidwave,
                3.0e-6,
                5.0e-6,
                NucPixelValue::U8(255),
                300.25,
                NucPointType::NucPoint,
            )],
        },
        NucTempData {
            points: vec![
                point(
                    -1,
                    BandType::IrLongwave,
                    8.0e-6,
                    1.2e-5,
                    NucPixelValue::U16(65535),
                    0.0,
                    NucPointType::MfaEstimate,
                ),
                point(
                    i64::MAX,
                    BandType::UvVacuum,
                    1.0e-8,
                    2.0e-7,
                    NucPixelValue::U32(u32::MAX),
                    1.0e6,
                    NucPointType::Other,
                ),
                point(
                    42,
                    BandType::Invalid,
                    -1.0,
                    0.0,
                    NucPixelValue::U8(0),
                    -5.5,
                    NucPointType::NucPoint,
                ),
            ],
        },
    ]
}

fn drain(stream: &mut NucStream, count: usize) -> Vec<NucTempData> {
    (0..count).map(|_| stream.receive(0).unwrap()).collect()
}

fn expect(stream: &mut NucStream, kind: ErrorKind) {
    assert_eq!(stream.receive(0).unwrap_err().kind(), &kind);
}

struct Scratch(PathBuf);

impl Scratch {
    fn new() -> Self {
        let dir = env::temp_dir().join(format!("ams-rust-nuc-{}", std::process::id()));
        fs::create_dir(&dir).unwrap();
        let scratch = Self(dir);
        env::set_var("AMS_MEL_TEST_LIFETIME_LOG", scratch.0.join("lifetime"));
        scratch
    }

    fn events(&self) -> String {
        fs::read_to_string(self.0.join("lifetime")).unwrap_or_default()
    }

    fn record(&self, line: &str) {
        use std::io::Write;
        writeln!(
            fs::OpenOptions::new()
                .append(true)
                .create(true)
                .open(self.0.join("lifetime"))
                .unwrap(),
            "{line}"
        )
        .unwrap();
    }

    fn unloaded(&self) {
        assert!(
            self.events()
                .ends_with("control_destroyed\nmanager_destroyed\nlibrary_unloaded\n"),
            "{}",
            self.events()
        );
    }
}

impl Drop for Scratch {
    fn drop(&mut self) {
        env::remove_var("AMS_MEL_TEST_LIFETIME_LOG");
        env::remove_var(BARRIER);
        env::remove_var("AMS_MEL_TEST_NUC_CALLBACK_FAILURE");
        let _ = fs::remove_dir_all(&self.0);
    }
}

fn order(events: &str, expected: &[&str]) {
    let positions: Vec<_> = expected
        .iter()
        .map(|line| {
            events
                .lines()
                .position(|v| v == *line)
                .unwrap_or_else(|| panic!("missing {line}: {events}"))
        })
        .collect();
    assert!(
        positions.windows(2).all(|pair| pair[0] < pair[1]),
        "{events}"
    );
}

fn run(case: &str, image: bool) {
    let output = Command::new(env::current_exe().unwrap())
        .args([
            "--exact",
            "child_entry",
            "--ignored",
            "--nocapture",
            "--test-threads=1",
        ])
        .env(
            CHILD,
            format!("{case}:{}", if image { "image" } else { "health" }),
        )
        .env_remove("AMS_MEL_TEST_LIFETIME_LOG")
        .env_remove(BARRIER)
        .env_remove("AMS_MEL_TEST_NUC_CALLBACK_FAILURE")
        .output()
        .unwrap();
    assert!(
        output.status.success(),
        "{case}/{image}:\n{}\n{}",
        String::from_utf8_lossy(&output.stdout),
        String::from_utf8_lossy(&output.stderr)
    );
    assert!(String::from_utf8_lossy(&output.stdout).contains("NUC_CHILD_COMPLETE"));
}

macro_rules! families {
    ($($name:ident => $case:literal),+ $(,)?) => {$(
        #[test]
        fn $name() { for image in [true, false] { run($case, image); } }
    )+};
}

families! {
    rich_synchronous_complete_owned_data => "rich",
    optional_not_supported_preserves_owner_and_metadata => "unsupported",
    registration_fail_is_not_exception => "fail",
    registration_exception_has_no_return => "throw",
    malformed_callback_events_and_counters => "malformed",
    drop_incoming_preserves_first_two => "overflow",
    callback_allocation_failure_is_only_malformed => "allocation",
    late_callback_and_queued_drain_after_parent_close => "late",
    session_first_close_retains_channel => "session-first",
    explicit_repeated_close_and_closed_operations => "close",
    drop_without_explicit_close => "drop",
    image_stop_or_health_destruction_drains_then_stops => "stop",
    callback_return_precedes_unload_after_public_close => "barrier",
    callback_return_precedes_unload_after_public_drop => "barrier-drop",
    independent_image_and_health_nuc_streams => "independent",
}

#[test]
#[ignore = "isolated child entry selected by parent tests"]
fn child_entry() {
    let selected = env::var(CHILD).expect("child selection");
    let (case, family) = selected.split_once(':').unwrap();
    let image = family == "image";
    let scratch = Scratch::new();
    if case == "independent" {
        independent(&scratch, image);
        println!("NUC_CHILD_COMPLETE");
        return;
    }
    let scenario = match case {
        "unsupported" if image => "image-metadata-four-mixed",
        "unsupported" => "health-rich",
        "fail" => "nuc-register-fail",
        "throw" => "nuc-register-throw",
        "malformed" => "nuc-malformed",
        "overflow" => "nuc-overflow",
        "allocation" => "nuc-allocation",
        "late" => "nuc-late",
        "barrier" | "barrier-drop" => "nuc-nonquiescing",
        _ => "nuc-rich",
    };
    if case.starts_with("barrier") {
        env::set_var(BARRIER, scratch.0.join("barrier"));
    }
    let mut session = SessionOwner(Some(Session::open(provider(), scenario, "").unwrap()));
    let mut parent = Parent::open(&session, image);
    if ["unsupported", "fail", "throw"].contains(&case) {
        let error = parent.nuc(4).unwrap_err();
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
                "unsupported" => Some(CommandReturn::NotSupported),
                "fail" => Some(CommandReturn::Fail),
                _ => None,
            }
        );
        let again = parent.nuc(4).unwrap_err();
        assert_eq!(again.error().kind(), &ErrorKind::InvalidArgument);
        assert_eq!(again.registration(), None);
        parent.usable();
        if let Parent::Health(health) = &mut parent {
            let mut ordinary = health.open_metadata(32).unwrap();
            if case == "unsupported" {
                ordinary.receive(0).unwrap();
            } else {
                assert_eq!(ordinary.receive(0).unwrap_err().kind(), &ErrorKind::Timeout);
            }
            ordinary.close().unwrap();
        }
        parent.close();
        session.close().unwrap();
        if case == "fail" {
            order(
                &scratch.events(),
                &[
                    "late_nuc_callback_invoked",
                    "late_nuc_callback_returned",
                    "library_unloaded",
                ],
            );
        }
        if case != "unsupported" {
            assert_eq!(
                scratch
                    .events()
                    .lines()
                    .filter(|l| *l == "nuc_registration_attempted")
                    .count(),
                1
            );
        }
        scratch.unloaded();
        println!("NUC_CHILD_COMPLETE");
        return;
    }
    // Zero capacity never reaches native registration and never consumes it.
    let zero = parent.nuc(0).unwrap_err();
    assert_eq!(zero.error().kind(), &ErrorKind::InvalidArgument);
    assert_eq!(zero.registration(), None);
    assert!(!scratch.events().contains("nuc_registration_attempted"));
    if case == "allocation" {
        env::set_var("AMS_MEL_TEST_NUC_CALLBACK_FAILURE", "allocation");
    }
    // Ordinary Health metadata registration does not register NUC and remains
    // usable/independent before and after the explicit NUC registration.
    let mut ordinary = if let Parent::Health(health) = &mut parent {
        let mut ordinary = health.open_metadata(4).unwrap();
        assert_eq!(ordinary.receive(0).unwrap_err().kind(), &ErrorKind::Timeout);
        assert!(!scratch.events().contains("nuc_registration_attempted"));
        Some(ordinary)
    } else {
        None
    };
    let mut stream = parent.nuc(if case == "overflow" { 2 } else { 4 }).unwrap();
    assert!(stream.is_open());
    assert_eq!(
        parent.nuc(4).unwrap_err().error().kind(),
        &ErrorKind::InvalidArgument
    );
    if let Some(ordinary) = &mut ordinary {
        assert_eq!(ordinary.receive(0).unwrap_err().kind(), &ErrorKind::Timeout);
        ordinary.close().unwrap();
    }
    let mut copied = None;
    match case {
        "rich" => {
            assert_eq!(stream.counters().unwrap(), counters(3, 0, 0));
            copied = Some(drain(&mut stream, 3));
            assert_eq!(copied.as_ref().unwrap(), &rich());
            expect(&mut stream, ErrorKind::Timeout);
        }
        "malformed" => {
            assert_eq!(stream.counters().unwrap(), counters(4, 0, 3));
            let data = stream.receive(0).unwrap();
            assert_eq!(data.points.len(), 1);
            assert_eq!(data.points[0].timestamp_ns, 5);
            assert_eq!(data.points[0].pixel, NucPixelValue::U16(1234));
            expect(&mut stream, ErrorKind::Timeout);
        }
        "overflow" => {
            assert_eq!(stream.counters().unwrap(), counters(5, 3, 0));
            for timestamp in [0, 1] {
                assert_eq!(stream.receive(0).unwrap().points[0].timestamp_ns, timestamp);
            }
            expect(&mut stream, ErrorKind::Timeout);
        }
        "allocation" => {
            assert!(env::var_os("AMS_MEL_TEST_NUC_CALLBACK_FAILURE").is_none());
            assert_eq!(stream.counters().unwrap(), counters(2, 0, 1));
            assert_eq!(stream.receive(0).unwrap().points[0].timestamp_ns, 2);
            expect(&mut stream, ErrorKind::Timeout);
        }
        "late" => {
            parent.close();
            session.close().unwrap();
            scratch.unloaded();
            copied = Some(drain(&mut stream, 3));
            assert_eq!(copied.as_ref().unwrap(), &rich());
            expect(&mut stream, ErrorKind::StreamStopped);
            assert_eq!(stream.counters().unwrap(), counters(4, 0, 0));
            order(
                &scratch.events(),
                &[
                    "late_nuc_callback_invoked",
                    "late_nuc_callback_returned",
                    "library_unloaded",
                ],
            );
        }
        "session-first" => {
            session.close().unwrap();
            assert!(!scratch.events().contains("library_unloaded"));
            copied = Some(drain(&mut stream, 3));
            assert_eq!(copied.as_ref().unwrap(), &rich());
            expect(&mut stream, ErrorKind::Timeout);
            parent.close();
            scratch.unloaded();
            expect(&mut stream, ErrorKind::StreamStopped);
        }
        "stop" => {
            match &mut parent {
                Parent::Image(image) => {
                    image.start().unwrap();
                    image.stop().unwrap();
                }
                Parent::Health(_) => parent.close(),
                Parent::Closed => unreachable!(),
            }
            copied = Some(drain(&mut stream, 3));
            expect(&mut stream, ErrorKind::StreamStopped);
        }
        "barrier" | "barrier-drop" => {
            fs::write(scratch.0.join("barrier.start"), "start").unwrap();
            let deadline = Instant::now() + Duration::from_secs(10);
            while !scratch.0.join("barrier.entered").exists() {
                assert!(Instant::now() < deadline, "callback did not enter");
                std::thread::yield_now();
            }
            if case == "barrier" {
                stream.close().unwrap();
            } else {
                drop(stream);
            }
            scratch.record("nuc_owner_closed");
            assert!(!scratch.events().contains("nuc_callback_returned"));
            // Channel destruction releases the barrier and joins the callback.
            parent.close();
            session.close().unwrap();
            order(
                &scratch.events(),
                &[
                    "nuc_callback_entered",
                    "nuc_owner_closed",
                    "nuc_callback_returned",
                    "library_unloaded",
                ],
            );
            scratch.unloaded();
            println!("NUC_CHILD_COMPLETE");
            return;
        }
        "close" => {
            stream.close().unwrap();
            stream.close().unwrap();
            assert!(!stream.is_open());
            expect(&mut stream, ErrorKind::ProviderFailed);
            assert_eq!(
                stream.counters().unwrap_err().kind(),
                &ErrorKind::ProviderFailed
            );
            parent.usable();
        }
        "drop" => {
            drop(stream);
            parent.usable();
            parent.close();
            session.close().unwrap();
            scratch.unloaded();
            println!("NUC_CHILD_COMPLETE");
            return;
        }
        other => panic!("unknown child {other}"),
    }
    stream.close().unwrap();
    drop(stream);
    parent.close();
    session.close().unwrap();
    drop(parent);
    drop(session);
    scratch.unloaded();
    if let Some(copied) = copied {
        assert_eq!(copied, rich());
    }
    println!("NUC_CHILD_COMPLETE");
}

fn independent(scratch: &Scratch, image_first: bool) {
    let mut session = SessionOwner(Some(Session::open(provider(), "nuc-rich", "").unwrap()));
    let mut first = Parent::open(&session, image_first);
    let mut second = Parent::open(&session, !image_first);
    let mut a = first.nuc(4).unwrap();
    let mut b = second.nuc(4).unwrap();
    a.close().unwrap();
    first.close();
    assert_eq!(b.counters().unwrap(), counters(3, 0, 0));
    let copied = drain(&mut b, 3);
    assert_eq!(copied, rich());
    expect(&mut b, ErrorKind::Timeout);
    second.close();
    session.close().unwrap();
    scratch.unloaded();
    expect(&mut b, ErrorKind::StreamStopped);
    b.close().unwrap();
    assert_eq!(copied, rich());
}
