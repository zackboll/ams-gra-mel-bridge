"""Reconstruct the schema-2 delta from MEL sources and pristine native inputs."""

from collections import Counter
import html
import json
import posixpath

import body_profiles as profiles
import build_enhanced_site as p
import source_epoch as epoch
import source_view_audit as spec_view

MODE = 'public-specifications-and-bodies'
EVIDENCE = 'docs/gnatdoc-body-native.json'
CONFIG = 'docs/gnatdoc-body-effective.toml'
METADATA = 'api/presentation.json'


def eligible(specs, bodies):
    return {key: body for key, body in bodies.items() if not specs[key].private}


def evidence(root, pristine, specs, bodies):
    def rows(sources):
        return {s.path: dict(package=s.name, sha256=p.sha(s.raw)) for s in sources}
    return dict(schema_version=2, mode=MODE, files=spec_view.evidence(root, pristine)['files'],
                sources=rows(s for s in specs.values() if not s.private),
                bodies=rows(eligible(specs, bodies).values()))


def options(data):
    return dict(schema_version=2, mode=MODE, native_manifest=EVIDENCE,
                native_manifest_sha256=p.sha(epoch.canonical(data)))


def bind(export, pristine, specs, bodies, data):
    """Caller supplies a fresh disposable export, never the publication or checkout."""
    p.require(spec_view.options(export, pristine), 'schema 2 requires the reviewed schema-1 baseline')
    p.require(data == evidence(export, pristine, specs, bodies), 'forged schema-2 input evidence')
    base = (export / spec_view.CONFIG).read_text()
    left, right = base.index('[source_view]\n'), base.index('[guides]\n')
    effective = options(data)
    lines = '\n'.join(f'{key} = {json.dumps(value)}' for key, value in effective.items())
    with (export / EVIDENCE).open('xb') as stream:
        stream.write(epoch.canonical(data))
    with (export / CONFIG).open('x') as stream:
        stream.write(base[:left] + '[source_view]\n' + lines + '\n\n' + base[right:])
    return CONFIG


def navigation(source, other, pairs, reverse=False):
    label = 'View specification' if reverse else 'View body'
    local, remote = ('body_line', 'line') if reverse else ('line', 'body_line')
    relative = posixpath.relpath(other.page, posixpath.dirname(source.page))

    def link(path, line, title):
        return f'<a href="{html.escape(path, quote=True)}#L{line}">{html.escape(title)}</a>'

    result = '<nav aria-label="Ada source correspondence"><p>' + link(relative, pairs[0][remote], label) + '</p>'
    if len(pairs) > 1:
        result += '<details><summary>Corresponding operations</summary><ul>'
        for pair in sorted(pairs[1:], key=lambda entry: entry[local]):
            a, b = pair[local], pair[remote]
            result += f'<li>{link("", a, pair["name"] + f" (line {a})")} — {link(relative, b, label + f" at line {b}")}</li>'
        result += '</ul></details>'
    return result + '</nav>'


def reconstruct(root, pristine, authority, specs, bodies, compiled, data):
    p.require(data == evidence(root, pristine, specs, bodies), 'source/native evidence differs from authority')
    _, insertions, metadata = spec_view.reconstruct(root, pristine, authority)
    metadata.update(schema_version=2, mode=MODE, native_evidence=options(data), bodies=[])
    scans = {s.path: profiles.scan(s) for s in (*specs.values(), *bodies.values())}
    pages, details = {}, {}
    native = {package: 'api/' + path for path, package in authority.page_packages.items()}
    selected = eligible(specs, bodies)
    for file in metadata['files']:
        package = file['package']
        key = package.lower()
        spec, body = specs[key], selected.get(key)
        p.require(file['source'] == spec.path and file['declarations'][0]['line'] == spec.opening,
                  'package/source heading differs')
        entries = insertions[native[package]][2]
        calls = [d for d in entries if d['kind'] in profiles.CALLABLES]
        source_calls = [d for d in authority.packages[key]['declarations']
                        if d['signature'].startswith(('function ', 'procedure '))]
        p.require({d['line'] for d in calls} == {d['start'] for d in source_calls} and
                  len(calls) == len(source_calls), 'missing/duplicate native-backed public callable: ' + package)
        matches = profiles.correspondence(spec, body, calls, specs, scans, compiled[key])
        for decl in calls:
            line, reason = matches[decl['anchor']]
            if reason:
                decl.update(body_omission=reason)
            else:
                decl.update(body_source=body.path, body_line=line)
        correspondence = ''
        if body:
            entries[0].update(body_source=body.path, body_line=body.opening)
            pairs = [entry for entry in entries if 'body_line' in entry]
            correspondence = navigation(spec, body, pairs)
            pages[body.page], lines = spec_view.page(body.path, body.raw,
                                                    correspondence=navigation(body, spec, pairs, reverse=True))
            p.require(all(0 < d['body_line'] <= lines for d in pairs), 'out-of-range implementation line')
            metadata['bodies'].append(dict(package=package, source=body.path, page=body.page,
                                           sha256=p.sha(body.raw), lines=lines, specification=spec.page))
        pages[spec.page], _ = spec_view.page(spec.path, spec.raw, correspondence=correspondence)
        details[package] = dict(native=native[package], specification=spec.path,
                                body=body.path if body else None, declarations=entries)
    return pages, insertions, metadata, details


