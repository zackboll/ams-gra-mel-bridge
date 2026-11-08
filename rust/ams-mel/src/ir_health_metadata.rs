//! Safe IR HealthStatus metadata reception (RUST-IR-002) over exactly the six
//! existing C exports `ams_mel_ir_health_metadata_open`, `_receive`,
//! `_get_counters`, `_close`, `_event_view`, and `_event_close`. No C ABI is
//! added or changed, and no Rust callback, thread, mutex, or queue is added:
//! the native facade owns every provider callback and the bounded queue, and
//! this module only polls it.
//!
//! Each received native event is held by a private RAII guard. Only its active
//! payload is validated and deeply copied into a Rust-owned
//! [`HealthMetadataEvent`]; the guard then closes the native event, and closes
//! it on every error or unwind path as well. Returned values contain no
//! native pointers.
//!
//! Conversion helpers below read native spans through the crate's
//! `checked_span`. They are called only on records borrowed from a live
//! native event owner (or, in unit tests, on fabricated records whose storage
//! outlives the call); the C facade is the trusted memory-provenance boundary.

use std::ffi::c_char;
use std::ptr;
use std::rc::Rc;

use ams_mel_sys as sys;

use crate::{
    call_with_diagnostic, checked_bool, checked_span, copy_mapped, owned_text, owned_uci_id,
    protocol, ComponentLocation, Error, ErrorKind, RfMfaState, UciId,
};

const CLOSED: &str = "IR Health metadata stream is closed";

/// Declares a fixed, reviewed native `uint32_t` domain as a typed enum whose
/// discriminants are exactly the native values. Decoding is checked: an
/// unpublished raw value is a `ProtocolInconsistency`, never another variant.
macro_rules! native_enum {
    (
        $(#[$meta:meta])*
        $name:ident {
            $($variant:ident = $value:expr),+ $(,)?
        }
    ) => {
        $(#[$meta])*
        #[derive(Clone, Copy, Debug, Eq, PartialEq, Hash)]
        #[repr(u32)]
        pub enum $name {
            $($variant = $value),+
        }

        impl $name {
            /// Every published value, in ascending raw-value order.
            pub const ALL: &'static [$name] = &[$(Self::$variant),+];

            /// Returns the exact native raw value.
            pub const fn as_raw(self) -> u32 {
                self as u32
            }

            fn decode(raw: u32, field: &str) -> Result<Self, Error> {
                Self::ALL
                    .iter()
                    .copied()
                    .find(|value| value.as_raw() == raw)
                    .ok_or_else(|| {
                        protocol(&format!("native {field} has unpublished value {raw}"))
                    })
            }
        }

        impl From<$name> for u32 {
            fn from(value: $name) -> Self {
                value.as_raw()
            }
        }
    };
}

native_enum! {
    /// Native HealthStatus metadata family (`ams_mel_ir_health_metadata_kind_t`)
    /// in the published registration order. Discriminants are exactly 1..=6.
    HealthMetadataKind {
        MfaStatus = sys::AMS_MEL_IR_HEALTH_METADATA_MFA_STATUS,
        BitStatus = sys::AMS_MEL_IR_HEALTH_METADATA_BIT_STATUS,
        SubsystemStatus = sys::AMS_MEL_IR_HEALTH_METADATA_SUBSYSTEM_STATUS,
        DiscreteStatus = sys::AMS_MEL_IR_HEALTH_METADATA_DISCRETE_STATUS,
        SecurityAudit = sys::AMS_MEL_IR_HEALTH_METADATA_SECURITY_AUDIT,
        MfaStatusDetailed = sys::AMS_MEL_IR_HEALTH_METADATA_MFA_STATUS_DETAILED,
    }
}

native_enum! {
    /// Common MEL `StateTransitionStatus` (`ams_mel_state_transition_status_t`).
    StateTransitionStatus {
        NotSet = sys::AMS_MEL_STATE_TRANSITION_NOT_SET,
        NotTransitioning = sys::AMS_MEL_STATE_TRANSITION_NOT_TRANSITIONING,
        ShuttingDown = sys::AMS_MEL_STATE_TRANSITION_SHUTTING_DOWN,
        Transitioning = sys::AMS_MEL_STATE_TRANSITION_TRANSITIONING,
    }
}

native_enum! {
    /// Common MEL `ComponentState` (`ams_mel_component_state_t`).
    ComponentState {
        NotSet = sys::AMS_MEL_COMPONENT_STATE_NOT_SET,
        Unknown = sys::AMS_MEL_COMPONENT_STATE_UNKNOWN,
        NotInstalled = sys::AMS_MEL_COMPONENT_STATE_NOT_INSTALLED,
        Off = sys::AMS_MEL_COMPONENT_STATE_OFF,
        Initializing = sys::AMS_MEL_COMPONENT_STATE_INITIALIZING,
        Operational = sys::AMS_MEL_COMPONENT_STATE_OPERATIONAL,
        Degraded = sys::AMS_MEL_COMPONENT_STATE_DEGRADED,
        Disabled = sys::AMS_MEL_COMPONENT_STATE_DISABLED,
        Faulted = sys::AMS_MEL_COMPONENT_STATE_FAULTED,
    }
}

native_enum! {
    /// Common MEL `TemperatureState` (`ams_mel_temperature_state_t`).
    TemperatureState {
        NotSet = sys::AMS_MEL_TEMPERATURE_STATE_NOT_SET,
        UnderTemp = sys::AMS_MEL_TEMPERATURE_STATE_UNDER_TEMP,
        Normal = sys::AMS_MEL_TEMPERATURE_STATE_NORMAL,
        OverTempWarning = sys::AMS_MEL_TEMPERATURE_STATE_OVER_TEMP_WARNING,
        OverTempDegraded = sys::AMS_MEL_TEMPERATURE_STATE_OVER_TEMP_DEGRADED,
        OverTempShutdown = sys::AMS_MEL_TEMPERATURE_STATE_OVER_TEMP_SHUTDOWN,
    }
}

native_enum! {
    /// Common MEL `BIT_Result` (`ams_mel_bit_result_t`).
    BitResult {
        NotSet = sys::AMS_MEL_BIT_RESULT_NOT_SET,
        Pass = sys::AMS_MEL_BIT_RESULT_PASS,
        Fail = sys::AMS_MEL_BIT_RESULT_FAIL,
        Interrupted = sys::AMS_MEL_BIT_RESULT_INTERRUPTED,
        NotTested = sys::AMS_MEL_BIT_RESULT_NOT_TESTED,
    }
}

native_enum! {
    /// Common MEL `FaultSeverity` (`ams_mel_fault_severity_t`).
    FaultSeverity {
        NotSet = sys::AMS_MEL_FAULT_SEVERITY_NOT_SET,
        Nominal = sys::AMS_MEL_FAULT_SEVERITY_NOMINAL,
        Caution = sys::AMS_MEL_FAULT_SEVERITY_CAUTION,
        Warning = sys::AMS_MEL_FAULT_SEVERITY_WARNING,
        Failed = sys::AMS_MEL_FAULT_SEVERITY_FAILED,
    }
}

native_enum! {
    /// Common MEL `FaultState` (`ams_mel_fault_state_t`).
    FaultState {
        NotSet = sys::AMS_MEL_FAULT_STATE_NOT_SET,
        Set = sys::AMS_MEL_FAULT_STATE_SET,
        Cleared = sys::AMS_MEL_FAULT_STATE_CLEARED,
        Unknown = sys::AMS_MEL_FAULT_STATE_UNKNOWN,
    }
}

native_enum! {
    /// IR MEL subsystem `Failure` category (`ams_mel_ir_failure_t`): a reported
    /// category, not a numeric severity scale or bitmask.
    FailureLevel {
        Na = sys::AMS_MEL_IR_FAILURE_NA,
        Critical = sys::AMS_MEL_IR_FAILURE_CRITICAL,
        Major = sys::AMS_MEL_IR_FAILURE_MAJOR,
        Parametric = sys::AMS_MEL_IR_FAILURE_PARAMETRIC,
        Informational = sys::AMS_MEL_IR_FAILURE_INFORMATIONAL,
        Available = sys::AMS_MEL_IR_FAILURE_AVAILABLE,
        NotPresent = sys::AMS_MEL_IR_FAILURE_NOT_PRESENT,
    }
}

native_enum! {
    /// IR MEL `CSCIMode` (`ams_mel_ir_csci_mode_t`).
    CsciMode {
        Unknown = sys::AMS_MEL_IR_CSCI_MODE_UNKNOWN,
        Unused = sys::AMS_MEL_IR_CSCI_MODE_UNUSED,
        Initialization = sys::AMS_MEL_IR_CSCI_MODE_INITIALIZATION,
        Maintenance = sys::AMS_MEL_IR_CSCI_MODE_MAINTENANCE,
        Idle = sys::AMS_MEL_IR_CSCI_MODE_IDLE,
        Operational = sys::AMS_MEL_IR_CSCI_MODE_OPERATIONAL,
        Vsa = sys::AMS_MEL_IR_CSCI_MODE_VSA,
        QuickLook = sys::AMS_MEL_IR_CSCI_MODE_QUICK_LOOK,
        Track = sys::AMS_MEL_IR_CSCI_MODE_TRACK,
        Imaging = sys::AMS_MEL_IR_CSCI_MODE_IMAGING,
        Noise = sys::AMS_MEL_IR_CSCI_MODE_NOISE,
    }
}

native_enum! {
    /// `MFA_SecurityAuditRecord::OutcomeEnum` (`ams_mel_security_outcome_t`).
    SecurityOutcome {
        NotSet = sys::AMS_MEL_SECURITY_OUTCOME_NOT_SET,
        Failure = sys::AMS_MEL_SECURITY_OUTCOME_FAILURE,
        Success = sys::AMS_MEL_SECURITY_OUTCOME_SUCCESS,
    }
}

native_enum! {
    /// `MFA_SecurityAuditRecord::SeverityEnum` (`ams_mel_security_severity_t`).
    SecuritySeverity {
        NotSet = sys::AMS_MEL_SECURITY_SEVERITY_NOT_SET,
        Critical = sys::AMS_MEL_SECURITY_SEVERITY_CRITICAL,
        Error = sys::AMS_MEL_SECURITY_SEVERITY_ERROR,
        Informational = sys::AMS_MEL_SECURITY_SEVERITY_INFORMATIONAL,
        Warning = sys::AMS_MEL_SECURITY_SEVERITY_WARNING,
    }
}

// Security categories are kind-specific `ams_mel_security_category_t` numbers.
// The C ABI names no constants for them; the raw values are the ordinals of
// the pinned upstream enums in
// `native/vendor/common-mel/include/mel/library/securityauditrecord/`, which
// the native facade range-checks per kind before it publishes an event.

native_enum! {
    /// `SecurityAuthenticationType::SecurityAuthenticationEnum`.
    SecurityAuthenticationCategory {
        NotSet = 0,
        SubsystemAuthentication = 1,
        ServiceAuthentication = 2,
        ConfigFileAuthentication = 3,
        OfpAuthentication = 4,
        MdfAuthentication = 5,
        OtherFileAuthentication = 6,
        OtherDeviceAuthentication = 7,
    }
}

native_enum! {
    /// `SecurityIntegrityType::SecurityIntegrityEnum`.
    SecurityIntegrityCategory {
        NotSet = 0,
        ConfigFileIntegrityCheck = 1,
        BootloaderIntegrityCheck = 2,
        OfpIntegrityCheck = 3,
        MdfIntegrityCheck = 4,
        OtherFileIntegrityCheck = 5,
        OtherIntegrityCheck = 6,
    }
}

native_enum! {
    /// `SecurityFileManagementType::SecurityFileManagementEnum`.
    SecurityFileManagementCategory {
        NotSet = 0,
        ConfigFileUpdate = 1,
        OfpUpdate = 2,
        MdfUpdate = 3,
        OtherFileUpdate = 4,
    }
}

native_enum! {
    /// `SecurityKeyManagementType::SecurityKeyManagementEnum`.
    SecurityKeyManagementCategory {
        NotSet = 0,
        KeyLoad = 1,
        KeyGeneration = 2,
        KeyUpdate = 3,
        KeyDeletion = 4,
        CertificateLoad = 5,
        CertificateDeletion = 6,
    }
}

native_enum! {
    /// `SecuritySystemType::SecuritySystemEnum`.
    SecuritySystemCategory {
        NotSet = 0,
        Startup = 1,
        Shutdown = 2,
        Reset = 3,
        Fault = 4,
    }
}

native_enum! {
    /// `SecuritySanitizationType::SecuritySanitizationEnum`.
    SecuritySanitizationCategory {
        NotSet = 0,
        Zeroization = 1,
        Sanitization = 2,
        ClassifiedDataErase = 3,
    }
}

/// Roll/pitch/yaw triple (`ams_mel_euler_v1`), copied bit-for-bit.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Euler {
    pub roll: f64,
    pub pitch: f64,
    pub yaw: f64,
}

/// Common MEL `ForeignKey` (`ams_mel_foreign_key_v1`).
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ForeignKey {
    pub key: String,
    pub system_name: String,
}

/// Component installation (`ams_mel_installation_details_v1`).
#[derive(Clone, Debug, PartialEq)]
pub struct InstallationDetails {
    pub location: ComponentLocation,
    pub orientation: Euler,
    pub boresight: Euler,
}

/// Component temperature (`ams_mel_temperature_status_v1`).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct TemperatureStatus {
    /// Degrees Celsius exactly as reported; special values are not normalized.
    pub temperature_c: f64,
    pub state: TemperatureState,
}

