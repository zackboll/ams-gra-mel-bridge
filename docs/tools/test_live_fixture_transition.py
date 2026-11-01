"""DOC-PERF-001-R2 authentic fixture and exact-source proposal controls."""

import ast
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
sys.path[:0] = [str(ROOT / "docs/tools"), str(ROOT / "docs/tests")]
import build_enhanced_site as p
import documentation_ci as ci
import test_live_docs_site as live_tests
import test_documentation_ci as ci_tests
from local_type_audit import LINK, SECTION


class LiveFixtureTransitionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.authority = p.Authority(ROOT, ROOT / "_site")
        cls.required = sorted(cls.authority.required_additions(), key=p.occurrence_key)
        cls.record = cls.required[0]
        cls.page = cls.record["containing"]["page"]
        cls.old = ci.git_bytes(ci.CI001_SHA, "docs/tests/test_live_docs_site.py")
        cls.current = (ROOT / "docs/tests/test_live_docs_site.py").read_bytes()
        cls.workspace = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.workspace.cleanup)
        cls.approved = Path(cls.workspace.name) / "approved"
        p.build(ROOT / "_site", cls.approved)

    def setUp(self):
        self.workspace = tempfile.TemporaryDirectory()
        self.addCleanup(self.workspace.cleanup)
        self.site = Path(self.workspace.name) / "site"
        shutil.copytree(self.approved, self.site)
        self.case = live_tests.QualifiedArtifactTests("test_wrong_native_anchor")
        self.case.site = self.site

    def original_helper(self):
        cls = next(n for n in ast.parse(self.old).body
                   if isinstance(n, ast.ClassDef) and n.name == "QualifiedArtifactTests")
        node = next(n for n in cls.body if isinstance(n, ast.FunctionDef) and n.name == "api_page")
        module = ast.Module(body=[node], type_ignores=[])
        namespace = {}
        exec(compile(module, "immutable-CI001-api_page", "exec"), namespace)
        return namespace["api_page"]

    def test_original_zero_counts_reproduced_without_optimizer(self):
        paths = sorted((self.site / "api").glob("*.html"))
        bad = next(path for path in paths if path.name != "index.html" and
                   self.authority.page_packages[path.name] == "AMS")
        for name in ("test_wrong_native_anchor", "test_missing_required_type_link"):
            with self.subTest(test=name):
                with mock.patch.object(Path, "glob", return_value=iter([bad, *paths])), \
                     mock.patch.object(live_tests.QualifiedArtifactTests, "api_page", self.original_helper()):
                    with self.assertRaisesRegex(AssertionError, "0 != 1"):
                        getattr(self.case, name)()

    def test_selection_independent_of_enumeration(self):
        original_glob = Path.glob
        for reverse in (False, True):
            def ordered(path, pattern, **kwargs):
                return iter(sorted(original_glob(path, pattern, **kwargs), reverse=reverse))
            with self.subTest(reverse=reverse), mock.patch.object(Path, "glob", ordered):
                self.assertEqual(self.case.api_page().name, self.page)
        text = self.case.api_page().read_text()
        containing = self.record["containing"]
        self.assertEqual(self.authority.sections[(self.page, containing["anchor"])], containing)
        self.assertEqual(self.authority.ledger[containing["identity"]]["anchor"], containing["anchor"])
        self.assertIn("<h4 id=" + containing["anchor"] + ">", text)
        _, audit = p.restore_page(self.page, text, self.authority)
        self.assertIn(self.record, audit)

    def test_original_mutation_assertions_pass_under_both_orders(self):
        original_glob = Path.glob
        for reverse in (False, True):
            def ordered(path, pattern, **kwargs):
                return iter(sorted(original_glob(path, pattern, **kwargs), reverse=reverse))
            for name in ("test_wrong_native_anchor", "test_missing_required_type_link"):
                with self.subTest(reverse=reverse, test=name):
                    path = self.site / "api" / self.page
                    original = path.read_bytes()
                    try:
                        with mock.patch.object(Path, "glob", ordered):
                            getattr(self.case, name)()
                    finally:
                        path.write_bytes(original)

    def test_forged_manifests_cannot_authorize_required_link_or_anchor(self):
        path = self.case.api_page()
        original = path.read_text()
        record = self.record
        section = next(m for m in SECTION.finditer(original)
                       if m[2] == record["containing"]["anchor"])
        link = next(m[0] for m in LINK.finditer(section[3]) if m[1] == record["href"]
                    and m[2] == record["target_identity"] and m[3] == record["visible"])
        for text, guard in (
                (original.replace(link, record["visible"], 1),
                 "original ordered type-link audit|required type-link occurrence"),
                (original.replace("<h4 id=" + record["containing"]["anchor"] + ">",
                                  "<h4 id=wrong>", 1), "native|declaration|section")):
            with self.subTest(guard=guard):
                manifest = self.site / "api/presentation.json"
                before = manifest.read_bytes()
                path.write_text(text)
                data = json.loads(before)
                data["output_sha256"]["api/" + self.page] = p.sha(text.encode())
                manifest.write_text(json.dumps(data))
                try:
                    with self.assertRaisesRegex(ValueError, guard):
                        live_tests.live.qualified(self.site, ROOT / "_site")
                finally:
                    path.write_text(original)
                    manifest.write_bytes(before)

    def test_missing_or_changed_authority_fails_closed(self):
        with mock.patch.object(p.Authority, "required_additions", return_value=[]):
            with self.assertRaisesRegex(AssertionError, "missing trusted fixture occurrence"):
                self.case.api_page()
        changed = copy.deepcopy(self.required)
        changed[0]["containing"]["anchor"] = "0" * 64
        with mock.patch.object(p.Authority, "required_additions", return_value=changed):
            with self.assertRaises(KeyError):
                self.case.api_page()
        path = self.case.api_page()
        original = path.read_bytes()
        path.unlink()
        with self.assertRaises(FileNotFoundError):
            self.case.api_page()
        path.write_bytes(original)
        other = next(entry for entry in self.authority.sections.values()
                     if entry["anchor"] != self.record["target"]["anchor"])
        path.write_text(path.read_text().replace(self.record["href"],
                                                 other["page"] + "#" + other["anchor"]))
        with self.assertRaisesRegex(ValueError, "wrong existing/native destination"):
            self.case.api_page()

    def test_exact_old_new_reconstruction_and_all_test_bodies(self):
        self.assertEqual(ci.live_fixture_source(self.old), self.old)
        self.assertEqual(ci.live_fixture_source(self.current), self.old)
        self.assertEqual(ci.source_methods(self.old, "test_live_docs_site"),
                         ci.source_methods(self.current, "test_live_docs_site"))
        # Only the api_page function node can differ, including imports/classes/helpers.
        def without_helper(content):
            tree = ast.parse(content)
            for node in tree.body:
                if isinstance(node, ast.ClassDef) and node.name == "QualifiedArtifactTests":
                    node.body = [n for n in node.body if not
                                 (isinstance(n, ast.FunctionDef) and n.name == "api_page")]
            return ast.dump(tree)
        self.assertEqual(without_helper(self.old), without_helper(self.current))

    def test_policy_rejects_assertions_other_helpers_and_alternate_replacements(self):
        cases = [self.current.replace(b"self.assertEqual(count, 1)", b"self.assertEqual(count, 0)", 1),
                 self.current.replace(b"self.assertRaisesRegex(ValueError, guard)",
                                      b"self.assertRaisesRegex(Exception, guard)", 1),
                 self.current.replace(b"return path\n", b"return sorted(self.site.glob('api/*.html'))[0]\n", 1),
                 self.current.replace(b"required[0]", b"required[-1]", 1),
                 self.current.replace(b"class WorkflowTests", b"class ReplacedWorkflowTests", 1),
                 self.current + b"\n# unrelated addition\n"]
        for candidate in cases:
            self.assertNotEqual(candidate, self.current)
            with self.assertRaisesRegex(ValueError, "unapproved inherited live fixture source"):
                ci.live_fixture_source(candidate)
        # Duplicated and removed test methods cannot pass exact reconstruction.
        for candidate in (self.current.replace(b"    def test_wrong_native_anchor(self):",
                                               b"    def test_removed_native_anchor(self):", 1),
                          self.current.replace(b"    def api_page(self):",
                                               b"    def api_page(self):\n        return None\n\n    def api_page(self):", 1)):
            with self.assertRaisesRegex(ValueError, "unapproved inherited live fixture source"):
                ci.live_fixture_source(candidate)

    def test_historical_fixture_and_current_inventory_unchanged(self):
        inv = ci.inventory()
        self.assertEqual({g: len(ids) for g, ids in inv["groups"].items()},
                         {"scripts": 142, "enhanced": 508})
        self.assertEqual(len(inv["shards"]["live"]), 72)
        inherited = json.loads(ci.git_bytes(ci.CI001_SHA, "docs/tests/ci001-inherited-inventory.json"))
        self.assertTrue(all(set(ids) <= set(inv["groups"][g]) for g, ids in inherited.items()))
        self.assertEqual(sum(n.startswith("test_bootstrap_ci_policy.")
                             for n in inv["groups"]["enhanced"]), 71)
        fixture = live_tests.QualifiedArtifactTests
        self.assertEqual(len(ci.source_methods(self.current, "test_live_docs_site")),
                         len(ci.source_methods(self.old, "test_live_docs_site")))
        self.assertTrue(hasattr(fixture, "test_wrong_native_anchor"))

    def test_full_inventory_policy_keeps_historical_fixture_and_rejects_extra_edits(self):
        case = ci_tests.ReviewedTransitionTests("test_original_497_remain_admitted")
        case.setUpClass()
        try:
            case.setUp()
            case.validate()  # Existing immutable historical fixture remains admitted.
            path = case.root / "docs/tests/test_live_docs_site.py"
            path.write_bytes(self.current)
            case.validate()  # Only the reconstructed helper successor is also admitted.
            for candidate in (
                    self.current.replace(b"self.assertEqual(count, 1)", b"self.assertEqual(count, 0)", 1),
                    self.current.replace(b"def reject(self, guard):", b"def reject(self, other):", 1),
                    self.current.replace(b"required[0]", b"required[-1]", 1)):
                path.write_bytes(candidate)
                with self.assertRaisesRegex(ValueError, "unapproved inherited live fixture source"):
                    case.validate()
            path.write_bytes(self.old)
            case.validate()
        finally:
            case.doCleanups()
            case.doClassCleanups()


if __name__ == "__main__":
    unittest.main(verbosity=2)
