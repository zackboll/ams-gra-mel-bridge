"""MEL-owned source/native evidence and exact source-view byte contracts.

No enhancer inventory, source renderer, destination resolver or output-side hash
is an authority here. Full-profile identities come from MEL's measured native
ledger and local_type_audit; package visibility comes from gnatdoc_inventory.
"""

import html
import posixpath
import re
import tomllib

import build_enhanced_site as p
from ada_examples import tokenize
from gnatdoc_inventory import specifications, HEADER
from compare_gnatdoc_warnings import code_tokens
from gnatdoc_coverage_model import specification
from local_type_audit import lex
import source_epoch as epoch

CONFIG = 'docs/gnatdoc-enhance.toml'
EVIDENCE = 'docs/gnatdoc-source-native.json'
CSS = 'api/gnatdoc-source.css'
# SHA-256 of assets/gnatdoc-source.css extracted from the verified official wheel.
CSS_SHA256 = '2e4eb69bd23e1ea97e3877383dc7e75d701429006c25581a0e16a75041823fdc'
HEAD = '<link rel="stylesheet" href="gnatdoc-source.css">'
TOKEN_CLASSES = ('keyword', 'comment', 'string', 'character', 'number', 'attribute', 'operator')
TOKEN_SPAN = re.compile(rb'<span class="ada-(?:' + '|'.join(TOKEN_CLASSES).encode() + rb')">([^<\n]+)</span>')


def sources(root):
    directory = root / 'ada/src'
    p.require(not any(q.is_symlink() for q in (directory, *directory.parents)) and
              not any(q.is_symlink() for q in directory.rglob('*')), 'symlink source-view input')
    public, _ = specifications(root)
    for path in public.values():
        name = path.relative_to(root).as_posix()
        p.require(path.is_file() and re.fullmatch(r'ada/src/[A-Za-z0-9_.-]+\.ads', name),
                  'unsafe source-view ownership')
    return public


def evidence(root, site):
    native = {n: b for n, b in p.files(site).items() if n.startswith('api/')}
    p.require(native and all(re.fullmatch(r'api/(?:index\.html|[a-f0-9]{64}\.html|gnatdoc\.css)', n)
                            for n in native), 'source evidence requires pristine native API')
    return dict(schema_version=1, files=p.hashes(native),
                sources={path.relative_to(root).as_posix(): p.sha(path.read_bytes())
                         for path in sources(root).values()})


def options(root, site):
    configured = tomllib.loads((root / CONFIG).read_text()).get('source_view')
    if configured is None:
        return {}
    p.require(configured == dict(schema_version=1, mode='public-specifications',
                                native_manifest=EVIDENCE, native_manifest_sha256='0' * 64),
              'unexpected source-view opt-in contract')
    return dict(configured, native_manifest_sha256=p.sha(epoch.canonical(evidence(root, site))))


def prepare(root, site):
    """Only call in a disposable export, before any source-enabled config.load."""
    effective = options(root, site)
    if not effective:
        return CONFIG
    payload = epoch.canonical(evidence(root, site))
    path = root / EVIDENCE
    p.require(not path.is_symlink() and (not path.exists() or path.read_bytes() == payload),
              'source/native evidence changed')
    path.write_bytes(payload)
    original = (root / CONFIG).read_text()
    sentinel = 'native_manifest_sha256 = "' + '0' * 64 + '"'
    p.require(original.count(sentinel) == 1, 'source evidence binding must be unique')
    result = 'docs/gnatdoc-effective.toml'
    target = root / result
    p.require(not target.is_symlink(), 'symlink effective configuration')
    target.write_text(original.replace(sentinel, 'native_manifest_sha256 = "' + p.sha(payload) + '"'))
    p.require(p.sha(path.read_bytes()) == effective['native_manifest_sha256'], 'source evidence binding differs')
    return result


def page(name, raw, *, highlighted=True):
    """Independent serialization of the official schema-1 source page contract."""
    text = raw.decode('utf-8')
    p.require('\x00' not in text and '\r' not in text.replace('\r\n', ''), 'unsupported source text')
    def escape(value):
        return html.escape(value).replace('\r', '&#13;').replace('{', '&#123;').replace('}', '&#125;')
    if highlighted:
        # Use the existing MEL display lexer on the whole source (tick context
        # can cross a line). It cannot introduce links or choose source bytes.
        pieces = []
        for token in tokenize(text):
            escaped = escape(token.text)
            if token.kind in TOKEN_CLASSES:
                p.require('\n' not in token.text, 'multiline Ada display token')
                escaped = f'<span class="ada-{token.kind}">{escaped}</span>'
            pieces.append(escaped)
        display = ''.join(pieces)
    else:
        display = escape(text)
    parts = display.split('\n')
    lines = [part + '\n' for part in parts[:-1]] + ([parts[-1]] if parts[-1] else [])
    code = ''.join(f'<span class="source-line" id="L{i}"><a class="source-line-number" '
                   f'href="#L{i}" aria-label="Line {i}"></a>{line}</span>'
                   for i, line in enumerate(lines, 1))
    css = posixpath.relpath(CSS, posixpath.dirname('api/source/' + name + '.html'))
    value = ('<!DOCTYPE html>\n<html lang="en"><head><meta charset="utf-8">'
             '<meta name="viewport" content="width=device-width, initial-scale=1">'
             '<title>' + html.escape(name + ' — AMS MEL Bridge') + '</title><link rel="stylesheet" href="' +
             css + '"></head><body class="source-view"><h1>' + html.escape(name) + '</h1>' +
             '<p>Complete specification, including any private section. SHA-256: <samp>' + p.sha(raw) +
             '</samp></p><pre class="source-code"><code>' + code + '</code></pre></body></html>\n')
    return value.encode(), len(lines)