/// One MFA component (`ams_mel_mfa_component_v1`).
#[derive(Clone, Debug, PartialEq)]
pub struct MfaComponent {
    pub component_id: UciId,
    pub state: ComponentState,
    pub temperature: TemperatureStatus,
    pub installation_location_id: ForeignKey,
    pub installation_details: InstallationDetails,
}

/// MFA `About` identification (`ams_mel_about_v1`).
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct About {
    pub model: String,
    pub serial_number: String,
    pub software_version: String,
    pub bootloader_software_version: String,
    pub hardware_version: String,
}

/// Complete `MFA_Status` record (`ams_mel_mfa_status_v1`).
///
/// `state` reuses [`RfMfaState`], the crate's type for the single published
/// Common MEL `MFA_State` domain (`ams_mel_rf_mfa_state_t` is a typedef of
/// `ams_mel_ir_mfa_state_t`), shared by RF Admin and IR MEL.
#[derive(Clone, Debug, PartialEq)]
pub struct MfaStatus {
    pub state: RfMfaState,
    pub state_description: String,
    pub mode_description: String,
    pub transition_status: StateTransitionStatus,
    pub about: About,
    /// Components in provider order.
    pub components: Vec<MfaComponent>,
}

/// One active BIT (`ams_mel_active_bit_v1`).
#[derive(Clone, Debug, PartialEq)]
pub struct ActiveBit {
    pub bit_id: UciId,
    /// Signed estimated completion time count in nanoseconds.
    pub estimated_completion_time_ns: i64,
    pub estimated_percent_complete: f64,
}

/// One item of a completed BIT (`ams_mel_completed_bit_item_v1`).
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CompletedBitItem {
    pub bit_item_name: String,
    pub result: BitResult,
    pub fail_reason: String,
}

/// One completed BIT (`ams_mel_completed_bit_v1`).
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CompletedBit {
    pub bit_id: UciId,
    /// Signed time tag in nanoseconds.
    pub time_tag_ns: i64,
    pub result: BitResult,
    pub fail_reason: String,
    /// Nested BIT items in provider order.
    pub bit_items: Vec<CompletedBitItem>,
}

/// One fault data entry (`ams_mel_fault_data_v1`).
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct FaultData {
    pub key: String,
    pub value: String,
    pub format: String,
    pub units: String,
}

/// One fault ambiguity group (`ams_mel_fault_ambiguity_group_v1`).
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct FaultAmbiguityGroup {
    pub diagnostic_test_ids: Vec<UciId>,
    pub component_ids: Vec<UciId>,
}

/// One fault (`ams_mel_fault_v1`).
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Fault {
    pub fault_id: UciId,
    pub severity: FaultSeverity,
    pub state: FaultState,
    pub fault_data: Vec<FaultData>,
    /// Signed detection time in nanoseconds.
    pub detection_time_ns: i64,
    pub fault_code: String,
    pub fault_description: String,
    /// Implicated component IDs in provider order.
    pub component_ids: Vec<UciId>,
    pub ambiguity_groups: Vec<FaultAmbiguityGroup>,
}

/// Complete `BIT_Status` record (`ams_mel_bit_status_v1`). Every collection
/// keeps provider order and duplicates.
#[derive(Clone, Debug, PartialEq)]
pub struct BitStatus {
    pub active_bits: Vec<ActiveBit>,
    pub completed_bits: Vec<CompletedBit>,
    pub faults: Vec<Fault>,
}

/// One dependent subsystem (`ams_mel_ir_subsystem_dep_info_v1`).
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct SubsystemDependency {
    pub subsystem_id: u32,
    pub criticality: u32,
    pub failure: FailureLevel,
}

/// Four-part CSCI version (`ams_mel_ir_version_v1`).
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct CsciVersion {
    pub source: u32,
    pub major_revision: u32,
    pub minor_revision: u32,
    pub engineering_revision: u32,
}

/// One CSCI record (`ams_mel_ir_subsystem_csci_info_v1`).
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CsciInfo {
    pub csci: String,
    pub mode: CsciMode,
    pub version: CsciVersion,
    pub criticality: u32,
    pub failure: FailureLevel,
    /// Provider BIT report counter, copied as the full `u32`.
    pub bit_report: u32,
    pub connection_established: bool,
}

/// Complete `SubsystemStatusResp` record (`ams_mel_ir_subsystem_status_v1`).
///
/// `subsystem_count` and `csci_count` are the provider-reported counts. They
/// are independent of, and never replaced by, the lengths of the copied
/// `subsystems` and `csci` collections.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct SubsystemStatus {
    pub subsystem_id: u32,
    pub criticality: u32,
    pub status_sequence_number: u32,
    pub failure: FailureLevel,
    pub subsystem_count: u32,
    pub subsystems: Vec<SubsystemDependency>,
    pub csci_count: u32,
    pub csci: Vec<CsciInfo>,
}

/// One ordered name/value pair (`ams_mel_name_value_pair_v1`). Discrete Status
/// and MFA Status Detailed keep pairs as ordered sequences, never as maps, so
/// duplicate and empty names survive.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct NameValuePair {
    pub name: String,
    pub value: String,
}

/// One security artifact (`ams_mel_security_artifact_v1`).
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct SecurityArtifact {
    pub component_id: UciId,
    pub associated_id: UciId,
}

native_enum! {
    /// Selected `MFA_SecurityAuditRecord::SecurityEventType` alternative
    /// (`ams_mel_security_event_kind_t`), including the empty `None`
    /// (`std::monostate`) alternative.
    SecurityEventKind {
        None = sys::AMS_MEL_SECURITY_EVENT_NONE,
        Authentication = sys::AMS_MEL_SECURITY_EVENT_AUTHENTICATION,
        Integrity = sys::AMS_MEL_SECURITY_EVENT_INTEGRITY,
        FileManagement = sys::AMS_MEL_SECURITY_EVENT_FILE_MANAGEMENT,
        KeyManagement = sys::AMS_MEL_SECURITY_EVENT_KEY_MANAGEMENT,
        System = sys::AMS_MEL_SECURITY_EVENT_SYSTEM,
        Sanitization = sys::AMS_MEL_SECURITY_EVENT_SANITIZATION,
    }
}

/// The published security-event alternative with exactly its own subtype
/// fields and typed, kind-specific category.
///
/// The native record is flattened: identifiers that do not belong to the
/// selected alternative are left unset (all-zero UUID, empty label) and `None`
/// carries no category or details. Conversion verifies those inapplicable
/// fields are unset, so representing each alternative only by its own fields
/// never discards provider data; a set inapplicable field is rejected as
/// [`ErrorKind::ProtocolInconsistency`].
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum SecurityEvent {
    /// Upstream `std::monostate`: no security-event subtype.
    None,
    Authentication {
        category: SecurityAuthenticationCategory,
        details: String,
        subsystem_id: UciId,
        service_id: UciId,
        mdf_id: UciId,
    },
    Integrity {
        category: SecurityIntegrityCategory,
        details: String,
        mdf_id: UciId,
    },
    FileManagement {
        category: SecurityFileManagementCategory,
        details: String,
        mdf_id: UciId,
    },
    KeyManagement {
        category: SecurityKeyManagementCategory,
        details: String,
    },
    System {
        category: SecuritySystemCategory,
        details: String,
    },
    Sanitization {
        category: SecuritySanitizationCategory,
        details: String,
    },
}

impl SecurityEvent {
    /// The selected alternative's native kind.
    pub fn kind(&self) -> SecurityEventKind {
        match self {
            Self::None => SecurityEventKind::None,
            Self::Authentication { .. } => SecurityEventKind::Authentication,
            Self::Integrity { .. } => SecurityEventKind::Integrity,
            Self::FileManagement { .. } => SecurityEventKind::FileManagement,
            Self::KeyManagement { .. } => SecurityEventKind::KeyManagement,
            Self::System { .. } => SecurityEventKind::System,
            Self::Sanitization { .. } => SecurityEventKind::Sanitization,
        }
    }
}

/// Complete `MFA_SecurityAuditRecord` (`ams_mel_security_audit_record_v1`).
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct SecurityAuditRecord {
    pub security_event_id: UciId,
    /// Signed event timestamp in nanoseconds.
    pub event_timestamp_ns: i64,
    pub subsystem_id: UciId,
    /// Security artifacts in provider order.
    pub artifacts: Vec<SecurityArtifact>,
    pub event: SecurityEvent,
    pub outcome: SecurityOutcome,
    pub severity: SecuritySeverity,
}

/// One fully Rust-owned HealthStatus metadata event.
///
/// Each alternative represents the complete corresponding native record and
/// contains no native pointer or borrowed memory, so it stays valid after the
/// metadata stream, the HealthStatus channel, the Session, and the provider
/// library are closed. Discrete Status and MFA Status Detailed are distinct
/// alternatives of ordered pairs, deliberately not maps.
#[derive(Clone, Debug, PartialEq)]
pub enum HealthMetadataEvent {
    MfaStatus(MfaStatus),
    BitStatus(BitStatus),
    SubsystemStatus(SubsystemStatus),
    DiscreteStatus(Vec<NameValuePair>),
    SecurityAudit(SecurityAuditRecord),
    MfaStatusDetailed(Vec<NameValuePair>),
}

impl HealthMetadataEvent {
    /// The exact native discriminant family (1..=6) of this event.
    pub fn kind(&self) -> HealthMetadataKind {
        match self {
            Self::MfaStatus(_) => HealthMetadataKind::MfaStatus,
            Self::BitStatus(_) => HealthMetadataKind::BitStatus,
            Self::SubsystemStatus(_) => HealthMetadataKind::SubsystemStatus,
            Self::DiscreteStatus(_) => HealthMetadataKind::DiscreteStatus,
            Self::SecurityAudit(_) => HealthMetadataKind::SecurityAudit,
            Self::MfaStatusDetailed(_) => HealthMetadataKind::MfaStatusDetailed,
        }
    }
}

/// Saturating native callback counters across all six families. They count
/// provider callback entries and their dispositions, not Rust receives.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct HealthMetadataCounters {
    /// Callback entries observed, including entries after public consumption
    /// became inactive.
    pub events_received: u64,
    /// Valid copied events dropped because the bounded queue was full; queued
    /// events are never evicted.
    pub events_dropped_queue_full: u64,
    /// Null, unknown-enum, invalid-text or otherwise unsupported records
    /// rejected by native conversion. A callback exception instead marks the
    /// stream failed.
    pub malformed_or_unsupported: u64,
}

/// Uniquely owned public consumer of one HealthStatus channel's native
/// metadata queue (`ams_mel_ir_health_metadata *`), created by
/// [`HealthStatusChannel::open_metadata`](crate::HealthStatusChannel::open_metadata).
///
/// Native callback lifecycle, followed exactly:
///
/// * Open registers all six callback families in the published order (MFA
///   Status, BIT Status, Subsystem Status, Discrete Status, Security Audit,
///   MFA Status Detailed). Registration can deliver events synchronously
///   before Open returns. Only one registration attempt is permitted per
///   channel, including a failed or partial one; the facade refuses every
///   later attempt and this crate never retries.
/// * Callback state stays retained by the provider channel, because upstream
///   has no unregister operation. [`close`](Self::close) only ends public
///   consumption: it neither unregisters callbacks nor closes the channel.
/// * Closing the HealthStatus channel stops further enqueueing; events that
///   were already queued remain available, after which receive reports
///   [`ErrorKind::StreamStopped`]. Native channel cleanup waits for in-flight
///   callbacks before the provider graph is released.
///
/// The stream does not borrow its channel or Session and may outlive both.
/// All six families share one bounded drop-incoming queue owned by the native
/// facade; no Rust callback, thread, or additional queue exists.
///
/// The owner is neither `Send` nor `Sync`, and receive, counters and Close
/// take `&mut self`, so operations on one owner cannot overlap.
///
/// ```compile_fail
/// fn require_send<T: Send>() {}
/// require_send::<ams_mel::HealthMetadataStream>();
/// ```
///
/// ```compile_fail
/// fn require_sync<T: Sync>() {}
/// require_sync::<ams_mel::HealthMetadataStream>();
/// ```
#[derive(Debug)]
pub struct HealthMetadataStream {
    raw: *mut sys::AmsMelIrHealthMetadata,
    _not_send_sync: Rc<()>,
}

