"""Single-generation current-Git documentation, sealed and verified before Pages.

No historical source admission, warning baseline, or coverage ratchet. Local
builds are explicitly non-deployable. Artifact digests come from job outputs,
not from an archive's claims about itself.
"""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import tarfile
import tempfile

import feature_source as source
import source_epoch as epoch
import build_enhanced_site as presentation
import rustdoc_publication as rustdoc

ROOT = Path(__file__).resolve().parents[2]
require = source.require


def write(path, value):
    path.write_bytes(epoch.canonical(value))


def identity(root):
    commit = source.revision(root, 'HEAD')
    source.exact_checkout(root, commit, source.tree(root, commit))
    result = {'commit': commit, 'tree': source.git(root, 'rev-parse', 'HEAD^{tree}').decode().strip(),
              'mode': 'local-not-deployable'}
    if os.environ.get('GITHUB_ACTIONS') == 'true':
        event = json.loads(Path(os.environ['GITHUB_EVENT_PATH']).read_bytes())
        kind = os.environ['GITHUB_EVENT_NAME']
        repository = os.environ['GITHUB_REPOSITORY']
        require(event['repository']['full_name'] == repository, 'event repository mismatch')
        if kind == 'pull_request':
            require(event['pull_request']['base']['ref'] == 'main' and
                    event['pull_request']['base']['repo']['full_name'] == repository,
                    'untrusted PR destination')
            expected = event['pull_request']['head']['sha']
            require(os.environ['GITHUB_REF'] == f"refs/pull/{event['number']}/merge", 'PR ref mismatch')
        else:
            require(kind in {'push', 'workflow_dispatch', 'schedule'}, 'unsupported event')
            expected = os.environ['GITHUB_SHA']
            if kind == 'push':
                require(event['after'] == expected and event['ref'] == os.environ['GITHUB_REF'],
                        'push event mismatch')
        require(commit == expected == os.environ['DOC_SOURCE_SHA'], 'event source mismatch')
        result.update(mode='hosted', event=kind, ref=os.environ['GITHUB_REF'],
                      repository=repository, run_id=os.environ['GITHUB_RUN_ID'],
                      run_attempt=os.environ['GITHUB_RUN_ATTEMPT'])
    return result


def output(**values):
    if os.environ.get('GITHUB_OUTPUT'):
        with open(os.environ['GITHUB_OUTPUT'], 'a') as stream:
            for name, value in values.items():
                stream.write(f'{name}={value}\n')


