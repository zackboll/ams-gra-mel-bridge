"""DOC-012 source/native reconstruction and isolated catalog mutation controls."""

import copy
import json
import re
import shutil
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "docs/tools"))
import api_catalog as catalog
import build_enhanced_site as publication
import check_live_docs_site as live
from ada_examples import References


class CatalogTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.references = References(ROOT, ROOT / "_site")
        cls.expected = catalog.inventory(ROOT, cls.references, independent=True)
        cls.text = (ROOT / "_site/guides/api-map.html").read_text()
        cls.blocks = re.findall(r'<li data-catalog-entry=.*?</li>', cls.text)

    def test_complete_source_owned_inventory(self):
        rows = self.expected["entries"]
        self.assertEqual({s: sum(r["scope"] == s for r in rows) for s in catalog.SCOPES},
                         {"stable": 238, "additional": 9, "operation": 81})
        self.assertEqual(sum(r["kind"] == "subtype" for r in rows if r["scope"] == "stable"), 7)
        self.assertEqual(sum(len(r["examples"]) for r in rows), 196)
        self.assertEqual(sum(len(r["examples"]) for r in rows if r["scope"] == "operation"), 111)
        self.assertEqual(catalog.link_count(self.expected), 524)
        self.assertEqual(len(self.blocks), len(rows))

    def test_independent_reconstruction_equals_ledger(self):
        self.assertEqual(catalog.inventory(ROOT, self.references), self.expected)
        with mock.patch.object(catalog.examples, "occurrence_ledger", side_effect=AssertionError("untrusted ledger")):
            self.assertEqual(catalog.verify(self.text, ROOT, self.references), self.expected)

    def test_excluded_owners_do_not_gain_stable_identity(self):
        excluded = {"AMS.MEL.IR.C2", "AMS.MEL.IR.Channel", "AMS.MEL.IR.Health_Status.Metadata", "AMS.MEL.Status"}
        self.assertFalse(excluded & {r["owner"] for r in self.expected["entries"] if r["scope"] == "stable"})
        self.assertTrue(all(r["owner"] in excluded for r in self.expected["entries"] if r["scope"] == "additional"))
        self.assertNotIn('?symbol=', self.text)

    def test_support_sources_follow_actual_containing_example(self):
        proofs = self.expected["occurrences"]
        manifest = catalog.examples.inventory(ROOT)
        listing_owners = {(e["id"], n) for e in manifest for n in [e["main"], *e["support"]]}
        self.assertTrue(all((p["example"], p["source"]) in listing_owners for p in proofs))
        shared = {p["source"] for p in proofs if sum(p["source"] in e["support"] for e in manifest) > 1}
        self.assertTrue(shared)
        self.assertTrue(any(len({p["example"] for p in proofs if p["source"] == n}) > 1 for n in shared))

    def test_every_native_destination_matches_public_section_and_ledger(self):
        for row in self.expected["entries"]:
            key = row["page"], row["anchor"]
            self.assertEqual(self.references.native.sections[key]["identity"], row["ledger_identity"])
            ledger = self.references.native.ledger[row["ledger_identity"]]
            self.assertEqual(key, (ledger["page"], ledger["anchor"]))

    def test_ambiguous_overload_not_admitted(self):
        refs = copy.copy(self.references)
        refs.declarations = copy.deepcopy(refs.declarations)
        identity = next(p["identity"] for p in self.expected["occurrences"] if p["context"] == "unique-operation")
        refs.declarations[identity] *= 2
        reduced = catalog.inventory(ROOT, refs, independent=True)
        self.assertFalse(any(r["canonical"].lower() == identity for r in reduced["entries"]))
        with self.assertRaises(ValueError):
            catalog.verify(self.text, ROOT, refs)

    def test_changed_source_and_support_identity(self):
        for relative in ("src/provider_session.adb", "src/example_support.ads", "manifest.json"):
            with self.subTest(relative=relative), tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                shutil.copytree(ROOT / "docs/examples", root / "docs/examples",
                                ignore=shutil.ignore_patterns("obj", "bin"))
                path = root / "docs/examples" / relative
                self.assertTrue(path.is_file())
                original = path.read_bytes()
                try:
                    path.write_bytes(original + b"\n")
                    with self.assertRaisesRegex(ValueError, "example/support/manifest identity"):
                        catalog.example_identity(root, catalog.examples.inventory(ROOT))
                finally:
                    path.write_bytes(original)
                self.assertEqual(path.read_bytes(), original)

    def test_rejected_before_http_even_with_forged_manifest(self):
        with tempfile.TemporaryDirectory() as temp:
            site = Path(temp) / "site"
            shutil.copytree(ROOT / "_site-published", site)
            path = site / "guides/api-map.html"
            path.write_text(path.read_text().replace('data-scope="stable"', 'data-scope="additional"', 1))
            manifest = site / "api/presentation.json"
            data = json.loads(manifest.read_text())
            data["counts"]["catalog_symbols"] = 238
            data["output_sha256"]["guides/api-map.html"] = publication.sha(path.read_bytes())
            manifest.write_text(json.dumps(data))
            with mock.patch.object(live, "get_entity", side_effect=AssertionError("HTTP forbidden")) as get:
                with self.assertRaises(ValueError):
                    live.verify(site, ROOT / "_site", "https://owner.github.io/project/", "a" * 40)
                get.assert_not_called()