impl HealthMetadataStream {
    /// Performs the single native registration attempt on a live Health owner.
    pub(crate) fn open(
        health: *mut sys::AmsMelIrHealth,
        queue_capacity: usize,
    ) -> Result<Self, Error> {
        if queue_capacity == 0 {
            // Rejected before any native call, so the channel's single
            // registration attempt is not consumed.
            return Err(Error::new(
                ErrorKind::InvalidArgument,
                "Health metadata queue capacity must be nonzero",
            ));
        }
        let mut raw = ptr::null_mut();
        let result = call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: `health` is a live Health owner exclusively borrowed by
            // the caller for this call; `raw` is an initially null owner slot.
            unsafe {
                sys::ams_mel_ir_health_metadata_open(
                    health,
                    queue_capacity,
                    &mut raw,
                    diagnostic,
                    capacity,
                    required,
                )
            }
        });
        if let Err(error) = result {
            if !raw.is_null() {
                best_effort_close_metadata(&mut raw);
            }
            return Err(error);
        }
        if raw.is_null() {
            return Err(protocol(
                "Health metadata open succeeded without a metadata owner",
            ));
        }
        Ok(Self {
            raw,
            _not_send_sync: Rc::new(()),
        })
    }

    /// Returns whether this wrapper still owns its native metadata handle.
    /// This is local handle ownership only, not callback or provider state.
    pub fn is_open(&self) -> bool {
        !self.raw.is_null()
    }

    /// Dequeues one event, deeply copies its active payload into a Rust-owned
    /// [`HealthMetadataEvent`], and closes the native event before returning.
    ///
    /// A zero `timeout_ms` polls. Native outcomes are preserved:
    /// [`ErrorKind::Timeout`] while consumption is active and the queue stays
    /// empty, [`ErrorKind::StreamStopped`] once the queue is empty and
    /// inactive or stopped, and [`ErrorKind::ProviderFailed`] once it is empty
    /// after a callback failure. Queued events drain before either terminal
    /// status. A record that cannot be represented exactly is rejected (for
    /// example [`ErrorKind::ProtocolInconsistency`] or
    /// [`ErrorKind::InvalidUtf8`]) and never partially returned; its native
    /// event is still released. A closed owner returns
    /// [`ErrorKind::ProviderFailed`] without a native call.
    pub fn receive(&mut self, timeout_ms: u32) -> Result<HealthMetadataEvent, Error> {
        let metadata = self.live()?;
        let mut event = NativeEvent::new(&NATIVE_EVENT_OPS);
        call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: `metadata` is this wrapper's live unique owner, used
            // exclusively through `&mut self`; `event.raw` is a null slot that
            // the guard releases on every later path.
            unsafe {
                sys::ams_mel_ir_health_metadata_receive(
                    metadata,
                    timeout_ms,
                    &mut event.raw,
                    diagnostic,
                    capacity,
                    required,
                )
            }
        })?;
        event.into_owned()
    }

    /// Copies the current saturating native callback counters. They remain
    /// readable after the HealthStatus channel is closed. A closed owner
    /// returns [`ErrorKind::ProviderFailed`] without a native call.
    pub fn counters(&mut self) -> Result<HealthMetadataCounters, Error> {
        let metadata = self.live()?;
        let mut raw = sys::AmsMelIrC2MetadataCountersV1::default();
        call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: `metadata` is this wrapper's live owner and `raw` is
            // writable storage for the exact counters record.
            unsafe {
                sys::ams_mel_ir_health_metadata_get_counters(
                    metadata, &mut raw, diagnostic, capacity, required,
                )
            }
        })?;
        Ok(HealthMetadataCounters {
            events_received: raw.events_received,
            events_dropped_queue_full: raw.events_dropped_queue_full,
            malformed_or_unsupported: raw.malformed_or_unsupported,
        })
    }

    /// Ends public consumption and releases the native owner, waking any
    /// native waiter. It does not unregister provider callbacks or close the
    /// HealthStatus channel. Repeated Close returns `Ok(())` without a native
    /// call.
    pub fn close(&mut self) -> Result<(), Error> {
        if self.raw.is_null() {
            return Ok(());
        }
        call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: `raw` is this wrapper's unique owner; native Close
            // clears the slot.
            unsafe {
                sys::ams_mel_ir_health_metadata_close(&mut self.raw, diagnostic, capacity, required)
            }
        })?;
        if !self.raw.is_null() {
            return Err(protocol(
                "Health metadata close succeeded without releasing the owner",
            ));
        }
        Ok(())
    }

    fn live(&self) -> Result<*mut sys::AmsMelIrHealthMetadata, Error> {
        if self.raw.is_null() {
            Err(Error::new(ErrorKind::ProviderFailed, CLOSED))
        } else {
            Ok(self.raw)
        }
    }
}

impl Drop for HealthMetadataStream {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            best_effort_close_metadata(&mut self.raw);
        }
    }
}

fn best_effort_close_metadata(raw: &mut *mut sys::AmsMelIrHealthMetadata) {
    // SAFETY: called only for a unique metadata owner; null diagnostics are
    // supported. The result is ignored because Drop cannot report it.
    let _ =
        unsafe { sys::ams_mel_ir_health_metadata_close(raw, ptr::null_mut(), 0, ptr::null_mut()) };
}

/// The two native event operations used by [`NativeEvent`]. Production code
/// uses only [`NATIVE_EVENT_OPS`]; unit tests substitute fakes to observe the
/// guard's cleanup without a provider.
struct NativeEventOps {
    view: unsafe extern "C" fn(
        *const sys::AmsMelIrHealthMetadataEvent,
        *mut *const sys::AmsMelIrHealthMetadataEventV1,
        *mut c_char,
        usize,
        *mut usize,
    ) -> sys::AmsMelStatus,
    close: unsafe extern "C" fn(
        *mut *mut sys::AmsMelIrHealthMetadataEvent,
        *mut c_char,
        usize,
        *mut usize,
    ) -> sys::AmsMelStatus,
}

static NATIVE_EVENT_OPS: NativeEventOps = NativeEventOps {
    view: sys::ams_mel_ir_health_metadata_event_view,
    close: sys::ams_mel_ir_health_metadata_event_close,
};

/// Private RAII owner of one received native metadata event. Drop releases a
/// still-owned event on every early return and on unwind, so a failed or
/// partial conversion can neither leak the native event nor publish an
/// incomplete value.
struct NativeEvent {
    raw: *mut sys::AmsMelIrHealthMetadataEvent,
    ops: &'static NativeEventOps,
}

impl NativeEvent {
    fn new(ops: &'static NativeEventOps) -> Self {
        Self {
            raw: ptr::null_mut(),
            ops,
        }
    }

    /// Views the native record, converts only its active payload into owned
    /// Rust data, then closes the native event before returning that data.
    fn into_owned(mut self) -> Result<HealthMetadataEvent, Error> {
        if self.raw.is_null() {
            return Err(protocol(
                "Health metadata receive succeeded without an event",
            ));
        }
        let mut view: *const sys::AmsMelIrHealthMetadataEventV1 = ptr::null();
        let raw = self.raw;
        let ops = self.ops;
        call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: `raw` is this guard's live event and `view` is writable
            // storage for the borrowed record pointer.
            unsafe { (ops.view)(raw, &mut view, diagnostic, capacity, required) }
        })?;
        // SAFETY: a non-null, aligned record pointer refers to one record
        // owned by the live event in `self`, which outlives this borrow; the
        // borrow ends before `close` below.
        let record = unsafe { checked_span(view, 1, "Health metadata event record")? };
        let value = event_from_raw(&record[0])?;
        self.close()?;
        Ok(value)
    }

    fn close(&mut self) -> Result<(), Error> {
        let ops = self.ops;
        call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: `raw` is this guard's unique event owner; no record
            // borrowed from it is used after this call.
            unsafe { (ops.close)(&mut self.raw, diagnostic, capacity, required) }
        })?;
        if !self.raw.is_null() {
            // Native code reported the event released. Never close the same
            // handle a second time from Drop; forgetting it is the safe choice.
            self.raw = ptr::null_mut();
            return Err(protocol(
                "Health metadata event close succeeded without releasing the event",
            ));
        }
        Ok(())
    }
}

impl Drop for NativeEvent {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            // SAFETY: unique event owner; null diagnostics are supported. The
            // result cannot be reported from Drop.
            let _ = unsafe { (self.ops.close)(&mut self.raw, ptr::null_mut(), 0, ptr::null_mut()) };
        }
    }
}

// ---------------------------------------------------------------------------
// Checked conversion of a borrowed native record into owned Rust values.
//
// Following the crate's existing `owned_text`/`owned_u32_mapped` convention,
// these helpers are called only with records borrowed from a live native event
// (or, in unit tests, fabricated records whose storage outlives the call).
// Every span is validated by `checked_span` (null-with-size, alignment and
// size bound) and copied with fallible reservation via `copy_mapped`.
// ---------------------------------------------------------------------------

fn items<'a, T>(data: *const T, size: usize, field: &str) -> Result<&'a [T], Error> {
    // SAFETY: the span belongs to a native record kept alive by the caller for
    // the duration of the conversion; `checked_span` rejects null nonempty,
    // misaligned and impossibly sized spans before forming a slice.
    unsafe { checked_span(data, size, field) }
}

fn ids(span: sys::AmsMelUciIdSpanV1, field: &str) -> Result<Vec<UciId>, Error> {
    copy_mapped(items(span.data, span.size, field)?, field, |id| {
        owned_uci_id(id, field)
    })
}

fn euler(raw: &sys::AmsMelEulerV1) -> Euler {
    Euler {
        roll: raw.roll,
        pitch: raw.pitch,
        yaw: raw.yaw,
    }
}

/// Dispatches on the native discriminant and converts only the active payload.
/// An unpublished discriminant is a `ProtocolInconsistency`; no default family
/// is ever selected.
fn event_from_raw(raw: &sys::AmsMelIrHealthMetadataEventV1) -> Result<HealthMetadataEvent, Error> {
    Ok(
        match HealthMetadataKind::decode(raw.kind, "Health metadata event kind")? {
            HealthMetadataKind::MfaStatus => {
                HealthMetadataEvent::MfaStatus(mfa_status(&raw.mfa_status)?)
            }
            HealthMetadataKind::BitStatus => {
                HealthMetadataEvent::BitStatus(bit_status(&raw.bit_status)?)
            }
            HealthMetadataKind::SubsystemStatus => {
                HealthMetadataEvent::SubsystemStatus(subsystem_status(&raw.subsystem_status)?)
            }
            HealthMetadataKind::DiscreteStatus => {
                HealthMetadataEvent::DiscreteStatus(pairs(raw.discrete_status, "DiscreteStatus")?)
            }
            HealthMetadataKind::SecurityAudit => {
                HealthMetadataEvent::SecurityAudit(security_audit(&raw.security_audit)?)
            }
            HealthMetadataKind::MfaStatusDetailed => HealthMetadataEvent::MfaStatusDetailed(pairs(
                raw.mfa_status_detailed,
                "MFA_StatusDetailed",
            )?),
        },
    )
}

/// Common MEL `MFA_State` is one published domain shared by IR and RF
/// (`ams_mel_rf_mfa_state_t` is a typedef of `ams_mel_ir_mfa_state_t`), so the
/// existing [`RfMfaState`] is reused. Values at or beyond `MAX_EXCLUSIVE` are a
/// `ProtocolInconsistency` here, because they arrive from the provider.
fn mfa_state(raw: u32) -> Result<RfMfaState, Error> {
    RfMfaState::ALL
        .iter()
        .copied()
        .find(|state| state.as_raw() == raw)
        .ok_or_else(|| {
            protocol(&format!(
                "native MFA_Status.state has unpublished value {raw}"
            ))
        })
}

fn mfa_status(raw: &sys::AmsMelMfaStatusV1) -> Result<MfaStatus, Error> {
    let about = &raw.about;
    Ok(MfaStatus {
        state: mfa_state(raw.state)?,
        state_description: owned_text(raw.state_description, "MFA_Status.state_description")?,
        mode_description: owned_text(raw.mode_description, "MFA_Status.mode_description")?,
        transition_status: StateTransitionStatus::decode(
            raw.transition_status,
            "MFA_Status.transition_status",
        )?,
        about: About {
            model: owned_text(about.model, "About.model")?,
            serial_number: owned_text(about.serial_number, "About.serial_number")?,
            software_version: owned_text(about.software_version, "About.software_version")?,
            bootloader_software_version: owned_text(
                about.bootloader_software_version,
                "About.bootloader_software_version",
            )?,
            hardware_version: owned_text(about.hardware_version, "About.hardware_version")?,
        },
        components: copy_mapped(
            items(
                raw.components.data,
                raw.components.size,
                "MFA_Status.components",
            )?,
            "MFA_Status.components",
            mfa_component,
        )?,
    })
}

