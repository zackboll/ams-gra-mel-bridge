"""Runtime-reference qualification tests without a native library or provider."""

import copy
import hashlib
import os
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest.mock import patch

from qualify_python_reference import QualificationError, qualify, qualify_inventory, resolve_default


SOURCE = '''from __future__ import annotations
from dataclasses import dataclass, field
from enum import Enum

class Choice(Enum):
    A = "a"
    ALIAS = "a"
    B = "b"

def _items():
    raise AssertionError("default factory must not be called")

@dataclass(frozen=True)
class Value:
    required: int
    mode: Choice = Choice.A
    items: list[int] = field(default_factory=_items, kw_only=True)

class _Owner:
    @property
    def is_open(self) -> bool:
        raise AssertionError("property must not be evaluated")
    def close(self) -> None:
        raise AssertionError("owner must not be called")
    def __enter__(self) -> _Owner:
        raise AssertionError("owner must not be entered")
    def __exit__(self, kind: object, error: object, traceback: object) -> bool:
        raise AssertionError("owner must not be exited")

class Owner(_Owner):
    def __init__(self, size: int = 3) -> None:
        raise AssertionError("owner must not be constructed")
    @classmethod
    def open(cls, *, option: bool = False) -> Owner:
        raise AssertionError("provider must not be opened")
    @staticmethod
    def calculate(value: int, /, increment: int = 0) -> int:
        raise AssertionError("static method must not be called")

def query(value: int = 16, *, mode: Choice = Choice.A) -> Value:
    raise AssertionError("public function must not be called")
'''


def parameter(name, annotation=None, default=None, kind="positional_or_keyword"):
    return dict(name=name, annotation=annotation, default=default, kind=kind)


def callable_record(name, parameters, returns="None", *, binding="instance", owner="Owner", inherited=False):
    return dict(name=name, parameters=parameters, returns=returns, binding=binding,
                declaring_class=owner, inherited=inherited)


def fixture_inventory():
    def export(name, kind, **extra):
        return dict(name=name, kind=kind, definition_module="ams_mel._fixture",
                    definition_name=name, members=[], constructor=None, **extra)

    choice = export("Choice", "enum", bases=["Enum"], enum_members=[
        dict(name="A", value="a"), dict(name="ALIAS", value="a"), dict(name="B", value="b")])
    value = export("Value", "dataclass", bases=[], frozen=True, fields=[
        dict(name="required", annotation="int", default=None, default_factory=None, init=True, kw_only=False),
        dict(name="mode", annotation="Choice", default="Choice.A", default_factory=None, init=True, kw_only=False),
        dict(name="items", annotation="list[int]", default=None, default_factory="_items", init=True, kw_only=True),
    ])
    alias = {**copy.deepcopy(value), "name": "ValueAlias"}
    owner = export("Owner", "class", bases=["_Owner"])
    owner["constructor"] = callable_record("__init__", [parameter("self"), parameter("size", "int", "3")])
    owner["members"] = [
        callable_record("is_open", [parameter("self")], "bool", binding="property", owner="_Owner", inherited=True),
        callable_record("close", [parameter("self")], owner="_Owner", inherited=True),
        callable_record("__enter__", [parameter("self")], "_Owner", owner="_Owner", inherited=True),
        callable_record("__exit__", [parameter("self"), parameter("kind", "object"),
                                   parameter("error", "object"), parameter("traceback", "object")],
                        "bool", owner="_Owner", inherited=True),
        callable_record("open", [parameter("cls"), parameter("option", "bool", "False", "keyword_only")],
                        "Owner", binding="class"),
        callable_record("calculate", [parameter("value", "int", kind="positional_only"),
                                      parameter("increment", "int", "0")], "int", binding="static"),
    ]
    query = export("query", "function")
    query.update(callable_record("query", [parameter("value", "int", "16"),
                                           parameter("mode", "Choice", "Choice.A", "keyword_only")],
                                 "Value", binding="function"))
    return dict(schema=1, module="ams_mel", exports=[choice, value, alias, owner, query])


