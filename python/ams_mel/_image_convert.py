"""Checked active-payload deep copies; borrowed storage never escapes."""

from __future__ import annotations

import ctypes
import sys

from . import _native
from ._errors import ErrorKind, _local_error
from ._ffi import _checked_span, _checked_span_address, _copy_bool
from ._health_types import Euler
from ._image_types import (
    AzimuthElevation, BadPixel, BadPixelList, BadPixelReason,
    ImageMetadataEvent, ImageMetadataKind, LineOfSightEuler,
    LineOfSightReport, NavigationResponse,
)


def _pixel_from_raw(raw: _native.IrBadPixelV1) -> BadPixel:
    try:
        reason = BadPixelReason(int(raw.reason))
    except ValueError:
        raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                           "unpublished BadPixelReason") from None
    return BadPixel(int(raw.row), int(raw.column), reason)


def _euler(raw: _native.EulerV1) -> Euler:
    return Euler(float(raw.roll), float(raw.pitch), float(raw.yaw))


def _az_el(raw: _native.IrAzElV1) -> AzimuthElevation:
    return AzimuthElevation(float(raw.azimuth_rad), float(raw.elevation_rad))


def _event_from_raw(raw: _native.IrImageMetadataEventV1) -> ImageMetadataEvent:
    try:
        kind = ImageMetadataKind(int(raw.kind))
    except ValueError:
        raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                           "unpublished ImageMetadataKind") from None
    # Never inspect an inactive record, including its pointers or enum fields.
    if kind is ImageMetadataKind.BAD_PIXEL_LIST:
        value = raw.bad_pixel_list
        span = value.pixels
        address = _checked_span_address(span.data, span.size, _native.IrBadPixelV1, "bad pixels")
        # Besides Py_ssize_t, the entire span must fit the native address space.
        if address is not None and int(span.size) * ctypes.sizeof(_native.IrBadPixelV1) > (
                (1 << (8 * ctypes.sizeof(ctypes.c_void_p))) - address):
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY, "bad pixel span address overflow")
        if int(span.size) > sys.maxsize // ctypes.sizeof(ctypes.py_object):
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY, "bad pixel tuple size overflow")
        payload = BadPixelList(int(value.reported_size), int(value.reported_count),
                               _checked_span(span.data, span.size, _native.IrBadPixelV1,
                                             _pixel_from_raw, "bad pixels"))
    elif kind is ImageMetadataKind.LINE_OF_SIGHT_REPORT:
        value = raw.line_of_sight_report
        payload = LineOfSightReport(
            int(value.system_time_ns), _az_el(value.pointing_angle),
            _az_el(value.pointing_angle_rates), _copy_bool(value.at_speed, "at_speed"),
            _copy_bool(value.in_tolerance, "in_tolerance"), _euler(value.platform_attitude),
            int(value.validity_flag_bitfield), float(value.image_rotation_rad))
    elif kind is ImageMetadataKind.LINE_OF_SIGHT_EULER:
        value = raw.line_of_sight_euler
        payload = LineOfSightEuler(int(value.system_time_ns), _euler(value.attitude),
                                   _euler(value.attitude_rates))
    else:
        value = raw.navigation_response
        payload = NavigationResponse(int(value.system_time_ns), int(value.command_id),
                                     int(value.request_id))
    return ImageMetadataEvent(kind, payload)
