"""Generate the safe Python reference from source without importing the binding.

This is a deliberately bounded source reader, not a Python evaluator. Public
exports come from the package's and public modules' literal __all__; unsupported
public constructs raise ReferenceError instead of disappearing from the reference. A separate
native-backed qualification command compares this inventory with Python's real
runtime API. Documentation prose coverage is informational.
"""

import argparse
import ast
import hashlib
import html
import json
from pathlib import Path
import re


class ReferenceError(ValueError):
    """The selected public source cannot be represented faithfully."""


def require(condition, message):
    if not condition:
        raise ReferenceError(message)


def canonical(value):
    return (json.dumps(value, sort_keys=True, indent=2, ensure_ascii=True) + "\n").encode()


def expression(node):
    return ast.unparse(node) if node is not None else None


def assigned_names(target):
    if isinstance(target, ast.Name):
        return [target.id]
    if isinstance(target, (ast.Tuple, ast.List)):
        return [name for item in target.elts for name in assigned_names(item)]
    return []


def public_member(name):
    return not name.startswith("_") or name in {"__enter__", "__exit__"}


class Sources:
    def __init__(self, root):
        self.root = Path(root)
        directory = self.root / "python/ams_mel"
        require(directory.is_dir(), "missing Python binding source")
        self.modules, self.text, self.paths, self.bindings = {}, {}, {}, {}
        for path in sorted(directory.glob("*.py")):
            require(not path.is_symlink(), "symlink Python source: " + str(path))
            name = "ams_mel" if path.stem == "__init__" else "ams_mel." + path.stem
            content = path.read_text(encoding="utf-8")
            self.modules[name] = ast.parse(content, filename=str(path))
            self.text[name] = content
            self.paths[name] = path.relative_to(self.root).as_posix()
        require("ams_mel" in self.modules, "missing public Python package")
        for module, tree in self.modules.items():
            bindings = {}
            for node in tree.body:
                if isinstance(node, (ast.ClassDef, ast.FunctionDef, ast.AsyncFunctionDef)):
                    self.bind(bindings, node.name, ("definition", module, node))
                elif isinstance(node, ast.ImportFrom):
                    require(node.level in {0, 1}, "unsupported relative import depth")
                    for alias in node.names:
                        require(alias.name != "*", "wildcard import prevents exact public resolution")
                        name = alias.asname or alias.name
                        if node.level:
                            target = "ams_mel" + ("." + node.module if node.module else "")
                            if node.module:
                                self.bind(bindings, name, ("import", target, alias.name))
                            else:
                                self.bind(bindings, name, ("module", target + "." + alias.name, None))
                        else:
                            self.bind(bindings, name, ("external", node.module, alias.name))
                elif isinstance(node, ast.Import):
                    for alias in node.names:
                        self.bind(bindings, alias.asname or alias.name.split(".")[0],
                                  ("external-module", alias.name, None))
                elif isinstance(node, (ast.Assign, ast.AnnAssign)):
                    targets = node.targets if isinstance(node, ast.Assign) else [node.target]
                    for target in targets:
                        for name in assigned_names(target):
                            self.bind(bindings, name, ("assignment", module, (node, target, name)))
                elif isinstance(node, (ast.If, ast.For, ast.While, ast.Try, ast.With)):
                    def conditional_writes(statement):
                        if isinstance(statement, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
                            yield statement.name
                            return
                        if isinstance(statement, (ast.Assign, ast.AnnAssign, ast.AugAssign)):
                            targets = statement.targets if isinstance(statement, ast.Assign) else [statement.target]
                            for target in targets:
                                yield from assigned_names(target)
                        if isinstance(statement, (ast.Import, ast.ImportFrom)):
                            for alias in statement.names:
                                yield alias.asname or alias.name.split(".")[0]
                        if isinstance(statement, (ast.For, ast.AsyncFor)):
                            yield from assigned_names(statement.target)
                        if isinstance(statement, (ast.With, ast.AsyncWith)):
                            for item in statement.items:
                                if item.optional_vars is not None:
                                    yield from assigned_names(item.optional_vars)
                        for child in ast.iter_child_nodes(statement):
                            if isinstance(child, ast.stmt):
                                yield from conditional_writes(child)
                    for name in conditional_writes(node):
                        bindings[name] = ("ambiguous", name, None)
            self.bindings[module] = bindings

    @staticmethod
    def bind(bindings, name, value):
        if name in bindings:
            # Private runtime constants/helpers are outside the API model.
            # Their ambiguity is retained and rejected if resolution needs them.
            bindings[name] = ("ambiguous", name, None)
        else:
            bindings[name] = value

    def resolve(self, module, name, trail=()):
        key = (module, name)
        require(key not in trail, "cyclic public binding: " + ".".join(key))
        require(module in self.bindings and name in self.bindings[module],
                "unresolved public binding: " + ".".join(key))
        binding = self.bindings[module][name]
        kind, location, value = binding
        if kind == "import":
            return self.resolve(location, value, (*trail, key))
        if kind == "assignment":
            node = value[0].value
            if isinstance(node, ast.Name):
                return self.resolve(module, node.id, (*trail, key))
            if isinstance(node, ast.Attribute) and isinstance(node.value, ast.Name):
                owner = self.bindings[module].get(node.value.id)
                if owner and owner[0] == "module":
                    return self.resolve(owner[1], node.attr, (*trail, key))
        require(kind != "ambiguous", "ambiguous public binding: " + ".".join(key))
        return binding

    def class_base(self, module, node):
        if isinstance(node, ast.Name):
            if node.id in {"object", "Exception", "BaseException"}:
                return ("external", "builtins", node.id)
            return self.resolve(module, node.id)
        if isinstance(node, ast.Attribute) and isinstance(node.value, ast.Name):
            owner = self.bindings[module].get(node.value.id)
            if owner and owner[0] == "module":
                return self.resolve(owner[1], node.attr)
            if owner and owner[0] == "external-module":
                return ("external", owner[1], node.attr)
        raise ReferenceError("unsupported public base: " + expression(node))

    def ancestors(self, module, node, trail=()):
        require((module, node.name) not in trail, "cyclic class ancestry")
        require(len(node.bases) <= 1, "multiple public class bases require explicit qualification")
        result = []
        for base in node.bases:
            resolved = self.class_base(module, base)
            if resolved[0] == "definition":
                _, base_module, base_node = resolved
                require(isinstance(base_node, ast.ClassDef), "non-class public base")
                result.extend(self.ancestors(base_module, base_node, (*trail, (module, node.name))))
            else:
                require(resolved[0] == "external" and (
                    resolved[1] == "enum" and resolved[2] in {"Enum", "IntEnum", "IntFlag"} or
                    resolved[1] == "builtins" and resolved[2] in {"object", "Exception", "BaseException"}),
                    "unqualified external base: " + repr(resolved))
        result.append((module, node))
        return result

    def enum_class(self, module, node):
        for owner, ancestor in self.ancestors(module, node):
            for base in ancestor.bases:
                binding = self.class_base(owner, base)
                if binding[0] == "external" and binding[1] == "enum":
                    return True
        return False

    def exception_class(self, module, node):
        for owner, ancestor in self.ancestors(module, node):
            for base in ancestor.bases:
                binding = self.class_base(owner, base)
                if binding[0] == "external" and binding[1] == "builtins" and binding[2] in {
                        "Exception", "BaseException"}:
                    return True
        return False

    def literal(self, module, node, local=None, trail=()):
        """Resolve only finite literal/constant expressions, never call code."""
        local = local or {}
        if isinstance(node, ast.Constant):
            require(type(node.value) in {int, float, str, bool, type(None)}, "unsupported constant")
            return node.value
        if isinstance(node, (ast.Tuple, ast.List)):
            return [self.literal(module, item, local, trail) for item in node.elts]
        if isinstance(node, ast.Name):
            if node.id in local:
                return local[node.id]
            key = (module, node.id)
            require(key not in trail, "cyclic constant")
            binding = self.bindings[module].get(node.id)
            require(binding is not None, "unresolved constant: " + ".".join(key))
            if binding[0] == "import":
                return self.literal(binding[1], ast.Name(id=binding[2]), {}, (*trail, key))
            require(binding[0] == "assignment", "nonliteral constant: " + ".".join(key))
            statement, target, selected = binding[2]
            value = self.literal(module, statement.value, local, (*trail, key))
            if isinstance(target, ast.Name):
                return value
            names = assigned_names(target)
            require(isinstance(value, list) and len(names) == len(value), "constant unpacking mismatch")
            require(all(isinstance(item, ast.Name) for item in target.elts), "nested constant unpacking")
            return value[names.index(selected)]
        if isinstance(node, ast.Attribute) and isinstance(node.value, ast.Name):
            binding = self.bindings[module].get(node.value.id)
            if binding and binding[0] == "module":
                return self.literal(binding[1], ast.Name(id=node.attr), {}, trail)
            if binding:
                resolved = self.resolve(module, node.value.id)
                if resolved[0] == "definition" and isinstance(resolved[2], ast.ClassDef):
                    values = {}
                    for member in resolved[2].body:
                        if isinstance(member, ast.Assign):
                            for target in member.targets:
                                if isinstance(target, ast.Name) and not target.id.startswith("_"):
                                    values[target.id] = self.literal(resolved[1], member.value, values, trail)
                    require(node.attr in values, "missing enum constant")
                    return values[node.attr]
            raise ReferenceError("unresolved constant attribute: " + expression(node))
        if isinstance(node, ast.UnaryOp):
            value = self.literal(module, node.operand, local, trail)
            require(type(value) in {int, float}, "nonnumeric constant unary operation")
            if isinstance(node.op, ast.USub):
                return -value
            if isinstance(node.op, ast.UAdd):
                return value
            if isinstance(node.op, ast.Invert) and type(value) is int:
                return ~value
        if isinstance(node, ast.BinOp):
            left = self.literal(module, node.left, local, trail)
            right = self.literal(module, node.right, local, trail)
            require(type(left) is int and type(right) is int, "noninteger constant arithmetic")
            require(left.bit_length() < 4096 and right.bit_length() < 4096, "oversized constant")
            operations = {ast.Add: lambda: left + right, ast.Sub: lambda: left - right,
                          ast.Mult: lambda: left * right, ast.BitOr: lambda: left | right,
                          ast.BitAnd: lambda: left & right, ast.BitXor: lambda: left ^ right}
            if isinstance(node.op, (ast.LShift, ast.RShift)):
                require(0 <= right <= 4096, "oversized shift")
                return left << right if isinstance(node.op, ast.LShift) else left >> right
            require(type(node.op) in operations, "unsupported constant operator")
            return operations[type(node.op)]()
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and node.func.id == "range":
            require("range" not in self.bindings[module], "shadowed range constant")
            require(not node.keywords and 1 <= len(node.args) <= 3, "unsupported range constant")
            args = [self.literal(module, arg, local, trail) for arg in node.args]
            require(all(type(arg) is int for arg in args), "noninteger range constant")
            values = range(*args)
            require(len(values) <= 4096, "oversized range constant")
            return list(values)
        raise ReferenceError("unsupported constant expression: " + expression(node))

    def location(self, module, node):
        return {"source": self.paths[module], "line": node.lineno, "end_line": node.end_lineno}

    def annotation(self, module, node):
        """Display named private type aliases through their public constituents."""
        if isinstance(node, ast.Name) and node.id.startswith("_"):
            binding = self.bindings[module].get(node.id)
            if binding and binding[0] == "assignment":
                return expression(binding[2][0].value)
        return expression(node)

    def callable(self, module, node, binding="instance"):
        require(not isinstance(node, ast.AsyncFunctionDef), "async public callable requires qualification")
        require(not getattr(node, "type_params", []), "generic public callable requires qualification")
        parameters = []
        args = node.args
        positional = [*args.posonlyargs, *args.args]
        defaults = [None] * (len(positional) - len(args.defaults)) + list(args.defaults)
        for index, (arg, default) in enumerate(zip(positional, defaults)):
            parameters.append({"name": arg.arg, "kind": "positional_only" if index < len(args.posonlyargs)
                               else "positional_or_keyword", "annotation": expression(arg.annotation),
                               "display_annotation": self.annotation(module, arg.annotation),
                               "default": expression(default)})
        if args.vararg:
            parameters.append({"name": args.vararg.arg, "kind": "var_positional",
                               "annotation": expression(args.vararg.annotation),
                               "display_annotation": self.annotation(module, args.vararg.annotation), "default": None})
        for arg, default in zip(args.kwonlyargs, args.kw_defaults):
            parameters.append({"name": arg.arg, "kind": "keyword_only", "annotation": expression(arg.annotation),
                               "display_annotation": self.annotation(module, arg.annotation),
                               "default": expression(default)})
        if args.kwarg:
            parameters.append({"name": args.kwarg.arg, "kind": "var_keyword",
                               "annotation": expression(args.kwarg.annotation),
                               "display_annotation": self.annotation(module, args.kwarg.annotation), "default": None})
        return {"name": node.name, "binding": binding, "parameters": parameters,
                "returns": expression(node.returns), "doc": ast.get_docstring(node) or "",
                "decorators": [expression(item) for item in node.decorator_list],
                "signature": node.name + "(" + expression(args) + ")" +
                (" -> " + expression(node.returns) if node.returns else ""), **self.location(module, node)}

    def class_record(self, module, node, name):
        ancestors = self.ancestors(module, node)
        require(not node.keywords and not getattr(node, "type_params", []),
                "metaclass/generic public class requires qualification")
        require(all(not ancestor.decorator_list for _, ancestor in ancestors[:-1]),
                "decorated public ancestor requires qualification")
        decorators = [expression(item) for item in node.decorator_list]
        dataclass = None
        for decorator in node.decorator_list:
            target = decorator.func if isinstance(decorator, ast.Call) else decorator
            require(isinstance(target, ast.Name) and target.id == "dataclass",
                    "unsupported public class decorator: " + expression(decorator))
            require(self.resolve(module, target.id) == ("external", "dataclasses", "dataclass"),
                    "unqualified dataclass decorator")
            require(dataclass is None, "duplicate dataclass decorator")
            dataclass = {}
            if isinstance(decorator, ast.Call):
                require(not decorator.args and all(k.arg is not None for k in decorator.keywords),
                        "dynamic dataclass configuration")
                dataclass = {k.arg: self.literal(module, k.value) for k in decorator.keywords}
                require(set(dataclass) <= {"init", "repr", "eq", "order", "unsafe_hash", "frozen",
                                          "match_args", "kw_only", "slots", "weakref_slot"} and
                        all(type(value) is bool for value in dataclass.values()),
                        "unsupported dataclass configuration")
        enum = self.enum_class(module, node)
        kind = "dataclass" if dataclass is not None else "enum" if enum else (
            "exception" if self.exception_class(module, node) else "class")
        record = {"name": name, "kind": kind, "definition_module": module,
                  "definition_name": node.name, "doc": ast.get_docstring(node) or "",
                  "decorators": decorators, "bases": [expression(base) for base in node.bases],
                  "members": [], "fields": [], "attributes": [], "enum_members": [],
                  "constructor": None, **self.location(module, node)}
        members, attributes, fields, enum_values = {}, {}, {}, {}
        for ancestor_module, ancestor in ancestors:
            inherited = ancestor is not node
            seen = set()
            for item in ancestor.body:
                if isinstance(item, (ast.FunctionDef, ast.AsyncFunctionDef)):
                    if not public_member(item.name) and item.name != "__init__":
                        continue
                    require(item.name not in seen, "duplicate/overloaded public member: " + item.name)
                    seen.add(item.name)
                    deco = [expression(d) for d in item.decorator_list]
                    require(all(d in {"classmethod", "staticmethod", "property"} for d in deco) and len(deco) <= 1,
                            "unsupported public method decorator: " + repr(deco))
                    binding = {"classmethod": "class", "staticmethod": "static", "property": "property"}.get(
                        deco[0] if deco else "", "instance")
                    member = self.callable(ancestor_module, item, binding)
                    member.update(inherited=inherited, declaring_class=ancestor.name)
                    if item.name == "__init__":
                        body = item.body[1:] if ast.get_docstring(item) is not None else item.body
                        if len(body) == 1 and isinstance(body[0], ast.Raise):
                            error = body[0].exc
                            if (isinstance(error, ast.Call) and isinstance(error.func, ast.Name) and
                                    error.func.id == "TypeError" and "TypeError" not in self.bindings[ancestor_module] and
                                    not error.keywords and len(error.args) == 1 and
                                    isinstance(error.args[0], ast.Constant) and isinstance(error.args[0].value, str)):
                                member["direct_construction_error"] = error.args[0].value
                        record["constructor"] = member
                        # Public instance attributes on exception/value constructors.
                        # Owner internals begin with underscores and are excluded.
                        parameter_types = {p["name"]: p["annotation"] for p in member["parameters"]}
                        for statement in item.body:
                            targets = statement.targets if isinstance(statement, ast.Assign) else (
                                [statement.target] if isinstance(statement, ast.AnnAssign) else [])
                            for target in targets:
                                if isinstance(target, ast.Attribute) and isinstance(target.value, ast.Name) and (
                                        target.value.id == "self" and not target.attr.startswith("_")):
                                    value = statement.value
                                    annotation = expression(statement.annotation) if isinstance(
                                        statement, ast.AnnAssign) else parameter_types.get(
                                            value.id if isinstance(value, ast.Name) else "")
                                    attributes[target.attr] = {"name": target.attr, "annotation": annotation,
                                        "value": expression(value), "inherited": inherited,
                                        **self.location(ancestor_module, statement)}
                    else:
                        members[item.name] = member
                elif isinstance(item, ast.AnnAssign) and isinstance(item.target, ast.Name):
                    field_name = item.target.id
                    if field_name.startswith("_"):
                        continue
                    require(field_name not in seen, "duplicate public class member: " + field_name)
                    seen.add(field_name)
                    annotation = expression(item.annotation)
                    if enum:
                        require(item.value is not None, "annotation-only enum member requires qualification")
                        value = self.literal(ancestor_module, item.value, enum_values)
                        require(type(value) in {int, str}, "unsupported enum value: " + field_name)
                        enum_values[field_name] = value
                        record["enum_members"].append({"name": field_name, "value": value,
                            "expression": expression(item.value), **self.location(ancestor_module, item)})
                    elif annotation.startswith("ClassVar["):
                        attributes[field_name] = {"name": field_name, "annotation": annotation,
                            "value": expression(item.value), "inherited": inherited,
                            **self.location(ancestor_module, item)}
                    elif dataclass is not None:
                        default, factory, init, kw_only = expression(item.value), None, True, bool(
                            (dataclass or {}).get("kw_only", False))
                        if isinstance(item.value, ast.Call):
                            require(isinstance(item.value.func, ast.Name) and item.value.func.id == "field" and
                                    not item.value.args, "unsupported dataclass field: " + field_name)
                            require(self.resolve(ancestor_module, "field") == ("external", "dataclasses", "field"),
                                    "unqualified dataclass field")
                            options = {k.arg: k.value for k in item.value.keywords}
                            require(None not in options and set(options) <= {
                                "default", "default_factory", "init", "repr", "hash", "compare", "metadata", "kw_only"},
                                "unsupported dataclass field option")
                            default, factory = expression(options.get("default")), expression(options.get("default_factory"))
                            require(default is None or factory is None, "field has default and factory")
                            init = self.literal(ancestor_module, options["init"]) if "init" in options else True
                            kw_only = self.literal(ancestor_module, options["kw_only"]) if "kw_only" in options else kw_only
                            require(type(init) is bool and type(kw_only) is bool, "nonboolean dataclass field option")
                        fields[field_name] = {"name": field_name, "annotation": annotation,
                            "display_annotation": self.annotation(ancestor_module, item.annotation),
                            "default": default, "default_factory": factory, "init": init, "kw_only": kw_only,
                            "inherited": inherited, **self.location(ancestor_module, item)}
                    else:
                        attributes[field_name] = {"name": field_name, "annotation": annotation,
                            "value": expression(item.value), "inherited": inherited,
                            **self.location(ancestor_module, item)}
                elif isinstance(item, ast.Assign):
                    for target in item.targets:
                        if not isinstance(target, ast.Name) or target.id.startswith("_"):
                            continue
                        require(target.id not in seen, "duplicate public class member: " + target.id)
                        seen.add(target.id)
                        if enum:
                            value = self.literal(ancestor_module, item.value, enum_values)
                            require(type(value) in {int, str}, "unsupported enum value: " + target.id)
                            enum_values[target.id] = value
                            record["enum_members"].append({"name": target.id, "value": value,
                                "expression": expression(item.value), **self.location(ancestor_module, item)})
                        else:
                            attributes[target.id] = {"name": target.id, "annotation": None,
                                "value": expression(item.value), "inherited": inherited,
                                **self.location(ancestor_module, item)}
                elif isinstance(item, (ast.If, ast.For, ast.While, ast.Try, ast.With, ast.ClassDef)):
                    raise ReferenceError("dynamic/nested public class body: " + ancestor.name)
        record.update(members=list(members.values()), fields=list(fields.values()), attributes=list(attributes.values()))
        if dataclass is not None:
            require(dataclass.get("init", True) is True, "dataclass init=False requires qualification")
            record["frozen"] = bool(dataclass.get("frozen", False))
            record["dataclass_options"] = dataclass
        identities = [x["name"] for group in ("members", "fields", "attributes", "enum_members") for x in record[group]]
        require(len(set(identities)) == len(identities), "ambiguous public member identity: " + name)
        return record

    def export_names(self, module):
        tree = self.modules[module]
        binding = self.bindings[module].get("__all__")
        require(binding and binding[0] == "assignment", "public __all__ must be one literal assignment")
        all_node = binding[2][0].value
        require(isinstance(all_node, (ast.List, ast.Tuple)) and all(
            isinstance(n, ast.Constant) and isinstance(n.value, str) for n in all_node.elts),
            "public __all__ must be literal names")
        names = [n.value for n in all_node.elts]
        require(names and len(set(names)) == len(names), "empty or duplicate public exports")
        require(all(re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*", name) for name in names), "invalid public export name")
        # Dynamic mutation of __all__ cannot be inferred by a literal inventory.
        for node in ast.walk(tree):
            if isinstance(node, ast.AugAssign) and "__all__" in assigned_names(node.target):
                raise ReferenceError("dynamic __all__ mutation")
            if isinstance(node, (ast.Assign, ast.AnnAssign, ast.Delete)):
                changed = node.targets if isinstance(node, (ast.Assign, ast.Delete)) else [node.target]
                if any(isinstance(target, ast.Subscript) and isinstance(target.value, ast.Name) and
                       target.value.id == "__all__" for target in changed):
                    raise ReferenceError("dynamic __all__ mutation")
            if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute) and isinstance(
                    node.func.value, ast.Name) and node.func.value.id == "__all__":
                raise ReferenceError("dynamic __all__ mutation")
        return names

    def inventory(self):
        tree = self.modules["ams_mel"]
        names = self.export_names("ams_mel")
        exports = []
        for name in names:
            kind, module, node = self.resolve("ams_mel", name)
            require(kind == "definition", "unsupported public export: " + name)
            if isinstance(node, ast.ClassDef):
                record = self.class_record(module, node, name)
            else:
                require(not node.decorator_list, "decorated public function requires qualification")
                record = self.callable(module, node, "function")
                record.update(name=name, kind="function", definition_module=module,
                              definition_name=node.name, members=[], fields=[], attributes=[], enum_members=[])
            exports.append(record)
        public_modules = []
        for module in sorted(self.modules):
            if module == "ams_mel" or module.rsplit(".", 1)[1].startswith("_"):
                continue
            require(re.fullmatch(r"ams_mel\.[A-Za-z][A-Za-z0-9_]*", module),
                    "invalid public module name: " + module)
            require(module.removeprefix("ams_mel.") not in names,
                    "public module name shadows package export: " + module)
            module_exports = self.export_names(module)
            for name in module_exports:
                require(name in names, "public module export is absent from package: " + module + "." + name)
                require(self.resolve(module, name) == self.resolve("ams_mel", name),
                        "public module export differs from package: " + module + "." + name)
            public_modules.append({"name": module, "source": self.paths[module],
                                   "doc": ast.get_docstring(self.modules[module]) or "",
                                   "exports": module_exports})
        return {"schema": 1, "module": "ams_mel", "module_doc": ast.get_docstring(tree) or "",
                "exports": exports, "public_modules": public_modules, "sources": {
                    self.paths[m]: hashlib.sha256(text.encode()).hexdigest() for m, text in self.text.items()}}


def inventory(root):
    return Sources(root).inventory()


def targets(model):
    result = {}
    for item in model["exports"]:
        result[item["name"]] = "api/" + item["name"] + ".html"
        for group in ("members", "fields", "attributes", "enum_members"):
            for member in item[group]:
                result[item["name"] + "." + member["name"]] = result[item["name"]] + "#" + member["name"]
    exports = {item["name"]: item for item in model["exports"]}
    for module in model.get("public_modules", []):
        short = module["name"].removeprefix("ams_mel.")
        result[short] = "modules/" + short + ".html"
        for name in module["exports"]:
            result[short + "." + name] = result[name]
            for group in ("members", "fields", "attributes", "enum_members"):
                for member in exports[name][group]:
                    member_name = name + "." + member["name"]
                    result[short + "." + member_name] = result[member_name]
    return result


def reference(name, table, prefix="", owner=None, strict=False):
    normalized = name.removeprefix("~").removeprefix("ams_mel.")
    if normalized not in table and owner and owner + "." + normalized in table:
        normalized = owner + "." + normalized
    if normalized in table:
        return '<a href="' + html.escape(prefix + table[normalized], quote=True) + '"><code>' + html.escape(name) + "</code></a>"
    require(not strict, "unknown authored Python reference: " + name)
    return "<code>" + html.escape(name) + "</code>"


INLINE = re.compile(r":(?:class|meth|attr|func|mod|data):\x60([^\x60]+)\x60|\x60\x60([^\x60]+)\x60\x60|\*\*([^*]+)\*\*")


def inline(text, table, prefix="", owner=None):
    chunks, end = [], 0
    for match in INLINE.finditer(text):
        chunks.append(html.escape(text[end:match.start()]))
        if match.group(1):
            chunks.append(reference(match.group(1), table, prefix, owner))
        elif match.group(2):
            chunks.append("<code>" + html.escape(match.group(2)) + "</code>")
        else:
            chunks.append("<strong>" + html.escape(match.group(3)) + "</strong>")
        end = match.end()
    chunks.append(html.escape(text[end:]))
    return "".join(chunks)


def prose(text, table, prefix="", owner=None):
    if not text:
        return '<p class="muted">No explicit source description. The signature and values below are source-derived.</p>'
    blocks = []
    for block in re.split(r"\n\s*\n", text.strip()):
        lines = block.splitlines()
        if all(not line.strip() or line.startswith((" ", "\t", ">>>")) for line in lines):
            blocks.append("<pre><code>" + html.escape(block) + "</code></pre>")
        elif len(lines) == 2 and re.fullmatch(r"[-=]{3,}", lines[1].strip()):
            blocks.append("<h3>" + html.escape(lines[0]) + "</h3>")
        elif all(line.startswith("- ") for line in lines):
            blocks.append("<ul>" + "".join("<li>" + inline(line[2:], table, prefix, owner) + "</li>" for line in lines) + "</ul>")
        else:
            blocks.append("<p>" + inline(" ".join(line.strip() for line in lines), table, prefix, owner) + "</p>")
    return "\n".join(blocks)


NAVIGATION = (
    ("index.html", "Python overview"), ("getting-started.html", "Getting started"),
    ("ownership-and-errors.html", "Ownership and errors"), ("ir-workflows.html", "IR workflows"),
    ("rf-admin.html", "RF Admin"), ("api-index.html", "API index"),
)


def navigation(prefix="", modules=()):
    items = [('<a href="' + prefix + "../index.html" + '">Documentation portal</a>')]
    items.extend('<a href="' + prefix + path + '">' + label + "</a>" for path, label in NAVIGATION)
    items.extend('<a href="' + prefix + "modules/" + module["name"].removeprefix("ams_mel.") +
                 '.html"><code>' + html.escape(module["name"]) + "</code></a>" for module in modules)
    return '<nav aria-label="Python documentation"><ul>' + "".join("<li>" + item + "</li>" for item in items) + "</ul></nav>"


def page(title, body, prefix="", scripts=(), modules=()):
    return ('<!doctype html>\n<html lang="en"><head><meta charset="utf-8">\n'
            '<meta name="viewport" content="width=device-width, initial-scale=1">\n'
            "<title>" + html.escape(title) + " — AMS MEL Python</title>\n"
            '<link rel="stylesheet" href="' + prefix + 'assets/reference.css"></head>\n'
            '<body><a class="skip" href="#main">Skip to content</a>\n'
            "<header><p>AMS MEL Python documentation</p>" + navigation(prefix, modules) + "</header>\n"
            '<main id="main">' + body + "</main>\n"
            "<footer><p>Generated from the safe public Python source. Provider support remains runtime-specific.</p></footer>\n" +
            "".join('<script src="' + script + '" defer></script>\n' for script in scripts) +
            "</body></html>\n").encode()


def source_name(path):
    return Path(path).stem + ".html"


def source_link(item, prefix="../"):
    return '<a href="' + prefix + "source/" + source_name(item["source"]) + "#L" + str(item["line"]) + '">Source definition</a>'


def signature(item, name=None, strip_owner=False):
    parameters = item.get("parameters", [])
    if strip_owner and parameters and parameters[0]["name"] in {"self", "cls"}:
        parameters = parameters[1:]
    pieces = []
    for index, param in enumerate(parameters):
        kind = param["kind"]
        if kind == "keyword_only" and not any(p["kind"] in {"var_positional", "keyword_only"} for p in parameters[:index]):
            pieces.append("*")
        text = ("*" if kind == "var_positional" else "**" if kind == "var_keyword" else "") + param["name"]
        if param["annotation"]:
            text += ": " + (param.get("display_annotation") or param["annotation"])
        if param["default"] is not None:
            text += " = " + param["default"]
        pieces.append(text)
        if kind == "positional_only" and (index + 1 == len(parameters) or parameters[index + 1]["kind"] != kind):
            pieces.append("/")
    result = (name or item["name"]) + "(" + ", ".join(pieces) + ")"
    if item.get("returns"):
        result += " -> " + item["returns"]
    return result


def class_signature(item):
    if item["kind"] == "function":
        return signature(item)
    if item["constructor"]:
        return signature({**item["constructor"], "returns": None}, name=item["name"], strip_owner=True)
    if item["kind"] == "dataclass":
        params = [{"name": f["name"], "kind": "keyword_only" if f["kw_only"] else "positional_or_keyword",
                   "annotation": f["annotation"], "display_annotation": f.get("display_annotation"),
                   "default": "<factory>" if f["default_factory"] is not None else f["default"]}
                  for f in item["fields"] if f["init"]]
        params.sort(key=lambda p: p["kind"] == "keyword_only")
        return signature({"name": item["name"], "parameters": params, "returns": None})
    return "class " + item["name"]


def table_rows(headers, rows):
    return ('<div class="table-scroll"><table><thead><tr>' +
            "".join("<th>" + html.escape(h) + "</th>" for h in headers) +
            "</tr></thead><tbody>" + "".join("<tr>" + "".join("<td>" + cell + "</td>" for cell in row) +
            "</tr>" for row in rows) + "</tbody></table></div>")


def export_page(item, links, modules=()):
    name = item["name"]
    body = "<h1><code>ams_mel." + html.escape(name) + "</code></h1>"
    body += '<p class="kind">' + html.escape(item["kind"].capitalize()) + " · " + source_link(item) + "</p>"
    namespaces = [module for module in modules if name in module["exports"]]
    if namespaces:
        body += "<p>Public module: " + ", ".join(
            reference(module["name"], links, "../", strict=True) for module in namespaces) + ". "
        body += "Import " + ", ".join("<code>from " + html.escape(module["name"]) + " import " +
                                    html.escape(name) + "</code>" for module in namespaces) + ".</p>"
    rejected_constructor = (item.get("constructor") or {}).get("direct_construction_error")
    if rejected_constructor:
        body += "<pre><code>class " + html.escape(name) + "</code></pre>"
        body += ('<aside><p><strong>Create this owner through a factory method.</strong> '
                 'Direct construction raises <code>TypeError</code> with this source-defined message:</p><p><code>' +
                 html.escape(rejected_constructor) + '</code></p></aside>')
        body += ('<details><summary>Rejected direct constructor signature</summary><pre><code>' +
                 html.escape(class_signature(item)) + '</code></pre></details>')
    else:
        body += "<pre><code>" + html.escape(class_signature(item)) + "</code></pre>"
    body += prose(item["doc"], links, "../", name)
    if item["kind"] == "dataclass":
        body += "<p>This dataclass is " + ("frozen" if item["frozen"] else "mutable") + (
            ". Field defaults and factories are shown without executing them.</p>")
    if item["fields"]:
        body += "<h2>Fields</h2>"
        rows = []
        for field in item["fields"]:
            default = ("Factory: " + field["default_factory"] if field["default_factory"] is not None else
                       field["default"] if field["default"] is not None else "Required")
            rows.append(['<code id="' + field["name"] + '">' + html.escape(field["name"]) + "</code>",
                         "<code>" + html.escape(field["display_annotation"]) + "</code>",
                         "<code>" + html.escape(default) + "</code>",
                         ("Keyword only" if field["kw_only"] else "Constructor field") if field["init"] else "Not in constructor"])
        body += table_rows(("Name", "Type", "Default", "Construction"), rows)
    if item["enum_members"]:
        body += "<h2>Named values</h2>" + table_rows(("Name", "Value"), [
            ['<code id="' + member["name"] + '">' + html.escape(member["name"]) + "</code>",
             "<code>" + html.escape(repr(member["value"])) + "</code>"] for member in item["enum_members"]])
    if item["attributes"]:
        body += "<h2>Attributes</h2>" + table_rows(("Name", "Type", "Source value"), [
            ['<code id="' + attr["name"] + '">' + html.escape(attr["name"]) + "</code>",
             "<code>" + html.escape(attr["annotation"] or "Not annotated") + "</code>",
             "<code>" + html.escape(attr["value"] or "No class default") + "</code>"] for attr in item["attributes"]])
    if item["members"]:
        body += "<h2>Methods and properties</h2><ul>" + "".join(
            '<li><a href="#' + member["name"] + '"><code>' + html.escape(member["name"]) + "</code></a></li>"
            for member in item["members"]) + "</ul>"
    for member in item["members"]:
        label = (member["name"] + (": " + member["returns"] if member["returns"] else "")
                 if member["binding"] == "property" else signature(member, strip_owner=member["binding"] != "static"))
        body += '<section><h3 id="' + member["name"] + '"><code>' + html.escape(member["name"]) + "</code></h3>"
        body += "<pre><code>" + html.escape(label) + "</code></pre>"
        body += "<p>" + {"instance": "Instance method", "class": "Class method", "static": "Static method",
                         "property": "Read-only property"}[member["binding"]]
        body += ("; inherited public operation" if member["inherited"] else "") + " · " + source_link(member) + "</p>"
        body += prose(member["doc"], links, "../", name) + "</section>"
    return page("ams_mel." + name, body, "../", modules=modules)


def module_page(module, exports, links, modules):
    name = module["name"]
    body = "<h1><code>" + html.escape(name) + "</code></h1>"
    body += "<p>Public module · " + source_link({"source": module["source"], "line": 1}) + "</p>"
    body += "<pre><code>from ams_mel import " + html.escape(name.removeprefix("ams_mel.")) + "</code></pre>"
    body += prose(module["doc"], links, "../")
    body += ("<h2>Public exports</h2><p>These names refer to the same objects exported by "
             "<code>ams_mel</code>. Their canonical reference pages retain the existing URLs.</p>")
    body += table_rows(("Name", "Kind", "Description"), [
        [reference(name + "." + exported, links, "../", strict=True),
         html.escape(exports[exported]["kind"]),
         inline(exports[exported]["doc"].split("\n", 1)[0], links, "../", exported)]
        for exported in sorted(module["exports"], key=str.casefold)])
    return page(name, body, "../", modules=modules)


def generate(root):
    root = Path(root)
    model = inventory(root)
    links = targets(model)
    modules = model["public_modules"]
    files = {}
    entries = []
    for item in model["exports"]:
        name = item["name"]
        files[links[name]] = export_page(item, links, modules)
        entries.append({"name": name, "kind": item["kind"], "url": links[name],
                        "description": item["doc"].split("\n", 1)[0]})
        for group in ("members", "fields", "attributes", "enum_members"):
            for member in item[group]:
                qualified = name + "." + member["name"]
                entries.append({"name": qualified, "kind": {
                    "members": member.get("binding", "method"), "fields": "field",
                    "attributes": "attribute", "enum_members": "enum value"}[group],
                    "url": links[qualified], "description": member.get("doc", "").split("\n", 1)[0]})
    exports = {item["name"]: item for item in model["exports"]}
    canonical_entries = {entry["name"]: entry for entry in entries}
    for module in modules:
        short = module["name"].removeprefix("ams_mel.")
        files[links[short]] = module_page(module, exports, links, modules)
        entries.append({"name": module["name"], "kind": "module", "url": links[short],
                        "description": module["doc"].split("\n", 1)[0]})
        for name in module["exports"]:
            qualified_names = [name] + [name + "." + member["name"]
                for group in ("members", "fields", "attributes", "enum_members")
                for member in exports[name][group]]
            for qualified in qualified_names:
                canonical_entries[qualified].setdefault("aliases", []).append(module["name"] + "." + qualified)
    module_directory = ""
    if modules:
        module_directory = table_rows(("Module", "Public exports", "Description"), [
            [reference(module["name"], links, strict=True), str(len(module["exports"])),
             inline(module["doc"].split("\n", 1)[0], links)] for module in modules])
    directory = "<ul class=\"api-directory\">" + "".join(
        "<li>" + reference(item["name"], links, strict=True) + ' <span class="muted">' +
        html.escape(item["kind"]) + "</span></li>" for item in sorted(model["exports"], key=lambda e: e["name"].casefold())) + "</ul>"
    member_directory = "<h2>Members by export</h2>"
    for item in sorted(model["exports"], key=lambda e: e["name"].casefold()):
        members = [member for group in ("members", "fields", "attributes", "enum_members") for member in item[group]]
        if members:
            member_directory += "<details><summary><code>" + html.escape(item["name"]) + "</code></summary><ul>"
            member_directory += "".join("<li>" + reference(item["name"] + "." + member["name"], links,
                                                         strict=True) + "</li>" for member in members)
            member_directory += "</ul></details>"
    search = '<h1>Python API index</h1>'
    if modules:
        search += '<h2>Public modules</h2>' + module_directory
    search += ('<p>Every name exported by <code>ams_mel.__all__</code> appears below. '
              'Search also includes methods, fields and named enum values. Private implementation modules are not application APIs.</p>'
              '<label for="api-search">Search the Python API</label><input id="api-search" type="search" '
              'autocomplete="off" placeholder="Name, method, field or description">'
              '<p id="search-status" role="status" aria-live="polite">The complete alphabetical directory is available below.</p>'
              '<ul id="search-results" hidden></ul><h2>Public exports</h2>' + directory + member_directory)
    files["api-index.html"] = page("Python API index", search,
                                   scripts=("assets/search-index.js", "assets/search.js"), modules=modules)
    guide_paths = sorted((root / "docs/python").glob("*.html"))
    require(guide_paths, "missing authored Python guides")
    require({p.name for p in guide_paths} == {path for path, _ in NAVIGATION if path != "api-index.html"},
            "unexpected or missing authored Python guide")
    token = re.compile(r"\{\{([^{}]+)\}\}")
    for path in guide_paths:
        require(not path.is_symlink(), "symlink Python guide")
        content = path.read_text(encoding="utf-8")
        def replace(match):
            name = match.group(1)
            if name == "navigation":
                return navigation(modules=modules)
            if name == "api-directory":
                return directory
            if name == "module-directory":
                return module_directory
            if name.startswith("api:"):
                return reference(name[4:], links, strict=True)
            raise ReferenceError("unknown Python guide token: " + name)
        content = token.sub(replace, content)
        require("{{" not in content and "}}" not in content, "malformed Python guide token")
        files[path.name] = content.encode()
    source_paths = sorted({item["source"] for item in model["exports"]} |
                          {member["source"] for item in model["exports"] for member in item["members"]} |
                          {module["source"] for module in modules})
    for path in source_paths:
        content = (root / path).read_text(encoding="utf-8")
        lines = "".join('<span id="L' + str(number) + '">' + html.escape(line) + "</span>\n"
                        for number, line in enumerate(content.splitlines(), 1))
        body = "<h1>Python source: <code>" + html.escape(path) + "</code></h1>"
        body += "<p>This source view explains definitions; private names are implementation details, not additional supported public APIs.</p>"
        body += '<pre class="source"><code>' + lines + "</code></pre>"
        files["source/" + source_name(path)] = page(path, body, "../", modules=modules)
    for name in ("reference.css", "search.js"):
        path = root / "docs/python" / name
        require(path.is_file() and not path.is_symlink(), "missing Python reference asset: " + name)
        files["assets/" + name] = path.read_bytes()
    search_data = json.dumps(sorted(entries, key=lambda e: e["name"].casefold()), ensure_ascii=True, separators=(",", ":"))
    files["assets/search-index.js"] = ("'use strict';\nwindow.AMS_MEL_PYTHON_INDEX = " +
                                      search_data.replace("<", "\\u003c") + ";\n").encode()
    operations = [member for item in model["exports"] for member in item["members"]]
    report = {"schema": 1, "inventory": model,
              "counts": {"exports": len(model["exports"]), "public_modules": len(modules),
                         "public_module_exports": sum(len(module["exports"]) for module in modules),
                         "members": len(operations),
                         "fields": sum(len(item["fields"]) for item in model["exports"]),
                         "attributes": sum(len(item["attributes"]) for item in model["exports"]),
                         "enum_values": sum(len(item["enum_members"]) for item in model["exports"]),
                         "search_records": len(entries), "html": sum(n.endswith(".html") for n in files),
                         "assets": sum(not n.endswith(".html") for n in files)},
              "documentation": {"exports_with_explicit_docstrings": sum(bool(e["doc"]) for e in model["exports"]),
                                "exports_without_explicit_docstrings": [e["name"] for e in model["exports"] if not e["doc"]],
                                "public_operations_with_explicit_docstrings": sum(bool(m["doc"]) for m in operations),
                                "public_operations_without_explicit_docstrings": [
                                    e["name"] + "." + m["name"] for e in model["exports"] for m in e["members"] if not m["doc"]],
                                "policy": "Informational; signatures, field/enum inventories and prose quality are distinct measures."}}
    return files, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    require(not args.output.exists() and not any(p.is_symlink() for p in (args.output, *args.output.parents)),
            "output must be fresh without symlink ancestors")
    files, report = generate(args.root)
    args.output.mkdir(parents=True)
    for name, data in files.items():
        target = args.output / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    (args.output / "python-reference.json").write_bytes(canonical(report))
    print(json.dumps({"counts": report["counts"], "documentation": report["documentation"]}, indent=2))


if __name__ == "__main__":
    main()
