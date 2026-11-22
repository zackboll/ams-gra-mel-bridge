//! Actual onImage delivery from the compiled provider. Only mock controls use
//! FFI here; every checkout and lifecycle operation uses the public binding.
use ams_mel_ir::{
    ChannelView, ComponentLocation, ErrorKind, HostBuffer, ImageCheckoutCounters,
    ImageCheckoutReceiver, ImageConfig, ImageControlConfig, ImageStream, Session, UciId,
};
use ams_mel_sys as sys;
use std::{
    cell::Cell,
    env,
    ffi::{c_char, c_void, CString},
    fs,
    path::PathBuf,
    process::Command,
};

const CHILD: &str = "AMS_MEL_RUST021_CHILD";
const LOG: &str = "AMS_MEL_TEST_LIFETIME_LOG";

fn provider() -> PathBuf {
    env::var_os("AMS_MEL_TEST_PROVIDER_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| {
            PathBuf::from(env!("CARGO_MANIFEST_DIR"))
                .join("../../native/build-tests/test-providers")
        })
        .join("libmock_ir_provider.so")
}

fn ids() -> (UciId, UciId, ComponentLocation) {
    (
        UciId::new([1; 16], "image").unwrap(),
        UciId::new([2; 16], "platform").unwrap(),
        ComponentLocation::new(0.0, 0.0, 0.0, "sensor", "platform").unwrap(),
    )
}

fn open(case: &str) -> (Session, ImageStream) {
    let parent = Session::open(provider(), format!("checkout021-{case}"), "").unwrap();
    let (id, platform, location) = ids();
    let image = parent
        .open_image_control(&ImageControlConfig::new(id, platform, location))
        .unwrap();
    (parent, image)
}

fn registered(parent: &Session, image: &ImageStream) -> (ChannelView, HostBuffer) {
    let mut channel = image.channel_view().unwrap();
    let mut buffer = parent.open_host_buffer(16, -20).unwrap();
    buffer.write_storage(0, &[0, 127, 128, 255]).unwrap();
    channel.register_buffer(&mut buffer).unwrap();
    (channel, buffer)
}

fn blocked(channel: &mut ChannelView, buffer: &mut HostBuffer) {
    let error = channel.unregister_buffer(buffer).unwrap_err();
    assert_eq!(error.error.kind(), &ErrorKind::ProviderFailed);
    assert_eq!(error.native_status, Some(sys::AMS_MEL_PROVIDER_FAILED));
    assert_eq!(error.provider_return, None);
    assert_eq!(count("application_unregister_entered"), 0);
}

fn snapshot(
    receiver: &ImageCheckoutReceiver,
    [seen, accepted, overflow, rejected, released, uncertain]: [u64; 6],
) {
    assert_eq!(
        receiver.counters().unwrap(),
        ImageCheckoutCounters {
            callbacks_received: seen,
            checkouts_accepted: accepted,
            queue_overflow: overflow,
            callbacks_rejected: rejected,
            releases_succeeded: released,
            releases_uncertain: uncertain,
        }
    );
}

fn events() -> String {
    fs::read_to_string(env::var_os(LOG).unwrap()).unwrap()
}
fn count(name: &str) -> usize {
    events().lines().filter(|line| *line == name).count()
}

fn ordered(expected: &[&str]) {
    let log = events();
    let mut lines = log.lines();
    for event in expected {
        assert!(
            lines.any(|line| line == *event),
            "missing ordered event {event}:\n{log}"
        );
    }
}

fn run(case: &str) {
    let log = env::temp_dir().join(format!("rust021-{}-{case}.log", std::process::id()));
    fs::write(&log, "").unwrap();
    let output = Command::new(env::current_exe().unwrap())
        .args(["--exact", "mock_child", "--nocapture"])
        .env(CHILD, case)
        .env(LOG, &log)
        .output()
        .unwrap();
    assert!(
        output.status.success(),
        "{case}: {}\n{}\n{}",
        String::from_utf8_lossy(&output.stdout),
        String::from_utf8_lossy(&output.stderr),
        fs::read_to_string(&log).unwrap()
    );
    fs::remove_file(log).unwrap();
}

#[test]
fn installation_and_unsupported_acquisition() {
    run("installation");
}

