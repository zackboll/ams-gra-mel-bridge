"""Public compatibility snapshot captured before PY-PKG-001 extraction."""

import ast
import dataclasses
import enum
import importlib
import inspect
import json
import os
from pathlib import Path
import pickle
import shutil
import subprocess
import sys
import tempfile
import unittest

import ams_mel as mel


def public_snapshot():
    """Process-independent identities, signatures, fields and enum representations."""
    symbols = {}
    for name in mel.__all__:
        value = getattr(mel, name)
        entry = {"name": value.__name__, "qualname": value.__qualname__,
                 "module": value.__module__}
        if inspect.isclass(value):
            methods = {}
            for member in dir(value):
                if member.startswith("_") and member not in (
                    "__init__", "__enter__", "__exit__",
                ):
                    continue
                method = getattr(value, member)
                if isinstance(inspect.getattr_static(value, member), property):
                    methods[member] = "property"
                elif callable(method) and (getattr(method, "__module__", None) or "").startswith("ams_mel"):
                    try:
                        methods[member] = str(inspect.signature(method))
                    except (ValueError, TypeError):
                        methods[member] = None
            entry["methods"] = methods
            if issubclass(value, enum.Enum):
                entry["members"] = [
                    [key, item.value, repr(item), str(item)]
                    for key, item in value.__members__.items()
                ]
            if dataclasses.is_dataclass(value):
                entry["fields"] = [
                    [field.name, str(field.type), repr(field.default),
                     field.init, field.kw_only]
                    for field in dataclasses.fields(value)
                ]
                # MISSING's default repr contains a process-specific address.
                for field, record in zip(dataclasses.fields(value), entry["fields"]):
                    if field.default is dataclasses.MISSING:
                        record[2] = "MISSING"
                entry["frozen"] = value.__dataclass_params__.frozen
        else:
            entry["signature"] = str(inspect.signature(value))
        symbols[name] = entry
    return {"all": mel.__all__, "symbols": symbols,
            "aliases": [[left, right] for i, left in enumerate(mel.__all__)
                        for right in mel.__all__[i + 1:]
                        if getattr(mel, left) is getattr(mel, right)]}


