"""Complete assembled same-source v0.1.7 disabled/enabled publication delta."""

import argparse
import json
from pathlib import Path
import tempfile
import subprocess

import authored_html
import build_enhanced_site as p
import docs_ci
import python_publication as python
import rustdoc_publication as rust
import source_epoch as epoch
import source_view_audit as source
from check_doc_coverage import measure
from example_sources import References
from local_type_audit import Authority


def compare(root, site, before, after, authority):
    pages, insertions, manifest = source.qualify(root, site, authority, after)
    additions = set(pages) | {source.CSS}
    changed = set(insertions) | {'api/presentation.json'}
    epoch.require(set(after) - set(before) == additions and not set(before) - set(after),
                  'unexpected source publication inventory delta')
    actual_delta = {n for n in before.keys() | after.keys() if before.get(n) != after.get(n)}
    epoch.require(actual_delta == additions | changed, 'unclassified source publication delta')
    for name, record in insertions.items():
        epoch.require(source.restore(after[name].decode(), (site / name).read_text(), record).encode() == before[name],
                      'source toggle changed unrelated declaration bytes')
    old, new = (json.loads(tree['api/presentation.json']) for tree in (before, after))
    epoch.require(new.pop('source_view') == manifest, 'source presentation manifest differs')
    epoch.require(new['counts'].pop('source_pages') == len(pages) and
                  new['counts'].pop('source_links') == sum(len(r[2]) for r in insertions.values()),
                  'source manifest counts differ')
    for name in additions:
        epoch.require(new['output_sha256'].pop(name) == p.sha(after[name]), 'source manifest output differs')
    for name in insertions:
        epoch.require(new['output_sha256'][name] == p.sha(after[name]), 'source API manifest differs')
        new['output_sha256'][name] = p.sha(before[name])
    epoch.require(old == new, 'unrelated presentation manifest change')
    return dict(delta=sorted(actual_delta), source_pages=len(pages),
                source_links=sum(len(rec[2]) for rec in insertions.values()),
                unaffected_files=len(before) - len(changed),
                disabled=p.hashes(before), enabled=p.hashes(after))


def qualify(root, candidate, manifest_sha, output, previous_python):
    seal = docs_ci.verify(root, candidate, manifest_sha)
    context = seal['identity']
    after = p.files(candidate / 'published')
    with tempfile.TemporaryDirectory(prefix='mel-source-control-') as temp:
        exported, control = Path(temp) / 'source', Path(temp) / 'control'
        epoch.export(root, context['commit'], exported)
        site = candidate / 'pristine'
        authority = Authority(exported, site, ledger=json.loads((candidate / 'native-anchors.json').read_bytes()))
        config_path = exported / source.CONFIG
        original = config_path.read_bytes()
        # TOML tables have explicit boundaries; remove only the source opt-in.
        start = original.index(b'[source_view]\n')
        end = original.index(b'[guides]\n', start)
        config_path.write_bytes(original[:start] + original[end:])
        report = measure(site, exported)
        epoch.enhance(site, exported, report, References(exported, site, ledger=report['native']), control)
        disabled = p.files(control)
        legacy = Path(temp) / 'previous'
        report_path = Path(temp) / 'report.json'
        # Authored schema-3 inputs hash the inherited native-ledger JSON bytes;
        # retain the measured mapping's insertion order across this process hop.
        report_path.write_text(json.dumps(report))
        subprocess.run([str(previous_python), str(Path(__file__).with_name('source_view_previous.py')),
                        '--root', str(exported), '--site', str(site), '--report', str(report_path),
                        '--output', str(legacy)], check=True)
        previous = p.files(legacy)
        epoch.require(set(previous) == set(disabled), 'previous publication inventory differs')
        epoch.require({n for n in previous if previous[n] != disabled[n]} == {'api/presentation.json'},
                      'previous compatibility delta exceeds enhancer metadata')
        old_manifest = json.loads(previous['api/presentation.json'])
        new_manifest = json.loads(disabled['api/presentation.json'])
        epoch.require(old_manifest.pop('enhancer_version') == '0.1.6' and
                      new_manifest.pop('enhancer_version') == '0.1.7' and old_manifest == new_manifest,
                      'previous compatibility metadata differs')
        rust_files = {n.removeprefix('rust/'): d for n, d in after.items() if n.startswith('rust/')}
        python_files = {n.removeprefix('python/'): d for n, d in after.items() if n.startswith('python/')}
        before = python.assembled(rust.assembled(disabled, rust_files), python_files)
        config_path.write_bytes(original)
        result = compare(exported, site, before, after, authority)
        result['previous_release'] = dict(passed=True, files=p.hashes(previous),
                                         delta=['api/presentation.json:enhancer_version'])
        portal = authored_html.manifest(exported)
        names = [portal['home'], portal['stylesheet'], *(g['path'] for g in portal['guides'])]
        result['authored'] = {n: p.sha(after[n]) for n in names}
        result['rust'] = p.hashes(rust_files)
        result['python'] = p.hashes(python_files)
    epoch.require(docs_ci.identity(root) == context, 'source changed during comparison')
    payload = p.files(candidate)
    del payload['manifest.json']
    epoch.require(p.hashes(payload) == seal['payload'], 'candidate changed during comparison')
    result.update(passed=True, identity=context, manifest_sha256=manifest_sha,
                  comparison='same-source-v0.1.7-source-toggle')
    output.write_bytes(epoch.canonical(result))
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--manifest-sha', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--previous-python', type=Path, required=True)
    args = parser.parse_args()
    result = qualify(args.root, args.candidate, args.manifest_sha, args.output, args.previous_python)
    print(json.dumps({k: result[k] for k in ('passed', 'source_pages', 'source_links', 'unaffected_files')}))