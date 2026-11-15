#!/usr/bin/env python3
"""Compare the static Python reference with the real, imported public API.

This is an explicit local/test qualification task, separate from documentation
generation and deployment. Set AMS_MEL_NATIVE_LIB to an already built facade.
Reflection never constructs an owner, calls a public operation, evaluates an
annotation, or invokes a dataclass default factory.
"""

from __future__ import annotations

import argparse
import ast
import builtins
import dataclasses
import enum
import hashlib
import importlib
import importlib.util
import inspect
import json
import os
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
KINDS = {
    inspect.Parameter.POSITIONAL_ONLY: "positional_only",
    inspect.Parameter.POSITIONAL_OR_KEYWORD: "positional_or_keyword",
    inspect.Parameter.VAR_POSITIONAL: "var_positional",
    inspect.Parameter.KEYWORD_ONLY: "keyword_only",
    inspect.Parameter.VAR_KEYWORD: "var_keyword",
}
PROTOCOL_METHODS = {"__enter__", "__exit__"}


@dataclasses.dataclass
class _FactoryMarker:
    """Obtain the interpreter's factory signature marker through public APIs."""

    value: object = dataclasses.field(default_factory=list)


FACTORY_DEFAULT = inspect.signature(_FactoryMarker).parameters["value"].default


class QualificationError(ValueError):
    """The static inventory and the loaded package do not describe the same API."""


def require(condition, message):
    if not condition:
        raise QualificationError(message)


def annotation_key(value, *, source=False):
    """Normalize syntax only; postponed annotations must not execute code."""
    if value is inspect.Signature.empty or (source and value is None):
        return None
    if value is None:
        value = "None"
    require(isinstance(value, str), "runtime annotation is not postponed text")
    try:
        return ast.dump(ast.parse(value, mode="eval").body, include_attributes=False)
    except SyntaxError as error:
        raise QualificationError("invalid annotation syntax: " + value) from error


def resolve_default(expression, namespace):
    """Resolve literals and static names without eval, calls, or descriptors."""
    try:
        node = ast.parse(expression, mode="eval").body
    except (TypeError, SyntaxError) as error:
        raise QualificationError("invalid default expression") from error

    def resolve(item):
        try:
            return ast.literal_eval(item)
        except (ValueError, TypeError):
            pass
        if isinstance(item, ast.Name):
            require(item.id in namespace or item.id in vars(builtins),
                    "unknown default name: " + item.id)
            return namespace[item.id] if item.id in namespace else vars(builtins)[item.id]
        if isinstance(item, ast.Attribute):
            owner = resolve(item.value)
            require(inspect.ismodule(owner) or inspect.isclass(owner),
                    "default attribute is not a module/class constant")
            try:
                value = inspect.getattr_static(owner, item.attr)
            except AttributeError as error:
                raise QualificationError("unknown default attribute: " + item.attr) from error
            require(not isinstance(value, (property, classmethod, staticmethod)),
                    "default attribute would require a descriptor")
            return value
        raise QualificationError("unsupported default expression: " + expression)

    return resolve(node)


def same_value(left, right):
    """Keep enum identity and bool/int distinctions; never call user equality."""
    if left is right:
        return True
    if type(left) is not type(right):
        return False
    if type(left) in {str, bytes, int, float, bool, type(None)}:
        return left == right
    if type(left) in {list, tuple}:
        return len(left) == len(right) and all(same_value(a, b) for a, b in zip(left, right))
    if type(left) is dict:
        return len(left) == len(right) and all(
            same_value(key, other_key) and same_value(value, other_value)
            for (key, value), (other_key, other_value) in zip(left.items(), right.items()))
    # Other values, including Enum members and factories, require identity.
    return False


def check_default(expected, actual, missing, namespace, label, *, factory=False):
    if expected is None:
        require(actual is missing, label + ": unexpected runtime default")
        return
    require(actual is not missing, label + ": missing runtime default")
    value = resolve_default(expected, namespace)
    require(actual is value if factory else same_value(actual, value),
            label + ": default differs")