def build(root, directory, prefix):
    context = identity(root)
    require(not directory.exists() and not any(p.is_symlink() for p in (directory, *directory.parents)),
            'output must be fresh with no symlink ancestors')
    directory.mkdir(parents=True)
    epoch.toolchain(prefix)
    with tempfile.TemporaryDirectory(prefix='docs-current-') as temp:
        exported = Path(temp) / 'source'
        site, report, references = epoch.generate(root, context['commit'], exported, prefix,
                                                  directory / 'generation.log')
        epoch.enhance(site, exported, report, references, Path(temp) / 'published', deep=False)
        shutil.copytree(site, directory / 'pristine')
        shutil.copytree(Path(temp) / 'published', directory / 'ada-qualified')
        rust_files, rust_record = rustdoc.generate(exported, Path(temp) / 'rust-generation',
                                                  context, directory / 'rustdoc.log')
        combined = rustdoc.assembled(presentation.files(directory / 'ada-qualified'), rust_files)
        for name, data in combined.items():
            target = directory / 'published' / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        write(directory / 'rustdoc.json', rust_record)
        for name in ('coverage.json', 'warnings.json', 'native-anchors.json', 'gnatdoc.log'):
            shutil.copyfile(exported / 'obj/docs' / name, directory / name)
    manifest = {'schema': 1, 'identity': context,
                'payload': presentation.hashes(presentation.files(directory))}
    write(directory / 'manifest.json', manifest)
    manifest_sha = epoch.sha((directory / 'manifest.json').read_bytes())
    verify(root, directory, manifest_sha)
    bundle = directory.parent / (directory.name + '.tar')
    require(not bundle.exists() and not bundle.is_symlink(), 'bundle must be fresh')
    with tarfile.open(bundle, 'w') as archive:
        for name in presentation.files(directory):
            archive.add(directory / name, arcname=name, recursive=False)
    output(bundle_sha=epoch.sha(bundle.read_bytes()), manifest_sha=manifest_sha)
    warning_report = json.loads((directory / 'warnings.json').read_bytes())
    warning_summary = (str(warning_report['raw_total']) if 'raw_total' in warning_report
                       else 'classification unavailable; raw diagnostics retained')
    summary = (f"### Documentation (current Git source)\n\nCommit: `{context['commit']}`\n\n"
               f"Rustdoc {rustdoc.TOOLCHAIN}: {rust_record['counts']['html']} HTML, "
               f"{rust_record['counts']['assets']} assets, {rust_record['seconds']:.2f}s. "
               "Locked/offline; no native facade or doctests.\n\n"
               f"Coverage: {report['documented_elements']}/{report['total_elements']} "
               f"({report['documented_percentage']}%). Informational; no completeness threshold.\n\n"
               f"GNATdoc warnings: {warning_summary}. Informational; see `gnatdoc.log` and "
               f"`warnings.json` in the diagnostic artifact.\n")
    print(summary)
    if os.environ.get('GITHUB_STEP_SUMMARY'):
        with open(os.environ['GITHUB_STEP_SUMMARY'], 'a') as stream:
            stream.write(summary)
    return manifest


def verify(root, directory, manifest_sha, deployment=False):
    context = identity(root)
    raw = (directory / 'manifest.json').read_bytes()
    require(epoch.sha(raw) == manifest_sha, 'manifest digest mismatch')
    manifest = json.loads(raw)
    require(raw == epoch.canonical(manifest), 'noncanonical/duplicate manifest entries')
    require(manifest['schema'] == 1 and manifest['identity'] == context, 'source/run identity mismatch')
    actual = presentation.files(directory)
    del actual['manifest.json']
    require(presentation.hashes(actual) == manifest['payload'], 'artifact payload mismatch')
    if deployment:
        require(context['mode'] == 'hosted' and context['event'] == 'push' and
                context['ref'] == 'refs/heads/main', 'only same-run main pushes may deploy')
    with tempfile.TemporaryDirectory(prefix='docs-verify-') as temp:
        exported = Path(temp) / 'source'
        epoch.export(root, context['commit'], exported)
        from check_doc_coverage import measure
        from validate_docs import validate
        from example_sources import References
        ledger = json.loads((directory / 'native-anchors.json').read_bytes())
        report = measure(directory / 'pristine', exported, ledger)
        require(report == json.loads((directory / 'coverage.json').read_bytes()), 'coverage evidence mismatch')
        validate(directory / 'pristine', exported, ledger)
        references = References(exported, directory / 'pristine', ledger=ledger)
        epoch.verify_presentation(directory / 'pristine', exported, report, references,
                                  directory / 'ada-qualified')
        rust_raw = (directory / 'rustdoc.json').read_bytes()
        rust_record = json.loads(rust_raw)
        require(rust_raw == epoch.canonical(rust_record), 'noncanonical/duplicate Rustdoc record')
        rustdoc.verify_assembly(directory, context, rust_record)
        rust_files = {n.removeprefix('rust/'): d for n, d in
                      presentation.files(directory / 'published').items() if n.startswith('rust/')}
        rustdoc.verify_regenerated(rust_files, rust_record, exported, Path(temp) / 'rust-generation',
                                  context, Path(temp) / 'rustdoc.log')
    return manifest


