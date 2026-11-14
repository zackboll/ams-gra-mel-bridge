"""PY-MEL-014: existing Image owner, zero-layout attachment, hostile lifetimes."""

from __future__ import annotations

import ctypes as c
import dataclasses as dc
from decimal import Decimal
from fractions import Fraction
import gc
import math
import os
from pathlib import Path
import pickle
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import weakref

import ams_mel as m
from ams_mel import _image, _native as n, _types
from test_common_channel import HIGH, HIGH_REPORT, KEEPALIVE_SUCCESS, assert_owned_graph
from test_ir_image_metadata import RICH, navigation
from test_ir_navigation import diagnostic, report as mock_navigation_report, reclaimed


OPEN = "ams_mel_ir_image_control_open"
CLOSE = "ams_mel_ir_stream_close"
RECEIVE_DIAGNOSTIC = "image receive unavailable on control-only attachment"


def provider():
    return Path(os.environ["AMS_MEL_TEST_PROVIDER_DIR"]) / "libmock_ir_provider.so"


def config(**changes):
    return m.ImageControlConfig(**(dict(
        channel_id=m.UciId(bytes(range(16)), "Image α"),
        platform_id=m.UciId(bytes(range(240, 256)), "test platform µ"),
        sensor_location=m.ComponentLocation(1.25, -2.5, 3.75, "station β", "mock γ"),
    ) | changes))


def error(test, kind, operation):
    with test.assertRaises(m.MelError) as caught:
        operation()
    test.assertIs(caught.exception.kind, kind)
    return caught.exception


def assert_zero(test, capability):
    for field in ("height", "width", "bit_depth", "row_pitch", "buffer_size",
                  "image_size", "number_of_bands", "pixel_format"):
        test.assertEqual(getattr(capability, field), 0, field)
    assert_owned_graph(test, capability)
    test.assertEqual(len(capability.sensor_types), 2)
    test.assertEqual(capability.channel_types, (m.ChannelType.IRST_IMAGE,))
    test.assertEqual(capability.task_schedule_depth, 17)
    test.assertTrue(capability.odc_available)
    test.assertTrue(capability.nuc_available)
    test.assertEqual(len(capability.metadata_capabilities), 4)
    test.assertEqual(len(capability.image_bands), 2)
    test.assertEqual(len(capability.image_bands[0].bands), 2)
    test.assertEqual(len(capability.nav_frames), 2)


def assert_no_storage(test, lines):
    for event in ("buffer_factory_called", "buffer_initialized", "buffer_registered",
                  "buffer_unregistered"):
        test.assertNotIn(event, lines)


