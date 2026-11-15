"""Consumer authority for the official schema-3 authored-HTML pipeline.

Called only after source_epoch exports immutable Git objects and measures fresh
native output. This module derives inputs, not HTML; the installed wheel owns all
publication rendering. No output-side hashes can authorize source substitution.
"""

import json

import build_enhanced_site as presentation
from build_docs_portal import GROUPS, group, manifest, package_map
from gnatdoc_inventory import specifications
import example_sources


def contract(root, site, report):
    data = manifest(root)
    packages = package_map(site, root, report['native'])
    entries = example_sources.inventory(root)
    inputs = {'docs/' + p for p in
              [data['home'], data['stylesheet'], *(g['path'] for g in data['guides'])]}
    inputs.update({'docs/site-map.json', 'docs/gnatdoc-native-anchors.json',
                   'docs/examples/manifest.json', 'docs/examples/ams_mel_doc_examples.gpr'})
    inputs.update(p.relative_to(root).as_posix() for p in specifications(root)[0].values())
    inputs.update('docs/examples/' + p for e in entries for p in [e['main'], *e['support']])
    return dict(
        schema_version=3,
        home=dict(source='docs/' + data['home'], path=data['home']),
        stylesheet=dict(source='docs/' + data['stylesheet'], path=data['stylesheet']),
        guides=[dict(g, source='docs/' + g['path'], entry_points=[]) for g in data['guides']],
        inputs={p: presentation.sha(json.dumps(report['native']).encode() if
                                   p == 'docs/gnatdoc-native-anchors.json' else (root / p).read_bytes())
                for p in sorted(inputs)},
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
        renderer_exceptions=['AMS.MEL.IR.Health_Status.Metadata.Status'],
        source_blocks=dict(profile='executable-ada-catalog-v1',
                           manifest='docs/examples/manifest.json',
                           project='docs/examples/ams_mel_doc_examples.gpr',
                           guide='guides/examples.html', catalog_guide='guides/api-map.html',
                           symbol_policy='stable-public-types-and-root-constants-v1',
                           use_policy='qualified-types-and-unique-operations-v1'))


def bind(root, site, report):
    """Bind ephemeral manifest/digest only inside the disposable source export."""
    from dataclasses import replace
    from ada_gnatdoc_enhancer.config import load
    import source_epoch
    # Only disposable exports call bind. Verification uses the pure contract()
    # path and does not rewrite a supplied or tracked ledger.
    (root / 'docs/gnatdoc-native-anchors.json').write_text(json.dumps(report['native']))
    payload = source_epoch.canonical(contract(root, site, report))
    path = root / 'docs/gnatdoc-authored.json'
    presentation.require(not path.exists() and not path.is_symlink(), 'authored manifest must be fresh')
    path.write_bytes(payload)
    config = load(root, 'docs/gnatdoc-enhance.toml')
    presentation.require(config.guides == 'authored-html' and
                         config.guide_manifest == 'docs/gnatdoc-authored.json' and
                         config.guide_sha256 == '0' * 64,
                         'unexpected authored configuration authority')
    return replace(config, guide_sha256=presentation.sha(payload))


def independent_contract(root, site, report, supplied, source_bytes):
    """Check Git-derived input bytes separately from resealable manifest claims."""
    for name, expected in source_bytes.items():
        path = root / name
        presentation.require(not any(p.is_symlink() for p in (path, *path.parents)) and
                             path.is_file() and path.read_bytes() == expected,
                             'authored source differs from Git: ' + name)
    presentation.require(supplied == contract(root, site, report),
                         'authored contract differs from independent source/native authority')
