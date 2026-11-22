"""Complete assembled v0.1.7/v0.1.8 compatibility and source-toggle checks."""

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
import source_view_previous as previous_release
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


def compare_releases(before, after, *, plain_pages=None):
    """Compare entire publications; only exact, audited replacements are allowed."""
    metadata = 'api/presentation.json'
    allowed = {metadata}
    epoch.require(set(before) == set(after), 'previous publication inventory differs')
    old, new = (json.loads(tree[metadata]) for tree in (before, after))
    for tree, manifest, version in ((before, old, previous_release.VERSION), (after, new, p.VERSION)):
        serialized = json.dumps(manifest, sort_keys=True, indent=2, ensure_ascii=False) + '\n'
        epoch.require(manifest['enhancer_version'] == version and tree[metadata] == serialized.encode(),
                      'wrong or noncanonical serialized enhancer metadata')
    if plain_pages is not None:
        epoch.require(p.sha(before[source.CSS]) == previous_release.CSS_SHA256 and
                      p.sha(after[source.CSS]) == source.CSS_SHA256, 'release source stylesheet differs')
        for name, expected in plain_pages.items():
            epoch.require(before[name] == expected, 'previous plain source page differs: ' + name)
            epoch.require(source.TOKEN_SPAN.sub(rb'\1', after[name]) == expected,
                          'highlighting did not restore previous source page: ' + name)
        allowed.update(plain_pages)
        allowed.add(source.CSS)
        for name in allowed - {metadata}:
            epoch.require(old['output_sha256'][name] == p.sha(before[name]) and
                          new['output_sha256'][name] == p.sha(after[name]), 'release source output hash differs')
            new['output_sha256'][name] = old['output_sha256'][name]
    new['enhancer_version'] = old['enhancer_version']
    epoch.require(old == new, 'unrelated release presentation metadata changed')
    delta = {n for n in before if before[n] != after[n]}
    epoch.require(metadata in delta and delta <= allowed, 'previous compatibility delta exceeds approved files')
    return dict(passed=True, files=p.hashes(before), delta=sorted(delta))


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
        report_path = Path(temp) / 'report.json'
        # Authored schema-3 inputs hash the inherited native-ledger JSON bytes;
        # retain the measured mapping's insertion order across this process hop.
        report_path.write_text(json.dumps(report))
        rust_files = {n.removeprefix('rust/'): d for n, d in after.items() if n.startswith('rust/')}
        python_files = {n.removeprefix('python/'): d for n, d in after.items() if n.startswith('python/')}

        def assembled(ada):
            return python.assembled(rust.assembled(ada, rust_files), python_files)

        def previous(mode):
            legacy = Path(temp) / ('previous-' + mode)
            subprocess.run([str(previous_python), '-I', str(Path(__file__).with_name('source_view_previous.py')),
                            '--root', str(exported), '--site', str(site), '--report', str(report_path),
                            '--output', str(legacy)], check=True)
            return assembled(p.files(legacy))

        before = assembled(disabled)
        previous_disabled = compare_releases(previous('disabled'), before)
        config_path.write_bytes(original)
        result = compare(exported, site, before, after, authority)
        result['previous_release'] = dict(version=previous_release.VERSION, disabled=previous_disabled,
                                         schema1=compare_releases(previous('schema1'), after,
                                                                  plain_pages=source.source_pages(exported, highlighted=False)))
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
                  comparison='same-source-v0.1.7-v0.1.8-and-source-toggle')
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
