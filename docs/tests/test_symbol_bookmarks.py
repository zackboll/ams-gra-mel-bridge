"""Consumer-owned source/native symbol proof and negative publication controls."""

import copy
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "docs/tools"))
import build_enhanced_site as p
import check_live_docs_site as live


class SymbolTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.pristine = ROOT / "_site"
        cls.authority = p.Authority(ROOT, cls.pristine)
        cls.symbols = p.symbol_audit.inventory(cls.authority, cls.pristine)
        cls.workspace = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.workspace.cleanup)
        cls.approved = Path(cls.workspace.name) / "approved"
        p.build(cls.pristine, cls.approved)
        cls.metadata = p.symbol_audit.metadata(cls.symbols)
        cls.index = (cls.approved / "api/index.html").read_text()
        cls.session = next(e for e in cls.symbols if e["canonical"] == "AMS.MEL.Session")
        cls.version = next(e for e in cls.symbols if e["canonical"] == "AMS.MEL.Version")

    def test_complete_bounded_public_inventory(self):
        self.assertEqual(len(self.symbols), 238)
        self.assertEqual(sum(e["kind"] == "type" for e in self.symbols), 231)
        self.assertEqual(sum(e["kind"] == "subtype" for e in self.symbols), 7)
        self.assertEqual(len({e["owner"] for e in self.symbols}), 24)
        self.assertEqual(len({(e["page"], e["anchor"]) for e in self.symbols}), 238)

    def test_session_and_version_are_genuine_public_profiles(self):
        self.assertIn("limited private", self.session["declaration"])
        self.assertIn("record", self.version["declaration"])
        for entry in (self.session, self.version):
            self.assertEqual(entry["source"], "ada/src/ams-mel.ads")
            self.assertEqual(entry["owner"], "AMS.MEL")
            self.assertIn((entry["page"], entry["anchor"]), self.authority.sections)

    def test_nested_private_and_synthetic_owners_not_promoted(self):
        excluded = {"AMS.MEL.IR.C2", "AMS.MEL.IR.Channel",
                    "AMS.MEL.IR.Health_Status.Metadata", "AMS.MEL.Status"}
        self.assertFalse(excluded & {e["owner"] for e in self.symbols})
        self.assertTrue(all("internal" not in e["owner"].lower().split(".") for e in self.symbols))
        self.assertTrue(all(e["visibility"] == "public" for e in self.symbols))

    def test_source_ordered_metadata_exactly_restores_native_index(self):
        restored = p.restore_navigation(self.index, self.symbols)
        restored = p.restore_document(restored, (self.pristine / "api/index.html").read_text(),
                                      "api/index.html", self.authority)
        self.assertEqual(restored.encode(), (self.pristine / "api/index.html").read_bytes())

    def test_native_profile_mutation_rejected_without_manifest(self):
        authority = copy.deepcopy(self.authority)
        authority.types["ams.mel.version"]["native"] = "type Version is null record;"
        with self.assertRaisesRegex(ValueError, "native profile"):
            p.symbol_audit.inventory(authority, self.pristine)

    def test_wrong_existing_owner_rejected_without_manifest(self):
        authority = copy.deepcopy(self.authority)
        authority.page_packages[self.session["page"]] = "AMS.MEL.RF"
        with self.assertRaisesRegex(ValueError, "native owner"):
            p.symbol_audit.inventory(authority, self.pristine)

    def test_default_requires_source_and_native_authority(self):
        authority = copy.deepcopy(self.authority)
        authority.packages["ams.mel"]["name"] = "AMS.MEL.Forged"
        with self.assertRaises(ValueError):
            p.symbol_audit.inventory(authority, self.pristine)

    def test_changed_native_anchor_rejected_without_manifest(self):
        authority = copy.deepcopy(self.authority)
        authority.types["ams.mel.session"]["anchor"] = self.version["anchor"]
        with self.assertRaisesRegex(ValueError, "anchor ledger"):
            p.symbol_audit.inventory(authority, self.pristine)

    def test_misplaced_metadata_rejected_without_fingerprint(self):
        text = self.index.replace(self.metadata, "", 1) + self.metadata
        with self.assertRaisesRegex(ValueError, "symbol metadata"):
            p.restore_navigation(text, self.symbols)

    def test_altered_enum_literal_rejected_without_manifest(self):
        with tempfile.TemporaryDirectory() as temp:
            native = Path(temp) / "native"
            shutil.copytree(self.pristine, native)
            entry = next(e for e in self.symbols if "…" in e["native_profile"])
            path = native / "api" / entry["page"]
            text = path.read_text()
            start = text.index("<h4 id=" + entry["anchor"] + ">")
            before, after = text[:start], text[start:]
            path.write_text(before + after.replace("<dt>", "<dt>Forged_", 1))
            with self.assertRaisesRegex(ValueError, "enumeration literals"):
                p.symbol_audit.inventory(self.authority, native)

    def test_symbol_reference_delta_is_independently_derived(self):
        raw = p.files(self.pristine)
        actual = p.files(self.approved)
        links = sum(len(p.Page(d.decode()).links) for n, d in actual.items() if n.endswith(".html"))
        base = sum(len(p.Page(d.decode()).links) for n, d in raw.items() if n.endswith(".html"))
        self.assertEqual(links - base, 30 + 1428 + 1 + len(self.symbols))

    def test_mutated_artifact_rejected_before_http(self):
        with tempfile.TemporaryDirectory() as temp:
            site = Path(temp) / "site"
            shutil.copytree(self.approved, site)
            path = site / "api/index.html"
            path.write_text(self.index.replace('data-symbol="AMS.MEL.Version"',
                                               'data-symbol="AMS.MEL.Forged"', 1))
            with mock.patch.object(live, "get_entity", side_effect=AssertionError("HTTP forbidden")) as get:
                with self.assertRaisesRegex(ValueError, "symbol metadata"):
                    live.verify(site, self.pristine, "https://owner.github.io/project/", "a" * 40)
                get.assert_not_called()

    def test_altered_router_rejected_before_http(self):
        with tempfile.TemporaryDirectory() as temp:
            site = Path(temp) / "site"
            shutil.copytree(self.approved, site)
            path = site / "api/gnatdoc-navigation.js"
            path.write_bytes(path.read_bytes() + b"\n/* altered */\n")
            with mock.patch.object(live, "get_entity", side_effect=AssertionError("HTTP forbidden")) as get:
                with self.assertRaisesRegex(ValueError, "JavaScript asset identity"):
                    live.verify(site, self.pristine, "https://owner.github.io/project/", "a" * 40)
                get.assert_not_called()


