"""DOC-PERF-001 additive controls; run separately from the frozen CI-001 inventory.

Use authentic qualified sites and the installed pinned enhancer. These controls
do not replace any inherited test or authorize changes to the shard inventory.
"""

import copy
import json
from pathlib import Path
import re
import shutil
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "docs/tools"))
import build_enhanced_site as publication
import symbol_audit


class InvocationIsolationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workspace = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.workspace.cleanup)
        cls.pristine = ROOT / "_site"
        cls.approved = Path(cls.workspace.name) / "approved"
        publication.build(cls.pristine, cls.approved)
        cls.authority = publication.Authority(ROOT, cls.pristine)
        cls.symbols = symbol_audit.inventory(cls.authority, cls.pristine)

    def setUp(self):
        self.workspace = tempfile.TemporaryDirectory()
        self.addCleanup(self.workspace.cleanup)
        self.base = Path(self.workspace.name)

    def test_public_projection_is_per_source_and_per_invocation(self):
        paths = {ROOT / entry["source"] for entry in self.authority.types.values()}
        with mock.patch.object(symbol_audit, "specification", wraps=symbol_audit.specification) as parse:
            for repeat in (1, 2):
                self.assertEqual(symbol_audit.inventory(self.authority, self.pristine), self.symbols)
                self.assertEqual(parse.call_count, repeat * len(paths))

    def test_existing_authority_cannot_reuse_public_source_projection(self):
        root = self.base / "root"
        shutil.copytree(ROOT / "ada/src", root / "ada/src")
        authority = copy.deepcopy(self.authority)
        authority.root = root
        path = root / "ada/src/ams-mel.ads"
        original = path.read_bytes()
        self.assertEqual(symbol_audit.inventory(authority, self.pristine), self.symbols)
        for before, after in ((b"type Version is record", b"type Forgery is record"),
                              (b"type Session is limited private", b"type Forgery is limited private")):
            self.assertIn(before, original)
            path.write_bytes(original.replace(before, after, 1))
            with self.assertRaisesRegex(ValueError, "public source identity"):
                symbol_audit.inventory(authority, self.pristine)
            path.write_bytes(original)
            self.assertEqual(symbol_audit.inventory(authority, self.pristine), self.symbols)

    def test_native_enumeration_is_reread_between_calls(self):
        native = self.base / "pristine"
        shutil.copytree(self.pristine, native)
        entry = next(e for e in self.symbols if "…" in e["native_profile"])
        path = native / "api" / entry["page"]
        original = path.read_text()
        marker = "<h4 id=" + entry["anchor"] + ">"
        start = original.index(marker)
        offset = original.index("<dt>", start) + len("<dt>")
        self.assertEqual(symbol_audit.inventory(self.authority, native), self.symbols)
        for name in ("Forged_First", "Forged_Second"):
            path.write_text(original[:offset] + name + original[offset:])
            with self.assertRaisesRegex(ValueError, "enumeration literals"):
                symbol_audit.inventory(self.authority, native)
            path.write_text(original)
            self.assertEqual(symbol_audit.inventory(self.authority, native), self.symbols)

    def test_publication_success_mutation_restoration_and_second_mutation(self):
        site = self.base / "publication"
        shutil.copytree(self.approved, site)
        expected = publication.validate(site, self.pristine)
        index = site / "api/index.html"
        javascript = site / "api/gnatdoc-navigation.js"
        package = next(p for p in sorted((site / "api").glob("*.html"))
                       if "type-reference" in p.read_text())
        text = package.read_text()
        link = re.search(r'<a class="type-reference"[^>]*>[^<]+</a>', text)[0]
        anchor = re.search(r'<h4 id=([a-f0-9]{64})>', text)[1]
        first, second = self.symbols[:2]
        first_href = first["page"] + "#" + first["anchor"]
        second_href = second["page"] + "#" + second["anchor"]
        changes = [
            (javascript, javascript.read_bytes() + b"\n/* mutation */"),
            (package, text.replace(anchor, "0" * 64, 1).encode()),
            (package, text.replace(link, re.sub(r"<[^>]+>", "", link), 1).encode()),
            (package, text.replace(link, link.replace('data-type="', 'data-type="forged.', 1), 1).encode()),
            (index, index.read_bytes().replace(first_href.encode(), second_href.encode(), 1)),
            (index, index.read_bytes().replace(b'data-symbol="AMS.MEL.', b'data-symbol="AMS.MEL.Internal.', 1)),
        ]
        for path, mutation in changes:
            with self.subTest(path=path.name, mutation=publication.sha(mutation)):
                original = path.read_bytes()
                manifest = site / "api/presentation.json"
                manifest_bytes = manifest.read_bytes()
                path.write_bytes(mutation)
                forged = json.loads(manifest_bytes)
                forged["output_sha256"][path.relative_to(site).as_posix()] = publication.sha(mutation)
                manifest.write_text(json.dumps(forged))
                try:
                    with self.assertRaises(ValueError):
                        publication.validate(site, self.pristine)
                finally:
                    path.write_bytes(original)
                    manifest.write_bytes(manifest_bytes)
                self.assertEqual(publication.validate(site, self.pristine), expected)
        extra = site / "unexpected.txt"
        extra.write_text("unexpected")
        with self.assertRaisesRegex(ValueError, "unexpected publication files"):
            publication.validate(site, self.pristine)
        extra.unlink()
        original = javascript.read_bytes()
        javascript.unlink()
        javascript.symlink_to(self.approved / "api/gnatdoc-navigation.js")
        with self.assertRaisesRegex(ValueError, "symlink input/output member"):
            publication.validate(site, self.pristine)
        javascript.unlink()
        javascript.write_bytes(original)
        self.assertEqual(publication.validate(site, self.pristine), expected)

    def test_authoritative_source_configuration_and_ledger_between_calls(self):
        root = self.base / "root"
        shutil.copytree(ROOT / "ada/src", root / "ada/src")
        shutil.copytree(ROOT / "docs", root / "docs",
                        ignore=shutil.ignore_patterns("alire", "obj", "bin", "__pycache__"))
        publication.qualified_original(self.pristine, root)
        paths = [root / "ada/src/ams-mel.ads", root / "docs/gnatdoc-enhance.toml",
                 root / "docs/gnatdoc-native-anchors.json"]
        for path in paths:
            original = path.read_bytes()
            for mutation in (original + b"\n", original + b"\n\n"):
                # TOML whitespace is semantically unchanged: change its actual
                # project value instead, without altering the configuration pin.
                if path.suffix == ".toml":
                    mutation = original.replace(b"AMS MEL Bridge", b"Forged Project")
                path.write_bytes(mutation)
                try:
                    with self.assertRaises(ValueError):
                        publication.validate(self.approved, self.pristine, root)
                finally:
                    path.write_bytes(original)
                publication.qualified_original(self.pristine, root)

    def test_wrong_native_input_after_success(self):
        pristine = self.base / "pristine"
        shutil.copytree(self.pristine, pristine)
        publication.validate(self.approved, pristine)
        page = pristine / "api" / self.symbols[0]["page"]
        original = page.read_bytes()
        for mutation in (original + b"\n", original.replace(self.symbols[0]["anchor"].encode(), b"0" * 64, 1)):
            self.assertNotEqual(original, mutation)
            page.write_bytes(mutation)
            with self.assertRaisesRegex(ValueError, "pristine native/portal bytes"):
                publication.validate(self.approved, pristine)
            page.write_bytes(original)
            publication.validate(self.approved, pristine)

    def test_installed_implementation_and_asset_reauthenticated(self):
        import ada_gnatdoc_enhancer
        installed = Path(ada_gnatdoc_enhancer.__file__).parent
        package = self.base / "enhancer"
        shutil.copytree(installed, package, ignore=shutil.ignore_patterns("__pycache__"))
        with mock.patch.object(ada_gnatdoc_enhancer, "__file__", str(package / "__init__.py")):
            publication.tool()
            for path in (package / "pipeline.py", package / "assets/gnatdoc-navigation.js"):
                original = path.read_bytes()
                for suffix in (b"\n# mutation\n", b"\n# second mutation\n"):
                    path.write_bytes(original + suffix)
                    with self.assertRaisesRegex(ValueError, "implementation/assets mismatch"):
                        publication.validate(self.approved, self.pristine)
                    path.write_bytes(original)
                    publication.tool()


if __name__ == "__main__":
    unittest.main(verbosity=2)
