"""Safe submission tests; synchronization imports are test-only, never public API."""

import ctypes
import os
import subprocess
import sys
from pathlib import Path
import unittest

from ams_mel import (
    CommandReturn,
    CommsTestRequest,
    ErrorKind,
    MelError,
    MelErrorCode,
    ModeRejected,
    ReturnCompleted,
    ReturnRequest,
    Session,
    _native,
)
import test_control_channel


class AdmissionTests(unittest.TestCase):
    def test_bounded_admission_and_default_compatibility(self) -> None:
        # Isolate the deliberately retained ctypes DSO handle and mock flags
        # from the existing library-unload tests in this interpreter.
        if os.environ.get("AMS_MEL_ADMISSION_CHILD") != "1":
            environment = dict(os.environ, AMS_MEL_ADMISSION_CHILD="1")
            environment["PYTHONPATH"] = os.pathsep.join(sys.path)
            result = subprocess.run(
                [sys.executable, "-W", "error", "-m", "unittest",
                 "test_admission.AdmissionTests.test_bounded_admission_and_default_compatibility"],
                env=environment, capture_output=True, text=True, timeout=60,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            return
        provider_path = Path(os.environ["AMS_MEL_TEST_PROVIDER_DIR"]) / "libmock_ir_provider.so"
        provider = ctypes.CDLL(str(provider_path))
        gate = provider.mock_completion_gate
        gate.argtypes = [ctypes.c_uint, ctypes.c_uint64,
                         ctypes.POINTER(ctypes.c_uint64), ctypes.POINTER(ctypes.c_uint64)]
        gate.restype = ctypes.c_int
        boundary = _native._LIBRARY.ams_mel_test_completion_boundary
        boundary.argtypes = [ctypes.c_uint, ctypes.c_uint, ctypes.c_uint64,
                             ctypes.POINTER(ctypes.c_uint64)]
        boundary.restype = ctypes.c_int
        owner = _native._LIBRARY.ams_mel_test_completion_owner
        owner.argtypes = [ctypes.c_uint, ctypes.c_uint64, ctypes.POINTER(ctypes.c_uint64)]
        owner.restype = ctypes.c_int

        def reclaimed() -> None:
            for family in (0, 1):
                counts = (ctypes.c_uint64 * 4)()
                observed = ctypes.c_uint64()
                self.assertEqual(boundary(family, 3, 0, counts), 1)
                self.assertEqual(owner(family, counts[2], ctypes.byref(observed)), 1)

        for options in ({}, {"max_async_requests": 0}, {"max_async_requests": 1}):
            with Session.open(provider_path, "completion-scale", "", **options) as session:
                with session.open_control_channel(test_control_channel.ControlChannelTests.config()) as channel:
                    channel.enable()
                    requests = [channel.submit_operate(1)]
                    try:
                        if options.get("max_async_requests") == 1:
                            for submit in (channel.submit_operate, channel.submit_bit_noop):
                                with self.assertRaises(MelError) as caught:
                                    submit(2)
                                self.assertEqual(caught.exception.kind, ErrorKind.RESOURCE_EXHAUSTED)
                                self.assertIn("async request limit reached", str(caught.exception))
                            requests[0].close()
                            with self.assertRaises(MelError) as caught:
                                channel.submit_operate(3)
                            self.assertEqual(caught.exception.kind, ErrorKind.RESOURCE_EXHAUSTED)
                        else:
                            requests.append(channel.submit_bit_noop(2))
                    finally:
                        self.assertEqual(gate(2, len(requests), None, None), 1)
                        reclaimed()
                        for request in requests:
                            request.close()
                    retry = channel.submit_operate(4)
                    self.assertEqual(gate(2, 1, None, None), 1)
                    retry.wait(15000)
                    reclaimed()
                    retry.close()

        # The provider's resource rejection is a terminal Mode result, not a
        # submission error. The mock flag applies only to completion-scale Mode.
        self.assertEqual(gate(4, 0, None, None), 1)
        with Session.open(provider_path, "completion-scale", "", max_async_requests=1) as session:
            with session.open_control_channel(test_control_channel.ControlChannelTests.config()) as channel:
                channel.enable()
                request = channel.submit_operate(5)
                self.assertEqual(gate(2, 1, None, None), 1)
                self.assertEqual(request.wait(15000), ModeRejected(
                    MelErrorCode.INSUFFICIENT_RESOURCES, "provider resource rejection"))
                reclaimed()
                request.close()

    def test_common_channel_view_shares_session_admission(self) -> None:
        """Task 032B4: ChannelView KeepAlive/CommsTest share Session admission.

        Uses only the existing child-process isolation, completion-scale
        scenario, and completion gate; no new mock behavior.
        """
        if os.environ.get("AMS_MEL_ADMISSION_CHILD") != "1":
            environment = dict(os.environ, AMS_MEL_ADMISSION_CHILD="1")
            environment["PYTHONPATH"] = os.pathsep.join(sys.path)
            result = subprocess.run(
                [sys.executable, "-W", "error", "-m", "unittest",
                 "test_admission.AdmissionTests.test_common_channel_view_shares_session_admission"],
                env=environment, capture_output=True, text=True, timeout=60,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            return
        provider_path = Path(os.environ["AMS_MEL_TEST_PROVIDER_DIR"]) / "libmock_ir_provider.so"
        provider = ctypes.CDLL(str(provider_path))
        gate = provider.mock_completion_gate
        gate.argtypes = [ctypes.c_uint, ctypes.c_uint64,
                         ctypes.POINTER(ctypes.c_uint64), ctypes.POINTER(ctypes.c_uint64)]
        gate.restype = ctypes.c_int
        boundary = _native._LIBRARY.ams_mel_test_completion_boundary
        boundary.argtypes = [ctypes.c_uint, ctypes.c_uint, ctypes.c_uint64,
                             ctypes.POINTER(ctypes.c_uint64)]
        boundary.restype = ctypes.c_int
        owner = _native._LIBRARY.ams_mel_test_completion_owner
        owner.argtypes = [ctypes.c_uint, ctypes.c_uint64, ctypes.POINTER(ctypes.c_uint64)]
        owner.restype = ctypes.c_int

        def reclaimed() -> None:
            for family in (0, 1):
                counts = (ctypes.c_uint64 * 4)()
                observed = ctypes.c_uint64()
                self.assertEqual(boundary(family, 3, 0, counts), 1)
                self.assertEqual(owner(family, counts[2], ctypes.byref(observed)), 1)

        def exhausted(call: object) -> None:
            with self.assertRaises(MelError) as caught:
                call()  # type: ignore[operator]
            self.assertEqual(caught.exception.kind, ErrorKind.RESOURCE_EXHAUSTED)
            self.assertIn("async request limit reached", str(caught.exception))

        success = ReturnCompleted(CommandReturn.SUCCESS)
        comms = CommsTestRequest(1, 2, 3)
        with Session.open(provider_path, "completion-scale", "", max_async_requests=1) as session:
            channel = session.open_control_channel(test_control_channel.ControlChannelTests.config())
            channel.enable()
            view = channel.channel_view()

            # Common KeepAlive holds the only permit.
            keepalive = view.send_keepalive()
            self.assertIsInstance(keepalive, ReturnRequest)
            exhausted(lambda: channel.submit_bit_noop(10))
            exhausted(lambda: channel.submit_operate(11))
            exhausted(lambda: view.submit_comms_test(comms))
            # Closing the public owner before provider release returns nothing.
            keepalive.close()
            exhausted(lambda: channel.submit_bit_noop(12))
            exhausted(lambda: view.send_keepalive())
            self.assertEqual(gate(2, 1, None, None), 1)
            reclaimed()
            retry = channel.submit_bit_noop(13)
            self.assertEqual(gate(2, 1, None, None), 1)
            self.assertEqual(retry.wait(15000), success)
            reclaimed()
            retry.close()

            # Reverse: typed BIT holds the permit; common KeepAlive is refused.
            bit = channel.submit_bit_noop(14)
            exhausted(view.send_keepalive)
            self.assertEqual(gate(2, 1, None, None), 1)
            self.assertEqual(bit.wait(15000), success)
            reclaimed()
            bit.close()
            common_retry = view.send_keepalive()
            self.assertEqual(gate(2, 1, None, None), 1)
            self.assertEqual(common_retry.wait(15000), success)
            reclaimed()
            common_retry.close()

            # Cross-family: a common Image KeepAlive on the same Session.
            stream = session.open_image_stream(test_control_channel.ControlChannelTests.image_config())
            image_view = stream.channel_view()
            image_keepalive = image_view.send_keepalive()
            exhausted(lambda: channel.submit_bit_noop(15))
            exhausted(view.send_keepalive)
            self.assertEqual(gate(2, 1, None, None), 1)
            self.assertEqual(image_keepalive.wait(15000), success)
            reclaimed()
            image_keepalive.close()
            typed = channel.submit_bit_noop(16)
            self.assertEqual(gate(2, 1, None, None), 1)
            self.assertEqual(typed.wait(15000), success)
            reclaimed()
            typed.close()
            cross_retry = view.send_keepalive()
            self.assertEqual(gate(2, 1, None, None), 1)
            self.assertEqual(cross_retry.wait(15000), success)
            reclaimed()
            cross_retry.close()

            image_view.close()
            view.close()
            stream.close()
            channel.close()

    def test_invalid_limits(self) -> None:
        for value in (-1, 2**32, True, 1.5, "1"):
            with self.subTest(value=value), self.assertRaises(MelError) as caught:
                Session.open("unused", "", "", max_async_requests=value)
            self.assertEqual(caught.exception.kind, ErrorKind.INVALID_ARGUMENT)
