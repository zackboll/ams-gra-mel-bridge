"""Copy only the selected v3 payload while its native event is alive."""

from __future__ import annotations

from . import _native
from ._errors import ErrorKind, _local_error
from ._ffi import _checked_span, _copy_bool
from ._health_convert import _enum
from ._los_quaternion import Quaternion
from ._navigation_types import NorthEastDown
from ._optical_distortion_map import _copy_row_col
from ._track_types import (
    CandidateObject, CandidateObjectHeader, CandidateObjectMessage,
    CandidateObjectPreProc, CandidateObjectPreProcMessage, Directional,
    HotRegion, HotRegionType, IRSTTrackMode, IRSTTrackReport, IRSTTrackState,
    RequestSystemTrackData, SensorInertialState, TrackMetadataEvent,
    TrackMetadataKind, Uncertainty,
)


def _ned(raw: _native.NorthEastDownV1) -> NorthEastDown:
    return NorthEastDown(raw.north, raw.east, raw.down)


def _report(raw: _native.IrTrackReportV1) -> IRSTTrackReport:
    return IRSTTrackReport(
        system_time_ns=raw.system_time_ns, activity_id=raw.activity_id,
        measured_ned=_ned(raw.measured_ned), measured_intensity=raw.measured_intensity,
        measured_snr=raw.measured_snr, filtered_ned=_ned(raw.filtered_ned),
        filtered_intensity=raw.filtered_intensity, filtered_snr=raw.filtered_snr,
        range_m=raw.range_m, range_error_m=raw.range_error_m,
        spatial_extent_rad=raw.spatial_extent_rad, track_quality=raw.track_quality,
        clutter=raw.clutter, age_ns=raw.age_ns,
        state=_enum(IRSTTrackState, raw.state), mode=_enum(IRSTTrackMode, raw.mode),
    )


def _directional(raw: _native.IrDirectionalV1) -> Directional:
    return Directional(raw.x, raw.y, raw.z)


def _quaternion(raw: _native.IrQuaternionV1) -> Quaternion:
    return Quaternion(raw.x, raw.y, raw.z, raw.w)


def _inertial_state(raw: _native.IrSensorInertialStateV1) -> SensorInertialState:
    return SensorInertialState(
        raw.system_time_ns, _quaternion(raw.q_xyzw), _quaternion(raw.q_ecef_xyzw),
        _directional(raw.sensor_position), _directional(raw.sensor_velocity),
        Uncertainty(raw.uncertainties.sensor_uncertainties,
                    raw.uncertainties.platform_uncertainties),
    )


def _hot_region(raw: _native.IrHotRegionV1) -> HotRegion:
    return HotRegion(_enum(HotRegionType, raw.kind), raw.size, raw.top,
                     raw.left, raw.right, raw.bottom)


def _header(raw: _native.IrCandidateObjectHeaderV1) -> CandidateObjectHeader:
    return CandidateObjectHeader(raw.number_of_cos, raw.stack_frame_index, raw.cfar,
                                 raw.validity_flag_bitfield, raw.tov_utc_ns)


def _span(raw, element_type, convert, field):
    size = raw.size
    return _checked_span(raw.data if size else None, size, element_type, convert, field)


def _candidate_fields(raw: _native.IrCandidateObjectV1 | _native.IrCandidateObjectPreProcV1):
    # The two ABI entries share these eight fields, but are distinct records.
    return (raw.system_time_ns, raw.detection_category, raw.sensor_index,
            _copy_row_col(raw.subpixel), raw.intensity, _directional(raw.sensor_relative_unit),
            raw.signal_to_interference_ratio, raw.signal_to_noise_ratio)


def _candidate(raw: _native.IrCandidateObjectV1) -> CandidateObject:
    return CandidateObject(*_candidate_fields(raw))


def _candidate_message(raw: _native.IrCandidateObjectMessageV1) -> CandidateObjectMessage:
    header = _header(raw.header)
    if (header.number_of_cos > _native.AMS_MEL_IR_MAX_CANDIDATE_OBJECTS
            or raw.candidate_objects.size != header.number_of_cos):
        raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                           "CandidateObjectMessage span must match the fixed-array prefix count")
    return CandidateObjectMessage(
        header, _inertial_state(raw.inertial_state),
        _span(raw.hot_regions, _native.IrHotRegionV1, _hot_region, "candidate hot regions"),
        _span(raw.candidate_objects, _native.IrCandidateObjectV1, _candidate, "candidate objects"),
    )


def _preproc(raw: _native.IrCandidateObjectPreProcV1) -> CandidateObjectPreProc:
    samples = raw.candidate_object_with_background.samples
    return CandidateObjectPreProc(
        *_candidate_fields(raw),
        candidate_object_with_background=(
            (samples[0], samples[1], samples[2]),
            (samples[3], samples[4], samples[5]),
            (samples[6], samples[7], samples[8]),
        ),
        clutter=raw.clutter, candidate_object_quality=raw.candidate_object_quality,
        sir_delta=raw.sir_delta, inertial_state=_inertial_state(raw.inertial_state),
        edge=_copy_bool(raw.edge, "PreProc edge"), az_sigma=raw.az_sigma,
        el_sigma=raw.el_sigma, background_normalizer=raw.background_normalizer,
    )


def _preproc_message(raw: _native.IrCandidateObjectPreProcMessageV1) -> CandidateObjectPreProcMessage:
    # No equality or fixed-array limit applies to this independent vector.
    return CandidateObjectPreProcMessage(
        _header(raw.header), _inertial_state(raw.inertial_state),
        _span(raw.hot_regions, _native.IrHotRegionV1, _hot_region, "PreProc hot regions"),
        _span(raw.candidate_object_preprocs, _native.IrCandidateObjectPreProcV1, _preproc,
              "candidate PreProc entries"),
    )


def _event_from_raw(raw: _native.IrTrackMetadataEventV3) -> TrackMetadataEvent:
    v2 = raw.base
    v1 = v2.base
    kind = _enum(TrackMetadataKind, v1.kind)
    if kind is TrackMetadataKind.IRST_TRACK_REPORT:
        payload = _report(v1.track_report)
    elif kind is TrackMetadataKind.REQUEST_SYSTEM_TRACK_DATA:
        value = v1.request_system_track_data
        payload = RequestSystemTrackData(value.system_time_ns, value.command_id,
                                         value.request_id, value.track_id)
    elif kind is TrackMetadataKind.CANDIDATE_OBJECT_MESSAGE:
        payload = _candidate_message(v2.candidate_object_message)
    else:
        payload = _preproc_message(raw.candidate_object_preproc_message)
    return TrackMetadataEvent(kind, payload)
