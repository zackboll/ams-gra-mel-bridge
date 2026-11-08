"""Owned C2 callback values; BIT status uses the canonical Health models."""

from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum
from typing import Union

from ._health_types import BitStatus
from ._types import CommsTestReport, UciId


class C2MetadataKind(IntEnum):
    COMMAND_STATUS = 1
    BIT_CONFIGURATION = 2
    BIT_STATUS = 3
    CHANNEL_COMMS_TEST = 4


class C2CommandState(IntEnum):
    """Command-message state: ACCEPTED does not mean physical completion."""

    NOT_SET = 0
    RECEIVED = 1
    ACCEPTED = 2
    REJECTED = 3
    CANCELLED = 4


class C2CannotComply(IntEnum):
    """Exact published reason domain; UNKNOWN is not an unpublished raw value."""

    NOT_SET = 0
    CONSTRAINT_ATTEMPTS = 1
    CONSTRAINT_ENDURANCE = 2
    CONSTRAINT_CLASSIFICATION = 3
    CONSTRAINT_FOR_FOV_LIMIT = 4
    CONSTRAINT_GATING = 5
    CONSTRAINT_MANEUVER_LIMIT = 6
    CONSTRAINT_OP = 7
    CONSTRAINT_OCCLUSION = 8
    CAPABILITY_RANGE = 9
    CAPABILITY_PERFORMANCE = 10
    CONSTRAINT_RF = 11
    CONSTRAINT_ROUTE = 12
    CONSTRAINT_SAFETY = 13
    CONSTRAINT_TARGET_ANGLE = 14
    CONSTRAINT_TIME = 15
    CONSTRAINT_SYSTEM = 16
    INFEASIBLE_ROUTE = 17
    MISSION_EVENT = 18
    STATE_OR_SETTINGS = 19
    STATE_OR_SETTINGS_CHANGE = 20
    SYSTEM_UNAVAILABLE = 21
    SYSTEM_FAULT = 22
    SYSTEM_CONFLICT = 23
    SUBSYSTEM_UNAVAILABLE = 24
    SUBSYSTEM_FAULT = 25
    CAPABILITY_FAULT = 26
    CAPABILITY_PRECEDENCE = 27
    CAPABILITY_UNAVAILABLE = 28
    INSUFFICIENT_RESOURCES = 29
    RANKING = 30
    WEATHER = 31
    INELIGIBLE_CONTROL_SOURCE = 32
    DEPENDENCY_PREDECESSOR = 33
    DEPENDENCY_ALL_OR_NOTHING = 34
    DEPENDENCY_EITHER_OR = 35
    INIT_CRITERIA_NOT_MET = 36
    UNKNOWN_ID = 37
    INVALID_INPUT_PARAMETER = 38
    INPUT_OTHER = 39
    MDF_ACTIVATION_ERROR = 40
    MULTIPLE = 41
    CANCELLED = 42
    OTHER = 43
    UNKNOWN = 44
    ABORTED = 45
    ALIGNMENT_MANEUVER = 46


class C2BitControlInterface(IntEnum):
    """Pinned names/numbers, without resolving contradictory upstream prose."""

    NOT_SET = 0
    SUBSYSTEM_BIT_COMMAND = 1
    SUBSYSTEM_STATE_COMMAND = 2
    SUBSYSTEM_INITIATED = 3


@dataclass(frozen=True)
class C2CommandStatus:
    command_id: int
    state: C2CommandState
    reason: C2CannotComply
    reason_description: str


@dataclass(frozen=True)
class C2BitType:
    bit_id: UciId
    accepted_interface: C2BitControlInterface
    bit_item_names: tuple[str, ...]
    subsystem_component_ids: tuple[UciId, ...]
    expected_duration_ns: int


@dataclass(frozen=True)
class C2BitConfiguration:
    bit_types: tuple[C2BitType, ...]


_C2Payload = Union[C2CommandStatus, C2BitConfiguration, BitStatus, CommsTestReport]


@dataclass(frozen=True)
class C2MetadataEvent:
    kind: C2MetadataKind
    payload: _C2Payload

    def __post_init__(self) -> None:
        if type(self.kind) is not C2MetadataKind:
            raise TypeError("kind must be C2MetadataKind")
        expected = (C2CommandStatus, C2BitConfiguration, BitStatus, CommsTestReport)[
            self.kind.value - 1]
        if type(self.payload) is not expected:
            raise TypeError(f"{self.kind.name} requires {expected.__name__}")


@dataclass(frozen=True)
class C2MetadataCounters:
    events_received: int
    events_dropped_queue_full: int
    malformed_or_unsupported: int