def metadata_mutation(name):
    def test(self):
        first = self.symbols[0]
        target = self.symbols[1]
        href = first["page"] + "#" + first["anchor"]
        replacement = {
            "missing": "",
            "duplicate": self.metadata * 2,
            "forged_identity": self.metadata.replace(first["canonical"], "AMS.MEL.Forged", 1),
            "wrong_existing_destination": self.metadata.replace(href, target["page"] + "#" + target["anchor"], 1),
            "changed_anchor": self.metadata.replace(first["anchor"], "0" * 64, 1),
            "wrong_default": self.metadata.replace('data-default-package="AMS.MEL"',
                                                    'data-default-package="AMS.MEL.RF"'),
            "hidden_extra_content": self.metadata.replace("</div>", "<p hidden>unauthorized</p></div>"),
            "duplicate_attribute": self.metadata.replace('data-symbol="', 'hidden data-symbol="', 1),
            "reordered": self.metadata.replace(href, "TEMP", 1).replace(
                target["page"] + "#" + target["anchor"], href, 1).replace("TEMP", target["page"] + "#" + target["anchor"], 1),
            "encoded_identity": self.metadata.replace("AMS.MEL", "AMS&#46;MEL", 1),
        }[name]
        mutated = self.index.replace(self.metadata, replacement, 1)
        self.assertNotEqual(mutated, self.index)
        # Reach the independent guard without any fingerprint/manifest gate.
        with self.assertRaisesRegex(ValueError, "symbol metadata"):
            p.restore_navigation(mutated, self.symbols)
        # A fully self-consistent forged output map still cannot authorize it.
        with tempfile.TemporaryDirectory() as temp:
            site = Path(temp) / "site"
            shutil.copytree(self.approved, site)
            (site / "api/index.html").write_text(mutated)
            path = site / "api/presentation.json"
            manifest = json.loads(path.read_text())
            manifest["output_sha256"]["api/index.html"] = p.sha(mutated.encode())
            path.write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, "symbol metadata"):
                p.validate(site, self.pristine)
    return test


for name in ("missing", "duplicate", "forged_identity", "wrong_existing_destination", "changed_anchor",
             "wrong_default", "hidden_extra_content", "duplicate_attribute", "reordered", "encoded_identity"):
    setattr(SymbolTests, "test_metadata_" + name, metadata_mutation(name))
