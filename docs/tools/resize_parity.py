"""Complete publication parity and disposable resize-disabled release control."""

import argparse
import json
from pathlib import Path
import tempfile

import authored_html
import build_enhanced_site as p
import python_publication as pythondoc
import rustdoc_publication as rustdoc
import source_epoch as epoch
from check_doc_coverage import measure
from example_sources import References

DELTA = {'api/gnatdoc-resize.css', 'api/gnatdoc-resize.js',
         'api/index.html', 'api/presentation.json'}


def qualify(root, baseline, candidate, output):
    before, after = (p.files(d / 'published') for d in (baseline, candidate))
    native_before, native_after = (p.files(d / 'pristine') for d in (baseline, candidate))
    epoch.require(native_before == native_after, 'source/native or authored baseline differs')
    for name in ('native-anchors.json', 'coverage.json'):
        epoch.require((baseline / name).read_bytes() == (candidate / name).read_bytes(),
                      'source/native identity differs: ' + name)
    changed = {n for n in before.keys() | after.keys() if before.get(n) != after.get(n)}
    epoch.require(changed == DELTA, 'not the exact four-file resize publication delta')
    epoch.require(set(after) - set(before) == DELTA - {'api/index.html', 'api/presentation.json'}
                  and not set(before) - set(after), 'unexpected inventory delta')
    # Independently re-render the current immutable source with only resize off.
    with tempfile.TemporaryDirectory(prefix='mel-resize-control-') as temp:
        exported, control = Path(temp) / 'source', Path(temp) / 'published'
        epoch.export(root, 'HEAD', exported)
        path = exported / 'docs/gnatdoc-enhance.toml'
        text = path.read_text()
        epoch.require(text.count('resizable_sidebar = true') == 1, 'resize configuration differs')
        path.write_text(text.replace('resizable_sidebar = true', 'resizable_sidebar = false'))
        site = candidate / 'pristine'
        report = measure(site, exported)
        epoch.enhance(site, exported, report, References(exported, site, ledger=report['native']), control)
        control_files = p.files(control)
        # Assembly deliberately adds language portal links to index.html. Compare
        # the isolated Ada control first, then run the real assembly functions.
        expected_ada = p.files(baseline / 'ada-qualified')
        epoch.require(control_files == expected_ada, 'resize-disabled control differs from production')
        portal = authored_html.manifest(exported)
        authored_names = [portal['home'], portal['stylesheet'], *(g['path'] for g in portal['guides'])]
    result = {'passed': True, 'baseline': p.hashes(before), 'candidate': p.hashes(after),
              'native': p.hashes(native_after), 'delta': sorted(changed),
              'unchanged_files': len(set(before) - changed),
              'counts': {'baseline': len(before), 'candidate': len(after),
                         'baseline_ada': len(expected_ada),
                         'candidate_ada': len(p.files(candidate / 'ada-qualified')),
                         'rust': sum(n.startswith('rust/') for n in after),
                         'python': sum(n.startswith('python/') for n in after)},
              'authored': {n: p.sha(after[n]) for n in authored_names},
              'search_sha256': p.sha(after['api/gnatdoc-search-index.json']),
              'disabled_control': p.hashes(control_files),
              'disabled_complete_inventory': p.hashes(pythondoc.assembled(
                  rustdoc.assembled(control_files,
                      {n.removeprefix('rust/'): d for n, d in after.items() if n.startswith('rust/')}),
                  {n.removeprefix('python/'): d for n, d in after.items() if n.startswith('python/')}))}
    epoch.require(result['disabled_complete_inventory'] == result['baseline'],
                  'complete disabled assembly differs')
    output.write_bytes(epoch.canonical(result))
    print(json.dumps({k: result[k] for k in ('passed', 'counts', 'delta', 'unchanged_files')}))
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    qualify(args.root, args.baseline, args.candidate, args.output)
