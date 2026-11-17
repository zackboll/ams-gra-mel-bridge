"""IRMEL-ADA-021 private ABI mirrors; no safe Python checkout API."""
import ctypes as c
from pathlib import Path
import subprocess
import tempfile
import unittest
from ams_mel import _native as n


class CheckoutABI(unittest.TestCase):
    def test_signatures(self):
        p = c.POINTER
        shapes = {
            "open": [n.IrStreamHandle, c.c_size_t, c.c_size_t, p(n.IrImageCheckoutHandle)],
            "receive": [n.IrImageCheckoutHandle, c.c_uint32, p(n.IrImageCheckoutLeaseHandle)],
            "get_counters": [n.IrImageCheckoutHandle, p(n.IrImageCheckoutCountersV1)],
            "close": [p(n.IrImageCheckoutHandle)],
            "lease_close": [p(n.IrImageCheckoutLeaseHandle), p(c.c_uint32)],
        }
        for name, arguments in shapes.items():
            function = getattr(n, "ams_mel_ir_image_checkout_" + name)
            self.assertEqual(function.argtypes, arguments + [n.CharPointer, c.c_size_t, n.SizePointer])
            self.assertIs(function.restype, c.c_int32)

    def test_layout(self):
        root = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "probe"
            subprocess.run(["cc", "-std=c11", "-pedantic-errors", "-Wall", "-Wextra", "-Werror",
                            "-I" + str(root / "native/include"), str(root / "native/tests/checkout_layout_probe.c"),
                            "-o", str(executable)], check=True)
            actual = list(map(int, subprocess.check_output([str(executable)], text=True).split()))
        record = n.IrImageCheckoutCountersV1
        self.assertEqual(actual, [c.sizeof(record), c.alignment(record)] +
                         [getattr(record, field).offset for field, _ in record._fields_])