def source_pages(root, *, highlighted=True):
    return {'api/source/' + path.relative_to(root).as_posix() + '.html':
            page(path.relative_to(root).as_posix(), path.read_bytes(), highlighted=highlighted)[0]
            for path in sources(root).values()}


def reconstruct(root, site, authority):
    records, pages, insertions = [], {}, {}
    for package, path in sorted(sources(root).items(), key=lambda item: item[0].lower()):
        name = path.relative_to(root).as_posix()
        raw = path.read_bytes()
        destination = 'api/source/' + name + '.html'
        pages[destination], count = page(name, raw)
        masked = '\n'.join(code_tokens(line) for line in raw.decode().splitlines())
        header = HEADER.search(masked)
        p.require(header and not header[1] and header[2] == package, 'unproved public package ownership')
        package_line = masked[:header.start()].count('\n') + 1
        entries = [dict(kind='package', name=package, anchor=None, line=package_line)]
        native_page = next(n for n, pkg in authority.page_packages.items() if pkg == package)
        _, lines, header_line, boundary, declarations, _ = specification(path)
        public_tokens = {t[0] for t in lex('\n'.join(lines[header_line:boundary - 1]))}
        flat = not public_tokens & {'package', 'generic', 'task', 'protected', 'begin', 'declare'}
        has_operations = any(d['signature'].startswith(('function ', 'procedure ')) for d in declarations)
        # Native ledger matches full declaration profiles, including parameter
        # modes/types and returns. Never pair overloads by name or ordinal.
        for (native, anchor), entry in sorted(authority.sections.items(), key=lambda item: item[1]['line']):
            if native != native_page:
                continue
            kind = entry['identity'].split(': ', 3)[2]
            if kind == 'object' and re.search(r':\s*constant\b', entry['declaration'], re.I):
                if not flat or not has_operations:
                    continue  # Official v1 constant support is limited to flat callable packages.
                kind = 'constant'
            if kind not in {'type', 'subtype', 'function', 'procedure', 'constant'}:
                continue
            entries.append(dict(kind=kind, name=package + '.' + entry['name'],
                                anchor=anchor, line=entry['line']))
        p.require(all(1 <= e['line'] <= count for e in entries), 'source destination outside file')
        insertions['api/' + native_page] = (package, name, entries)
        records.append(dict(package=package, source=name, page=destination, sha256=p.sha(raw),
                            lines=count, declarations=entries))
    return pages, insertions, dict(schema_version=1, mode='public-specifications',
                                  native_evidence=options(root, site), files=records)


def restore(text, original, record):
    package, source, entries = record
    p.require(text.count(HEAD) == 1 and
              text.count('<link rel="stylesheet" href="gnatdoc-theme.css">' + HEAD + '</head>') == 1,
              'missing/duplicate/misplaced source stylesheet')
    text = text.replace(HEAD, '', 1)
    for entry in entries:
        if entry['anchor'] is None:
            marker = '<h1>' + html.escape(package) + '</h1>'
        else:
            anchor = entry['anchor']
            found = re.findall(r'<h4 id=(?:"' + anchor + '"|' + anchor + r')>[^<]+</h4>', original)
            p.require(len(found) == 1, 'ambiguous native source-link destination')
            marker = found[0]
        link = ('<a class="view-source" target="_top" href="source/' + source +
                '.html#L' + str(entry['line']) + '">View source</a>')
        p.require(text.count(marker + link) == 1, 'wrong/missing/duplicate declaration source destination')
        text = text.replace(marker + link, marker, 1)
    p.require('view-source' not in text and 'gnatdoc-source' not in text,
              'unauthorized source-view insertion')
    p.require(not p.Page(text).issues, 'unsafe declaration publication HTML')
    return text


def qualify(root, site, authority, actual):
    if not options(root, site):
        return {}, {}, None
    pages, insertions, manifest = reconstruct(root, site, authority)
    p.require({n for n in actual if n.startswith('api/source/')} == set(pages),
              'source-page inventory mismatch')
    for name, expected in pages.items():
        # Exact equality checks the token classes and boundaries as well as the
        # escaped text, complete ordered anchors, and absence of active markup.
        p.require(actual[name] == expected, 'source page differs from approved text/markup: ' + name)
    p.require(CSS in actual and p.sha(actual[CSS]) == CSS_SHA256, 'source stylesheet identity mismatch')
    return pages, insertions, manifest