fn mfa_component(raw: &sys::AmsMelMfaComponentV1) -> Result<MfaComponent, Error> {
    let details = &raw.installation_details;
    let location = &details.location;
    Ok(MfaComponent {
        component_id: owned_uci_id(&raw.component_id, "MFA_Component.component_id")?,
        state: ComponentState::decode(raw.state, "MFA_Component.state")?,
        temperature: TemperatureStatus {
            temperature_c: raw.temperature.temperature_c,
            state: TemperatureState::decode(raw.temperature.state, "TemperatureStatus.state")?,
        },
        installation_location_id: ForeignKey {
            key: owned_text(raw.installation_location_id.key, "ForeignKey.key")?,
            system_name: owned_text(
                raw.installation_location_id.system_name,
                "ForeignKey.system_name",
            )?,
        },
        installation_details: InstallationDetails {
            // Text is NUL-free (`owned_text` rejects NUL), which is the
            // `ComponentLocation` invariant; offsets are copied bit-for-bit.
            location: ComponentLocation {
                offset_x_m: location.offset_x_m,
                offset_y_m: location.offset_y_m,
                offset_z_m: location.offset_z_m,
                key: owned_text(location.key, "InstallationDetails.location.key")?,
                system_name: owned_text(
                    location.system_name,
                    "InstallationDetails.location.system_name",
                )?,
            },
            orientation: euler(&details.orientation),
            boresight: euler(&details.boresight),
        },
    })
}

pub(crate) fn bit_status(raw: &sys::AmsMelBitStatusV1) -> Result<BitStatus, Error> {
    let active = &raw.active_bits;
    let completed = &raw.completed_bits;
    Ok(BitStatus {
        active_bits: copy_mapped(
            items(active.data, active.size, "BIT_Status.active_bits")?,
            "BIT_Status.active_bits",
            |bit| {
                Ok(ActiveBit {
                    bit_id: owned_uci_id(&bit.bit_id, "ActiveBIT.bit_id")?,
                    estimated_completion_time_ns: bit.estimated_completion_time_ns,
                    estimated_percent_complete: bit.estimated_percent_complete,
                })
            },
        )?,
        completed_bits: copy_mapped(
            items(completed.data, completed.size, "BIT_Status.completed_bits")?,
            "BIT_Status.completed_bits",
            completed_bit,
        )?,
        faults: copy_mapped(
            items(raw.faults.data, raw.faults.size, "BIT_Status.faults")?,
            "BIT_Status.faults",
            fault,
        )?,
    })
}

fn completed_bit(raw: &sys::AmsMelCompletedBitV1) -> Result<CompletedBit, Error> {
    let bit_items = &raw.bit_items;
    Ok(CompletedBit {
        bit_id: owned_uci_id(&raw.bit_id, "CompletedBIT.bit_id")?,
        time_tag_ns: raw.time_tag_ns,
        result: BitResult::decode(raw.result, "CompletedBIT.result")?,
        fail_reason: owned_text(raw.fail_reason, "CompletedBIT.fail_reason")?,
        bit_items: copy_mapped(
            items(bit_items.data, bit_items.size, "CompletedBIT.bit_items")?,
            "CompletedBIT.bit_items",
            |item| {
                Ok(CompletedBitItem {
                    bit_item_name: owned_text(item.bit_item_name, "CompletedBIT_Item.name")?,
                    result: BitResult::decode(item.result, "CompletedBIT_Item.result")?,
                    fail_reason: owned_text(item.fail_reason, "CompletedBIT_Item.fail_reason")?,
                })
            },
        )?,
    })
}

fn fault(raw: &sys::AmsMelFaultV1) -> Result<Fault, Error> {
    let groups = &raw.ambiguity_groups;
    Ok(Fault {
        fault_id: owned_uci_id(&raw.fault_id, "Fault.fault_id")?,
        severity: FaultSeverity::decode(raw.severity, "Fault.severity")?,
        state: FaultState::decode(raw.state, "Fault.state")?,
        fault_data: copy_mapped(
            items(raw.fault_data.data, raw.fault_data.size, "Fault.fault_data")?,
            "Fault.fault_data",
            |data| {
                Ok(FaultData {
                    key: owned_text(data.key, "FaultData.key")?,
                    value: owned_text(data.value, "FaultData.value")?,
                    format: owned_text(data.format, "FaultData.format")?,
                    units: owned_text(data.units, "FaultData.units")?,
                })
            },
        )?,
        detection_time_ns: raw.detection_time_ns,
        fault_code: owned_text(raw.fault_code, "Fault.fault_code")?,
        fault_description: owned_text(raw.fault_description, "Fault.fault_description")?,
        component_ids: ids(raw.component_ids, "Fault.component_ids")?,
        ambiguity_groups: copy_mapped(
            items(groups.data, groups.size, "Fault.ambiguity_groups")?,
            "Fault.ambiguity_groups",
            |group| {
                Ok(FaultAmbiguityGroup {
                    diagnostic_test_ids: ids(
                        group.diagnostic_test_ids,
                        "FaultAmbiguityGroup.diagnostic_test_ids",
                    )?,
                    component_ids: ids(group.component_ids, "FaultAmbiguityGroup.component_ids")?,
                })
            },
        )?,
    })
}

fn subsystem_status(raw: &sys::AmsMelIrSubsystemStatusV1) -> Result<SubsystemStatus, Error> {
    let dependencies = &raw.subsystems;
    Ok(SubsystemStatus {
        subsystem_id: raw.subsystem_id,
        criticality: raw.criticality,
        status_sequence_number: raw.status_sequence_number,
        failure: FailureLevel::decode(raw.failure, "SubsystemStatusResp.failure")?,
        subsystem_count: raw.subsystem_count,
        subsystems: copy_mapped(
            items(
                dependencies.data,
                dependencies.size,
                "SubsystemStatusResp.subsystems",
            )?,
            "SubsystemStatusResp.subsystems",
            |dependency| {
                Ok(SubsystemDependency {
                    subsystem_id: dependency.subsystem_id,
                    criticality: dependency.criticality,
                    failure: FailureLevel::decode(dependency.failure, "SubsystemDepInfo.failure")?,
                })
            },
        )?,
        csci_count: raw.csci_count,
        csci: copy_mapped(
            items(raw.csci.data, raw.csci.size, "SubsystemStatusResp.csci")?,
            "SubsystemStatusResp.csci",
            csci_info,
        )?,
    })
}

fn csci_info(raw: &sys::AmsMelIrSubsystemCsciInfoV1) -> Result<CsciInfo, Error> {
    Ok(CsciInfo {
        csci: owned_text(raw.csci, "SubsystemCSCIInfo.csci")?,
        mode: CsciMode::decode(raw.mode, "SubsystemCSCIInfo.mode")?,
        version: CsciVersion {
            source: raw.version.source,
            major_revision: raw.version.major_revision,
            minor_revision: raw.version.minor_revision,
            engineering_revision: raw.version.engineering_revision,
        },
        criticality: raw.criticality,
        failure: FailureLevel::decode(raw.failure, "SubsystemCSCIInfo.failure")?,
        bit_report: raw.bit_report,
        connection_established: checked_bool(
            raw.connection_established,
            "SubsystemCSCIInfo.connection_established",
        )?,
    })
}

fn pairs(span: sys::AmsMelNameValuePairSpanV1, field: &str) -> Result<Vec<NameValuePair>, Error> {
    copy_mapped(items(span.data, span.size, field)?, field, |pair| {
        Ok(NameValuePair {
            name: owned_text(pair.name, field)?,
            value: owned_text(pair.value, field)?,
        })
    })
}

fn security_audit(raw: &sys::AmsMelSecurityAuditRecordV1) -> Result<SecurityAuditRecord, Error> {
    let artifacts = &raw.artifacts;
    Ok(SecurityAuditRecord {
        security_event_id: owned_uci_id(&raw.security_event_id, "SecurityAudit.security_event_id")?,
        event_timestamp_ns: raw.event_timestamp_ns,
        subsystem_id: owned_uci_id(&raw.subsystem_id, "SecurityAudit.subsystem_id")?,
        artifacts: copy_mapped(
            items(artifacts.data, artifacts.size, "SecurityAudit.artifacts")?,
            "SecurityAudit.artifacts",
            |artifact| {
                Ok(SecurityArtifact {
                    component_id: owned_uci_id(
                        &artifact.component_id,
                        "SecurityArtifact.component_id",
                    )?,
                    associated_id: owned_uci_id(
                        &artifact.associated_id,
                        "SecurityArtifact.associated_id",
                    )?,
                })
            },
        )?,
        event: security_event(&raw.event)?,
        outcome: SecurityOutcome::decode(raw.outcome, "SecurityAudit.outcome")?,
        severity: SecuritySeverity::decode(raw.severity, "SecurityAudit.severity")?,
    })
}

/// Requires an identifier outside the selected alternative to be unset (the
/// native facade's default: all-zero UUID and empty label), so omitting it
/// from the typed variant discards nothing.
fn inapplicable_id(raw: &sys::AmsMelUciIdV1, field: &str) -> Result<(), Error> {
    if raw.uuid == [0; 16] && raw.descriptive_label.size == 0 {
        Ok(())
    } else {
        Err(protocol(&format!(
            "native {field} is set but does not belong to the security event kind"
        )))
    }
}

fn security_event(raw: &sys::AmsMelSecurityEventV1) -> Result<SecurityEvent, Error> {
    const CATEGORY: &str = "SecurityEvent.category";
    const SUBSYSTEM: &str = "SecurityEvent.subsystem_id";
    const SERVICE: &str = "SecurityEvent.service_id";
    const MDF: &str = "SecurityEvent.mdf_id";
    let kind = SecurityEventKind::decode(raw.kind, "SecurityEvent.kind")?;
    let authentication = kind == SecurityEventKind::Authentication;
    let has_mdf = matches!(
        kind,
        SecurityEventKind::Authentication
            | SecurityEventKind::Integrity
            | SecurityEventKind::FileManagement
    );
    if !authentication {
        inapplicable_id(&raw.subsystem_id, SUBSYSTEM)?;
        inapplicable_id(&raw.service_id, SERVICE)?;
    }
    if !has_mdf {
        inapplicable_id(&raw.mdf_id, MDF)?;
    }
    if kind == SecurityEventKind::None {
        if raw.category != 0 || raw.details.size != 0 {
            return Err(protocol(
                "native SecurityEvent of kind None carries a category or details",
            ));
        }
        return Ok(SecurityEvent::None);
    }
    let details = owned_text(raw.details, "SecurityEvent.details")?;
    Ok(match kind {
        SecurityEventKind::None => SecurityEvent::None,
        SecurityEventKind::Authentication => SecurityEvent::Authentication {
            category: SecurityAuthenticationCategory::decode(raw.category, CATEGORY)?,
            details,
            subsystem_id: owned_uci_id(&raw.subsystem_id, SUBSYSTEM)?,
            service_id: owned_uci_id(&raw.service_id, SERVICE)?,
            mdf_id: owned_uci_id(&raw.mdf_id, MDF)?,
        },
        SecurityEventKind::Integrity => SecurityEvent::Integrity {
            category: SecurityIntegrityCategory::decode(raw.category, CATEGORY)?,
            details,
            mdf_id: owned_uci_id(&raw.mdf_id, MDF)?,
        },
        SecurityEventKind::FileManagement => SecurityEvent::FileManagement {
            category: SecurityFileManagementCategory::decode(raw.category, CATEGORY)?,
            details,
            mdf_id: owned_uci_id(&raw.mdf_id, MDF)?,
        },
        SecurityEventKind::KeyManagement => SecurityEvent::KeyManagement {
            category: SecurityKeyManagementCategory::decode(raw.category, CATEGORY)?,
            details,
        },
        SecurityEventKind::System => SecurityEvent::System {
            category: SecuritySystemCategory::decode(raw.category, CATEGORY)?,
            details,
        },
        SecurityEventKind::Sanitization => SecurityEvent::Sanitization {
            category: SecuritySanitizationCategory::decode(raw.category, CATEGORY)?,
            details,
        },
    })
}

#[cfg(test)]
mod tests {
    use std::cell::Cell;
    use std::mem;
    use std::panic::{self, AssertUnwindSafe};
    use std::ptr::NonNull;

    use super::*;

    macro_rules! span {
        ($ty:ident, $values:expr) => {
            sys::$ty {
                data: $values.as_ptr(),
                size: $values.len(),
            }
        };
    }

    fn text(bytes: &'static [u8]) -> sys::AmsMelStringViewV1 {
        sys::AmsMelStringViewV1 {
            data: if bytes.is_empty() {
                ptr::null()
            } else {
                bytes.as_ptr().cast::<c_char>()
            },
            size: bytes.len(),
        }
    }

    fn uci(seed: u8, label: &'static [u8]) -> sys::AmsMelUciIdV1 {
        sys::AmsMelUciIdV1 {
            uuid: [seed; 16],
            descriptive_label: text(label),
        }
    }

