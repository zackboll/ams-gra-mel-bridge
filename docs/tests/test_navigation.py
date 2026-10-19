"""Exact package-browser exceptions against authentic MEL publication bytes."""

import json
import re
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "docs/tools"))
import build_enhanced_site as presentation


class NavigationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workspace = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.workspace.cleanup)
        cls.original = ROOT / "_site"
        cls.approved = Path(cls.workspace.name) / "approved"
        presentation.build(cls.original, cls.approved)
        # Every mutation starts with this independently accepted real input.
        cls.report = presentation.validate(cls.approved, cls.original)
        cls.native = (cls.original / "api/index.html").read_text()
        cls.packages = re.findall(r'<a href=([a-f0-9]{64}\.html) target=document-content>([^<]+)</a>',
                                  cls.native.split('<h2 id=tagged-types>')[0])
        cls.tagged = re.findall(r'<a href=([a-f0-9]{64}\.html#[a-f0-9]{64}) target=document-content>([^<]+)</a>',
                                cls.native.split('<h2 id=tagged-types>')[1])

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.site = self.base / "publication"
        shutil.copytree(self.approved, self.site)
        self.index = self.site / "api/index.html"

    def reject(self, reason):
        with self.assertRaisesRegex(presentation.PresentationError, reason):
            presentation.validate(self.site, self.original)

    def replace(self, old, new, path=None):
        path = path or self.index
        text = path.read_text()
        self.assertIn(old, text)
        path.write_text(text.replace(old, new, 1))

    def forge_manifest(self):
        path = self.site / "api/presentation.json"
        data = json.loads(path.read_text())
        data["output_sha256"] = presentation.hashes({n: b for n, b in presentation.files(self.site).items()
                                                   if n.startswith("api/") and n != "api/presentation.json"})
        path.write_text(json.dumps(data, sort_keys=True, indent=2) + "\n")

    def test_exact_insertions_restore_complete_index(self):
        enhanced = self.index.read_text()
        restored = presentation.restore_navigation(enhanced).replace(presentation.METADATA, "", 1)
        self.assertEqual(restored.encode(), (self.original / "api/index.html").read_bytes())
        self.assertEqual((len(self.packages), len(self.tagged)), (29, 8))
        self.assertEqual(self.report["files"], 42)
        self.assertEqual(self.report["links"], 2965)
        self.assertEqual(self.report["output_sha256"], presentation.hashes(presentation.files(self.site)))

    def test_all_authored_and_native_non_html_bytes_preserved(self):
        for name, data in presentation.files(self.original).items():
            if not (name.startswith("api/") and name.endswith(".html")):
                self.assertEqual((self.site / name).read_bytes(), data, name)

    def test_script_asset_missing(self):
        (self.site / "api/gnatdoc-navigation.js").unlink()
        self.reject("missing/unexpected publication files")

    def test_script_asset_modified(self):
        path = self.site / "api/gnatdoc-navigation.js"
        path.write_bytes(path.read_bytes() + b"\n// changed\n")
        self.forge_manifest()
        self.reject("JavaScript asset identity")

    def test_script_asset_substituted(self):
        (self.site / "api/gnatdoc-navigation.js").write_text("/* no behavior */\n")
        self.forge_manifest()
        self.reject("JavaScript asset identity")

    def test_script_asset_symlink(self):
        path = self.site / "api/gnatdoc-navigation.js"
        external = self.base / "exact.js"
        external.write_bytes(path.read_bytes())
        path.unlink()
        path.symlink_to(external)
        self.reject("symlink input/output member")

    def test_wrong_existing_package_destination_with_forged_manifest(self):
        self.replace('href=' + self.packages[0][0], 'href=' + self.packages[1][0])
        self.forge_manifest()
        self.reject("native declaration/anchor/link/markup changed: api/index.html")

    def test_wrong_existing_tagged_fragment_with_forged_manifest(self):
        # These two actual tagged declarations share their owning RF C2 page.
        first = next(h for h, n in self.tagged if n == "AMS.MEL.RF.C2.Job_Request")
        second = next(h for h, n in self.tagged if n == "AMS.MEL.RF.C2.Virtual_Aperture")
        self.assertEqual(first.split('#')[0], second.split('#')[0])
        self.replace('href=' + first, 'href=' + second)
        self.forge_manifest()
        self.reject("native declaration/anchor/link/markup changed: api/index.html")

    def test_native_label(self):
        self.replace('>' + self.packages[0][1] + '</a>', '>Changed</a>')
        self.reject("native declaration/anchor/link/markup changed: api/index.html")

    def test_native_group_heading(self):
        self.replace('>Tagged types</h2>', '>Other Types</h2>')
        self.reject("native declaration/anchor/link/markup changed: api/index.html")

    def test_native_order(self):
        first, second = [f'<li><a href={h} target=document-content>{n}</a>'
                         for h, n in self.packages[:2]]
        text = self.index.read_text()
        self.assertIn(first, text)
        self.assertIn(second, text)
        self.index.write_text(text.replace(first, "ORDER_PLACEHOLDER", 1).replace(second, first, 1)
                              .replace("ORDER_PLACEHOLDER", second, 1))
        self.reject("native declaration/anchor/link/markup changed: api/index.html")

    def test_native_iframe_destination(self):
        self.replace('<iframe name=document-content>', '<iframe name=document-content src="' + self.packages[1][0] + '">')
        self.reject("native declaration/anchor/link/markup changed: api/index.html")

    def test_additional_local_resource(self):
        self.replace('<body class=main>', '<link rel="stylesheet" href="gnatdoc.css"><body class=main>')
        # The script is now misplaced, not a generic resource permission.
        self.reject("navigation script")

    def test_extra_script_after_approved_script(self):
        self.replace('</iframe>', '</iframe><script src="gnatdoc-navigation.js" defer></script>')
        self.reject("navigation script")

    def test_inline_executable_script(self):
        self.replace('</iframe>', '</iframe><script>alert(1)</script>')
        self.reject("unsafe HTML")

    def test_event_handler(self):
        self.replace('<body class=main>', '<body class=main onclick="alert(1)">')
        self.reject("navigation script")

    def test_package_script(self):
        path = self.site / "api" / self.packages[0][0]
        path.write_text(path.read_text() + presentation.NAVIGATION_SCRIPT)
        self.reject("unsafe HTML")

    def test_package_controls(self):
        path = self.site / "api" / self.packages[0][0]
        path.write_text(path.read_text() + presentation.NAVIGATION_CONTROL)
        self.reject("native declaration/anchor/link/markup changed")

    def test_portal_script(self):
        path = self.site / "index.html"
        path.write_text(path.read_text() + presentation.NAVIGATION_SCRIPT)
        self.reject("changed native CSS or authored portal")

    def test_portal_controls(self):
        path = self.site / "guides/api-map.html"
        path.write_text(path.read_text() + presentation.NAVIGATION_CONTROL)
        self.reject("changed native CSS or authored portal")

    def test_unapproved_configuration(self):
        root = self.base / "repository"
        shutil.copytree(ROOT / "ada/src", root / "ada/src")
        shutil.copytree(ROOT / "docs", root / "docs")
        path = root / "docs/gnatdoc-enhance.toml"
        original = path.read_text()
        mutations = [original.replace('mode = "packages"', 'mode = "operations"'),
                     original.replace('mode = "packages"', 'mode = "none"'),
                     original.replace('[navigation]', '[navigation]\ncategories = "categories.json"'),
                     original.replace('[guides]\nmode = "none"', '[guides]\nmode = "authored-html"'),
                     original + '\n[external_types]\nmapping = "types.json"\n',
                     original + '\ndependencies = ["foreign"]\n',
                     original + '\nallowed_hosts = ["example.com"]\n',
                     original + '\n[generic_instances]\nqualification = "instances.json"\n']
        for changed in mutations:
            with self.subTest(config=changed):
                path.write_text(changed)
                with self.assertRaisesRegex(presentation.PresentationError, "unapproved presentation configuration"):
                    presentation.validate(self.site, self.original, root)


