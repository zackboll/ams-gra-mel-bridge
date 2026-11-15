"""PY-MEL-017: compiled ODM contract, owned copies and hostile FFI outputs.

Provider globals, explicit DSO hooks and teardown barriers run in fresh children.
Invalid addresses are only used where structural validation must reject them;
all dereferenced storage and all owner/event handles have native provenance.
"""

from __future__ import annotations

from contextlib import contextmanager, redirect_stdout
import _ctypes
import ctypes as c
import dataclasses
import gc
import inspect
import io
import math
import os
from pathlib import Path
import pickle
import struct
import subprocess
import sys
import tempfile
import threading
import unittest
from unittest import mock
import weakref

import ams_mel as m
from ams_mel import _native as n, _optical_distortion_map as impl
import test_image_stream as frames
from test_ir_health import provider
from test_ir_health_metadata import assert_owned


P = "ams_mel_ir_image_odm_"
RAW = n.IrOpticalDistortionMapV1
PROTOCOL = m.ErrorKind.PROTOCOL_INCONSISTENCY
F = m.ForwardRightDown
R = m.RowCol


def expected(fixture, tag=0):
    """Independent expectations from the compiled C11 ODM fixture contract."""
    big = sys.float_info.max
    values = {
        0: ((0, 0, 0, 0, 0), (0., 0., 0., 0.), (), ()),
        1: ((0xFFFFFFFF, 0, 0x80000001, 1, 0x7FFFFFFF),
            (-0., big, -big, float.fromhex("0x0.0000000000001p-1022")),
            (F(1., 0., 0.), F(.5, -.25, .125), F(.5, -.25, .125), F(-.75, 1e-300, -2.5)),
            (R(0., 639.5), R(479.25, -0.), R(1e300, -1e-300))),
        2: ((640, 480, 33, 25, 20), (-.5, .5, -.375, .375), (F(.25, .5, .75),), ()),
        3: ((1, 2, 3, 4, 5), (1., 2., 3., 4.), (), (R(12.5, 7.25),)),
        4: ((0, 0, 0, 0, 0), (math.inf, -math.inf, math.nan, -0.),
            (F(math.nan, math.inf, -math.inf),), (R(-math.inf, math.nan), R(0., -0.))),
        5: ((5, 5, 5, 5, tag), (-1., 1., -2., 2.), (F(float(tag), 0., 0.),),
            (R(1., 2.), R(3., 4.), R(5., 6.), R(7., 8.))),
        6: ((tag, 6, 6, 6, 6), (-6., 6., -6.5, 6.5),
            (F(float(tag), 1., 2.),), (R(float(tag), 3.),)),
    }
    scalars, extrema, frd, row_col = values[fixture]
    return m.OpticalDistortionMap(*scalars, *extrema, frd, row_col)


def same(test, actual, wanted):
    if dataclasses.is_dataclass(wanted):
        test.assertIs(type(actual), type(wanted))
        for field in dataclasses.fields(wanted):
            same(test, getattr(actual, field.name), getattr(wanted, field.name))
    elif isinstance(wanted, (tuple, list)):
        test.assertIs(type(actual), type(wanted))
        test.assertEqual(len(actual), len(wanted))
        for left, right in zip(actual, wanted):
            same(test, left, right)
    elif isinstance(wanted, float):
        test.assertIs(type(actual), float)
        if math.isnan(wanted):
            test.assertTrue(math.isnan(actual))
        else:
            test.assertEqual(struct.pack("=d", actual), struct.pack("=d", wanted))
    else:
        test.assertEqual(actual, wanted)


def error(test, kind, operation, status=None):
    with test.assertRaises(m.MelError) as caught:
        operation()
    test.assertIs(caught.exception.kind, kind)
    test.assertEqual(caught.exception.native_status, status)
    return caught.exception


def diagnostic(output, capacity, required, message=b""):
    if output is not None and capacity:
        c.memmove(output, message + b"\0", min(capacity, len(message) + 1))
    if required is not None:
        c.cast(required, c.POINTER(c.c_size_t))[0] = len(message) + 1


@contextmanager
def owners(scenario="odm-sync", capacity=16):
    with m.Session.open(provider(), scenario, "") as session:
        with session.open_image_stream(frames.ImageStreamTests.config()) as image:
            if capacity is None:
                yield session, image
            else:
                with image.open_optical_distortion_map(capacity) as stream:
                    yield session, image, stream


@contextmanager
def inject_view(pointer):
    def view(event, output, diag, capacity, required):
        c.cast(output, c.POINTER(c.POINTER(RAW)))[0] = pointer
        diagnostic(diag, capacity, required)
        return n.AMS_MEL_OK
    with mock.patch.object(n, P + "event_view", side_effect=view):
        yield


