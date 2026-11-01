//! RUST-IR-002: safe Rust IR HealthStatus metadata reception against the
//! existing compiled `libmock_ir_provider.so` and the test-enabled facade.
//!
//! The mock's lifetime log (`AMS_MEL_TEST_LIFETIME_LOG`) and its test
//! failpoint environment variables are process-global, so every test is
//! serialized. Scenarios whose callback state is retained for the provider
//! channel's lifetime, or that install a callback barrier, run only in
//! re-executed child processes. Synchronization is observed through the
//! mock's file barriers and lifetime log, never by sleeping.

use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Mutex, MutexGuard, PoisonError};
use std::time::{Duration, Instant};

use ams_mel::{
    About, ActiveBit, BitResult, BitStatus, CompletedBit, CompletedBitItem, ComponentLocation,
    ComponentState, CsciInfo, CsciMode, CsciVersion, ErrorKind, Euler, FailureLevel, Fault,
    FaultAmbiguityGroup, FaultData, FaultSeverity, FaultState, ForeignKey, HealthConfig,
    HealthMetadataCounters, HealthMetadataEvent, HealthMetadataKind, HealthMetadataStream,
    HealthStatusChannel, InstallationDetails, MfaComponent, MfaStatus, NameValuePair, RfMfaState,
    SecurityArtifact, SecurityAuditRecord, SecurityAuthenticationCategory, SecurityEvent,
    SecurityFileManagementCategory, SecurityIntegrityCategory, SecurityKeyManagementCategory,
    SecurityOutcome, SecuritySanitizationCategory, SecuritySeverity, SecuritySystemCategory,
    Session, StateTransitionStatus, SubsystemDependency, SubsystemStatus, TemperatureState,
    TemperatureStatus, UciId,
};

static NEXT_DIR: AtomicU64 = AtomicU64::new(0);
static METADATA_TEST: Mutex<()> = Mutex::new(());

const CHILD_ENV: &str = "AMS_MEL_RUST_IR_HEALTH_METADATA_CHILD";
const CHILD_BARRIER_ENV: &str = "AMS_MEL_TEST_HEALTH_CALLBACK_BARRIER";
const CHILD_DONE: &str = "IR_HEALTH_METADATA_CHILD_COMPLETE";
const STREAM_CLOSED: &str = "IR Health metadata stream is closed";
const HEALTH_CLOSED: &str = "IR Health channel is closed";
const UNLOADED: &str = "control_destroyed\nmanager_destroyed\nlibrary_unloaded\n";
/// Health-rich publishes 12 events during registration: MFA, BIT, Subsystem,
/// Discrete, seven Security Audit alternatives, MFA Status Detailed.
const RICH_EVENTS: usize = 12;

fn lock() -> MutexGuard<'static, ()> {
    METADATA_TEST.lock().unwrap_or_else(PoisonError::into_inner)
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

fn config_id(start: u8, label: &str) -> UciId {
    let mut uuid = [0; 16];
    for (index, byte) in uuid.iter_mut().enumerate() {
        *byte = start.wrapping_add(index as u8);
    }
    UciId::new(uuid, label).expect("valid ID")
}

fn open_health(session: &Session) -> HealthStatusChannel {
    let config = HealthConfig::new(
        config_id(0, "IR Health channel"),
        config_id(0xf0, "test platform"),
        ComponentLocation::new(1.25, -2.5, 3.75, "station-1", "mock-aircraft").expect("location"),
    );
    session.open_health_channel(&config).expect("open Health")
}

fn counters(received: u64, dropped: u64, malformed: u64) -> HealthMetadataCounters {
    HealthMetadataCounters {
        events_received: received,
        events_dropped_queue_full: dropped,
        malformed_or_unsupported: malformed,
    }
}

fn expect_kind(result: Result<HealthMetadataEvent, ams_mel::Error>, kind: ErrorKind) {
    let error = result.expect_err("receive must fail");
    assert_eq!(error.kind(), &kind, "{error}");
}

fn drain(stream: &mut HealthMetadataStream, count: usize) -> Vec<HealthMetadataEvent> {
    (0..count)
        .map(|index| {
            stream
                .receive(0)
                .unwrap_or_else(|e| panic!("event {index}: {e}"))
        })
        .collect()
}

fn count(events: &str, event: &str) -> usize {
    events.lines().filter(|line| *line == event).count()
}