#[test]
fn bounded_fifo_polls_timeouts_and_all_six_counters() {
    run("enable-callback");
}

#[test]
fn independent_owners_and_provider_code_residency() {
    for case in [
        "receiver-first",
        "image-first",
        "session-first",
        "receiver-last",
    ] {
        run(case);
    }
}

#[test]
fn uncertain_releases_consume_the_public_owner_without_destruction_or_retry() {
    for case in [
        "fail",
        "throw",
        "not-supported",
        "drop-failure",
        "drain-provider-fail",
    ] {
        run(case);
    }
}

#[test]
fn genuine_synchronous_release_reentry_and_deferred_rejection() {
    run("reentry");
}

#[test]
fn mock_child() {
    let Ok(case) = env::var(CHILD) else { return };
    match case.as_str() {
        "installation" => installation(),
        "enable-callback" => fifo(),
        "receiver-last" => receiver_last(),
        "receiver-first" | "image-first" | "session-first" => closure_order(&case),
        "fail" | "throw" | "not-supported" | "drop-failure" | "drain-provider-fail" => {
            uncertain(&case)
        }
        "reentry" => reentry(),
        _ => panic!("unknown mock case {case}"),
    }
    assert_eq!(count("foundation_release_forbidden"), 0);
    assert_eq!(count("frame_produced"), 0);
}

fn installation() {
    let (parent, mut image) = open("installation");
    for (queue, limit) in [(0, 0), (0, 1), (1, 0), (2, 1), (1, usize::MAX)] {
        assert_eq!(
            image
                .open_checkout_receiver(queue, limit)
                .unwrap_err()
                .kind(),
            &ErrorKind::InvalidArgument
        );
    }
    let mut receiver = image.open_checkout_receiver(1, 1).unwrap();
    assert_eq!(
        image.open_checkout_receiver(1, 1).unwrap_err().kind(),
        &ErrorKind::ProviderFailed
    );
    receiver.close().unwrap();
    assert_eq!(
        image.open_checkout_receiver(1, 1).unwrap_err().kind(),
        &ErrorKind::ProviderFailed
    );
    image.try_close().unwrap();
    assert_eq!(
        image.open_checkout_receiver(1, 1).unwrap_err().kind(),
        &ErrorKind::ProviderFailed
    );
    parent.close().unwrap();

    let (parent, mut image) = open("after-start");
    image.start().unwrap();
    assert_eq!(
        image.open_checkout_receiver(1, 1).unwrap_err().kind(),
        &ErrorKind::ProviderFailed
    );
    let mut channel = image.channel_view().unwrap();
    channel.disable().unwrap();
    assert_eq!(
        image.open_checkout_receiver(1, 1).unwrap_err().kind(),
        &ErrorKind::ProviderFailed
    );
    image.close().unwrap();
    parent.close().unwrap();

    let parent = Session::open(provider(), "valid", "").unwrap();
    let (id, platform, location) = ids();
    let mut acquisition = parent
        .open_image_stream(&ImageConfig::new(id, platform, location))
        .unwrap();
    assert_eq!(
        acquisition.open_checkout_receiver(1, 1).unwrap_err().kind(),
        &ErrorKind::ProviderFailed
    );
    acquisition.close().unwrap();
    parent.close().unwrap();
}