def compare(pristine, baseline, candidate, expected):
    """Classify every byte delta; restore both native views without broad exclusions."""
    pages, insertions, metadata, _ = expected
    bodies = {r['page'] for r in metadata['bodies']}
    p.require(candidate.keys() - baseline.keys() == bodies and baseline.keys() <= candidate.keys(),
              'body-page inventory delta mismatch')
    p.require({n for n in candidate if n.startswith('api/source/')} == pages.keys(), 'source-page inventory mismatch')
    p.require(p.sha(candidate[spec_view.CSS]) == spec_view.CSS_SHA256, 'source stylesheet identity mismatch')
    changed = {METADATA, *insertions, *(r['specification'] for r in metadata['bodies'])}
    for name in baseline.keys() - changed:
        p.require(baseline[name] == candidate[name], 'unrelated publication file differs: ' + name)
    for name, expected_page in pages.items():
        p.require(candidate[name] == expected_page, 'source page differs from approved text/markup: ' + name)
    for name, (package, spec, entries) in insertions.items():
        original = (pristine / name).read_bytes().decode()
        plain = (package, spec, [{k: v for k, v in entry.items() if not k.startswith('body_')} for entry in entries])
        p.require(spec_view.restore(candidate[name].decode(), original, (package, spec, entries), with_bodies=True) ==
                  spec_view.restore(baseline[name].decode(), original, plain),
                  'body navigation changed unrelated native presentation: ' + name)
    wanted = json.loads(baseline[METADATA])
    wanted['source_view'] = metadata
    callables = [e for record in metadata['files'] for e in record['declarations'] if e['kind'] in profiles.CALLABLES]
    wanted['counts'].update(body_pages=len(bodies), body_operation_links=sum('body_line' in d for d in callables),
                            body_operation_omissions=sum('body_omission' in d for d in callables))
    # Enhancer metadata fingerprints the pre-assembly Ada portal, not the
    # Rust/Python landing additions. Only the exact classified Ada deltas change.
    for name in (changed - {METADATA}) | bodies:
        wanted['output_sha256'][name] = p.sha(candidate[name])
    canonical = json.dumps(wanted, sort_keys=True, indent=2, ensure_ascii=False) + '\n'
    p.require(candidate[METADATA] == canonical.encode(), 'altered body omission evidence or publication metadata')
    return dict(added_body_pages=len(bodies), native_pages=len(insertions),
                changed_specifications=len(bodies), unrelated_files=len(baseline) - len(changed),
                files=len(candidate))


def metrics(specs, bodies, details):
    selected = eligible(specs, bodies)
    packages, groups = {}, {}
    for package, record in details.items():
        key = package.lower()
        calls = [d for d in record['declarations'] if d['kind'] in profiles.CALLABLES]
        reasons = Counter(d['body_omission'] for d in calls if 'body_omission' in d)
        names = Counter(d['name'] for d in calls)
        overloads = [d for d in calls if names[d['name']] > 1]
        subset = 'IR' if key == 'ams.mel.ir' or key.startswith('ams.mel.ir.') else (
            'RF' if key == 'ams.mel.rf' or key.startswith('ams.mel.rf.') else 'Common')
        spec, body = specs[key], selected.get(key)
        counts = dict(public_specifications=1, eligible_bodies=int(body is not None),
                      body_pages=int(body is not None), public_callables=len(calls),
                      direct_destinations=len(calls) - sum(reasons.values()), omitted_destinations=sum(reasons.values()),
                      specification_lines=spec.summary()['lines'], body_lines=body.summary()['lines'] if body else 0,
                      specification_bytes=len(spec.raw), body_bytes=len(body.raw) if body else 0,
                      overloaded_names=sum(n > 1 for n in names.values()), overloaded_declarations=len(overloads),
                      overload_destinations=sum('body_line' in d for d in overloads))
        groups.setdefault(subset, []).append((Counter(counts), reasons))
        packages[package] = dict(counts=counts, omissions=dict(reasons), specification=spec.path, body=record['body'])

    def aggregate(data):
        counts, omissions = Counter(), Counter()
        for items, reasons in data:
            counts.update(items)
            omissions.update(reasons)
        return dict(counts=dict(counts), omissions=dict(omissions))
    return dict(packages=packages, groups={k: aggregate(v) for k, v in groups.items()},
                total=aggregate(item for group in groups.values() for item in group),
                source_inventory={s.path: dict(s.summary(), eligible=not specs[k].private)
                                  for entries in (specs, bodies) for k, s in entries.items()})