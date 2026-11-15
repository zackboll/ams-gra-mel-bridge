"""Private Buffer foundation mirrors only; no safe Python API."""
import ctypes as c
from pathlib import Path
import subprocess
import tempfile
import unittest
from ams_mel import _native as n

class BufferABI(unittest.TestCase):
    def test_shapes(self):
        p = c.POINTER
        signatures = {
            "open": [n.SessionHandle, c.c_uint64, c.c_int64, p(n.IrBufferHandle), p(c.c_uint32)],
            "get_info": [n.IrBufferHandle, p(n.IrBufferInfoV1)],
            "get_flags": [n.IrBufferHandle, p(c.c_uint32), c.c_size_t, p(c.c_size_t), p(c.c_uint32)],
            "set_flags": [n.IrBufferHandle, p(c.c_uint32), c.c_size_t],
            "add_flag": [n.IrBufferHandle, c.c_uint32],
            "read_storage": [n.IrBufferHandle, c.c_uint64, p(c.c_uint8), c.c_size_t],
            "write_storage": [n.IrBufferHandle, c.c_uint64, p(c.c_uint8), c.c_size_t],
            "close": [p(n.IrBufferHandle)],
        }
        for operation, arguments in signatures.items():
            name = "ams_mel_ir_buffer_" + operation
            function = getattr(n, name)
            self.assertEqual(function.argtypes, arguments + [n.CharPointer, c.c_size_t, n.SizePointer])
            self.assertIs(function.restype, c.c_int32)
            self.assertEqual(n.BOUND_FUNCTION_NAMES.count(name), 1)
        import ams_mel
        self.assertFalse(hasattr(ams_mel, "HostBuffer"))

    def test_layout(self):
        root = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "probe"
            subprocess.run(["cc", "-std=c11", "-pedantic-errors", "-Wall", "-Wextra", "-Werror",
                            "-I" + str(root / "native/include"), str(root / "native/tests/buffer_layout_probe.c"),
                            "-o", str(executable)], check=True)
            actual = list(map(int, subprocess.check_output([str(executable)], text=True).split()))
        info = n.IrBufferInfoV1
        self.assertEqual(actual, [c.sizeof(info), c.alignment(info)] +
                         [getattr(info, field).offset for field, _ in info._fields_] +
                         [n.AMS_MEL_IR_BUFFER_HOST, n.AMS_MEL_IR_BUFFER_GPU, n.AMS_MEL_IR_BUFFER_RDMA])
