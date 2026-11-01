//! RUST-IR-001: safe Rust IR HealthAndStatus channel lifecycle against the
//! existing compiled `libmock_ir_provider.so` and the test-enabled facade.
//!
//! The mock's lifetime log (`AMS_MEL_TEST_LIFETIME_LOG`) and completion probe
//! are process-global, so every test is serialized. Provider teardown is
//! observed through the log, and completion-worker quiescence through the
//! facade's blocking test-only owner counter, never by sleeping. A Drop whose
//! detach fails deliberately retains the provider graph (and DSO) for the rest
//! of its process, so that case runs only in a re-executed child process.
//!
//! HealthStatus metadata callbacks are out of scope (RUST-IR-002).

use std::env;
use std::ffi::c_int;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Mutex, MutexGuard, PoisonError};

use ams_mel::{
    BandInfo, BandType, ChannelCapability, ChannelType, ChannelView, CommandReturn,
    CommsTestReport, CommsTestRequest, CommsTestResult, ComponentLocation, ControlConfig,
    CoordinateSystem, ErrorKind, HealthConfig, HealthStatusChannel, ImageBand, ImageConfig,
    MetadataCapability, PixelFormat, ReturnResult, SensorType, Session, UciId,
};

static NEXT_LOG: AtomicU64 = AtomicU64::new(0);
static HEALTH_TEST: Mutex<()> = Mutex::new(());

const CHILD_ENV: &str = "AMS_MEL_RUST_IR_HEALTH_CHILD";
const CHILD_DONE: &str = "IR_HEALTH_CHILD_COMPLETE";
const CLOSED: &str = "IR Health channel is closed";
const UNLOADED: &str = "control_destroyed\nmanager_destroyed\nlibrary_unloaded\n";

/// Completion probe families of the shared common request engine.
const RETURN_FAMILY: u32 = 1;
const COMMS_FAMILY: u32 = 2;

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

// Test-enabled facade only (AMS_MEL_ENABLE_TEST_FAILPOINTS), exactly as used by
// `admission.rs`: snapshot and blocking wait on completion final owners.
extern "C" {
    fn ams_mel_test_completion_boundary(
        family: u32,
        operation: u32,
        target: u64,
        counts: *mut u64,
    ) -> c_int;
    fn ams_mel_test_completion_owner(family: u32, target: u64, observed: *mut u64) -> c_int;
}

fn lock() -> MutexGuard<'static, ()> {
    HEALTH_TEST.lock().unwrap_or_else(PoisonError::into_inner)
}

