"""Negative controls against authentic publications; never retain mutated HTML."""

import copy
import json
from pathlib import Path
import re
import tempfile

import body_profiles as profiles
import body_source_inventory as source
import body_view_audit as audit
import build_enhanced_site as p
import source_epoch as epoch
import source_view_audit as spec_view


def run(root, pristine, report, references, authority, baseline, actual, expected,
        specs, bodies, compiled, evidence):
    results = []

    def rejects(label, message, call):
        try:
            call()
        except ValueError as error:
            p.require(message in str(error), 'wrong mutation rejection: ' + label + ': ' + str(error))
        else:
            raise ValueError('accepted mutation: ' + label)
        results.append(dict(control=label, rejected_for=message))

    def publication(label, message, change, meta=lambda _: None):
        tree = dict(actual)
        metadata = json.loads(tree[audit.METADATA])
        change(tree)
        for name in actual.keys() ^ tree.keys():
            metadata['output_sha256'].pop(name, None)
        for name in tree.keys() - {audit.METADATA, 'api/gnatdoc.css'}:
            if tree[name] != actual.get(name):
                metadata['output_sha256'][name] = p.sha(tree[name])
        meta(metadata)
        tree[audit.METADATA] = (json.dumps(metadata, sort_keys=True, indent=2, ensure_ascii=False) + '\n').encode()
        rejects(label, message, lambda: audit.compare(pristine, baseline, tree, expected))

    details = expected[3]
    image = details['AMS.MEL.IR.Image']
    command = details['AMS.MEL.IR.Scheduling.Commands']
    body_page = 'api/source/' + image['body'] + '.html'
    publication('missing body', 'body-page inventory', lambda tree: tree.pop(body_page))
    for case, suffix in [('duplicate body', '-copy.adb.html'), ('case-colliding body', '.ADB.html')]:
        publication(case, 'body-page inventory', lambda tree: tree.update({body_page.removesuffix('.adb.html') + suffix: tree[body_page]}))
    publication('unexpected private body', 'body-page inventory', lambda tree: tree.update({
        'api/source/' + next(s.path for k, s in bodies.items() if specs[k].private) + '.html': tree[body_page]}))
    publication('wrong body page content/owner', 'approved text/markup', lambda tree: tree.update({
        body_page: tree['api/source/' + command['body'] + '.html']}))

    def substitute(tree, name, old, new):
        p.require(old != new and old in tree[name], 'mutation was not applied')
        tree[name] = tree[name].replace(old, new, 1)

    for label, old, new in [
            ('line anchor', b'id="L2"', b'id="L1"'),
            ('body source bytes', b'ada-keyword">package', b'ada-keyword">forged'),
            ('comment becomes markup', b'</code>', b'<img src="https://example.invalid/image"></code>'),
            ('active attribute', b'<body ', b'<body onload="void(0)" '),
            ('external resource', b'href="../../../gnatdoc-source.css"', b'href="https://example.invalid/source.css"')]:
        publication(label, 'approved text/markup', lambda tree: substitute(tree, body_page, old, new))

    def direct(record, declaration, line=None):
        return f'source/{record["body"]}.html#L{line or declaration["body_line"]}">View body</a>'.encode()

    wait = [d for d in command['declarations'] if d['name'].endswith('.Wait') and 'body_line' in d]
    p.require(len(wait) >= 2, 'need distinct ordinary Wait overloads for control')
    first, second = (direct(command, d) for d in wait[:2])

    def swap(tree):
        native = command['native']
        substitute(tree, native, first, b'__body_a__')
        substitute(tree, native, second, first)
        substitute(tree, native, b'__body_a__', second)

    publication('swapped overloads', 'declaration source destination', swap)
    submit = next(d for d in image['declarations'] if d['name'].endswith('.Submit_Navigation_Report'))
    text = (root / image['body']).read_text().splitlines()
    for description, pattern in [('private helper', r'^   function Raw_Report\b'),
                                 ('unrelated declaration', r'^   function Is_Open \(Request')]:
        target = next(i for i, line in enumerate(text, 1) if re.match(pattern, line))
        publication('direct link to ' + description, 'declaration source destination',
                    lambda tree: substitute(tree, image['native'], direct(image, submit), direct(image, submit, target)))

    def omission(manifest, mode):
        rows = manifest['source_view']['files']
        target = next(d for row in rows for d in row['declarations'] if d.get('body_omission'))
        if mode == 'altered':
            target['body_omission'] = 'invented'
        else:
            rows[0]['declarations'][0]['body_omission'] = 'implementation-separate'

    for mode in ('altered', 'invented'):
        publication(mode + ' omission evidence', 'altered body omission evidence', lambda tree: None,
                    lambda manifest: omission(manifest, mode))

    for field in ('files', 'sources', 'bodies'):
        forged = copy.deepcopy(evidence)
        forged[field][next(iter(forged[field]))] = 'f' * 64
        rejects('forged ' + field + ' authorization', 'forged schema-2 input evidence',
                lambda: audit.bind(root, pristine, specs, bodies, forged))

    config = root / spec_view.CONFIG
    original = config.read_bytes()
    try:
        config.write_bytes((root / audit.CONFIG).read_bytes())
        rejects('implicit schema 2', 'unexpected source-view opt-in contract',
                lambda: spec_view.prepare(root, pristine))
    finally:
        config.write_bytes(original)
    with tempfile.TemporaryDirectory(prefix='schema1-body-rejection-', dir=root.parent) as directory:
        candidate = Path(directory)
        for name, data in {**baseline, body_page: actual[body_page]}.items():
            path = candidate / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        rejects('ordinary verifier rejects body publication', 'source-page inventory mismatch',
                lambda: epoch.verify_presentation(pristine, root, report, references, candidate))

    # Exercise source-owned inventory in an isolated synthetic tree. The path
    # names deliberately need not match their package identity.
    with tempfile.TemporaryDirectory(prefix='body-ownership-', dir=root.parent) as directory:
        small = Path(directory)
        (small / 'docs').mkdir()
        (small / 'ada/src').mkdir(parents=True)
        (small / spec_view.CONFIG).write_bytes(original)
        spec = small / 'ada/src/one.ads'
        body = small / 'ada/src/two.adb'
        spec.write_text('package Example is\nend Example;\n')
        initial = 'package body Example is\nend Example;\n'
        body.write_text(initial)
        p.require(len(source.inventory(small)[1]) == 1, 'ownership inferred from filename')
        for label, replacement, reason in [
                ('case-inconsistent owner', initial.replace('Example', 'example'), 'ownership or spelling differs'),
                ('wrong owner', initial.replace('Example', 'Other'), 'ownership or spelling differs'),
                ('second library unit', initial + 'package Other is end Other;', 'single-library-unit boundary')]:
            body.write_text(replacement)
            rejects(label, reason, lambda: source.inventory(small))
        body.write_text(initial)
        for label, name, reason in [
                ('case-colliding path', 'TWO.adb', 'case-colliding Ada source path'),
                ('duplicate package identity', 'duplicated.adb', 'duplicate/case-colliding package owner'),
                ('encoded path', 'bad%20name.adb', 'unsafe or cross-directory'),
                ('cross-directory ownership', 'nested/other.adb', 'unsafe or cross-directory')]:
            path = body.parent / name
            path.parent.mkdir(exist_ok=True)
            path.write_text(initial)
            rejects(label, reason, lambda: source.inventory(small))
            path.unlink()
        path = body.with_name('link.adb')
        path.symlink_to(body)
        rejects('symlink in source inventory', 'symlink source member', lambda: source.inventory(small))

    # Profile controls use independent unmodified GNAT cross-references: a
    # misleading source form cannot become an approved line by resemblance.
    key = 'ams.mel.ir.image'
    spec, body = specs[key], bodies[key]
    scans = {s.path: profiles.scan(s) for s in (*specs.values(), *bodies.values())}

    def probe(replacement):
        changed = source.unit(body.path, replacement.encode())
        return profiles.correspondence(spec, changed, [submit], specs,
                                       {**scans, body.path: profiles.scan(changed)}, compiled[key])[submit['anchor']]

    original = body.raw.decode()
    at = original.index('   function Submit_Navigation_Report')
    end = original.index('   end Submit_Navigation_Report;', at) + len('   end Submit_Navigation_Report;')
    operation = original[at:end]
    for label, altered, reason in [
            ('parameter-name mismatch', operation.replace('Object :', 'Different :', 1), 'unsupported-or-nonconformant-body-profile'),
            ('unresolved type mark', operation.replace('Report : Navigation_Report', 'Report : Unknown_Type', 1), 'unsupported-or-nonconformant-body-profile'),
            ('separate stub', operation.split('\n   is\n')[0] + '\n   is separate;', 'implementation-separate'),
            ('nested operation', '\n'.join('   ' + s for s in operation.splitlines()), 'no-conformant-direct-body'),
            ('shadowed type mark', operation + '\n   subtype Navigation_Report is Integer;', 'unsupported-or-nonconformant-body-profile')]:
        if label == 'parameter-name mismatch':
            # Keep the full declaration and defining coordinates but alter a
            # formal name. Matching on only kind/name/return would accept it.
            p.require(altered != operation, 'unchanged profile mutation')
        p.require(probe(original[:at] + altered + original[end:]) == (None, reason), 'unexpected profile outcome: ' + label)
        results.append(dict(control=label, rejected_for=reason))

    raw = '-- <script>ignored</script> {é}\r\n\tX := 1;\n\n-- no terminal newline'.encode()
    rendered, lines = spec_view.page('ada/src/example.adb', raw)
    p.require(lines == 4 and b'&lt;script&gt;' in rendered and b'<script>' not in rendered and b'&#13;\n' in rendered,
              'source comment/Unicode/physical-line escaping differs')
    results.append(dict(control='malicious comment, CRLF, tab, Unicode and no terminal newline',
                        rejected_for='active markup escaped as data'))
    return results