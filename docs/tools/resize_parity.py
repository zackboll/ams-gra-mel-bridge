"""Exact four-file resize delta on one independently verified source revision."""

import argparse
import json
from pathlib import Path
import tempfile

import authored_html
import build_enhanced_site as p
import docs_ci
import python_publication as pythondoc
import rustdoc_publication as rustdoc
import source_epoch as epoch
from check_doc_coverage import measure
from example_sources import References

DELTA = {'api/gnatdoc-resize.css', 'api/gnatdoc-resize.js',
         'api/index.html', 'api/presentation.json'}
CONFIGURATION = 'docs/gnatdoc-enhance.toml'
ENABLED = 'resizable_sidebar = true'
DISABLED = 'resizable_sidebar = false'


def compare(before, after):
    """Compare every byte, including all language subtrees; no projections."""
    changed = {n for n in before.keys() | after.keys() if before.get(n) != after.get(n)}
    epoch.require(changed == DELTA, 'not the exact four-file resize publication delta')
    epoch.require(set(after) - set(before) == DELTA - {'api/index.html', 'api/presentation.json'}
                  and not set(before) - set(after), 'unexpected resize inventory delta')
    return sorted(changed)


def qualify(root, candidate, output, manifest_sha):
    # A seal alone is not authority: verify exact checkout/event identity, native
    # anchors, Ada restoration/search/destinations, complete assembly, Python
    # source reconstruction and independently regenerated Rust bytes first.
    manifest = docs_ci.verify(root, candidate, manifest_sha)
    context = manifest['identity']
    after = p.files(candidate / 'published')
    native = p.files(candidate / 'pristine')
    with tempfile.TemporaryDirectory(prefix='mel-resize-control-') as temp:
        exported, control = Path(temp) / 'source', Path(temp) / 'published'
        epoch.export(root, context['commit'], exported)
        path = exported / CONFIGURATION
        original = path.read_bytes()
        epoch.require(original.count(ENABLED.encode()) == 1 and
                      original.count(DISABLED.encode()) == 0,
                      'resize configuration differs')
        replacement = original.replace(ENABLED.encode(), DISABLED.encode(), 1)
        path.write_bytes(replacement)
        # This modified export is NOT an immutable Git checkout. No docs_ci
        # identity or publication manifest is fabricated for its disabled output.
        site = candidate / 'pristine'
        report = measure(site, exported)
        epoch.enhance(site, exported, report, References(exported, site, ledger=report['native']), control)
        control_files = p.files(control)
        compare(control_files, p.files(candidate / 'ada-qualified'))
        # Reuse only complete language inventories source-verified above. Real
        # assembly inserts both portals; no unrelated mainline bytes are copied.
        rust = {n.removeprefix('rust/'): d for n, d in after.items() if n.startswith('rust/')}
        python = {n.removeprefix('python/'): d for n, d in after.items() if n.startswith('python/')}
        before = pythondoc.assembled(rustdoc.assembled(control_files, rust), python)
        delta = compare(before, after)
        portal = authored_html.manifest(exported)
        authored_names = [portal['home'], portal['stylesheet'], *(g['path'] for g in portal['guides'])]
    # Reject source or payload mutation during disposable reconstruction.
    epoch.require(docs_ci.identity(root) == context, 'source changed during resize qualification')
    payload = p.files(candidate)
    del payload['manifest.json']
    epoch.require(p.hashes(payload) == manifest['payload'], 'candidate changed during resize qualification')
    result = {'passed': True, 'identity': context, 'manifest_sha256': manifest_sha,
              'comparison': 'same-source-resize-toggle',
              'disabled_control': {'mode': 'modified-export-not-deployable',
                  'derived_from': context, 'configuration': CONFIGURATION,
                  'original_sha256': p.sha(original), 'modified_sha256': p.sha(replacement),
                  'ada_files': p.hashes(control_files)},
              'disabled_complete_inventory': p.hashes(before), 'candidate': p.hashes(after),
              'native': p.hashes(native), 'delta': delta,
              'unchanged_files': len(set(before) - DELTA),
              'counts': {'disabled': len(before), 'candidate': len(after),
                         'rust': len(rust), 'python': len(python)},
              'records': {n: p.sha((candidate / n).read_bytes()) for n in
                          ('native-anchors.json', 'coverage.json', 'rustdoc.json', 'python-reference.json')},
              'authored': {n: p.sha(after[n]) for n in authored_names},
              'search_sha256': p.sha(after['api/gnatdoc-search-index.json'])}
    output.write_bytes(epoch.canonical(result))
    print(json.dumps({k: result[k] for k in ('passed', 'identity', 'counts', 'delta', 'unchanged_files')}))
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--manifest-sha', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    qualify(args.root, args.candidate, args.output, args.manifest_sha)
