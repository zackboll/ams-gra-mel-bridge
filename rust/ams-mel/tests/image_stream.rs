use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Mutex;
use std::thread;
use std::time::Duration;

use ams_mel::{ComponentLocation, ErrorKind, ImageConfig, ImageFlip, ImageType, Session, UciId};

static NEXT_LOG: AtomicU64 = AtomicU64::new(0);
static STREAM_TEST: Mutex<()> = Mutex::new(());

#[test]
fn receives_owned_mono8_metadata_and_pixels() {
    let _guard = STREAM_TEST.lock().expect("stream test lock");
    let session = open("success");
    let mut stream = session.open_image_stream(&config(4)).expect("open stream");
    stream.start().expect("start");

    let first = stream.receive(1_000).expect("first frame");
    assert_eq!(first.system_time_ns, 1_000_001);
    assert_eq!(first.integration_time_ns, 20_001);
    assert_eq!((first.width, first.height), (4, 3));
    assert_eq!((first.bits_per_pixel, first.number_of_bands), (8, 1));
    assert_eq!(
        (first.horizontal_fov_rad, first.vertical_fov_rad),
        (0.25, 0.125)
    );
    assert_eq!(
        (first.frame_id, first.subframe_id, first.subframe_total),
        (1, 2, 4)
    );
    assert_eq!(first.image_type, ImageType::Staring);
    assert_eq!(first.image_flip, ImageFlip::Horizontal);
    assert_eq!(first.image_flags, 1 << 2);
    assert_eq!((first.dither_row, first.dither_column), (0.5, -0.25));
    assert_eq!(
        (first.row_offset, first.column_offset, first.band_index),
        (7, 9, 3)
    );
    assert_eq!(first.pixels, (16_u8..28).collect::<Vec<_>>());

    let second = stream.receive(1_000).expect("second frame");
    assert_eq!(second.frame_id, 2);
    assert_eq!(second.pixels, (32_u8..44).collect::<Vec<_>>());
    stream.close().expect("close stream");
    session.close().expect("close session");
}

#[test]
fn timeout_is_distinct_and_clean_stop_drains_frames() {
    let _guard = STREAM_TEST.lock().expect("stream test lock");
    let idle_session = open("idle");
    let mut idle = idle_session
        .open_image_stream(&config(4))
        .expect("open idle");
    idle.start().expect("start idle");
    assert_eq!(
        idle.receive(2).expect_err("timeout").kind(),
        &ErrorKind::Timeout
    );
    idle.stop().expect("stop idle");
    assert_eq!(
        idle.start()
            .expect_err("cannot restart stopped stream")
            .kind(),
        &ErrorKind::StreamStopped
    );
    assert_eq!(
        idle.receive(0).expect_err("stopped").kind(),
        &ErrorKind::StreamStopped
    );
    idle.close().expect("close idle");
    idle_session.close().expect("close idle session");

    let session = open("success");
    let mut stream = session.open_image_stream(&config(4)).expect("open stream");
    stream.start().expect("start");
    thread::sleep(Duration::from_millis(20));
    stream.stop().expect("stop");
    let mut drained = 0;
    loop {
        match stream.receive(0) {
            Ok(_) => drained += 1,
            Err(error) if error.kind() == &ErrorKind::StreamStopped => break,
            Err(error) => panic!("unexpected drain error: {error}"),
        }
    }
    assert!(drained > 0);
}

#[test]
fn reports_overflow_counters_and_drains_bounded_queue() {
    let _guard = STREAM_TEST.lock().expect("stream test lock");
    let session = open("overflow");
    let mut stream = session.open_image_stream(&config(2)).expect("open stream");
    stream.start().expect("start");
    thread::sleep(Duration::from_millis(50));
    assert_eq!(
        stream.counters().expect("counters"),
        ams_mel::StreamCounters {
            frames_received: 20,
            frames_dropped_queue_full: 18,
            malformed_or_unsupported: 0,
        }
    );
    stream.stop().expect("stop");
    assert!(stream.receive(0).is_ok());
    assert!(stream.receive(0).is_ok());
    assert_eq!(
        stream.receive(0).expect_err("stopped").kind(),
        &ErrorKind::StreamStopped
    );
}