def unpack(bundle, directory, bundle_sha, manifest_sha):
    require(epoch.sha(bundle.read_bytes()) == bundle_sha, 'bundle digest mismatch')
    require(not directory.exists() and not any(p.is_symlink() for p in (directory, *directory.parents)),
            'unpack destination must be fresh with no symlink ancestors')
    with tarfile.open(bundle) as archive:
        members = archive.getmembers()
        names = [member.name for member in members]
        require(len(names) == len(set(names)), 'duplicate archive member')
        for member in members:
            name = member.name
            require(member.isfile() and not member.issym() and not member.islnk() and
                    re.fullmatch(r'[A-Za-z0-9_.\-/]+', name) and
                    not name.startswith('/') and all(p not in {'', '.', '..'} for p in name.split('/')),
                    'unsafe archive member')
        require('manifest.json' in names, 'missing archive manifest')
        raw = archive.extractfile('manifest.json').read()
        require(epoch.sha(raw) == manifest_sha, 'archive manifest mismatch')
        manifest = json.loads(raw)
        require(raw == epoch.canonical(manifest), 'noncanonical/duplicate manifest entries')
        require(set(names) == set(manifest['payload']) | {'manifest.json'}, 'archive inventory mismatch')
        directory.mkdir(parents=True)
        for member in members:
            target = directory / member.name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(archive.extractfile(member).read())


def live(directory, base, evidence):
    from check_live_docs_site import base_url, file_url, get_entity, graph
    from concurrent.futures import ThreadPoolExecutor
    import time
    base = base_url(base)
    files = presentation.files(directory / 'published')
    # Run the unchanged active-content/anchor policy on the isolated Ada bytes.
    graph(presentation.files(directory / 'ada-qualified'))
    rustdoc.validate({n.removeprefix('rust/'): d for n, d in files.items() if n.startswith('rust/')})
    attempts = []
    for _ in range(10):
        def fetch(item):
            name, data = item
            return get_entity(file_url(base, name), base, data, rustdoc=name.startswith('rust/'))[0]
        # Bounded parallel GETs retain exhaustive ordered evidence and all transport
        # checks. No external documentation link is fetched; no redirects added.
        with ThreadPoolExecutor(max_workers=8) as pool:
            attempts.append(list(pool.map(fetch, files.items())))
        if all(item['comparison'] for item in attempts[-1]):
            require(presentation.files(directory / 'published') == files, 'artifact changed during HTTP verification')
            write(evidence, {'successful': True, 'attempts': attempts})
            return
        time.sleep(30)
    write(evidence, {'successful': False, 'attempts': attempts})
    raise ValueError('live publication differs from verified artifact')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('build', 'verify', 'unpack', 'live', 'qualify'))
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--directory', type=Path, default=ROOT / '.doc-tools/current')
    parser.add_argument('--gnatdoc-prefix', type=Path)
    parser.add_argument('--bundle', type=Path)
    parser.add_argument('--bundle-sha')
    parser.add_argument('--manifest-sha')
    parser.add_argument('--deployment', action='store_true')
    parser.add_argument('--base-url')
    parser.add_argument('--evidence', type=Path)
    args = parser.parse_args()
    if args.command == 'build':
        build(args.root, args.directory, args.gnatdoc_prefix)
    elif args.command == 'unpack':
        unpack(args.bundle, args.directory, args.bundle_sha, args.manifest_sha)
        verify(args.root, args.directory, args.manifest_sha, args.deployment)
    elif args.command in {'verify', 'live'}:
        verify(args.root, args.directory, args.manifest_sha, args.deployment)
        if args.command == 'live':
            live(args.directory, args.base_url, args.evidence)
    else:
        context = identity(args.root)
        epoch.toolchain(args.gnatdoc_prefix)
        result = epoch.qualify_evidence(args.root, {'candidate_commit': context['commit'],
                                                   'target_commit': context['commit']},
                                         args.directory, args.gnatdoc_prefix)
        print(json.dumps({'publication_files': len(result['publication']['files'])}))


if __name__ == '__main__':
    main()
