"""Offline live-publication acceptance: real HTTP and independently qualified input."""

import contextlib
import functools
import gzip
import http.server
import json
import re
import shutil
import sys
import tempfile
import threading
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "docs/tools"))
import check_live_docs_site as live


@contextlib.contextmanager
def server(handler):
    instance = http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler)
    thread = threading.Thread(target=instance.serve_forever, daemon=True)
    thread.start()
    try:
        yield f"http://127.0.0.1:{instance.server_port}/project/"
    finally:
        instance.shutdown()
        instance.server_close()
        thread.join()


class TransportTests(unittest.TestCase):
    def response(self, status=200, body=b"qualified", headers=None, location=None):
        class Handler(http.server.BaseHTTPRequestHandler):
            def do_GET(self):
                self.send_response(status)
                for key, value in (headers if headers is not None else [("Content-Type", "text/html; charset=utf-8")]):
                    self.send_header(key, value)
                if location:
                    self.send_header("Location", location)
                self.end_headers()
                self.wfile.write(body)

            def log_message(self, *args):
                pass
        with server(Handler) as base:
            return live.get_entity(base + "index.html", base, b"qualified")

    def test_actual_get_success(self):
        record, body = self.response()
        self.assertEqual(body, b"qualified")
        self.assertTrue(record["comparison"])

    def test_gzip_decoded_entity(self):
        record, body = self.response(body=gzip.compress(b"qualified"), headers=[
            ("Content-Type", "text/html"), ("Content-Encoding", "gzip")])
        self.assertTrue(record["comparison"])
        self.assertEqual(body, b"qualified")

    def test_invalid_gzip(self):
        record, body = self.response(headers=[("Content-Type", "text/html"), ("Content-Encoding", "gzip")])
        self.assertIsNone(body)
        self.assertIn("gzip", record["error"])

    def test_duplicate_response_header(self):
        record, body = self.response(headers=[("Content-Type", "text/html"), ("Content-Type", "text/html")])
        self.assertIsNone(body)
        self.assertIn("duplicate response header", record["error"])

    def test_malformed_length(self):
        record, body = self.response(headers=[("Content-Type", "text/html"), ("Content-Length", "garbage")])
        self.assertIsNone(body)
        self.assertIn("response length", record["error"])

    def test_ambiguous_framing(self):
        record, body = self.response(headers=[("Content-Type", "text/html"), ("Content-Length", "9"),
                                               ("Transfer-Encoding", "chunked")])
        self.assertIsNone(body)
        self.assertIn("ambiguous response framing", record["error"])

    def test_truncated_response(self):
        record, body = self.response(headers=[("Content-Type", "text/html"), ("Content-Length", "12")])
        self.assertIsNone(body)
        self.assertIn("response length", record["error"])

    def test_wrong_css_mime(self):
        with server(type("Handler", (http.server.BaseHTTPRequestHandler,), {
                "do_GET": lambda self: (self.send_response(200), self.send_header("Content-Type", "text/html"),
                                         self.end_headers(), self.wfile.write(b"qualified")),
                "log_message": lambda *args: None})) as base:
            record, body = live.get_entity(base + "theme.css", base, b"qualified")
        self.assertIsNone(body)
        self.assertIn("MIME", record["error"])

    def test_wrong_js_mime(self):
        with server(type("Handler", (http.server.BaseHTTPRequestHandler,), {
                "do_GET": lambda self: (self.send_response(200), self.send_header("Content-Type", "text/plain"),
                                         self.end_headers(), self.wfile.write(b"qualified")),
                "log_message": lambda *args: None})) as base:
            record, body = live.get_entity(base + "navigation.js", base, b"qualified")
        self.assertIsNone(body)
        self.assertIn("MIME", record["error"])

    def test_root_redirect_encoded_alias_rejected(self):
        class Handler(http.server.BaseHTTPRequestHandler):
            def do_GET(self):
                self.send_response(301)
                self.send_header("Location", "/other/../project/")
                self.end_headers()

            def log_message(self, *args):
                pass
        with server(Handler) as base:
            record, body = live.get_entity(base[:-1], base, b"qualified")
        self.assertIsNone(body)
        self.assertIn("unintended redirect", record["error"])


def transport_test(**kwargs):
    expected = kwargs.pop("guard")
    def test(self):
        record, body = self.response(**kwargs)
        self.assertIsNone(body)
        self.assertFalse(record["comparison"])
        self.assertIn(expected, record["error"])
    return test