    fn owned(seed: u8, label: &str) -> UciId {
        UciId::new([seed; 16], label).expect("test ID")
    }

    fn euler_raw(roll: f64, pitch: f64, yaw: f64) -> sys::AmsMelEulerV1 {
        sys::AmsMelEulerV1 { roll, pitch, yaw }
    }

    fn blank() -> sys::AmsMelIrHealthMetadataEventV1 {
        // SAFETY: every field of this plain C record is an integer, an f64,
        // or a span/view whose all-zero value is the valid empty span.
        unsafe { mem::zeroed() }
    }

    fn kind_of(kind: u32) -> sys::AmsMelIrHealthMetadataEventV1 {
        let mut event = blank();
        event.kind = kind;
        event
    }

    fn failure(result: Result<HealthMetadataEvent, Error>, kind: ErrorKind, needle: &str) -> Error {
        let error = result.expect_err("conversion must fail, never return a partial event");
        assert_eq!(error.kind(), &kind, "{error}");
        assert!(
            error.diagnostic().is_some_and(|text| text.contains(needle)),
            "diagnostic of {error} does not mention {needle}"
        );
        error
    }

    /// One valid, fully populated instance of every nested native record.
    /// Vec heap buffers never move, so the spans wired here stay valid while
    /// tests mutate individual elements in place.
    struct Fixture {
        components: Vec<sys::AmsMelMfaComponentV1>,
        active: Vec<sys::AmsMelActiveBitV1>,
        items: Vec<sys::AmsMelCompletedBitItemV1>,
        completed: Vec<sys::AmsMelCompletedBitV1>,
        fault_data: Vec<sys::AmsMelFaultDataV1>,
        fault_components: Vec<sys::AmsMelUciIdV1>,
        group_tests: Vec<sys::AmsMelUciIdV1>,
        group_components: Vec<sys::AmsMelUciIdV1>,
        groups: Vec<sys::AmsMelFaultAmbiguityGroupV1>,
        faults: Vec<sys::AmsMelFaultV1>,
        dependencies: Vec<sys::AmsMelIrSubsystemDepInfoV1>,
        csci: Vec<sys::AmsMelIrSubsystemCsciInfoV1>,
        discrete: Vec<sys::AmsMelNameValuePairV1>,
        detailed: Vec<sys::AmsMelNameValuePairV1>,
        artifacts: Vec<sys::AmsMelSecurityArtifactV1>,
        mfa: sys::AmsMelMfaStatusV1,
        bit: sys::AmsMelBitStatusV1,
        subsystem: sys::AmsMelIrSubsystemStatusV1,
        discrete_span: sys::AmsMelNameValuePairSpanV1,
        detailed_span: sys::AmsMelNameValuePairSpanV1,
        security: sys::AmsMelSecurityAuditRecordV1,
    }

    fn component_raw() -> sys::AmsMelMfaComponentV1 {
        sys::AmsMelMfaComponentV1 {
            component_id: uci(0x91, "component-\u{3b3}".as_bytes()),
            state: sys::AMS_MEL_COMPONENT_STATE_OPERATIONAL,
            temperature: sys::AmsMelTemperatureStatusV1 {
                temperature_c: 42.25,
                state: sys::AMS_MEL_TEMPERATURE_STATE_NORMAL,
            },
            installation_location_id: sys::AmsMelForeignKeyV1 {
                key: text(b"rack"),
                system_name: text(b"aircraft"),
            },
            installation_details: sys::AmsMelInstallationDetailsV1 {
                location: sys::AmsMelComponentLocationV1 {
                    offset_x_m: 1.5,
                    offset_y_m: -2.5,
                    offset_z_m: 3.5,
                    key: text(b"bay"),
                    system_name: text("platform-\u{20ac}".as_bytes()),
                },
                orientation: euler_raw(0.1, 0.2, 0.3),
                boresight: euler_raw(-0.4, 0.5, -0.6),
            },
        }
    }

    fn pair(name: &'static [u8], value: &'static [u8]) -> sys::AmsMelNameValuePairV1 {
        sys::AmsMelNameValuePairV1 {
            name: text(name),
            value: text(value),
        }
    }

    fn empty_ids() -> sys::AmsMelUciIdSpanV1 {
        sys::AmsMelUciIdSpanV1 {
            data: ptr::null(),
            size: 0,
        }
    }

    fn csci_raw() -> sys::AmsMelIrSubsystemCsciInfoV1 {
        sys::AmsMelIrSubsystemCsciInfoV1 {
            csci: text(b"flight"),
            mode: sys::AMS_MEL_IR_CSCI_MODE_OPERATIONAL,
            version: sys::AmsMelIrVersionV1 {
                source: 1,
                major_revision: 2,
                minor_revision: 3,
                engineering_revision: u32::MAX,
            },
            criticality: 10,
            failure: sys::AMS_MEL_IR_FAILURE_PARAMETRIC,
            bit_report: 0x8000_0005,
            connection_established: 1,
        }
    }

    fn bit_item(result: u32, reason: &'static [u8]) -> sys::AmsMelCompletedBitItemV1 {
        sys::AmsMelCompletedBitItemV1 {
            bit_item_name: text(b"item"),
            result,
            fail_reason: text(reason),
        }
    }

    impl Fixture {
        fn new() -> Box<Self> {
            /// Placeholder for a plain C record that `wire` fully assigns.
            fn zero<T: Copy>() -> T {
                // SAFETY: used only for the fixture's `repr(C)` records of
                // integers, f64s and spans/views, for which all-zero bits are
                // a valid value (zero kind, empty spans, null text).
                unsafe { mem::zeroed() }
            }
            let mut fx = Box::new(Self {
                components: vec![component_raw()],
                active: vec![sys::AmsMelActiveBitV1 {
                    bit_id: uci(0x81, b"active"),
                    estimated_completion_time_ns: -5,
                    estimated_percent_complete: 0.5,
                }],
                items: vec![
                    bit_item(sys::AMS_MEL_BIT_RESULT_PASS, b""),
                    bit_item(sys::AMS_MEL_BIT_RESULT_FAIL, b"reason"),
                ],
                completed: Vec::new(),
                fault_data: vec![sys::AmsMelFaultDataV1 {
                    key: text(b"temperature"),
                    value: text(b"101"),
                    format: text(b"integer"),
                    units: text("\u{b0}C".as_bytes()),
                }],
                fault_components: vec![uci(1, b"part"), uci(1, b"part")],
                group_tests: vec![uci(2, b"diagnostic")],
                group_components: vec![uci(3, b"ambiguous")],
                groups: Vec::new(),
                faults: Vec::new(),
                dependencies: vec![sys::AmsMelIrSubsystemDepInfoV1 {
                    subsystem_id: 0x8000_0003,
                    criticality: u32::MAX,
                    failure: sys::AMS_MEL_IR_FAILURE_CRITICAL,
                }],
                csci: vec![csci_raw()],
                discrete: vec![
                    pair(b"dup", b"one"),
                    pair(b"dup", b""),
                    pair(b"", "\u{20ac}".as_bytes()),
                ],
                detailed: vec![pair(b"load", b"23")],
                artifacts: vec![sys::AmsMelSecurityArtifactV1 {
                    component_id: uci(0xf1, b"artifact"),
                    associated_id: uci(0x82, b"associated"),
                }],
                mfa: zero(),
                bit: zero(),
                subsystem: zero(),
                discrete_span: zero(),
                detailed_span: zero(),
                security: zero(),
            });
            fx.completed = vec![sys::AmsMelCompletedBitV1 {
                bit_id: uci(0x92, b"completed"),
                time_tag_ns: 42,
                result: sys::AMS_MEL_BIT_RESULT_FAIL,
                fail_reason: text("failure-\u{20ac}".as_bytes()),
                bit_items: span!(AmsMelCompletedBitItemSpanV1, fx.items),
            }];
            fx.groups = vec![
                sys::AmsMelFaultAmbiguityGroupV1 {
                    diagnostic_test_ids: span!(AmsMelUciIdSpanV1, fx.group_tests),
                    component_ids: span!(AmsMelUciIdSpanV1, fx.group_components),
                },
                sys::AmsMelFaultAmbiguityGroupV1 {
                    diagnostic_test_ids: empty_ids(),
                    component_ids: empty_ids(),
                },
            ];
            fx.faults = vec![sys::AmsMelFaultV1 {
                fault_id: uci(0xd0, b"fault"),
                severity: sys::AMS_MEL_FAULT_SEVERITY_WARNING,
                state: sys::AMS_MEL_FAULT_STATE_SET,
                fault_data: span!(AmsMelFaultDataSpanV1, fx.fault_data),
                detection_time_ns: -1_234_567,
                fault_code: text(b"F-42"),
                fault_description: text(b"overheat"),
                component_ids: span!(AmsMelUciIdSpanV1, fx.fault_components),
                ambiguity_groups: span!(AmsMelFaultAmbiguityGroupSpanV1, fx.groups),
            }];
            fx.wire();
            fx
        }

        /// (Re)builds every top-level record from the current collections.
        fn wire(&mut self) {
            self.mfa = sys::AmsMelMfaStatusV1 {
                state: sys::AMS_MEL_IR_MFA_STATE_MAINTENANCE,
                state_description: text("healthy-\u{3b1}".as_bytes()),
                mode_description: text(b""),
                transition_status: sys::AMS_MEL_STATE_TRANSITION_TRANSITIONING,
                about: sys::AmsMelAboutV1 {
                    model: text(b"model"),
                    serial_number: text(b"serial"),
                    software_version: text(b"software"),
                    bootloader_software_version: text(b"boot"),
                    hardware_version: text(b"hardware"),
                },
                components: span!(AmsMelMfaComponentSpanV1, self.components),
            };
            self.bit = sys::AmsMelBitStatusV1 {
                active_bits: span!(AmsMelActiveBitSpanV1, self.active),
                completed_bits: span!(AmsMelCompletedBitSpanV1, self.completed),
                faults: span!(AmsMelFaultSpanV1, self.faults),
            };
            self.subsystem = sys::AmsMelIrSubsystemStatusV1 {
                subsystem_id: 0x8000_0001,
                criticality: 7,
                status_sequence_number: u32::MAX,
                failure: sys::AMS_MEL_IR_FAILURE_MAJOR,
                subsystem_count: 99,
                subsystems: span!(AmsMelIrSubsystemDepInfoSpanV1, self.dependencies),
                csci_count: 0xffff_fffe,
                csci: span!(AmsMelIrSubsystemCsciInfoSpanV1, self.csci),
            };
            self.discrete_span = span!(AmsMelNameValuePairSpanV1, self.discrete);
            self.detailed_span = span!(AmsMelNameValuePairSpanV1, self.detailed);
            self.security = sys::AmsMelSecurityAuditRecordV1 {
                security_event_id: uci(0xd1, b"event"),
                event_timestamp_ns: i64::MIN,
                subsystem_id: uci(0xe1, b"security subsystem"),
                artifacts: span!(AmsMelSecurityArtifactSpanV1, self.artifacts),
                event: sys::AmsMelSecurityEventV1 {
                    kind: sys::AMS_MEL_SECURITY_EVENT_AUTHENTICATION,
                    category: 5,
                    details: text(b"auth"),
                    subsystem_id: uci(1, b"auth subsystem"),
                    service_id: uci(2, b"auth service"),
                    mdf_id: uci(3, b"auth mdf"),
                },
                outcome: sys::AMS_MEL_SECURITY_OUTCOME_FAILURE,
                severity: sys::AMS_MEL_SECURITY_SEVERITY_WARNING,
            };
        }

        /// A native record of `kind` whose active payload is the fixture's
        /// record. Inactive payloads stay zero.
        fn event(&self, kind: u32) -> sys::AmsMelIrHealthMetadataEventV1 {
            let mut event = kind_of(kind);
            match kind {
                sys::AMS_MEL_IR_HEALTH_METADATA_MFA_STATUS => event.mfa_status = self.mfa,
                sys::AMS_MEL_IR_HEALTH_METADATA_BIT_STATUS => event.bit_status = self.bit,
                sys::AMS_MEL_IR_HEALTH_METADATA_SUBSYSTEM_STATUS => {
                    event.subsystem_status = self.subsystem
                }
                sys::AMS_MEL_IR_HEALTH_METADATA_DISCRETE_STATUS => {
                    event.discrete_status = self.discrete_span
                }
                sys::AMS_MEL_IR_HEALTH_METADATA_SECURITY_AUDIT => {
                    event.security_audit = self.security
                }
                sys::AMS_MEL_IR_HEALTH_METADATA_MFA_STATUS_DETAILED => {
                    event.mfa_status_detailed = self.detailed_span
                }
                _ => {}
            }
            event
        }

        fn convert(&self, kind: u32) -> Result<HealthMetadataEvent, Error> {
            event_from_raw(&self.event(kind))
        }
    }

