"""Non-deploying MEL-GNATDOC-015-002 compatibility probe for official v0.1.5.

Never substitutes a consumer renderer for an unsupported enhancer feature.
The candidate directory contains only actually accepted authored output; missing
pages remain missing. The complete production publication is independently
reconstructed before comparisons, without using the authored contract as approval.
"""

import argparse
from dataclasses import replace
import json
from pathlib import Path
import shutil
import tempfile

import build_enhanced_site as presentation
import feature_source
import source_epoch
from ada_examples import References
import api_catalog
import example_sources
from build_docs_portal import GROUPS, group, manifest, package_map, regular, render
from check_doc_coverage import measure
from gnatdoc_inventory import specifications
from validate_docs import validate


def contract(root, site, report):
    """Derive every field from source and independently reconciled native output."""
    data = manifest(root)
    packages = package_map(site, root, report['native'])
    paths = [data['home'], data['stylesheet'], *(g['path'] for g in data['guides'])]
    inputs = ['docs/' + p for p in paths]
    inputs += ['docs/site-map.json', 'docs/gnatdoc-native-anchors.json']
    return dict(
        schema_version=2,
        home=dict(source='docs/' + data['home'], path=data['home']),
        stylesheet=dict(source='docs/' + data['stylesheet'], path=data['stylesheet']),
        guides=[dict(g, source='docs/' + g['path'], entry_points=[]) for g in data['guides']],
        inputs={p: presentation.sha((root / p).read_bytes()) for p in inputs},
        native_sha256=presentation.hashes({n: b for n, b in presentation.files(site).items()
                                          if n.startswith('api/')}),
        site_map='docs/site-map.json', anchors='docs/gnatdoc-native-anchors.json',
        groups=[dict(label=label, packages=[p for p in packages if group(p) == label])
                for label in GROUPS],
        examples=[], languages=[], source_links={}, attribution_base='index.html',
        title_suffix='AMS MEL Bridge', api_label='Complete API Reference',
        source_inventory=None, landing_example=None, ada_policy='source-text',
        example_project=None, landing_project=None,
        directory_pages=['index.html', 'guides/api-map.html'],
        renderer_exceptions=['AMS.MEL.IR.Health_Status.Metadata.Status'])


def bind(root, data):
    from ada_gnatdoc_enhancer.config import load
    payload = source_epoch.canonical(data)
    (root / 'docs/gnatdoc-authored.json').write_bytes(payload)
    original = (root / 'docs/gnatdoc-enhance.toml').read_text()
    # Called only on a disposable export; never writes production configuration.
    config = original.replace('mode = "none"', 'mode = "authored-html"')
    config += ('manifest = "docs/gnatdoc-authored.json"\nsha256 = "'
               + presentation.sha(payload) + '"\n')
    (root / 'docs/gnatdoc-authored-pilot.toml').write_text(config)
    return load(root, 'docs/gnatdoc-authored-pilot.toml')


def independent_contract(root, site, report, supplied, source_bytes):
    # Source bytes originate in an exact Git export, not the supplied contract.
    for name, expected in source_bytes.items():
        path = root / name
        source_epoch.require(not any(p.is_symlink() for p in (path, *path.parents))
                             and path.is_file() and path.read_bytes() == expected,
                             'pilot source differs from Git: ' + name)
    source_epoch.require(supplied == contract(root, site, report),
                         'pilot contract differs from independently derived authority')