def check_callable(record, function, label):
    """Compare each parameter and return annotation, including self/cls."""
    signature = inspect.signature(function, follow_wrapped=False, eval_str=False)
    parameters = list(signature.parameters.values())
    expected = record["parameters"]
    require([item["name"] for item in expected] == [item.name for item in parameters],
            label + ": parameter names/order differ")
    namespace = function.__globals__
    for item, actual in zip(expected, parameters):
        location = label + "." + actual.name
        require(item["kind"] == KINDS[actual.kind], location + ": parameter kind differs")
        require(annotation_key(item["annotation"], source=True) == annotation_key(actual.annotation),
                location + ": annotation differs")
        check_default(item["default"], actual.default, inspect.Parameter.empty,
                      namespace, location)
    require(annotation_key(record["returns"], source=True) == annotation_key(signature.return_annotation),
            label + ": return annotation differs")


def check_dataclass_constructor(record, value, namespace):
    """Also qualify the constructor signatures synthesized from field metadata."""
    fields = [field for field in record["fields"] if field["init"]]
    fields.sort(key=lambda field: field["kw_only"])
    signature = inspect.signature(value, follow_wrapped=False, eval_str=False)
    parameters = list(signature.parameters.values())
    label = record["name"] + ".__init__"
    require([field["name"] for field in fields] == [parameter.name for parameter in parameters],
            label + ": generated parameter names/order differ")
    for field, parameter in zip(fields, parameters):
        location = label + "." + parameter.name
        expected_kind = "keyword_only" if field["kw_only"] else "positional_or_keyword"
        require(KINDS[parameter.kind] == expected_kind, location + ": generated parameter kind differs")
        require(annotation_key(field["annotation"], source=True) == annotation_key(parameter.annotation),
                location + ": generated parameter annotation differs")
        if field["default_factory"] is not None:
            require(parameter.default is FACTORY_DEFAULT, location + ": generated factory default differs")
        else:
            check_default(field["default"], parameter.default, inspect.Parameter.empty, namespace, location)


def descriptor_function(descriptor):
    if isinstance(descriptor, classmethod):
        return "class", descriptor.__func__
    if isinstance(descriptor, staticmethod):
        return "static", descriptor.__func__
    if isinstance(descriptor, property):
        require(descriptor.fset is None and descriptor.fdel is None,
                "writable property requires reference qualification support")
        return "property", descriptor.fget
    if inspect.isfunction(descriptor):
        return "instance", descriptor
    return None, None


def first_party(function, module_name):
    name = getattr(function, "__module__", "") or ""
    return inspect.isfunction(function) and (name == module_name or name.startswith(module_name + "."))


def runtime_members(value, module_name):
    """Inspect the MRO directly so properties and owner methods remain uncalled."""
    members, seen = {}, set()
    for owner in value.__mro__:
        for name, descriptor in vars(owner).items():
            if name in seen:
                continue
            seen.add(name)
            if name.startswith("_") and name not in PROTOCOL_METHODS:
                continue
            binding, function = descriptor_function(descriptor)
            if first_party(function, module_name):
                members[name] = owner, binding, function
    return members


def runtime_constructor(value, module_name):
    for owner in value.__mro__:
        if "__init__" not in vars(owner):
            continue
        binding, function = descriptor_function(vars(owner)["__init__"])
        if not first_party(function, module_name):
            return None
        # dataclasses generate __init__ from fields; there is no source callable.
        if dataclasses.is_dataclass(value) and function.__code__.co_filename == "<string>":
            return None
        return owner, binding, function
    return None


def check_member(record, actual, value, label):
    owner, binding, function = actual
    require(record["binding"] == binding, label + ": descriptor binding differs")
    require(record["declaring_class"] == owner.__name__, label + ": declaring class differs")
    require(record["inherited"] is (owner is not value), label + ": inheritance differs")
    check_callable(record, function, label)


def kind(value):
    if inspect.isfunction(value):
        return "function"
    require(inspect.isclass(value), "public export is neither a function nor class")
    if dataclasses.is_dataclass(value):
        return "dataclass"
    if issubclass(value, enum.Enum):
        return "enum"
    return "exception" if issubclass(value, BaseException) else "class"