fn fifo() {
    let (parent, mut image) = open("enable-callback");
    let mock = Mock::open();
    let mut receiver = image.open_checkout_receiver(2, 3).unwrap();
    snapshot(&receiver, [0; 6]);
    for timeout in [0, 15] {
        assert_eq!(
            receiver.receive(timeout).unwrap_err().kind(),
            &ErrorKind::Timeout
        );
        assert!(receiver.is_open());
        snapshot(&receiver, [0; 6]);
    }
    mock.deliver(1, false); // id 1: attached and unregistered, rejected
    let (mut channel, mut buffer) = registered(&parent, &image);
    image.start().unwrap(); // id 2: synchronous enable callback, rejected while Starting
    snapshot(&receiver, [2, 0, 0, 2, 2, 0]);
    assert_eq!(receiver.receive(0).unwrap_err().kind(), &ErrorKind::Timeout);
    mock.deliver(3, false); // ids 3 and 4 accepted, id 5 overflows FIFO
    mock.deliver(1, true); // null on a full queue is rejected, not overflow
    snapshot(&receiver, [6, 2, 1, 4, 3, 0]);
    let mut first = receiver.receive(0).unwrap();
    let second = receiver.receive(20).unwrap();
    mock.deliver(2, false); // id 6 accepted; id 7 exceeds HOLD limit, FIFO not full
    snapshot(&receiver, [8, 3, 2, 5, 4, 0]);
    assert_eq!(count("checkout_released_3"), 0);
    assert_eq!(count("checkout_released_4"), 0);
    assert_eq!(count("checkout_released_6"), 0);
    channel.disable().unwrap();
    blocked(&mut channel, &mut buffer);
    first.close().unwrap();
    assert!(!first.is_open());
    first.close().unwrap();
    assert_eq!(count("checkout_released_3"), 1);
    assert_eq!(count("checkout_released_4"), 0);
    drop(second); // implicit release of the second FIFO item
    snapshot(&receiver, [8, 3, 2, 5, 6, 0]);
    blocked(&mut channel, &mut buffer); // still has a queued obligation
    receiver.close().unwrap(); // drains id 6, not Disable or unregister
    receiver.close().unwrap();
    assert!(!receiver.is_open());
    assert_eq!(
        receiver.receive(0).unwrap_err().kind(),
        &ErrorKind::StreamStopped
    );
    assert_eq!(
        receiver.counters().unwrap_err().kind(),
        &ErrorKind::StreamStopped
    );
    mock.deliver(1, false); // id 8 delivered after Close; never accepted
    drop(mock);
    channel.unregister_buffer(&mut buffer).unwrap();
    assert_eq!(buffer.read_storage(0, 4).unwrap(), [0, 127, 128, 255]);
    buffer.close().unwrap();
    image.close().unwrap();
    parent.close().unwrap();
    ordered(&[
        "checkout_released_3",
        "checkout_destroyed_3",
        "checkout_released_4",
        "checkout_destroyed_4",
        "checkout_released_6",
        "checkout_destroyed_6",
        "checkout_destroyed_8",
        "application_unregister_entered",
        "foundation_buffer_destroyed_with_storage",
        "library_unloaded",
    ]);
    assert_eq!(count("application_checkout_delivered"), 9);
    assert_eq!(count("application_checkout_released"), 8);
    assert_eq!(count("application_checkout_destroyed"), 8);
}

fn closure_order(case: &str) {
    let (parent, mut image) = open(case);
    let mut parent = Some(parent);
    let mut receiver = image.open_checkout_receiver(2, 3).unwrap();
    let (mut channel, mut buffer) = registered(parent.as_ref().unwrap(), &image);
    image.start().unwrap();
    let mock = Mock::open();
    mock.deliver(1, false);
    if case == "receiver-first" {
        mock.deliver(1, false);
    }
    drop(mock); // no test pin masks native DSO residency for the rest of the test
    let mut lease = receiver.receive(0).unwrap();
    match case {
        "receiver-first" => {
            receiver.close().unwrap();
            ordered(&["checkout_released_2", "checkout_destroyed_2"]);
            assert_eq!(count("checkout_released_1"), 0);
            blocked(&mut channel, &mut buffer); // receiver Close did not disable
            assert_eq!(count("channel_disabled"), 0);
        }
        "image-first" => {
            assert_eq!(
                image.try_close().unwrap_err().kind(),
                &ErrorKind::ProviderFailed
            );
            snapshot(&receiver, [1, 1, 0, 0, 0, 0]);
        }
        "session-first" => {
            parent.take().unwrap().close().unwrap();
            snapshot(&receiver, [1, 1, 0, 0, 0, 0]);
        }
        _ => unreachable!(),
    }
    assert!(lease.is_open());
    assert_eq!(count("library_unloaded"), 0);
    channel.disable().unwrap();
    blocked(&mut channel, &mut buffer);
    assert_eq!(count("checkout_released_1"), 0);
    lease.close().unwrap();
    lease.close().unwrap();
    drop(lease);
    assert_eq!(count("checkout_released_1"), 1);
    assert_eq!(count("checkout_destroyed_1"), 1);
    channel.unregister_buffer(&mut buffer).unwrap();
    buffer.close().unwrap();
    image.try_close().unwrap();
    receiver.close().unwrap();
    if let Some(parent) = parent {
        parent.close().unwrap();
    }
    ordered(&[
        "checkout_destroyed_1",
        "application_unregister_entered",
        "foundation_buffer_destroyed_with_storage",
        "library_unloaded",
    ]);
    channel.close().unwrap(); // weak view never pins the DSO
}

