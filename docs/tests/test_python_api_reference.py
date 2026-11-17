"""Source-only API fidelity, rejection of unsupported shapes and safe HTML."""

from html.parser import HTMLParser
import json
from pathlib import Path
import posixpath
import sys
import tempfile
import unittest
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "docs/tools"))
import python_api_reference as reference


PACKAGE = '''"""A source-only package fixture."""
from ._model import Owner as Alias, Value, Kind, run
__all__ = ["Alias", "Value", "Kind", "run"]
raise AssertionError("The documentation generator imported the binding")
'''

MODEL = '''from __future__ import annotations
from dataclasses import dataclass, field
from enum import IntEnum
from typing import ClassVar
from . import _native

class _Parent:
    def __init__(self, code: int = 0):
        self.code = code

    @property
    def is_open(self) -> bool:
        """Whether the owner is open."""
        return True

    def close(self) -> None:
        """Close this owner."""
        pass

    def __enter__(self) -> Owner:
        """Enter the owner context."""
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> None:
        """Close on leaving the context."""
        self.close()

    def _secret(self):
        pass

class Owner(_Parent):
    """Open with :meth:`open`; inspect :class:`Value` and :class:`~ams_mel.Kind`."""

    @classmethod
    def open(cls, target: str, /, *, timeout: float = 1.25) -> Owner:
        """Open an owner; close it after use."""
        return cls()

    @staticmethod
    def validate(value: int, /, *limits: int, strict: bool = True, **labels: str) -> bool:
        """Validate a value."""
        return True

    def close(self) -> None:
        """Close this specific owner."""
        pass

@dataclass(frozen=True, kw_only=True)
class Value:
    """A value with a required name and an independent tuple default."""
    name: str
    samples: tuple[int, ...] = field(default_factory=tuple)
    status: int = field(default=9, init=False)
    category: ClassVar[str] = "value"

class Kind(IntEnum):
    """Named values including an intentional alias."""
    FIRST = _native.FIRST
    SECOND = _native.SECOND
    SHIFTED = 1 << 4
    ALIAS = SHIFTED
    ANNOTATED: int = 7

def run(items: tuple[int, ...] = (), /, enabled: bool = True, *, label: str = "<b>") -> int:
    """Return the input count."""
    return len(items)
'''


class Links(HTMLParser):
    def __init__(self, text):
        super().__init__(convert_charrefs=True)
        self.ids, self.links, self.tags = set(), [], []
        self.feed(text)

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        self.tags.append((tag, attrs))
        if "id" in attrs:
            if attrs["id"] in self.ids:
                raise AssertionError("duplicate HTML id: " + attrs["id"])
            self.ids.add(attrs["id"])
        self.links.extend(attrs[key] for key in ("href", "src") if key in attrs)


class PythonReferenceTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.package = self.root / "python/ams_mel"
        self.package.mkdir(parents=True)
        (self.package / "__init__.py").write_text(PACKAGE)
        (self.package / "_model.py").write_text(MODEL)
        (self.package / "_native.py").write_text(
            'FIRST, SECOND, THIRD = range(4, 7)\nraise AssertionError("Native import forbidden")\n')
        docs = self.root / "docs/python"
        docs.mkdir(parents=True)
        for name, _ in reference.NAVIGATION:
            if name == "api-index.html":
                continue
            (docs / name).write_text(
                '<!doctype html><html><head><title>Fixture</title>'
                '<link rel="stylesheet" href="assets/reference.css"></head><body>'
                '{{navigation}}<main id="main"><p>{{api:Alias.open}} and {{api:Value}}</p>'
                '{{api-directory}}</main></body></html>')
        for name in ("reference.css", "search.js"):
            (docs / name).write_bytes((ROOT / "docs/python" / name).read_bytes())

    def records(self):
        return {item["name"]: item for item in reference.inventory(self.root)["exports"]}

    def rewrite(self, filename, before, after):
        path = self.package / filename
        content = path.read_text()
        self.assertIn(before, content)
        path.write_text(content.replace(before, after))

    def add_public_modules(self):
        (self.package / "common.py").write_text(
            '"""Shared values for :mod:`ams_mel.ir`."""\n'
            'from ._model import Value, Kind\n__all__ = ["Value", "Kind"]\n'
            'raise AssertionError("Public module import forbidden")\n')
        (self.package / "ir.py").write_text(
            '"""IR owners; see :class:`ams_mel.common.Value`."""\n'
            'from ._model import Owner as Alias, run\n__all__ = ["run", "Alias"]\n')
        self.rewrite("__init__.py", "from ._model import Owner as Alias, Value, Kind, run",
                     "from . import common, ir\nfrom .common import Value, Kind\nfrom .ir import Alias, run")

    def test_public_modules_preserve_ordered_aliases_and_canonical_identity(self):
        self.add_public_modules()
        model = reference.inventory(self.root)
        self.assertEqual(model["public_modules"], [
            {"name": "ams_mel.common", "source": "python/ams_mel/common.py",
             "doc": "Shared values for :mod:`ams_mel.ir`.", "exports": ["Value", "Kind"]},
            {"name": "ams_mel.ir", "source": "python/ams_mel/ir.py",
             "doc": "IR owners; see :class:`ams_mel.common.Value`.", "exports": ["run", "Alias"]}])
        self.assertEqual([item["name"] for item in model["exports"]], ["Alias", "Value", "Kind", "run"])
        links = reference.targets(model)
        self.assertEqual(links["ir.Alias"], links["Alias"])
        self.assertEqual(links["ir.Alias.open"], links["Alias.open"])
        self.assertEqual(links["common.Kind.FIRST"], links["Kind.FIRST"])
        self.assertEqual(links["ir"], "modules/ir.html")
        self.assertNotIn("ams_mel", sys.modules)

    def test_public_module_exports_reject_inaccurate_or_dynamic_reexports(self):
        variants = (
            ('from ._model import Value\n', "public __all__ must be one literal assignment"),
            ('from ._model import Value\n__all__ = ["Value", "Value"]\n', "duplicate public exports"),
            ('from ._model import Value\n__all__ = ["Value"]\n__all__.clear()\n', "dynamic __all__ mutation"),
            ('from ._model import Owner\n__all__ = ["Owner"]\n', "absent from package"),
            ('from ._model import Value as Alias\n__all__ = ["Alias"]\n', "differs from package"),
            ('from ._model import Value\n__all__ = ["Value"]\nif enabled:\n    Value = None\n',
             "ambiguous public binding"),
        )
        for source, reason in variants:
            with self.subTest(reason=reason):
                (self.package / "common.py").write_text(source)
                with self.assertRaisesRegex(reference.ReferenceError, reason):
                    reference.inventory(self.root)

    def test_public_module_name_cannot_shadow_canonical_export(self):
        (self.package / "Value.py").write_text('from ._model import Value\n__all__ = ["Value"]\n')
        with self.assertRaisesRegex(reference.ReferenceError, "shadows package export"):
            reference.inventory(self.root)

    def test_module_pages_navigation_search_and_qualified_references_are_discoverable(self):
        self.add_public_modules()
        guide = self.root / "docs/python/index.html"
        guide.write_text(guide.read_text().replace("{{api-directory}}", "{{module-directory}}"))
        files, report = reference.generate(self.root)
        self.assertEqual(report["counts"]["public_modules"], 2)
        self.assertEqual(report["counts"]["public_module_exports"], 4)
        self.assertIn(b'<a href="modules/ir.html"><code>ams_mel.ir</code></a>', files["index.html"])
        self.assertIn(b'<a href="../modules/common.html"><code>ams_mel.common</code></a>', files["api/Value.html"])
        self.assertIn(b"from ams_mel.ir import Alias", files["api/Alias.html"])
        self.assertIn(b'../api/Value.html', files["modules/common.html"])
        self.assertIn(b'../source/common.html#L1', files["modules/common.html"])
        self.assertIn(b'<a href="../api/Value.html"><code>ams_mel.common.Value</code></a>',
                      files["modules/ir.html"])
        script = files["assets/search-index.js"].decode()
        entries = {e["name"]: e for e in json.loads(script.split(" = ", 1)[1].removesuffix(";\n"))}
        for name in ("Alias", "Alias.open", "ams_mel.ir"):
            self.assertIn(name, entries)
        self.assertEqual(entries["Alias"]["aliases"], ["ams_mel.ir.Alias"])
        self.assertEqual(entries["Alias.open"]["aliases"], ["ams_mel.ir.Alias.open"])
        self.assertEqual(entries["ams_mel.ir"]["url"], "modules/ir.html")
        self.assertEqual(len({entry["url"] for entry in entries.values()}), len(entries))
        self.assertNotIn("ams_mel", sys.modules)

    def test_reexports_alias_identity_and_source_only_generation(self):
        self.rewrite("__init__.py", "from ._model import Owner as Alias, Value, Kind, run",
                     "from ._shim import Alias\nfrom ._model import Value, Kind, run")
        (self.package / "_shim.py").write_text("from ._model import Owner as _Owner\nAlias = _Owner\n")
        model = reference.inventory(self.root)
        self.assertEqual([item["name"] for item in model["exports"]], ["Alias", "Value", "Kind", "run"])
        owner = model["exports"][0]
        self.assertEqual((owner["definition_module"], owner["definition_name"]), ("ams_mel._model", "Owner"))
        self.assertIn("python/ams_mel/_native.py", model["sources"])
        files, report = reference.generate(self.root)
        self.assertIn("api/Alias.html", files)
        self.assertEqual(report["inventory"], model)
        self.assertNotIn("ams_mel", sys.modules)

    def test_exact_parameter_kinds_defaults_and_descriptor_bindings(self):
        records = self.records()
        methods = {member["name"]: member for member in records["Alias"]["members"]}
        opened = methods["open"]
        self.assertEqual(opened["binding"], "class")
        self.assertEqual([p["kind"] for p in opened["parameters"]],
                         ["positional_only", "positional_only", "keyword_only"])
        self.assertEqual(opened["parameters"][-1]["default"], "1.25")
        self.assertEqual(reference.signature(opened, strip_owner=True),
                         "open(target: str, /, *, timeout: float = 1.25) -> Owner")
        validator = methods["validate"]
        self.assertEqual(validator["binding"], "static")
        self.assertEqual([p["kind"] for p in validator["parameters"]],
                         ["positional_only", "var_positional", "keyword_only", "var_keyword"])
        self.assertEqual(records["run"]["parameters"][0]["default"], "()")
        self.assertIn("/, enabled: bool = True, *, label: str = '<b>'", reference.signature(records["run"]))

    def test_dataclass_fields_factories_and_class_attributes(self):
        value = self.records()["Value"]
        self.assertTrue(value["frozen"])
        self.assertEqual([field["name"] for field in value["fields"]], ["name", "samples", "status"])
        fields = {field["name"]: field for field in value["fields"]}
        self.assertIsNone(fields["name"]["default"])
        self.assertEqual(fields["samples"]["default_factory"], "tuple")
        self.assertIsNone(fields["samples"]["default"])
        self.assertEqual(fields["status"]["default"], "9")
        self.assertFalse(fields["status"]["init"])
        self.assertTrue(all(field["kw_only"] for field in value["fields"]))
        self.assertEqual([a["name"] for a in value["attributes"]], ["category"])
        self.assertEqual(reference.class_signature(value),
                         "Value(*, name: str, samples: tuple[int, ...] = <factory>)")

    def test_custom_dataclass_constructor_is_not_replaced_by_fields(self):
        self.rewrite("_model.py", '    category: ClassVar[str] = "value"',
                     '    category: ClassVar[str] = "value"\n'
                     '    def __init__(self, label: bytes, /):\n        pass')
        self.assertEqual(reference.class_signature(self.records()["Value"]), "Value(label: bytes, /)")

    def test_factory_only_construction_is_derived_from_the_exact_raise_body(self):
        self.rewrite("_model.py", "        self.code = code", '        raise TypeError("Use Owner.open")')
        owner = self.records()["Alias"]
        self.assertEqual(owner["constructor"]["direct_construction_error"], "Use Owner.open")
        files, _ = reference.generate(self.root)
        body = files["api/Alias.html"].decode()
        self.assertIn("Create this owner through a factory method", body)
        self.assertIn("Rejected direct constructor signature", body)
        self.assertIn("Use Owner.open", body)
        self.rewrite("_model.py", '        raise TypeError("Use Owner.open")',
                     '        if code < 0:\n            raise TypeError("Use Owner.open")')
        self.assertNotIn("direct_construction_error", self.records()["Alias"]["constructor"])

    def test_enum_values_resolve_native_constants_ranges_arithmetic_and_aliases(self):
        values = self.records()["Kind"]["enum_members"]
        self.assertEqual([(m["name"], m["value"]) for m in values],
                         [("FIRST", 4), ("SECOND", 5), ("SHIFTED", 16), ("ALIAS", 16), ("ANNOTATED", 7)])
        self.assertEqual(values[0]["expression"], "_native.FIRST")
        self.assertEqual(values[3]["expression"], "SHIFTED")

    def test_inherited_public_operations_keep_their_defining_source(self):
        owner = self.records()["Alias"]
        methods = {member["name"]: member for member in owner["members"]}
        self.assertEqual(set(methods), {"close", "is_open", "__enter__", "__exit__", "open", "validate"})
        self.assertTrue(methods["is_open"]["inherited"])
        self.assertEqual(methods["is_open"]["declaring_class"], "_Parent")
        self.assertEqual(methods["is_open"]["binding"], "property")
        self.assertFalse(methods["close"]["inherited"])
        self.assertEqual(methods["close"]["declaring_class"], "Owner")
        self.assertTrue(owner["constructor"]["inherited"])
        self.assertEqual(reference.class_signature(owner), "Alias(code: int = 0)")
        self.assertEqual([(a["name"], a["annotation"]) for a in owner["attributes"]], [("code", "int")])
        self.assertNotIn("_Parent", reference.targets(reference.inventory(self.root)))

    def test_missing_duplicate_and_dynamic_exports_fail_explicitly(self):
        variants = (
            (PACKAGE.replace('"run"]', '"absent"]'), "unresolved public binding"),
            (PACKAGE.replace('"run"]', '"run", "run"]'), "duplicate public exports"),
            (PACKAGE + '__all__.append("new")\n', "dynamic __all__ mutation"),
            (PACKAGE + '__all__ += ["new"]\n', "dynamic __all__ mutation"),
            (PACKAGE + '__all__[0] = "new"\n', "dynamic __all__ mutation"),
            (PACKAGE.replace('["Alias", "Value", "Kind", "run"]', 'list(("Alias",))'), "literal names"),
            (PACKAGE + 'from ._model import *\n', "wildcard import"),
        )
        for source, reason in variants:
            with self.subTest(reason=reason):
                (self.package / "__init__.py").write_text(source)
                with self.assertRaisesRegex(reference.ReferenceError, reason):
                    reference.inventory(self.root)

    def test_conditional_rebindings_are_not_silently_selected(self):
        for suffix in ('if enabled:\n    Alias = Value\n',
                       'if enabled:\n    from ._model import Value as Alias\n',
                       'for Alias in objects:\n    pass\n'):
            with self.subTest(suffix=suffix):
                (self.package / "__init__.py").write_text(PACKAGE + suffix)
                with self.assertRaisesRegex(reference.ReferenceError, "ambiguous public binding"):
                    reference.inventory(self.root)

    def test_unrepresentable_public_shapes_fail_instead_of_disappearing(self):
        variants = (
            (MODEL.replace("def run(", "async def run("), "async public callable"),
            (MODEL.replace("class Owner(_Parent):", "class Owner(_Parent, object):"), "multiple public class bases"),
            (MODEL.replace("    @staticmethod", "    @custom_decorator"), "unsupported public method decorator"),
            (MODEL.replace("class Owner(_Parent):", "class Owner(_Parent, metaclass=Custom):"), "metaclass/generic"),
            (MODEL.replace("    FIRST = _native.FIRST", "    FIRST = choose_value()"), "unsupported constant expression"),
            (MODEL.replace("    FIRST = _native.FIRST", "    if enabled:\n        FIRST = 1"), "dynamic/nested public class body"),
            (MODEL.replace("    FIRST = _native.FIRST", "    FIRST = 1\n    FIRST = 2"), "duplicate public class member"),
        )
        for source, reason in variants:
            with self.subTest(reason=reason):
                (self.package / "_model.py").write_text(source)
                with self.assertRaisesRegex(reference.ReferenceError, reason):
                    reference.inventory(self.root)

    def test_private_type_alias_is_expanded_for_display(self):
        self.rewrite("_model.py", "def run(items: tuple[int, ...]", "_Payload = tuple[int, ...]\n\ndef run(items: _Payload")
        item = self.records()["run"]["parameters"][0]
        self.assertEqual(item["annotation"], "_Payload")
        self.assertEqual(item["display_annotation"], "tuple[int, ...]")

    def test_every_reference_anchor_and_local_destination_exists(self):
        self.add_public_modules()
        files, report = reference.generate(self.root)
        pages = {name: Links(data.decode()) for name, data in files.items() if name.endswith(".html")}
        for name, parser in pages.items():
            for href in parser.links:
                url = urlsplit(href)
                self.assertFalse(url.scheme or url.netloc, (name, href))
                destination = posixpath.normpath(posixpath.join(posixpath.dirname(name), unquote(url.path))) if url.path else name
                if destination == "../index.html":
                    continue
                self.assertIn(destination, files, (name, href))
                if url.fragment:
                    self.assertIn(unquote(url.fragment), pages[destination].ids, (name, href))
        links = reference.targets(report["inventory"])
        for public_name, destination in links.items():
            path, separator, anchor = destination.partition("#")
            self.assertIn(path, pages, public_name)
            if separator:
                self.assertIn(anchor, pages[path].ids, public_name)
            self.assertIn('href="' + destination + '"', files["api-index.html"].decode(), public_name)

    def test_docstring_html_is_escaped_and_known_roles_link_safely(self):
        self.rewrite("_model.py", 'Return the input count.',
                     'Use <script>alert(1)</script>, <img src=x onerror=alert(1)> and ``<code>``.')
        files, _ = reference.generate(self.root)
        body = files["api/run.html"].decode()
        self.assertIn("&lt;script&gt;alert(1)&lt;/script&gt;", body)
        self.assertIn("<code>&lt;code&gt;</code>", body)
        self.assertFalse(any(tag in {"script", "img"} for tag, _ in Links(body).tags))
        self.assertNotIn(b"<script>", files["assets/search-index.js"])
        self.assertIn(b"\\u003cscript>", files["assets/search-index.js"])
        owner = files["api/Alias.html"].decode()
        self.assertIn('href="../api/Alias.html#open"', owner)
        self.assertIn('href="../api/Value.html"', owner)
        self.assertIn('href="../api/Kind.html"', owner)

    def test_search_inventory_contains_every_export_and_member(self):
        files, report = reference.generate(self.root)
        script = files["assets/search-index.js"].decode()
        entries = json.loads(script.split(" = ", 1)[1].removesuffix(";\n"))
        self.assertEqual({e["name"]: e["url"] for e in entries}, reference.targets(report["inventory"]))
        self.assertEqual(len(entries), report["counts"]["search_records"])
        self.assertNotIn(b"fetch(", files["assets/search.js"])
        self.assertNotIn(b"innerHTML", files["assets/search.js"])
        self.assertIn(b"textContent", files["assets/search.js"])

    def test_missing_prose_is_reported_without_a_generation_threshold(self):
        self.rewrite("_model.py", '    """Named values including an intentional alias."""\n', "")
        files, report = reference.generate(self.root)
        self.assertEqual(report["documentation"]["exports_without_explicit_docstrings"], ["Kind"])
        self.assertIn(b"No explicit source description", files["api/Kind.html"])
        self.assertEqual(report["counts"]["exports"], 4)

    def test_unknown_or_malformed_authored_tokens_are_errors(self):
        path = self.root / "docs/python/index.html"
        for text in ("{{api:Alias.missing}}", "{{unknown}}", "{{api:Alias}"):
            path.write_text(text)
            with self.subTest(text=text), self.assertRaises(reference.ReferenceError):
                reference.generate(self.root)

    def test_source_hashes_include_nonpublic_implementation_changes(self):
        before = reference.inventory(self.root)
        path = self.package / "_native.py"
        path.write_text(path.read_text() + "# implementation-only edit\n")
        after = reference.inventory(self.root)
        self.assertEqual(before["exports"], after["exports"])
        self.assertNotEqual(before["sources"], after["sources"])


if __name__ == "__main__":
    unittest.main()