def qualify_public_modules(inventory, public, modules):
    """Check namespace reexports without evaluating module attribute hooks."""
    root_name = inventory["module"]
    records = inventory.get("public_modules", [])
    names = [record["name"] for record in records]
    require(len(names) == len(set(names)), "duplicate public module records")
    root_exports = {record["name"] for record in inventory["exports"]}
    count = 0
    for record in records:
        name = record["name"]
        prefix = root_name + "."
        short_name = name[len(prefix):] if name.startswith(prefix) else ""
        require(short_name.isidentifier() and not short_name.startswith("_"),
                "public module is not a direct public package child: " + name)
        require(name in modules, name + ": public module is not loaded")
        module = modules[name]
        require(inspect.ismodule(module) and vars(module).get("__name__") == name,
                name + ": public module identity differs")
        require(vars(public).get(short_name) is module,
                name + ": parent package binding differs")
        exports = record["exports"]
        require(len(exports) == len(set(exports)), name + ": duplicate public module exports")
        require(set(exports) <= root_exports, name + ": export is outside the root public API")
        namespace = vars(module)
        require(exports == list(namespace.get("__all__", ())),
                name + ": public __all__ names/order differ")
        for export in exports:
            require(export in namespace and namespace[export] is vars(public)[export],
                    name + "." + export + ": canonical export identity differs")
        count += len(exports)
    return len(records), count


def qualify_inventory(inventory, public, modules=None):
    """Qualify an inventory against loaded modules; exposed for focused tests."""
    modules = sys.modules if modules is None else modules
    require(inventory["schema"] == 1, "unsupported Python inventory schema")
    module_name = inventory["module"]
    require(public.__name__ == module_name, "public module identity differs")
    records = inventory["exports"]
    names = [record["name"] for record in records]
    require(len(names) == len(set(names)), "duplicate inventory exports")
    require(names == list(vars(public).get("__all__", ())), "public __all__ names/order differ")
    counts = dict(exports=len(names), aliases=0, callables=0, methods=0, properties=0,
                  dataclasses=0, dataclass_fields=0, generated_dataclass_constructors=0,
                  enums=0, enum_members=0)
    identities = set()
    for record in records:
        name = record["name"]
        value = vars(public).get(name)
        defining_name = record["definition_module"]
        require(defining_name == module_name or defining_name.startswith(module_name + "."),
                name + ": definition is outside the public package")
        require(defining_name in modules, name + ": defining module is not loaded")
        namespace = vars(modules[defining_name])
        require(record["definition_name"] in namespace and
                value is namespace[record["definition_name"]], name + ": export identity differs")
        require(value.__module__ == module_name, name + ": public __module__ differs")
        require(value.__name__ == record["definition_name"] and
                value.__qualname__ == record["definition_name"], name + ": definition name differs")
        require(record["kind"] == kind(value), name + ": export kind differs")
        counts["aliases"] += id(value) in identities
        identities.add(id(value))
        if record["kind"] == "function":
            check_callable(record, value, name)
            counts["callables"] += 1
            continue
        if "bases" in record:
            bases = tuple(resolve_default(base, namespace) for base in record["bases"]) or (object,)
            require(len(bases) == len(value.__bases__) and
                    all(expected is actual for expected, actual in zip(bases, value.__bases__)),
                    name + ": base classes differ")
        actual_members = runtime_members(value, module_name)
        members = record["members"]
        member_names = [item["name"] for item in members]
        require(len(member_names) == len(set(member_names)), name + ": duplicate member records")
        require(set(member_names) == set(actual_members), name + ": public member set differs")
        for member in members:
            check_member(member, actual_members[member["name"]], value, name + "." + member["name"])
            counts["callables"] += 1
            counts["properties" if member["binding"] == "property" else "methods"] += 1
        constructor = runtime_constructor(value, module_name)
        require((record.get("constructor") is None) == (constructor is None),
                name + ": source constructor presence differs")
        if constructor is not None:
            check_member(record["constructor"], constructor, value, name + ".__init__")
            counts["callables"] += 1
        if record["kind"] == "dataclass":
            fields = dataclasses.fields(value)
            require([item["name"] for item in record["fields"]] == [field.name for field in fields],
                    name + ": dataclass field names/order differ")
            require(record["frozen"] is value.__dataclass_params__.frozen,
                    name + ": dataclass frozen status differs")
            for item, field in zip(record["fields"], fields):
                location = name + "." + field.name
                require(annotation_key(item["annotation"], source=True) == annotation_key(field.type),
                        location + ": field annotation differs")
                require(item["init"] is field.init and item["kw_only"] is field.kw_only,
                        location + ": field constructor flags differ")
                check_default(item["default"], field.default, dataclasses.MISSING, namespace, location)
                check_default(item["default_factory"], field.default_factory, dataclasses.MISSING,
                              namespace, location + ".default_factory", factory=True)
            if constructor is None:
                check_dataclass_constructor(record, value, namespace)
                counts["generated_dataclass_constructors"] += 1
                counts["callables"] += 1
            counts["dataclasses"] += 1
            counts["dataclass_fields"] += len(fields)
        if record["kind"] == "enum":
            require([item["name"] for item in record["enum_members"]] == list(value.__members__),
                    name + ": enum member names/order differ")
            for item in record["enum_members"]:
                require(same_value(item["value"], value.__members__[item["name"]].value),
                        name + "." + item["name"] + ": enum value differs")
            counts["enums"] += 1
            counts["enum_members"] += len(record["enum_members"])
    counts["public_modules"], counts["public_module_exports"] = qualify_public_modules(
        inventory, public, modules)
    return counts