/// Blocks (on the facade's condition variable, not by sleeping) until every
/// common KeepAlive/CommsTest completion owner created so far is destroyed.
/// The final owner is the worker input's first field and is destroyed last,
/// so afterwards no worker retains a family claim, future, or permit.
fn quiesce_common_requests() {
    for family in [RETURN_FAMILY, COMMS_FAMILY] {
        let mut counts = [0_u64; 4];
        let mut observed = 0;
        // SAFETY: exact test-only signatures; `counts` has the four slots the
        // snapshot operation writes and `observed` is writable storage.
        unsafe {
            assert_eq!(
                ams_mel_test_completion_boundary(family, 3, 0, counts.as_mut_ptr()),
                1
            );
            assert_eq!(
                ams_mel_test_completion_owner(family, counts[2], &mut observed),
                1,
                "completion owners of family {family} were not reclaimed"
            );
        }
    }
}
/// KeepAlive (existing ReturnRequest) and high-ID CommsTest through the weak
/// view, each with a cached repeated Wait, then the view's capabilities.
fn exercise(view: &mut ChannelView) -> ChannelCapability {
    let mut keepalive = view.send_keepalive().expect("KeepAlive");
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

    view.capabilities().expect("view capabilities")
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

fn assert_closed_owner(health: &mut HealthStatusChannel) {
    assert!(!health.is_open());
    for error in [
        health.enable().expect_err("enable after close"),
        health.capabilities().expect_err("capabilities after close"),
        health.channel_view().expect_err("view after close"),
    ] {
        assert_eq!(error.kind(), &ErrorKind::ProviderFailed);
        assert_eq!(error.diagnostic(), Some(CLOSED));
    }
}

/// The mock Health profile: `rich_channel_capability()` with exactly the
/// HealthAndStatus channel type and four Health metadata capabilities, in the
/// order established by the C `test_ir_health.c` capability contract.
fn assert_health_profile(value: &ChannelCapability) {
    assert_eq!(value.channel_types, vec![ChannelType::HealthAndStatus]);
    assert_eq!(
        value.metadata_capabilities,
        vec![
            MetadataCapability::MfaStatus,
            MetadataCapability::MfaStatusDetailed,
            MetadataCapability::BitStatus,
            MetadataCapability::SubsystemStatusResp,
        ]
    );
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
    assert_eq!(value.task_schedule_depth, 17);
    assert!(value.odc_available && value.nuc_available);
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
/// The mock's `metadata_id(seed, ...)` UUID: byte i = seed + 7 * i.
fn mock_uuid(seed: u8) -> [u8; 16] {
    let mut uuid = [0; 16];
    for (index, byte) in uuid.iter_mut().enumerate() {
        *byte = seed.wrapping_add((index * 7) as u8);
    }
    uuid
}

fn mock_provider() -> PathBuf {
    let repository = Path::new(env!("CARGO_MANIFEST_DIR")).join("../..");
    env::var_os("AMS_MEL_TEST_PROVIDER_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| repository.join("native/build-tests/test-providers"))
        .join("libmock_ir_provider.so")
}

fn open(scenario: &str) -> Session {
    Session::open(mock_provider(), scenario, "").expect("open session")
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

fn health_config() -> HealthConfig {
    HealthConfig::new(
        id(0, "IR Health channel"),
        id(0xf0, "test platform"),
        location(),
    )
}

fn open_health(session: &Session) -> HealthStatusChannel {
    let health = session
        .open_health_channel(&health_config())
        .expect("open Health");
    assert!(health.is_open());
    health
}

fn count(events: &str, event: &str) -> usize {
    events.lines().filter(|line| *line == event).count()
}

fn assert_order(events: &str, first: &str, second: &str) {
    let first_at = events.find(first).expect("first event");
    let second_at = events.find(second).expect("second event");
    assert!(first_at < second_at, "events out of order:\n{events}");
}

/// Per-test provider lifetime log. Only provider lifetime events are
/// compared; the test-enabled facade's `completion_*`/probe lines are filtered
/// out by [`provider_events`].
struct LifetimeLog {
    directory: PathBuf,
    path: PathBuf,
}

impl LifetimeLog {
    fn new(name: &str) -> Self {
        let sequence = NEXT_LOG.fetch_add(1, Ordering::Relaxed);
        let directory = env::temp_dir().join(format!(
            "ams-mel-rust-ir-health-{name}-{}-{sequence}",
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

    /// Provider-side lifetime events only, in order.
    fn provider_events(&self) -> String {
        self.contents()
            .lines()
            .filter(|line| !is_probe_line(line))
            .map(|line| format!("{line}\n"))
            .collect()
    }
}

impl Drop for LifetimeLog {
    fn drop(&mut self) {
        env::remove_var("AMS_MEL_TEST_LIFETIME_LOG");
        let _ = fs::remove_file(&self.path);
        let _ = fs::remove_dir(&self.directory);
    }
}

/// Test-facade completion probe lines (`<event>_<family>`), as opposed to mock
/// provider lifetime events.
fn is_probe_line(line: &str) -> bool {
    [
        "completion_",
        "future_consumed_",
        "provider_result_scope_exited_",
        "safe_owner_destroyed_",
        "worker_return_boundary_",
        "get_returned_",
        "get_threw_",
    ]
    .iter()
    .any(|prefix| line.starts_with(prefix))
}
/// Open attaches but does not enable. Capabilities are complete while
/// Attached and Enabled, Enable is explicit and idempotent after success, and
/// Close disables, detaches, and destroys the provider channel exactly once.
#[test]
fn open_attaches_without_enabling_and_enable_is_explicit_and_idempotent() {
    let _guard = lock();
    let log = LifetimeLog::new("lifecycle");
    let session = open("health-capability");
    let mut health = open_health(&session);
    let attached_events = log.provider_events();
    assert_eq!(count(&attached_events, "channel_attached"), 1);
    assert_eq!(count(&attached_events, "health_enabled"), 0, "Open enabled");

    let attached = health.capabilities().expect("attached capabilities");
    assert_health_profile(&attached);
    assert_eq!(attached.channel_types, vec![ChannelType::HealthAndStatus]);

    health.enable().expect("enable");
    health.enable().expect("repeated enable");
    assert_eq!(
        count(&log.provider_events(), "health_enabled"),
        1,
        "repeated Enable after success reached the provider"
    );
    let enabled = health.capabilities().expect("enabled capabilities");
    assert_eq!(enabled, attached);

    health.close().expect("close");
    assert_closed_owner(&mut health);
    health.close().expect("repeated close");
    let closed = log.provider_events();
    assert_eq!(count(&closed, "health_disabled"), 1);
    assert_eq!(count(&closed, "channel_detached"), 1);
    assert_eq!(count(&closed, "health_channel_destroyed"), 1);
    assert_order(&closed, "health_enabled", "health_disabled");
    assert_order(&closed, "health_disabled", "channel_detached");
    assert_order(&closed, "channel_detached", "health_channel_destroyed");
    assert_eq!(count(&closed, "library_unloaded"), 0);

    session.close().expect("session close");
    assert!(log.provider_events().ends_with(UNLOADED));
    // The Rust-owned snapshot outlives the owner, Session, and provider DSO.
    assert_health_profile(&enabled);
}

/// Close of a never-enabled channel detaches without a provider disable.
#[test]
fn close_without_enable_detaches_without_disable() {
    let _guard = lock();
    let log = LifetimeLog::new("attached-close");
    let session = open("health-capability");
    let mut health = open_health(&session);
    health.close().expect("close");
    session.close().expect("session close");
    let events = log.provider_events();
    assert_eq!(count(&events, "health_enabled"), 0);
    assert_eq!(count(&events, "health_disabled"), 0);
    assert_eq!(count(&events, "channel_detached"), 1);
    assert_eq!(count(&events, "health_channel_destroyed"), 1);
    assert!(events.ends_with(UNLOADED), "{events}");
}

/// A rejected Enable is an error, never enabled service. The native family
/// is then Failed, so Enable and capabilities fail, while Close still
/// disables and detaches.
#[test]
fn enable_failure_is_reported_and_owner_remains_closable() {
    let _guard = lock();
    let log = LifetimeLog::new("enable-fail");
    let session = open("health-enable-fail");
    let mut health = open_health(&session);
    let error = health.enable().expect_err("provider rejects enable");
    assert_eq!(error.kind(), &ErrorKind::ProviderFailed);
    assert_eq!(error.diagnostic(), Some("Health enable failed"));
    assert!(health.is_open());
    assert_eq!(
        health.enable().expect_err("enable after failure").kind(),
        &ErrorKind::ProviderFailed
    );
    assert_eq!(
        health
            .capabilities()
            .expect_err("capabilities after failure")
            .kind(),
        &ErrorKind::ProviderFailed
    );
    assert_eq!(count(&log.provider_events(), "health_enabled"), 1);
    health.close().expect("close after failed enable");
    assert!(!health.is_open());
    session.close().expect("session close");
    let events = log.provider_events();
    assert_eq!(count(&events, "health_disabled"), 1);
    assert_eq!(count(&events, "channel_detached"), 1);
    assert!(events.ends_with(UNLOADED), "{events}");
}
/// Inherited KeepAlive and high-ID CommsTest through the weak view while
/// Attached and again while Enabled; view and typed capabilities agree.
#[test]
fn view_keepalive_comms_and_capabilities_attached_and_enabled() {
    let _guard = lock();
    let log = LifetimeLog::new("view-services");
    let session = open("health-ada-common");
    let mut health = open_health(&session);
    let mut view = health.channel_view().expect("Health view");
    assert!(view.is_open());

    let attached = exercise(&mut view);
    assert_health_profile(&attached);
    assert_eq!(attached, health.capabilities().expect("typed capabilities"));
    health.enable().expect("enable");
    let enabled = exercise(&mut view);
    assert_eq!(enabled, attached);

    let events = log.provider_events();
    assert_eq!(count(&events, "keepalive_sent"), 2);
    assert_eq!(count(&events, "comms_sent"), 2);
    view.close().expect("view close");
    assert!(!view.is_open());
    assert!(health.is_open(), "view close closed the typed owner");
    quiesce_common_requests();
    health.close().expect("Health close");
    session.close().expect("session close");
    assert!(log.provider_events().ends_with(UNLOADED));
}

/// Provider send exceptions keep their category and diagnostic, create no
/// request owner, and leave the typed owner usable and closable.
#[test]
fn view_send_exceptions_keep_category_and_owner() {
    let _guard = lock();
    for (scenario, comms, message) in [
        (
            "health-keepalive-throw",
            false,
            "Health KeepAlive send failed",
        ),
        ("health-comms-throw", true, "Health Comms send failed"),
    ] {
        let log = LifetimeLog::new(scenario);
        let session = open(scenario);
        let mut health = open_health(&session);
        let mut view = health.channel_view().expect("view");
        let error = if comms {
            view.submit_comms_test(HIGH).expect_err("CommsTest throws")
        } else {
            view.send_keepalive().expect_err("KeepAlive throws")
        };
        assert_eq!(error.kind(), &ErrorKind::ProviderException, "{scenario}");
        assert_eq!(error.diagnostic(), Some(message), "{scenario}");
        health.enable().expect("owner remains usable");
        health
            .capabilities()
            .expect("capabilities after send failure");
        quiesce_common_requests();
        health.close().expect("close");
        session.close().expect("session close");
        drop(view);
        let events = log.provider_events();
        assert_eq!(count(&events, "health_channel_destroyed"), 1, "{events}");
        assert!(events.ends_with(UNLOADED), "{events}");
    }
}

/// Closing the parent Session first does not invalidate the independently
/// retained Health owner or its view; the provider graph stays alive until
/// the Health owner closes, then tears down completely.
#[test]
fn parent_session_close_before_health_close() {
    let _guard = lock();
    let log = LifetimeLog::new("parent-first");
    let session = open("health-ada-common");
    let mut health = open_health(&session);
    let mut view = health.channel_view().expect("view");
    session.close().expect("parent session close first");
    let after_parent = log.provider_events();
    assert_eq!(
        count(&after_parent, "control_destroyed"),
        0,
        "{after_parent}"
    );
    assert_eq!(
        count(&after_parent, "library_unloaded"),
        0,
        "{after_parent}"
    );

    assert!(health.is_open());
    let attached = health
        .capabilities()
        .expect("capabilities after parent close");
    assert_health_profile(&attached);
    health.enable().expect("enable after parent close");
    assert_eq!(exercise(&mut view), attached);
    quiesce_common_requests();
    let before_close = log.provider_events();
    assert_eq!(count(&before_close, "control_destroyed"), 0);

    health.close().expect("Health close");
    let events = log.provider_events();
    assert_eq!(count(&events, "health_disabled"), 1);
    assert_eq!(count(&events, "channel_detached"), 1);
    assert_order(&events, "health_channel_destroyed", "control_destroyed");
    assert!(events.ends_with(UNLOADED), "{events}");
    assert_expired(&mut view);
    view.close().expect("expired view close");
}
/// Weak view lifetime: the view never retains the typed owner or delays
/// teardown. After typed-owner destruction (explicit Close or Drop) every view
/// operation fails cleanly with ProviderFailed and reaches no provider object,
/// and view Close causes no lifetime activity.
#[test]
fn weak_view_survives_owner_destruction_and_fails_cleanly() {
    let _guard = lock();
    for drop_owner in [false, true] {
        let log = LifetimeLog::new(if drop_owner { "weak-drop" } else { "weak" });
        let session = open("health-ada-common");
        let mut health = open_health(&session);
        let mut first = health.channel_view().expect("first view");
        let mut second = health.channel_view().expect("second view");
        exercise(&mut first);
        first.close().expect("close first view");
        assert!(health.is_open(), "view close closed the typed owner");
        exercise(&mut second);
        quiesce_common_requests();
        if drop_owner {
            drop(health);
        } else {
            health.close().expect("typed close");
            assert_closed_owner(&mut health);
        }
        session.close().expect("session close");

        let events = log.provider_events();
        assert_eq!(count(&events, "health_channel_destroyed"), 1, "{events}");
        assert!(
            events.ends_with(UNLOADED),
            "provider teardown was delayed by the live view:\n{events}"
        );
        assert_expired(&mut second);
        assert_eq!(
            log.provider_events(),
            events,
            "expired view operations reached the provider"
        );
        second.close().expect("expired view close");
        assert!(!second.is_open());
        second.close().expect("repeated expired view close");
        drop(first); // Drop after explicit Close is harmless.
        assert_eq!(log.provider_events(), events, "view Close had effects");
    }
}

/// Closing a view first leaves the typed owner fully usable.
#[test]
fn view_close_first_leaves_typed_owner_usable() {
    let _guard = lock();
    let log = LifetimeLog::new("view-first");
    let session = open("health-ada-common");
    let mut health = open_health(&session);
    let mut view = health.channel_view().expect("view");
    view.close().expect("view close first");
    drop(view);
    assert!(health.is_open());
    health.enable().expect("enable after view close");
    assert_health_profile(&health.capabilities().expect("capabilities"));
    let mut later = health.channel_view().expect("later view");
    exercise(&mut later);
    quiesce_common_requests();
    health.close().expect("typed close");
    session.close().expect("session close");
    assert!(log.provider_events().ends_with(UNLOADED));
}

/// `health-detach-fail`: the first native Close fails and retains the public
/// handle; nothing is reported as cleaned up. A later explicit Close (after
/// the parent Session has also closed) detaches and tears everything down.
#[test]
fn detach_failure_retains_owner_for_explicit_close_retry() {
    let _guard = lock();
    let log = LifetimeLog::new("detach-retry");
    let session = open("health-detach-fail");
    let mut health = open_health(&session);
    health.enable().expect("enable");
    let mut view = health.channel_view().expect("view");

    let error = health.close().expect_err("first close fails to detach");
    assert_eq!(error.kind(), &ErrorKind::ProviderFailed);
    assert_eq!(
        error.diagnostic(),
        Some("Health detach failed; provider state retained")
    );
    assert!(health.is_open(), "retained handle was reported as closed");
    let failed = log.provider_events();
    assert_eq!(count(&failed, "channel_detach_failed"), 1);
    assert_eq!(count(&failed, "channel_detached"), 0);
    assert_eq!(count(&failed, "health_channel_destroyed"), 0);
    // The native family is Closed, so new common requests are refused.
    assert_eq!(
        view.send_keepalive()
            .expect_err("closed family refuses KeepAlive")
            .kind(),
        &ErrorKind::ProviderFailed
    );

    session
        .close()
        .expect("parent session close between attempts");
    let retained = log.provider_events();
    assert_eq!(count(&retained, "health_channel_destroyed"), 0);
    assert_eq!(count(&retained, "library_unloaded"), 0, "{retained}");

    health.close().expect("explicit close retry");
    assert_closed_owner(&mut health);
    health.close().expect("repeated close after retry");
    let events = log.provider_events();
    // Native cleanup keeps `enable_attempted` after a failed detach, so the
    // retry disables again before the successful detach; the wrapper adds
    // no provider calls of its own.
    assert_eq!(count(&events, "health_disabled"), 2, "{events}");
    assert_eq!(count(&events, "channel_detach_failed"), 1);
    assert_eq!(count(&events, "channel_detached"), 1);
    assert_eq!(count(&events, "health_channel_destroyed"), 1);
    assert_order(&events, "channel_detach_failed", "channel_detached");
    assert!(events.ends_with(UNLOADED), "{events}");
    assert_expired(&mut view);
}
/// Invalid configuration text is rejected before any native call, so no
/// channel is attached and the Session remains usable.
#[test]
fn invalid_configuration_is_rejected_before_attachment() {
    let _guard = lock();
    let log = LifetimeLog::new("invalid");
    let session = open("health-capability");
    for error in [
        UciId::new([0; 16], "bad\0channel").expect_err("NUL channel label"),
        ComponentLocation::new(0.0, 0.0, 0.0, "bad\0key", "system").expect_err("NUL key"),
        ComponentLocation::new(0.0, 0.0, 0.0, "key", "bad\0system").expect_err("NUL system"),
    ] {
        assert_eq!(error.kind(), &ErrorKind::InvalidArgument);
    }
    assert_eq!(count(&log.provider_events(), "channel_attached"), 0);
    let mut health = open_health(&session);
    health.close().expect("valid open still works");
    session.close().expect("session close");
}

/// Failed attachment keeps the native category and diagnostic, publishes no
/// owner, and leaves nothing that prevents complete provider teardown.
#[test]
fn failed_attachment_publishes_no_owner() {
    let _guard = lock();
    for (scenario, kind, message) in [
        (
            "attach-null",
            ErrorKind::FactoryFailed,
            "attachChannel returned null",
        ),
        (
            "attach-throw",
            ErrorKind::ProviderException,
            "provider exception during Health open",
        ),
    ] {
        let log = LifetimeLog::new(scenario);
        let session = open(scenario);
        let error = session
            .open_health_channel(&health_config())
            .expect_err("attachment fails");
        assert_eq!(error.kind(), &kind, "{scenario}");
        assert_eq!(error.diagnostic(), Some(message), "{scenario}");
        session.close().expect("session close");
        let events = log.provider_events();
        assert_eq!(count(&events, "channel_attached"), 1, "{events}");
        assert_eq!(count(&events, "health_enabled"), 0, "{events}");
        assert!(events.ends_with(UNLOADED), "{events}");
    }
}
/// HealthStatus, C2, and Image owners opened from one Session are
/// independent typed owners: each has its own capability profile and weak
/// view, and closing one never closes or invalidates another.
#[test]
fn health_c2_and_image_owners_are_independent() {
    let _guard = lock();
    let log = LifetimeLog::new("cross-family");
    // `health-common` advertises both HealthAndStatus and CommandAndControl.
    let session = open("health-common");
    let mut health = open_health(&session);
    let mut c2 = session
        .open_control_channel(&ControlConfig::new(
            id(0, "IR C2 channel"),
            id(0xf0, "test platform"),
            location(),
        ))
        .expect("open C2");
    let stream = session
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
        .expect("open Image");
    health.enable().expect("Health enable");
    c2.enable().expect("C2 enable");
    let mut health_view = health.channel_view().expect("Health view");
    let mut c2_view = c2.channel_view().expect("C2 view");
    let mut image_view = stream.channel_view().expect("Image view");
    assert_health_profile(&health_view.capabilities().expect("Health view caps"));
    assert_eq!(
        c2_view.capabilities().expect("C2 caps").channel_types,
        vec![ChannelType::CommandAndControl]
    );
    assert_eq!(
        image_view.capabilities().expect("Image caps").pixel_format,
        PixelFormat::Mono
    );

    health.close().expect("Health close first");
    assert_expired(&mut health_view);
    assert!(c2.is_open());
    assert_eq!(
        c2_view
            .capabilities()
            .expect("C2 after Health close")
            .channel_types,
        vec![ChannelType::CommandAndControl]
    );
    image_view.capabilities().expect("Image after Health close");
    let after_health = log.provider_events();
    assert_eq!(count(&after_health, "health_channel_destroyed"), 1);
    assert_eq!(count(&after_health, "c2_channel_destroyed"), 0);
    assert_eq!(count(&after_health, "channel_destroyed"), 0);

    c2.close().expect("C2 close");
    image_view.capabilities().expect("Image after C2 close");
    stream.close().expect("Image close");
    session.close().expect("session close");
    let events = log.provider_events();
    assert_eq!(count(&events, "health_channel_destroyed"), 1);
    assert_eq!(count(&events, "c2_channel_destroyed"), 1);
    assert_eq!(count(&events, "channel_destroyed"), 1);
    assert_eq!(count(&events, "channel_detached"), 3);
    assert!(events.ends_with(UNLOADED), "{events}");
}

/// Runs the Drop-after-detach-failure case in a fresh process: a Drop whose
/// native Close cannot detach deliberately retains the provider graph (and
/// the mock DSO) for the rest of that process.
#[test]
fn drop_after_detach_failure_retains_provider_graph_without_panic() {
    let _guard = lock();
    let output = Command::new(env::current_exe().expect("test executable"))
        .args([
            "--exact",
            "drop_detach_failure_child_entry",
            "--ignored",
            "--nocapture",
            "--test-threads=1",
        ])
        .env(CHILD_ENV, "drop")
        .env_remove("AMS_MEL_TEST_LIFETIME_LOG")
        .output()
        .expect("spawn child");
    let stdout = String::from_utf8_lossy(&output.stdout);
    let stderr = String::from_utf8_lossy(&output.stderr);
    assert!(
        output.status.success(),
        "child failed\nstdout:\n{stdout}\nstderr:\n{stderr}"
    );
    assert!(
        stdout.contains(CHILD_DONE),
        "child did not run\nstdout:\n{stdout}\nstderr:\n{stderr}"
    );
}

/// Child-process entry for the test above; it does nothing unless selected.
#[test]
#[ignore = "child-process entry, run only by the Drop detach-failure test"]
fn drop_detach_failure_child_entry() {
    if env::var_os(CHILD_ENV).is_none() {
        return;
    }
    let log = LifetimeLog::new("drop-detach-fail");
    let session = open("health-detach-fail");
    let mut health = open_health(&session);
    health.enable().expect("enable");
    let mut view = health.channel_view().expect("view");
    drop(health); // Best-effort native Close: detach fails, no panic.
    session.close().expect("session close");
    let events = log.provider_events();
    assert_eq!(count(&events, "channel_detach_failed"), 1, "{events}");
    assert_eq!(count(&events, "channel_detached"), 0, "{events}");
    // Not destroyed, not retried, and the provider graph is not unloaded.
    assert_eq!(count(&events, "health_channel_destroyed"), 0, "{events}");
    assert_eq!(count(&events, "control_destroyed"), 0, "{events}");
    assert_eq!(count(&events, "library_unloaded"), 0, "{events}");
    // The retained family is Closed: the weak view fails cleanly.
    assert_expired(&mut view);
    assert_eq!(log.provider_events(), events);
    view.close().expect("view close");
    println!("{CHILD_DONE}");
}