class QualificationTests(unittest.TestCase):
    def setUp(self):
        self.public = types.ModuleType("ams_mel")
        self.implementation = types.ModuleType("ams_mel._fixture")
        self.modules = {"ams_mel": self.public, "ams_mel._fixture": self.implementation}
        with patch.dict(sys.modules, self.modules):
            exec(compile(SOURCE, "qualification_fixture.py", "exec"), vars(self.implementation))
        self.inventory = fixture_inventory()
        self.public.__all__ = [record["name"] for record in self.inventory["exports"]]
        for record in self.inventory["exports"]:
            value = getattr(self.implementation, record["definition_name"])
            value.__module__ = "ams_mel"
            setattr(self.public, record["name"], value)

    def qualify(self, inventory=None):
        return qualify_inventory(self.inventory if inventory is None else inventory,
                                 self.public, self.modules)

    def add_public_modules(self):
        self.inventory["public_modules"] = []
        for short_name, exports in (("common", ["Choice", "Value", "ValueAlias"]),
                                    ("ir", ["Owner", "query"])):
            name = "ams_mel." + short_name
            module = types.ModuleType(name)
            module.__all__ = list(exports)
            for export in exports:
                setattr(module, export, vars(self.public)[export])
            setattr(self.public, short_name, module)
            self.modules[name] = module
            self.inventory["public_modules"].append(dict(
                name=name, source="python/ams_mel/" + short_name + ".py", doc="", exports=exports))

    def test_complete_inventory_without_calling_owners_properties_or_factories(self):
        result = self.qualify()
        self.assertEqual(result, dict(exports=5, aliases=1, callables=10, methods=5,
                                     properties=1, dataclasses=2, dataclass_fields=6,
                                     generated_dataclass_constructors=2,
                                     enums=1, enum_members=3,
                                     public_modules=0, public_module_exports=0))

    def test_public_modules_reuse_canonical_objects_without_duplicate_api_counts(self):
        self.add_public_modules()
        result = self.qualify()
        self.assertEqual(result["public_modules"], 2)
        self.assertEqual(result["public_module_exports"], 5)
        self.assertEqual(result["exports"], 5)
        self.assertEqual(result["aliases"], 1)
        self.assertEqual(result["dataclasses"], 2)
        self.assertIs(self.public.common.Value, self.public.Value)
        self.assertIs(self.public.common.ValueAlias, self.public.Value)
        self.assertEqual(self.public.common.Value.__module__, "ams_mel")

    def test_public_module_export_omission_order_identity_and_unknown_names_fail(self):
        self.add_public_modules()
        common = self.public.common
        for mutation in (lambda module: module.__all__.pop(),
                         lambda module: module.__all__.reverse(),
                         lambda module: setattr(module, "Value", self.public.Choice)):
            with self.subTest(mutation=mutation):
                with patch.dict(vars(common), {"__all__": list(common.__all__)}):
                    mutation(common)
                    with self.assertRaises(QualificationError):
                        self.qualify()
        for mutation in (lambda records: records[0]["exports"].append("query"),
                         lambda records: records[0]["exports"].append("private_operation"),
                         lambda records: records[0]["exports"].append("Choice")):
            with self.subTest(mutation=mutation):
                changed = copy.deepcopy(self.inventory)
                mutation(changed["public_modules"])
                with self.assertRaises(QualificationError):
                    self.qualify(changed)

    def test_public_module_record_identity_and_parent_binding_fail(self):
        self.add_public_modules()
        for mutation in (lambda records: records.append(copy.deepcopy(records[0])),
                         lambda records: records[0].update(name="other.common"),
                         lambda records: records[0].update(name="ams_mel._private"),
                         lambda records: records[0].update(name="ams_mel.ir.nested"),
                         lambda records: records[0].update(name="ams_mel.missing")):
            with self.subTest(mutation=mutation):
                changed = copy.deepcopy(self.inventory)
                mutation(changed["public_modules"])
                with self.assertRaises(QualificationError):
                    self.qualify(changed)
        with patch.object(self.public.common, "__name__", "other.common"):
            with self.assertRaisesRegex(QualificationError, "public module identity"):
                self.qualify()
        with patch.object(self.public, "common", self.public.ir):
            with self.assertRaisesRegex(QualificationError, "parent package binding"):
                self.qualify()

    def test_missing_public_module_exports_do_not_invoke_attribute_hooks(self):
        self.add_public_modules()

        def forbidden(name):
            self.fail("qualification executed a module attribute hook: " + name)

        del self.public.common.Value
        self.public.common.__getattr__ = forbidden
        with self.assertRaisesRegex(QualificationError, "canonical export identity"):
            self.qualify()

    def test_checkout_requires_public_module_inventory_and_exact_source_paths_before_import(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            package = root / "python/ams_mel"
            package.mkdir(parents=True)
            (package / "__init__.py").write_text("# Package fixture.\n", encoding="utf-8")
            (package / "common.py").write_text("# Namespace fixture.\n", encoding="utf-8")
            native = root / "native-fixture.so"
            native.write_bytes(b"Not loaded: namespace inventory is checked before imports.")
            sources = {path.relative_to(root).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                       for path in package.glob("*.py")}
            base = dict(sources=sources)
            mutations = (
                ([], "inventory names"),
                ([dict(name="ams_mel.common", source="python/ams_mel/_other.py")], "source paths"),
                ([dict(name="ams_mel.common", source="python/ams_mel/common.py")] * 2, "inventory names"),
            )
            with patch.dict(os.environ, {"AMS_MEL_NATIVE_LIB": str(native)}):
                with patch("qualify_python_reference.importlib.import_module",
                           side_effect=AssertionError("package must not be imported")):
                    for records, message in mutations:
                        with self.subTest(records=records):
                            with self.assertRaisesRegex(QualificationError, message):
                                qualify(root, {**base, "public_modules": records})

    def test_omitted_or_reordered_exports_and_wrong_identity_fail(self):
        original = self.inventory
        for mutation in (lambda rows: rows.pop(), lambda rows: rows.reverse(),
                         lambda rows: rows[2].update(definition_name="Owner")):
            with self.subTest(mutation=mutation):
                changed = copy.deepcopy(original)
                mutation(changed["exports"])
                with self.assertRaises(QualificationError):
                    self.qualify(changed)

    def test_signature_parameter_kind_annotation_default_and_return_fail(self):
        for key, value in (("name", "renamed"), ("kind", "positional_only"),
                           ("annotation", "str"), ("default", "17"), ("default", None)):
            with self.subTest(key=key, value=value):
                changed = copy.deepcopy(self.inventory)
                changed["exports"][-1]["parameters"][0][key] = value
                with self.assertRaises(QualificationError):
                    self.qualify(changed)
        changed = copy.deepcopy(self.inventory)
        changed["exports"][-1]["returns"] = "str"
        with self.assertRaisesRegex(QualificationError, "return annotation"):
            self.qualify(changed)

    def test_annotation_syntax_is_normalized_without_evaluation(self):
        self.inventory["exports"][-1]["parameters"][0]["annotation"] = "(int)"
        self.qualify()

    def test_inherited_member_omission_binding_and_owner_fail(self):
        mutations = (
            lambda owner: owner["members"].pop(0),
            lambda owner: owner["members"][0].update(binding="instance"),
            lambda owner: owner["members"][0].update(inherited=False),
            lambda owner: owner["members"][0].update(declaring_class="Owner"),
            lambda owner: owner.update(constructor=None),
        )
        for mutate in mutations:
            with self.subTest(mutation=mutate):
                changed = copy.deepcopy(self.inventory)
                mutate(changed["exports"][3])
                with self.assertRaises(QualificationError):
                    self.qualify(changed)

    def test_dataclass_field_default_factory_flags_and_frozen_fail(self):
        mutations = (
            lambda value: value["fields"].pop(),
            lambda value: value["fields"][1].update(default="Choice.B"),
            lambda value: value["fields"][1].update(default=None),
            lambda value: value["fields"][2].update(default_factory="list"),
            lambda value: value["fields"][2].update(kw_only=False),
            lambda value: value["fields"][0].update(init=False),
            lambda value: value.update(frozen=False),
        )
        for mutate in mutations:
            with self.subTest(mutation=mutate):
                changed = copy.deepcopy(self.inventory)
                mutate(changed["exports"][1])
                with self.assertRaises(QualificationError):
                    self.qualify(changed)

    def test_enum_alias_omission_wrong_value_and_value_type_fail(self):
        mutations = (
            lambda choice: choice["enum_members"].pop(1),
            lambda choice: choice["enum_members"][0].update(value="other"),
            lambda choice: choice["enum_members"][0].update(value=1),
        )
        for mutate in mutations:
            with self.subTest(mutation=mutate):
                changed = copy.deepcopy(self.inventory)
                mutate(changed["exports"][0])
                with self.assertRaises(QualificationError):
                    self.qualify(changed)

    def test_generated_dataclass_signature_is_checked_independently_of_fields(self):
        initializer = self.public.Value.__init__
        with patch.object(initializer, "__defaults__", (self.public.Choice.B,)):
            with self.assertRaisesRegex(QualificationError, "default differs"):
                self.qualify()
        with patch.object(initializer, "__kwdefaults__", {"items": None}):
            with self.assertRaisesRegex(QualificationError, "generated factory default differs"):
                self.qualify()

    def test_default_expressions_cannot_call_code_or_properties(self):
        def forbidden():
            self.fail("qualification executed a default expression")
        with self.assertRaisesRegex(QualificationError, "unsupported default"):
            resolve_default("forbidden()", {"forbidden": forbidden})
        with self.assertRaisesRegex(QualificationError, "descriptor"):
            resolve_default("Owner.is_open", {"Owner": self.public.Owner})

    def test_bool_and_integer_defaults_are_distinct(self):
        self.inventory["exports"][3]["members"][-1]["parameters"][1]["default"] = "False"
        with self.assertRaisesRegex(QualificationError, "default differs"):
            self.qualify()


if __name__ == "__main__":
    unittest.main()