def negative_cases(root, site, report, references, config, native, data, source_bytes):
    """Actual fail-closed checks; resealing never makes substituted input trusted."""
    from ada_gnatdoc_enhancer import authored
    from ada_gnatdoc_enhancer.errors import EnhancementError
    results = {}

    def reject(name, action):
        try:
            action()
        except (ValueError, OSError, EnhancementError) as error:
            results[name] = dict(rejected=True, reason=str(error))
        else:
            raise ValueError('negative case accepted: ' + name)

    def reseal(changed):
        payload = source_epoch.canonical(changed)
        (root / 'docs/gnatdoc-authored.json').write_bytes(payload)
        return replace(config, guide_sha256=presentation.sha(payload))

    reject('manipulated-contract-digest', lambda: authored.manifest(
        replace(config, guide_sha256='0' * 64), native))
    changed = json.loads(json.dumps(data))
    changed['guides'][1] = changed['guides'][0]
    reject('duplicate-guide-identity', lambda: authored.manifest(reseal(changed), native))
    changed = json.loads(json.dumps(data))
    changed['groups'][0]['packages'].append(changed['groups'][0]['packages'][0])
    reject('resealed-group-destination', lambda: independent_contract(
        root, site, report, changed, source_bytes))
    changed = json.loads(json.dumps(data))
    changed['native_sha256']['api/index.html'] = '0' * 64
    reject('resealed-native-digest', lambda: authored.manifest(reseal(changed), native))
    reseal(data)
    path = root / 'docs/guides/architecture.html'
    original = path.read_bytes()
    try:
        path.unlink()
        reject('missing-guide-source', lambda: authored.manifest(config, native))
        path.symlink_to(root / 'docs/index.html')
        reject('symlink-guide-source', lambda: authored.manifest(config, native))
        path.unlink()
        path.write_bytes(original + b'\n<p>substituted source</p>\n')
        changed = contract(root, site, report)
        forged = reseal(changed)
        authored.manifest(forged, native)  # Hashes alone allow this substitution.
        reject('resealed-substituted-source', lambda: independent_contract(
            root, site, report, changed, source_bytes))
    finally:
        if path.is_symlink():
            path.unlink()
        path.write_bytes(original)
        reseal(data)
    ledger_path = root / 'docs/gnatdoc-native-anchors.json'
    ledger_bytes = ledger_path.read_bytes()
    try:
        forged = json.loads(ledger_bytes)
        first = next(iter(forged.values()))
        first['page'] = next(v['page'] for v in forged.values() if v['page'] != first['page'])
        ledger_path.write_bytes(source_epoch.canonical(forged))
        changed = contract(root, site, report)
        reject('resealed-altered-anchor-destination', lambda: authored.manifest(reseal(changed), native))
    finally:
        ledger_path.write_bytes(ledger_bytes)
        reseal(data)
    reject('unsafe-uri', lambda: authored.safe_uri('javascript:alert(1)', config))
    for name, text in {
        'unsafe-script': '<script>alert(1)</script>',
        'unsafe-event': '<p onclick="alert(1)">bad</p>',
        'arbitrary-expansion': '{{execute:sh:whoami}}',
    }.items():
        reject(name, (lambda text=text: authored.portal(text, 'index.html', data, native))
               if name == 'arbitrary-expansion' else lambda text=text: authored.check_html(text))
    catalog_text = (site / 'guides/api-map.html').read_text()
    reject('forged-catalog-entry', lambda: api_catalog.verify(catalog_text.replace(
        'data-catalog-entry="', 'data-catalog-entry="Forged.', 1), root, references))
    example_text = (site / 'guides/examples.html').read_text()
    reject('incorrect-rendered-example-source', lambda: example_sources.verify_rendered(
        example_text.replace('data-example-source="src/', 'data-example-source="src/wrong_', 1),
        root, references))
    entry = example_sources.inventory(root)[0]
    example_path = root / 'docs/examples' / entry['main']
    example_bytes = example_path.read_bytes()
    try:
        example_path.write_bytes(example_bytes + b'-- substituted\n')
        reject('substituted-executable-source', lambda: independent_contract(
            root, site, report, data, source_bytes))
    finally:
        example_path.write_bytes(example_bytes)
    extra = root / 'docs/examples/src/unexpected.adb'
    try:
        extra.write_text('procedure Unexpected is begin null; end Unexpected;\n')
        reject('unexpected-example-file', lambda: example_sources.inventory(root))
    finally:
        extra.unlink()
    with tempfile.TemporaryDirectory(prefix='mel-pilot-negative-') as temp:
        copied = Path(temp) / 'site'
        shutil.copytree(site, copied)
        (copied / 'api/unexpected.html').write_text('<h1>Forged</h1>')
        reject('unexpected-native-file', lambda: package_map(copied, root, report['native']))
        (copied / 'api/unexpected.html').unlink()
        (copied / 'unexpected.txt').write_text('not authorized')
        reject('unexpected-publication-file', lambda: validate(copied, root, report['native']))
    return results


