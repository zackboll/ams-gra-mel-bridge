#!/usr/bin/env python3
"""Check structured trailing GNATdoc annotations on changed public Ada APIs.

Source-only, local development check; no renderer, baseline or CI policy changes.
The comparison is the specified Git tree versus the current working files,
including committed, staged, unstaged and untracked specifications.
"""

import argparse
from collections import Counter
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from check_doc_coverage import attached_block, source_prose
from check_doc_source_integrity import tokens
from compare_gnatdoc_warnings import (
    HEADER_RE, WarningComparisonError, _declaration_span, _normalize, code_tokens,
)
from gnatdoc_coverage_model import CoverageError, expected_items, specification, without_aspects
from gnatdoc_inventory import HEADER


class AnnotationError(ValueError):
    """Git or source cannot be inspected safely."""


def git(root, *args):
    result = subprocess.run(['git', '-C', str(root), *args], capture_output=True)
    if result.returncode:
        raise AnnotationError(result.stderr.decode(errors='replace').strip())
    return result.stdout


def normalized_code(text):
    """Keep literal/default contents; ignore Ada identifier case and layout."""
    return tuple(token if token.startswith(('"', "'")) else token.lower()
                 for token in tokens(text))


def annotation_sections(block):
    """Keep tag occurrences separate (source_prose deliberately combines them)."""
    sections, current = [], []
    for line in block:
        if line.startswith('@'):
            if current:
                sections.append(current)
            current = []
        current.append(line)
    if current:
        sections.append(current)
    return sections


def normalized_docs(block):
    return tuple(' '.join(' '.join(section).split()) for section in annotation_sections(block))


def source_model(text, filename):
    """Adapt existing inventory/model helpers, never parse Ada profiles anew.

    Inventory visibility is library packages in ada/src, excluding private units
    and everything after the first private boundary. Nested public packages are
    diagnosed rather than pretending this top-level model supports them.
    """
    lines = text.splitlines()
    code = [code_tokens(line) for line in lines]
    header = HEADER.search('\n'.join(code))
    if header is None:
        raise AnnotationError('unsupported library-unit header')
    if header[1]:
        return []
    boundary = next((n for n, line in enumerate(code)
                     if line.strip().lower() == 'private'), len(lines))
    # Lowercase code only: the inherited scanner is case-sensitive internally.
    # Retain line/column positions and use original source for literal identities.
    public = [line.lower() for line in code[:boundary]]
    if boundary < len(lines):
        public.append('end ' + header[2].lower() + ';')
    with tempfile.TemporaryDirectory(prefix='ada-doc-model-') as temporary:
        path = Path(temporary) / Path(filename).name
        path.write_text('\n'.join(public) + '\n', encoding='utf-8')
        package, _, _, _, declarations, members = specification(path)
    result = []
    header_start = '\n'.join(code).count('\n', 0, header.start())
    header_end, _, _ = _declaration_span(public, header_start, 'package')
    covered = set(range(header_start + 1, header_end + 2))
    if re.search(r'\bis\s+\S', public[header_end], re.I):
        raw = lines[header_end]
        result.append(dict(start=header_end + 1, end=header_end + 1, display=header[2],
                           raw=raw, scope=package, signature='', block=[],
                           unsupported='unsupported code on package header line; put declarations on separate lines',
                           identity=(package, normalized_code(raw), ())))
    for declaration in declarations:
        start, end = declaration['start'], declaration['end']
        covered.update(range(start, end + 1))
        if declaration['scope'] == package:
            continue
        raw = '\n'.join(lines[start - 1:end])
        block = attached_block(lines[:boundary], declaration)
        # Canonical token spacing also supports compact F(X:T) profiles.
        declaration['signature'] = _normalize(' '.join(tokens(declaration['signature'])))
        declaration['signature'] = re.sub(r'^(?:(?:overriding|not|abstract|private) )+',
                                          '', declaration['signature'])
        name = re.match(r'[\w.]+', lines[start - 1][declaration['column'] - 1:])
        display = name[0] if name else declaration['name']
        declaration.update(display=display, block=block, raw=raw)
        declaration['identity'] = (declaration['scope'], normalized_code(raw), normalized_docs(block))
        declaration['members'] = members
        result.append(declaration)
    # A header the shared index skipped (e.g. an operator-symbol function) is
    # unsupported, not a successful zero-parameter declaration. Use its existing
    # span helper to locate trailing docs and identify changes.
    for number, line in enumerate(public[:boundary], 1):
        match = HEADER_RE.match(line)
        if not match or number in covered:
            continue
        end, _, _ = _declaration_span(public, number - 1, match[3])
        declaration = {'start': number, 'end': end + 1}
        block = attached_block(lines[:boundary], declaration)
        raw = '\n'.join(lines[number - 1:end + 1])
        display = lines[number - 1].strip()
        result.append(dict(declaration, display=display, block=block, raw=raw,
                           signature='', scope=package,
                           unsupported='unsupported public declaration; shared index cannot identify it',
                           identity=(package, normalized_code(raw), normalized_docs(block))))
    return result


def meaningful(text):
    return bool(re.search(r'\w', text)) and not re.search(r'\b(?:TODO|TBD|FIXME)\b', text, re.I)


