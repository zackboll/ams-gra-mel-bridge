"""Canonical-source/inventory and executable-runner negative controls."""

import copy
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "docs/tools"))
import example_sources as sources
import run_examples as runner


class ExampleTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        shutil.copytree(ROOT / "docs/examples", self.root / "docs/examples",
                        ignore=shutil.ignore_patterns("obj", "bin"))
        (self.root / "docs/guides").mkdir(parents=True)
        shutil.copyfile(ROOT / "docs/guides/examples.html", self.root / "docs/guides/examples.html")
        self.path = self.root / "docs/examples/manifest.json"
        self.data = json.loads(self.path.read_text())

    def reject(self, mutate):
        data = copy.deepcopy(self.data)
        mutate(data)
        self.path.write_text(json.dumps(data))
        with self.assertRaises((ValueError, KeyError)):
            sources.inventory(self.root)

    def test_complete_inventory_and_rendered_source(self):
        entries = sources.inventory(self.root)
        self.assertEqual(len(entries), 9)
        self.assertEqual(sum(len(e["cases"]) for e in entries), 13)
        rendered = sources.render((self.root / "docs/guides/examples.html").read_text(), self.root)
        self.assertEqual(sources.verify_rendered(rendered, self.root), 31)

    def test_missing_duplicate_unknown_id(self):
        for change in (lambda d: d["examples"].pop(),
                       lambda d: d["examples"].append(d["examples"][0]),
                       lambda d: d["examples"][0].update(id="unknown")):
            self.reject(change)

    def test_missing_duplicate_changed_execution_case(self):
        for change in (lambda d: d["examples"][0]["cases"].pop(),
                       lambda d: d["examples"][0]["cases"].append(d["examples"][0]["cases"][0]),
                       lambda d: d["examples"][0]["cases"][0].update(configuration="wrong"),
                       lambda d: d["examples"][0]["cases"][1].update(exit=0),
                       lambda d: d["examples"][0]["cases"][1].update(provider="normal")):
            self.reject(change)

    def test_duplicate_json_key(self):
        self.path.write_text(self.path.read_text().replace('"schema_version": 1',
                             '"schema_version": 1, "schema_version": 1'))
        with self.assertRaisesRegex(ValueError, "duplicate"):
            sources.inventory(self.root)

    def test_invalid_paths(self):
        for name in ("../outside.adb", "/tmp/outside.adb", "src/../../outside.adb",
                     "src/x.html", "src\\outside.adb"):
            self.reject(lambda d: d["examples"][0].update(main=name))

    def test_missing_and_symlink_source(self):
        path = self.root / "docs/examples/src/provider_session.adb"
        path.unlink()
        with self.assertRaises(ValueError):
            sources.inventory(self.root)
        path.symlink_to(ROOT / "docs/examples/src/provider_session.adb")
        with self.assertRaises(ValueError):
            sources.inventory(self.root)

    def test_symlink_source_directory(self):
        path = self.root / "docs/examples/src"
        shutil.rmtree(path)
        path.symlink_to(ROOT / "docs/examples/src", target_is_directory=True)
        with self.assertRaises(ValueError):
            sources.inventory(self.root)

    def test_unlisted_source(self):
        (self.root / "docs/examples/src/unlisted.adb").write_text("procedure Unlisted is begin null; end;\n")
        with self.assertRaises(ValueError):
            sources.inventory(self.root)

    def test_gpr_inventory_disagreement(self):
        path = self.root / "docs/examples/ams_mel_doc_examples.gpr"
        path.write_text(path.read_text().replace('"provider_session.adb",', ''))
        with self.assertRaisesRegex(ValueError, "GPR"):
            sources.inventory(self.root)

    def test_package_reference_disagreement(self):
        self.reject(lambda d: d["examples"][0].update(packages=["Unknown.Package"]))

    def test_readme_execution_disagreement(self):
        path = self.root / "docs/examples/README.md"
        path.write_text(path.read_text().replace("sh docs/examples/run.sh owned-image", ""))
        with self.assertRaisesRegex(ValueError, "README"):
            sources.inventory(self.root)

    def test_missing_duplicate_guide_placeholder_and_anchor(self):
        path = self.root / "docs/guides/examples.html"
        original = path.read_text()
        for text in (original.replace("{{example:provider-session}}", ""),
                     original + "{{example:provider-session}}",
                     original.replace('id="provider-session"', 'id="wrong"')):
            path.write_text(text)
            with self.assertRaisesRegex(ValueError, "guide"):
                sources.inventory(self.root)

    def test_unsafe_dependency(self):
        path = self.root / "docs/examples/src/provider_session.adb"
        original = path.read_text()
        for token in ("AMS.MEL_C_API", "AMS.MEL.RF.Product_Rx.Testing", "Interfaces.C",
                      "Ada.Unchecked_Conversion", "System"):
            path.write_text("with " + token + ";\n" + original)
            with self.assertRaisesRegex(ValueError, "unsafe/private"):
                sources.inventory(self.root)

    def test_tampered_stale_missing_or_duplicate_listing(self):
        rendered = sources.render((self.root / "docs/guides/examples.html").read_text(), self.root)
        for text in (rendered.replace("procedure Provider_Session", "procedure Tampered", 1),
                     rendered.replace('data-example-source="src/provider_session.adb"', '', 1),
                     rendered + sources.listing("src/provider_session.adb", self.root),
                     rendered.replace("procedure Provider_Session", "<b>procedure</b> Provider_Session", 1)):
            with self.assertRaises(ValueError):
                sources.verify_rendered(text, self.root)
        path = self.root / "docs/examples/src/provider_session.adb"
        path.write_text(path.read_text() + "-- new source revision\n")
        with self.assertRaisesRegex(ValueError, "stale"):
            sources.verify_rendered(rendered, self.root)

    def test_html_escape_round_trip(self):
        path = self.root / "docs/examples/src/provider_session.adb"
        path.write_text(path.read_text() + '-- <script> & "quoted" café\n')
        rendered = sources.render((self.root / "docs/guides/examples.html").read_text(), self.root)
        self.assertIn("&lt;script&gt; &amp; &quot;quoted&quot; café", rendered)
        self.assertEqual(sources.verify_rendered(rendered, self.root), 31)

    def test_loader_environment_is_not_inherited(self):
        with mock.patch.dict(os.environ, {"LD_LIBRARY_PATH": "/bad/build-tests/lib",
                                          "LD_PRELOAD": "/bad.so", "AMS_MEL_FAILPOINT": "1"}):
            env = runner.environment()
        self.assertEqual(env["LD_LIBRARY_PATH"], str(ROOT / "native/build/lib"))
        self.assertNotIn("LD_PRELOAD", env)
        self.assertNotIn("AMS_MEL_FAILPOINT", env)

    def test_explicit_isolated_alire_settings(self):
        with mock.patch.dict(os.environ, {"DOC_EXAMPLES_ALIRE_SETTINGS": "/isolated/settings"}):
            self.assertEqual(runner.alire(), ["alr", "-n", "-s", "/isolated/settings"])
        with mock.patch.dict(os.environ, {}, clear=True):
            self.assertEqual(runner.alire(), ["alr", "-n"])

    def test_wrong_facade_rejected(self):
        wrong = self.root / "libams_mel_c.so.0"
        wrong.write_bytes(b"not the production facade")
        production = self.root / "native/build/lib/libams_mel_c.so"
        production.parent.mkdir(parents=True)
        production.write_bytes(b"fixture production identity")
        output = "libams_mel_c.so.0 => " + str(wrong)
        with mock.patch.object(runner, "ROOT", self.root), mock.patch.object(
                subprocess, "run", return_value=subprocess.CompletedProcess([], 0, output)):
            with self.assertRaisesRegex(ValueError, "production facade"):
                runner.loader(Path("ignored"), runner.environment())

    def test_semantic_exit_and_order_checks(self):
        case = {"id": "control", "exit": 0, "expected": ["first", "second"]}
        runner.qualify(case, subprocess.CompletedProcess([], 0, "first second", ""))
        for result in (subprocess.CompletedProcess([], 1, "first second", ""),
                       subprocess.CompletedProcess([], 0, "PASS", ""),
                       subprocess.CompletedProcess([], 0, "second first", "")):
            with self.assertRaises(ValueError):
                runner.qualify(case, result)

    def test_outer_timeout_is_failure(self):
        with self.assertRaises(subprocess.TimeoutExpired):
            runner.command([sys.executable, "-c", "import time; time.sleep(2)"],
                           runner.environment(), timeout=0.02)


if __name__ == "__main__":
    unittest.main()
