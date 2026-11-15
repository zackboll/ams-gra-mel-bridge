"""Offline live-publication acceptance: real HTTP and independently qualified input."""

import contextlib
import gzip
import http.server
import sys
import tempfile
import threading
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "docs/tools"))
import check_live_docs_site as live
import documentation_fixtures as fixtures


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
    def test_isolated_historical_artifact_security_campaign(self):
        # Generate from the exact old source, then let its unchanged consumer
        # prove complete native/source/portal fingerprints before HTTP testing.
        python = fixtures.historical_python()
        with tempfile.TemporaryDirectory(prefix='historical-live-fixture-') as temp:
            root = fixtures.checkout(Path(temp) / 'source')
            fixtures.execute(root, ['sh', 'scripts/generate_docs_baseline.sh'])
            fixtures.verify(root)
            fixtures.execute(root, [python, '-W', 'error',
                                    str(ROOT / 'docs/tests/historical_live_artifact_cases.py'),
                                    str(root), '-v'])
            fixtures.verify(root)


class WorkflowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='historical-workflow-')
        cls.addClassCleanup(cls.temp.cleanup)
        cls.history = fixtures.checkout(Path(cls.temp.name) / 'source')

    def contract(self, name):
        fixtures.verify(self.history)
        fixtures.execute(self.history, [sys.executable, '-c',
                         "import sys; sys.path.insert(0, 'docs/tools'); "
                         f"from ci001r3_workflow import {name}; {name}('.')"])
        fixtures.verify(self.history)

    def test_main_only_read_only_dependency(self):
        self.contract('live_contract')

    def test_pr_fixture_discovery_retained(self):
        self.contract('preparation_contract')

    def test_historical_deployment_contract(self):
        self.contract('deploy_contract')

    def test_current_workflow_independently_qualified(self):
        from docs_workflow import check
        self.assertEqual(set(check(ROOT)), {'build', 'deploy', 'verify-live'})

    def test_current_security_mutations_rejected(self):
        from docs_workflow import check
        text = (ROOT / '.github/workflows/documentation.yml').read_text()
        mutations = (
            ("github.ref == 'refs/heads/main' && github.event_name == 'push'", 'always()'),
            ('    needs: [build, deploy]', '    needs: build'),
            ('${{ needs.build.outputs.artifact_id }}', '123'),
            ('--deployment', ''),
            ('--manifest-sha', '--untrusted-manifest'),
            ('--bundle-sha', '--untrusted-bundle'),
            ('      persist-credentials: false', '      persist-credentials: true'),
            ('      contents: read', '      contents: write'),
            ('include-hidden-files: true', 'include-hidden-files: false'),
            ('make test-docs-ci', 'true'),
            ('docs_ci.py live', 'docs_ci.py build'),
            ('docs_ci.py build --gnatdoc-prefix', 'docs_ci.py live --gnatdoc-prefix'),
            ('actions/checkout@', 'unreviewed/checkout@'),
        )
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            path = root / '.github/workflows/documentation.yml'
            path.parent.mkdir(parents=True)
            for before, after in mutations:
                with self.subTest(protection=before):
                    self.assertIn(before, text)
                    path.write_text(text.replace(before, after))
                    with self.assertRaises(AssertionError):
                        check(root)

    def test_historical_source_substitution_rejected(self):
        path = self.history / '.github/workflows/documentation.yml'
        original = path.read_bytes()
        try:
            path.write_bytes((ROOT / '.github/workflows/documentation.yml').read_bytes())
            with self.assertRaisesRegex(ValueError, 'working tree differs'):
                fixtures.verify(self.history)
        finally:
            path.write_bytes(original)

    def test_historical_tree_substitution_rejected(self):
        from unittest import mock
        with mock.patch.object(fixtures, 'TREE', '0' * 40):
            with self.assertRaisesRegex(ValueError, 'fixture tree mismatch'):
                fixtures.verify(self.history)


if __name__ == "__main__":
    unittest.main()
