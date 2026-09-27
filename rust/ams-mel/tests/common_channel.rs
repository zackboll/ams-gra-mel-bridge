//! Task 032B3: safe Rust common Channel façade for the existing safe C2 and
//! Image owners. Every test uses only the public safe API.
use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Mutex;

use ams_mel::{
    BandInfo, BandType, ChannelCapability, ChannelType, ChannelView, CommandReturn,
    CommsTestReport, CommsTestRequest, CommsTestResult, ComponentLocation, ControlChannel,
    ControlConfig, CoordinateSystem, ErrorKind, ImageBand, ImageConfig, ImageStream, MelErrorCode,
    MetadataCapability, MfaMode, ModeResult, PixelFormat, ReturnRequest, ReturnResult, SensorType,
    Session, UciId,
};

static NEXT_LOG: AtomicU64 = AtomicU64::new(0);
/// Lifetime logs are process-global environment state, so tests serialize.
static COMMON_TEST: Mutex<()> = Mutex::new(());

const HIGH: CommsTestRequest = CommsTestRequest {
    command_id: 0x8000_0001,
    channel_id: 0xf000_0002,
    request_id: 0xe000_0003,
};
const HIGH_REPORT: CommsTestResult = CommsTestResult::Completed {
    report: CommsTestReport {
        command_id: 0x8000_0001,
        request_id: 0xe000_0003,
    },
};
const KEEPALIVE_SUCCESS: ReturnResult = ReturnResult::Completed {
    value: CommandReturn::Success,
};

fn lock() -> std::sync::MutexGuard<'static, ()> {
    COMMON_TEST
        .lock()
        .unwrap_or_else(|poisoned| poisoned.into_inner())
}

/// KeepAlive (existing ReturnRequest), high-ID CommsTest, and Capabilities,
/// each with a cached repeated Wait.
fn exercise(view: &mut ChannelView) -> ChannelCapability {
    let mut keepalive: ReturnRequest = view.send_keepalive().expect("KeepAlive");
    assert_eq!(
        keepalive.wait(5_000).expect("KeepAlive wait"),
        KEEPALIVE_SUCCESS
    );
    assert_eq!(
        keepalive.wait(0).expect("cached KeepAlive"),
        KEEPALIVE_SUCCESS
    );
    keepalive.close().expect("KeepAlive close");

    let mut comms = view.submit_comms_test(HIGH).expect("CommsTest");
    assert_eq!(comms.wait(5_000).expect("CommsTest wait"), HIGH_REPORT);
    assert_eq!(comms.wait(0).expect("cached CommsTest"), HIGH_REPORT);
    comms.close().expect("CommsTest close");

    view.capabilities().expect("capabilities")
}

fn assert_expired(view: &mut ChannelView) {
    assert!(view.is_open(), "an expired weak view is still owned");
    assert_eq!(
        view.send_keepalive().expect_err("expired KeepAlive").kind(),
        &ErrorKind::ProviderFailed
    );
    assert_eq!(
        view.submit_comms_test(HIGH)
            .expect_err("expired CommsTest")
            .kind(),
        &ErrorKind::ProviderFailed
    );
    assert_eq!(
        view.capabilities()
            .expect_err("expired capabilities")
            .kind(),
        &ErrorKind::ProviderFailed
    );
}

#[test]
fn c2_view_works_attached_and_enabled_and_typed_api_is_unchanged() {
    let _guard = lock();
    let session = open("comms-high");
    let mut c2 = open_c2(&session);
    let mut view = c2.channel_view().expect("C2 view");
    assert!(view.is_open());

    // Attached: common operations work while typed Enabled-only ones do not.
    assert_eq!(
        c2.submit_operate(1)
            .expect_err("Operate before enable")
            .kind(),
        &ErrorKind::ProviderFailed
    );
    assert_eq!(
        c2.submit_bit_noop(2).expect_err("BIT before enable").kind(),
        &ErrorKind::ProviderFailed
    );
    let capability = exercise(&mut view);
    assert_eq!(
        capability.channel_types,
        vec![ChannelType::CommandAndControl]
    );

    c2.enable().expect("enable");
    let capability = exercise(&mut view);
    assert_eq!(
        capability.channel_types,
        vec![ChannelType::CommandAndControl]
    );

    let mut operate = c2.submit_operate(3).expect("Operate");
    assert_eq!(
        operate.wait(5_000).expect("Operate wait"),
        ModeResult::Success {
            mode: MfaMode::TaskSched
        }
    );
    let mut bit = c2.submit_bit_noop(4).expect("BIT");
    assert_eq!(bit.wait(5_000).expect("BIT wait"), KEEPALIVE_SUCCESS);
    operate.close().expect("Operate close");
    bit.close().expect("BIT close");

    view.close().expect("view close");
    assert!(!view.is_open());
    view.close().expect("repeated view close");
    assert!(c2.is_open(), "view close closed the typed owner");
    c2.close().expect("C2 close");
    session.close().expect("session close");
}

