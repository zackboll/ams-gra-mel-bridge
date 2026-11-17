"""Python source reconstruction, publication isolation and integrity negatives."""

import copy
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / 'docs/tools'), str(ROOT / 'scripts')]
import docs_ci as ci
import python_publication as python
import rustdoc_publication as rust
import check_live_docs_site as live
from python_reference_fixtures import site
from test_rustdoc_publication import fixture as rust_site


class PythonPublicationTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.files = site()
        self.context = {'commit': 'a' * 40, 'tree': 'b' * 40, 'mode': 'local-not-deployable'}
        self.report = {'exports': ['Session'], 'documented_exports': 0}
        self.record = {'schema': 1, 'identity': self.context, 'generator': python.GENERATOR,
                       'report': self.report, 'counts': python.validate(self.files),
                       'files': python.publication.hashes(self.files)}
        patch = mock.patch.object(python.reference, 'generate', return_value=(self.files, self.report))
        patch.start()
        self.addCleanup(patch.stop)

    def write(self, folder, files):
        for name, data in files.items():
            target = self.root / folder / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)

    def test_zero_documented_prose_is_informational(self):
        generated, record = python.generate(self.root, self.context, self.root / 'python.log')
        self.assertEqual(record['report']['documented_exports'], 0)
        python.verify_record(self.root, generated, record, self.context)
        self.assertIn('no binding import', (self.root / 'python.log').read_text())

    def test_python_reversal_preserves_and_verifies_ada_and_rust(self):
        ada = {'index.html': b'<main><h1>Ada</h1></main>', 'api/index.html': b'Ada reference'}
        rust_files = rust_site()
        base = rust.assembled(ada, rust_files)
        complete = python.assembled(base, self.files)
        self.write('ada-qualified', ada)
        self.write('published', complete)
        projection = python.verify_assembly(self.root, self.root, self.context, self.record)
        self.assertEqual(projection, base)
        rust_record = {'schema': 1, 'identity': self.context, 'toolchain': rust.TOOLCHAIN,
                       'inputs': {'theme': dict(rust.THEME_IDENTITY)}, 'invocation': rust.invocation(),
                       'flags': rust.FLAGS, 'tools': {t: {'version': v, 'binary_sha256': rust.BINARIES[t]}
                                                     for t, v in rust.VERSIONS.items()},
                       'files': rust.publication.hashes(rust_files), 'counts': rust.validate(rust_files)}
        rust.verify_assembly(self.root, self.context, rust_record, published=projection)
        for name in ('api/index.html', 'rust/ams_mel/index.html'):
            bad = dict(projection)
            bad[name] += b'changed'
            with self.subTest(name=name), self.assertRaises(ValueError):
                rust.verify_assembly(self.root, self.context, rust_record, published=bad)

    def test_missing_extra_and_changed_python_files_fail(self):
        for name in self.files:
            for operation in ('remove', 'alter'):
                files = dict(self.files)
                if operation == 'remove':
                    del files[name]
                else:
                    files[name] += b'changed'
                with self.subTest(name=name, operation=operation), self.assertRaisesRegex(ValueError, 'inventory'):
                    python.verify_record(self.root, files, self.record, self.context)
        files = dict(self.files, **{'unexpected.js': b'new code'})
        with self.assertRaisesRegex(ValueError, 'inventory'):
            python.verify_record(self.root, files, self.record, self.context)

    def test_resealed_output_cannot_replace_source_reconstruction(self):
        files = dict(self.files)
        files['api/Session.html'] = files['api/Session.html'].replace(b'Session.open</h1>', b'Invented API</h1>')
        record = {**self.record, 'files': python.publication.hashes(files), 'counts': python.validate(files)}
        with self.assertRaisesRegex(ValueError, 'source reconstruction'):
            python.verify_record(self.root, files, record, self.context)
        record = {**self.record, 'report': {'exports': ['Invented'], 'documented_exports': 1}}
        with self.assertRaisesRegex(ValueError, 'source reconstruction'):
            python.verify_record(self.root, self.files, record, self.context)

    def test_wrong_source_generator_and_counts_fail(self):
        for key, value in (('schema', 99), ('identity', {}), ('generator', 'arbitrary'), ('counts', {})):
            record = copy.deepcopy(self.record)
            record[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                python.verify_record(self.root, self.files, record, self.context)

    def test_reserved_prefix_and_portal_changes_fail(self):
        for base in ({'python/extra.js': b'', 'index.html': b'</main>'},
                     {'python': b'', 'index.html': b'</main>'},
                     {'index.html': b'</main></main>'},
                     {'index.html': python.INSERTION + b'</main>'}):
            with self.subTest(base=base), self.assertRaises(ValueError):
                python.assembled(base, self.files)
        self.write('published', python.assembled({'index.html': b'<main></main>'}, self.files))
        home = self.root / 'published/index.html'
        for content in (b'<main></main>', python.INSERTION * 2 + b'<main></main>'):
            home.write_bytes(content)
            with self.assertRaisesRegex(ValueError, 'reversible'):
                python.verify_assembly(self.root, self.root, self.context, self.record)

    def test_symlink_payload_is_not_a_document(self):
        self.write('published', python.assembled({'index.html': b'<main></main>'}, self.files))
        (self.root / 'published/python/alias.html').symlink_to('index.html')
        with self.assertRaisesRegex(ValueError, 'symlink'):
            python.verify_assembly(self.root, self.root, self.context, self.record)

    def test_active_html_remote_resources_and_css_are_rejected(self):
        snippets = ('<iframe src="api/Session.html"></iframe>', '<base href="../">',
                    '<a href="api-index.html" onclick="run()">link</a>',
                    '<a href="api-index.html" ping="https://evil.test/track">link</a>',
                    '<script src="assets/search.js" attributionsrc="https://evil.test/track"></script>',
                    '<a href="api-index.html" target="remote">link</a>',
                    '<p style="background:url(https://evil.test)">text</p>',
                    '<meta http-equiv="refresh" content="0; url=https://evil.test">',
                    '<script>run()</script>', '<script src="assets/search.js">run()</script>',
                    '<script src="https://evil.test/code.js"></script>',
                    '<script src="source/_session.html"></script>',
                    '<link rel="stylesheet" href="https://evil.test/style.css">',
                    '<input type="password">', '<form action="api-index.html"></form>',
                    '<p id="same" id="other">x</p>', '<p id="same"></p><p id="same"></p>')
        for snippet in snippets:
            files = dict(self.files)
            files['index.html'] = snippet.encode()
            with self.subTest(snippet=snippet), self.assertRaises(ValueError):
                python.validate(files)
        for css in ('@import "https://evil.test";', 'body { background:url(data:text/plain,x); }'):
            files = dict(self.files)
            files['assets/reference.css'] = css.encode()
            with self.assertRaisesRegex(ValueError, 'CSS'):
                python.validate(files)

    def test_local_destinations_fragments_and_only_exact_portal_escape(self):
        for value in ('missing.html', 'api/Session.html#absent', '../rust/index.html',
                      '../index.html?extra=1', '/index.html', '%2e%2e/index.html',
                      '//evil.test/x', 'https://example.test/', 'javascript:run()',
                      'api\\Session.html'):
            files = dict(self.files)
            files['index.html'] = f'<a href="{value}">link</a>'.encode()
            with self.subTest(value=value), self.assertRaises(ValueError):
                python.validate(files)
        for name in ('../outside.html', '/absolute.html', 'api//bad.html', 'extra.svg'):
            files = dict(self.files, **{name: b'content'})
            with self.subTest(name=name), self.assertRaises(ValueError):
                python.validate(files)

    def test_live_rejects_invalid_python_before_network(self):
        ada = {'index.html': b'<main>Ada</main>'}
        self.write('ada-qualified', ada)
        files = python.assembled(rust.assembled(ada, rust_site()), self.files)
        files['python/api/Session.html'] = b'<script src="https://evil.test/x"></script>'
        self.write('published', files)
        with mock.patch.object(live, 'get_entity') as fetch:
            with self.assertRaises(ValueError):
                ci.live(self.root, 'https://owner.github.io/project/', self.root / 'live.json')
            fetch.assert_not_called()


if __name__ == '__main__':
    unittest.main()