def child(scenario):
    """Dangerous provider state/DSO tests run in a fresh interpreter, with timeout."""
    test = unittest.TestCase()
    with tempfile.TemporaryDirectory(prefix="py014-") as directory:
        log = Path(directory) / "lifetime"
        os.environ["AMS_MEL_TEST_LIFETIME_LOG"] = str(log)
        session = m.Session.open(provider(), scenario, "")
        failures = {
            "attach-null": m.ErrorKind.FACTORY_FAILED,
            "attach-throw": m.ErrorKind.PROVIDER_EXCEPTION,
            "image-control-wrong-type": m.ErrorKind.INITIALIZATION_FAILED,
            "image-control-open-detach-fail": m.ErrorKind.PROVIDER_FAILED,
            "image-control-attach-reentry-throw": m.ErrorKind.PROVIDER_EXCEPTION,
        }
        if scenario in failures:
            with mock.patch.object(n, OPEN, wraps=getattr(n, OPEN)) as opened:
                error(test, failures[scenario], lambda: session.open_image_control(config()))
                opened.assert_called_once()
            session.close()
            lines = log.read_text().splitlines()
            if scenario in ("attach-null", "attach-throw", "image-control-open-detach-fail",
                            "image-control-attach-reentry-throw"):
                test.assertNotIn("control_destroyed", lines)
                test.assertNotIn("library_unloaded", lines)
            else:
                test.assertIn("library_unloaded", lines)
            if scenario == "image-control-attach-reentry-throw":
                test.assertEqual(lines.count("control_attach_callback_returned"), 1)
                test.assertEqual(lines.count("control_buffer_released"), 1)
            assert_no_storage(test, lines)
            return

        if scenario == "image-control-pending":
            # Test-only native barrier, no sleeps or invented future completion.
            library = c.CDLL(str(provider()))
            release = library.mock_completion_release_family
            release.argtypes, release.restype = [c.c_uint, c.c_uint], c.c_int
            image = session.open_image_control(config())
            metadata = image.open_metadata(16)
            test.assertEqual(tuple(metadata.receive(0) for _ in range(4)), RICH)
            request = image.submit_navigation_report(mock_navigation_report())
            error(test, m.ErrorKind.TIMEOUT, lambda: request.wait(0))
            image_ref, session_ref = weakref.ref(image), weakref.ref(session)
            image.close()
            session.close()
            del image, session
            gc.collect()
            test.assertIsNone(image_ref())
            test.assertIsNone(session_ref())
            test.assertNotIn("channel_detached", log.read_text().splitlines())
            metadata.close()
            test.assertEqual(release(3, 1), 1)
            test.assertEqual(request.wait(5000), m.NavigationCompleted(navigation().payload))
            request.close()
            reclaimed()
            test.assertEqual(log.read_text().splitlines().count("channel_destroyed"), 1)
            assert_no_storage(test, log.read_text().splitlines())
            # Drop the explicit test dlopen before checking actual safe unload.
            import _ctypes
            _ctypes.dlclose(library._handle)
            test.assertIn("library_unloaded", log.read_text().splitlines())
            return

        image = session.open_image_control(config())
        test.assertIs(type(image), m.ImageStream)
        metadata = image.open_metadata(queue_capacity=16)
        values = tuple(metadata.receive(0) for _ in range(4))
        test.assertEqual(values, RICH)
        test.assertNotIn("channel_enabled", log.read_text().splitlines())
        test.assertEqual(log.read_text().splitlines().count("control_registration_callbacks_returned"), 1)
        uncertain = scenario in ("image-control-release-fail", "image-control-release-throw")
        if uncertain:
            error(test, m.ErrorKind.PROVIDER_FAILED, image.start)
        else:
            image.start()
            image.start()  # Native idempotent enable, not a second buffer setup.
        test.assertEqual(log.read_text().splitlines().count("channel_enabled"), 1)
        if not uncertain:
            with image.channel_view() as view:
                assert_zero(test, view.capabilities())
        receive_error = error(test, m.ErrorKind.INITIALIZATION_FAILED, lambda: image.receive(0))
        test.assertEqual(receive_error.diagnostic, RECEIVE_DIAGNOSTIC)
        snapshot = n.IrFrameSnapshotHandle()
        from ams_mel._errors import _call_with_diagnostic
        receive_error = error(test, m.ErrorKind.INITIALIZATION_FAILED, lambda: _call_with_diagnostic(
            lambda diagnostic, required: n.ams_mel_ir_stream_receive_snapshot(
                image._owner, 0, c.byref(snapshot), diagnostic, len(diagnostic), c.byref(required))))
        test.assertEqual(receive_error.diagnostic, RECEIVE_DIAGNOSTIC)
        test.assertFalse(snapshot.value)
        if uncertain:
            error(test, m.ErrorKind.PROVIDER_FAILED, image.close)
            # Preserve existing Image lifecycle; do not let a test finalizer retry.
            image._owner.value = None
        else:
            image.stop()
            image.stop()
            image.close()
            image.close()
        session.close()
        lines = log.read_text().splitlines()
        assert_no_storage(test, lines)
        if uncertain:
            test.assertNotIn("channel_destroyed", lines)
            test.assertNotIn("library_unloaded", lines)
            test.assertEqual(lines.count("control_buffer_destroyed"), 1)
        else:
            test.assertEqual(lines.count("control_shutdown_callbacks_returned"), 1)
            test.assertEqual(lines.count("channel_destroyed"), 1)
            test.assertEqual(lines.count("library_unloaded"), 1)
        # Metadata's native state outlives the parents without retaining Python
        # owners; teardown callbacks are safely discarded after logical stop.
        test.assertTrue(metadata.is_open)
        error(test, m.ErrorKind.STREAM_STOPPED, lambda: metadata.receive(0))
        metadata.close()
        for value in values:
            assert_owned_graph(test, value)
            test.assertEqual(pickle.loads(pickle.dumps(value)), value)
        if scenario in ("image-control-unexpected", "image-control-release-fail", "image-control-release-throw"):
            test.assertEqual(lines.count("control_buffer_released"), 2)
            test.assertEqual(lines.count("control_reentry_returned"), 1)
        if scenario == "image-control-attach-reentry":
            test.assertEqual(lines.count("control_buffer_released"), 1)
            test.assertEqual(lines.count("control_attach_callback_returned"), 1)


class ImageControlProviderTests(unittest.TestCase):
    pass


