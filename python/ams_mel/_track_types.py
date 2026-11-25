"""Owned Track callback values; no tracking or response logic."""

from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum

from . import _native
from ._los_quaternion import Quaternion
from ._navigation_types import NorthEastDown
from ._optical_distortion_map import RowCol


class TrackMetadataKind(IntEnum):
    """The four native Track callback alternatives."""

    IRST_TRACK_REPORT = _native.AMS_MEL_IR_TRACK_METADATA_IRST_TRACK_REPORT
    REQUEST_SYSTEM_TRACK_DATA = _native.AMS_MEL_IR_TRACK_METADATA_REQUEST_SYSTEM_TRACK_DATA
    CANDIDATE_OBJECT_MESSAGE = _native.AMS_MEL_IR_TRACK_METADATA_CANDIDATE_OBJECT_MESSAGE
    CANDIDATE_OBJECT_PREPROC_MESSAGE = _native.AMS_MEL_IR_TRACK_METADATA_CANDIDATE_OBJECT_PREPROC_MESSAGE


class IRSTTrackState(IntEnum):
    """Published Track states (no sentinel)."""

    IDLE = _native.AMS_MEL_IR_TRACK_STATE_IDLE
    DETECTED = _native.AMS_MEL_IR_TRACK_STATE_DETECTED
    COAST = _native.AMS_MEL_IR_TRACK_STATE_COAST
    DROPPED = _native.AMS_MEL_IR_TRACK_STATE_DROPPED


class IRSTTrackMode(IntEnum):
    """Published Track modes (no sentinel)."""

    IDLE = _native.AMS_MEL_IR_TRACK_MODE_IDLE
    SCAN = _native.AMS_MEL_IR_TRACK_MODE_SCAN
    STARE = _native.AMS_MEL_IR_TRACK_MODE_STARE


@dataclass(frozen=True)
class IRSTTrackReport:
    """Owned report with independent measured and filtered values and signed ns counts."""

    system_time_ns: int
    activity_id: int
    measured_ned: NorthEastDown
    measured_intensity: float
    measured_snr: float
    filtered_ned: NorthEastDown
    filtered_intensity: float
    filtered_snr: float
    range_m: float
    range_error_m: float
    spatial_extent_rad: float
    track_quality: float
    clutter: float
    age_ns: int
    state: IRSTTrackState
    mode: IRSTTrackMode


@dataclass(frozen=True)
class RequestSystemTrackData:
    """Inbound callback data, not an asynchronous request or an automatic response."""

    system_time_ns: int
    command_id: int
    request_id: int
    track_id: int


@dataclass(frozen=True)
class Directional:
    """Owned XYZ components with no normalization or frame conversion."""

    x: float
    y: float
    z: float


@dataclass(frozen=True)
class Uncertainty:
    """Independent, uninterpreted sensor and platform uint32 bitfields."""

    sensor_uncertainties: int
    platform_uncertainties: int


@dataclass(frozen=True)
class SensorInertialState:
    """Complete inertial state with independent XYZW quaternions and XYZ vectors."""

    system_time_ns: int
    q_xyzw: Quaternion
    q_ecef_xyzw: Quaternion
    sensor_position: Directional
    sensor_velocity: Directional
    uncertainties: Uncertainty


class HotRegionType(IntEnum):
    """Published labels for hot regions."""

    INVALID = _native.AMS_MEL_IR_HOT_REGION_INVALID
    FLARE = _native.AMS_MEL_IR_HOT_REGION_FLARE
    SOLAR = _native.AMS_MEL_IR_HOT_REGION_SOLAR
    MASK = _native.AMS_MEL_IR_HOT_REGION_MASK


@dataclass(frozen=True)
class HotRegion:
    """Complete uint16 size and boundaries, without geometric interpretation."""

    kind: HotRegionType
    size: int
    top: int
    left: int
    right: int
    bottom: int


@dataclass(frozen=True)
class CandidateObjectHeader:
    """Shared header including the received binary32 CFAR value and UTC ns count."""

    number_of_cos: int
    stack_frame_index: int
    cfar: float
    validity_flag_bitfield: int
    tov_utc_ns: int


@dataclass(frozen=True)
class CandidateObject:
    """One candidate with the published measurements and opaque detection category."""

    system_time_ns: int
    detection_category: int
    sensor_index: int
    subpixel: RowCol
    intensity: float
    sensor_relative_unit: Directional
    signal_to_interference_ratio: float
    signal_to_noise_ratio: float


@dataclass(frozen=True)
class CandidateObjectMessage:
    """Owned message with the meaningful candidate prefix and all hot regions."""

    header: CandidateObjectHeader
    inertial_state: SensorInertialState
    hot_regions: tuple[HotRegion, ...]
    candidate_objects: tuple[CandidateObject, ...]


@dataclass(frozen=True)
class CandidateObjectPreProc:
    """One PreProc entry with independent inertial state and a signed 3×3 patch."""

    system_time_ns: int
    detection_category: int
    sensor_index: int
    subpixel: RowCol
    intensity: float
    sensor_relative_unit: Directional
    signal_to_interference_ratio: float
    signal_to_noise_ratio: float
    candidate_object_with_background: tuple[
        tuple[int, int, int], tuple[int, int, int], tuple[int, int, int]
    ]
    clutter: float
    candidate_object_quality: float
    sir_delta: float
    inertial_state: SensorInertialState
    edge: bool
    az_sigma: float
    el_sigma: float
    background_normalizer: float


@dataclass(frozen=True)
class CandidateObjectPreProcMessage:
    """Owned message; the vector length is independent of the header count."""

    header: CandidateObjectHeader
    inertial_state: SensorInertialState
    hot_regions: tuple[HotRegion, ...]
    candidate_object_preprocs: tuple[CandidateObjectPreProc, ...]


@dataclass(frozen=True)
class TrackMetadataEvent:
    """One owned callback snapshot with an exact kind/payload type match."""

    kind: TrackMetadataKind
    payload: IRSTTrackReport | RequestSystemTrackData | CandidateObjectMessage | CandidateObjectPreProcMessage

    def __post_init__(self) -> None:
        if type(self.kind) is not TrackMetadataKind:
            raise TypeError("kind must be TrackMetadataKind")
        expected = (IRSTTrackReport, RequestSystemTrackData,
                    CandidateObjectMessage, CandidateObjectPreProcMessage)[self.kind - 1]
        if type(self.payload) is not expected:
            raise TypeError(f"{self.kind.name} payload must be {expected.__name__}")


@dataclass(frozen=True)
class TrackMetadataCounters:
    """Saturating native uint64 counters for the shared Track metadata FIFO."""

    events_received: int
    events_dropped_queue_full: int
    malformed_or_unsupported: int