class PackageCompatibilityTests(unittest.TestCase):
    def test_exports_are_defining_module_objects(self):
        # Public __module__ is deliberately rewritten, so find definitions in
        # implementation source rather than trusting that attribute or a fixed
        # inventory that would need editing whenever an export is added.
        definitions = {}
        for path in Path(mel.__file__).parent.glob("_*.py"):
            if path.name == "__init__.py":
                continue
            implementation = importlib.import_module(f"ams_mel.{path.stem}")
            for node in ast.parse(path.read_text()).body:
                if isinstance(node, (ast.ClassDef, ast.FunctionDef, ast.AsyncFunctionDef)):
                    definitions.setdefault(node.name, []).append(
                        getattr(implementation, node.name)
                    )
        for name in mel.__all__:
            with self.subTest(export=name):
                value = getattr(mel, name)
                self.assertEqual(value.__module__, "ams_mel")
                self.assertTrue(
                    any(value is definition
                        for definition in definitions.get(value.__name__, ())),
                    f"{name} is not its defining-module object",
                )

    def test_internal_import_graph_is_acyclic_and_not_public(self):
        package = Path(mel.__file__).parent
        graph = {}
        for path in package.glob("_*.py"):
            if path.name == "__init__.py":
                continue
            dependencies = set()
            for node in ast.walk(ast.parse(path.read_text())):
                if isinstance(node, ast.ImportFrom):
                    if node.level:
                        self.assertEqual(node.level, 1)
                        if node.module:
                            self.assertNotEqual(node.module, "__init__")
                            dependencies.add(node.module)
                        else:
                            self.assertTrue(all(alias.name.startswith("_")
                                                for alias in node.names))
                            dependencies.update(alias.name for alias in node.names)
                    else:
                        self.assertNotEqual(node.module, "ams_mel")
                elif isinstance(node, ast.Import):
                    self.assertTrue(all(not alias.name.startswith("ams_mel")
                                        for alias in node.names))
            graph[path.stem] = dependencies

        def visit(module, ancestors):
            self.assertNotIn(module, ancestors, f"import cycle: {ancestors} -> {module}")
            self.assertIn(module, graph)
            for dependency in graph[module]:
                visit(dependency, ancestors + (module,))

        for module in graph:
            visit(module, ())
        # A fresh interpreter must initialize the package and every implementation.
        script = "import importlib; " + "; ".join(
            f"importlib.import_module('ams_mel.{module}')" for module in reversed(graph)
        )
        subprocess.run([sys.executable, "-W", "error", "-c", script],
                       check=True, capture_output=True, text=True, timeout=30)

    def test_original_public_snapshot(self):
        baseline = json.loads(Path(__file__).with_name("public_api_baseline.json").read_text())
        current = public_snapshot()
        original = baseline["api"]
        # Preserve the original export order, but allow additions anywhere.
        self.assertEqual([name for name in current["all"] if name in original["all"]],
                         original["all"])
        for name, expected in original["symbols"].items():
            with self.subTest(export=name):
                self.assertIn(name, current["symbols"])
                actual = current["symbols"][name]
                self.assertEqual(actual.keys(), expected.keys())
                for contract, value in expected.items():
                    if contract == "methods":
                        for method, signature in value.items():
                            self.assertIn(method, actual["methods"], f"{name}.{method}")
                            self.assertEqual(actual["methods"][method], signature,
                                             f"{name}.{method}")
                    else:
                        self.assertEqual(actual[contract], value, f"{name}.{contract}")
        self.assertEqual([pair for pair in current["aliases"]
                          if all(name in original["all"] for name in pair)],
                         original["aliases"])

    def _check_modified_package(self, exports, implementation=""):
        """Run the real compatibility/identity checks on an isolated package copy."""
        with tempfile.TemporaryDirectory() as directory:
            package = Path(directory) / "ams_mel"
            shutil.copytree(Path(mel.__file__).parent, package,
                            ignore=shutil.ignore_patterns("__pycache__"))
            with (package / "_types.py").open("a") as stream:
                stream.write("\n" + implementation + "\n")
            with (package / "__init__.py").open("a") as stream:
                stream.write("\n" + exports + "\n")
            return subprocess.run(
                [sys.executable, "-W", "error", "-m", "unittest", "-v",
                 "test_package_layout.PackageCompatibilityTests.test_original_public_snapshot",
                 "test_package_layout.PackageCompatibilityTests.test_exports_are_defining_module_objects"],
                cwd=directory,
                env={**os.environ, "PYTHONPATH": os.pathsep.join(
                    (directory, str(Path(__file__).resolve().parent)))},
                capture_output=True, text=True, timeout=30,
            )

    def test_additive_public_api_is_accepted(self):
        result = self._check_modified_package(
            'from ._types import AdditiveValue\n'
            'AdditiveValue.__module__ = __name__\n'
            '__all__.insert(0, "AdditiveValue")',
            'class AdditiveValue:\n'
            '    pass\n'
            'def _additive_method(self):\n'
            '    return None\n'
            'AbiVersion.additive_method = _additive_method',
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_original_api_removal_or_incompatible_change_is_rejected(self):
        changes = {
            "export removal": '__all__.remove("Session")',
            "method removal": 'del Session.provider_version',
            "method signature": 'Session.close = lambda self, required: None',
            "public module": 'Session.__module__ = "ams_mel._session"',
            "enum numeric value": 'SensorType.GIMBAL_HORIZONTAL._value_ = 42',
            "dataclass default": 'AbiVersion.__dataclass_fields__["major"].default = 0',
            "dataclass frozen": 'AbiVersion.__dataclass_params__.frozen = False',
        }
        for contract, change in changes.items():
            with self.subTest(contract=contract):
                result = self._check_modified_package(change)
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("FAIL: test_original_public_snapshot", result.stderr)

    def test_new_export_must_be_its_implementation_object(self):
        result = self._check_modified_package(
            'class AdditiveValue:\n'
            '    pass\n'
            '__all__.append("AdditiveValue")',
            'class AdditiveValue:\n'
            '    pass',
        )
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("test_original_public_snapshot) ... ok", result.stderr)
        self.assertIn("FAIL: test_exports_are_defining_module_objects", result.stderr)

    def test_pickle_values_keep_public_identity(self):
        values = (
            mel.SensorType(0xFFFFFFFF), mel.ErrorKind.TIMEOUT,
            mel.MFAState.STANDBY, mel.InstrumentationPriority(0),
            mel.AbiVersion(0, 1), mel.UciId(bytes(16), "µ"),
            mel.HealthConfig(mel.UciId(bytes(16), "channel"),
                             mel.UciId(bytes(16), "platform"),
                             mel.ComponentLocation(1.0, 2.0, 3.0, "key", "system")),
            mel.InstrumentationReport(0xFFFFFFFF, 1, -1, 0),
            mel.CommsCompleted(mel.CommsTestReport(1, 2)),
            mel.BandInfo(mel.BandType.UVA, 0.0, 1.0),
        )
        for value in values:
            with self.subTest(value=value):
                restored = pickle.loads(pickle.dumps(value))
                self.assertIs(type(restored), type(value))
                self.assertEqual(restored, value)
                self.assertEqual(type(restored).__module__, "ams_mel")

    def test_error_mapping_and_validation_snapshot(self):
        baseline = json.loads(Path(__file__).with_name("public_api_baseline.json").read_text())
        self.assertEqual(error_snapshot(), baseline["errors"])


def error_snapshot():
    # Native status conversion is private; its observable exceptions are not.
    mapping = mel.MelError.__init__.__globals__["_error_from_status"]
    errors = []
    for status in (*range(1, 13), 12345):
        error = mapping(status, "diagnostic µ", 14)
        errors.append([error.kind.value, error.diagnostic,
                       error.diagnostic_required, error.native_status,
                       str(error), list(error.args)])
    for factory in (
        lambda: mel.UciId(b"short", "label"),
        lambda: mel.UciId(bytes(16), "nul\0"),
        lambda: mel.CommsTestRequest(True, 1, 2),
        lambda: mel.InstrumentationReport(1, 2, 1 << 63, 0),
        mel.Session, mel.RFAdmin, mel.HealthStatusChannel,
    ):
        try:
            factory()
        except Exception as error:
            errors.append([type(error).__name__, str(error),
                           getattr(getattr(error, "kind", None), "value", None)])
        else:
            raise AssertionError("invalid construction unexpectedly succeeded")
    return errors


if __name__ == "__main__":
    unittest.main()