def mutation(name):
    def test(self):
        first, second = self.blocks[:2]
        href = re.search(r'href="([^"]+)"', first)[1]
        other = re.search(r'href="([^"]+)"', second)[1]
        used = next(b for b in self.blocks if ' Used in: ' in b)
        example_link = re.search(r'<a href="examples.html#[^"]+">[^<]+</a>', used)[0]
        additional = next(b for b in self.blocks if 'data-scope="additional"' in b)
        operation = next(b for b in self.blocks if 'data-scope="operation"' in b)
        changes = {
            "missing": (first, ""),
            "duplicate": (first, first * 2),
            "wrong_existing_destination": (first, first.replace(href, other)),
            "anchor": (first, first.replace(href.split('#')[1], '0' * 64)),
            "forged_identity": (first, first.replace('AMS.MEL.', 'AMS.MEL.Forged_')),
            "private_identity": (first, first.replace('AMS.MEL.', 'AMS.MEL.Internal.')),
            "promoted_excluded": (additional, additional.replace('data-scope="additional"', 'data-scope="stable"')),
            "classification": (first, first.replace('data-kind="type"', 'data-kind="subtype"')),
            "operation_as_type": (operation, operation.replace('data-scope="operation"', 'data-scope="stable"')),
            "unrelated_example": (used, used.replace(example_link, '<a href="examples.html#rf-product-rx">Unrelated</a>')),
            "duplicate_association": (used, used.replace(example_link, example_link * 2)),
            "encoded_traversal": (first, first.replace(href, '%2e%2e/' + href[3:])),
            "double_encoded_traversal": (first, first.replace(href, '%252e%252e/' + href)),
            "traversal": (first, first.replace(href, '../guides/../' + href)),
            "malformed_url": (first, first.replace(href, '//evil.invalid/x')),
            "external": (first, first.replace(href, 'https://evil.invalid/')),
            "script": (first, first.replace('</li>', '<script>alert(1)</script></li>')),
            "event_handler": (first, first.replace('<a href=', '<a onclick="alert(1)" href=')),
            "resource": (first, first.replace('</li>', '<img src="https://evil.invalid/x"></li>')),
            "html": (first, first.replace('</li>', '<strong>injected</strong></li>')),
            "duplicate_id": (first, first.replace('<code>', '<code id="main">', 1)),
            "duplicate_attribute": (first, first.replace('data-kind=', 'data-scope="stable" data-kind=')),
            "encoded_identity": (first, first.replace('AMS.MEL.', 'AMS&#46;MEL.')),
            "forged_count": (first, first.replace('No verified example use.', '238 symbols verified.')),
            "nondeterministic_order": (first + second, second + first),
            "hidden_markup": (first, first.replace('</li>', '<code hidden>extra</code></li>')),
        }
        old, new = changes[name]
        self.assertIn(old, self.text)
        changed = self.text.replace(old, new, 1)
        self.assertNotEqual(changed, self.text, "mutation did not occur")
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "api-map.html"
            path.write_text(self.text)
            try:
                path.write_text(changed)
                with self.assertRaises(ValueError):
                    catalog.verify(path.read_text(), ROOT, self.references)
            finally:
                path.write_text(self.text)
            self.assertEqual(catalog.verify(path.read_text(), ROOT, self.references), self.expected)
    return test


for name in ("missing", "duplicate", "wrong_existing_destination", "anchor", "forged_identity",
             "private_identity", "promoted_excluded", "classification", "operation_as_type",
             "unrelated_example", "duplicate_association", "encoded_traversal", "double_encoded_traversal",
             "traversal", "malformed_url", "external", "script", "event_handler", "resource", "html",
             "duplicate_id", "duplicate_attribute", "encoded_identity", "forged_count",
             "nondeterministic_order", "hidden_markup"):
    setattr(CatalogTests, "test_mutation_" + name, mutation(name))
