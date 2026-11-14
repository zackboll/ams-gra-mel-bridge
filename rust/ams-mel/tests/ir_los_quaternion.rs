//! Mock-only RUST-IR-007 qualification. Process-global provider/facade state is
//! isolated per child. Dynamic mock hooks and bridge probes exist only in tests.
use ams_mel::{
    CommandReturn, ComponentLocation, ErrorKind, ForwardRightDown, ImageConfig,
    ImageMetadataCounters, ImageStream, LineOfSightQuaternion, LineOfSightQuaternionStream,
    Quaternion, Session, UciId,
};
use std::{
    env,
    ffi::{c_char, c_void, CString},
    fs,
    path::{Path, PathBuf},
    process::Command,
};

const CHILD: &str = "AMS_MEL_RUST_LOSQ_CHILD";

fn provider() -> PathBuf {
    env::var_os("AMS_MEL_TEST_PROVIDER_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| {
            Path::new(env!("CARGO_MANIFEST_DIR")).join("../../native/build-tests/test-providers")
        })
        .join("libmock_ir_provider.so")
}

fn image(session: &Session) -> ImageStream {
    let id = || UciId::new([0; 16], "image").unwrap();
    session
        .open_image_stream(
            &ImageConfig::with_limits(
                id(),
                id(),
                ComponentLocation::new(0.0, 0.0, 0.0, "key", "system").unwrap(),
                3,
                64,
                2,
            )
            .unwrap(),
        )
        .unwrap()
}

#[link(name = "dl")]
extern "C" {
    fn dlopen(path: *const c_char, flags: i32) -> *mut c_void;
    fn dlsym(library: *mut c_void, symbol: *const c_char) -> *mut c_void;
    fn dlclose(library: *mut c_void) -> i32;
}

extern "C" {
    fn ams_mel_test_image_los_quaternion_probe(operation: u32, value: usize) -> i32;
}

fn probe(operation: u32, value: usize) -> i32 {
    // SAFETY: test-facade-only scalar probe with the compiled C11 contract.
    unsafe { ams_mel_test_image_los_quaternion_probe(operation, value) }
}

struct Hooks {
    library: *mut c_void,
    deliver_op: unsafe extern "C" fn(u32, u32, u32) -> i32,
    registrations_op: unsafe extern "C" fn() -> u32,
}
impl Hooks {
    fn open() -> Self {
        let path = CString::new(provider().as_os_str().as_encoded_bytes()).unwrap();
        // SAFETY: NUL-terminated path. RTLD_NOW | RTLD_NOLOAD only observes an
        // already loaded provider. The extra DSO reference is explicitly closed.
        unsafe {
            let library = dlopen(path.as_ptr(), 2 | 4);
            assert!(!library.is_null());
            let deliver = dlsym(library, c"mock_losq_deliver".as_ptr());
            let registrations = dlsym(library, c"mock_losq_registrations".as_ptr());
            assert!(!deliver.is_null() && !registrations.is_null());
            Self {
                library,
                deliver_op: std::mem::transmute::<
                    *mut c_void,
                    unsafe extern "C" fn(u32, u32, u32) -> i32,
                >(deliver),
                registrations_op: std::mem::transmute::<*mut c_void, unsafe extern "C" fn() -> u32>(
                    registrations,
                ),
            }
        }
    }
    fn deliver(&self, fixture: u32, tag: u32, mode: u32) {
        // SAFETY: provider remains loaded through this hook owner; exact signature.
        assert_eq!(unsafe { (self.deliver_op)(fixture, tag, mode) }, 1);
    }
    fn registrations(&self) -> u32 {
        // SAFETY: exact compiled mock hook, live DSO.
        unsafe { (self.registrations_op)() }
    }
}
impl Drop for Hooks {
    fn drop(&mut self) {
        // SAFETY: uniquely held extra dlopen reference, no outstanding hook calls.
        assert_eq!(unsafe { dlclose(self.library) }, 0);
    }
}

