"""Owned Health/Status values over the published six-family C ABI."""

from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum
from typing import ClassVar, Union

from ._types import ComponentLocation, MFAState, UciId


class HealthMetadataKind(IntEnum):
    MFA_STATUS = 1
    BIT_STATUS = 2
    SUBSYSTEM_STATUS = 3
    DISCRETE_STATUS = 4
    SECURITY_AUDIT = 5
    MFA_STATUS_DETAILED = 6


class StateTransitionStatus(IntEnum):
    NOT_SET = 0
    NOT_TRANSITIONING = 1
    SHUTTING_DOWN = 2
    TRANSITIONING = 3


class ComponentState(IntEnum):
    NOT_SET = 0
    UNKNOWN = 1
    NOT_INSTALLED = 2
    OFF = 3
    INITIALIZING = 4
    OPERATIONAL = 5
    DEGRADED = 6
    DISABLED = 7
    FAULTED = 8


class TemperatureState(IntEnum):
    NOT_SET = 0
    UNDER_TEMP = 1
    NORMAL = 2
    OVER_TEMP_WARNING = 3
    OVER_TEMP_DEGRADED = 4
    OVER_TEMP_SHUTDOWN = 5


class BitResult(IntEnum):
    NOT_SET = 0
    PASS = 1
    FAIL = 2
    INTERRUPTED = 3
    NOT_TESTED = 4


class FaultSeverity(IntEnum):
    NOT_SET = 0
    NOMINAL = 1
    CAUTION = 2
    WARNING = 3
    FAILED = 4


class FaultState(IntEnum):
    NOT_SET = 0
    SET = 1
    CLEARED = 2
    UNKNOWN = 3


class FailureLevel(IntEnum):
    """Reported categories, not an ordered severity scale."""

    NA = 0
    CRITICAL = 1
    MAJOR = 2
    PARAMETRIC = 3
    INFORMATIONAL = 4
    AVAILABLE = 5
    NOT_PRESENT = 6


class CsciMode(IntEnum):
    UNKNOWN = 0
    UNUSED = 1
    INITIALIZATION = 2
    MAINTENANCE = 3
    IDLE = 4
    OPERATIONAL = 5
    VSA = 6
    QUICK_LOOK = 7
    TRACK = 8
    IMAGING = 9
    NOISE = 10


class SecurityOutcome(IntEnum):
    NOT_SET = 0
    FAILURE = 1
    SUCCESS = 2


class SecuritySeverity(IntEnum):
    NOT_SET = 0
    CRITICAL = 1
    ERROR = 2
    INFORMATIONAL = 3
    WARNING = 4


class SecurityEventKind(IntEnum):
    NONE = 0
    AUTHENTICATION = 1
    INTEGRITY = 2
    FILE_MANAGEMENT = 3
    KEY_MANAGEMENT = 4
    SYSTEM = 5
    SANITIZATION = 6


# Category ordinals are pinned upstream values, reviewed in the Rust binding.
# The C ABI deliberately provides no common category enumeration.
class SecurityAuthenticationCategory(IntEnum):
    NOT_SET = 0
    SUBSYSTEM_AUTHENTICATION = 1
    SERVICE_AUTHENTICATION = 2
    CONFIG_FILE_AUTHENTICATION = 3
    OFP_AUTHENTICATION = 4
    MDF_AUTHENTICATION = 5
    OTHER_FILE_AUTHENTICATION = 6
    OTHER_DEVICE_AUTHENTICATION = 7


class SecurityIntegrityCategory(IntEnum):
    NOT_SET = 0
    CONFIG_FILE_INTEGRITY_CHECK = 1
    BOOTLOADER_INTEGRITY_CHECK = 2
    OFP_INTEGRITY_CHECK = 3
    MDF_INTEGRITY_CHECK = 4
    OTHER_FILE_INTEGRITY_CHECK = 5
    OTHER_INTEGRITY_CHECK = 6


class SecurityFileManagementCategory(IntEnum):
    NOT_SET = 0
    CONFIG_FILE_UPDATE = 1
    OFP_UPDATE = 2
    MDF_UPDATE = 3
    OTHER_FILE_UPDATE = 4


