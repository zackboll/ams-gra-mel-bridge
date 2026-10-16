"""Deterministic portal and fail-closed static publication mutation tests."""

import json
import shutil
import tempfile
import unittest
from pathlib import Path

from build_docs_portal import ROOT, PortalError, build, group, manifest, package_map
from validate_docs import Page, SiteError, validate


class PortalTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "source"
        self.site = Path(self.temp.name) / "site"
        shutil.copytree(ROOT / "docs", self.root / "docs")
        shutil.copytree(ROOT / "ada/src", self.root / "ada/src")
        shutil.copytree(ROOT / "_site", self.site)

    def snapshot(self):
        return {p.relative_to(self.site).as_posix(): p.read_bytes()
                for p in self.site.rglob("*") if p.is_file()}

    def test_deterministic_native_untouched(self):
        before = self.snapshot()
        packages = build(self.site, self.root)
        self.assertEqual(before, self.snapshot())
        build(self.site, self.root)
        self.assertEqual(before, self.snapshot())
        self.assertEqual(len(packages), 29)
        self.assertEqual(set(map(group, packages)), {"Shared/Core", "IR", "RF"})
        self.assertEqual(len(set(packages.values())), 29)
        self.assertEqual(validate(self.site)["html_pages"], 37)
        self.assertEqual(len(before), 39)
        for name in ("index.html", "guides/api-map.html"):
            text = (self.site / name).read_text()
            self.assertEqual(len(Page(text).package_entries), 29)
            for category in ("Shared/Core", "IR", "RF"):
                self.assertIn(f"<h3>{category}</h3>", text)
        for entry in manifest(self.root)["guides"]:
            self.assertIn(entry["path"], before)

    def test_manifest_mutations(self):
        path = self.root / "docs/site-map.json"
        original = path.read_text()
        for change in (lambda x: x["guides"].pop(),
                       lambda x: x["guides"].append(x["guides"][0]),
                       lambda x: x["guides"][0].update(id="unknown"),
                       lambda x: x["guides"][0].update(path="../outside.html"),
                       lambda x: x.update(stylesheet="assets/other.css")):
            data = json.loads(original)
            change(data)
            path.write_text(json.dumps(data))
            with self.assertRaises(PortalError):
                build(self.site, self.root)
        path.write_text(original.replace('"schema_version": 1,',
                                         '"schema_version": 1, "schema_version": 1,'))
        with self.assertRaises(PortalError):
            manifest(self.root)

    def test_missing_and_symlink_sources(self):
        path = self.root / "docs/guides/architecture.html"
        path.unlink()
        with self.assertRaises(PortalError):
            build(self.site, self.root)
        path.symlink_to(ROOT / "docs/guides/architecture.html")
        with self.assertRaises(PortalError):
            build(self.site, self.root)

    def test_unknown_and_unresolved_placeholders(self):
        path = self.root / "docs/index.html"
        original = path.read_text()
        for token in ("{{package:Unknown.Package}}", "{{unknown}}", "{{package:bad-name}}"):
            path.write_text(original + token)
            with self.assertRaises(PortalError):
                build(self.site, self.root)

    def test_missing_duplicate_swapped_packages(self):
        packages = package_map(self.site)
        first, second = list(packages)[:2]
        path = self.site / packages[first]
        original = path.read_text()
        path.write_text(original.replace(f"<h1>{first}</h1>", f"<h1>{second}</h1>"))
        with self.assertRaises(PortalError):
            package_map(self.site)
        other = self.site / packages[second]
        other.write_text(other.read_text().replace(f"<h1>{second}</h1>", f"<h1>{first}</h1>"))
        with self.assertRaises(PortalError):
            package_map(self.site)
        path.unlink()
        with self.assertRaises(PortalError):
            package_map(self.site)

    def test_security_negative_controls(self):
        path = self.site / "guides/architecture.html"
        original = path.read_text()
        for mutation in ('<script>x()</script>', '<p onclick="x()">bad</p>',
                         '<p style="color:red">bad</p>', '<a href="absent.html">bad</a>',
                         '<a href="../index.html#absent">bad</a>',
                         '<a href="../../outside.html">bad</a>',
                         '<a href="https://example.org">bad</a>',
                         '<a href="/etc/passwd">bad</a>', '<p>/home/person/private</p>',
                         '<p id="duplicate"></p><p id="duplicate"></p>',
                         '{{package:AMS.MEL}}', '<img srcset="https://example.org/x 1x">'):
            with self.subTest(mutation=mutation):
                path.write_text(original + mutation)
                with self.assertRaises(SiteError):
                    validate(self.site)
        path.write_text(original)

    def test_css_resources(self):
        path = self.site / "assets/portal.css"
        original = path.read_text()
        for mutation in ('@import "https://example.org/x";',
                         '@im\\70ort "https://example.org/x";',
                         'body { background: u/**/rl(https://example.org/x); }',
                         'body { background: url(https://example.org/x); }'):
            path.write_text(original + mutation)
            with self.assertRaises(SiteError):
                validate(self.site)
        path.unlink()
        with self.assertRaises(SiteError):
            validate(self.site)

    def test_extra_file_and_symlink(self):
        path = self.site / "extra.txt"
        path.write_text("unapproved\n")
        with self.assertRaises(SiteError):
            validate(self.site)
        path.unlink()
        path.symlink_to(self.site / "index.html")
        with self.assertRaises(SiteError):
            validate(self.site)

    def test_missing_guide(self):
        (self.site / "guides/architecture.html").unlink()
        with self.assertRaises(SiteError):
            validate(self.site)

    def test_portal_package_destination_swap(self):
        packages = list(package_map(self.site).values())
        path = self.site / "index.html"
        text = path.read_text().replace(packages[0], "TEMPDEST")
        text = text.replace(packages[1], packages[0]).replace("TEMPDEST", packages[1])
        path.write_text(text)
        with self.assertRaises(SiteError):
            validate(self.site)

    def test_navigation_missing_or_duplicate(self):
        path = self.site / "index.html"
        original = path.read_text()
        for text in (original.replace('href="guides/architecture.html"', 'href="index.html"'),
                     original + '<a href="guides/architecture.html">Duplicate</a>'):
            path.write_text(text)
            with self.assertRaises(SiteError):
                validate(self.site)

    def test_native_frame_integrity(self):
        path = self.site / "api/index.html"
        path.write_text(path.read_text().replace('name=document-content', 'name=wrong-frame'))
        with self.assertRaises(SiteError):
            validate(self.site)

    def test_duplicate_and_removed_package_entries(self):
        path = self.site / "index.html"
        original = path.read_text()
        entry = Page(original).package_entries[0]
        marker = 'data-package-entry="' + entry[0] + '"'
        for text in (original.replace(marker, 'data-removed="entry"', 1),
                     original + f'<a {marker} href="{entry[1]}">Duplicate</a>'):
            path.write_text(text)
            with self.assertRaises(SiteError):
                validate(self.site)


if __name__ == "__main__":
    unittest.main()