    fn expected_component() -> MfaComponent {
        MfaComponent {
            component_id: owned(0x91, "component-\u{3b3}"),
            state: ComponentState::Operational,
            temperature: TemperatureStatus {
                temperature_c: 42.25,
                state: TemperatureState::Normal,
            },
            installation_location_id: ForeignKey {
                key: "rack".into(),
                system_name: "aircraft".into(),
            },
            installation_details: InstallationDetails {
                location: ComponentLocation::new(1.5, -2.5, 3.5, "bay", "platform-\u{20ac}")
                    .expect("location"),
                orientation: Euler {
                    roll: 0.1,
                    pitch: 0.2,
                    yaw: 0.3,
                },
                boresight: Euler {
                    roll: -0.4,
                    pitch: 0.5,
                    yaw: -0.6,
                },
            },
        }
    }

    fn expected_pairs(values: &[(&str, &str)]) -> Vec<NameValuePair> {
        values
            .iter()
            .map(|(name, value)| NameValuePair {
                name: (*name).into(),
                value: (*value).into(),
            })
            .collect()
    }

    fn expected_bit() -> BitStatus {
        let item = |result, reason: &str| CompletedBitItem {
            bit_item_name: "item".into(),
            result,
            fail_reason: reason.into(),
        };
        BitStatus {
            active_bits: vec![ActiveBit {
                bit_id: owned(0x81, "active"),
                estimated_completion_time_ns: -5,
                estimated_percent_complete: 0.5,
            }],
            completed_bits: vec![CompletedBit {
                bit_id: owned(0x92, "completed"),
                time_tag_ns: 42,
                result: BitResult::Fail,
                fail_reason: "failure-\u{20ac}".into(),
                bit_items: vec![item(BitResult::Pass, ""), item(BitResult::Fail, "reason")],
            }],
            faults: vec![Fault {
                fault_id: owned(0xd0, "fault"),
                severity: FaultSeverity::Warning,
                state: FaultState::Set,
                fault_data: vec![FaultData {
                    key: "temperature".into(),
                    value: "101".into(),
                    format: "integer".into(),
                    units: "\u{b0}C".into(),
                }],
                detection_time_ns: -1_234_567,
                fault_code: "F-42".into(),
                fault_description: "overheat".into(),
                component_ids: vec![owned(1, "part"), owned(1, "part")],
                ambiguity_groups: vec![
                    FaultAmbiguityGroup {
                        diagnostic_test_ids: vec![owned(2, "diagnostic")],
                        component_ids: vec![owned(3, "ambiguous")],
                    },
                    FaultAmbiguityGroup {
                        diagnostic_test_ids: Vec::new(),
                        component_ids: Vec::new(),
                    },
                ],
            }],
        }
    }

    fn expected_subsystem() -> SubsystemStatus {
        SubsystemStatus {
            subsystem_id: 0x8000_0001,
            criticality: 7,
            status_sequence_number: u32::MAX,
            failure: FailureLevel::Major,
            subsystem_count: 99,
            subsystems: vec![SubsystemDependency {
                subsystem_id: 0x8000_0003,
                criticality: u32::MAX,
                failure: FailureLevel::Critical,
            }],
            csci_count: 0xffff_fffe,
            csci: vec![CsciInfo {
                csci: "flight".into(),
                mode: CsciMode::Operational,
                version: CsciVersion {
                    source: 1,
                    major_revision: 2,
                    minor_revision: 3,
                    engineering_revision: u32::MAX,
                },
                criticality: 10,
                failure: FailureLevel::Parametric,
                bit_report: 0x8000_0005,
                connection_established: true,
            }],
        }
    }

    #[test]
    fn all_six_discriminants_map_to_complete_owned_payloads() {
        let raw: Vec<u32> = HealthMetadataKind::ALL.iter().map(|k| k.as_raw()).collect();
        assert_eq!(raw, vec![1, 2, 3, 4, 5, 6]);
        let fx = Fixture::new();
        let expected = [
            HealthMetadataEvent::MfaStatus(MfaStatus {
                state: RfMfaState::Maintenance,
                state_description: "healthy-\u{3b1}".into(),
                mode_description: String::new(),
                transition_status: StateTransitionStatus::Transitioning,
                about: About {
                    model: "model".into(),
                    serial_number: "serial".into(),
                    software_version: "software".into(),
                    bootloader_software_version: "boot".into(),
                    hardware_version: "hardware".into(),
                },
                components: vec![expected_component()],
            }),
            HealthMetadataEvent::BitStatus(expected_bit()),
            HealthMetadataEvent::SubsystemStatus(expected_subsystem()),
            HealthMetadataEvent::DiscreteStatus(expected_pairs(&[
                ("dup", "one"),
                ("dup", ""),
                ("", "\u{20ac}"),
            ])),
            HealthMetadataEvent::SecurityAudit(SecurityAuditRecord {
                security_event_id: owned(0xd1, "event"),
                event_timestamp_ns: i64::MIN,
                subsystem_id: owned(0xe1, "security subsystem"),
                artifacts: vec![SecurityArtifact {
                    component_id: owned(0xf1, "artifact"),
                    associated_id: owned(0x82, "associated"),
                }],
                event: SecurityEvent::Authentication {
                    category: SecurityAuthenticationCategory::MdfAuthentication,
                    details: "auth".into(),
                    subsystem_id: owned(1, "auth subsystem"),
                    service_id: owned(2, "auth service"),
                    mdf_id: owned(3, "auth mdf"),
                },
                outcome: SecurityOutcome::Failure,
                severity: SecuritySeverity::Warning,
            }),
            HealthMetadataEvent::MfaStatusDetailed(expected_pairs(&[("load", "23")])),
        ];
        for (kind, expected) in HealthMetadataKind::ALL.iter().zip(expected) {
            let value = fx.convert(kind.as_raw()).expect("valid event");
            assert_eq!(value.kind(), *kind);
            assert_eq!(value, expected);
        }
    }

    #[test]
    fn unknown_event_kinds_are_protocol_errors_never_a_default_family() {
        let fx = Fixture::new();
        for kind in [0, 7, 0x8000_0000, u32::MAX] {
            // Every payload is valid, so only the discriminant can fail.
            let mut raw = fx.event(sys::AMS_MEL_IR_HEALTH_METADATA_MFA_STATUS);
            raw.bit_status = fx.bit;
            raw.subsystem_status = fx.subsystem;
            raw.discrete_status = fx.discrete_span;
            raw.security_audit = fx.security;
            raw.mfa_status_detailed = fx.detailed_span;
            raw.kind = kind;
            failure(
                event_from_raw(&raw),
                ErrorKind::ProtocolInconsistency,
                &format!("event kind has unpublished value {kind}"),
            );
        }
    }

    /// Inactive alternatives are never inspected: poisoning all of them with
    /// null nonempty spans and invalid enums does not affect the active one.
    #[test]
    fn only_the_active_payload_is_read() {
        let fx = Fixture::new();
        let mut poisoned = blank();
        let null = |size| sys::AmsMelNameValuePairSpanV1 {
            data: ptr::null(),
            size,
        };
        poisoned.mfa_status.state = u32::MAX;
        poisoned.mfa_status.components.size = 3;
        poisoned.bit_status.faults.size = 9;
        poisoned.subsystem_status.failure = u32::MAX;
        poisoned.subsystem_status.csci.size = 4;
        poisoned.discrete_status = null(5);
        poisoned.security_audit.outcome = u32::MAX;
        poisoned.security_audit.artifacts.size = 6;
        poisoned.mfa_status_detailed = null(7);
        for &kind in HealthMetadataKind::ALL {
            let clean = fx.event(kind.as_raw());
            let mut raw = poisoned;
            raw.kind = kind.as_raw();
            match kind {
                HealthMetadataKind::MfaStatus => raw.mfa_status = clean.mfa_status,
                HealthMetadataKind::BitStatus => raw.bit_status = clean.bit_status,
                HealthMetadataKind::SubsystemStatus => {
                    raw.subsystem_status = clean.subsystem_status
                }
                HealthMetadataKind::DiscreteStatus => raw.discrete_status = clean.discrete_status,
                HealthMetadataKind::SecurityAudit => raw.security_audit = clean.security_audit,
                HealthMetadataKind::MfaStatusDetailed => {
                    raw.mfa_status_detailed = clean.mfa_status_detailed
                }
            }
            assert_eq!(
                event_from_raw(&raw).expect("active payload only"),
                fx.convert(kind.as_raw()).expect("clean event")
            );
        }
    }

    #[test]
    fn empty_spans_are_empty_collections_and_empty_text() {
        let event = event_from_raw(&kind_of(sys::AMS_MEL_IR_HEALTH_METADATA_BIT_STATUS));
        assert_eq!(
            event.expect("empty BIT"),
            HealthMetadataEvent::BitStatus(BitStatus {
                active_bits: Vec::new(),
                completed_bits: Vec::new(),
                faults: Vec::new(),
            })
        );
        assert_eq!(
            event_from_raw(&kind_of(sys::AMS_MEL_IR_HEALTH_METADATA_DISCRETE_STATUS))
                .expect("empty discrete"),
            HealthMetadataEvent::DiscreteStatus(Vec::new())
        );
        assert_eq!(
            event_from_raw(&kind_of(
                sys::AMS_MEL_IR_HEALTH_METADATA_MFA_STATUS_DETAILED
            ))
            .expect("empty detailed"),
            HealthMetadataEvent::MfaStatusDetailed(Vec::new())
        );
        let zero_mfa = event_from_raw(&kind_of(sys::AMS_MEL_IR_HEALTH_METADATA_MFA_STATUS));
        let HealthMetadataEvent::MfaStatus(mfa) = zero_mfa.expect("zero MFA") else {
            panic!("wrong family");
        };
        assert_eq!(mfa.state, RfMfaState::NotSet);
        assert!(mfa.state_description.is_empty() && mfa.components.is_empty());
        // Empty text may use a non-null pointer, too.
        let mut fx = Fixture::new();
        fx.detailed[0].name = sys::AmsMelStringViewV1 {
            data: b"ignored".as_ptr().cast(),
            size: 0,
        };
        fx.wire();
        assert_eq!(
            fx.convert(sys::AMS_MEL_IR_HEALTH_METADATA_MFA_STATUS_DETAILED)
                .expect("empty text"),
            HealthMetadataEvent::MfaStatusDetailed(expected_pairs(&[("", "23")]))
        );
    }

    /// Every fixed enum field rejects its first unpublished value and
    /// `u32::MAX` as `ProtocolInconsistency`; none is mapped to a variant.
    #[test]
    fn invalid_fixed_enum_values_are_rejected_at_every_nesting_level() {
        type Mutate = fn(&mut Fixture, u32);
        let cases: [(u32, &str, u32, Mutate); 16] = [
            (1, "MFA_Status.state", 15, |f, v| f.mfa.state = v),
            (1, "MFA_Status.transition_status", 4, |f, v| {
                f.mfa.transition_status = v
            }),
            (1, "MFA_Component.state", 9, |f, v| {
                f.components[0].state = v
            }),
            (1, "TemperatureStatus.state", 6, |f, v| {
                f.components[0].temperature.state = v
            }),
            (2, "CompletedBIT.result", 5, |f, v| {
                f.completed[0].result = v
            }),
            (2, "CompletedBIT_Item.result", 5, |f, v| {
                f.items[1].result = v
            }),
            (2, "Fault.severity", 5, |f, v| f.faults[0].severity = v),
            (2, "Fault.state", 4, |f, v| f.faults[0].state = v),
            (3, "SubsystemStatusResp.failure", 7, |f, v| {
                f.subsystem.failure = v
            }),
            (3, "SubsystemDepInfo.failure", 7, |f, v| {
                f.dependencies[0].failure = v
            }),
            (3, "SubsystemCSCIInfo.mode", 11, |f, v| f.csci[0].mode = v),
            (3, "SubsystemCSCIInfo.failure", 7, |f, v| {
                f.csci[0].failure = v
            }),
            (5, "SecurityAudit.outcome", 3, |f, v| f.security.outcome = v),
            (5, "SecurityAudit.severity", 5, |f, v| {
                f.security.severity = v
            }),
            (5, "SecurityEvent.kind", 7, |f, v| f.security.event.kind = v),
            (5, "SecurityEvent.category", 8, |f, v| {
                f.security.event.category = v
            }),
        ];
        for (kind, field, first_invalid, mutate) in cases {
            for value in [first_invalid, u32::MAX] {
                // Nested collections are referenced in place, and top-level
                // records are not re-wired after mutation.
                let mut fx = Fixture::new();
                mutate(&mut fx, value);
                let raw = fx.event(kind);
                failure(
                    event_from_raw(&raw),
                    ErrorKind::ProtocolInconsistency,
                    &format!("{field} has unpublished value {value}"),
                );
            }
        }
    }

