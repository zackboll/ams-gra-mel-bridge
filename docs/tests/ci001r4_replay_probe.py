"""CI-001R3-R4 fixture probe, executed inside a corrected-replay checkout.

Run with ``python -P`` so this file's own directory never shadows the replay
modules. Every imported test/publication module must resolve inside the replay
checkout (asserted below). The probe executes the unchanged historical test
bodies and both api_page() helpers taken from immutable Git blobs; it only
controls Path.glob enumeration order to simulate unfavorable filesystems.
Results are evidence for the parent controls, never qualification authority.
"""

import ast
import contextlib
import copy
import json
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

REPO, OUTPUT, HISTORY = Path(sys.argv[1]).resolve(), Path(sys.argv[2]), sys.argv[3]
ROOT_PAGE = '5f9de8789b28ed02de7bf3d4f6aa6447445b50a3c927e5e3bb132a2c8efb1779.html'
ANCHOR = r'<h4 id=[a-f0-9]{64}>'
LINK = r'<a class="type-reference"[^>]*>(.*?)</a>'
sys.path[:0] = [str(REPO / 'docs/tests'), str(REPO / 'docs/tools'), str(REPO / 'scripts')]
import test_live_docs_site as t  # noqa: E402

p = t.live.publication
for module in (t, t.live, p):
    assert Path(module.__file__).resolve().is_relative_to(REPO), module.__file__
result = {'modules': {m.__name__: str(Path(m.__file__).resolve().relative_to(REPO)) for m in (t, t.live, p)}}
authority = p.Authority(REPO, REPO / '_site')
required = sorted(authority.required_additions(), key=p.occurrence_key)
work = Path(tempfile.mkdtemp(prefix='ci001r4-probe-'))
approved = work / 'approved'
p.build(REPO / '_site', approved)
result['fingerprints'] = {'pristine': p.fingerprint(p.hashes(p.files(REPO / '_site'))) == p.PRISTINE_SHA256,
                          'published': p.fingerprint(p.hashes(p.files(approved))) == p.PUBLICATION_SHA256,
                          'files': [len(p.files(REPO / '_site')), len(p.files(approved))]}


def fresh():
    site = work / 'project'
    if site.exists():
        shutil.rmtree(site)
    shutil.copytree(approved, site)
    return site


@contextlib.contextmanager
def ordered(first=None, reverse=False):
    original = Path.glob
    def glob(path, pattern, **kwargs):
        items = sorted(original(path, pattern, **kwargs), reverse=reverse)
        if first is not None:
            items.sort(key=lambda item: item.name != first)
        return iter(items)
    with mock.patch.object(Path, 'glob', glob):
        yield


def helper(revision):
    content = subprocess.check_output(['git', '-C', str(REPO), 'show', revision + ':docs/tests/test_live_docs_site.py'])
    cls = next(n for n in ast.parse(content).body if isinstance(n, ast.ClassDef) and n.name == 'QualifiedArtifactTests')
    node = next(n for n in cls.body if isinstance(n, ast.FunctionDef) and n.name == 'api_page')
    namespace = dict(vars(t))
    exec(compile(ast.Module(body=[node], type_ignores=[]), revision + ':api_page', 'exec'), namespace)
    return namespace['api_page']


def case(name, site):
    value = t.QualifiedArtifactTests(name)
    value.site = site
    return value


def outcome(function):
    try:
        function()
    except BaseException as error:  # Recorded verbatim; parents assert exact types.
        return [type(error).__name__, str(error), isinstance(error, ValueError)]
    return ['passed', '', False]


def counts(text):
    return [re.subn(ANCHOR, '<h4 id=wrong>', text, count=1)[1], re.subn(LINK, r'\1', text, count=1)[1]]


site = fresh()
pages = {path.name: counts(path.read_text()) for path in sorted((site / 'api').glob('*.html')) if path.name != 'index.html'}
result['root'] = {'package': authority.page_packages.get(ROOT_PAGE), 'counts': pages[ROOT_PAGE],
                  'sections': sum(k[0] == ROOT_PAGE for k in authority.sections),
                  'required': sum(r['containing']['page'] == ROOT_PAGE for r in required),
                  'pages': len(pages), 'eligible': sorted(n for n, c in pages.items() if c == [1, 1])}
