"""Focused offline regression tests for the standalone Rustdoc foundation."""

import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import rustdoc_001_foundation as foundation


class FoundationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.site = Path(self.temp.name)
        for name in ("index.html", "ams_mel/index.html", "ams_mel_sys/index.html", "ams_mel_common/index.html",
                     "ams_mel/struct.Session.html", "src-files.js", "crates.js",
                     "static.files/style.css", "src/ams_mel/lib.rs.html",
                     "src/ams_mel_sys/lib.rs.html", "src/ams_mel_common/lib.rs.html"):
            path = self.site / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("", encoding="utf-8")
        (self.site / "index.html").write_text(foundation.LANDING, encoding="utf-8")

    def test_safe_and_raw_landing_and_nested_assets(self):
        (self.site / "ams_mel/index.html").write_text(
            '<link href="../static.files/style.css"><a href="struct.Session.html#methods">'
            '<a href="https://doc.rust-lang.org/std/index.html">', encoding="utf-8")
        self.assertEqual(foundation.verify_site(self.site), 8)

    def test_missing_raw_reference_is_not_safe_reference_success(self):
        (self.site / "ams_mel_sys/index.html").unlink()
        with self.assertRaisesRegex(ValueError, "entry point"):
            foundation.verify_site(self.site)

    def test_missing_asset_is_rejected(self):
        (self.site / "ams_mel/index.html").write_text(
            '<script src="../static.files/missing.js"></script>', encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "local link"):
            foundation.verify_site(self.site)

    def test_root_relative_and_escaping_links_are_rejected(self):
        for url in ("/ams_mel/index.html", "../../outside.html", "..%2f..%2foutside.html"):
            with self.subTest(url=url):
                (self.site / "ams_mel/index.html").write_text(f'<a href="{url}">')
                with self.assertRaises(ValueError):
                    foundation.verify_site(self.site)

    def test_symlinks_are_rejected(self):
        (self.site / "extra.html").symlink_to(self.site / "index.html")
        with self.assertRaisesRegex(ValueError, "symlink"):
            foundation.verify_site(self.site)

    def test_diagnostics_do_not_double_count_cargo_summaries(self):
        warning = {"reason": "compiler-message", "message": {
            "level": "warning", "code": {"code": "missing_docs"}}}
        text = "\n".join(json.dumps(event) for event in (
            warning, warning, {"reason": "build-finished", "success": True}))
        self.assertEqual(foundation.diagnostics(text), {"warning:missing_docs": 2})

    def test_subprocess_failure_is_not_reported_as_success(self):
        with self.assertRaisesRegex(RuntimeError, "failed"):
            foundation.run(["sh", "-c", "exit 7"], self.site, {}, self.site, "bad")
        self.assertTrue((self.site / "bad.stderr.log").exists())

    def test_build_contract_is_offline_fresh_and_non_deploying(self):
        # Exercise orchestration without Cargo/native compilation in unit tests.
        calls = []

        def fake_run(command, directory, env, output, label):
            calls.append((command, env.copy()))
            if label in foundation.CRATES:
                for name in ("index.html", "ams_mel/index.html", "ams_mel_sys/index.html", "ams_mel_common/index.html",
                             "ams_mel/struct.Session.html", "src-files.js", "crates.js",
                             "static.files/style.css", "src/ams_mel/lib.rs.html",
                             "src/ams_mel_sys/lib.rs.html", "src/ams_mel_common/lib.rs.html"):
                    path = output / "target/doc" / name
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes((self.site / name).read_bytes())
                return ""
            return "test identity\n"

        with patch.object(foundation, "ROOT", self.site), \
                patch.object(foundation, "run", side_effect=fake_run), \
                patch.dict("os.environ", {"CARGO_BUILD_TARGET": ""}):
            first = foundation.qualify()
            second = foundation.qualify(doctests=True)
        self.assertNotEqual(first, second)
        docs = [cmd for cmd, _ in calls if cmd[:2] == ["cargo", "doc"]]
        self.assertEqual(len(docs), 6)
        for command in docs:
            self.assertIn("--locked", command)
            self.assertIn("--no-deps", command)
        self.assertEqual(sum(cmd[:2] == ["cargo", "test"] for cmd, _ in calls), 1)
        self.assertEqual(sum(cmd[0] == "sh" for cmd, _ in calls), 1)
        for _, env in calls:
            self.assertEqual(env["CARGO_NET_OFFLINE"], "true")
            self.assertIn("--deny rustdoc::broken_intra_doc_links", env["RUSTDOCFLAGS"])
            self.assertIn("--warn missing_docs", env["RUSTDOCFLAGS"])
        report = json.loads((second / "qualification.json").read_text())
        self.assertIn("ams_mel_sys/index.html", "ams_mel_common/index.html", report["files"])


if __name__ == "__main__":
    unittest.main()