    #[test]
    fn every_security_category_domain_is_bounded_per_kind() {
        let limits = [
            (sys::AMS_MEL_SECURITY_EVENT_AUTHENTICATION, 8),
            (sys::AMS_MEL_SECURITY_EVENT_INTEGRITY, 7),
            (sys::AMS_MEL_SECURITY_EVENT_FILE_MANAGEMENT, 5),
            (sys::AMS_MEL_SECURITY_EVENT_KEY_MANAGEMENT, 7),
            (sys::AMS_MEL_SECURITY_EVENT_SYSTEM, 5),
            (sys::AMS_MEL_SECURITY_EVENT_SANITIZATION, 4),
        ];
        for (kind, max_exclusive) in limits {
            for category in 0..=max_exclusive {
                let mut event = security_event_raw(kind);
                event.category = category;
                let result = security_event(&event);
                if category < max_exclusive {
                    let value = result.expect("published category");
                    assert_eq!(value.kind().as_raw(), kind);
                } else {
                    let error = result.expect_err("first unpublished category");
                    assert_eq!(error.kind(), &ErrorKind::ProtocolInconsistency);
                }
            }
        }
    }

    #[test]
    fn invalid_boolean_values_are_rejected() {
        for value in [2, 0x8000_0000, u32::MAX] {
            let mut fx = Fixture::new();
            fx.csci[0].connection_established = value;
            failure(
                fx.convert(sys::AMS_MEL_IR_HEALTH_METADATA_SUBSYSTEM_STATUS),
                ErrorKind::ProtocolInconsistency,
                &format!("connection_established has invalid boolean value {value}"),
            );
        }
        let mut fx = Fixture::new();
        fx.csci[0].connection_established = 0;
        let HealthMetadataEvent::SubsystemStatus(status) = fx
            .convert(sys::AMS_MEL_IR_HEALTH_METADATA_SUBSYSTEM_STATUS)
            .expect("false")
        else {
            panic!("wrong family");
        };
        assert!(!status.csci[0].connection_established);
    }

    /// The native facade's flattened security event for `kind`: only the
    /// identifiers that belong to that alternative are set.
    fn security_event_raw(kind: u32) -> sys::AmsMelSecurityEventV1 {
        let none = sys::AmsMelUciIdV1 {
            uuid: [0; 16],
            descriptive_label: text(b""),
        };
        let authentication = kind == sys::AMS_MEL_SECURITY_EVENT_AUTHENTICATION;
        let has_mdf = authentication
            || kind == sys::AMS_MEL_SECURITY_EVENT_INTEGRITY
            || kind == sys::AMS_MEL_SECURITY_EVENT_FILE_MANAGEMENT;
        sys::AmsMelSecurityEventV1 {
            kind,
            category: u32::from(kind != sys::AMS_MEL_SECURITY_EVENT_NONE),
            details: if kind == sys::AMS_MEL_SECURITY_EVENT_NONE {
                text(b"")
            } else {
                text("details-\u{3b6}".as_bytes())
            },
            subsystem_id: if authentication { uci(1, b"s") } else { none },
            service_id: if authentication { uci(2, b"v") } else { none },
            mdf_id: if has_mdf { uci(3, b"m") } else { none },
        }
    }

    #[test]
    fn every_security_event_alternative_keeps_its_subtype_fields() {
        let details = || "details-\u{3b6}".to_string();
        let expected = [
            SecurityEvent::None,
            SecurityEvent::Authentication {
                category: SecurityAuthenticationCategory::SubsystemAuthentication,
                details: details(),
                subsystem_id: owned(1, "s"),
                service_id: owned(2, "v"),
                mdf_id: owned(3, "m"),
            },
            SecurityEvent::Integrity {
                category: SecurityIntegrityCategory::ConfigFileIntegrityCheck,
                details: details(),
                mdf_id: owned(3, "m"),
            },
            SecurityEvent::FileManagement {
                category: SecurityFileManagementCategory::ConfigFileUpdate,
                details: details(),
                mdf_id: owned(3, "m"),
            },
            SecurityEvent::KeyManagement {
                category: SecurityKeyManagementCategory::KeyLoad,
                details: details(),
            },
            SecurityEvent::System {
                category: SecuritySystemCategory::Startup,
                details: details(),
            },
            SecurityEvent::Sanitization {
                category: SecuritySanitizationCategory::Zeroization,
                details: details(),
            },
        ];
        for (kind, expected) in SecurityEventKind::ALL.iter().zip(expected) {
            let value = security_event(&security_event_raw(kind.as_raw())).expect("event");
            assert_eq!(value.kind(), *kind);
            assert_eq!(value, expected);
        }
    }

    /// A set identifier, category or detail that the selected alternative
    /// cannot represent is rejected rather than silently dropped.
    #[test]
    fn inapplicable_security_fields_are_rejected_not_erased() {
        let stray = uci(9, b"");
        type Mutate = fn(&mut sys::AmsMelSecurityEventV1);
        let cases: [(u32, Mutate, &str); 7] = [
            (sys::AMS_MEL_SECURITY_EVENT_NONE, |e| e.category = 1, "None"),
            (
                sys::AMS_MEL_SECURITY_EVENT_NONE,
                |e| e.details = text(b"x"),
                "None",
            ),
            (
                sys::AMS_MEL_SECURITY_EVENT_NONE,
                |e| e.mdf_id.descriptive_label = text(b"x"),
                "mdf_id",
            ),
            (
                sys::AMS_MEL_SECURITY_EVENT_INTEGRITY,
                |e| e.service_id.uuid[15] = 1,
                "service_id",
            ),
            (
                sys::AMS_MEL_SECURITY_EVENT_FILE_MANAGEMENT,
                |e| e.subsystem_id.uuid[0] = 1,
                "subsystem_id",
            ),
            (
                sys::AMS_MEL_SECURITY_EVENT_KEY_MANAGEMENT,
                |e| e.mdf_id.uuid[7] = 1,
                "mdf_id",
            ),
            (
                sys::AMS_MEL_SECURITY_EVENT_SANITIZATION,
                |e| e.service_id.descriptive_label = text(b"x"),
                "service_id",
            ),
        ];
        for (kind, mutate, needle) in cases {
            let mut event = security_event_raw(kind);
            mutate(&mut event);
            let error = security_event(&event).expect_err("inapplicable field");
            assert_eq!(error.kind(), &ErrorKind::ProtocolInconsistency);
            assert!(
                error.diagnostic().is_some_and(|text| text.contains(needle)),
                "{error}"
            );
        }
        // A UUID-only identifier is still a set identifier.
        let mut event = security_event_raw(sys::AMS_MEL_SECURITY_EVENT_SYSTEM);
        event.mdf_id = stray;
        assert!(security_event(&event).is_err());
    }

    /// A non-null pointer that is deliberately misaligned for 8-byte types.
    /// It is only ever rejected by `checked_span`, never dereferenced.
    fn misaligned() -> *const u8 {
        (NonNull::<u64>::dangling().as_ptr() as usize + 1) as *const u8
    }

    fn dangling() -> *const u8 {
        NonNull::<u64>::dangling().as_ptr().cast()
    }

    type SpanMutate = fn(&mut Fixture, *const u8, usize);