original_helper = helper(HISTORY)
result['original_root_first'] = {}
for name in ('test_wrong_native_anchor', 'test_missing_required_type_link'):
    site = fresh()
    with ordered(first=ROOT_PAGE), mock.patch.object(t.QualifiedArtifactTests, 'api_page', original_helper):
        result['original_root_first'][name] = outcome(getattr(case(name, site), name))
site = fresh()
selected = {}
for first, reverse in ((None, False), (None, True), (ROOT_PAGE, False), (ROOT_PAGE, True)):
    with ordered(first, reverse):
        selected[str((first, reverse))] = case('test_wrong_native_anchor', site).api_page().name
record = required[0]
entry = record['containing']
native = (REPO / '_site/api' / entry['page']).read_text()
restored, audit = p.restore_page(entry['page'], p.restore_document(
    (site / 'api' / entry['page']).read_text(), native, 'api/' + entry['page'], authority), authority)
result['selection'] = {'orders': selected, 'expected': entry['page'], 'anchor': entry['anchor'],
                       'visible': record['visible'], 'href': record['href'],
                       'section_bound': authority.sections[(entry['page'], entry['anchor'])] == entry,
                       'native_anchor_count': native.count('<h4 id=' + entry['anchor'] + '>'),
                       'occurrence_in_audit': record in audit, 'restored_native': restored == native,
                       'counts': counts((site / 'api' / entry['page']).read_text())}
result['unchanged_tests'] = {}
for name in ('test_wrong_native_anchor', 'test_missing_required_type_link'):
    for first, reverse in ((None, False), (None, True), (ROOT_PAGE, False), (ROOT_PAGE, True)):
        site = fresh()
        with ordered(first, reverse):
            result['unchanged_tests'][name + str((first, reverse))] = outcome(getattr(case(name, site), name))
site = fresh()
path = case('test_wrong_native_anchor', site).api_page()
text = path.read_text()
result['validator'] = {'unchanged': outcome(lambda: t.live.qualified(site, REPO / '_site'))}
for label, changed in (('anchor', re.sub(ANCHOR, '<h4 id=wrong>', text, count=1)),
                       ('link', re.sub(LINK, r'\1', text, count=1))):
    path.write_text(changed)
    result['validator'][label] = outcome(lambda: t.live.qualified(site, REPO / '_site'))
    path.write_text(text)
fabrications = {'missing': []}
# The helper sorts by occurrence_key, so fabricate *every* trusted occurrence;
# a single altered record could otherwise sort behind an authentic one.
for label, mutate in (('root-page', lambda r: r['containing'].update(page=ROOT_PAGE)),
                      ('unknown-anchor', lambda r: r['containing'].update(anchor='0' * 64)),
                      ('shifted-occurrence', lambda r: r.update(native_token_start=r['native_token_start'] + 1)),
                      ('wrong-target', lambda r: r.update(href=ROOT_PAGE + '#' + '0' * 64))):
    value = copy.deepcopy(required)
    for record in value:
        mutate(record)
    fabrications[label] = value
result['fabricated'] = {}
for label, value in fabrications.items():
    site = fresh()
    with mock.patch.object(p.Authority, 'required_additions', return_value=value):
        result['fabricated'][label] = outcome(case('test_wrong_native_anchor', site).api_page)
suite = unittest.TestSuite(t.QualifiedArtifactTests(n) for n in ('test_wrong_native_anchor', 'test_missing_required_type_link'))
with ordered(first=ROOT_PAGE):
    run = unittest.TextTestRunner(stream=open('/dev/null', 'w')).run(suite)
result['runner_root_first'] = {'run': run.testsRun, 'failures': len(run.failures), 'errors': len(run.errors)}
shutil.rmtree(work)
OUTPUT.write_text(json.dumps(result, indent=2, sort_keys=True) + '\n')