for name, arguments in {
    "missing_file": dict(status=404, guard="HTTP status"),
    "forbidden": dict(status=403, guard="HTTP status"),
    "server_error": dict(status=500, guard="HTTP status"),
    "no_content": dict(status=204, guard="HTTP status"),
    "partial_response": dict(status=206, guard="HTTP status"),
    "changed_bytes": dict(body=b"tampered", guard="entity bytes differ"),
    "stale_publication": dict(body=b"old qualified version", guard="entity bytes differ"),
    "correct_name_wrong_content": dict(body=b"other file", guard="entity bytes differ"),
    "wrong_mime": dict(headers=[("Content-Type", "application/octet-stream")], guard="MIME"),
    "missing_mime": dict(headers=[], guard="MIME"),
    "wrong_encoding": dict(headers=[("Content-Type", "text/html; charset=iso-8859-1")], guard="encoding"),
    "unsupported_compression": dict(headers=[("Content-Type", "text/html"), ("Content-Encoding", "br")], guard="content encoding"),
    "foreign_redirect": dict(status=301, location="https://example.com/project/index.html", guard="unintended redirect"),
    "repository_escape_redirect": dict(status=302, location="/other/index.html", guard="unintended redirect"),
    "query_authorization_redirect": dict(status=307, location="index.html?token=secret", guard="unintended redirect"),
    "unexpected_location": dict(location="index.html?token=secret", guard="unexpected response Location"),
}.items():
    setattr(TransportTests, "test_" + name, transport_test(**arguments))


class URLTests(unittest.TestCase):
    def test_production_origin(self):
        self.assertEqual(live.base_url("https://zackboll.github.io/ams-gra-mel-bridge/"),
                         "https://zackboll.github.io/ams-gra-mel-bridge/")

    def test_safe_join(self):
        self.assertEqual(live.file_url("https://owner.github.io/repo/", "api/index.html"),
                         "https://owner.github.io/repo/api/index.html")

    def test_parent_guide_link_inside_root(self):
        files = {"index.html": b'<h1 id="ok">Home</h1>',
                 "guides/example.html": b'<a href="../index.html#ok">Home</a>'}
        self.assertEqual(live.graph(files)[0]["target"], "index.html")

    def test_unsafe_origins(self):
        for value in ("http://owner.github.io/repo/", "https://evil.test/repo/", "https://owner.github.io/other/../repo/",
                      "https://owner.github.io/repo/?token=secret", "https://user:secret@owner.github.io/repo/",
                      "https://owner.github.io/repo", "https://owner.github.io:443/repo/"):
            with self.subTest(value=value), self.assertRaises(live.LiveError):
                live.base_url(value)

    def test_unapproved_active_content(self):
        for text in ('<script src="remote.js"></script>', '<img src="https://evil.test/img.png">',
                     '<p onclick="alert(1)">x</p>', '<base href="../">', '<style>@import "evil";</style>',
                     '<h1 id="same">x</h1><h2 id="same">y</h2>', '<a href="#" href="evil.html">x</a>'):
            with self.subTest(text=text), self.assertRaises(live.LiveError):
                live.graph({"index.html": text.encode()})


def path_test(value):
    def test(self):
        with self.assertRaisesRegex(live.LiveError, "path"):
            live.file_url("https://owner.github.io/repo/", value)
    return test


for number, value in enumerate(("../secret", "%2e%2e/secret", "%252e%252e/secret", "/other/index.html",
                                "api/../../secret", "api\\secret", "index.html?token=secret", "api//index.html",
                                "./index.html", "https://evil.test/file", "api/\nfile")):
    setattr(URLTests, f"test_unsafe_path_{number:02d}", path_test(value))


def graph_test(reference, guard):
    def test(self):
        with self.assertRaisesRegex(live.LiveError, guard):
            live.graph({"index.html": f'<a href="{reference}">link</a>'.encode()})
    return test


for number, (reference, guard) in enumerate((
        ("../outside.html", "directory escape"), ("%2e%2e/outside.html", "encoded"),
        ("index.html?authorization=secret", "query"), ("https://other.test/file", "scheme/origin"),
        ("javascript:alert(1)", "scheme/origin"), ("missing.html", "destination missing"),
        ("#wrong-native-anchor", "fragment missing"), ("//other.test/file", "scheme/origin"))):
    setattr(URLTests, f"test_graph_rejection_{number:02d}", graph_test(reference, guard))


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
        self.assertEqual(len(result["internal_references"]), 3021)
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
        return next(p for p in (self.site / "api").glob("*.html") if p.name != "index.html")

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


class WorkflowTests(unittest.TestCase):
    def test_main_only_read_only_dependency(self):
        text = (ROOT / ".github/workflows/documentation.yml").read_text()
        live_job = text.split("  verify-live:\n")[1]
        self.assertIn("if: github.ref == 'refs/heads/main' && github.event_name != 'pull_request'", live_job)
        self.assertIn("needs: deploy", live_job)
        self.assertIn("contents: read", live_job)
        self.assertIn("actions: read", live_job)
        self.assertNotIn(": write", live_job)
        self.assertIn("ref: ${{ github.sha }}", live_job)
        self.assertIn("name: ada-api-${{ github.sha }}", live_job)
        self.assertIn("name: ada-api-pristine-${{ github.sha }}", live_job)
        self.assertNotIn("run-id:", live_job)
        self.assertNotIn("secrets.", live_job)
        self.assertIn("if: always()", live_job)
        self.assertIn("--attempts 10 --interval 30", live_job)

    def test_pr_fixture_discovery_retained(self):
        text = (ROOT / ".github/workflows/documentation.yml").read_text()
        self.assertIn("-m unittest discover -s docs/tests -p 'test_*.py' -v", text)
        self.assertIn("Compile and execute canonical public Ada examples", text)
        self.assertIn("DOC_EXAMPLES_ALIRE_SETTINGS", text)


if __name__ == "__main__":
    unittest.main()