def validate(declaration):
    """Return actionable annotation errors for one selected declaration."""
    if 'unsupported' in declaration:
        return [declaration['unsupported']]
    signature = declaration['signature']
    kind = signature.split()[0]
    if kind == 'package' and ' is' in signature and ' renames ' not in signature:
        return ['unsupported nested public package; shared model cannot resolve its visibility']
    if re.search(r';\s*(?:(?:overriding|not)\s+)*(?:function|procedure|type|package)\b',
                 ' '.join(code_tokens(line) for line in declaration['raw'].splitlines()), re.I):
        return ['unsupported multiple declarations on one line; put each declaration on its own line']
    if declaration.get('ambiguous'):
        return ['ambiguous public declaration identity']
    # Instantiations have no locally available formal profile to validate.
    if kind in ('function', 'procedure') and ' is new ' in signature:
        return ['unsupported generic subprogram instantiation; formal profile is unavailable']
    if kind == 'type' and re.search(r'\brecord\b', signature):
        code = ' '.join(code_tokens(line) for line in declaration['raw'].splitlines())
        if re.search(r'\bcase\b', code, re.I):
            return ['unsupported variant record; shared model cannot verify all public components']
        if any(line.count(';') > 1 for line in
               (code_tokens(line) for line in declaration['raw'].splitlines()[1:])):
            return ['unsupported multiple record components on one line; put each component on its own line']
    expected = {'param': [], 'return': [], 'field': []}
    model = dict(declaration, signature=without_aspects(signature))
    try:
        for heading, names in expected_items(model, declaration['members']):
            tag = {'Parameters': 'param', 'Return Value': 'return', 'Record fields': 'field'}.get(heading)
            if tag and (tag != 'field' or re.search(r'\brecord\b', signature)):
                expected[tag] = names
    except CoverageError as error:
        return [f'unsupported public profile: {error}']
    # Retain declaration spelling for actionable diagnostics.
    spelling = {token.lower(): token for token in tokens(declaration['raw'])
                if re.fullmatch(r'[A-Za-z][A-Za-z0-9_]*', token)}
    found, errors = Counter(), []
    for section in annotation_sections(declaration['block']):
        if not section[0].startswith('@'):
            continue
        match = re.match(r'@(param|return|field|enum|formal|exception)\b\s*(.*)', section[0])
        if match is None:
            errors.append(f'malformed or unsupported structured tag {section[0]}')
            continue
        tag, content = match.groups()
        if tag not in expected:
            continue  # Existing optional enum/formal/exception conventions unchanged.
        parts = content.split(maxsplit=1)
        name = parts[0] if tag != 'return' and parts else None
        key = (tag, name.lower() if name else None)
        found[key] += 1
        label = '@' + tag + (' ' + name if name else '')
        if key[1] not in expected[tag]:
            errors.append(f'unexpected {label}')
        if found[key] > 1:
            errors.append(f'duplicate {label}')
        if not meaningful(source_prose(section, tag, name)):
            errors.append(f'empty or placeholder {label}')
    for tag, names in expected.items():
        for name in names:
            if not found[(tag, name)]:
                errors.append('missing @' + tag + (' ' + spelling.get(name, name) if name else ''))
    return errors


def check(root, base):
    try:
        revision = git(root, 'rev-parse', '--verify', '--end-of-options', base + '^{commit}').decode().strip()
    except AnnotationError as error:
        raise AnnotationError(f'base revision {base!r} is unavailable; fetch it or specify --base SHA') from error
    changed = git(root, 'diff', '--name-only', '-z', revision, '--', 'ada/src/*.ads')
    untracked = git(root, 'ls-files', '--others', '--exclude-standard', '-z', '--', 'ada/src/*.ads')
    base_paths = set(git(root, 'ls-tree', '-r', '--name-only', '-z', revision, '--', 'ada/src').decode().split('\0'))
    inspected, diagnostics = 0, []
    for filename in sorted(set((changed + untracked).decode().split('\0')) - {''}):
        path = root / filename
        if not path.exists():
            continue  # Deleted declarations impose no annotation requirements.
        if path.is_symlink() or not path.is_file():
            diagnostics.append(f'{filename}:1: GNATDOC: unsupported nonregular specification')
            continue
        try:
            current = source_model(path.read_text(encoding='utf-8'), filename)
            old = source_model(git(root, 'show', revision + ':' + filename).decode(), filename) if filename in base_paths else []
            remaining = Counter(d['identity'] for d in old)
            identities = Counter((d['scope'], normalized_code(d['raw'])) for d in current)
            for declaration in current:
                identity = declaration['identity']
                if remaining[identity]:
                    remaining[identity] -= 1
                    continue
                inspected += 1
                if identities[(declaration['scope'], normalized_code(declaration['raw']))] > 1:
                    declaration['ambiguous'] = True
                for error in validate(declaration):
                    diagnostics.append(f"{filename}:{declaration['start']}: GNATDOC: {declaration['display']} {error}")
        except (CoverageError, WarningComparisonError, AnnotationError) as error:
            location = re.search(r'line (\d+)', str(error))
            diagnostics.append(f'{filename}:{location[1] if location else 1}: GNATDOC: cannot safely inspect changed public source: {error}')
    return inspected, diagnostics


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', default='origin/main', help='Git base tree (default: origin/main)')
    args = parser.parse_args(argv)
    try:
        root = Path(git(Path.cwd(), 'rev-parse', '--show-toplevel').decode().strip())
        inspected, diagnostics = check(root, args.base)
    except (AnnotationError, OSError, UnicodeError) as error:
        print(f'GNATDOC: error: {error}', file=sys.stderr)
        return 2
    print('Ada GNATdoc annotation check')
    for diagnostic in diagnostics:
        print(diagnostic)
    print(f'Public declarations inspected: {inspected}')
    print(f'Missing or invalid annotations: {len(diagnostics)}')
    print('FAIL' if diagnostics else 'PASS')
    return 1 if diagnostics else 0


if __name__ == '__main__':
    sys.exit(main())