class SecurityKeyManagementCategory(IntEnum):
    NOT_SET = 0
    KEY_LOAD = 1
    KEY_GENERATION = 2
    KEY_UPDATE = 3
    KEY_DELETION = 4
    CERTIFICATE_LOAD = 5
    CERTIFICATE_DELETION = 6


class SecuritySystemCategory(IntEnum):
    NOT_SET = 0
    STARTUP = 1
    SHUTDOWN = 2
    RESET = 3
    FAULT = 4


class SecuritySanitizationCategory(IntEnum):
    NOT_SET = 0
    ZEROIZATION = 1
    SANITIZATION = 2
    CLASSIFIED_DATA_ERASE = 3


@dataclass(frozen=True)
class Euler:
    roll: float
    pitch: float
    yaw: float


@dataclass(frozen=True)
class ForeignKey:
    key: str
    system_name: str


@dataclass(frozen=True)
class InstallationDetails:
    location: ComponentLocation
    orientation: Euler
    boresight: Euler


@dataclass(frozen=True)
class TemperatureStatus:
    temperature_c: float
    state: TemperatureState


@dataclass(frozen=True)
class MfaComponent:
    component_id: UciId
    state: ComponentState
    temperature: TemperatureStatus
    installation_location_id: ForeignKey
    installation_details: InstallationDetails


@dataclass(frozen=True)
class About:
    model: str
    serial_number: str
    software_version: str
    bootloader_software_version: str
    hardware_version: str


@dataclass(frozen=True)
class MfaStatus:
    state: MFAState
    state_description: str
    mode_description: str
    transition_status: StateTransitionStatus
    about: About
    components: tuple[MfaComponent, ...]


@dataclass(frozen=True)
class ActiveBit:
    bit_id: UciId
    estimated_completion_time_ns: int
    estimated_percent_complete: float


@dataclass(frozen=True)
class CompletedBitItem:
    bit_item_name: str
    result: BitResult
    fail_reason: str


@dataclass(frozen=True)
class CompletedBit:
    bit_id: UciId
    time_tag_ns: int
    result: BitResult
    fail_reason: str
    bit_items: tuple[CompletedBitItem, ...]


@dataclass(frozen=True)
class FaultData:
    key: str
    value: str
    format: str
    units: str


@dataclass(frozen=True)
class FaultAmbiguityGroup:
    diagnostic_test_ids: tuple[UciId, ...]
    component_ids: tuple[UciId, ...]


@dataclass(frozen=True)
class Fault:
    fault_id: UciId
    severity: FaultSeverity
    state: FaultState
    fault_data: tuple[FaultData, ...]
    detection_time_ns: int
    fault_code: str
    fault_description: str
    component_ids: tuple[UciId, ...]
    ambiguity_groups: tuple[FaultAmbiguityGroup, ...]


@dataclass(frozen=True)
class BitStatus:
    active_bits: tuple[ActiveBit, ...]
    completed_bits: tuple[CompletedBit, ...]
    faults: tuple[Fault, ...]


@dataclass(frozen=True)
class SubsystemDependency:
    subsystem_id: int
    criticality: int
    failure: FailureLevel


@dataclass(frozen=True)
class CsciVersion:
    source: int
    major_revision: int
    minor_revision: int
    engineering_revision: int


@dataclass(frozen=True)
class CsciInfo:
    csci: str
    mode: CsciMode
    version: CsciVersion
    criticality: int
    failure: FailureLevel
    bit_report: int
    connection_established: bool


@dataclass(frozen=True)
class SubsystemStatus:
    subsystem_id: int
    criticality: int
    status_sequence_number: int
    failure: FailureLevel
    subsystem_count: int
    subsystems: tuple[SubsystemDependency, ...]
    csci_count: int
    csci: tuple[CsciInfo, ...]


@dataclass(frozen=True)
class NameValuePair:
    name: str
    value: str


@dataclass(frozen=True)
class DiscreteStatus:
    pairs: tuple[NameValuePair, ...]


@dataclass(frozen=True)
class MfaStatusDetailed:
    pairs: tuple[NameValuePair, ...]


@dataclass(frozen=True)
class SecurityArtifact:
    component_id: UciId
    associated_id: UciId


@dataclass(frozen=True)
class SecurityNone:
    kind: ClassVar[SecurityEventKind] = SecurityEventKind.NONE