fn receiver_last() {
    let (parent, mut image) = open("receiver-last");
    let mut receiver = image.open_checkout_receiver(1, 1).unwrap();
    image.close().unwrap();
    parent.close().unwrap();
    assert!(receiver.is_open());
    assert_eq!(count("library_unloaded"), 0);
    assert_eq!(
        receiver.receive(20).unwrap_err().kind(),
        &ErrorKind::StreamStopped
    );
    snapshot(&receiver, [0; 6]);
    drop(receiver);
    assert_eq!(count("library_unloaded"), 1);
}

fn uncertain(case: &str) {
    let (parent, mut image) = open(if case == "drop-failure" { "fail" } else { case });
    let mut receiver = image.open_checkout_receiver(2, 3).unwrap();
    let (mut channel, mut buffer) = registered(&parent, &image);
    image.start().unwrap();
    let mock = Mock::open();
    mock.deliver(1, false);
    let mut lease = receiver.receive(0).unwrap();
    if case == "drain-provider-fail" {
        mock.deliver(2, false);
    }
    drop(mock);
    channel.disable().unwrap();
    blocked(&mut channel, &mut buffer);
    if case == "drain-provider-fail" {
        assert_eq!(
            receiver.close().unwrap_err().kind(),
            &ErrorKind::ProviderFailed
        );
        assert!(!receiver.is_open());
        receiver.close().unwrap();
        assert_eq!(count("application_checkout_released"), 2);
        assert_eq!(count("checkout_released_1"), 0);
        assert!(lease.is_open());
    }
    if case == "drop-failure" {
        assert!(std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| drop(lease))).is_ok());
    } else {
        assert_eq!(count("checkout_released_1"), 0);
        let error = lease.close().unwrap_err();
        let (kind, status, returned) = match case {
            "throw" => (
                ErrorKind::ProviderException,
                sys::AMS_MEL_PROVIDER_EXCEPTION,
                None,
            ),
            "not-supported" => (
                ErrorKind::ProviderFailed,
                sys::AMS_MEL_PROVIDER_FAILED,
                Some(sys::AMS_MEL_IR_RETURN_NOT_SUPPORTED),
            ),
            _ => (
                ErrorKind::ProviderFailed,
                sys::AMS_MEL_PROVIDER_FAILED,
                Some(sys::AMS_MEL_IR_RETURN_FAIL),
            ),
        };
        assert_eq!(error.error.kind(), &kind);
        assert!(error
            .error
            .to_string()
            .contains("Image checkout operation failed"));
        assert_eq!(error.native_status, Some(status));
        assert_eq!(error.provider_return, returned);
        assert!(!lease.is_open());
        assert_eq!(count("checkout_released_1"), 1);
        lease.close().unwrap();
        drop(lease);
    }
    if receiver.is_open() {
        snapshot(&receiver, [1, 1, 0, 0, 0, 1]);
    }
    blocked(&mut channel, &mut buffer);
    assert_eq!(
        buffer.close().unwrap_err().kind(),
        &ErrorKind::ProviderFailed
    );
    assert_eq!(
        image.try_close().unwrap_err().kind(),
        &ErrorKind::ProviderFailed
    );
    receiver.close().unwrap();
    parent.close().unwrap();
    drop((channel, image, buffer, receiver));
    assert_eq!(count("checkout_released_1"), 1);
    assert_eq!(
        count("application_checkout_released"),
        if case == "drain-provider-fail" { 3 } else { 1 }
    );
    for forbidden in [
        "application_unregister_entered",
        "application_checkout_destroyed",
        "foundation_buffer_destroyed_with_storage",
        "library_unloaded",
    ] {
        assert_eq!(count(forbidden), 0);
    }
}

