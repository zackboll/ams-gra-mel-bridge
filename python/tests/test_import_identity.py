"""Runtime protection against package shadowing of the private ctypes module."""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

import ams_mel


class ImportIdentityTests(unittest.TestCase):
    def test_intended_private_module_is_loaded(self):
        expected = Path(ams_mel.__file__).parent / '_native.py'
        self.assertEqual(Path(ams_mel._native.__file__).resolve(), expected.resolve())
        self.assertIs(sys.modules['ams_mel._native'], ams_mel._native)
        self.assertFalse(hasattr(ams_mel._native, '__path__'))
        self.assertIsNotNone(ams_mel._native._LIBRARY)

    def test_identity_check_detects_added_shadow_package(self):
        with tempfile.TemporaryDirectory() as temp:
            package = Path(temp) / 'ams_mel'
            shutil.copytree(Path(ams_mel.__file__).parent, package,
                            ignore=shutil.ignore_patterns('__pycache__'))
            shadow = package / '_native'
            shadow.mkdir(exist_ok=True)
            # A functional-looking package must not evade the identity contract.
            shutil.copyfile(package / '_native.py', shadow / '__init__.py')
            script = (
                'import ams_mel, importlib; from pathlib import Path; '
                'm=importlib.import_module("ams_mel._native"); '
                'assert m is ams_mel._native; '
                'assert Path(m.__file__).resolve()==Path(ams_mel.__file__).with_name("_native.py").resolve(); '
                'assert m._LIBRARY is not None'
            )
            result = subprocess.run([sys.executable, '-W', 'error', '-c', script],
                                    cwd=temp, env={**os.environ, 'PYTHONPATH': temp},
                                    capture_output=True, text=True, timeout=30)
            self.assertNotEqual(result.returncode, 0, 'shadow package evaded identity check')
            self.assertIn('AssertionError', result.stderr)


if __name__ == '__main__':
    unittest.main()
