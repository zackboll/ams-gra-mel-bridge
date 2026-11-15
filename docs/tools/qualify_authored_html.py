"""Full candidate authored-HTML qualification; no fallback/overlay publication."""

import argparse
import copy
from dataclasses import replace
import json
from pathlib import Path
import shutil
import tempfile

import authored_html as authority
import authored_html_pilot as historical
import build_enhanced_site as p
import feature_source as source
import source_epoch as epoch
from check_doc_coverage import measure
from ada_examples import References


def qualify(root, candidate, output, baseline=None):
    from ada_gnatdoc_enhancer import authored
    from ada_gnatdoc_enhancer.native_html import snapshot
    from ada_gnatdoc_enhancer.source_blocks import Blocks
    p.tool()
    epoch.require(not output.exists(), 'qualification output must be fresh')
    output.mkdir(parents=True)
    commit = source.revision(root, 'HEAD')
    record = json.loads((candidate / 'epoch.json').read_bytes())
    epoch.require(record['candidate_commit'] == commit, 'candidate source epoch differs')
    with tempfile.TemporaryDirectory(prefix='mel-authored-qualification-') as temp:
        exported = Path(temp) / 'source'
        epoch.export(root, commit, exported)
        site = candidate / 'pristine'
        report = measure(site, exported)
        epoch.require(report['native'] == record['native'], 'native source reconstruction differs')
        references = References(exported, site, ledger=report['native'])
        audit = epoch.verify_presentation(site, exported, report, references, candidate / 'published', deep=True)
        epoch.require(audit == record['publication'], 'independent candidate reconstruction differs')
        config = authority.bind(exported, site, report)
        native = snapshot(config, site)
        data = authority.contract(exported, site, report)
        source_bytes = {n: (exported / n).read_bytes() for n in data['inputs']}
        authority.independent_contract(exported, site, report, data, source_bytes)
        negatives = historical.negative_cases(exported, site, report, references, config, native,
            data, source_bytes, contract_builder=authority.contract, verify_contract=authority.independent_contract)

        def reject(name, action):
            try:
                action()
            except (ValueError, OSError) as error:
                negatives[name] = dict(rejected=True, reason=str(error))
            else:
                raise ValueError('schema-3 negative accepted: ' + name)

        def reseal(changed):
            raw = epoch.canonical(changed)
            (exported / config.guide_manifest).write_bytes(raw)
            return replace(config, guide_sha256=p.sha(raw))

        for name, key, value in (
                ('unsupported-source-profile', 'profile', 'execute-python'),
                ('unsupported-symbol-policy', 'symbol_policy', 'all-declarations'),
                ('unsupported-use-policy', 'use_policy', 'guess-overloads'),
                ('wrong-guide-ownership', 'guide', 'guides/architecture.html'),
                ('wrong-project-ownership', 'project', 'docs/examples/other.gpr')):
            changed = copy.deepcopy(data)
            changed['source_blocks'][key] = value
            reject(name, lambda changed=changed: authored.manifest(reseal(changed), native))
        changed = copy.deepcopy(data)
        changed['inputs'].pop(next(n for n in data['inputs'] if n.startswith('ada/src/')))
        reject('unbound-public-source', lambda: authored.manifest(reseal(changed), native))
        reseal(data)
        blocks = Blocks(config, data, native)
        text = (exported / 'docs/guides/examples.html').read_text()
        first = '{{example:' + blocks.entries[0]['id'] + '}}'
        for name, replacement in (
                ('unknown-example', '{{example:not-owned}}'),
                ('duplicate-example', first + first),
                ('token-in-comment', '<!--' + first + '-->'),
                ('token-in-attribute', '<p title="' + first + '">x</p>'),
                ('token-in-code', '<pre><code>' + first + '</code></pre>'),
                ('injected-generated-markup', '<details><summary>x</summary></details>' + first)):
            reject(name, lambda replacement=replacement: blocks.expand(text.replace(first, replacement, 1),
                                                                       'guides/examples.html'))
        generated = blocks.expand(text, 'guides/examples.html')
        reject('tampered-complete-listing', lambda: blocks.check(
            generated.replace('data-example-source="src/', 'data-example-source="src/forged_', 1),
            'guides/examples.html'))
        path = exported / data['source_blocks']['manifest']
        raw = path.read_bytes()
        try:
            changed = json.loads(raw)
            changed['examples'][0]['main'] = changed['examples'][1]['main']
            path.write_bytes(epoch.canonical(changed))
            reject('resealed-wrong-main-owner', lambda: authored.manifest(
                reseal(authority.contract(exported, site, report)), native))
        finally:
            path.write_bytes(raw)
            reseal(data)
        portal = authority.manifest(exported)
        names = [portal['home'], *(e['path'] for e in portal['guides']), portal['stylesheet']]
        comparison = {}
        if baseline:
            before, after = p.files(baseline / 'published'), p.files(candidate / 'published')
            epoch.require(set(before) == set(after), 'complete publication inventory changed')
            for name in before:
                if name != 'api/presentation.json':
                    epoch.require(before[name] == after[name], 'unrelated output difference: ' + name)
            old_meta, new_meta = (json.loads(v['api/presentation.json']) for v in (before, after))
            expected = copy.deepcopy(old_meta)
            expected['enhancer_version'] = p.VERSION
            expected['output_sha256'].update({n: p.sha(after[n]) for n in names})
            expected['authored_source_blocks'] = new_meta['authored_source_blocks']
            epoch.require(expected == new_meta, 'unclassified presentation-manifest difference')
            comparison = {n: dict(baseline_sha256=p.sha(before[n]), candidate_sha256=p.sha(after[n]),
                                  identical=before[n] == after[n]) for n in names}
        result = dict(commit=commit, tree=source.git(root, 'rev-parse', commit + '^{tree}').decode().strip(),
                      wheel_sha256=p.WHEEL_SHA256, package_sha256=p.PACKAGE_SHA256,
                      negative_cases=negatives, portal_parity=comparison,
                      inventory=p.hashes(p.files(candidate / 'published')), native=report['native'],
                      symbols=audit['symbols'], type_occurrences=audit['type_occurrences'])
        (output / 'evidence.json').write_bytes(epoch.canonical(result))
        (output / 'authored-manifest.json').write_bytes(epoch.canonical(data))
    print(json.dumps(dict(negative_cases=len(negatives), parity_assets=len(comparison), commit=commit)))
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    qualify(args.root, args.candidate, args.output, args.baseline)