def insertion_test(insertion, replacement, label):
    def test(self):
        self.replace(insertion, replacement)
        self.forge_manifest()
        self.reject("navigation " + label)
    return test


for label, insertion in (("controls", presentation.NAVIGATION_CONTROL),
                         ("script", presentation.NAVIGATION_SCRIPT)):
    for mutation, replacement in (
            ("missing", ""), ("duplicated", insertion * 2),
            ("altered", insertion.replace('package-search', 'package-find') if label == "controls"
             else insertion.replace('defer', 'async'))):
        setattr(NavigationTests, 'test_' + label + '_' + mutation, insertion_test(insertion, replacement, label))

    def misplaced(self, insertion=insertion, label=label):
        self.replace(insertion, "")
        self.index.write_text(self.index.read_text() + insertion)
        self.forge_manifest()
        self.reject("navigation " + label)
    setattr(NavigationTests, 'test_' + label + '_misplaced', misplaced)

for name, replacement in (
        ("attributes", '<script src="gnatdoc-navigation.js" defer type="module"></script>'),
        ("inline_content", '<script src="gnatdoc-navigation.js" defer>alert(1)</script>'),
        ("remote", '<script src="https://example.com/gnatdoc-navigation.js" defer></script>'),
        ("filename", '<script src="navigation.js" defer></script>'),
        ("location", '<script src="../api/gnatdoc-navigation.js" defer></script>')):
    setattr(NavigationTests, 'test_script_' + name,
            insertion_test(presentation.NAVIGATION_SCRIPT, replacement, "script"))


if __name__ == "__main__":
    unittest.main()