struct Scratch(PathBuf);
impl Scratch {
    fn new() -> Self {
        let dir = env::temp_dir().join(format!("ams-rust-losq-{}", std::process::id()));
        fs::create_dir(&dir).unwrap();
        env::set_var("AMS_MEL_TEST_LIFETIME_LOG", dir.join("lifetime"));
        Self(dir)
    }
    fn events(&self) -> String {
        fs::read_to_string(self.0.join("lifetime")).unwrap_or_default()
    }
    fn unloaded(&self) {
        assert!(
            self.events().ends_with("library_unloaded\n"),
            "{}",
            self.events()
        );
    }
}
impl Drop for Scratch {
    fn drop(&mut self) {
        env::remove_var("AMS_MEL_TEST_LIFETIME_LOG");
        let _ = fs::remove_dir_all(&self.0);
    }
}

fn run(case: &str) {
    let output = Command::new(env::current_exe().unwrap())
        .args([
            "--exact",
            "child_entry",
            "--ignored",
            "--nocapture",
            "--test-threads=1",
        ])
        .env(CHILD, case)
        .env_remove("AMS_MEL_TEST_LIFETIME_LOG")
        .output()
        .unwrap();
    assert!(
        output.status.success(),
        "{case}:\n{}\n{}",
        String::from_utf8_lossy(&output.stdout),
        String::from_utf8_lossy(&output.stderr)
    );
    assert!(String::from_utf8_lossy(&output.stdout).contains("LOSQ_CHILD_COMPLETE"));
}