class OpticalDistortionMapTests(unittest.TestCase):
    def test_public_exports_signatures_identity_pickle_and_immutability(self):
        signatures = {
            m.ImageStream.open_optical_distortion_map:
                "(self, queue_capacity: 'int' = 16) -> 'OpticalDistortionMapStream'",
            m.OpticalDistortionMapStream.receive:
                "(self, timeout_ms: 'int' = 0) -> 'OpticalDistortionMap'",
            m.OpticalDistortionMapStream.counters: "(self) -> 'ImageMetadataCounters'",
            m.OpticalDistortionMapStream.close: "(self) -> 'None'",
            F: "(forward: 'float', right: 'float', down: 'float') -> None",
            R: "(row: 'float', column: 'float') -> None",
            m.OpticalDistortionMap:
                "(width: 'int', height: 'int', right_size: 'int', down_size: 'int', "
                "pixels_per_odm: 'int', right_min: 'float', right_max: 'float', "
                "down_min: 'float', down_max: 'float', frd: 'tuple[ForwardRightDown, ...]', "
                "row_col: 'tuple[RowCol, ...]') -> None",
        }
        for function, signature in signatures.items():
            self.assertEqual(str(inspect.signature(function)), signature)
        for cls in (F, R, m.OpticalDistortionMap, m.OpticalDistortionMapStream,
                    m.OpticalDistortionMapOpenError):
            self.assertIs(getattr(impl, cls.__name__), cls)
            self.assertEqual(cls.__module__, "ams_mel")
            self.assertEqual(m.__all__.count(cls.__name__), 1)
        self.assertIsInstance(inspect.getattr_static(m.OpticalDistortionMapStream, "is_open"), property)
        with self.assertRaises(TypeError):
            m.OpticalDistortionMapStream()
        for value in (F(1., 2., 3.), R(4., 5.), *(expected(i) for i in range(7))):
            same(self, pickle.loads(pickle.dumps(value)), value)
            assert_owned(self, value)
            with self.assertRaises(dataclasses.FrozenInstanceError):
                setattr(value, dataclasses.fields(value)[0].name, 7)

    def test_compiled_sync_full_snapshot_is_released_before_publication(self):
        with owners() as (_, image, stream):
            with mock.patch.object(n, P + "event_close", wraps=n.ams_mel_ir_image_odm_event_close) as close:
                values = tuple(stream.receive() for _ in range(4))
                self.assertEqual(close.call_count, 4)
                self.assertTrue(all(not call.args[0]._obj.value for call in close.call_args_list))
            for fixture, value in enumerate(values):
                same(self, value, expected(fixture))
                assert_owned(self, value)
            self.assertEqual(stream.counters(), m.ImageMetadataCounters(4, 0, 0))
            error(self, m.ErrorKind.TIMEOUT, stream.receive, n.AMS_MEL_TIMEOUT)
            self.assertTrue(image.is_open)

    def test_all_uint32_fields_and_duplicate_row_col_without_dimension_constraints(self):
        raw = RAW()
        frd = (n.IrForwardRightDownV1 * 2)(*(n.IrForwardRightDownV1(7., -8., 9.) for _ in range(2)))
        rc = (n.IrRowColV1 * 3)(*(n.IrRowColV1(-0., 2.) for _ in range(3)))
        raw.frd = n.IrForwardRightDownSpanV1(frd, 2)
        raw.row_col = n.IrRowColSpanV1(rc, 3)
        for number in (0, 0xFFFFFFFF):
            for field in ("width", "height", "right_size", "down_size", "pixels_per_odm"):
                setattr(raw, field, number)
            with owners() as (_, _, stream), inject_view(c.pointer(raw)):
                value = stream.receive()
            self.assertEqual((value.width, value.height, value.right_size, value.down_size,
                              value.pixels_per_odm), (number,) * 5)
            same(self, value.frd, (F(7., -8., 9.),) * 2)
            same(self, value.row_col, (R(-0., 2.),) * 3)
        frd[0].forward = 99.
        rc[0].row = 99.
        self.assertEqual(value.frd[0].forward, 7.)
        same(self, value.row_col[0].row, -0.)

    def test_invalid_capacity_does_not_consume_one_shot_or_call_native(self):
        with owners(capacity=None) as (_, image):
            with mock.patch.object(n, P + "open") as opened:
                for capacity in (0, -1, 1 << (8 * c.sizeof(c.c_size_t)), True, 1.5, None, "1"):
                    failure = error(self, m.ErrorKind.INVALID_ARGUMENT,
                                    lambda: image.open_optical_distortion_map(capacity))
                    self.assertIsInstance(failure, m.OpticalDistortionMapOpenError)
                    self.assertIsNone(failure.registration)
                opened.assert_not_called()
            with mock.patch.object(n, "ams_mel_ir_stream_start") as start, \
                    mock.patch.object(n, "ams_mel_ir_image_metadata_open") as base, \
                    mock.patch.object(n, "ams_mel_ir_image_nuc_open") as nuc, \
                    mock.patch.object(n, "ams_mel_ir_image_camera_response_open") as camera, \
                    mock.patch.object(n, "ams_mel_ir_image_los_quaternion_open") as los:
                with image.open_optical_distortion_map() as stream:
                    same(self, stream.receive(), expected(0))
                for call in (start, base, nuc, camera, los):
                    call.assert_not_called()
            with mock.patch.object(n, P + "open") as opened:
                error(self, m.ErrorKind.INVALID_ARGUMENT, image.open_optical_distortion_map)
                opened.assert_not_called()

    def test_timeout_validation_and_exact_once_uint32_forwarding(self):
        with owners() as (_, _, stream):
            with mock.patch.object(n, P + "receive") as receive:
                for timeout in (-1, 1 << 32, True, 1.5, None, "0"):
                    error(self, m.ErrorKind.INVALID_ARGUMENT, lambda: stream.receive(timeout))
                receive.assert_not_called()
            with mock.patch.object(n, P + "receive", wraps=n.ams_mel_ir_image_odm_receive) as receive:
                for timeout in (0, 1, 0xFFFFFFFF):
                    stream.receive(timeout)
                    self.assertEqual(receive.call_args.args[1], timeout)
                self.assertEqual(receive.call_count, 3)

    def test_pre_native_allocation_failure_preserves_registration_and_close_eligibility(self):
        with owners(capacity=None) as (_, image):
            original = MemoryError("diagnostic allocation")
            with mock.patch.object(impl, "_call_with_diagnostic", side_effect=original), \
                    mock.patch.object(n, P + "open") as opened:
                with self.assertRaises(MemoryError) as caught:
                    image.open_optical_distortion_map()
                self.assertIs(caught.exception, original)
                opened.assert_not_called()
            with image.open_optical_distortion_map(c.c_size_t(-1).value) as stream:
                native = stream._close_function
                with mock.patch.object(impl, "_call_with_diagnostic", side_effect=original), \
                        mock.patch.object(stream, "_close_function", wraps=native) as close:
                    with self.assertRaises(MemoryError):
                        stream.close()
                    close.assert_not_called()
                self.assertTrue(stream.is_open)
                same(self, stream.receive(), expected(0))
                with mock.patch.object(n, "IrImageOdmEventHandle", side_effect=original), \
                        mock.patch.object(n, P + "receive") as receive:
                    error(self, m.ErrorKind.INTERNAL_ERROR, stream.receive)
                    receive.assert_not_called()

    def test_every_float_field_preserves_ieee_classes_and_signed_zero(self):
        frd = (n.IrForwardRightDownV1 * 1)()
        rc = (n.IrRowColV1 * 1)()
        raw = RAW()
        raw.frd = n.IrForwardRightDownSpanV1(frd, 1)
        raw.row_col = n.IrRowColSpanV1(rc, 1)
        for value in (0., -0., 2.5, -7.25, math.inf, -math.inf, math.nan,
                      sys.float_info.max, float.fromhex("0x0.0000000000001p-1022")):
            for field in ("right_min", "right_max", "down_min", "down_max"):
                setattr(raw, field, value)
            frd[0] = n.IrForwardRightDownV1(value, value, value)
            rc[0] = n.IrRowColV1(value, value)
            wanted = m.OpticalDistortionMap(0, 0, 0, 0, 0, value, value, value, value,
                                           (F(value, value, value),), (R(value, value),))
            with self.subTest(value=value), owners() as (_, _, stream), inject_view(c.pointer(raw)):
                copied = stream.receive()
            same(self, copied, wanted)
            same(self, pickle.loads(pickle.dumps(copied)), wanted)

    def test_missing_event_failed_receive_and_malformed_diagnostics(self):
        for case in ("null", "failure", "throw", "zero", "oversized", "utf8", "unterminated", "embedded", "message"):
            with self.subTest(case=case), owners() as (_, _, stream):
                native = n.ams_mel_ir_image_odm_receive
                def receive(owner, timeout, output, diag, capacity, required):
                    status = native(owner, timeout, output, diag, capacity, required)
                    if case == "null":
                        n.ams_mel_ir_image_odm_event_close(output, None, 0, None)
                    elif case == "failure":
                        return n.AMS_MEL_PROVIDER_EXCEPTION
                    elif case == "throw":
                        raise RuntimeError("native invocation uncertainty")
                    elif case in ("zero", "oversized"):
                        c.cast(required, c.POINTER(c.c_size_t))[0] = 0 if case == "zero" else capacity + 1
                    elif case == "unterminated":
                        c.memset(diag, ord("x"), capacity)
                    else:
                        diagnostic(diag, capacity, required,
                                   b"\xff" if case == "utf8" else b"a\0b" if case == "embedded" else b"x")
                    return status
                with mock.patch.object(n, P + "receive", side_effect=receive) as received, \
                        mock.patch.object(n, P + "event_close", wraps=n.ams_mel_ir_image_odm_event_close) as close:
                    kind = (m.ErrorKind.PROVIDER_EXCEPTION if case == "failure" else
                            m.ErrorKind.INVALID_UTF8 if case == "utf8" else PROTOCOL)
                    if case == "throw":
                        with self.assertRaisesRegex(RuntimeError, "native invocation uncertainty"):
                            stream.receive()
                    else:
                        error(self, kind, stream.receive,
                              n.AMS_MEL_PROVIDER_EXCEPTION if case == "failure" else None)
                    self.assertEqual(received.call_count, 1)
                    self.assertEqual(close.call_count, 1)

    def test_null_misaligned_and_overflowing_event_views(self):
        backing = c.create_string_buffer(c.sizeof(RAW) + c.alignment(RAW))
        end = (1 << (8 * c.sizeof(c.c_void_p))) - c.alignment(RAW)
        for address in (None, c.addressof(backing) + 1, end):
            with self.subTest(address=address), owners() as (_, _, stream), \
                    inject_view(c.cast(address, c.POINTER(RAW))), \
                    mock.patch.object(n, P + "event_close", wraps=n.ams_mel_ir_image_odm_event_close) as close:
                error(self, PROTOCOL, stream.receive)
                self.assertEqual(close.call_count, 1)

    def test_both_spans_validated_before_copy_null_alignment_count_and_address_bounds(self):
        for field, span_type, element in (
                ("frd", n.IrForwardRightDownSpanV1, n.IrForwardRightDownV1),
                ("row_col", n.IrRowColSpanV1, n.IrRowColV1)):
            storage = (element * 1)()
            end = (1 << (8 * c.sizeof(c.c_void_p))) - c.alignment(element)
            for address, size in ((None, 1), (c.addressof(storage) + 1, 1), (end, 1),
                                  (c.addressof(storage), 0x80000000),
                                  (c.addressof(storage), c.c_size_t(-1).value)):
                raw = RAW()
                first = (n.IrForwardRightDownV1 * 1)()
                raw.frd = n.IrForwardRightDownSpanV1(first, 1)
                setattr(raw, field, span_type(c.cast(address, c.POINTER(element)), size))
                with self.subTest(field=field, address=address, size=size), owners() as (_, _, stream), \
                        inject_view(c.pointer(raw)), \
                        mock.patch.object(impl, "_checked_span") as copy, \
                        mock.patch.object(n, P + "event_close", wraps=n.ams_mel_ir_image_odm_event_close) as close:
                    error(self, PROTOCOL, stream.receive)
                    copy.assert_not_called()
                    self.assertEqual(close.call_count, 1)
        # Zero lengths never inspect/dereference inactive pointers.
        raw = RAW()
        raw.frd = n.IrForwardRightDownSpanV1(c.cast(1, c.POINTER(n.IrForwardRightDownV1)), 0)
        raw.row_col = n.IrRowColSpanV1(c.cast(1, c.POINTER(n.IrRowColV1)), 0)
        same(self, impl._map_from_raw(raw), expected(0))

    def test_conversion_allocation_and_cleanup_failures_publish_nothing(self):
        for function, original, kind in (
                ("_copy_frd", MemoryError("frd"), m.ErrorKind.INTERNAL_ERROR),
                ("_copy_row_col", MemoryError("row_col"), m.ErrorKind.INTERNAL_ERROR),
                ("OpticalDistortionMap", MemoryError("snapshot"), m.ErrorKind.INTERNAL_ERROR),
                ("_map_from_raw", MemoryError("conversion"), m.ErrorKind.INTERNAL_ERROR),
                ("_copy_row_col", RuntimeError("conversion"), None),
                ("_map_from_raw", KeyboardInterrupt("conversion"), None)):
            with self.subTest(function=function, original=type(original)), owners() as (_, _, stream):
                stream.receive()  # next is the rich map, with both collections
                native = n.ams_mel_ir_image_odm_event_close
                def close(*args):
                    native(*args)
                    raise RuntimeError("secondary cleanup failure")
                with mock.patch.object(impl, function, side_effect=original), \
                        mock.patch.object(n, P + "event_close", side_effect=close) as closed:
                    if kind is not None:
                        error(self, kind, stream.receive)
                    else:
                        with self.assertRaises(type(original)) as caught:
                            stream.receive()
                        self.assertIs(caught.exception, original)
                    self.assertEqual(closed.call_count, 1)

    def test_event_view_status_preserved_and_cleanup_once(self):
        with owners() as (_, _, stream):
            native = n.ams_mel_ir_image_odm_event_view
            def view(*args):
                native(*args)
                return n.AMS_MEL_PROVIDER_FAILED
            with mock.patch.object(n, P + "event_view", side_effect=view), \
                    mock.patch.object(n, P + "event_close", wraps=n.ams_mel_ir_image_odm_event_close) as close:
                error(self, m.ErrorKind.PROVIDER_FAILED, stream.receive, n.AMS_MEL_PROVIDER_FAILED)
                self.assertEqual(close.call_count, 1)

    def test_event_close_never_retried_on_uncertain_consumption(self):
        for case in ("status", "throw", "retained", "diagnostic"):
            with self.subTest(case=case), owners() as (_, _, stream):
                native = n.ams_mel_ir_image_odm_event_close
                retained = n.IrImageOdmEventHandle()
                def close(output, diag, capacity, required):
                    if case == "retained":
                        retained.value = c.cast(output, c.POINTER(n.IrImageOdmEventHandle))[0]
                        diagnostic(diag, capacity, required)
                        return n.AMS_MEL_OK
                    status = native(output, diag, capacity, required)
                    if case == "throw":
                        raise RuntimeError("release uncertainty")
                    if case == "diagnostic":
                        c.cast(required, c.POINTER(c.c_size_t))[0] = 0
                    return n.AMS_MEL_INTERNAL_ERROR if case == "status" else status
                try:
                    with mock.patch.object(n, P + "event_close", side_effect=close) as closed:
                        if case == "throw":
                            with self.assertRaisesRegex(RuntimeError, "release uncertainty"):
                                stream.receive()
                        else:
                            error(self, m.ErrorKind.INTERNAL_ERROR if case == "status" else PROTOCOL,
                                  stream.receive, n.AMS_MEL_INTERNAL_ERROR if case == "status" else None)
                        self.assertEqual(closed.call_count, 1)
                finally:
                    # Test injector knows the retained handle was NOT consumed.
                    if retained.value:
                        native(c.byref(retained), None, 0, None)

    def test_open_success_requires_status_registration_and_owner(self):
        for case in ("no-owner", "unknown", "unpublished", "rejection", "failure", "diagnostic"):
            with self.subTest(case=case), owners(capacity=None) as (_, image):
                native = n.ams_mel_ir_image_odm_open
                def opened(parent, capacity, output, registration, diag, length, required):
                    status = native(parent, capacity, output, registration, diag, length, required)
                    if case == "no-owner":
                        n.ams_mel_ir_image_odm_close(output, None, 0, None)
                    if case in ("unknown", "unpublished", "rejection"):
                        c.cast(registration, c.POINTER(c.c_uint32))[0] = {
                            "unknown": 77, "unpublished": 0xFFFFFFFF,
                            "rejection": int(m.CommandReturn.FAIL)}[case]
                    if case == "failure":
                        return n.AMS_MEL_PROVIDER_FAILED
                    if case == "diagnostic":
                        c.cast(required, c.POINTER(c.c_size_t))[0] = 0
                    return status
                close = mock.Mock(wraps=n.ams_mel_ir_image_odm_close)
                with mock.patch.object(n, P + "open", side_effect=opened), \
                        mock.patch.object(m.OpticalDistortionMapStream, "_close_function", staticmethod(close)):
                    failure = error(self, m.ErrorKind.PROVIDER_FAILED if case == "failure" else PROTOCOL,
                                    image.open_optical_distortion_map,
                                    n.AMS_MEL_PROVIDER_FAILED if case == "failure" else None)
                    self.assertIs(failure.registration,
                                  None if case in ("unknown", "unpublished") else
                                  m.CommandReturn.FAIL if case == "rejection" else m.CommandReturn.SUCCESS)
                    gc.collect()
                    self.assertEqual(close.call_count, 0 if case == "no-owner" else 1)

    def test_failed_open_unexpected_owner_cleanup_attempted_once_preserving_details(self):
        with owners(capacity=None) as (_, image):
            native = n.ams_mel_ir_image_odm_open
            native_close = n.ams_mel_ir_image_odm_close
            retained = n.IrImageOdmMetadataHandle()
            def opened(*args):
                native(*args)
                diagnostic(args[-3], args[-2], args[-1], b"primary")
                return n.AMS_MEL_PROVIDER_FAILED
            def close(output, *args):
                retained.value = c.cast(output, c.POINTER(n.IrImageOdmMetadataHandle))[0]
                raise RuntimeError("secondary")
            cleanup = mock.Mock(side_effect=close)
            try:
                with mock.patch.object(n, P + "open", side_effect=opened), \
                        mock.patch.object(m.OpticalDistortionMapStream, "_close_function", staticmethod(cleanup)):
                    failure = error(self, m.ErrorKind.PROVIDER_FAILED, image.open_optical_distortion_map,
                                    n.AMS_MEL_PROVIDER_FAILED)
                    self.assertEqual((failure.diagnostic, failure.diagnostic_required), ("primary", 8))
                    self.assertIs(failure.registration, m.CommandReturn.SUCCESS)
                    del failure
                    gc.collect()
                    self.assertEqual(cleanup.call_count, 1)
            finally:
                if retained.value:
                    native_close(c.byref(retained), None, 0, None)

    def test_consumer_close_failure_remembered_for_all_explicit_operations(self):
        for case in ("status", "throw", "retained", "diagnostic"):
            with self.subTest(case=case), owners(capacity=None) as (_, image):
                stream = image.open_optical_distortion_map()
                native = n.ams_mel_ir_image_odm_close
                retained = n.IrImageOdmMetadataHandle()
                def close(output, diag, capacity, required):
                    if case == "retained":
                        retained.value = c.cast(output, c.POINTER(n.IrImageOdmMetadataHandle))[0]
                        diagnostic(diag, capacity, required)
                        return n.AMS_MEL_OK
                    status = native(output, diag, capacity, required)
                    if case == "throw":
                        raise RuntimeError("uncertain close")
                    if case == "diagnostic":
                        c.cast(required, c.POINTER(c.c_size_t))[0] = 0
                    return n.AMS_MEL_PROVIDER_FAILED if case == "status" else status
                try:
                    with mock.patch.object(stream, "_close_function", side_effect=close) as closed:
                        with self.assertRaises(RuntimeError if case == "throw" else m.MelError) as caught:
                            stream.close()
                        first = caught.exception
                        if case != "throw":
                            self.assertIs(first.kind, m.ErrorKind.PROVIDER_FAILED if case == "status" else PROTOCOL)
                        for operation in (stream.close, stream.receive, stream.counters, stream.__enter__):
                            with self.assertRaises(type(first)) as again:
                                operation()
                            self.assertIs(again.exception, first)
                        stream.__del__()
                        stream.__del__()
                        self.assertEqual(closed.call_count, 1)
                finally:
                    if retained.value:
                        native(c.byref(retained), None, 0, None)

    def test_finalization_once_no_parent_reference_and_application_exception_preserved(self):
        with owners(capacity=None) as (session, image):
            stream = image.open_optical_distortion_map()
            image_ref, session_ref = weakref.ref(image), weakref.ref(session)
            self.assertNotIn(image, stream.__dict__.values())
            self.assertNotIn(session, stream.__dict__.values())
            self.assertIsNotNone(image_ref())
            self.assertIsNotNone(session_ref())
            native = stream._close_function
            def close(*args):
                native(*args)
                raise RuntimeError("secondary finalizer")
            original = KeyboardInterrupt("application")
            with mock.patch.object(stream, "_close_function", side_effect=close) as closed:
                with self.assertRaises(KeyboardInterrupt) as caught:
                    with stream:
                        raise original
                self.assertIs(caught.exception, original)
                stream.__del__()
                self.assertEqual(closed.call_count, 1)
            self.assertFalse(stream.is_open)
            self.assertTrue(image.is_open)
            error(self, m.ErrorKind.INVALID_ARGUMENT, stream.receive)

    def test_automatic_gc_one_attempt_and_successful_close_idempotent(self):
        with owners(capacity=None) as (_, image):
            stream = image.open_optical_distortion_map()
            reference = weakref.ref(stream)
            native = stream._close_function
            calls = []
            def close(*args):
                calls.append(1)
                native(*args)
                raise RuntimeError("non-raising finalizer")
            stream._close_function = close
            del stream
            gc.collect()
            self.assertIsNone(reference())
            self.assertEqual(calls, [1])
        with owners() as (_, _, stream):
            stream.close()
            stream.close()
            self.assertFalse(stream.is_open)
            error(self, m.ErrorKind.INVALID_ARGUMENT, stream.counters)
            error(self, m.ErrorKind.INVALID_ARGUMENT, stream.__enter__)

    def test_full_width_counters_and_native_failure(self):
        with owners() as (_, _, stream):
            native = n.ams_mel_ir_image_odm_get_counters
            def counters(owner, output, *args):
                native(owner, output, *args)
                raw = c.cast(output, c.POINTER(n.IrC2MetadataCountersV1)).contents
                raw.events_received, raw.events_dropped_queue_full, raw.malformed_or_unsupported = (
                    (1 << 64) - 1, (1 << 63) + 1, (1 << 63) - 1)
                return n.AMS_MEL_OK
            with mock.patch.object(n, P + "get_counters", side_effect=counters):
                self.assertEqual(stream.counters(), m.ImageMetadataCounters(
                    (1 << 64) - 1, (1 << 63) + 1, (1 << 63) - 1))
            def failed(*args):
                native(*args)
                return n.AMS_MEL_INTERNAL_ERROR
            with mock.patch.object(n, P + "get_counters", side_effect=failed):
                error(self, m.ErrorKind.INTERNAL_ERROR, stream.counters, n.AMS_MEL_INTERNAL_ERROR)

    def test_documented_example_runs_with_caller_supplied_mock_configuration(self):
        path = Path(__file__).resolve().parents[2] / "docs/py-mel-017-optical-distortion-map.md"
        code = path.read_text().split("```python\n", 1)[1].split("```", 1)[0]
        namespace = {}
        exec(compile(code, str(path), "exec"), namespace)
        with redirect_stdout(io.StringIO()) as output:
            value = namespace["receive_optical_map"](provider(), "odm-sync", "", frames.ImageStreamTests.config())
        self.assertEqual(output.getvalue(), "0 0\n")
        same(self, value, expected(0))

    def test_isolated_compiled_provider_scenarios(self):
        cases = ("fidelity", "running", "stopped", "failed", "closed", "capacity", "timeout", "stop-drain",
                 "parent-first", "session-first", "consumer-first", "gc-parents", "late", "late-closed",
                 "detach-fail", "in-flight", "register-fail", "register-not-implemented", "register-unknown",
                 "register-throw", "not-supported", "independence-base", "independence-nuc", "independence-camera",
                 "independence-odm", "independence-los")
        for case in cases:
            with self.subTest(case=case):
                env = dict(os.environ)
                env["PYTHONPATH"] = os.pathsep.join((str(Path(__file__).resolve().parent),
                                                      env.get("PYTHONPATH", "")))
                result = subprocess.run(
                    [sys.executable, "-W", "error", "-c",
                     "from test_ir_image_optical_distortion_map import isolated; isolated(" + repr(case) + ")"],
                    env=env, capture_output=True, text=True, timeout=40)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("ODM_CHILD_COMPLETE", result.stdout)


