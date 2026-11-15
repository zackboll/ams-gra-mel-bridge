"""Historical HTTP security mutations, executed only with frozen source authority.

Invoked by test_live_docs_site.py in a separate interpreter. The explicit source
root is an authenticated disposable checkout, not the current repository _site.
"""

import contextlib
import functools
import http.server
import json
from pathlib import Path
import re
import shutil
import sys
import tempfile
import threading
import unittest
from unittest import mock

ROOT = Path(sys.argv.pop(1)).absolute()
sys.path[:0] = [str(ROOT / 'docs/tools'), str(ROOT / 'scripts')]
import check_live_docs_site as live


@contextlib.contextmanager
def server(handler):
    instance = http.server.ThreadingHTTPServer(('127.0.0.1', 0), handler)
    thread = threading.Thread(target=instance.serve_forever, daemon=True)
    thread.start()
    try:
        yield f'http://127.0.0.1:{instance.server_port}/project/'
    finally:
        instance.shutdown()
        instance.server_close()
        thread.join()


def catalog_links():
    from api_catalog import inventory, link_count
    from ada_examples import References
    return 3 + link_count(inventory(ROOT, References(ROOT, ROOT / '_site'), independent=True))


class QualifiedArtifactTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workspace = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.workspace.cleanup)
        cls.approved = Path(cls.workspace.name) / "approved"
        live.publication.build(ROOT / "_site", cls.approved)

    def setUp(self):
        self.workspace = tempfile.TemporaryDirectory()
        self.addCleanup(self.workspace.cleanup)
        self.site = Path(self.workspace.name) / "project"
        shutil.copytree(self.approved, self.site)

    def reject(self, guard):
        with self.assertRaisesRegex(ValueError, guard):
            live.qualified(self.site, ROOT / "_site")

    def test_complete_live_fixture_acceptance(self):
        class Quiet(http.server.SimpleHTTPRequestHandler):
            def log_message(self, *args):
                pass
        with server(functools.partial(Quiet, directory=self.workspace.name)) as base:
            result = live.verify(self.site, ROOT / "_site", base, "a" * 40, loopback=True)
        self.assertTrue(result["passed"])
        self.assertEqual(len(result["publication_sha256"]), 43)
        self.assertEqual(len(result["internal_references"]), 3525 + catalog_links())
        self.assertEqual(len(result["verified_symbol_destinations"]), 238)
        self.assertEqual(len(result["verified_local_type_destinations"]), 1428)
        self.assertEqual(len(result["ten_required_additions"]), 10)
        self.assertEqual(result["original_1418_audit_sha256"], live.publication.ORIGINAL_AUDIT_SHA256)
        self.assertEqual(result["canonical_source_listings"], 31)
        self.assertEqual((result["example_programs"], result["example_execution_cases"]), (9, 13))
        self.assertEqual(len(result["attempts"][0]["root_normalization"]["redirect_chain"]), 1)

    def test_symlink_artifact(self):
        path = self.site / "assets/portal.css"
        path.unlink()
        path.symlink_to(ROOT / "docs/assets/portal.css")
        self.reject("symlink")

    def test_unexpected_asset(self):
        (self.site / "extra.js").write_text("alert(1)")
        self.reject("missing/unexpected publication files")

    def test_missing_artifact_member(self):
        (self.site / "api/gnatdoc-navigation.js").unlink()
        self.reject("missing/unexpected publication files")

    def test_modified_manifest(self):
        (self.site / "api/presentation.json").write_text("{}")
        self.reject("altered/forged presentation manifest")

    def test_forged_self_consistent_hashes(self):
        path = self.site / "assets/portal.css"
        path.write_bytes(path.read_bytes() + b"/* forged */")
        manifest = self.site / "api/presentation.json"
        data = json.loads(manifest.read_bytes())
        data["output_sha256"]["assets/portal.css"] = live.digest(path.read_bytes())
        manifest.write_text(json.dumps(data))
        self.reject("changed native CSS or authored portal")

    def test_historical_source_authority_cannot_be_replaced(self):
        path = ROOT / 'ada/src/ams.ads'
        original = path.read_bytes()
        try:
            path.write_bytes(original + b'\n-- substituted historical source\n')
            self.reject('approved source-profile identity changed')
        finally:
            path.write_bytes(original)

    def test_historical_native_authority_cannot_be_replaced(self):
        path = ROOT / '_site/api/index.html'
        original = path.read_bytes()
        try:
            path.write_bytes(original + b'<!-- substituted native authority -->')
            self.reject('pristine native/portal bytes differ')
        finally:
            path.write_bytes(original)

    def test_wrong_javascript(self):
        path = self.site / "api/gnatdoc-navigation.js"
        path.write_bytes(path.read_bytes() + b"/* changed */")
        self.reject("navigation JavaScript")

    def test_wrong_stylesheet(self):
        path = self.site / "api/gnatdoc-theme.css"
        path.write_bytes(path.read_bytes() + b"/* changed */")
        self.reject("theme")

    def test_truncated_source_listing(self):
        path = self.site / "guides/examples.html"
        text = path.read_text()
        text, count = re.subn(r'(<code data-example-source="[^"]+">).*?(</code>)', r'\1truncated\2', text, count=1, flags=re.S)
        self.assertEqual(count, 1)
        path.write_text(text)
        with self.assertRaisesRegex(ValueError, "canonical source listing"):
            live.verify_rendered(text)
        self.reject("changed native CSS or authored portal")

    def test_missing_source_listing(self):
        path = self.site / "guides/examples.html"
        text, count = re.subn(r'<code data-example-source="[^"]+">.*?</code>', '', path.read_text(), count=1, flags=re.S)
        self.assertEqual(count, 1)
        path.write_text(text)
        with self.assertRaisesRegex(ValueError, "canonical source listing"):
            live.verify_rendered(text)
        self.reject("changed native CSS or authored portal")

    def api_page(self):
        authority = live.publication.Authority(ROOT, ROOT / "_site")
        required = sorted(authority.required_additions(), key=live.publication.occurrence_key)
        self.assertTrue(required, "missing trusted fixture occurrence")
        record = required[0]
        entry = record["containing"]
        self.assertEqual(authority.sections[(entry["page"], entry["anchor"])], entry)
        path = self.site / "api" / entry["page"]
        native = (ROOT / "_site/api" / entry["page"]).read_text()
        text = live.publication.restore_document(path.read_text(), native,
                                                "api/" + entry["page"], authority)
        restored, audit = live.publication.restore_page(entry["page"], text, authority)
        self.assertIn("<h4 id=" + entry["anchor"] + ">", native)
        self.assertIn(record, audit, "missing trusted fixture type-reference link")
        self.assertEqual(restored, native, "changed trusted fixture native declaration")
        return path

    def test_wrong_document_title(self):
        path = self.api_page()
        path.write_text(path.read_text().replace(" — AMS MEL Bridge</title>", " wrong</title>"))
        self.reject("document metadata")

    def test_wrong_document_encoding(self):
        path = self.api_page()
        path.write_text(path.read_text().replace('charset="utf-8"', 'charset="latin1"'))
        self.reject("document metadata")

    def test_wrong_native_anchor(self):
        path = self.api_page()
        text, count = re.subn(r'<h4 id=[a-f0-9]{64}>', '<h4 id=wrong>', path.read_text(), count=1)
        self.assertEqual(count, 1)
        path.write_text(text)
        self.reject("native|declaration|section")

    def test_missing_required_type_link(self):
        path = self.api_page()
        text, count = re.subn(r'<a class="type-reference"[^>]*>(.*?)</a>', r'\1', path.read_text(), count=1)
        self.assertEqual(count, 1)
        path.write_text(text)
        self.reject("original ordered type-link audit|required type-link occurrence")

    def test_extra_unsupported_link(self):
        path = self.api_page()
        path.write_text(path.read_text() + '<a href="extra.html">unsupported</a>')
        self.reject("native declaration/anchor/link/markup changed")

    def test_bounded_readiness_requires_complete_attempt(self):
        with mock.patch.object(live, "get_entity", return_value=({"comparison": False, "error": "stale"}, None)) as get:
            result = live.verify(self.site, ROOT / "_site", "https://owner.github.io/project/", "a" * 40,
                                 attempts=2, interval=0)
        self.assertFalse(result["passed"])
        self.assertEqual(len(result["attempts"]), 2)
        self.assertEqual(get.call_count, 88)

    def test_extra_empty_directory(self):
        (self.site / "unexpected").mkdir()
        self.reject("unexpected artifact directory")

    def test_no_network_before_artifact_qualification(self):
        (self.site / "api/presentation.json").write_text("{}")
        with mock.patch.object(live, "get_entity") as get:
            with self.assertRaisesRegex(ValueError, "altered/forged presentation manifest"):
                live.verify(self.site, ROOT / "_site", "https://owner.github.io/project/", "a" * 40)
        get.assert_not_called()

    def test_missing_commit_identity(self):
        with self.assertRaisesRegex(live.LiveError, "explicit full Git SHA"):
            live.verify(self.site, ROOT / "_site", "https://owner.github.io/project/", "main")

    def test_unbounded_retry_policy(self):
        with self.assertRaisesRegex(live.LiveError, "unbounded readiness"):
            live.verify(self.site, ROOT / "_site", "https://owner.github.io/project/", "a" * 40, attempts=21)


if __name__ == '__main__':
    unittest.main()
