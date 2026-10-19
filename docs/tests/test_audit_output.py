"""Audit destinations must not invalidate a successful real-site verification."""

import json
import os
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "docs/tools"))
import build_enhanced_site as presentation


class AuditOutputTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workspace = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.workspace.cleanup)
        cls.approved = Path(cls.workspace.name) / "approved"
        presentation.build(ROOT / "_site", cls.approved)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.root = self.base / "repository"
        shutil.copytree(ROOT / "ada/src", self.root / "ada/src")
        shutil.copytree(ROOT / "docs", self.root / "docs")
        self.pristine = self.base / "pristine"
        shutil.copytree(ROOT / "_site", self.pristine)
        self.site = self.base / "published"
        shutil.copytree(self.approved, self.site)
        self.before = self.checked_bytes()

    def checked_bytes(self):
        return {name: presentation.files(path) for name, path in
                (("repository", self.root), ("pristine", self.pristine), ("publication", self.site))}

    def reject(self, path, root=None):
        try:
            with self.assertRaisesRegex(presentation.PresentationError, "audit destination"):
                presentation.validate(self.site, self.pristine, root or self.root, audit_path=path)
        finally:
            self.assertTrue(self.before == self.checked_bytes(), "checked tree bytes changed")

    def test_extra_file_inside_publication_rejected(self):
        self.reject(self.site / "type-audit.json")

    def test_existing_publication_html_overwrite_rejected(self):
        self.reject(self.site / "api/index.html")

    def test_pristine_destination_rejected(self):
        self.reject(self.pristine / "type-audit.json")

    def test_repository_destination_rejected(self):
        self.reject(self.root / "docs/type-audit.json")

    def test_parent_symlink_aliases_into_checked_trees_rejected(self):
        for name, target in (("repo", self.root), ("raw", self.pristine), ("site", self.site)):
            with self.subTest(tree=name):
                alias = self.base / (name + "-alias")
                alias.symlink_to(target, target_is_directory=True)
                self.reject(alias / "type-audit.json")

    def test_dotdot_and_repository_root_alias_rejected(self):
        alias = self.base / "root-alias"
        alias.symlink_to(self.root, target_is_directory=True)
        self.reject(self.root / "docs/../type-audit.json", root=alias)

    def test_existing_external_hardlink_rejected(self):
        destination = self.base / "hardlink.json"
        os.link(self.site / "api/index.html", destination)
        before = destination.read_bytes()
        self.reject(destination)
        self.assertEqual(before, destination.read_bytes())

    def test_existing_external_file_rejected(self):
        destination = self.base / "existing.json"
        destination.write_bytes(b"do not overwrite\n")
        self.reject(destination)
        self.assertEqual(destination.read_bytes(), b"do not overwrite\n")

    def test_file_symlink_and_dangling_symlink_rejected(self):
        for name, target in (("existing", self.site / "api/index.html"),
                             ("dangling", self.base / "not-created.json")):
            with self.subTest(alias=name):
                destination = self.base / (name + "-alias.json")
                destination.symlink_to(target)
                self.reject(destination)
                self.assertTrue(destination.is_symlink())
        self.assertFalse((self.base / "not-created.json").exists())

    def test_missing_external_parent_rejected_without_creation(self):
        destination = self.base / "missing/audit.json"
        self.reject(destination)
        self.assertFalse(destination.parent.exists())

    def successful_output(self, destination, actual):
        report = presentation.validate(self.site, self.pristine, self.root, audit_path=destination)
        audit = json.loads(actual.read_text())
        self.assertEqual(len(audit), 1418)
        self.assertTrue(all(r["containing"]["source"].startswith("ada/src/")
                            and r["href"] == r["target"]["page"] + "#" + r["target"]["anchor"] for r in audit))
        self.assertEqual((report["files"], report["html_pages"], report["links"],
                          report["linked_local_types"], report["linked_external_types"]), (42, 37, 2965, 1418, 0))
        self.assertEqual(report["output_sha256"], presentation.hashes(presentation.files(self.site)))
        self.assertTrue(self.before == self.checked_bytes(), "checked tree bytes changed")

    def test_fresh_external_output_complete_and_report_still_current(self):
        destination = self.base / "audit.json"
        self.successful_output(destination, destination)

    def test_external_parent_alias_uses_resolved_destination(self):
        external = self.base / "external"
        external.mkdir()
        alias = self.base / "external-alias"
        alias.symlink_to(external, target_is_directory=True)
        self.successful_output(alias / "audit.json", external / "audit.json")


if __name__ == "__main__":
    unittest.main()