def load_inventory(root):
    path = root / "docs/tools/python_api_reference.py"
    spec = importlib.util.spec_from_file_location("_python_reference_qualification_generator", path)
    require(spec is not None and spec.loader is not None, "cannot load static reference generator")
    generator = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(generator)
    return generator.inventory(root)


def qualify(root, inventory=None):
    """Load this checkout's facade-backed package, then inspect without calls."""
    root = Path(root).resolve()
    native = os.environ.get("AMS_MEL_NATIVE_LIB", "")
    require(bool(native) and Path(native).is_absolute() and Path(native).is_file(),
            "set AMS_MEL_NATIVE_LIB to the absolute path of an already built native facade")
    inventory = load_inventory(root) if inventory is None else inventory
    package = root / "python/ams_mel"
    sources = {path.relative_to(root).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
               for path in sorted(package.glob("*.py"))}
    require(sources == inventory["sources"], "inventory source hashes differ from this checkout")
    public_modules = inventory.get("public_modules", [])
    expected_modules = {"ams_mel." + path.stem: path.relative_to(root).as_posix()
                        for path in sorted(package.glob("*.py")) if not path.stem.startswith("_")}
    recorded_names = [record["name"] for record in public_modules]
    require(len(recorded_names) == len(set(recorded_names)) and
            set(recorded_names) == set(expected_modules),
            "public module inventory names differ from this checkout")
    require(all(record["source"] == expected_modules[record["name"]] for record in public_modules),
            "public module source paths differ from this checkout")

    def check_loaded_origins():
        for name, module in tuple(sys.modules.items()):
            if name == "ams_mel" or name.startswith("ams_mel."):
                expected = package / ("__init__.py" if name == "ams_mel" else name[8:] + ".py")
                actual = getattr(module, "__file__", None)
                require(actual is not None and Path(actual).resolve() == expected.resolve(),
                        "loaded module is from another checkout: " + name)

    check_loaded_origins()
    previous = sys.path[:]
    sys.path.insert(0, str(root / "python"))
    try:
        public = importlib.import_module("ams_mel")
        for record in public_modules:
            importlib.import_module(record["name"])
    finally:
        sys.path[:] = previous
    check_loaded_origins()
    counts = qualify_inventory(inventory, public)
    return {"schema": 1, "status": "qualified", "mode": "explicit-runtime-reflection",
            "python_version": sys.version.split()[0], "counts": counts,
            "native_library": str(Path(native).resolve()),
            "native_sha256": hashlib.sha256(Path(native).read_bytes()).hexdigest(),
            "source_files": len(sources)}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--inventory", type=Path,
                        help="qualify an existing inventory or python-reference.json report")
    args = parser.parse_args(argv)
    try:
        inventory = json.loads(args.inventory.read_text(encoding="utf-8")) if args.inventory else None
        if inventory is not None and "report" in inventory:
            inventory = inventory["report"]
        if inventory is not None and "inventory" in inventory:
            inventory = inventory["inventory"]
        report = qualify(args.root, inventory)
    except (QualificationError, ImportError, OSError, KeyError, TypeError, ValueError) as error:
        print("Python reference qualification failed: " + str(error), file=sys.stderr)
        return 1
    print(json.dumps(report, sort_keys=True, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