#[test]
fn image_view_works_attached_and_running() {
    let _guard = lock();
    let session = open("comms-high");
    let mut stream = open_stream(&session);
    let mut view = stream.channel_view().expect("Image view before Start");
    let capability = exercise(&mut view);
    assert_eq!(capability.pixel_format, PixelFormat::Mono);

    stream.start().expect("start");
    let capability = exercise(&mut view);
    assert_eq!(capability.pixel_format, PixelFormat::Mono);
    stream.counters().expect("counters remain available");

    view.close().expect("view close");
    view.close().expect("repeated view close");
    stream.close().expect("stream close");
    session.close().expect("session close");
}

#[test]
fn closed_c2_source_is_rejected_without_native_call() {
    let _guard = lock();
    let session = open("comms-high");
    let mut c2 = open_c2(&session);
    c2.close().expect("C2 close");
    let error = c2.channel_view().expect_err("closed C2");
    assert_eq!(error.kind(), &ErrorKind::ProviderFailed);
    assert_eq!(error.diagnostic(), Some("C2 channel is closed"));
    session.close().expect("session close");
}

#[test]
fn keepalive_return_fail_is_a_normal_completion() {
    let _guard = lock();
    let session = open("keepalive-fail");
    let c2 = open_c2(&session);
    let mut view = c2.channel_view().expect("view");
    let mut request = view.send_keepalive().expect("KeepAlive");
    let expected = ReturnResult::Completed {
        value: CommandReturn::Fail,
    };
    assert_eq!(request.wait(5_000).expect("Return::Fail is Ok"), expected);
    assert_eq!(request.wait(0).expect("cached"), expected);
}

#[test]
fn keepalive_rejection_preserves_complete_long_diagnostic() {
    let _guard = lock();
    let session = open("keepalive-reject");
    let c2 = open_c2(&session);
    let mut view = c2.channel_view().expect("view");
    let mut request = view.send_keepalive().expect("KeepAlive");
    let expected = ReturnResult::Rejected {
        code: MelErrorCode::InvalidState,
        description: long_description(),
    };
    assert_eq!(request.wait(5_000).expect("rejection"), expected);
    assert_eq!(request.wait(0).expect("cached rejection"), expected);
}

#[test]
fn comms_rejection_preserves_complete_long_diagnostic() {
    let _guard = lock();
    let session = open("comms-reject");
    let c2 = open_c2(&session);
    let mut view = c2.channel_view().expect("view");
    let mut request = view.submit_comms_test(HIGH).expect("CommsTest");
    let expected = CommsTestResult::Rejected {
        code: MelErrorCode::InvalidParameters,
        description: long_description(),
    };
    assert_eq!(request.wait(5_000).expect("rejection"), expected);
    assert_eq!(request.wait(0).expect("cached rejection"), expected);
}

#[test]
fn timeout_does_not_consume_and_results_are_cached() {
    let _guard = lock();
    let session = open("keepalive-delayed");
    let c2 = open_c2(&session);
    let mut view = c2.channel_view().expect("view");
    let mut keepalive = view.send_keepalive().expect("KeepAlive");
    assert_eq!(
        keepalive.wait(0).expect_err("poll timeout").kind(),
        &ErrorKind::Timeout
    );
    assert_eq!(keepalive.wait(5_000).expect("later"), KEEPALIVE_SUCCESS);
    assert_eq!(keepalive.wait(0).expect("cached"), KEEPALIVE_SUCCESS);
    keepalive.close().expect("close");

    let session = open("comms-delayed");
    let c2 = open_c2(&session);
    let mut view = c2.channel_view().expect("view");
    let mut comms = view.submit_comms_test(HIGH).expect("CommsTest");
    assert_eq!(
        comms.wait(0).expect_err("poll timeout").kind(),
        &ErrorKind::Timeout
    );
    assert_eq!(comms.wait(5_000).expect("later"), HIGH_REPORT);
    assert_eq!(comms.wait(0).expect("cached"), HIGH_REPORT);
    comms.close().expect("close");
}

