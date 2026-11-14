"""Owned values for the four ordinary Image metadata callback families."""

from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum
from typing import Union

from ._health_types import Euler


class ImageMetadataKind(IntEnum):
    BAD_PIXEL_LIST = 1
    LINE_OF_SIGHT_REPORT = 2
    LINE_OF_SIGHT_EULER = 3
    NAVIGATION_RESPONSE = 4


class BadPixelReason(IntEnum):
    UNKNOWN = 0


@dataclass(frozen=True)
class BadPixel:
    """Unrenumbered uint32 coordinates; no coordinate origin is inferred."""

    row: int
    column: int
    reason: BadPixelReason


@dataclass(frozen=True)
class BadPixelList:
    """Ordered pixels including duplicates; reported fields are independent."""

    reported_size: int
    reported_count: int
    pixels: tuple[BadPixel, ...]

    def __post_init__(self) -> None:
        if type(self.pixels) is not tuple or not all(type(p) is BadPixel for p in self.pixels):
            raise TypeError("pixels must be a tuple of BadPixel values")


@dataclass(frozen=True)
class AzimuthElevation:
    """Radians for angles; upstream specifies no time denominator for rates."""

    azimuth_rad: float
    elevation_rad: float


@dataclass(frozen=True)
class LineOfSightReport:
    """Complete pointing report; timestamps have no inferred epoch or clock."""

    system_time_ns: int
    pointing_angle: AzimuthElevation
    pointing_angle_rates: AzimuthElevation
    at_speed: bool
    in_tolerance: bool
    platform_attitude: Euler
    validity_flag_bitfield: int
    image_rotation_rad: float


@dataclass(frozen=True)
class LineOfSightEuler:
    """Distinct untransformed attitude and attitude-rate Euler triples."""

    system_time_ns: int
    attitude: Euler
    attitude_rates: Euler


@dataclass(frozen=True)
class NavigationResponse:
    """Owned NavigationReportResp payload, not itself a completion status.

    Received independently in Image metadata or inside NavigationCompleted.
    """

    system_time_ns: int
    command_id: int
    request_id: int


_ImagePayload = Union[BadPixelList, LineOfSightReport, LineOfSightEuler, NavigationResponse]


@dataclass(frozen=True)
class ImageMetadataEvent:
    kind: ImageMetadataKind
    payload: _ImagePayload

    def __post_init__(self) -> None:
        if type(self.kind) is not ImageMetadataKind:
            raise TypeError("kind must be ImageMetadataKind")
        expected = (BadPixelList, LineOfSightReport, LineOfSightEuler,
                    NavigationResponse)[self.kind.value - 1]
        if type(self.payload) is not expected:
            raise TypeError(f"{self.kind.name} requires {expected.__name__}")


@dataclass(frozen=True)
class ImageMetadataCounters:
    events_received: int
    events_dropped_queue_full: int
    malformed_or_unsupported: int
