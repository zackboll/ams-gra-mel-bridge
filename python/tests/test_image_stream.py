from __future__ import annotations

import gc
import os
from pathlib import Path
import tempfile
import time
import unittest
import weakref

from ams_mel import (
    ComponentLocation,
    ErrorKind,
    ImageConfig,
    ImageFlip,
    ImageStream,
    ImageType,
    MelError,
    Session,
    StreamCounters,
    UciId,
)


class ImageStreamTests(unittest.TestCase):
    def setUp(self) -> None:
        self.mock_provider = (
            Path(os.environ["AMS_MEL_TEST_PROVIDER_DIR"]) / "libmock_ir_provider.so"
        )
        self.temporary_directory = tempfile.TemporaryDirectory(
            prefix="ams-mel-python-stream-"
        )
        self.lifetime_log = Path(self.temporary_directory.name) / "lifetime.log"
        os.environ["AMS_MEL_TEST_LIFETIME_LOG"] = str(self.lifetime_log)

    def tearDown(self) -> None:
        os.environ.pop("AMS_MEL_TEST_LIFETIME_LOG", None)
        self.temporary_directory.cleanup()

    def lifecycle(self) -> str:
        try:
            return self.lifetime_log.read_text(encoding="utf-8")
        except FileNotFoundError:
            return ""

    def open(self, scenario: str) -> Session:
        return Session.open(self.mock_provider, scenario, "")

    @staticmethod
    def config(queue_capacity: int = 4) -> ImageConfig:
        return ImageConfig.new(
            UciId(bytes(range(16)), "IR image channel"),
            UciId(bytes(range(0xF0, 0x100)), "test platform"),
            ComponentLocation(1.25, -2.5, 3.75, "station-1", "mock-aircraft"),
            buffer_count=3,
            buffer_size=64,
            queue_capacity=queue_capacity,
        )

    def assert_error(self, kind: ErrorKind, call: object) -> MelError:
        with self.assertRaises(MelError) as caught:
            call()  # type: ignore[operator]
        self.assertEqual(caught.exception.kind, kind)
        return caught.exception

    def test_complete_metadata_and_owned_pixels(self) -> None:
        session = self.open("success")
        stream = session.open_image_stream(self.config())
        stream.start()
        first = stream.receive(1000)
        self.assertEqual(first.system_time_ns, 1_000_001)
        self.assertEqual(first.integration_time_ns, 20_001)
        self.assertEqual((first.width, first.height), (4, 3))
        self.assertEqual((first.bits_per_pixel, first.number_of_bands), (8, 1))
        self.assertEqual((first.horizontal_fov_rad, first.vertical_fov_rad), (0.25, 0.125))
        self.assertEqual((first.frame_id, first.subframe_id, first.subframe_total), (1, 2, 4))
        self.assertEqual(first.image_type, ImageType.STARING)
        self.assertEqual(first.image_flip, ImageFlip.HORIZONTAL)
        self.assertEqual(first.image_flags, 1 << 2)
        self.assertEqual((first.dither_row, first.dither_column), (0.5, -0.25))
        self.assertEqual((first.row_offset, first.column_offset, first.band_index), (7, 9, 3))
        self.assertEqual(first.pixels, bytes(range(16, 28)))
        self.assertIs(type(first.pixels), bytes)
        second = stream.receive(1000)
        self.assertEqual(second.frame_id, 2)
        self.assertEqual(second.pixels, bytes(range(32, 44)))
        self.assertEqual(first.pixels, bytes(range(16, 28)))
        stream.close()
        session.close()

    def test_timeout_stopped_and_restart_semantics(self) -> None:
        session = self.open("idle")
        stream = session.open_image_stream(self.config())
        stream.start()
        stream.start()
        self.assert_error(ErrorKind.TIMEOUT, lambda: stream.receive(2))
        stream.stop()
        stream.stop()
        self.assert_error(ErrorKind.STREAM_STOPPED, stream.start)
        self.assert_error(ErrorKind.STREAM_STOPPED, lambda: stream.receive(0))
        stream.close()
        session.close()

    def test_overflow_counters_and_drain(self) -> None:
        session = self.open("overflow")
        stream = session.open_image_stream(self.config(2))
        stream.start()
        deadline = time.monotonic() + 1.0
        counters = stream.counters()
        while counters.frames_received < 20 and time.monotonic() < deadline:
            time.sleep(0.005)
            counters = stream.counters()
        self.assertEqual(counters, StreamCounters(20, 18, 0))
        stream.stop()
        self.assertEqual([stream.receive(0).frame_id for _ in range(2)], [1, 2])
        self.assert_error(ErrorKind.STREAM_STOPPED, lambda: stream.receive(0))
        stream.close()
        session.close()

    def test_parent_first_lifetime_without_python_reference(self) -> None:
        session = self.open("success")
        session_reference = weakref.ref(session)
        stream = session.open_image_stream(self.config())
        self.assertFalse(any(value is session for value in vars(stream).values()))
        stream.start()
        session.close()
        del session
        gc.collect()
        self.assertIsNone(session_reference())
        self.assertNotIn("library_unloaded", self.lifecycle())
        self.assertEqual(stream.receive(1000).frame_id, 1)
        stream.close()
        self.assertTrue(self.lifecycle().endswith("library_unloaded\n"))

    def test_start_failure_poison_and_cleared_close(self) -> None:
        session = self.open("enable-fail")
        stream = session.open_image_stream(self.config())
        self.assert_error(ErrorKind.PROVIDER_FAILED, stream.start)
        self.assert_error(ErrorKind.PROVIDER_FAILED, stream.start)
        self.assert_error(ErrorKind.PROVIDER_FAILED, stream.stop)
        self.assert_error(ErrorKind.PROVIDER_FAILED, stream.close)
        self.assertFalse(stream.is_open)
        session.close()

    def test_malformed_frame_is_filtered_and_counted(self) -> None:
        session = self.open("unsupported-bpp")
        stream = session.open_image_stream(self.config())
        stream.start()
        self.assert_error(ErrorKind.TIMEOUT, lambda: stream.receive(20))
        counters = stream.counters()
        self.assertGreaterEqual(counters.frames_received, 1)
        self.assertGreaterEqual(counters.malformed_or_unsupported, 1)
        stream.close()
        session.close()

    def test_running_stream_finalizer_releases_provider(self) -> None:
        session = self.open("success")
        stream = session.open_image_stream(self.config())
        stream.start()
        session.close()
        reference = weakref.ref(stream)
        del stream
        gc.collect()
        self.assertIsNone(reference())
        self.assertTrue(self.lifecycle().endswith("library_unloaded\n"))

    def test_retryable_detach_close_retains_then_clears_owner(self) -> None:
        session = self.open("detach-fail")
        stream = session.open_image_stream(self.config())
        self.assert_error(ErrorKind.PROVIDER_FAILED, stream.close)
        self.assertTrue(stream.is_open)
        self.assert_error(ErrorKind.PROVIDER_FAILED, stream.close)
        self.assertFalse(stream.is_open)
        session.close()

    def test_stream_context_manager_does_not_start_and_closes_only_stream(self) -> None:
        session = self.open("idle")
        stream = session.open_image_stream(self.config())
        with stream as entered:
            self.assertIs(entered, stream)
            self.assert_error(ErrorKind.TIMEOUT, lambda: stream.receive(0))
            stream.start()
        self.assertFalse(stream.is_open)
        self.assertTrue(session.is_open)
        session.close()

    def test_configuration_validation(self) -> None:
        self.assert_error(ErrorKind.INVALID_ARGUMENT, lambda: UciId(b"short", "x"))
        base = self.config()
        strings = [
            lambda: ImageConfig.new(
                UciId(bytes(16), "bad\0channel"),
                base.platform_id, base.sensor_location,
            ),
            lambda: ImageConfig.new(
                base.channel_id, UciId(bytes(16), "bad\0platform"),
                base.sensor_location,
            ),
            lambda: ImageConfig.new(
                base.channel_id, base.platform_id,
                ComponentLocation(0.0, 0.0, 0.0, "bad\0key", "system"),
            ),
            lambda: ImageConfig.new(
                base.channel_id, base.platform_id,
                ComponentLocation(0.0, 0.0, 0.0, "key", "bad\0system"),
            ),
        ]
        for case in strings:
            self.assert_error(ErrorKind.INVALID_ARGUMENT, case)
        limits = [
            (0, 64, 4), (-1, 64, 4), (3, 0, 4), (3, -1, 4),
            (3, 64, 0), (3, 64, -1), (True, 64, 4),
            (1 << 64, 64, 4), (3, 1 << 64, 4), (3, 64, 1 << 64),
            ((1 << 63) + 1, 64, 4), (3, 1 << 63, 4),
        ]
        for count, size, capacity in limits:
            with self.subTest(count=count, size=size, capacity=capacity):
                self.assert_error(
                    ErrorKind.INVALID_ARGUMENT,
                    lambda: ImageConfig.new(
                        base.channel_id, base.platform_id, base.sensor_location,
                        count, size, capacity,
                    ),
                )
        self.assertEqual(self.lifecycle(), "")

    def test_timeout_input_validation_and_uint32_max(self) -> None:
        session = self.open("idle")
        stream = session.open_image_stream(self.config())
        for timeout in (-1, 1 << 32, True):
            self.assert_error(ErrorKind.INVALID_ARGUMENT, lambda value=timeout: stream.receive(value))
        self.assert_error(ErrorKind.TIMEOUT, lambda: stream.receive(0))
        stream.start()
        stream.stop()
        self.assert_error(ErrorKind.STREAM_STOPPED, lambda: stream.receive((1 << 32) - 1))
        stream.close()
        session.close()

    def test_direct_construction_is_rejected(self) -> None:
        with self.assertRaisesRegex(TypeError, "Session.open_image_stream"):
            ImageStream()


if __name__ == "__main__":
    unittest.main()