#[test]
fn c2_view_close_first_leaves_typed_owner_usable() {
    let _guard = lock();
    let session = open("comms-high");
    let mut c2 = open_c2(&session);
    let mut view = c2.channel_view().expect("view");
    view.close().expect("view close first");
    assert!(!view.is_open() && c2.is_open());
    c2.enable().expect("enable after view close");
    let mut operate = c2.submit_operate(5).expect("Operate");
    assert_eq!(
        operate.wait(5_000).expect("Operate"),
        ModeResult::Success {
            mode: MfaMode::TaskSched
        }
    );
    let mut bit = c2.submit_bit_noop(6).expect("BIT");
    assert_eq!(bit.wait(5_000).expect("BIT"), KEEPALIVE_SUCCESS);
    drop(view); // Drop after explicit Close is harmless.
    c2.close().expect("typed close");
    session.close().expect("session close");
}

#[test]
fn image_view_close_first_leaves_stream_usable() {
    let _guard = lock();
    let session = open("success");
    let mut stream = open_stream(&session);
    let mut view = stream.channel_view().expect("view");
    view.close().expect("view close first");
    stream.start().expect("start after view close");
    let frame = stream.receive(1_000).expect("frame");
    assert_eq!(frame.frame_id, 1);
    drop(view);
    stream.close().expect("stream close");
    session.close().expect("session close");
}

/// Typed owner and Session close while the View stays alive. The complete
/// provider graph, including library unload, must be torn down while the View
/// is still owned (the View never delays it).
///
/// With `no_prior_requests`, teardown must already be complete when Session
/// Close returns: the log is read once, with no waiting. Otherwise completion
/// workers of earlier, already-finished requests may release their final owner
/// on their own thread, so the unload event is awaited (the View stays alive
/// throughout either way). Then every View operation is ProviderFailed and View
/// Close causes no provider activity.
fn weak_view_after_teardown(
    log: &LifetimeLog,
    mut view: ChannelView,
    destroyed: &str,
    no_prior_requests: bool,
) {
    let events = if no_prior_requests {
        log.contents()
    } else {
        log.wait_for("library_unloaded\n")
    };
    assert!(view.is_open());
    assert_eq!(count(&events, destroyed), 1, "{events}");
    assert!(
        events.ends_with("control_destroyed\nmanager_destroyed\nlibrary_unloaded\n"),
        "provider teardown was delayed by the live View:\n{events}"
    );
    assert_expired(&mut view);
    // Expired operations reach no provider object. The only permitted new log
    // lines are the test-build facade's destruction of the completion owner it
    // preallocated for each refused KeepAlive (family 1, Return) and CommsTest
    // (family 2, Comms) submission.
    let after_expired = log.contents();
    let added = after_expired
        .strip_prefix(events.as_str())
        .expect("lifetime log is append-only");
    let mut added_lines: Vec<&str> = added.lines().collect();
    added_lines.sort_unstable();
    assert_eq!(
        added_lines,
        [
            "completion_owner_destroyed_1",
            "completion_owner_destroyed_2"
        ],
        "expired View operations reached the provider:\n{after_expired}"
    );
    view.close().expect("expired view close");
    assert!(!view.is_open());
    view.close().expect("repeated expired view close");
    assert_eq!(
        log.contents(),
        after_expired,
        "View Close produced lifetime activity"
    );
}

#[test]
fn c2_view_is_weak_and_does_not_delay_teardown() {
    let _guard = lock();
    let log = LifetimeLog::new("c2-weak");
    let session = open("comms-high");
    let mut c2 = open_c2(&session);
    let view = c2.channel_view().expect("view");
    c2.close().expect("typed close");
    session.close().expect("session close");
    weak_view_after_teardown(&log, view, "c2_channel_destroyed\n", true);
}

#[test]
fn image_view_is_weak_and_does_not_delay_teardown() {
    let _guard = lock();
    let log = LifetimeLog::new("image-weak");
    let session = open("success");
    let stream = open_stream(&session);
    let view = stream.channel_view().expect("view");
    stream.close().expect("stream close");
    session.close().expect("session close");
    weak_view_after_teardown(&log, view, "channel_destroyed\n", true);
}

#[test]
fn multiple_c2_views_are_independent_and_weak() {
    let _guard = lock();
    let log = LifetimeLog::new("c2-multiple");
    let session = open("comms-high");
    let mut c2 = open_c2(&session);
    let mut first = c2.channel_view().expect("first");
    let mut second = c2.channel_view().expect("second");
    first.close().expect("close first");
    exercise(&mut second);
    c2.close().expect("typed close");
    session.close().expect("session close");
    weak_view_after_teardown(&log, second, "c2_channel_destroyed\n", false);
}

