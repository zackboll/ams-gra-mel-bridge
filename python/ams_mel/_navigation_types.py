"""Complete, owned navigation inputs; no estimation or physical validation."""

from __future__ import annotations

from dataclasses import dataclass, fields
from enum import IntEnum

from . import _native
from ._errors import ErrorKind, _local_error
from ._health_types import Euler
from ._image_types import NavigationResponse
from ._types import MelErrorCode
from ._validation import _validate_double, _validate_int64


class PositionSolutionState(IntEnum):
    """Published states, not an ordered navigation-quality scale."""

    NOT_SET = 0
    ALIGNING = 1
    FREE_INERTIAL = 2
    GPS = 3
    BLENDED = 4


@dataclass(frozen=True)
class NorthEastDown:
    north: float
    east: float
    down: float

    def __post_init__(self) -> None:
        _double_fields(self, NorthEastDown)


@dataclass(frozen=True)
class AttitudeRate:
    """Independent signed ns timestamp; no epoch or rate units inferred."""

    attitude_rate: Euler
    attitude_rate_time_ns: int

    def __post_init__(self) -> None:
        _euler(self.attitude_rate)
        _validate_int64(self.attitude_rate_time_ns, "attitude_rate_time_ns")


@dataclass(frozen=True)
class PositionVelocityCovariance:
    """All 18 independent published terms; no units or symmetry inferred."""

    position_position_pn_pn: float
    position_position_pn_pe: float
    position_position_pn_pd: float
    position_position_pe_pe: float
    position_position_pe_pd: float
    position_position_pd_pd: float
    position_velocity_pn_vn: float
    position_velocity_pn_ve: float
    position_velocity_pn_vd: float
    position_velocity_pe_ve: float
    position_velocity_pe_vd: float
    position_velocity_pd_vd: float
    velocity_velocity_vn_vn: float
    velocity_velocity_vn_ve: float
    velocity_velocity_vn_vd: float
    velocity_velocity_ve_ve: float
    velocity_velocity_ve_vd: float
    velocity_velocity_vd_vd: float

    def __post_init__(self) -> None:
        _double_fields(self, PositionVelocityCovariance)


@dataclass(frozen=True)
class NavigationReport:
    """Complete untransformed MEL report. Every input must be specified."""

    system_time_ns: int
    state: PositionSolutionState
    latitude_rad: float
    longitude_rad: float
    altitude_m: float
    attitude: Euler
    attitude_rate: AttitudeRate
    speed: NorthEastDown
    acceleration: NorthEastDown
    wander_angle_rad: float
    magnetic_heading: float
    altitude_msl: float
    position_velocity_covariance_uncertainty: PositionVelocityCovariance

    def __post_init__(self) -> None:
        _report_to_raw(self)


@dataclass(frozen=True)
class NavigationCompleted:
    response: NavigationResponse


@dataclass(frozen=True)
class NavigationRejected:
    code: MelErrorCode
    description: str


def _record(value: object, expected: type) -> None:
    if type(value) is not expected:
        raise _local_error(ErrorKind.INVALID_ARGUMENT, f"expected {expected.__name__}")


def _double_fields(value: object, expected: type) -> None:
    _record(value, expected)
    for field in fields(expected):
        _validate_double(getattr(value, field.name), field.name)


def _euler(value: Euler) -> _native.EulerV1:
    _record(value, Euler)
    return _native.EulerV1(
        _validate_double(value.roll, "roll"),
        _validate_double(value.pitch, "pitch"),
        _validate_double(value.yaw, "yaw"),
    )


def _ned(value: NorthEastDown) -> _native.NorthEastDownV1:
    _record(value, NorthEastDown)
    return _native.NorthEastDownV1(
        _validate_double(value.north, "north"),
        _validate_double(value.east, "east"),
        _validate_double(value.down, "down"),
    )


def _report_to_raw(report: NavigationReport) -> _native.NavigationReportV1:
    # Revalidate the entire graph, including objects mutated through
    # object.__setattr__ or object.__delattr__, before any native admission.
    try:
        return _checked_report_to_raw(report)
    except AttributeError as error:
        raise _local_error(ErrorKind.INVALID_ARGUMENT, "incomplete NavigationReport graph") from error


def _checked_report_to_raw(report: NavigationReport) -> _native.NavigationReportV1:
    _record(report, NavigationReport)
    _validate_int64(report.system_time_ns, "system_time_ns")
    if (type(report.state) is not PositionSolutionState
            or not any(report.state is member for member in PositionSolutionState)):
        raise _local_error(ErrorKind.INVALID_ARGUMENT, "state must be a published PositionSolutionState")
    rate = report.attitude_rate
    _record(rate, AttitudeRate)
    _validate_int64(rate.attitude_rate_time_ns, "attitude_rate_time_ns")
    covariance = report.position_velocity_covariance_uncertainty
    _double_fields(covariance, PositionVelocityCovariance)
    # Explicit named mapping in published constructor order. No anonymous
    # matrix, inferred counterparts, normalization or covariance conditioning.
    return _native.NavigationReportV1(
        system_time_ns=report.system_time_ns,
        state=int(report.state),
        latitude_rad=_validate_double(report.latitude_rad, "latitude_rad"),
        longitude_rad=_validate_double(report.longitude_rad, "longitude_rad"),
        altitude_m=_validate_double(report.altitude_m, "altitude_m"),
        attitude=_euler(report.attitude),
        attitude_rate=_native.AttitudeRateV1(_euler(rate.attitude_rate), rate.attitude_rate_time_ns),
        speed=_ned(report.speed),
        acceleration=_ned(report.acceleration),
        wander_angle_rad=_validate_double(report.wander_angle_rad, "wander_angle_rad"),
        magnetic_heading=_validate_double(report.magnetic_heading, "magnetic_heading"),
        altitude_msl=_validate_double(report.altitude_msl, "altitude_msl"),
        position_velocity_covariance_uncertainty=_native.PositionVelocityCovarianceV1(
            position_position_pn_pn=covariance.position_position_pn_pn,
            position_position_pn_pe=covariance.position_position_pn_pe,
            position_position_pn_pd=covariance.position_position_pn_pd,
            position_position_pe_pe=covariance.position_position_pe_pe,
            position_position_pe_pd=covariance.position_position_pe_pd,
            position_position_pd_pd=covariance.position_position_pd_pd,
            position_velocity_pn_vn=covariance.position_velocity_pn_vn,
            position_velocity_pn_ve=covariance.position_velocity_pn_ve,
            position_velocity_pn_vd=covariance.position_velocity_pn_vd,
            position_velocity_pe_ve=covariance.position_velocity_pe_ve,
            position_velocity_pe_vd=covariance.position_velocity_pe_vd,
            position_velocity_pd_vd=covariance.position_velocity_pd_vd,
            velocity_velocity_vn_vn=covariance.velocity_velocity_vn_vn,
            velocity_velocity_vn_ve=covariance.velocity_velocity_vn_ve,
            velocity_velocity_vn_vd=covariance.velocity_velocity_vn_vd,
            velocity_velocity_ve_ve=covariance.velocity_velocity_ve_ve,
            velocity_velocity_ve_vd=covariance.velocity_velocity_ve_vd,
            velocity_velocity_vd_vd=covariance.velocity_velocity_vd_vd,
        ),
    )