type Deliver = unsafe extern "C" fn(u32, i32);
thread_local! { static REENTER_DELIVERY: Cell<Option<Deliver>> = const { Cell::new(None) }; }

extern "C" fn deliver_during_release() {
    if let Some(deliver) = REENTER_DELIVERY.take() {
        // SAFETY: the current mock pin and attached Image live through this
        // synchronous hook. No Rust owner or borrowed handle is reentered.
        unsafe { deliver(2, 0) };
    }
}

fn reentry() {
    let (parent, mut image) = open("reentry");
    let mut receiver = image.open_checkout_receiver(1, 1).unwrap();
    let (mut channel, mut buffer) = registered(&parent, &image);
    image.start().unwrap();
    let mock = Mock::open();
    mock.deliver(1, false);
    let mut first = receiver.receive(0).unwrap();
    mock.install_release_delivery();
    first.close().unwrap();
    assert!(REENTER_DELIVERY.take().is_none());
    // The first reentered callback takes the freed HOLD slot (id 2). The
    // second is rejected (id 3); its release runs outside the callback after
    // the outer release has destroyed id 1. This order rules out inline release.
    snapshot(&receiver, [3, 2, 1, 1, 2, 0]);
    ordered(&[
        "checkout_released_1",
        "application_checkout_delivered",
        "application_checkout_returned",
        "application_checkout_delivered",
        "application_checkout_returned",
        "checkout_destroyed_1",
        "checkout_released_3",
        "checkout_destroyed_3",
    ]);
    assert_eq!(count("checkout_released_2"), 0);
    let mut second = receiver.receive(0).unwrap();
    channel.disable().unwrap();
    blocked(&mut channel, &mut buffer);
    second.close().unwrap();
    snapshot(&receiver, [3, 2, 1, 1, 3, 0]);
    receiver.close().unwrap();
    drop(mock);
    channel.unregister_buffer(&mut buffer).unwrap();
    buffer.close().unwrap();
    image.close().unwrap();
    parent.close().unwrap();
    ordered(&[
        "checkout_destroyed_3",
        "checkout_released_2",
        "checkout_destroyed_2",
        "library_unloaded",
    ]);
}

unsafe extern "C" {
    fn dlopen(path: *const c_char, flags: i32) -> *mut c_void;
    fn dlsym(handle: *mut c_void, name: *const c_char) -> *mut c_void;
    fn dlclose(handle: *mut c_void) -> i32;
}

struct Mock(*mut c_void);

impl Mock {
    fn open() -> Self {
        let path = CString::new(provider().as_os_str().as_encoded_bytes()).unwrap();
        // SAFETY: NUL-terminated path; the test owns this RTLD_NOW pin.
        let pin = unsafe { dlopen(path.as_ptr(), 2) };
        assert!(!pin.is_null());
        Self(pin)
    }

    fn delivery(&self) -> Deliver {
        // SAFETY: the compiled mock exports this exact signature. Self pins it.
        let symbol = unsafe { dlsym(self.0, c"mock_checkout_deliver".as_ptr()) };
        assert!(!symbol.is_null());
        unsafe { std::mem::transmute::<*mut c_void, Deliver>(symbol) }
    }

    fn deliver(&self, count: u32, null: bool) {
        // SAFETY: the test invokes this only on a live checkout021 attachment,
        // with delivery and lifecycle operations externally serialized.
        unsafe { self.delivery()(count, i32::from(null)) };
    }

    fn install_release_delivery(&self) {
        type Install = unsafe extern "C" fn(extern "C" fn());
        // SAFETY: this exact mock export installs one synchronous hook. The
        // pin remains live until the test has called and checked release.
        unsafe {
            let symbol = dlsym(self.0, c"mock_checkout_release_hook".as_ptr());
            assert!(!symbol.is_null());
            REENTER_DELIVERY.set(Some(self.delivery()));
            std::mem::transmute::<*mut c_void, Install>(symbol)(deliver_during_release);
        }
    }
}

impl Drop for Mock {
    fn drop(&mut self) {
        // SAFETY: this releases the one pin obtained by Mock::open.
        assert_eq!(unsafe { dlclose(self.0) }, 0);
    }
}