@dataclass(frozen=True)
class SecurityAuthentication:
    category: SecurityAuthenticationCategory
    details: str
    subsystem_id: UciId
    service_id: UciId
    mdf_id: UciId
    kind: ClassVar[SecurityEventKind] = SecurityEventKind.AUTHENTICATION

    def __post_init__(self) -> None:
        if type(self.category) is not SecurityAuthenticationCategory:
            raise TypeError("category must be SecurityAuthenticationCategory")


@dataclass(frozen=True)
class SecurityIntegrity:
    category: SecurityIntegrityCategory
    details: str
    mdf_id: UciId
    kind: ClassVar[SecurityEventKind] = SecurityEventKind.INTEGRITY

    def __post_init__(self) -> None:
        if type(self.category) is not SecurityIntegrityCategory:
            raise TypeError("category must be SecurityIntegrityCategory")


@dataclass(frozen=True)
class SecurityFileManagement:
    category: SecurityFileManagementCategory
    details: str
    mdf_id: UciId
    kind: ClassVar[SecurityEventKind] = SecurityEventKind.FILE_MANAGEMENT

    def __post_init__(self) -> None:
        if type(self.category) is not SecurityFileManagementCategory:
            raise TypeError("category must be SecurityFileManagementCategory")


@dataclass(frozen=True)
class SecurityKeyManagement:
    category: SecurityKeyManagementCategory
    details: str
    kind: ClassVar[SecurityEventKind] = SecurityEventKind.KEY_MANAGEMENT

    def __post_init__(self) -> None:
        if type(self.category) is not SecurityKeyManagementCategory:
            raise TypeError("category must be SecurityKeyManagementCategory")


@dataclass(frozen=True)
class SecuritySystem:
    category: SecuritySystemCategory
    details: str
    kind: ClassVar[SecurityEventKind] = SecurityEventKind.SYSTEM

    def __post_init__(self) -> None:
        if type(self.category) is not SecuritySystemCategory:
            raise TypeError("category must be SecuritySystemCategory")


@dataclass(frozen=True)
class SecuritySanitization:
    category: SecuritySanitizationCategory
    details: str
    kind: ClassVar[SecurityEventKind] = SecurityEventKind.SANITIZATION

    def __post_init__(self) -> None:
        if type(self.category) is not SecuritySanitizationCategory:
            raise TypeError("category must be SecuritySanitizationCategory")


# Private aliases keep exported names limited to their defining class objects.
_SecurityEvent = Union[
    SecurityNone, SecurityAuthentication, SecurityIntegrity,
    SecurityFileManagement, SecurityKeyManagement, SecuritySystem,
    SecuritySanitization,
]


@dataclass(frozen=True)
class SecurityAuditRecord:
    security_event_id: UciId
    event_timestamp_ns: int
    subsystem_id: UciId
    artifacts: tuple[SecurityArtifact, ...]
    event: _SecurityEvent
    outcome: SecurityOutcome
    severity: SecuritySeverity

    def __post_init__(self) -> None:
        if type(self.event) not in (
            SecurityNone, SecurityAuthentication, SecurityIntegrity,
            SecurityFileManagement, SecurityKeyManagement, SecuritySystem,
            SecuritySanitization,
        ):
            raise TypeError("event must be an owned security variant")


_HealthPayload = Union[
    MfaStatus, BitStatus, SubsystemStatus, DiscreteStatus,
    SecurityAuditRecord, MfaStatusDetailed,
]


@dataclass(frozen=True)
class HealthMetadataEvent:
    kind: HealthMetadataKind
    payload: _HealthPayload

    def __post_init__(self) -> None:
        if type(self.kind) is not HealthMetadataKind:
            raise TypeError("kind must be HealthMetadataKind")
        expected = (
            MfaStatus, BitStatus, SubsystemStatus, DiscreteStatus,
            SecurityAuditRecord, MfaStatusDetailed,
        )[self.kind.value - 1]
        if type(self.payload) is not expected:
            raise TypeError(f"{self.kind.name} requires {expected.__name__}")


@dataclass(frozen=True)
class HealthMetadataCounters:
    events_received: int
    events_dropped_queue_full: int
    malformed_or_unsupported: int
