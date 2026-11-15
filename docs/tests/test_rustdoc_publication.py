"""Provider-free assembly, Rust-only resource policy and integrity negatives."""

from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / 'docs/tools'), str(ROOT / 'scripts')]
import rustdoc_publication as rust
import docs_ci as ci
import check_live_docs_site as live


def fixture():
    files = {n: b'<html></html>' for n in (
        'ams_mel/index.html', 'ams_mel_sys/index.html', 'ams_mel/struct.Session.html',
        'ams_mel_sys/fn.ams_mel_session_close.html', 'help.html', 'settings.html',
        'src/ams_mel/lib.rs.html', 'src/ams_mel_sys/lib.rs.html')}
    files.update({n: b'/* qualified generated resource */' for n in (
        'crates.js', 'src-files.js', 'static.files/main.js', 'search.index/root.js',
        'trait.impl/trait.js', 'type.impl/type.js')})
    files['index.html'] = rust.LANDING
    files['ams_mel/index.html'] = (b'<link href="../static.files/style.css">'
                                   b'<script src="../static.files/main.js"></script>'
                                   b'<a href="../src/ams_mel/lib.rs.html#1-2">Source</a>'
                                   b'<a href="struct.Session.html#method.open">Session</a>')
    files['ams_mel/struct.Session.html'] = b'<h1 id="method.open">Session</h1>'
    files['src/ams_mel/lib.rs.html'] = b'<a id="1"></a><a id="2"></a>'
    files['static.files/style.css'] = b'body {font-family:test} @font-face {src:url("test.woff2")}'
    files['static.files/test.woff2'] = b'font'
    return files


class AssemblyTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.directory = Path(temp.name)
        self.files = fixture()
        self.context = {'commit': 'a' * 40, 'tree': 'b' * 40}
        self.record = {'schema': 1, 'identity': self.context, 'toolchain': rust.TOOLCHAIN,
                       'flags': rust.FLAGS, 'tools': {t: {'version': v, 'binary_sha256': rust.BINARIES[t]}
                                                     for t, v in rust.VERSIONS.items()},
                       'files': rust.publication.hashes(self.files), 'counts': rust.validate(self.files)}

    def test_safe_raw_source_shared_css_font_and_script_links(self):
        rust.verify_record(self.files, self.record, self.context)
        for name in self.files:
            url = live.file_url('https://owner.github.io/project/', 'rust/' + name)
            self.assertTrue(url.startswith('https://owner.github.io/project/rust/'))

    def test_complete_asset_mutations_fail_even_if_graph_does_not_reference_them(self):
        for name in self.files:
            for mutation in ('remove', 'replace'):
                files = dict(self.files)
                files.pop(name) if mutation == 'remove' else files.update({name: b'forged JS or asset'})
                with self.subTest(name=name, mutation=mutation), self.assertRaisesRegex(ValueError, 'inventory'):
                    rust.verify_record(files, self.record, self.context)
        files = dict(self.files, unexpected=b'extra')
        with self.assertRaisesRegex(ValueError, 'inventory'):
            rust.verify_record(files, self.record, self.context)

    def test_missing_forged_source_tree_toolchain_and_inventory_fail(self):
        for key, value in (('identity', {}), ('toolchain', 'stable'), ('flags', ''), ('files', {}), ('counts', {})):
            record = dict(self.record, **{key: value})
            with self.subTest(key=key), self.assertRaises(ValueError):
                rust.verify_record(self.files, record, self.context)
        record = {**self.record, 'tools': {t: dict(v) for t, v in self.record['tools'].items()}}
        record['tools']['rustdoc']['binary_sha256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'toolchain mismatch'):
            rust.verify_record(self.files, record, self.context)

    def test_generation_rejects_wrong_toolchain_before_cargo(self):
        with mock.patch.object(rust.subprocess, 'check_output', return_value='cargo 1.99.0\n'):
            with self.assertRaisesRegex(ValueError, 'unqualified Rust documentation toolchain'):
                rust.generate(self.directory, self.directory / 'target', self.context, self.directory / 'log')

    def test_assembly_is_reversible_and_all_ada_bytes_unchanged(self):
        ada = {'index.html': b'<main><h1>Ada</h1></main>',
               'api/index.html': b'<h1 id="preserved">Ada API</h1>',
               'guides/api-map.html': b'original guide'}
        assembled = rust.assembled(ada, self.files)
        for tree, files in (('ada-qualified', ada), ('published', assembled)):
            for name, data in files.items():
                path = self.directory / tree / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
        rust.verify_assembly(self.directory, self.context, self.record)
        for name in ('index.html', 'api/index.html', 'guides/api-map.html'):
            path = self.directory / 'published' / name
            original = path.read_bytes()
            path.write_bytes(original + b'<!-- unauthorized -->')
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, 'transformation'):
                rust.verify_assembly(self.directory, self.context, self.record)
            path.write_bytes(original)
        (self.directory / 'published/rust/alias').symlink_to('index.html')
        with self.assertRaisesRegex(ValueError, 'symlink'):
            rust.verify_assembly(self.directory, self.context, self.record)

    def test_collision_and_duplicate_insertion_rejected(self):
        for ada in ({'rust/extra': b'', 'index.html': b'</main>'},
                    {'index.html': b'</main></main>'}, {'index.html': rust.INSERTION + b'</main>'}):
            with self.assertRaises(ValueError):
                rust.assembled(ada, self.files)

    def test_links_fragments_traversal_and_unapproved_external_resources(self):
        for value in ('missing.html', 'struct.Session.html#absent', '/rust/index.html',
                      '../../outside.html', '..%2f..%2fescape', 'https://evil.test/',
                      'https://doc.rust-lang.org.evil.test/1.98.1/', '//evil.test/x',
                      'javascript:alert(1)', 'https://doc.rust-lang.org/1.98.1/std/index.html'):
            # Even approved standard-library URLs cannot become script sources.
            files = dict(self.files)
            files['ams_mel/index.html'] = f'<script src="{value}"></script>'.encode()
            with self.subTest(value=value), self.assertRaises(ValueError):
                rust.validate(files)

    def test_generated_external_hyperlinks_are_not_fetched_or_ada_permissions(self):
        files = dict(self.files)
        files['ams_mel/index.html'] = (b'<a href="https://doc.rust-lang.org/1.98.1/std/index.html">std</a>'
                                      b'<a href="javascript:void(0)">UI</a>')
        rust.validate(files)
        with self.assertRaises(live.LiveError):
            live.graph({'index.html': files['ams_mel/index.html']})

    def test_manifest_canonical_order_and_duplicates_fail(self):
        # The externally approved digest alone is not permission for ambiguous JSON.
        for raw in (b'{"schema":1,"payload":{}}\n', b'{"payload":{},"payload":{}}\n'):
            (self.directory / 'manifest.json').write_bytes(raw)
            with mock.patch.object(ci, 'identity', return_value=self.context):
                with self.assertRaisesRegex(ValueError, 'noncanonical/duplicate'):
                    ci.verify(ROOT, self.directory, ci.epoch.sha(raw))

    def test_rust_mime_policy_does_not_apply_to_ada(self):
        # Same real HTTP framing/byte checks; only the scoped MIME selection changes.
        from test_live_docs_site import server
        from http.server import BaseHTTPRequestHandler

        class Handler(BaseHTTPRequestHandler):
            def do_GET(self):
                self.send_response(200)
                self.send_header('Content-Type', 'font/woff2')
                self.send_header('Content-Length', '4')
                self.end_headers()
                self.wfile.write(b'font')

            def log_message(self, *args):
                pass

        with server(Handler) as base:
            record, body = live.get_entity(base + 'rust/font.woff2', base, b'font', rustdoc=True)
            self.assertTrue(record['comparison'])
            self.assertEqual(body, b'font')
            for name, flag in (('assets/font.woff2', True), ('assets/font.woff2', False)):
                record, body = live.get_entity(base + name, base, b'font', rustdoc=flag)
                self.assertFalse(record['comparison'])
                self.assertIsNone(body)

    def test_live_exhaustively_checks_every_file_with_bounded_workers(self):
        published = self.directory / 'published'
        published.mkdir()
        for name, data in self.files.items():
            path = published / 'rust' / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        (published / 'index.html').write_bytes(b'<main>qualified</main>')
        ada = self.directory / 'ada-qualified'
        ada.mkdir()
        (ada / 'index.html').write_bytes(b'<main>qualified</main>')
        seen = []

        def fetch(url, base, data, **kwargs):
            seen.append((url, kwargs['rustdoc']))
            return {'comparison': True}, data

        with mock.patch.object(live, 'get_entity', side_effect=fetch):
            ci.live(self.directory, 'https://owner.github.io/project/', self.directory / 'live.json')
        self.assertEqual(len(seen), len(self.files) + 1)
        self.assertEqual(len({url for url, _ in seen}), len(seen))
        self.assertEqual(sum(flag for _, flag in seen), len(self.files))


if __name__ == '__main__':
    unittest.main()