def provider_test(scenario):
    def test(self):
        result = subprocess.run(
            [sys.executable, "-W", "error", "-c",
             "from test_ir_image_control import child; child(" + repr(scenario) + ")"],
            env=dict(os.environ, PYTHONPATH=os.pathsep.join(sys.path)),
            capture_output=True, text=True, timeout=45)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
    return test


for _scenario in (
    "image-control-zero", "image-control-unexpected", "image-control-attach-reentry",
    "image-control-release-fail", "image-control-release-throw", "image-control-pending",
    "attach-null", "attach-throw", "image-control-wrong-type",
    "image-control-open-detach-fail", "image-control-attach-reentry-throw",
):
    setattr(ImageControlProviderTests, "test_" + _scenario.replace("-", "_"), provider_test(_scenario))


class ImageControlTests(unittest.TestCase):
    def open(self):
        session = m.Session.open(provider(), "image-control-zero", "")
        self.addCleanup(session.close)
        return session

    def test_zero_layout_discovery_common_navigation_and_parent_independence(self):
        session = self.open()
        advertised = next(item for item in session.capabilities()
                          if m.ChannelType.IRST_IMAGE in item.channel_types)
        assert_zero(self, advertised)
        value = config(channel_id=advertised.channel_id)
        acquisition = m.ImageConfig(value.channel_id, value.platform_id, value.sensor_location,
                                    buffer_count=2, buffer_size=64, queue_capacity=2)
        error(self, m.ErrorKind.INITIALIZATION_FAILED, lambda: session.open_image_stream(acquisition))
        with mock.patch.object(n, OPEN, wraps=getattr(n, OPEN)) as opened:
            image = session.open_image_control(value)
            opened.assert_called_once()
        self.addCleanup(image.close)
        self.assertIs(type(image), m.ImageStream)
        with image.channel_view() as view:
            capability = view.capabilities()
            self.assertEqual(capability, advertised)
            assert_zero(self, capability)
            request = view.send_keepalive()
            try:
                self.assertIs(type(request), m.ReturnRequest)
                self.assertEqual(request.wait(5000), KEEPALIVE_SUCCESS)
            finally:
                request.close()
            request = view.submit_comms_test(HIGH)
            try:
                self.assertIs(type(request), m.CommsRequest)
                self.assertEqual(request.wait(5000), HIGH_REPORT)
            finally:
                request.close()
        self.assertTrue(image.is_open)  # Weak view Close is not typed-owner Close.
        metadata = image.open_metadata(queue_capacity=16)
        self.addCleanup(metadata.close)
        self.assertEqual(tuple(metadata.receive(0) for _ in range(4)), RICH)
        with image.submit_navigation_report(mock_navigation_report()) as request:
            self.assertIs(type(request), m.NavigationRequest)
            # Callback observed before waiting; it is not the future result.
            self.assertEqual(metadata.receive(0), navigation())
            self.assertEqual(request.wait(5000), m.NavigationRejected(
                m.MelErrorCode.INVALID_STATE, "no navigation hardware"))
        session.close()
        self.assertNotIn(session, image.__dict__.values())
        with image.channel_view() as view:
            self.assertEqual(view.capabilities(), capability)
        image.close()
        self.assertEqual(pickle.loads(pickle.dumps(capability)), advertised)

    def test_stream_retains_provider_not_python_session(self):
        session = m.Session.open(provider(), "image-control-zero", "")
        image = session.open_image_control(config())
        self.addCleanup(image.close)
        reference = weakref.ref(session)
        del session
        gc.collect()
        self.assertIsNone(reference())
        with image.channel_view() as view:
            assert_zero(self, view.capabilities())

    def test_public_identity_and_only_three_frozen_fields(self):
        self.assertIs(m.ImageControlConfig, _types.ImageControlConfig)
        self.assertIs(m.ImageStream, _image.ImageStream)
        self.assertIn("ImageControlConfig", m.__all__)
        self.assertEqual(m.ImageControlConfig.__module__, "ams_mel")
        self.assertIs(pickle.loads(pickle.dumps(m.ImageControlConfig)), m.ImageControlConfig)
        self.assertEqual([field.name for field in dc.fields(m.ImageControlConfig)],
                         ["channel_id", "platform_id", "sensor_location"])
        value = config()
        self.assertEqual(pickle.loads(pickle.dumps(value)), value)
        with self.assertRaises(dc.FrozenInstanceError):
            value.channel_id = value.platform_id

    def test_config_validation_construction_and_mutation_before_submission(self):
        session = self.open()
        invalid = {"channel_id": (None, b"x", 1), "platform_id": (None, "x", True),
                   "sensor_location": (None, (1, 2, 3), m.UciId(bytes(16), "x"))}
        with mock.patch.object(n, OPEN) as opened:
            for field, candidates in invalid.items():
                for candidate in candidates:
                    with self.subTest(field=field, candidate=candidate):
                        error(self, m.ErrorKind.INVALID_ARGUMENT, lambda: config(**{field: candidate}))
                        value = config()
                        object.__setattr__(value, field, candidate)
                        error(self, m.ErrorKind.INVALID_ARGUMENT, lambda: session.open_image_control(value))
            for parent, names, candidates in (
                ("channel_id", ("uuid",), (bytes(15), bytearray(16), None)),
                ("platform_id", ("uuid",), (bytes(17), "x", True)),
                ("channel_id", ("descriptive_label",), ("x\0y", "\ud800", b"x", None)),
                ("platform_id", ("descriptive_label",), ("x\0y", "\udfff", 1)),
                ("sensor_location", ("key", "system_name"), ("x\0y", "\ud800", b"x", None)),
                ("sensor_location", ("offset_x_m", "offset_y_m", "offset_z_m"),
                 (True, False, "1", None, 1j, 10 ** 1000, Decimal("1e10000"))),
            ):
                for name in names:
                    for candidate in candidates:
                        with self.subTest(parent=parent, name=name, candidate=repr(candidate)):
                            value = config()
                            nested = getattr(value, parent)
                            object.__setattr__(nested, name, candidate)
                            error(self, m.ErrorKind.INVALID_ARGUMENT, lambda: config(**{parent: nested}))
                            error(self, m.ErrorKind.INVALID_ARGUMENT, lambda: session.open_image_control(value))
                    value = config()
                    object.__delattr__(getattr(value, parent), name)
                    error(self, m.ErrorKind.INVALID_ARGUMENT, lambda: session.open_image_control(value))
            for field in dc.fields(m.ImageControlConfig):
                value = config()
                object.__delattr__(value, field.name)
                error(self, m.ErrorKind.INVALID_ARGUMENT, lambda: session.open_image_control(value))
            error(self, m.ErrorKind.INVALID_ARGUMENT, lambda: session.open_image_control(None))
            session.close()
            error(self, m.ErrorKind.INVALID_ARGUMENT, lambda: session.open_image_control(config()))
            opened.assert_not_called()

    def test_exact_native_record_utf8_uuid_and_double_values(self):
        session = self.open()
        native = getattr(n, OPEN)
        for number in (Decimal("1.25"), Fraction(-5, 2), -0.0, 1e300, float("inf"), float("nan")):
            value = config(sensor_location=m.ComponentLocation(number, number, number, "clé α", "系统"))

            def inspect(*args):
                gc.collect()  # Borrowed buffers must still be strongly owned.
                raw = c.cast(args[1], c.POINTER(n.IrImageControlConfigV1)).contents
                self.assertEqual(raw.channel_type, n.AMS_MEL_IR_CHANNEL_IRST_IMAGE)
                for name in ("channel_id", "platform_id"):
                    self.assertEqual(bytes(getattr(raw, name).uuid), getattr(value, name).uuid)
                for span, text in (
                    (raw.channel_id.descriptive_label, value.channel_id.descriptive_label),
                    (raw.platform_id.descriptive_label, value.platform_id.descriptive_label),
                    (raw.sensor_location.key, value.sensor_location.key),
                    (raw.sensor_location.system_name, value.sensor_location.system_name),
                ):
                    encoded = text.encode("utf-8")
                    self.assertEqual(span.size, len(encoded))
                    self.assertEqual(c.string_at(span.data, span.size), encoded)
                for name in ("offset_x_m", "offset_y_m", "offset_z_m"):
                    actual = getattr(raw.sensor_location, name)
                    if math.isnan(float(number)):
                        self.assertTrue(math.isnan(actual))
                    else:
                        self.assertEqual(struct.pack("d", actual), struct.pack("d", float(number)))
                return native(*args)

            with self.subTest(number=number), mock.patch.object(n, OPEN, side_effect=inspect) as opened:
                with session.open_image_control(value):
                    pass
                opened.assert_called_once()

    def test_success_without_owner_and_hostile_diagnostics(self):
        session = self.open()
        for status, text, kind in (
            (n.AMS_MEL_OK, b"", m.ErrorKind.PROTOCOL_INCONSISTENCY),
            (n.AMS_MEL_PROVIDER_FAILED, b"\xff", m.ErrorKind.INVALID_UTF8),
            (n.AMS_MEL_OK, b"unexpected", m.ErrorKind.PROTOCOL_INCONSISTENCY),
        ):
            with self.subTest(status=status, text=text), mock.patch.object(
                n, OPEN, side_effect=lambda *args: diagnostic(args, status, text)
            ) as opened, mock.patch.object(n, CLOSE) as closed:
                error(self, kind, lambda: session.open_image_control(config()))
                opened.assert_called_once()
                closed.assert_not_called()

    def test_invalid_diagnostic_shapes_cleanup_published_owner_once(self):
        session = self.open()
        for mode in ("zero", "unterminated", "embedded-nul", "oversized"):
            def opened(*args):
                c.cast(args[2], c.POINTER(n.IrStreamHandle))[0] = n.IrStreamHandle(1234)
                buffer, capacity, required = args[-3:]
                text = b"x" if mode == "unterminated" else b"x\0y\0"
                c.memmove(buffer, text, len(text))
                c.cast(required, c.POINTER(c.c_size_t))[0] = (
                    0 if mode == "zero" else capacity + 1 if mode == "oversized" else len(text))
                return n.AMS_MEL_OK

            with self.subTest(mode=mode), mock.patch.object(n, OPEN, side_effect=opened) as submit, \
                    mock.patch.object(n, CLOSE, return_value=n.AMS_MEL_PROVIDER_FAILED) as close:
                error(self, m.ErrorKind.PROTOCOL_INCONSISTENCY, lambda: session.open_image_control(config()))
                gc.collect()
                submit.assert_called_once()
                close.assert_called_once()

    def test_unexpected_owner_cleanup_once_preserves_original_failure(self):
        session = self.open()
        for failure in ("status", "diagnostic", "exception", "wrapping"):
            for cleanup in ("consumed", "failure", "throw"):
                original = KeyboardInterrupt("original open failure")

                def opened(*args):
                    c.cast(args[2], c.POINTER(n.IrStreamHandle))[0] = n.IrStreamHandle(1234)
                    if failure == "exception":
                        raise original
                    return diagnostic(args, n.AMS_MEL_PROVIDER_FAILED if failure == "status" else n.AMS_MEL_OK,
                                      b"original" if failure == "status" else b"\xff" if failure == "diagnostic" else b"")

                def close(*args):
                    if cleanup == "throw":
                        raise RuntimeError("cleanup failure")
                    if cleanup == "consumed":
                        c.cast(args[0], c.POINTER(n.IrStreamHandle))[0] = n.IrStreamHandle()
                    return n.AMS_MEL_PROVIDER_FAILED

                with self.subTest(failure=failure, cleanup=cleanup), \
                        mock.patch.object(n, OPEN, side_effect=opened) as submitted, \
                        mock.patch.object(n, CLOSE, side_effect=close) as closed, \
                        mock.patch.object(m.ImageStream, "_from_owner", side_effect=original) as wrapped:
                    expected = KeyboardInterrupt if failure in ("exception", "wrapping") else m.MelError
                    with self.assertRaises(expected) as caught:
                        session.open_image_control(config())
                    if expected is KeyboardInterrupt:
                        self.assertIs(caught.exception, original)
                    else:
                        self.assertIs(caught.exception.kind, m.ErrorKind.PROVIDER_FAILED if failure == "status"
                                      else m.ErrorKind.INVALID_UTF8)
                    gc.collect()
                    submitted.assert_called_once()
                    closed.assert_called_once()
                    self.assertEqual(wrapped.call_count, int(failure == "wrapping"))

    def test_documented_consumer_example(self):
        document = Path(__file__).resolve().parents[2] / "docs/py-mel-014-image-control-only.md"
        snippet = document.read_text().split("<!-- executable-control-example -->\n```python\n")[1].split("\n```", 1)[0]
        value = config()
        exec(compile(snippet, str(document), "exec"), dict(
            provider_path=provider(), instance_name="image-control-zero",
            platform_id=value.platform_id, sensor_location=value.sensor_location))

    def test_documented_navigation_result_example(self):
        document = Path(__file__).resolve().parents[2] / "docs/py-mel-014-image-control-only.md"
        snippet = document.read_text().split("```python\nfrom ams_mel import MelErrorCode", 1)[1].split("\n```", 1)[0]
        snippet = "from ams_mel import MelErrorCode" + snippet
        with self.open().open_image_control(config()) as image:
            exec(compile(snippet, str(document), "exec"), dict(
                image=image, report=mock_navigation_report()))


if __name__ == "__main__":
    unittest.main()