/// Asserts that the lines occur in this order (each at its first occurrence).
fn assert_order(events: &str, lines: &[&str]) {
    let positions: Vec<usize> = lines
        .iter()
        .map(|line| {
            events
                .lines()
                .position(|candidate| candidate == *line)
                .unwrap_or_else(|| panic!("missing {line} in:\n{events}"))
        })
        .collect();
    assert!(
        positions.windows(2).all(|pair| pair[0] < pair[1]),
        "events out of order {lines:?}:\n{events}"
    );
}

/// Per-test scratch directory holding the provider lifetime log (and, for the
/// quiescence child, the mock's callback barrier files).
struct Scratch {
    directory: PathBuf,
}

impl Scratch {
    fn new(name: &str) -> Self {
        let sequence = NEXT_DIR.fetch_add(1, Ordering::Relaxed);
        let directory = env::temp_dir().join(format!(
            "ams-mel-rust-ir-health-metadata-{name}-{}-{sequence}",
            std::process::id()
        ));
        fs::create_dir(&directory).expect("create scratch directory");
        env::set_var("AMS_MEL_TEST_LIFETIME_LOG", directory.join("lifetime.log"));
        Self { directory }
    }

    fn path(&self, name: &str) -> PathBuf {
        self.directory.join(name)
    }