#[test]
fn multiple_image_views_are_independent_and_weak() {
    let _guard = lock();
    let log = LifetimeLog::new("image-multiple");
    let session = open("comms-high");
    let stream = open_stream(&session);
    let mut first = stream.channel_view().expect("first");
    let mut second = stream.channel_view().expect("second");
    first.close().expect("close first");
    exercise(&mut second);
    stream.close().expect("stream close");
    session.close().expect("session close");
    weak_view_after_teardown(&log, second, "channel_destroyed\n", false);
}

/// The admitted KeepAlive, not the View, owns the provider graph: the View,
/// typed C2, and Session all close while it is pending, and the existing
/// ReturnRequest still completes. Teardown follows completion.
#[test]
fn pending_keepalive_outlives_view_c2_and_session() {
    let _guard = lock();
    let log = LifetimeLog::new("keepalive-lifetime");
    let session = open("keepalive-lifetime");
    let mut c2 = open_c2(&session);
    let mut view = c2.channel_view().expect("view");
    let mut request = view.send_keepalive().expect("KeepAlive");
    assert_eq!(
        request.wait(0).expect_err("pending").kind(),
        &ErrorKind::Timeout
    );
    view.close().expect("view close while pending");
    c2.close().expect("C2 close while pending");
    session.close().expect("session close while pending");
    assert_eq!(
        request.wait(5_000).expect("request outlives parents"),
        KEEPALIVE_SUCCESS
    );
    assert_eq!(request.wait(0).expect("cached"), KEEPALIVE_SUCCESS);
    request.close().expect("request close");
    let events = log.wait_for("library_unloaded\n");
    assert_order(&events, "keepalive_completed\n", "c2_channel_destroyed\n");
    assert_order(&events, "c2_channel_destroyed\n", "library_unloaded\n");
}

/// The rich C2 profile, with the values established by the C
/// (`test_ir_c2.c`) and Ada (`Check_Rich`) tests.
fn assert_rich_c2(value: &ChannelCapability) {
    assert_eq!(value.channel_id.uuid(), &mock_uuid(0x11));
    assert_eq!(value.channel_id.descriptive_label(), "channel-\u{3b1}");
    assert_eq!(value.platform_id.uuid(), &mock_uuid(0x31));
    assert_eq!(value.platform_id.descriptive_label(), "platform-\u{20ac}");
    assert_eq!((value.height, value.width), (1080, 1920));
    assert_eq!((value.bit_depth, value.row_pitch), (12, 4096));
    assert_eq!(
        (value.buffer_size, value.image_size),
        (8_388_608, 4_147_200)
    );
    assert_eq!(value.number_of_bands, 3);
    assert_eq!(value.pixel_format, PixelFormat::Rgb);
    assert_eq!(
        value.sensor_types,
        vec![SensorType::GimbalHorizontal, SensorType::StepStare]
    );
    let location = &value.sensor_location;
    assert_eq!(
        (
            location.offset_x_m(),
            location.offset_y_m(),
            location.offset_z_m()
        ),
        (1.25, -2.5, 3.75)
    );
    assert_eq!(location.key(), "sensor-key");
    assert_eq!(location.system_name(), "system-\u{3b2}");
    assert_eq!(
        value.channel_types,
        vec![
            ChannelType::CommandAndControl,
            ChannelType::Instrumentation,
            ChannelType::Reserved2
        ]
    );
    assert_eq!(value.task_schedule_depth, 17);
    assert!(value.odc_available && value.nuc_available);
    assert_eq!(
        value.metadata_capabilities,
        vec![
            MetadataCapability::BadPixelList,
            MetadataCapability::CommandStatus,
            MetadataCapability::ChannelCommsTestRep,
            MetadataCapability::Reserved10
        ]
    );
    assert_eq!(
        value.image_bands,
        vec![
            ImageBand {
                band_index: 2,
                bands: vec![
                    BandInfo {
                        kind: BandType::IrLongwave,
                        min_wavelength_m: 8.0e-6,
                        max_wavelength_m: 12.0e-6,
                    },
                    BandInfo {
                        kind: BandType::IrMidwave,
                        min_wavelength_m: 3.0e-6,
                        max_wavelength_m: 5.0e-6,
                    },
                ],
            },
            ImageBand {
                band_index: 9,
                bands: vec![BandInfo {
                    kind: BandType::VisibleRed,
                    min_wavelength_m: 620.0e-9,
                    max_wavelength_m: 750.0e-9,
                }],
            },
        ]
    );
    assert_eq!(
        value.nav_frames,
        vec![CoordinateSystem::NedSensor, CoordinateSystem::Ecef]
    );
}