def probe(root, commit, baseline, output):
    from ada_gnatdoc_enhancer import authored
    from ada_gnatdoc_enhancer.errors import EnhancementError
    from ada_gnatdoc_enhancer.native_html import snapshot
    from ada_gnatdoc_enhancer.pipeline import enhance, verify
    presentation.tool()
    source_epoch.require(not output.exists(), 'pilot output must be fresh')
    output.mkdir(parents=True)
    commit = feature_source.revision(root, commit)
    epoch = json.loads((baseline / 'epoch.json').read_bytes())
    source_epoch.require(epoch['candidate_commit'] == commit, 'baseline source epoch differs')
    site = baseline / 'pristine'
    with tempfile.TemporaryDirectory(prefix='mel-authored-pilot-') as temp:
        exported = Path(temp) / 'source'
        source_epoch.export(root, commit, exported)
        source_bytes = {n: (exported / n).read_bytes() for n in feature_source.tree(root, commit)
                        if n != 'docs/gnatdoc-native-anchors.json'}
        report = measure(site, exported)
        source_epoch.require(report['native'] == epoch['native'], 'baseline native evidence differs')
        validate(site, exported, report['native'])
        references = References(exported, site, ledger=report['native'])
        audit = source_epoch.verify_presentation(site, exported, report, references,
                                                baseline / 'published', deep=True)
        source_epoch.require(audit == epoch['publication'], 'baseline reconstruction differs')
        data = manifest(exported)
        packages = package_map(site, exported, report['native'])
        for name in [data['home'], *(e['path'] for e in data['guides'])]:
            source_epoch.require(render(regular(exported / 'docs' / name), name, data, packages,
                                        exported, references).encode() == (site / name).read_bytes(),
                                 'baseline authored reconstruction differs: ' + name)
        ledger = exported / 'docs/gnatdoc-native-anchors.json'
        ledger.write_bytes(source_epoch.canonical(report['native']))
        source_bytes['docs/gnatdoc-native-anchors.json'] = ledger.read_bytes()
        authored_data = contract(exported, site, report)
        independent_contract(exported, site, report, authored_data, source_bytes)
        config = bind(exported, authored_data)
        native = snapshot(config, site)
        authored.manifest(config, native)
        candidate = output / 'candidate-accepted-only'
        candidate.mkdir()
        rows = {}
        for name in [data['home'], *(e['path'] for e in data['guides'])]:
            text = (exported / 'docs' / name).read_text()
            try:
                rendered = authored.portal(text, name, authored_data, native)
                authored.check_html(rendered)
            except EnhancementError as error:
                rows[name] = dict(status='requires-shared-enhancer-extension', error=str(error))
            else:
                target = candidate / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(rendered.encode())
                rows[name] = dict(status='directly-supported', identical=(site / name).read_bytes()
                                  == rendered.encode())
                source_epoch.require(rows[name]['identical'], 'unexplained authored byte difference')
        css = data['stylesheet']
        (candidate / css).parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(exported / 'docs' / css, candidate / css)
        rows[css] = dict(status='directly-supported', identical=True)
        stage = output / 'failed-full-pipeline'
        shutil.copytree(site / 'api', stage / 'api')
        try:
            enhance(config, stage)
            verify(config, stage)
        except EnhancementError as error:
            pipeline = dict(completed=False, error=str(error))
        else:
            raise ValueError('v0.1.5 behavior changed: requalify boundary before migration')
        # Isolate unchanged API behavior without pretending the rejected full
        # authored pipeline produced it. Production uses this same none mode.
        api_control = output / 'api-only-control'
        shutil.copytree(site / 'api', api_control / 'api')
        control_config = replace(config, guides='none', guide_manifest=None, guide_sha256=None)
        enhance(control_config, api_control)
        verify(control_config, api_control)
        control_files = presentation.files(api_control)
        expected_api = {n: b for n, b in presentation.files(baseline / 'published').items()
                        if n.startswith('api/')}
        source_epoch.require(control_files == expected_api, 'API-only control differs')
        probes = {}
        for name, text in {
            'details': '<details><summary>Supporting source</summary><p>source</p></details>',
            'summary': '<summary>Supporting source</summary>',
            'example-source': '<code data-example-source="src/provider_session.adb">source</code>',
            'catalog': '<li data-catalog-entry="AMS.MEL.Version" data-kind="function" data-scope="stable">x</li>',
            'source-end': '<span class="ada-source-end" aria-hidden="true"></span>',
        }.items():
            try:
                authored.check_html(text)
            except EnhancementError as error:
                probes[name] = dict(supported=False, error=str(error))
            else:
                probes[name] = dict(supported=True)
        negatives = negative_cases(exported, site, report, references, config, native,
                                   authored_data, source_bytes)
        for name in ('gnatdoc-authored.json', 'gnatdoc-authored-pilot.toml'):
            shutil.copyfile(exported / 'docs' / name, output / name)
        catalogs = api_catalog.inventory(exported, references, independent=True)
        public, _ = specifications(exported)
        evidence = dict(
            disposition='COMPATIBILITY_QUALIFIED', migration_ready=False,
            decision='small-shared-enhancer-extension-required', source_commit=commit,
            source_tree=feature_source.git(root, 'rev-parse', commit + '^{tree}').decode().strip(),
            enhancer_version=presentation.VERSION, enhancer_wheel_sha256=presentation.WHEEL_SHA256,
            guides=rows, full_pipeline=pipeline, markup=probes, negative_cases=negatives,
            source_sha256={n: presentation.sha(b) for n, b in source_bytes.items()},
            public_packages={n: p.relative_to(exported).as_posix() for n, p in public.items()},
            native=report['native'], symbols=audit['symbols'], type_links=audit['type_occurrences'],
            catalogs=catalogs, executable_examples=example_sources.inventory(exported),
            pristine_sha256=presentation.hashes(presentation.files(site)),
            production_sha256=presentation.hashes(presentation.files(baseline / 'published')),
            api_control_sha256=presentation.hashes(control_files),
            candidate_sha256=presentation.hashes(presentation.files(candidate)))
        comparison = {n: dict(old_sha256=presentation.sha(b),
                             new_sha256=presentation.sha((candidate / n).read_bytes())
                             if (candidate / n).is_file() else None,
                             identical=(candidate / n).is_file() and b == (candidate / n).read_bytes(),
                             api_only_control_sha256=presentation.sha(control_files[n])
                             if n in control_files else None)
                      for n, b in presentation.files(baseline / 'published').items()}
        (output / 'comparison.json').write_bytes(source_epoch.canonical(comparison))
        (output / 'evidence.json').write_bytes(source_epoch.canonical(evidence))
        print(json.dumps(dict(disposition=evidence['disposition'], decision=evidence['decision'],
                              guides=rows, negative_cases=len(negatives)), indent=2))
    return evidence


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--commit', default='HEAD')
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    probe(args.root.absolute(), args.commit, args.baseline.absolute(), args.output.absolute())