macro_rules! cases { ($($name:ident),+ $(,)?) => { $(#[test] fn $name() { run(stringify!($name)); })+ }; }
cases!(
    fidelity,
    running,
    stopped_ineligible,
    capacity,
    register_fail,
    register_not_implemented,
    register_unknown,
    register_throw,
    not_supported,
    independence_base_first,
    independence_losq_first,
    independence_nuc_first,
    independence_odm_first,
    independence_others_first,
    fifo,
    blocking_receive,
    stop_empty,
    stop_drain,
    parent_first,
    session_first,
    owner_close,
    owner_drop,
    late,
    late_closed,
    detach_fail,
    in_flight
);

fn same(actual: f64, expected: f64) {
    if expected.is_nan() {
        assert!(actual.is_nan());
    } else {
        assert_eq!(
            actual.to_bits(),
            expected.to_bits(),
            "{actual:?} != {expected:?}"
        );
    }
}
fn frd(f: f64, r: f64, d: f64) -> ForwardRightDown {
    ForwardRightDown {
        forward: f,
        right: r,
        down: d,
    }
}
// Independent expectations from the compiled C11 contract's eight fixtures.
fn expected(fixture: u32, tag: u32) -> LineOfSightQuaternion {
    let (time, q, rates) = match fixture {
        0 => (0, [0.0; 4], [0.0; 3]),
        1 => (
            i64::MIN,
            [0.125, -0.25, 0.5, 0.8227241335952167],
            [1e-9, -2.5e-3, 0.0625],
        ),
        2 => (
            -1,
            [-0.125, 0.25, -0.5, -0.8227241335952167],
            [-1e-9, 2.5e-3, -0.0625],
        ),
        3 => (
            0,
            [3.0, -7.0, 11.0, -13.0],
            [6283.185307179586, -1.5e6, 42.0],
        ),
        4 => (1, [-0.0, 0.375, -0.625, 0.75], [0.875, -1.125, -0.0]),
        5 => (
            i64::MAX,
            [f64::NAN, f64::INFINITY, f64::NEG_INFINITY, 0.5],
            [f64::NEG_INFINITY, f64::NAN, f64::INFINITY],
        ),
        6 => (
            i64::from(tag),
            [f64::from_bits(1), -f64::from_bits(1), f64::MAX, -f64::MAX],
            [-f64::from_bits(1), f64::MAX, -f64::MAX],
        ),
        7 => (
            i64::from(tag),
            [f64::from(tag), 0.5, -0.5, 0.25],
            [-4.0, 8.0, -16.0],
        ),
        _ => panic!("unknown fixture {fixture}"),
    };
    LineOfSightQuaternion {
        system_time_ns: time,
        q_xyzw: Quaternion {
            x: q[0],
            y: q[1],
            z: q[2],
            w: q[3],
        },
        pqr_rps: frd(rates[0], rates[1], rates[2]),
    }
}
fn compare(value: &LineOfSightQuaternion, fixture: u32, tag: u32) {
    let e = expected(fixture, tag);
    assert_eq!(value.system_time_ns, e.system_time_ns);
    same(value.q_xyzw.x, e.q_xyzw.x);
    same(value.q_xyzw.y, e.q_xyzw.y);
    same(value.q_xyzw.z, e.q_xyzw.z);
    same(value.q_xyzw.w, e.q_xyzw.w);
    same(value.pqr_rps.forward, e.pqr_rps.forward);
    same(value.pqr_rps.right, e.pqr_rps.right);
    same(value.pqr_rps.down, e.pqr_rps.down);
    if fixture == 3 {
        let q = value.q_xyzw;
        assert_eq!(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w, 348.0);
    }
}
fn counters(stream: &mut LineOfSightQuaternionStream, received: u64, dropped: u64, malformed: u64) {
    assert_eq!(
        stream.counters().unwrap(),
        ImageMetadataCounters {
            events_received: received,
            events_dropped_queue_full: dropped,
            malformed_or_unsupported: malformed,
        }
    );
}
fn expect(stream: &mut LineOfSightQuaternionStream, kind: ErrorKind) {
    assert_eq!(stream.receive(0).unwrap_err().kind(), &kind);
}

#[test]
#[ignore = "subprocess entry, invoked by isolated scenario tests"]
fn child_entry() {
    let case = env::var(CHILD).unwrap();
    let scratch = Scratch::new();
    let scenario = match case.as_str() {
        "register_fail" => "losq-register-fail",
        "register_not_implemented" => "losq-register-not-implemented",
        "register_unknown" => "losq-register-unknown",
        "register_throw" => "losq-register-throw",
        "not_supported" | "independence_base_first" => "image-metadata-four-mixed",
        "independence_nuc_first" => "nuc-rich",
        "independence_odm_first" => "odm-sync",
        "fifo" | "blocking_receive" => "losq-capacity",
        "running" | "stopped_ineligible" => "losq-running",
        "capacity" | "stop_empty" => "losq-capacity",
        "late" => "losq-late",
        "late_closed" => "losq-late-closed",
        "detach_fail" => "losq-detach-fail",
        "in_flight" => "losq-in-flight",
        _ => "losq-sync",
    };
    if case == "in_flight" {
        assert_eq!(probe(0, 0), 1);
    }
    let session = Session::open(provider(), scenario, "").unwrap();
    let mut image = image(&session);
    let hooks = Hooks::open();
    assert_eq!(hooks.registrations(), 0);
    let error = image.open_line_of_sight_quaternion(0).unwrap_err();
    assert_eq!(error.error().kind(), &ErrorKind::InvalidArgument);
    assert_eq!(error.registration(), None);
    assert_eq!(hooks.registrations(), 0);

    if case.starts_with("register_") || case == "not_supported" {
        let (kind, registration, message) = match case.as_str() {
            "register_fail" => (
                ErrorKind::ProviderFailed,
                Some(CommandReturn::Fail),
                "LineOfSightQuaternion callback registration failed",
            ),
            "register_not_implemented" => (
                ErrorKind::ProviderFailed,
                Some(CommandReturn::NotImplemented),
                "LineOfSightQuaternion callback registration failed",
            ),
            "register_unknown" => (
                ErrorKind::ProviderFailed,
                None,
                "provider returned an unknown LineOfSightQuaternion registration result",
            ),
            "register_throw" => (
                ErrorKind::ProviderException,
                None,
                "provider exception during LineOfSightQuaternion registration",
            ),
            _ => (
                ErrorKind::ProviderFailed,
                Some(CommandReturn::NotSupported),
                "provider does not support LineOfSightQuaternion callback",
            ),
        };
        let error = image.open_line_of_sight_quaternion(4).unwrap_err();
        assert_eq!(error.error().kind(), &kind);
        assert_eq!(error.registration(), registration);
        assert_eq!(error.error().diagnostic(), Some(message));
        assert_eq!(hooks.registrations(), 1);
        assert_eq!(
            image
                .open_line_of_sight_quaternion(4)
                .unwrap_err()
                .error()
                .kind(),
            &ErrorKind::InvalidArgument
        );
        assert_eq!(hooks.registrations(), 1);
        drop(hooks);
        image.close().unwrap();
        session.close().unwrap();
        scratch.unloaded();
        if case == "register_fail" {
            assert!(scratch.events().contains("late_losq_callback_returned"));
        }
        println!("LOSQ_CHILD_COMPLETE");
        return;
    }

    if case == "independence_base_first" {
        let mut base = image.open_metadata(8).unwrap();
        assert_eq!(hooks.registrations(), 0);
        assert_eq!(
            image
                .open_line_of_sight_quaternion(4)
                .unwrap_err()
                .registration(),
            Some(CommandReturn::NotSupported)
        );
        for _ in 0..6 {
            base.receive(0).unwrap();
        }
        assert_eq!(base.receive(0).unwrap_err().kind(), &ErrorKind::Timeout);
        base.close().unwrap();
        drop(hooks);
        image.close().unwrap();
        session.close().unwrap();
        scratch.unloaded();
        println!("LOSQ_CHILD_COMPLETE");
        return;
    }
    if case == "independence_nuc_first" {
        let mut nuc = image.open_nuc(4).unwrap();
        assert_eq!(hooks.registrations(), 0);
        assert_eq!(
            image
                .open_line_of_sight_quaternion(4)
                .unwrap_err()
                .registration(),
            Some(CommandReturn::NotSupported)
        );
        for _ in 0..3 {
            nuc.receive(0).unwrap();
        }
        assert_eq!(nuc.receive(0).unwrap_err().kind(), &ErrorKind::Timeout);
        nuc.close().unwrap();
        drop(hooks);
        image.close().unwrap();
        session.close().unwrap();
        scratch.unloaded();
        println!("LOSQ_CHILD_COMPLETE");
        return;
    }
    if case == "independence_odm_first" {
        let mut odm = image.open_optical_distortion_map(8).unwrap();
        assert_eq!(hooks.registrations(), 0);
        assert_eq!(
            image
                .open_line_of_sight_quaternion(4)
                .unwrap_err()
                .registration(),
            Some(CommandReturn::NotSupported)
        );
        for _ in 0..4 {
            odm.receive(0).unwrap();
        }
        assert_eq!(odm.receive(0).unwrap_err().kind(), &ErrorKind::Timeout);
        odm.close().unwrap();
        drop(hooks);
        image.close().unwrap();
        session.close().unwrap();
        scratch.unloaded();
        println!("LOSQ_CHILD_COMPLETE");
        return;
    }
    if case == "independence_others_first" {
        let mut base = image.open_metadata(8).unwrap();
        assert_eq!(
            image.open_nuc(4).unwrap_err().registration(),
            Some(CommandReturn::NotSupported)
        );
        assert_eq!(
            image
                .open_optical_distortion_map(4)
                .unwrap_err()
                .registration(),
            Some(CommandReturn::NotSupported)
        );
        assert_eq!(hooks.registrations(), 0);
        base.close().unwrap();
    }
    if case == "stopped_ineligible" {
        image.start().unwrap();
        image.stop().unwrap();
        assert_eq!(
            image
                .open_line_of_sight_quaternion(4)
                .unwrap_err()
                .error()
                .kind(),
            &ErrorKind::InvalidArgument
        );
        assert_eq!(hooks.registrations(), 0);
        drop(hooks);
        image.close().unwrap();
        session.close().unwrap();
        scratch.unloaded();
        println!("LOSQ_CHILD_COMPLETE");
        return;
    }
    if case == "running" {
        image.start().unwrap();
    }
    let mut stream = image
        .open_line_of_sight_quaternion(match case.as_str() {
            "capacity" => 1,
            "fifo" => 3,
            _ => 16,
        })
        .unwrap();
    assert!(stream.is_open());
    assert_eq!(hooks.registrations(), 1);
    assert_eq!(
        image
            .open_line_of_sight_quaternion(4)
            .unwrap_err()
            .error()
            .kind(),
        &ErrorKind::InvalidArgument
    );
    assert_eq!(hooks.registrations(), 1);
    let mut copied = vec![];

    match case.as_str() {
        "fidelity" => {
            counters(&mut stream, 4, 0, 0);
            for fixture in 4..=7 {
                hooks.deliver(fixture, 100 + fixture, 0);
            }
            for fixture in 0..=7 {
                let tag = if fixture < 4 { 0 } else { 100 + fixture };
                let value = stream.receive(0).unwrap();
                compare(&value, fixture, tag);
                copied.push((value, fixture, tag));
            }
            counters(&mut stream, 8, 0, 0);
            expect(&mut stream, ErrorKind::Timeout);
        }
        "independence_losq_first" => {
            let mut base = image.open_metadata(8).unwrap();
            assert_eq!(hooks.registrations(), 1);
            assert_eq!(
                image
                    .open_optical_distortion_map(4)
                    .unwrap_err()
                    .registration(),
                Some(CommandReturn::NotSupported)
            );
            assert_eq!(base.receive(0).unwrap_err().kind(), &ErrorKind::Timeout);
            assert_eq!(
                image.open_nuc(4).unwrap_err().registration(),
                Some(CommandReturn::NotSupported)
            );
            assert_eq!(hooks.registrations(), 1);
            counters(&mut stream, 4, 0, 0);
            for f in 0..4 {
                compare(&stream.receive(0).unwrap(), f, 0);
            }
            base.close().unwrap();
        }
        "independence_others_first" => {
            counters(&mut stream, 4, 0, 0);
            for f in 0..4 {
                compare(&stream.receive(0).unwrap(), f, 0);
            }
        }
        "fifo" => {
            for tag in 1..=4 {
                hooks.deliver(7, tag, 0);
            }
            counters(&mut stream, 4, 1, 0);
            compare(&stream.receive(0).unwrap(), 7, 1);
            hooks.deliver(7, u32::MAX, 0);
            for tag in [2, 3, u32::MAX] {
                compare(&stream.receive(0).unwrap(), 7, tag);
            }
            counters(&mut stream, 5, 1, 0);
            expect(&mut stream, ErrorKind::Timeout);
        }
        "blocking_receive" => {
            assert_eq!(probe(8, 0), 1);
            let deliver = std::thread::spawn(|| {
                let hooks = Hooks::open();
                let waiting = probe(7, 0);
                hooks.deliver(7, 83, 0);
                assert_eq!(waiting, 1);
            });
            // No narrowing to signed milliseconds, polling loop or cancellation.
            compare(&stream.receive(u32::MAX).unwrap(), 7, 83);
            deliver.join().unwrap();
        }
        "capacity" => {
            hooks.deliver(7, 1, 0);
            counters(&mut stream, 1, 0, 0);
            let copies = probe(4, 0);
            assert_eq!(probe(5, 1), 1);
            hooks.deliver(1, 0, 0);
            counters(&mut stream, 2, 1, 0);
            assert_eq!(probe(4, 0), copies);
            assert_eq!(probe(6, 0), 1);
            hooks.deliver(0, 0, 1);
            counters(&mut stream, 3, 1, 1);
            compare(&stream.receive(0).unwrap(), 7, 1);
            // Full queue did not allocate; failure is still armed and now fires.
            hooks.deliver(1, 0, 0);
            counters(&mut stream, 4, 1, 2);
            assert_eq!(probe(6, 0), 0);
            expect(&mut stream, ErrorKind::Timeout);
            hooks.deliver(7, 91, 0);
            compare(&stream.receive(0).unwrap(), 7, 91);
            counters(&mut stream, 5, 1, 2);
        }
        "running" | "stop_empty" => {
            expect(&mut stream, ErrorKind::Timeout);
            assert_eq!(stream.receive(1).unwrap_err().kind(), &ErrorKind::Timeout);
            if case != "running" {
                image.start().unwrap();
            }
            image.stop().unwrap();
            expect(&mut stream, ErrorKind::StreamStopped);
        }
        "stop_drain" => {
            image.start().unwrap();
            image.stop().unwrap();
            for f in 0..4 {
                compare(&stream.receive(0).unwrap(), f, 0);
            }
            expect(&mut stream, ErrorKind::StreamStopped);
        }
        "parent_first" | "session_first" => {
            drop(hooks);
            if case == "session_first" {
                session.close().unwrap();
                image.close().unwrap();
            } else {
                image.close().unwrap();
                session.close().unwrap();
            }
            scratch.unloaded();
            for f in 0..4 {
                compare(&stream.receive(0).unwrap(), f, 0);
            }
            expect(&mut stream, ErrorKind::StreamStopped);
            stream.close().unwrap();
            println!("LOSQ_CHILD_COMPLETE");
            return;
        }
        "owner_close" | "owner_drop" | "late_closed" => {
            if case == "owner_drop" {
                drop(stream);
            } else {
                stream.close().unwrap();
                stream.close().unwrap();
                assert!(!stream.is_open());
                expect(&mut stream, ErrorKind::ProviderFailed);
                assert_eq!(
                    stream.counters().unwrap_err().kind(),
                    &ErrorKind::ProviderFailed
                );
                drop(stream);
            }
            assert_eq!(
                image
                    .open_line_of_sight_quaternion(4)
                    .unwrap_err()
                    .error()
                    .kind(),
                &ErrorKind::InvalidArgument
            );
            hooks.deliver(1, 0, 0); // callback after Close/Drop remains safe
            image.start().unwrap();
            image.receive(2000).unwrap();
            image.stop().unwrap();
            drop(hooks);
            image.close().unwrap();
            session.close().unwrap();
            scratch.unloaded();
            if case == "late_closed" {
                assert!(scratch.events().contains("late_losq_callback_returned"));
            }
            println!("LOSQ_CHILD_COMPLETE");
            return;
        }
        "late" => {
            drop(hooks);
            image.close().unwrap();
            session.close().unwrap();
            scratch.unloaded();
            assert!(scratch.events().contains("late_losq_callback_returned"));
            counters(&mut stream, 5, 0, 0);
            for f in 0..4 {
                compare(&stream.receive(0).unwrap(), f, 0);
            }
            expect(&mut stream, ErrorKind::StreamStopped);
            stream.close().unwrap();
            println!("LOSQ_CHILD_COMPLETE");
            return;
        }
        "detach_fail" => {
            drop(hooks);
            assert_eq!(image.stop().unwrap_err().kind(), &ErrorKind::ProviderFailed);
            compare(&stream.receive(0).unwrap(), 1, 0);
            compare(&stream.receive(0).unwrap(), 3, 0);
            expect(&mut stream, ErrorKind::ProviderFailed);
            assert_eq!(
                image.close().unwrap_err().kind(),
                &ErrorKind::ProviderFailed
            );
            stream.close().unwrap();
            session.close().unwrap();
            scratch.unloaded();
            println!("LOSQ_CHILD_COMPLETE");
            return;
        }
        "in_flight" => {
            struct Release;
            impl Drop for Release {
                fn drop(&mut self) {
                    probe(2, 0);
                }
            }
            let release = Release;
            assert_eq!(probe(1, 0), 1);
            stream.close().unwrap(); // must not wait for the parked callback
            drop(hooks);
            // Safe owners never cross threads. The helper only observes native
            // teardown and releases a test barrier; parent Close stays here.
            let observer = std::thread::spawn(|| {
                let waiting = probe(3, 0);
                let released = probe(2, 0);
                assert_eq!((waiting, released), (1, 1));
            });
            image.close().unwrap();
            observer.join().unwrap();
            drop(release);
            session.close().unwrap();
            scratch.unloaded();
            let events = scratch.events();
            let at = |s| events.lines().position(|v| v == s).unwrap();
            assert!(at("losq_in_flight_callback_entered") < at("channel_destroyed"));
            assert!(at("channel_destroyed") < at("losq_in_flight_callback_returned"));
            assert!(at("losq_in_flight_callback_returned") < at("library_unloaded"));
            println!("LOSQ_CHILD_COMPLETE");
            return;
        }
        other => panic!("unknown child {other}"),
    }
    stream.close().unwrap();
    drop(stream);
    drop(hooks);
    image.close().unwrap();
    session.close().unwrap();
    scratch.unloaded();
    for (value, fixture, tag) in copied {
        compare(&value, fixture, tag);
    }
    println!("LOSQ_CHILD_COMPLETE");
}