    /// Provider-side lifetime events only (test-facade probe lines removed).
    fn events(&self) -> String {
        fs::read_to_string(self.path("lifetime.log"))
            .unwrap_or_default()
            .lines()
            .filter(|line| {
                ![
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
            })
            .map(|line| format!("{line}\n"))
            .collect()
    }
}

impl Drop for Scratch {
    fn drop(&mut self) {
        env::remove_var("AMS_MEL_TEST_LIFETIME_LOG");
        let _ = fs::remove_dir_all(&self.directory);
    }
}

// ---- Exact `health-rich` content (native/tests/mock_provider.cpp) ----

/// The mock's `metadata_id(seed, label)`: UUID byte i = seed + 7 * i.
fn mock_id(seed: u8, label: &str) -> UciId {
    let mut uuid = [0; 16];
    for (index, byte) in uuid.iter_mut().enumerate() {
        *byte = seed.wrapping_add((index * 7) as u8);
    }
    UciId::new(uuid, label).expect("mock ID")
}

fn euler(roll: f64, pitch: f64, yaw: f64) -> Euler {
    Euler { roll, pitch, yaw }
}

/// `rich_mfa_status()`.
fn rich_mfa_status() -> MfaStatus {
    let location =
        ComponentLocation::new(1.5, -2.5, 3.5, "bay", "platform-\u{20ac}").expect("location");
    let installation_location_id = ForeignKey {
        key: "rack-\u{3b2}".into(),
        system_name: "aircraft".into(),
    };
    let component =
        |seed, label, state, temperature_c, temperature, orientation, boresight| MfaComponent {
            component_id: mock_id(seed, label),
            state,
            temperature: TemperatureStatus {
                temperature_c,
                state: temperature,
            },
            installation_location_id: installation_location_id.clone(),
            installation_details: InstallationDetails {
                location: location.clone(),
                orientation,
                boresight,
            },
        };
    let forward = euler(0.1, 0.2, 0.3);
    let back = euler(-0.4, 0.5, -0.6);
    MfaStatus {
        state: RfMfaState::Maintenance,
        state_description: "healthy-\u{3b1}".into(),
        mode_description: "mode-\u{20ac}".into(),
        transition_status: StateTransitionStatus::Transitioning,
        about: About {
            model: "model".into(),
            serial_number: "serial".into(),
            software_version: "software".into(),
            bootloader_software_version: "boot".into(),
            hardware_version: "hardware".into(),
        },
        components: vec![
            component(
                0x91,
                "component-\u{3b3}",
                ComponentState::Operational,
                42.25,
                TemperatureState::Normal,
                forward,
                back,
            ),
            // The second component deliberately swaps orientation/boresight.
            component(
                0xa1,
                "component two",
                ComponentState::Degraded,
                -12.5,
                TemperatureState::UnderTemp,
                back,
                forward,
            ),
        ],
    }
}

/// `rich_bit_status()`.
fn rich_bit_status() -> BitStatus {
    let item = |name: &str, result, reason: &str| CompletedBitItem {
        bit_item_name: name.into(),
        result,
        fail_reason: reason.into(),
    };
    let data = |key: &str, value: &str, format: &str, units: &str| FaultData {
        key: key.into(),
        value: value.into(),
        format: format.into(),
        units: units.into(),
    };
    BitStatus {
        active_bits: vec![
            ActiveBit {
                bit_id: mock_id(0x81, "active negative"),
                estimated_completion_time_ns: -5,
                estimated_percent_complete: 1.25,
            },
            ActiveBit {
                bit_id: mock_id(0x82, "active zero"),
                estimated_completion_time_ns: 0,
                estimated_percent_complete: 0.0,
            },
            ActiveBit {
                bit_id: mock_id(0x83, "active positive"),
                estimated_completion_time_ns: 987_654_321,
                estimated_percent_complete: 0.5,
            },
        ],
        completed_bits: vec![
            CompletedBit {
                bit_id: mock_id(0x91, "completed pass"),
                time_tag_ns: 42,
                result: BitResult::Pass,
                fail_reason: String::new(),
                bit_items: vec![
                    item("item pass", BitResult::Pass, ""),
                    item("item-\u{3b3}", BitResult::Fail, "item reason"),
                ],
            },
            CompletedBit {
                bit_id: mock_id(0x92, "completed fail"),
                time_tag_ns: -99,
                result: BitResult::Fail,
                fail_reason: "failure-\u{20ac}".into(),
                bit_items: Vec::new(),
            },
        ],
        faults: vec![Fault {
            fault_id: mock_id(0xd0, "fault-\u{3b6}"),
            severity: FaultSeverity::Warning,
            state: FaultState::Set,
            fault_data: vec![
                data("temperature", "101", "integer", "\u{b0}C"),
                data("phase-\u{3b4}", "bad", "text", ""),
            ],
            detection_time_ns: -1_234_567,
            fault_code: "F-42".into(),
            fault_description: "overheat-\u{20ac}".into(),
            component_ids: vec![
                mock_id(0xe1, "fault component one"),
                mock_id(0xe2, "fault component two"),
            ],
            ambiguity_groups: vec![
                FaultAmbiguityGroup {
                    diagnostic_test_ids: vec![
                        mock_id(0xb1, "diagnostic one"),
                        mock_id(0xb2, "diagnostic two"),
                    ],
                    component_ids: vec![
                        mock_id(0xc1, "ambiguous one"),
                        mock_id(0xc2, "ambiguous two"),
                    ],
                },
                FaultAmbiguityGroup {
                    diagnostic_test_ids: vec![mock_id(0xb3, "diagnostic three")],
                    component_ids: vec![mock_id(0xc3, "ambiguous three")],
                },
            ],
        }],
    }
}

/// `rich_subsystem_status()`: reported counts (99, 88) differ from the
/// copied collection lengths (2, 2) and are preserved independently.
fn rich_subsystem_status() -> SubsystemStatus {
    SubsystemStatus {
        subsystem_id: 0x8000_0001,
        criticality: 7,
        status_sequence_number: 0xf000_0002,
        failure: FailureLevel::Major,
        subsystem_count: 99,
        subsystems: vec![
            SubsystemDependency {
                subsystem_id: 0x8000_0003,
                criticality: 8,
                failure: FailureLevel::Critical,
            },
            SubsystemDependency {
                subsystem_id: 4,
                criticality: 9,
                failure: FailureLevel::Available,
            },
        ],
        csci_count: 88,
        csci: vec![
            CsciInfo {
                csci: "flight-\u{3b4}".into(),
                mode: CsciMode::Operational,
                version: CsciVersion {
                    source: 1,
                    major_revision: 2,
                    minor_revision: 3,
                    engineering_revision: 4,
                },
                criticality: 10,
                failure: FailureLevel::Parametric,
                bit_report: 0x8000_0005,
                connection_established: true,
            },
            CsciInfo {
                csci: "maintenance".into(),
                mode: CsciMode::Maintenance,
                version: CsciVersion {
                    source: 5,
                    major_revision: 6,
                    minor_revision: 7,
                    engineering_revision: 8,
                },
                criticality: 11,
                failure: FailureLevel::Informational,
                bit_report: 6,
                connection_established: false,
            },
        ],
    }
}

fn pairs(values: &[(&str, &str)]) -> Vec<NameValuePair> {
    values
        .iter()
        .map(|(name, value)| NameValuePair {
            name: (*name).into(),
            value: (*value).into(),
        })
        .collect()
}

/// `security_record(alternative)` for alternatives 0..=6: shared record
/// fields, plus each published SecurityEventType alternative in order.
fn rich_security_record(alternative: usize) -> SecurityAuditRecord {
    let event = match alternative {
        0 => SecurityEvent::None,
        1 => SecurityEvent::Authentication {
            category: SecurityAuthenticationCategory::MdfAuthentication,
            details: "auth-\u{3b6}".into(),
            subsystem_id: mock_id(1, "auth subsystem"),
            service_id: mock_id(2, "auth service"),
            mdf_id: mock_id(3, "auth mdf"),
        },
        2 => SecurityEvent::Integrity {
            category: SecurityIntegrityCategory::OtherIntegrityCheck,
            details: "integrity-\u{3b7}".into(),
            mdf_id: mock_id(4, "integrity mdf"),
        },
        3 => SecurityEvent::FileManagement {
            category: SecurityFileManagementCategory::OtherFileUpdate,
            details: "file-\u{3b8}".into(),
            mdf_id: mock_id(5, "file mdf"),
        },
        4 => SecurityEvent::KeyManagement {
            category: SecurityKeyManagementCategory::CertificateDeletion,
            details: "key-\u{3b9}".into(),
        },
        5 => SecurityEvent::System {
            category: SecuritySystemCategory::Fault,
            details: "system-\u{3ba}".into(),
        },
        6 => SecurityEvent::Sanitization {
            category: SecuritySanitizationCategory::ClassifiedDataErase,
            details: "sanitize-\u{3bb}".into(),
        },
        other => panic!("no security alternative {other}"),
    };
    SecurityAuditRecord {
        security_event_id: mock_id(0xd1, "event-\u{3b5}"),
        event_timestamp_ns: -987_654_321,
        subsystem_id: mock_id(0xe1, "security subsystem"),
        artifacts: vec![
            SecurityArtifact {
                component_id: mock_id(0xf1, "artifact one"),
                associated_id: mock_id(0x81, "associated one"),
            },
            SecurityArtifact {
                component_id: mock_id(0x82, "artifact two"),
                associated_id: mock_id(0x83, "associated two"),
            },
        ],
        event,
        outcome: SecurityOutcome::Failure,
        severity: SecuritySeverity::Warning,
    }
}

/// The exact `health-rich` registration-time event sequence, in the published
/// registration order.
fn rich_events() -> Vec<HealthMetadataEvent> {
    let mut events = vec![
        HealthMetadataEvent::MfaStatus(rich_mfa_status()),
        HealthMetadataEvent::BitStatus(rich_bit_status()),
        HealthMetadataEvent::SubsystemStatus(rich_subsystem_status()),
        HealthMetadataEvent::DiscreteStatus(pairs(&[
            ("duplicate", "one"),
            ("duplicate", ""),
            ("utf8", "\u{20ac}"),
        ])),
    ];
    events.extend(
        (0..7).map(|index| HealthMetadataEvent::SecurityAudit(rich_security_record(index))),
    );
    events.push(HealthMetadataEvent::MfaStatusDetailed(pairs(&[
        ("load", "23"),
        ("load", ""),
        ("label", "\u{3bc}"),
    ])));
    assert_eq!(events.len(), RICH_EVENTS);
    events
}

/// Every `health-rich` event, in exact registration order, compared field by
/// field and collection by collection, and still valid after the stream,
/// channel, Session and provider library are gone.
#[test]
fn health_rich_delivers_all_six_families_completely_and_in_order() {
    let _guard = lock();
    let scratch = Scratch::new("rich");
    let session = open("health-rich");
    let mut health = open_health(&session);
    let mut stream = health.open_metadata(32).expect("open metadata");
    assert!(stream.is_open());
    // Registration delivered every event synchronously, before Open returned.
    assert_eq!(
        stream.counters().expect("counters"),
        counters(RICH_EVENTS as u64, 0, 0)
    );
    health.enable().expect("enable");
    let received = drain(&mut stream, RICH_EVENTS);
    let kinds: Vec<HealthMetadataKind> = received.iter().map(HealthMetadataEvent::kind).collect();
    let mut expected_kinds = vec![
        HealthMetadataKind::MfaStatus,
        HealthMetadataKind::BitStatus,
        HealthMetadataKind::SubsystemStatus,
        HealthMetadataKind::DiscreteStatus,
    ];
    expected_kinds.extend([HealthMetadataKind::SecurityAudit; 7]);
    expected_kinds.push(HealthMetadataKind::MfaStatusDetailed);
    assert_eq!(kinds, expected_kinds);
    let expected = rich_events();
    for (index, (actual, expected)) in received.iter().zip(&expected).enumerate() {
        assert_eq!(actual, expected, "event {index}");
    }
    // Every published security alternative, in order.
    let security_kinds: Vec<u32> = received[4..11]
        .iter()
        .map(|event| match event {
            HealthMetadataEvent::SecurityAudit(record) => record.event.kind().as_raw(),
            other => panic!("not a security record: {other:?}"),
        })
        .collect();
    assert_eq!(security_kinds, vec![0, 1, 2, 3, 4, 5, 6]);
    expect_kind(stream.receive(0), ErrorKind::Timeout);

    stream.close().expect("metadata close");
    health.close().expect("Health close");
    session.close().expect("session close");
    let events = scratch.events();
    assert_eq!(count(&events, "health_channel_destroyed"), 1, "{events}");
    assert!(events.ends_with(UNLOADED), "{events}");
    // The owned values outlived the stream, channel, Session and library.
    assert_eq!(received, expected);
}

/// Queue capacity 2: the first two events are kept in order, the other ten
/// are dropped and counted; an empty active queue is a Timeout for both
/// zero and finite waits.
#[test]
fn health_overflow_keeps_the_first_two_events_and_counts_drops() {
    let _guard = lock();
    let _scratch = Scratch::new("overflow");
    let session = open("health-overflow");
    let mut health = open_health(&session);
    let mut stream = health.open_metadata(2).expect("open metadata");
    let expected = counters(12, 10, 0);
    assert_eq!(stream.counters().expect("counters"), expected);
    let rich = rich_events();
    assert_eq!(stream.receive(0).expect("first"), rich[0]);
    assert_eq!(stream.receive(1_000).expect("second"), rich[1]);
    expect_kind(stream.receive(0), ErrorKind::Timeout);
    // A finite wait on an empty active queue also times out, after waiting.
    let started = Instant::now();
    expect_kind(stream.receive(50), ErrorKind::Timeout);
    assert!(started.elapsed() >= Duration::from_millis(50));
    // Receiving never changes the callback counters.
    assert_eq!(stream.counters().expect("counters"), expected);
    stream.close().expect("metadata close");
    health.close().expect("Health close");
    session.close().expect("session close");
}

/// A malformed nested value (invalid UTF-8 in the MFA state description) is
/// counted and discarded natively; the following valid event is recovered.
#[test]
fn health_malformed_counts_the_bad_record_and_recovers_the_valid_one() {
    let _guard = lock();
    let _scratch = Scratch::new("malformed");
    let session = open("health-malformed");
    let mut health = open_health(&session);
    let mut stream = health.open_metadata(4).expect("open metadata");
    assert_eq!(stream.counters().expect("counters"), counters(2, 0, 1));
    assert_eq!(
        stream.receive(0).expect("recovered event"),
        HealthMetadataEvent::MfaStatus(rich_mfa_status())
    );
    expect_kind(stream.receive(0), ErrorKind::Timeout);
    stream.close().expect("metadata close");
    health.close().expect("Health close");
    session.close().expect("session close");
}

/// A callback copy failure marks the native stream failed: the event queued
/// before it still drains, then receive reports ProviderFailed rather than a
/// timeout, stop, or a silently counted malformed record.
#[test]
fn health_allocation_failure_drains_queued_events_then_reports_provider_failed() {
    let _guard = lock();
    let _scratch = Scratch::new("allocation");
    let session = open("health-allocation");
    let mut health = open_health(&session);
    let mut stream = health.open_metadata(8).expect("open metadata");
    assert_eq!(stream.counters().expect("counters"), counters(2, 0, 0));
    assert_eq!(
        stream.receive(0).expect("queued before failure"),
        HealthMetadataEvent::MfaStatus(rich_mfa_status())
    );
    expect_kind(stream.receive(0), ErrorKind::ProviderFailed);
    expect_kind(stream.receive(10), ErrorKind::ProviderFailed);
    health.close().expect("Health close");
    expect_kind(stream.receive(0), ErrorKind::ProviderFailed);
    stream.close().expect("metadata close");
    session.close().expect("session close");
}

/// Zero capacity is rejected before any native call and does not consume the
/// channel's single registration attempt; a consumed attempt is never
/// retried. A closed channel refuses without a native call.
#[test]
fn registration_is_one_shot_per_channel_and_zero_capacity_is_not_an_attempt() {
    let _guard = lock();
    let _scratch = Scratch::new("one-shot");
    let session = open("health-rich");
    let mut health = open_health(&session);
    let error = health.open_metadata(0).expect_err("zero capacity");
    assert_eq!(error.kind(), &ErrorKind::InvalidArgument);
    let mut stream = health.open_metadata(16).expect("first real attempt");
    let error = health.open_metadata(16).expect_err("second attempt");
    assert_eq!(error.kind(), &ErrorKind::InvalidArgument);
    // Closing the stream does not authorize re-registration either.
    stream.close().expect("metadata close");
    let error = health.open_metadata(16).expect_err("after metadata close");
    assert_eq!(error.kind(), &ErrorKind::InvalidArgument);
    health.close().expect("Health close");
    let error = health.open_metadata(16).expect_err("closed channel");
    assert_eq!(error.kind(), &ErrorKind::ProviderFailed);
    assert_eq!(error.diagnostic(), Some(HEALTH_CLOSED));
    session.close().expect("session close");
}

/// Metadata Close before HealthStatus Close: public consumption ends, repeat
/// Close is a no-op, receive/counters fail without a native call, and the
/// channel stays fully usable and closes normally.
#[test]
fn metadata_close_before_health_close_leaves_the_channel_usable() {
    let _guard = lock();
    let scratch = Scratch::new("metadata-first");
    let session = open("health-rich");
    let mut health = open_health(&session);
    let mut stream = health.open_metadata(32).expect("open metadata");
    let first = stream.receive(0).expect("first event");
    stream.close().expect("metadata close");
    assert!(!stream.is_open());
    stream.close().expect("repeated metadata close");
    for error in [
        stream.receive(0).expect_err("receive after close"),
        stream.counters().expect_err("counters after close"),
    ] {
        assert_eq!(error.kind(), &ErrorKind::ProviderFailed);
        assert_eq!(error.diagnostic(), Some(STREAM_CLOSED));
    }
    // Metadata Close neither closes nor disturbs the typed channel.
    assert!(health.is_open());
    health.enable().expect("enable after metadata close");
    health
        .capabilities()
        .expect("capabilities after metadata close");
    assert_eq!(count(&scratch.events(), "health_channel_destroyed"), 0);
    health.close().expect("Health close");
    session.close().expect("session close");
    assert!(scratch.events().ends_with(UNLOADED));
    assert_eq!(first, rich_events()[0]);
}

/// HealthStatus Close before Metadata Close: already-queued events remain
/// available, then the empty stopped queue reports StreamStopped; counters
/// stay readable.
#[test]
fn health_close_before_metadata_close_drains_queued_events_then_stops() {
    let _guard = lock();
    let scratch = Scratch::new("health-first");
    let session = open("health-rich");
    let mut health = open_health(&session);
    health.enable().expect("enable");
    let mut stream = health.open_metadata(32).expect("open metadata");
    let rich = rich_events();
    assert_eq!(stream.receive(0).expect("before close"), rich[0]);
    health.close().expect("Health close");
    assert_eq!(count(&scratch.events(), "health_channel_destroyed"), 1);
    assert_eq!(drain(&mut stream, RICH_EVENTS - 1), rich[1..]);
    expect_kind(stream.receive(0), ErrorKind::StreamStopped);
    expect_kind(stream.receive(1_000), ErrorKind::StreamStopped);
    assert_eq!(
        stream.counters().expect("counters after Health close"),
        counters(RICH_EVENTS as u64, 0, 0)
    );
    assert!(stream.is_open());
    stream.close().expect("metadata close");
    session.close().expect("session close");
    assert!(scratch.events().ends_with(UNLOADED));
}

/// Session Close before HealthStatus Close: the channel and stream keep the
/// provider graph alive; the library unloads only at Health Close, and an
/// owned event received earlier survives every closure.
#[test]
fn session_close_before_health_close_keeps_owned_events_valid() {
    let _guard = lock();
    let scratch = Scratch::new("session-first");
    let session = open("health-rich");
    let mut health = open_health(&session);
    let mut stream = health.open_metadata(32).expect("open metadata");
    let rich = rich_events();
    let owned = stream.receive(0).expect("owned event");
    session.close().expect("session close");
    let events = scratch.events();
    assert_eq!(count(&events, "library_unloaded"), 0, "{events}");
    assert_eq!(stream.receive(0).expect("after session close"), rich[1]);
    health.enable().expect("enable after session close");
    health.close().expect("Health close");
    assert!(scratch.events().ends_with(UNLOADED), "{}", scratch.events());
    assert_eq!(drain(&mut stream, RICH_EVENTS - 2), rich[2..]);
    expect_kind(stream.receive(0), ErrorKind::StreamStopped);
    stream.close().expect("metadata close");
    // Stream, channel, Session and provider library are all gone.
    assert_eq!(owned, rich[0]);
    let cloned = owned.clone();
    drop(owned);
    assert_eq!(cloned, rich[0]);
}

/// Dropping the stream (no explicit Close) and dropping the channel while a
/// stream is alive are both non-panicking and leave the other owner usable.
#[test]
fn drop_without_close_releases_owners_independently() {
    let _guard = lock();
    let scratch = Scratch::new("drop");
    let session = open("health-rich");
    let mut health = open_health(&session);
    let stream = health.open_metadata(32).expect("open metadata");
    drop(stream);
    health
        .capabilities()
        .expect("capabilities after stream drop");
    health.close().expect("Health close");

    let mut other = open_health(&session);
    let mut stream = other.open_metadata(32).expect("second channel metadata");
    drop(other); // Best-effort native Health Close.
    assert_eq!(count(&scratch.events(), "health_channel_destroyed"), 2);
    assert_eq!(drain(&mut stream, RICH_EVENTS), rich_events());
    expect_kind(stream.receive(0), ErrorKind::StreamStopped);
    drop(stream);
    session.close().expect("session close");
    assert!(scratch.events().ends_with(UNLOADED));
}

/// Registration is valid while Attached and while Enabled; each is shown on a
/// separate channel instance, with identical complete content.
#[test]
fn registration_while_attached_and_while_enabled_on_separate_channels() {
    let _guard = lock();
    let _scratch = Scratch::new("attached-enabled");
    let session = open("health-rich");
    let mut attached = open_health(&session);
    let mut attached_stream = attached.open_metadata(32).expect("metadata while Attached");
    let mut enabled = open_health(&session);
    enabled.enable().expect("enable");
    let mut enabled_stream = enabled.open_metadata(32).expect("metadata while Enabled");
    let rich = rich_events();
    assert_eq!(drain(&mut attached_stream, RICH_EVENTS), rich);
    assert_eq!(drain(&mut enabled_stream, RICH_EVENTS), rich);
    for stream in [&mut attached_stream, &mut enabled_stream] {
        expect_kind(stream.receive(0), ErrorKind::Timeout);
        stream.close().expect("metadata close");
    }
    attached.close().expect("Attached close");
    enabled.close().expect("Enabled close");
    session.close().expect("session close");
}

/// Ordinary HealthStatus metadata registration is separate from the optional
/// NUC_TempData registration: on a NUC-capable mock it registers exactly the
/// six families and never triggers the NUC registration.
#[test]
fn health_metadata_registration_never_registers_nuc() {
    let _guard = lock();
    let scratch = Scratch::new("nuc-independent");
    let session = open("nuc-rich");
    let mut health = open_health(&session);
    let mut stream = health.open_metadata(4).expect("open metadata");
    assert_eq!(stream.counters().expect("counters"), counters(0, 0, 0));
    expect_kind(stream.receive(0), ErrorKind::Timeout);
    stream.close().expect("metadata close");
    health.close().expect("Health close");
    session.close().expect("session close");
    let events = scratch.events();
    assert_eq!(count(&events, "nuc_registration_attempted"), 0, "{events}");
    assert!(events.ends_with(UNLOADED), "{events}");
}

// ---- Isolated child processes (process-global retained/barrier state) ----

fn run_child(case: &str) {
    let output = Command::new(env::current_exe().expect("test executable"))
        .args([
            "--exact",
            "child_entry",
            "--ignored",
            "--nocapture",
            "--test-threads=1",
        ])
        .env(CHILD_ENV, case)
        .env_remove("AMS_MEL_TEST_LIFETIME_LOG")
        .env_remove(CHILD_BARRIER_ENV)
        .output()
        .expect("spawn child");
    let stdout = String::from_utf8_lossy(&output.stdout);
    let stderr = String::from_utf8_lossy(&output.stderr);
    assert!(
        output.status.success(),
        "child {case} failed\nstdout:\n{stdout}\nstderr:\n{stderr}"
    );
    assert!(
        stdout.contains(&format!("{CHILD_DONE} {case}")),
        "child {case} did not run\nstdout:\n{stdout}\nstderr:\n{stderr}"
    );
}

/// Partial registration (the third family fails): Open reports
/// ProviderFailed and publishes no stream; the consumed attempt is not
/// retried; the retained, inactive callback state is safely invoked by the
/// provider during channel destruction, before the graph is unloaded.
#[test]
fn partial_registration_publishes_no_stream_and_is_never_retried() {
    let _guard = lock();
    run_child("partial");
}

/// The provider's disable returns while a metadata callback is still running.
/// Native channel cleanup waits for that callback: the channel is destroyed
/// and the library unloaded only after the callback returned. Ordering comes
/// from the mock's file barrier and lifetime log, not from sleeping.
#[test]
fn health_close_waits_for_an_in_flight_metadata_callback() {
    let _guard = lock();
    run_child("quiescence");
}

/// Child-process entry for the tests above; it does nothing unless selected.
#[test]
#[ignore = "child-process entry, run only by the isolated-process tests"]
fn child_entry() {
    let Some(case) = env::var_os(CHILD_ENV) else {
        return;
    };
    let case = case.into_string().expect("child case");
    match case.as_str() {
        "partial" => partial_child(),
        "quiescence" => quiescence_child(),
        other => panic!("unknown child case {other}"),
    }
    println!("{CHILD_DONE} {case}");
}

fn partial_child() {
    let scratch = Scratch::new("partial");
    let session = open("health-register-partial");
    let mut health = open_health(&session);
    let error = health.open_metadata(8).expect_err("partial registration");
    assert_eq!(error.kind(), &ErrorKind::ProviderFailed, "{error}");
    assert_eq!(
        error.diagnostic(),
        Some("Health metadata callback registration failed")
    );
    // The consumed attempt is never retried, even with another capacity.
    let error = health.open_metadata(4).expect_err("no second attempt");
    assert_eq!(error.kind(), &ErrorKind::InvalidArgument, "{error}");
    health.enable().expect("channel still usable");
    health.close().expect("Health close");
    session.close().expect("session close");
    assert_order(
        &scratch.events(),
        &[
            "retained_health_callback_invoked",
            "health_channel_destroyed",
            "control_destroyed",
            "manager_destroyed",
            "library_unloaded",
        ],
    );
}

fn quiescence_child() {
    let scratch = Scratch::new("quiescence");
    let barrier = scratch.path("barrier");
    env::set_var(CHILD_BARRIER_ENV, &barrier);
    let session = open("health-metadata-nonquiescing-disable");
    let mut health = open_health(&session);
    let mut stream = health.open_metadata(8).expect("open metadata");
    health.enable().expect("enable");
    // Release the mock's producer thread into the callback, whose barrier
    // keeps it running until the channel destructor releases it.
    fs::write(barrier.with_extension("start"), "start\n").expect("start barrier");
    health.close().expect("Health close");
    // The callback completed before Close returned.
    let events = scratch.events();
    assert_order(
        &events,
        &[
            "health_callback_entered",
            "disable_returned_with_health_callback_active",
            "health_callback_returned",
            "health_channel_destroyed",
        ],
    );
    // Cleanup made consumption inactive before disabling, so the late
    // callback entry is counted but not enqueued, and the drained, stopped
    // queue reports StreamStopped.
    assert_eq!(stream.counters().expect("counters"), counters(1, 0, 0));
    expect_kind(stream.receive(0), ErrorKind::StreamStopped);
    stream.close().expect("metadata close");
    session.close().expect("session close");
    assert_order(
        &scratch.events(),
        &[
            "health_callback_returned",
            "health_channel_destroyed",
            "library_unloaded",
        ],
    );
    env::remove_var(CHILD_BARRIER_ENV);
}