class Hooks:
    """An explicitly balanced extra DSO reference, used only in children."""

    def __init__(self):
        self.library = c.CDLL(str(provider()), mode=os.RTLD_NOW | os.RTLD_NOLOAD)
        self.deliver = self.library.mock_odm_deliver
        self.deliver.argtypes = [c.c_uint, c.c_uint32, c.c_uint]
        self.deliver.restype = c.c_int
        self.registrations = self.library.mock_odm_registrations
        self.registrations.argtypes = []
        self.registrations.restype = c.c_uint

    def close(self):
        _ctypes.dlclose(self.library._handle)


def probe(operation, value=0):
    function = n._LIBRARY.ams_mel_test_image_odm_probe
    function.argtypes = [c.c_uint, c.c_size_t]
    function.restype = c.c_int
    return function(operation, value)


def isolated(case):
    test = unittest.TestCase()
    with tempfile.TemporaryDirectory(prefix="py-odm-") as directory:
        log = Path(directory) / "lifetime"
        os.environ["AMS_MEL_TEST_LIFETIME_LOG"] = str(log)
        scenario = ("image-metadata-four-mixed" if case in ("not-supported", "independence-base") else
                    "nuc-rich" if case == "independence-nuc" else
                    "camera-callback-rich" if case == "independence-camera" else
                    "losq-sync" if case == "independence-los" else
                    "odm-detach-fail" if case == "failed" else
                    "odm-" + case if case.startswith("register-") or case in (
                        "late", "late-closed", "detach-fail", "in-flight", "capacity") else
                    "odm-running" if case in ("running", "stopped", "closed", "timeout") else "odm-sync")
        if case == "in-flight":
            test.assertEqual(probe(0), 1)
        session = m.Session.open(provider(), scenario, "")
        image = session.open_image_stream(frames.ImageStreamTests.config())
        hooks = Hooks()
        test.assertEqual(hooks.registrations(), 0)
        stream = None
        retained = []
        if case in ("stopped", "failed", "closed"):
            if case == "failed":
                error(test, m.ErrorKind.PROVIDER_FAILED, image.stop, n.AMS_MEL_PROVIDER_FAILED)
            else:
                image.stop() if case == "stopped" else image.close()
            failure = error(test, m.ErrorKind.INVALID_ARGUMENT, image.open_optical_distortion_map,
                            n.AMS_MEL_INVALID_ARGUMENT if case != "closed" else None)
            test.assertIsNone(failure.registration)
            test.assertEqual(hooks.registrations(), 0)
        elif case.startswith("register-") or case == "not-supported":
            registration = {"register-fail": m.CommandReturn.FAIL,
                            "register-not-implemented": m.CommandReturn.NOT_IMPLEMENTED,
                            "not-supported": m.CommandReturn.NOT_SUPPORTED}.get(case)
            status = n.AMS_MEL_PROVIDER_EXCEPTION if case == "register-throw" else n.AMS_MEL_PROVIDER_FAILED
            failure = error(test, m.ErrorKind.PROVIDER_EXCEPTION if case == "register-throw" else
                            m.ErrorKind.PROVIDER_FAILED, image.open_optical_distortion_map, status)
            test.assertIs(failure.registration, registration)
            test.assertTrue(failure.diagnostic)
            test.assertEqual(failure.diagnostic_required, len(failure.diagnostic.encode()) + 1)
            test.assertEqual(hooks.registrations(), 1)
            error(test, m.ErrorKind.INVALID_ARGUMENT, image.open_optical_distortion_map)
            test.assertEqual(hooks.registrations(), 1)
        elif case.startswith("independence-"):
            if case == "independence-base":
                with image.open_metadata() as base:
                    test.assertEqual(hooks.registrations(), 0)
                    failure = error(test, m.ErrorKind.PROVIDER_FAILED, image.open_optical_distortion_map,
                                    n.AMS_MEL_PROVIDER_FAILED)
                    test.assertIs(failure.registration, m.CommandReturn.NOT_SUPPORTED)
                    test.assertIsInstance(base.receive(), m.ImageMetadataEvent)
            elif case == "independence-nuc":
                with image.open_nuc() as nuc:
                    test.assertEqual(hooks.registrations(), 0)
                    error(test, m.ErrorKind.PROVIDER_FAILED, image.open_optical_distortion_map,
                          n.AMS_MEL_PROVIDER_FAILED)
                    test.assertIsInstance(nuc.receive(), m.NucTempData)
            elif case == "independence-camera":
                with image.open_camera_response() as camera:
                    test.assertEqual(hooks.registrations(), 0)
                    failure = error(test, m.ErrorKind.PROVIDER_FAILED, image.open_optical_distortion_map,
                                    n.AMS_MEL_PROVIDER_FAILED)
                    test.assertIs(failure.registration, m.CommandReturn.NOT_SUPPORTED)
                    test.assertIsInstance(camera.receive(), m.CameraCommandResponse)
            elif case == "independence-los":
                los = n.IrImageLosQuaternionMetadataHandle()
                registration = c.c_uint32(0xFFFFFFFF)
                test.assertEqual(n.ams_mel_ir_image_los_quaternion_open(
                    image._owner, 16, c.byref(los), c.byref(registration), None, 0, None), n.AMS_MEL_OK)
                test.assertEqual(hooks.registrations(), 0)
                test.assertEqual(n.ams_mel_ir_image_los_quaternion_close(c.byref(los), None, 0, None), n.AMS_MEL_OK)
                error(test, m.ErrorKind.PROVIDER_FAILED, image.open_optical_distortion_map,
                      n.AMS_MEL_PROVIDER_FAILED)
            else:
                stream = image.open_optical_distortion_map()
                with image.open_metadata() as base:
                    test.assertTrue(base.is_open)
                for operation in (image.open_nuc, image.open_camera_response):
                    error(test, m.ErrorKind.PROVIDER_FAILED, operation, n.AMS_MEL_PROVIDER_FAILED)
                los = n.IrImageLosQuaternionMetadataHandle()
                test.assertEqual(n.ams_mel_ir_image_los_quaternion_open(
                    image._owner, 16, c.byref(los), None, None, 0, None), n.AMS_MEL_PROVIDER_FAILED)
                test.assertEqual(hooks.registrations(), 1)
                retained = [stream.receive() for _ in range(4)]
        else:
            if case == "running":
                image.start()
            stream = image.open_optical_distortion_map(2 if case == "capacity" else 16)
            test.assertEqual(hooks.registrations(), 1)
            error(test, m.ErrorKind.INVALID_ARGUMENT, image.open_optical_distortion_map)
            if case == "capacity":
                test.assertEqual(hooks.deliver(6, 1, 0), 1)
                test.assertEqual(hooks.deliver(6, 2, 0), 1)
                test.assertEqual(probe(6, 1), 1)
                test.assertEqual(hooks.deliver(1, 0, 0), 1)  # full: no copy allocation
                test.assertEqual(probe(7), 1)
                test.assertEqual(hooks.deliver(0, 0, 1), 1)  # null: malformed even full
                test.assertEqual(stream.counters(), m.ImageMetadataCounters(4, 1, 1))
                same(test, stream.receive(), expected(6, 1))
                same(test, stream.receive(), expected(6, 2))
                test.assertEqual(hooks.deliver(6, 3, 0), 1)  # one-shot copy failure
                test.assertEqual(stream.counters(), m.ImageMetadataCounters(5, 1, 2))
                test.assertEqual(hooks.deliver(6, 4, 0), 1)
                same(test, stream.receive(), expected(6, 4))
                error(test, m.ErrorKind.TIMEOUT, stream.receive, n.AMS_MEL_TIMEOUT)
            elif case in ("timeout", "running"):
                error(test, m.ErrorKind.TIMEOUT, stream.receive, n.AMS_MEL_TIMEOUT)
                error(test, m.ErrorKind.TIMEOUT, lambda: stream.receive(1), n.AMS_MEL_TIMEOUT)
                test.assertEqual(probe(9), 1)
                observed = []
                def deliver_when_waiting():
                    observed.append(probe(8))
                    observed.append(hooks.deliver(6, 31, 0))
                helper = threading.Thread(target=deliver_when_waiting)
                helper.start()
                try:
                    same(test, stream.receive(10000), expected(6, 31))
                finally:
                    helper.join(15)
                test.assertFalse(helper.is_alive())
                test.assertEqual(observed, [1, 1])
            elif case == "detach-fail":
                error(test, m.ErrorKind.PROVIDER_FAILED, image.stop, n.AMS_MEL_PROVIDER_FAILED)
                retained = [stream.receive(), stream.receive()]
                same(test, retained, [expected(1), expected(2)])
                error(test, m.ErrorKind.PROVIDER_FAILED, stream.receive, n.AMS_MEL_PROVIDER_FAILED)
                error(test, m.ErrorKind.INVALID_ARGUMENT, image.open_optical_distortion_map)
                error(test, m.ErrorKind.PROVIDER_FAILED, image.close, n.AMS_MEL_PROVIDER_FAILED)
            elif case == "in-flight":
                test.assertEqual(probe(1), 1)
                stream.close()  # must not wait for the parked callback
                observed = []
                def release_at_drain():
                    observed.append(probe(3))
                    observed.append(probe(2))
                helper = threading.Thread(target=release_at_drain)
                helper.start()
                try:
                    image.close()
                finally:
                    probe(2)
                    helper.join(15)
                test.assertFalse(helper.is_alive())
                test.assertEqual(observed, [1, 1])
            else:
                if case == "fidelity":
                    for fixture in (4, 5, 6):
                        test.assertEqual(hooks.deliver(fixture, 0xFFFFFFFF, 0), 1)
                    retained = [stream.receive() for _ in range(7)]
                    for fixture, value in enumerate(retained):
                        same(test, value, expected(fixture, 0xFFFFFFFF))
                elif case in ("consumer-first", "late-closed"):
                    retained = [stream.receive() for _ in range(4)]
                    stream.close()
                    error(test, m.ErrorKind.INVALID_ARGUMENT, image.open_optical_distortion_map)
                    test.assertEqual(hooks.deliver(1, 0, 0), 1)  # safe callback after consumer close
                    image.start()
                    test.assertEqual(len(image.receive(2000).pixels), 12)
                else:
                    if case == "stop-drain":
                        image.stop()
                    elif case == "session-first":
                        session.close()
                        image.close()
                    elif case == "gc-parents":
                        ir, sr = weakref.ref(image), weakref.ref(session)
                        del image, session
                        gc.collect()
                        test.assertIsNone(ir())
                        test.assertIsNone(sr())
                    else:
                        image.close()
                    if case in ("parent-first", "session-first", "gc-parents", "late"):
                        if case != "gc-parents":
                            session.close()
                        hooks.close()
                        hooks = None
                        test.assertIn("library_unloaded", log.read_text().splitlines())
                    retained = [stream.receive() for _ in range(4)]
                    error(test, m.ErrorKind.STREAM_STOPPED, stream.receive, n.AMS_MEL_STREAM_STOPPED)
                    for fixture, value in enumerate(retained):
                        same(test, value, expected(fixture))
        if stream is not None:
            stream.close()
        if case != "gc-parents":
            if case == "failed":
                error(test, m.ErrorKind.PROVIDER_FAILED, image.close, n.AMS_MEL_PROVIDER_FAILED)
            image.close()
            session.close()
        if hooks is not None:
            hooks.close()
        gc.collect()
        lines = log.read_text().splitlines()
        test.assertIn("library_unloaded", lines)
        if Path("/proc/self/maps").exists():
            test.assertNotIn(str(provider().resolve()), Path("/proc/self/maps").read_text())
        if case in ("late", "late-closed", "register-fail"):
            indices = [lines.index(name) for name in (
                "late_odm_callback_invoked", "late_odm_callback_returned", "library_unloaded")]
            test.assertEqual(indices, sorted(indices))
        for value in retained:
            assert_owned(test, value)
            same(test, pickle.loads(pickle.dumps(value)), value)
        if case == "fidelity":
            for fixture, value in enumerate(retained):
                same(test, value, expected(fixture, 0xFFFFFFFF))
        print("ODM_CHILD_COMPLETE")


if __name__ == "__main__":
    unittest.main()
