"""DOC-011 independent lexical, occurrence-authority and markup mutations."""

import html
import json
from pathlib import Path
import re
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / "scripts"), str(ROOT / "docs/tools")]
from ada_examples import KEYWORDS, References, annotate, tokenize
from example_sources import inventory, occurrence_ledger, render, source, verify_rendered


class LexerTests(unittest.TestCase):
    def test_all_keywords_case_insensitive(self):
        self.assertTrue(all(tokenize(w.upper())[0].kind == "keyword" for w in KEYWORDS))

    def test_contexts_and_escaped_quotes(self):
        text = 'procedure X is -- begin "x"\r\n S := "end ""if"""; C := \'é\'; X\'Image;'
        tokens = tokenize(text)
        self.assertEqual("".join(t.text for t in tokens), text)
        self.assertEqual([t.text for t in tokens if t.kind == "keyword"], ["procedure", "is"])
        self.assertIn('"end ""if"""', [t.text for t in tokens if t.kind == "string"])
        self.assertIn("'é'", [t.text for t in tokens if t.kind == "character"])
        self.assertIn("'Image", [t.text for t in tokens if t.kind == "attribute"])

    def test_numbers_ranges_and_attributes(self):
        text = "16#1234_ABCD# 2#1.01#E+3 1_000.25E-2 1..4 X'Range X'Length X'(1)"
        tokens = tokenize(text)
        self.assertEqual([t.text for t in tokens if t.kind == "number"],
                         ["16#1234_ABCD#", "2#1.01#E+3", "1_000.25E-2", "1", "4", "1"])
        self.assertEqual([t.text for t in tokens if t.kind == "attribute"], ["'Range", "'Length"])

    def test_qualification_tick_is_not_character(self):
        tokens = tokenize("Character'('A')")
        self.assertEqual([t.text for t in tokens if t.kind == 'character'], ["'A'"])
        self.assertEqual(tokens[1].kind, 'operator')

    def test_exact_unicode_line_endings_and_escaping(self):
        text = '-- <script> & " café α\r\n\tX := "<&";\r\n'
        self.assertEqual(html.unescape(re.sub('<[^>]+>', '', annotate(text))), text)


class AuthorityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.references = References(ROOT, ROOT / "_site")
        cls.guide = (ROOT / "docs/guides/examples.html").read_text()
        cls.rendered = render(cls.guide, references=cls.references)
        cls.ledger = occurrence_ledger(references=cls.references)

    def records(self, text):
        return self.references.occurrences(text, "fixture.adb")

    def test_full_native_occurrence_audit(self):
        self.assertEqual(verify_rendered(self.rendered, references=self.references), 31)
        self.assertEqual(len(self.ledger), 266)
        for record in self.ledger:
            text = source(record["source"])
            self.assertEqual(text[record["start"]:record["end"]], record["visible"])
            self.assertTrue((ROOT / record["package_source"]).is_file())
            target = (ROOT / "_site/guides" / record["href"].split('#')[0]).resolve()
            self.assertTrue(target.is_relative_to(ROOT / "_site/api"))
            native = target.read_text()
            if '#' in record["href"]:
                self.assertIn('id=' + record["href"].split('#')[1] + '>', native)
                self.assertIn(record["declaration"]["declaration"].strip().splitlines()[0],
                              (ROOT / record["package_source"]).read_text())
        identities = {r["identity"] for r in self.ledger}
        self.assertTrue({'ams.mel.session', 'ams.mel.ir.image.full_frame',
                         'ams.mel.rf.c2.job_request', 'ams.mel.open',
                         'ams.mel.ir.image.receive', 'ams.mel.rf.c2.submit_job'} <= identities)

    def test_alias_spoof_missing_import_and_shadow(self):
        original = source("src/owned_image.adb")
        for text in (original.replace('renames AMS.MEL.IR.Image', 'renames Other.Image'),
                     original.replace('with AMS.MEL.IR.Image;', ''),
                     original.replace('begin', 'Image : Integer; begin', 1),
                     original.replace('begin', 'X, Image : Integer; begin', 1),
                     original.replace('begin', 'procedure P (Image : Integer); begin', 1),
                     original.replace('begin', 'package Image renames AMS.MEL.IR.Image; begin', 1),
                     original.replace('begin', 'AMS : Integer; begin', 1)):
            with self.subTest(text=text):
                self.assertFalse(any(r['identity'] == 'ams.mel.ir.image.full_frame' for r in self.records(text)))

    def test_ambiguous_unknown_private_operations_unlinked(self):
        text = ('with AMS.MEL.RF.C2; procedure X is begin AMS.MEL.RF.C2.Close (X); '
                'AMS.MEL.RF.C2.Unknown (X); AMS.MEL.RF.C2.Handle (X); end X;')
        self.assertEqual([r['context'] for r in self.records(text)], ['context-package'])

    def test_literals_comments_never_authorize(self):
        text = ('with AMS.MEL; procedure X is begin '
                'Put ("AMS.MEL.Open (X)"); -- AMS.MEL.Open (X)\nend X;')
        self.assertEqual([r['context'] for r in self.records(text)], ['context-package'])

    def test_missing_native_anchor_fails(self):
        with tempfile.TemporaryDirectory() as temp:
            site = Path(temp)
            shutil.copytree(ROOT / '_site/api', site / 'api')
            entry = next(r['declaration'] for r in self.ledger if r['declaration'])
            page = site / 'api' / entry['page']
            page.write_text(page.read_text().replace('id=' + entry['anchor'], 'id=' + '0' * 64))
            with self.assertRaises(ValueError):
                References(ROOT, site)

    def test_substituted_native_profile_fails(self):
        with tempfile.TemporaryDirectory() as temp:
            site = Path(temp)
            shutil.copytree(ROOT / '_site/api', site / 'api')
            entry = next(r['declaration'] for r in self.ledger if r['declaration'])
            page = site / 'api' / entry['page']
            page.write_text(page.read_text().replace(entry['native'], 'type Fabricated is private;', 1))
            with self.assertRaisesRegex(ValueError, 'profile changed'):
                References(ROOT, site)

    def test_wrong_existing_declaration_not_authorized(self):
        first, second = [r for r in self.ledger if r['declaration']][:2]
        self.assertNotEqual(first['href'], second['href'])
        text = self.rendered.replace(first['href'], second['href'], 1)
        with self.assertRaises(ValueError):
            verify_rendered(text, references=self.references)

    def test_missing_stale_substituted_canonical_files(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            shutil.copytree(ROOT / 'docs/examples', root / 'docs/examples',
                            ignore=shutil.ignore_patterns('bin', 'obj', 'alire'))
            (root / 'docs/guides').mkdir(parents=True)
            shutil.copyfile(ROOT / 'docs/guides/examples.html', root / 'docs/guides/examples.html')
            path = root / 'docs/examples/src/provider_session.adb'
            original = path.read_bytes()
            for value in (original + b'-- newer revision\n',
                          (root / 'docs/examples/src/owned_image.adb').read_bytes()):
                path.write_bytes(value)
                with self.assertRaises(ValueError):
                    verify_rendered(self.rendered, root, self.references)
            path.unlink()
            with self.assertRaises(ValueError):
                verify_rendered(self.rendered, root, self.references)

    def test_literal_package_alias_cannot_grant_authority(self):
        text = ('procedure X is S : String := "with AMS.MEL;"; '
                'begin AMS.MEL.Open (X); end X;')
        self.assertEqual(self.records(text), [])

    def test_nested_alias_scope_not_promoted(self):
        text = ('with AMS.MEL.IR.Image; procedure X is package Nested is '
                'package Image renames AMS.MEL.IR.Image; end Nested; '
                'Value : Image.Full_Frame; begin null; end X;')
        self.assertFalse(any(r['identity'] == 'ams.mel.ir.image.full_frame' for r in self.records(text)))

    def test_private_type_and_unsupported_qualification_stay_plain(self):
        text = ('with AMS.MEL; procedure X is '
                'Value : AMS.MEL.Native_Handle; '
                'package M renames Other; begin M.Open (X); end X;')
        self.assertEqual([r['context'] for r in self.records(text)], ['context-package'])

    def test_repeated_support_is_identical(self):
        copies = re.findall(r'<code data-example-source="src/example_support.adb">(.*?)</code>', self.rendered, re.S)
        self.assertEqual(len(copies), 9)
        self.assertEqual(len(set(copies)), 1)

    def test_forged_counts_do_not_authorize(self):
        text = self.rendered.replace('class="ada-reference"', 'class="wrong"', 1)
        text += '<p data-link-count="266">' + json.dumps({'links': 266}) + '</p>'
        with self.assertRaises(ValueError):
            verify_rendered(text, references=self.references)


def markup_mutation(change):
    def test(self):
        mutated = change(self.rendered)
        self.assertNotEqual(mutated, self.rendered, "mutation must actually alter candidate")
        with self.assertRaises(ValueError):
            verify_rendered(mutated, references=self.references)
    return test


def replace_link(text, href):
    return re.sub(r'(<a class="ada-reference" href=")[^"]+', lambda m: m[1] + href, text, count=1)


for name, change in {
    'hidden_text_change': lambda t: t.replace('Provider_Session', 'Different', 1),
    'dropped_token': lambda t: t.replace('<span class="ada-keyword">with</span>', '', 1),
    'duplicated_token': lambda t: t.replace('Provider_Session', 'Provider_Session Provider_Session', 1),
    'reordered_tokens': lambda t: t.replace('Provider_Session', 'Session_Provider', 1),
    'wrong_classification': lambda t: t.replace('class="ada-keyword"', 'class="ada-string"', 1),
    'comment_link': lambda t: re.sub(r'(<span class="ada-comment">)([^<]+)', r'\1<a href="../api/index.html">\2</a>', t, count=1),
    'string_link': lambda t: re.sub(r'(<span class="ada-string">)([^<]+)', r'\1<a href="../api/index.html">\2</a>', t, count=1),
    'wrong_existing_destination': lambda t: replace_link(t, '../api/index.html'),
    'fabricated_anchor': lambda t: replace_link(t, '../api/index.html#' + '0' * 64),
    'external_destination': lambda t: replace_link(t, 'https://example.org/api.html'),
    'encoded_destination': lambda t: replace_link(t, '%2e%2e/api/index.html'),
    'traversal_destination': lambda t: replace_link(t, '../../api/index.html'),
    'nested_link': lambda t: re.sub(r'(<a class="ada-reference"[^>]+>)(.*?)</a>', r'\1<a href="../api/index.html">\2</a></a>', t, count=1),
    'duplicate_link': lambda t: re.sub(r'(<a class="ada-reference"[^>]+>.*?</a>)', r'\1\1', t, count=1),
    'event_handler': lambda t: t.replace('class="ada-reference"', 'class="ada-reference" onclick="x()"', 1),
    'duplicate_attribute': lambda t: t.replace('class="ada-reference"', 'class="ada-reference" class="ada-reference"', 1),
    'html_injection': lambda t: t.replace('Provider_Session', '<img src="x">Provider_Session', 1),
    'script_injection': lambda t: t.replace('Provider_Session', '<script>alert(1)</script>Provider_Session', 1),
    'inline_style': lambda t: t.replace('class="ada-keyword"', 'class="ada-keyword" style="color:red"', 1),
    'malformed_entity': lambda t: t.replace('&quot;', '&quot', 1),
    'alternate_entity': lambda t: t.replace('&quot;', '&#34;', 1),
    'repeated_support_difference': lambda t: t.replace('function</span> Provider_Path', 'procedure</span> Provider_Path', 1),
}.items():
    setattr(AuthorityTests, 'test_mutation_' + name, markup_mutation(change))