/// The mock Image profile: only the fields it sets; every other field keeps
/// the upstream `ChannelCapability` default (zero, false, empty, and
/// `sensorTypes{SensorType::Unspecified}`).
fn assert_image_profile(value: &ChannelCapability) {
    assert_eq!(value.channel_id.uuid(), &[0; 16]);
    assert_eq!(value.channel_id.descriptive_label(), "");
    assert_eq!(value.platform_id.descriptive_label(), "");
    assert_eq!((value.height, value.width, value.bit_depth), (200, 320, 8));
    assert_eq!(
        (value.row_pitch, value.buffer_size, value.image_size),
        (0, 0, 0)
    );
    assert_eq!(value.number_of_bands, 1);
    assert_eq!(value.pixel_format, PixelFormat::Mono);
    assert_eq!(value.sensor_types, vec![SensorType::Unspecified]);
    assert_eq!(value.channel_types, vec![ChannelType::IrstImage]);
    assert_eq!(value.task_schedule_depth, 0);
    assert!(!value.odc_available && !value.nuc_available);
    assert_eq!(
        value.metadata_capabilities,
        vec![
            MetadataCapability::BadPixelList,
            MetadataCapability::LineOfSightReport,
            MetadataCapability::LineOfSightEuler,
            MetadataCapability::NavigationReportResp
        ]
    );
    assert!(value.image_bands.is_empty());
    assert!(value.nav_frames.is_empty());
}

#[test]
fn c2_rich_capability_snapshot_is_fully_owned() {
    let _guard = lock();
    let log = LifetimeLog::new("c2-capability");
    let session = open("capability-rich");
    let mut c2 = open_c2(&session);
    let mut view = c2.channel_view().expect("view");
    let capability = view.capabilities().expect("capabilities");
    assert_rich_c2(&capability);
    view.close().expect("view close");
    c2.close().expect("C2 close");
    session.close().expect("session close");
    assert!(log.contents().ends_with("library_unloaded\n"));
    assert_rich_c2(&capability);
    let copy = capability.clone();
    drop(capability);
    assert_rich_c2(&copy);
}

#[test]
fn image_capability_snapshot_is_fully_owned() {
    let _guard = lock();
    let log = LifetimeLog::new("image-capability");
    let session = open("image-capability-rich");
    let mut stream = open_stream(&session);
    let mut view = stream.channel_view().expect("view");
    let attached = view.capabilities().expect("attached capabilities");
    assert_image_profile(&attached);
    stream.start().expect("start");
    let running = view.capabilities().expect("running capabilities");
    assert_eq!(running, attached);
    view.close().expect("view close");
    stream.close().expect("stream close");
    session.close().expect("session close");
    assert!(log.contents().ends_with("library_unloaded\n"));
    assert_image_profile(&running);
}

fn long_description() -> String {
    let value = format!("{}\u{20ac}{}", "x".repeat(510), "y".repeat(100));
    assert_eq!(value.len(), 613);
    value
}

/// The mock's `metadata_id(seed, ...)` UUID: byte i = seed + 7 * i.
fn mock_uuid(seed: u8) -> [u8; 16] {
    let mut uuid = [0; 16];
    for (index, byte) in uuid.iter_mut().enumerate() {
        *byte = seed.wrapping_add((index * 7) as u8);
    }
    uuid
}

fn open(scenario: &str) -> Session {
    Session::open(mock_provider(), scenario, "").expect("open session")
}

fn open_c2(session: &Session) -> ControlChannel {
    session
        .open_control_channel(&ControlConfig::new(
            id(0, "IR C2 channel"),
            id(0xf0, "test platform"),
            location(),
        ))
        .expect("open C2")
}

fn open_stream(session: &Session) -> ImageStream {
    session
        .open_image_stream(
            &ImageConfig::with_limits(
                id(0, "IR image channel"),
                id(0xf0, "test platform"),
                location(),
                2,
                64,
                2,
            )
            .expect("image config"),
        )
        .expect("open stream")
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
        .unwrap_or_else(|| repository.join("native/build-tests/test-providers"))
        .join("libmock_ir_provider.so")
}

fn count(events: &str, event: &str) -> usize {
    events.matches(event).count()
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
            "ams-mel-rust-common-{name}-{}-{sequence}",
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

    /// Observes asynchronous worker-thread teardown events. This is event
    /// observation, not an ownership proof by sleep.
    fn wait_for(&self, event: &str) -> String {
        for _ in 0..5_000 {
            let contents = self.contents();
            if contents.contains(event) {
                return contents;
            }
            std::thread::sleep(std::time::Duration::from_millis(1));
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