#[test]
fn stream_outlives_parent_session_and_controls_provider_unload() {
    let _guard = STREAM_TEST.lock().expect("stream test lock");
    let log = LifetimeLog::new();
    let session = open("success");
    let mut stream = session.open_image_stream(&config(4)).expect("open stream");
    stream.start().expect("start");
    session.close().expect("parent closes first");
    assert!(!log.contents().contains("library_unloaded"));
    assert_eq!(
        stream
            .receive(1_000)
            .expect("frame after parent close")
            .frame_id,
        1
    );
    stream.close().expect("stream close");
    let events = log.contents();
    assert!(events.contains("control_destroyed\nmanager_destroyed\nlibrary_unloaded\n"));
}

#[test]
fn start_failure_preserves_provider_failure_and_poison() {
    let _guard = STREAM_TEST.lock().expect("stream test lock");
    let session = open("enable-fail");
    let mut stream = session.open_image_stream(&config(4)).expect("open stream");
    assert_eq!(
        stream.start().expect_err("start fails").kind(),
        &ErrorKind::ProviderFailed
    );
    assert_eq!(
        stream.start().expect_err("poison remains").kind(),
        &ErrorKind::ProviderFailed
    );
    assert_eq!(
        stream.stop().expect_err("poisoned stop").kind(),
        &ErrorKind::ProviderFailed
    );
    assert_eq!(
        stream.close().expect_err("close reports poison").kind(),
        &ErrorKind::ProviderFailed
    );
    session.close().expect("close session");
}

#[test]
fn malformed_frame_is_not_returned_and_is_counted() {
    let _guard = STREAM_TEST.lock().expect("stream test lock");
    let session = open("unsupported-bpp");
    let mut stream = session.open_image_stream(&config(4)).expect("open stream");
    stream.start().expect("start");
    assert_eq!(
        stream.receive(20).expect_err("no unsafe frame").kind(),
        &ErrorKind::Timeout
    );
    let counters = stream.counters().expect("counters");
    assert!(counters.frames_received >= 1);
    assert!(counters.malformed_or_unsupported >= 1);
}

#[test]
fn drop_of_running_stream_releases_provider() {
    let _guard = STREAM_TEST.lock().expect("stream test lock");
    let log = LifetimeLog::new();
    let session = open("success");
    let mut stream = session.open_image_stream(&config(4)).expect("open stream");
    stream.start().expect("start");
    session.close().expect("close parent");
    drop(stream);
    assert!(log.contents().ends_with("library_unloaded\n"));
}

#[test]
fn rejects_invalid_configuration_before_provider_activity() {
    let _guard = STREAM_TEST.lock().expect("stream test lock");
    for limits in [(0, 64, 4), (3, 0, 4), (3, 64, 0)] {
        let error = ImageConfig::with_limits(
            id(0, "channel"),
            id(16, "platform"),
            location(),
            limits.0,
            limits.1,
            limits.2,
        )
        .expect_err("zero configuration rejected");
        assert_eq!(error.kind(), &ErrorKind::InvalidArgument);
    }
    for error in [
        UciId::new([0; 16], "bad\0label").expect_err("label NUL"),
        ComponentLocation::new(0.0, 0.0, 0.0, "bad\0key", "system").expect_err("key NUL"),
        ComponentLocation::new(0.0, 0.0, 0.0, "key", "bad\0system").expect_err("system NUL"),
    ] {
        assert_eq!(error.kind(), &ErrorKind::InvalidArgument);
    }
}

fn open(scenario: &str) -> Session {
    Session::open(mock_provider(), scenario, "").expect("open session")
}

fn config(queue_capacity: usize) -> ImageConfig {
    ImageConfig::with_limits(
        id(0, "IR image channel"),
        id(0xf0, "test platform"),
        location(),
        3,
        64,
        queue_capacity,
    )
    .expect("valid image config")
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
    provider_path("libmock_ir_provider.so")
}

fn provider_path(name: &str) -> PathBuf {
    let repository = Path::new(env!("CARGO_MANIFEST_DIR")).join("../..");
    env::var_os("AMS_MEL_TEST_PROVIDER_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| repository.join("native/build/test-providers"))
        .join(name)
}

struct LifetimeLog {
    directory: PathBuf,
    path: PathBuf,
}

impl LifetimeLog {
    fn new() -> Self {
        let sequence = NEXT_LOG.fetch_add(1, Ordering::Relaxed);
        let directory = env::temp_dir().join(format!(
            "ams-mel-rust-stream-{}-{sequence}",
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
}

impl Drop for LifetimeLog {
    fn drop(&mut self) {
        env::remove_var("AMS_MEL_TEST_LIFETIME_LOG");
        let _ = fs::remove_file(&self.path);
        let _ = fs::remove_dir(&self.directory);
    }
}
