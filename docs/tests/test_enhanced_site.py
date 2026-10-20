"""Real-site positive/security mutations; no synthetic replacement of MEL input."""

import json
import re
import shutil
import sys
import tempfile
import unittest
from unittest import mock
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "docs/tools"))
import build_enhanced_site as presentation
from ada_gnatdoc_enhancer.config import load
from ada_gnatdoc_enhancer.errors import EnhancementError
from ada_gnatdoc_enhancer.inventory import direct_callables, packages
from ada_gnatdoc_enhancer.native_html import snapshot
from ada_gnatdoc_enhancer.pipeline import enhance
from ada_gnatdoc_enhancer.profiles import reconcile


class PublishedTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workspace = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.workspace.cleanup)
        cls.original = ROOT / "_site"
        cls.approved = Path(cls.workspace.name) / "approved"
        presentation.build(cls.original, cls.approved)
        cls.anchors = json.loads((ROOT / "docs/gnatdoc-native-anchors.json").read_text())
        cls.entry = next(iter(cls.anchors.values()))

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.site = Path(self.temp.name) / "site"
        shutil.copytree(self.approved, self.site)

    def reject(self):
        with self.assertRaises(ValueError):
            presentation.validate(self.site, self.original)

    def page(self):
        return self.site / "api" / self.entry["page"]

    def mutate(self, path, old, new):
        text = path.read_text()
        self.assertIn(old, text)
        path.write_text(text.replace(old, new, 1))

    def test_exact_inventory_and_portal_preservation(self):
        result = presentation.validate(self.site, self.original)
        self.assertEqual((result["files"], result["html_pages"], result["links"]), (43, 38, 3525))
        self.assertEqual(result["linked_local_types"], 1428)
        self.assertEqual(set(presentation.files(self.site)) - set(presentation.files(self.original)), presentation.EXTRAS)
        for name, data in presentation.files(self.original).items():
            if not name.startswith("api/"):
                self.assertEqual((self.site / name).read_bytes(), data)

    def test_fresh_determinism_and_idempotence(self):
        second = Path(self.temp.name) / "second"
        presentation.build(self.original, second)
        self.assertEqual(presentation.files(self.site), presentation.files(second))
        api_only = Path(self.temp.name) / "api-only"
        shutil.copytree(self.site / "api", api_only / "api")
        before = presentation.files(api_only)
        enhance(load(ROOT, "docs/gnatdoc-enhance.toml"), api_only)
        self.assertEqual(before, presentation.files(api_only))

    def test_missing_theme(self):
        (self.site / "api/gnatdoc-theme.css").unlink()
        self.reject()

    def test_altered_theme(self):
        path = self.site / "api/gnatdoc-theme.css"
        path.write_bytes(path.read_bytes() + b"\nbody {color:red}\n")
        self.reject()

    def test_removed_viewport(self):
        self.mutate(self.page(), presentation.METADATA.split('<link', 1)[0], "")
        self.reject()

    def test_duplicate_theme(self):
        self.mutate(self.page(), presentation.METADATA, presentation.METADATA * 2)
        self.reject()

    def test_changed_anchor(self):
        self.mutate(self.page(), self.entry["anchor"], "f" * 64)
        self.reject()

    def test_swapped_existing_anchors(self):
        path = self.page()
        ids = re.findall(r"<h4 id=([a-f0-9]{64})>", path.read_text())
        self.assertGreater(len(ids), 1)
        text = path.read_text().replace(ids[0], "SWAP").replace(ids[1], ids[0]).replace("SWAP", ids[1])
        path.write_text(text)
        self.reject()

    def test_wrong_but_existing_declaration_link(self):
        path = self.site / "api/index.html"
        links = re.findall(r"href=([a-f0-9]{64}\.html#[a-f0-9]{64})", path.read_text())
        self.assertGreater(len(links), 1)
        self.mutate(path, links[0], links[1])
        self.reject()

    def test_text_changed_inside_highlighting(self):
        self.mutate(self.page(), 'class="ada-keyword">function</span>', 'class="ada-keyword">procedure</span>')
        self.reject()

    def test_nested_spans(self):
        self.mutate(self.page(), 'class="ada-keyword">function</span>',
                    'class="ada-keyword"><span class="ada-keyword">function</span></span>')
        self.reject()

    def test_extra_flat_span_outside_declarations(self):
        self.mutate(self.page(), "<h1>AMS.MEL.IR.C2.Common</h1>",
                    '<h1><span class="ada-keyword">AMS.MEL.IR.C2.Common</span></h1>')
        self.reject()

    def test_script(self):
        with self.page().open("a") as stream:
            stream.write('<script>alert(1)</script>')
        self.reject()

    def test_event_handler(self):
        self.mutate(self.page(), "<h1>", '<h1 onclick="alert(1)">')
        self.reject()

    def test_unexpected_file(self):
        (self.site / "api/unapproved.js").write_text("alert(1)\n")
        self.reject()

    def test_deleted_package(self):
        self.page().unlink()
        self.reject()

    def test_swapped_package_destinations(self):
        path = self.site / "api/index.html"
        names = re.findall(r"href=([a-f0-9]{64}\.html) target=document-content", path.read_text())
        self.assertGreater(len(names), 1)
        text = path.read_text().replace(names[0], "SWAP").replace(names[1], names[0]).replace("SWAP", names[1])
        path.write_text(text)
        self.reject()

    def test_altered_manifest(self):
        path = self.site / "api/presentation.json"
        data = json.loads(path.read_text())
        data["counts"]["declaration_snippets"] -= 1
        path.write_text(json.dumps(data))
        self.reject()

    def test_forged_consistent_manifest(self):
        path = self.site / "api/presentation.json"
        data = json.loads(path.read_text())
        name = "api/" + self.entry["page"]
        data["original_api_html"][name] = data["original_api_html"][name].replace("Capabilities", "Forged")
        self.mutate(self.page(), "Capabilities", "Forged")
        data["output_sha256"][name] = presentation.sha(self.page().read_bytes())
        path.write_text(json.dumps(data))
        self.reject()

    def test_modified_raw_css(self):
        path = self.site / "api/gnatdoc.css"
        path.write_bytes(path.read_bytes() + b"\n/* altered */\n")
        self.reject()

    def test_modified_portal_guide(self):
        path = self.site / "guides/architecture.html"
        path.write_bytes(path.read_bytes() + b"\n<p>unexpected</p>\n")
        self.reject()

    def test_source_changed_after_enhancement(self):
        root = Path(self.temp.name) / "source"
        shutil.copytree(ROOT / "ada/src", root / "ada/src")
        path = root / "ada/src/ams-mel-rf-product_rx.ads"
        path.write_text(path.read_text().replace("Convention => C", "Convention => Ada", 1))
        with self.assertRaisesRegex(presentation.PresentationError, "source-profile"):
            presentation.validate(self.site, self.original, root)

    def test_changed_authoritative_profile(self):
        root = Path(self.temp.name) / "source"
        shutil.copytree(ROOT / "ada/src", root / "ada/src")
        shutil.copytree(ROOT / "docs", root / "docs")
        path = root / "docs/gnatdoc-native-anchors.json"
        path.write_text(path.read_text().replace(self.entry["signature"], "forged signature", 1))
        with self.assertRaisesRegex(presentation.PresentationError, "anchors/profiles"):
            presentation.validate(self.site, self.original, root)

    def test_broken_portal_link(self):
        self.mutate(self.site / "index.html", 'href="guides/architecture.html"', 'href="guides/absent.html"')
        self.reject()

    def test_broken_internal_link(self):
        self.mutate(self.page(), 'href=gnatdoc.css', 'href=absent.css')
        self.reject()

    def test_symlink(self):
        path = self.site / "api/gnatdoc-theme.css"
        path.unlink()
        path.symlink_to(self.approved / "api/gnatdoc-theme.css")
        self.reject()

    def test_configuration_cannot_enable_deferred_features(self):
        root = Path(self.temp.name) / "source"
        shutil.copytree(ROOT / "ada/src", root / "ada/src")
        shutil.copytree(ROOT / "docs", root / "docs")
        path = root / "docs/gnatdoc-enhance.toml"
        original = path.read_text()
        for value in (original.replace("type_links = true", "type_links = false"),
                      original.replace('mode = "none"', 'mode = "html"'),
                      original + '\n[external_types]\n"Other.T" = "https://example.org/api.html#t"\n',
                      original + '\n[security]\nallowed_external_hosts = ["example.org"]\n',
                      original + '\nexternal_dependencies = []\n'):
            with self.subTest(configuration=value):
                path.write_text(value)
                with self.assertRaisesRegex(presentation.PresentationError, "configuration"):
                    presentation.validate(self.site, self.original, root)

    def test_build_rejects_existing_destination(self):
        with self.assertRaisesRegex(presentation.PresentationError, "fresh directory"):
            presentation.build(self.original, self.site)

    def test_original_input_tampering(self):
        raw = Path(self.temp.name) / "raw"
        shutil.copytree(self.original, raw)
        with (raw / "api/gnatdoc.css").open("a") as stream:
            stream.write("/* forged baseline */")
        with self.assertRaisesRegex(presentation.PresentationError, "pristine"):
            presentation.validate(self.site, raw)

    def test_unqualified_tool_version(self):
        with mock.patch.object(presentation.importlib.metadata, "version", return_value="0.1.1"):
            with self.assertRaisesRegex(presentation.PresentationError, "version mismatch"):
                presentation.validate(self.site, self.original)

    def test_unqualified_tool_implementation(self):
        with mock.patch.object(presentation, "PACKAGE_SHA256", "0" * 64):
            with self.assertRaisesRegex(presentation.PresentationError, "implementation/assets mismatch"):
                presentation.validate(self.site, self.original)

    def test_dry_run_preserves_source_and_consumer_bytes(self):
        api_only = Path(self.temp.name) / "dry-run"
        shutil.copytree(self.original / "api", api_only / "api")
        source_before = presentation.hashes(presentation.files(ROOT / "ada/src"))
        consumer_before = presentation.hashes(presentation.files(self.original))
        published_before = presentation.hashes(presentation.files(self.site))
        input_before = presentation.hashes(presentation.files(api_only))
        result = enhance(load(ROOT, "docs/gnatdoc-enhance.toml"), api_only, dry_run=True)
        self.assertEqual(result["declaration_snippets"], 1237)
        self.assertEqual(input_before, presentation.hashes(presentation.files(api_only)))
        self.assertEqual(source_before, presentation.hashes(presentation.files(ROOT / "ada/src")))
        self.assertEqual(consumer_before, presentation.hashes(presentation.files(self.original)))
        self.assertEqual(published_before, presentation.hashes(presentation.files(self.site)))


class ProfileTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.config = load(ROOT, "docs/gnatdoc-enhance.toml")
        cls.source = packages(cls.config)
        cls.native = snapshot(cls.config, ROOT / "_site")

    def test_all_profiles_and_native_sections(self):
        self.assertEqual(len(self.source), 29)
        self.assertEqual(sum(len(p.sections) for p in self.native.pages.values()), 1237)
        self.assertEqual(sum(len(p.declarations) for p in self.source.values()), 1203)
        for key, pkg in self.source.items():
            for decl in pkg.declarations:
                matches = [s for s in self.native.pages[key].sections if s.name.lower() == decl.name.lower()
                           and reconcile(decl.signature, s.code, direct_callables(pkg)) is not None]
                self.assertEqual(len(matches), 1, (pkg.name, decl.name))

    def test_both_records_consume_and_project_only_approved_aspect(self):
        pkg = self.source["ams.mel.rf.product_rx"]
        for name in ("Complex_I16", "Complex_I8"):
            decl = next(d for d in pkg.declarations if d.name == name)
            section = next(s for s in self.native.pages[pkg.name.lower()].sections if s.name == name)
            self.assertTrue(decl.signature.endswith("with Convention => C;"))
            self.assertEqual(reconcile(decl.signature, section.code).rule, "record-convention-aspect")
            self.assertIsNone(reconcile(decl.signature.replace("Convention => C", "Convention => Ada"), section.code))
            self.assertIsNone(reconcile(decl.signature.replace("Imag", "Changed_Field"), section.code))

    def test_renaming_requires_unique_real_source_target(self):
        pkg = self.source["ams.mel.rf.c2"]
        decl = next(d for d in pkg.declarations if d.name == "Add_Job_Intervals")
        section = next(s for s in self.native.pages[pkg.name.lower()].sections if s.name == decl.name)
        target = direct_callables(pkg)
        self.assertEqual(reconcile(decl.signature, section.code, target).rule, "subprogram-renaming-profile")
        self.assertIsNone(reconcile(decl.signature, section.code))
        self.assertIsNone(reconcile(decl.signature, section.code, target + target))
        self.assertIsNone(reconcile(decl.signature, section.code.replace("Add_Job_Intervals", "Wrong_Name"), target))


if __name__ == "__main__":
    unittest.main()