    fn span_cases() -> [(u32, &'static str, SpanMutate); 10] {
        [
            (1, "MFA_Status.components", |f, p, n| {
                f.mfa.components = sys::AmsMelMfaComponentSpanV1 {
                    data: p.cast(),
                    size: n,
                }
            }),
            (2, "BIT_Status.active_bits", |f, p, n| {
                f.bit.active_bits = sys::AmsMelActiveBitSpanV1 {
                    data: p.cast(),
                    size: n,
                }
            }),
            (2, "CompletedBIT.bit_items", |f, p, n| {
                f.completed[0].bit_items = sys::AmsMelCompletedBitItemSpanV1 {
                    data: p.cast(),
                    size: n,
                }
            }),
            (2, "Fault.fault_data", |f, p, n| {
                f.faults[0].fault_data = sys::AmsMelFaultDataSpanV1 {
                    data: p.cast(),
                    size: n,
                }
            }),
            (2, "FaultAmbiguityGroup.component_ids", |f, p, n| {
                f.groups[0].component_ids = sys::AmsMelUciIdSpanV1 {
                    data: p.cast(),
                    size: n,
                }
            }),
            (3, "SubsystemStatusResp.subsystems", |f, p, n| {
                f.subsystem.subsystems = sys::AmsMelIrSubsystemDepInfoSpanV1 {
                    data: p.cast(),
                    size: n,
                }
            }),
            (3, "SubsystemStatusResp.csci", |f, p, n| {
                f.subsystem.csci = sys::AmsMelIrSubsystemCsciInfoSpanV1 {
                    data: p.cast(),
                    size: n,
                }
            }),
            (4, "DiscreteStatus", |f, p, n| {
                f.discrete_span = sys::AmsMelNameValuePairSpanV1 {
                    data: p.cast(),
                    size: n,
                }
            }),
            (5, "SecurityAudit.artifacts", |f, p, n| {
                f.security.artifacts = sys::AmsMelSecurityArtifactSpanV1 {
                    data: p.cast(),
                    size: n,
                }
            }),
            (6, "MFA_StatusDetailed", |f, p, n| {
                f.detailed_span = sys::AmsMelNameValuePairSpanV1 {
                    data: p.cast(),
                    size: n,
                }
            }),
        ]
    }

    #[test]
    fn null_misaligned_and_impossible_spans_are_rejected_in_every_collection() {
        for (kind, field, mutate) in span_cases() {
            for (pointer, size, message) in [
                (ptr::null(), 1, "span has a null pointer with nonzero size"),
                (misaligned(), 1, "span is misaligned"),
                (dangling(), usize::MAX, "span length is too large"),
            ] {
                let mut fx = Fixture::new();
                mutate(&mut fx, pointer, size);
                failure(
                    fx.convert(kind),
                    ErrorKind::ProtocolInconsistency,
                    &format!("{field} {message}"),
                );
            }
        }
    }

    #[test]
    fn null_and_impossible_text_views_are_rejected() {
        let mut fx = Fixture::new();
        fx.mfa.about.hardware_version = sys::AmsMelStringViewV1 {
            data: ptr::null(),
            size: 3,
        };
        failure(
            fx.convert(1),
            ErrorKind::ProtocolInconsistency,
            "About.hardware_version span has a null pointer",
        );
        let mut fx = Fixture::new();
        fx.mfa.about.model = sys::AmsMelStringViewV1 {
            data: b"x".as_ptr().cast(),
            size: usize::MAX,
        };
        failure(
            fx.convert(1),
            ErrorKind::ProtocolInconsistency,
            "About.model span length is too large",
        );
    }

    #[test]
    fn invalid_utf8_and_embedded_nul_are_rejected_in_nested_text() {
        let bad: [&'static [u8]; 4] = [b"bad\xc3\x28", b"\xff", b"\xed\xa0\x80", b"\xc0\x80"];
        for bytes in bad {
            let mut fx = Fixture::new();
            fx.discrete[1].value = text(bytes);
            failure(fx.convert(4), ErrorKind::InvalidUtf8, "DiscreteStatus");
        }
        let mut fx = Fixture::new();
        fx.fault_data[0].units = text(b"deg\0C");
        failure(
            fx.convert(2),
            ErrorKind::ProtocolInconsistency,
            "embedded NUL in FaultData.units",
        );
        let mut fx = Fixture::new();
        fx.group_tests[0] = uci(2, b"\xe2\x82");
        failure(
            fx.convert(2),
            ErrorKind::InvalidUtf8,
            "FaultAmbiguityGroup.diagnostic_test_ids",
        );
        let mut fx = Fixture::new();
        fx.security.event.details = text(b"a\0");
        failure(
            fx.convert(5),
            ErrorKind::ProtocolInconsistency,
            "embedded NUL in SecurityEvent.details",
        );
    }

    /// A failure in the last element of the deepest collection rejects the
    /// whole event; nothing earlier is returned.
    #[test]
    fn deeply_nested_conversion_failures_reject_the_whole_event() {
        let mut fx = Fixture::new();
        fx.group_components.push(uci(4, b"\xff"));
        fx.groups[0].component_ids = span!(AmsMelUciIdSpanV1, fx.group_components);
        failure(
            fx.convert(2),
            ErrorKind::InvalidUtf8,
            "FaultAmbiguityGroup.component_ids",
        );
        let mut fx = Fixture::new();
        fx.items[1].fail_reason = text(b"\xff");
        failure(
            fx.convert(2),
            ErrorKind::InvalidUtf8,
            "CompletedBIT_Item.fail_reason",
        );
        let mut fx = Fixture::new();
        fx.components[0].installation_details.location.system_name = text(b"\xff");
        failure(
            fx.convert(1),
            ErrorKind::InvalidUtf8,
            "InstallationDetails.location.system_name",
        );
        let mut fx = Fixture::new();
        fx.artifacts[0].associated_id = uci(1, b"\xff");
        failure(
            fx.convert(5),
            ErrorKind::InvalidUtf8,
            "SecurityArtifact.associated_id",
        );
    }

    #[test]
    fn high_bit_ids_and_signed_timestamp_extremes_are_preserved() {
        for (timestamp, completion) in [(i64::MIN, i64::MAX), (i64::MAX, i64::MIN), (-1, 0)] {
            let mut fx = Fixture::new();
            fx.active[0].estimated_completion_time_ns = completion;
            fx.completed[0].time_tag_ns = timestamp;
            fx.faults[0].detection_time_ns = completion;
            fx.subsystem.subsystem_id = u32::MAX;
            fx.subsystem.subsystem_count = 0x8000_0000;
            fx.dependencies[0].subsystem_id = 0xffff_fffe;
            fx.security.event_timestamp_ns = timestamp;
            fx.security.security_event_id.uuid = [0xff; 16];
            let HealthMetadataEvent::BitStatus(bit) = fx.convert(2).expect("BIT") else {
                panic!("wrong family");
            };
            assert_eq!(bit.active_bits[0].estimated_completion_time_ns, completion);
            assert_eq!(bit.completed_bits[0].time_tag_ns, timestamp);
            assert_eq!(bit.faults[0].detection_time_ns, completion);
            let HealthMetadataEvent::SubsystemStatus(status) = fx.convert(3).expect("subsystem")
            else {
                panic!("wrong family");
            };
            assert_eq!(status.subsystem_id, u32::MAX);
            // The reported count is preserved independently of the copied length.
            assert_eq!(status.subsystem_count, 0x8000_0000);
            assert_eq!(status.subsystems.len(), 1);
            assert_eq!(status.csci_count, 0xffff_fffe);
            assert_eq!(status.csci.len(), 1);
            assert_eq!(status.subsystems[0].subsystem_id, 0xffff_fffe);
            let HealthMetadataEvent::SecurityAudit(audit) = fx.convert(5).expect("audit") else {
                panic!("wrong family");
            };
            assert_eq!(audit.event_timestamp_ns, timestamp);
            assert_eq!(audit.security_event_id.uuid(), &[0xff; 16]);
        }
    }

    #[test]
    fn floating_point_special_values_are_copied_bit_for_bit() {
        let nan = f64::from_bits(0x7ff8_0000_dead_beef);
        let specials = [
            nan,
            f64::INFINITY,
            f64::NEG_INFINITY,
            -0.0,
            f64::MIN_POSITIVE / 2.0,
        ];
        for value in specials {
            let mut fx = Fixture::new();
            let component = &mut fx.components[0];
            component.temperature.temperature_c = value;
            component.installation_details.location.offset_z_m = value;
            component.installation_details.orientation.yaw = value;
            component.installation_details.boresight.roll = value;
            fx.active[0].estimated_percent_complete = value;
            let HealthMetadataEvent::MfaStatus(mfa) = fx.convert(1).expect("MFA") else {
                panic!("wrong family");
            };
            let details = &mfa.components[0].installation_details;
            for copied in [
                mfa.components[0].temperature.temperature_c,
                details.location.offset_z_m(),
                details.orientation.yaw,
                details.boresight.roll,
            ] {
                assert_eq!(copied.to_bits(), value.to_bits());
            }
            let HealthMetadataEvent::BitStatus(bit) = fx.convert(2).expect("BIT") else {
                panic!("wrong family");
            };
            let percent = bit.active_bits[0].estimated_percent_complete;
            assert_eq!(percent.to_bits(), value.to_bits());
        }
    }

    // ---- RAII native-event guard, driven through fake native operations ----

    thread_local! {
        static RECORD: Cell<*const sys::AmsMelIrHealthMetadataEventV1> =
            const { Cell::new(ptr::null()) };
        static VIEW_STATUS: Cell<sys::AmsMelStatus> = const { Cell::new(sys::AMS_MEL_OK) };
        static VIEWS: Cell<u32> = const { Cell::new(0) };
        static CLOSES: Cell<u32> = const { Cell::new(0) };
        static CLOSE_KEEPS_SLOT: Cell<bool> = const { Cell::new(false) };
    }

    /// Writes the empty diagnostic that `call_with_diagnostic` requires.
    unsafe fn empty_diagnostic(out: *mut c_char, capacity: usize, required: *mut usize) {
        if !required.is_null() {
            *required = 1;
        }
        if !out.is_null() && capacity > 0 {
            *out = 0;
        }
    }

    unsafe extern "C" fn fake_view(
        event: *const sys::AmsMelIrHealthMetadataEvent,
        out: *mut *const sys::AmsMelIrHealthMetadataEventV1,
        diagnostic: *mut c_char,
        capacity: usize,
        required: *mut usize,
    ) -> sys::AmsMelStatus {
        empty_diagnostic(diagnostic, capacity, required);
        assert!(!event.is_null(), "view of a released event");
        VIEWS.with(|views| views.set(views.get() + 1));
        let status = VIEW_STATUS.with(Cell::get);
        if status == sys::AMS_MEL_OK {
            *out = RECORD.with(Cell::get);
        }
        status
    }

    unsafe extern "C" fn fake_close(
        event: *mut *mut sys::AmsMelIrHealthMetadataEvent,
        diagnostic: *mut c_char,
        capacity: usize,
        required: *mut usize,
    ) -> sys::AmsMelStatus {
        empty_diagnostic(diagnostic, capacity, required);
        assert!(!(*event).is_null(), "close of an already released event");
        CLOSES.with(|closes| closes.set(closes.get() + 1));
        if !CLOSE_KEEPS_SLOT.with(Cell::get) {
            *event = ptr::null_mut();
        }
        sys::AMS_MEL_OK
    }

    static FAKE_OPS: NativeEventOps = NativeEventOps {
        view: fake_view,
        close: fake_close,
    };

    /// A live fake event whose view yields `record`; counters are reset.
    fn fake_event(record: *const sys::AmsMelIrHealthMetadataEventV1) -> NativeEvent {
        RECORD.with(|value| value.set(record));
        VIEW_STATUS.with(|value| value.set(sys::AMS_MEL_OK));
        VIEWS.with(|value| value.set(0));
        CLOSES.with(|value| value.set(0));
        CLOSE_KEEPS_SLOT.with(|value| value.set(false));
        let mut event = NativeEvent::new(&FAKE_OPS);
        event.raw = NonNull::dangling().as_ptr();
        event
    }

    fn closes() -> u32 {
        CLOSES.with(Cell::get)
    }

    #[test]
    fn guard_closes_the_native_event_before_returning_the_owned_value() {
        let fx = Fixture::new();
        let record = fx.event(sys::AMS_MEL_IR_HEALTH_METADATA_SECURITY_AUDIT);
        let value = fake_event(&record).into_owned().expect("owned event");
        assert_eq!((VIEWS.with(Cell::get), closes()), (1, 1));
        let expected = fx.convert(5).expect("expected");
        // The owned value borrows nothing from the native storage.
        drop(fx);
        assert_eq!(value, expected);
        assert_eq!(value.kind(), HealthMetadataKind::SecurityAudit);
    }

    #[test]
    fn guard_releases_the_event_exactly_once_on_every_failure_path() {
        let fx = Fixture::new();
        let mut bad_kind = fx.event(1);
        bad_kind.kind = 0;
        let mut deep = Fixture::new();
        deep.items[1].bit_item_name = text(b"\xff");
        let deep_record = deep.event(2);
        let aligned: *const sys::AmsMelIrHealthMetadataEventV1 = &bad_kind;
        let misaligned_record = (aligned as usize + 1) as *const _;
        for (record, kind, needle) in [
            (aligned, ErrorKind::ProtocolInconsistency, "event kind"),
            (
                &deep_record as *const _,
                ErrorKind::InvalidUtf8,
                "CompletedBIT_Item.name",
            ),
            (
                ptr::null(),
                ErrorKind::ProtocolInconsistency,
                "null pointer",
            ),
            (
                misaligned_record,
                ErrorKind::ProtocolInconsistency,
                "misaligned",
            ),
        ] {
            failure(fake_event(record).into_owned(), kind, needle);
            assert_eq!(closes(), 1, "{needle}");
        }
        // A failed native view still releases the event.
        let event = fake_event(&bad_kind);
        VIEW_STATUS.with(|value| value.set(sys::AMS_MEL_INTERNAL_ERROR));
        let error = event.into_owned().expect_err("view failure");
        assert_eq!(error.kind(), &ErrorKind::InternalError);
        assert_eq!(closes(), 1);
    }

    #[test]
    fn guard_releases_the_event_during_unwind() {
        let fx = Fixture::new();
        let record = fx.event(1);
        let result = panic::catch_unwind(AssertUnwindSafe(|| {
            let _event = fake_event(&record);
            panic!("unwind while a native event is owned");
        }));
        assert!(result.is_err());
        assert_eq!(closes(), 1);
    }

    #[test]
    fn successful_receive_without_an_event_is_rejected_without_native_calls() {
        let mut event = fake_event(ptr::null());
        event.raw = ptr::null_mut();
        failure(
            event.into_owned(),
            ErrorKind::ProtocolInconsistency,
            "receive succeeded without an event",
        );
        assert_eq!((VIEWS.with(Cell::get), closes()), (0, 0));
    }

    #[test]
    fn close_that_keeps_the_handle_is_reported_and_never_repeated() {
        let fx = Fixture::new();
        let record = fx.event(6);
        let event = fake_event(&record);
        CLOSE_KEEPS_SLOT.with(|value| value.set(true));
        failure(
            event.into_owned(),
            ErrorKind::ProtocolInconsistency,
            "event close succeeded without releasing",
        );
        assert_eq!(closes(), 1);
    }

    // ---- Stream owner without a provider ----

    fn null_stream() -> HealthMetadataStream {
        HealthMetadataStream {
            raw: ptr::null_mut(),
            _not_send_sync: Rc::new(()),
        }
    }

    #[test]
    fn closed_stream_operations_fail_without_native_calls_and_close_is_idempotent() {
        let mut stream = null_stream();
        assert!(!stream.is_open());
        for error in [
            stream.receive(0).expect_err("receive on closed stream"),
            stream.counters().expect_err("counters on closed stream"),
        ] {
            assert_eq!(error.kind(), &ErrorKind::ProviderFailed);
            assert_eq!(error.diagnostic(), Some(CLOSED));
        }
        stream.close().expect("close of closed stream");
        stream.close().expect("repeated close");
        drop(stream);
    }

    #[test]
    fn zero_queue_capacity_is_rejected_before_any_native_call() {
        // A null Health owner would be refused natively; the capacity check
        // must fire first so no registration attempt can be consumed.
        let error = HealthMetadataStream::open(ptr::null_mut(), 0).expect_err("zero capacity");
        assert_eq!(error.kind(), &ErrorKind::InvalidArgument);
        assert!(error
            .diagnostic()
            .is_some_and(|text| text.contains("capacity must be nonzero")));
    }

    /// The native facade independently rejects null owners and null event
    /// slots. Production exports only; no provider is loaded.
    #[test]
    fn native_facade_rejects_null_metadata_owners_and_events() {
        let out: *mut c_char = ptr::null_mut();
        let required: *mut usize = ptr::null_mut();
        let mut counters = sys::AmsMelIrC2MetadataCountersV1::default();
        let mut event: *mut sys::AmsMelIrHealthMetadataEvent = ptr::null_mut();
        let mut metadata: *mut sys::AmsMelIrHealthMetadata = ptr::null_mut();
        let mut view: *const sys::AmsMelIrHealthMetadataEventV1 = ptr::null();
        let error = HealthMetadataStream::open(ptr::null_mut(), 1).expect_err("null Health owner");
        assert_eq!(error.kind(), &ErrorKind::InvalidArgument);
        // SAFETY: null owners and diagnostics are documented inputs; every
        // output is initially null or default writable storage.
        unsafe {
            assert_eq!(
                sys::ams_mel_ir_health_metadata_receive(
                    ptr::null_mut(),
                    0,
                    &mut event,
                    out,
                    0,
                    required
                ),
                sys::AMS_MEL_INVALID_ARGUMENT
            );
            assert!(event.is_null());
            assert_eq!(
                sys::ams_mel_ir_health_metadata_get_counters(
                    ptr::null(),
                    &mut counters,
                    out,
                    0,
                    required
                ),
                sys::AMS_MEL_INVALID_ARGUMENT
            );
            assert_eq!(
                sys::ams_mel_ir_health_metadata_event_view(
                    ptr::null(),
                    &mut view,
                    out,
                    0,
                    required
                ),
                sys::AMS_MEL_INVALID_ARGUMENT
            );
            assert!(view.is_null());
            // Close of an already-null owner/event is the idempotent no-op
            // that repeated Close relies on.
            assert_eq!(
                sys::ams_mel_ir_health_metadata_close(&mut metadata, out, 0, required),
                sys::AMS_MEL_OK
            );
            assert_eq!(
                sys::ams_mel_ir_health_metadata_event_close(&mut event, out, 0, required),
                sys::AMS_MEL_OK
            );
        }
    }
}
