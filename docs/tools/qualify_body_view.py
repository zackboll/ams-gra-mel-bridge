"""Explicit, non-deploying three-mode qualification for official schema 2."""

import argparse
from dataclasses import replace
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

import authored_html
import body_source_inventory as ownership
import body_view_audit as audit
import build_enhanced_site as p
from check_live_docs_site import graph
import docs_ci
from example_sources import References
from local_type_audit import Authority
import python_publication as python
import rustdoc_publication as rust
import source_epoch as epoch
import source_view_audit as spec_view
from source_view_parity import compare as compare_toggle


def render(config, pristine, output):
    from ada_gnatdoc_enhancer.pipeline import enhance, verify
    output.mkdir()
    shutil.copytree(pristine / 'api', output / 'api')
    enhance(config, output)
    verify(config, output)
    first = p.files(output)
    enhance(config, output)
    p.require(p.files(output) == first, 'non-idempotent schema-2 Ada publication')
    return first


def variants(root, context, prefix, work, *, controls=False):
    from ada_gnatdoc_enhancer.config import load
    exported = work / 'source'
    pristine, report, references = epoch.generate(root, context['commit'], exported, prefix, work / 'native.log')
    native = p.files(pristine)
    authority = Authority(exported, pristine, ledger=report['native'])
    epoch.enhance(pristine, exported, report, references, work / 'schema1', deep=True)
    schema1_ada = p.files(work / 'schema1')
    config = exported / spec_view.CONFIG
    baseline_config = config.read_bytes()
    left, right = baseline_config.index(b'[source_view]\n'), baseline_config.index(b'[guides]\n')
    config.write_bytes(baseline_config[:left] + baseline_config[right:])
    try:
        epoch.enhance(pristine, exported, report, References(exported, pristine, ledger=report['native']),
                      work / 'disabled', deep=True)
    finally:
        config.write_bytes(baseline_config)
    specs, bodies = ownership.inventory(exported)
    compiled = ownership.compile_units(exported, work / 'compiled', specs, bodies)
    evidence = audit.evidence(exported, pristine, specs, bodies)
    expected = audit.reconstruct(exported, pristine, authority, specs, bodies, compiled, evidence)
    effective = audit.bind(exported, pristine, specs, bodies, evidence)
    authored = epoch.canonical(authored_html.contract(exported, pristine, report))
    p.require((exported / 'docs/gnatdoc-authored.json').read_bytes() == authored, 'authored input changed')
    schema2 = replace(load(exported, effective), guide_sha256=p.sha(authored))
    schema2_ada = render(schema2, pristine, work / 'schema2')
    # Apply the same full active-content/graph policy as the established Ada
    # surface. The separate Rust/Python policy is exercised by generation below.
    edges = len(graph(schema2_ada, source_pages=expected[0]))
    rust_files, _ = rust.generate(exported, work / 'rust-generation', context, work / 'rust.log')
    python_files, _ = python.generate(exported, context, work / 'python.log')

    def assembled(ada):
        return python.assembled(rust.assembled(ada, rust_files), python_files)

    disabled = assembled(p.files(work / 'disabled'))
    schema1 = assembled(schema1_ada)
    schema2 = assembled(schema2_ada)
    delta1 = compare_toggle(exported, pristine, disabled, schema1, authority)
    delta2 = audit.compare(pristine, schema1, schema2, expected)
    p.require(native == p.files(pristine) and config.read_bytes() == baseline_config and
              ownership.inventory(exported) == (specs, bodies), 'qualified input bytes changed')
    if controls:
        from body_view_controls import run
        mutations = run(exported, pristine, report, references, authority, schema1_ada,
                        schema2_ada, expected, specs, bodies, compiled, evidence)
        for name, data in schema2.items():
            path = work / 'schema2-complete' / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        for name, data in schema1.items():
            path = work / 'schema1-complete' / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
    else:
        mutations = []
    return dict(pristine=p.hashes(native), disabled=delta1['disabled'], schema1=delta1['enabled'],
                schema2=p.hashes(schema2), delta1={k: v for k, v in delta1.items() if k not in {'disabled', 'enabled'}},
                delta2=delta2, graph_edges=edges, idempotent=True,
                coverage=audit.metrics(specs, bodies, expected[3]),
                evidence_sha256=p.sha(epoch.canonical(evidence))), expected[3], mutations


def qualify(root, prefix, browser_python, output):
    p.require(not any(q.is_symlink() for q in (output, *output.parents)), 'symlink diagnostics path')
    p.require(not output.exists(), 'diagnostics output must be fresh')
    context = docs_ci.identity(root)
    p.require(context['mode'] == 'local-not-deployable' or context['event'] == 'workflow_dispatch',
              'hosted body qualification requires explicit workflow_dispatch')
    p.tool()
    prefix = epoch.toolchain(prefix)
    output.mkdir(parents=True)
    docs_ci.write(output / 'identity.json', dict(source=context, enhancer=dict(
        version=p.VERSION, commit=p.REVISION, tree=p.RELEASE_TREE, wheel_sha256=p.WHEEL_SHA256,
        package_sha256=p.PACKAGE_SHA256), deployable=False))
    with tempfile.TemporaryDirectory(prefix='mel-body-local-') as temp:
        work = Path(temp)
        first, journeys, mutations = variants(root, context, prefix, work / 'first', controls=True)
        docs_ci.write(output / 'qualification.json', dict(first, mutations=mutations, repeated=False))
        second, repeated_journeys, _ = variants(root, context, prefix, work / 'second')
        p.require(first == second and journeys == repeated_journeys, 'nondeterministic three-mode build')
        docs_ci.write(output / 'qualification.json', dict(first, mutations=mutations, repeated=True))
        inputs = work / 'browser-input.json'
        docs_ci.write(inputs, journeys)
        tools = Path(__file__).resolve().parent
        for name, script, args in (
                ('schema1-browser', 'source_view_browser.py', []),
                ('schema2-browser', 'body_view_browser.py', ['--evidence', str(inputs)])):
            site = work / 'first' / (name.split('-')[0] + '-complete')
            browser_result = work / (name + '.json')
            command = [str(browser_python), str(tools / script), '--root', str(work / 'first/source'),
                       '--site', str(site), '--output', str(browser_result), *args]
            result = subprocess.run(command, capture_output=True, text=True)
            # Never echo a browser diagnostic containing HTML or source text.
            if result.returncode and context['mode'] == 'local-not-deployable':
                print(result.stderr)
            p.require(result.returncode == 0, name + ' qualification failed')
            data = json.loads(browser_result.read_bytes())
            p.require(data['passed'], name + ' reported incomplete qualification')
            if name == 'schema1-browser':
                for journey in data['journeys']:
                    journey.pop('text', None)
            docs_ci.write(output / (name + '.json'), data)
        p.require(docs_ci.identity(root) == context, 'source changed during body qualification')
    docs_ci.write(output / 'result.json', dict(status='passed', source=context, deployable=False))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--gnatdoc-prefix', type=Path, required=True)
    parser.add_argument('--browser-python', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True, help='sanitized JSON diagnostics only')
    args = parser.parse_args()
    try:
        qualify(docs_ci.ROOT, args.gnatdoc_prefix, args.browser_python.absolute(), args.output.absolute())
    except Exception as error:
        if os.environ.get('GITHUB_ACTIONS') == 'true':
            # Keep source excerpts, compiler diagnostics, and raw browser logs
            # out of hosted logs as well as uploaded artifacts.
            raise SystemExit('Schema-2 qualification failed (' + type(error).__name__ + '); reproduce locally.') from None
        raise