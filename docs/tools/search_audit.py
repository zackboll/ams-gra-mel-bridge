"""Consumer-owned search authorization, independent of output manifests.

The released renderer reconstructs searchable text. The consumer independently
requires the complete source/native identity set, categories and destinations,
then removes only the exact authorized shell insertions for native restoration.
No output digest, count, or destination can grant itself authority.
"""

import html
import json
import tempfile
from pathlib import Path
from collections import Counter
from dataclasses import replace

import build_enhanced_site as p
import authored_html

EXTRAS = {'api/gnatdoc-search-index.json', 'api/gnatdoc-search.css'}


def qualify(root, site, report, authority, actual):
    from ada_gnatdoc_enhancer.config import load
    from ada_gnatdoc_enhancer.native_html import snapshot
    from ada_gnatdoc_enhancer.search import records, runtime
    from ada_gnatdoc_enhancer.theme import asset
    from ada_gnatdoc_enhancer.util import sorted_json
    from ada_gnatdoc_enhancer.authored import render

    import source_epoch as epoch
    contract = authored_html.contract(root, site, report)
    # Supply a reconstructed manifest in a disposable authority tree; never
    # rewrite checked source or trust a publication-provided manifest.
    with tempfile.TemporaryDirectory(prefix='mel-search-authority-') as temp:
        temporary = Path(temp)
        names = set(contract['inputs']) | {
            path.relative_to(root).as_posix() for path in (root / 'ada/src').glob('*.ads')}
        for name in names:
            path = temporary / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(json.dumps(report['native']).encode() if
                             name == 'docs/gnatdoc-native-anchors.json' else (root / name).read_bytes())
        configuration = 'docs/gnatdoc-enhance.toml'
        (temporary / configuration).write_bytes((root / configuration).read_bytes())
        (temporary / 'docs/gnatdoc-authored.json').write_bytes(epoch.canonical(contract))
        config = replace(load(temporary, configuration),
                         guide_sha256=p.sha(epoch.canonical(contract)))
        p.require(config.search, 'documentation search must be enabled')
        native = snapshot(config, site)
        rendered = render(config, native)
        expected = records(config, native, rendered)
    # Existing consumer reconciliation checks source visibility, full profiles,
    # unique native sections and observed anchors. Do not import the enhancer's
    # declaration walker as the completeness oracle.
    identities = {}
    for page, package in authority.page_packages.items():
        identities[page] = ('package', package)
    for (page, anchor), entry in authority.sections.items():
        kind = entry['identity'].split(': ', 3)[2]
        if kind not in {'type', 'subtype', 'function', 'procedure'}:
            continue
        identities[page + '#' + anchor] = (
            'symbol' if kind in {'type', 'subtype'} else 'operation',
            authority.packages[entry['package']]['name'] + '.' + entry['name'])
    portal = __import__('build_docs_portal').manifest(root)
    for guide in portal['guides']:
        identities['../' + guide['path']] = ('guide', guide['label'])
    observed = {r['href']: (r['kind'], r['name']) for r in expected}
    p.require(len(observed) == len(expected) and observed == identities,
              'search source/native declaration completeness mismatch')
    payload = sorted_json({'schema_version': 1, 'records': expected}).encode()
    p.require(actual['api/gnatdoc-search-index.json'] == payload,
              'search index differs from reconstructed source/native inputs')
    p.require(actual['api/gnatdoc-search.css'] == asset('gnatdoc-search.css'),
              'search stylesheet identity mismatch')
    # Pin the official composition, not a hash supplied by presentation.json.
    router = runtime(asset('gnatdoc-symbol-navigation.js'))
    p.require(actual['api/gnatdoc-navigation.js'] == router,
              'search router identity mismatch')
    links = []
    for record in expected:
        target = ' target="document-content"' if record['kind'] != 'guide' else ''
        links.append('<a href="' + html.escape(record['href'], quote=True) + '"' + target +
                     ' data-search-name="' + html.escape(record['name'], quote=True) +
                     '" data-search-kind="' + record['kind'] + '"></a>')
    insertion = ('<div id="documentation-search-authority" hidden data-schema="1" data-sha256="' +
                 p.sha(payload) + '" data-count="' + str(len(expected)) + '">' +
                 ''.join(links) + '</div>' +
                 '<ul id="documentation-search-results" aria-label="Search results" hidden></ul>')
    return insertion, {'counts': dict(Counter(r['kind'] for r in expected)),
                       'records': len(expected), 'sha256': p.sha(payload)}


def restore(text, insertion):
    style = '<link rel="stylesheet" href="gnatdoc-search.css">'
    for value, following in ((insertion, '<h2 id=compilation-units>'),
                             (style, '</head><body class=main>')):
        p.require(text.count(value) == 1 and text.count(value + following) == 1,
                  'missing/altered/misplaced search shell authority')
        text = text.replace(value, '', 1)
    control = p.NAVIGATION_CONTROL.replace('Filter navigation links', 'Search documentation')
    p.require(text.count(control) == 1, 'search controls changed')
    return text.replace(control, p.NAVIGATION_CONTROL, 1)