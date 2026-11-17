"""Complete publication parity and disposable resize-disabled release control."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import tomllib

import authored_html
import build_enhanced_site as p
import python_publication as pythondoc
import rustdoc_publication as rustdoc
import source_epoch as epoch
import feature_source as source
from check_doc_coverage import measure
from example_sources import References

DELTA = {'api/gnatdoc-resize.css', 'api/gnatdoc-resize.js',
         'api/index.html', 'api/presentation.json'}


def qualify_delta(root, baseline, candidate, output):
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


def verify_artifact(root, directory, *, baseline=False):
    """Run the unchanged normal verifier against each artifact's exact Git source.

    The main baseline is deliberately local/non-deployable; the candidate keeps
    the real hosted event environment. Neither artifact can authorize a different
    source epoch, and malformed bytes are not excused as ordinary API changes.
    """
    manifest = directory / 'manifest.json'
    value = json.loads(manifest.read_bytes())
    commit = source.revision(root, value['identity']['commit'])
    tree = source.git(root, 'rev-parse', commit + '^{tree}').decode().strip()
    epoch.require(value['identity']['tree'] == tree,
                  'resize artifact source tree differs')
    environment = dict(os.environ)
    if baseline:
        epoch.require(value['identity']['mode'] == 'local-not-deployable',
                      'resize baseline must be non-deployable')
        environment['GITHUB_ACTIONS'] = 'false'
    else:
        epoch.require(commit == source.revision(root, 'HEAD'),
                      'resize candidate source epoch differs')
    with tempfile.TemporaryDirectory(prefix='mel-resize-source-') as temp:
        checkout = Path(temp) / 'source'
        subprocess.run(['git', 'clone', '-q', '--shared', '--no-checkout', str(root), str(checkout)],
                       check=True)
        subprocess.run(['git', '-C', str(checkout), 'checkout', '-q', '--detach', commit], check=True)
        subprocess.run([sys.executable, str(root / 'docs/tools/docs_ci.py'), 'verify',
                        '--root', str(checkout), '--directory', str(directory),
                        '--manifest-sha', epoch.sha(manifest.read_bytes())],
                       env=environment, check=True)
    return commit


def qualify(root, baseline, candidate, output):
    root, baseline, candidate = (Path(p).absolute() for p in (root, baseline, candidate))
    baseline_commit = verify_artifact(root, baseline, baseline=True)
    candidate_commit = verify_artifact(root, candidate)
    config = tomllib.loads(source.blob(root, baseline_commit,
                                      'docs/gnatdoc-enhance.toml').decode())
    if not config.get('navigation', {}).get('resizable_sidebar', False):
        # Original resize feature introduction: retain the strict cross-commit
        # four-file comparison. This is not a fallback after a failed check.
        return qualify_delta(root, baseline, candidate, output)

    # Once resize is on main, ordinary API additions legitimately change native
    # pages and all three language publications. Compare resize on/off at the
    # SAME candidate source, never require those additions to disappear or
    # ignore their bytes. Both original publications were verified above.
    with tempfile.TemporaryDirectory(prefix='mel-resize-same-source-') as temp:
        temp = Path(temp)
        exported, control = temp / 'source', temp / 'control'
        epoch.export(root, candidate_commit, exported)
        path = exported / 'docs/gnatdoc-enhance.toml'
        text = path.read_text()
        epoch.require(text.count('resizable_sidebar = true') == 1, 'resize configuration differs')
        path.write_text(text.replace('resizable_sidebar = true', 'resizable_sidebar = false'))
        site = candidate / 'pristine'
        report = measure(site, exported)
        control.mkdir()
        epoch.enhance(site, exported, report, References(exported, site, ledger=report['native']),
                      control / 'ada-qualified')
        combined = pythondoc.assembled(
            rustdoc.assembled(p.files(control / 'ada-qualified'),
                {n.removeprefix('rust/'): d for n, d in p.files(candidate / 'published').items()
                 if n.startswith('rust/')}),
            {n.removeprefix('python/'): d for n, d in p.files(candidate / 'published').items()
             if n.startswith('python/')})
        for name, data in combined.items():
            path = control / 'published' / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        shutil.copytree(site, control / 'pristine')
        for name in ('native-anchors.json', 'coverage.json'):
            shutil.copyfile(candidate / name, control / name)
        result = qualify_delta(root, control, candidate, output)
        result['comparison'] = 'same-source-resize-disabled-control'
        result['verified_baseline_commit'] = baseline_commit
        result['candidate_commit'] = candidate_commit
        result['verified_baseline_publication'] = p.hashes(p.files(baseline / 'published'))
        output.write_bytes(epoch.canonical(result))
        return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    qualify(args.root, args.baseline, args.candidate, args.output)
