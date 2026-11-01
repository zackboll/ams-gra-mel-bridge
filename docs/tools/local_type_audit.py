"""Bounded independent acceptance of local type links, not an Ada resolver.

Authority is the original MEL declaration index, immutable native anchor ledger,
and pristine snippets. No enhancer inventory, resolver, counters or manifest is
used here. Unsupported grammar or visibility fails closed for emitted links.
"""

import html
import json
import re
from difflib import SequenceMatcher

from check_doc_coverage import declaration_identity
from gnatdoc_coverage_model import specification
from gnatdoc_inventory import specifications

LEX = re.compile(r'--[^\n]*|"(?:""|[^"\n])*"|\'[^\n]\'|\'[A-Za-z][\w]*|'
                 r'[A-Za-z][\w]*|:=|=>|\.\.|\S')
IDENT = re.compile(r'[A-Za-z][A-Za-z0-9_]*\Z')
LINK = re.compile(r'<a class="type-reference" href="([a-f0-9]{64}\.html#[a-f0-9]{64})" '
                  r'data-type="([a-z][a-z0-9_]*(?:\.[a-z][a-z0-9_]*)+)">([^<>]+)</a>')
SPAN = re.compile(r'<span class="ada-(?:keyword|number|string|character|attribute|comment|operator)">([^<]*)</span>')
SECTION = re.compile(r'(<h4 id=([a-f0-9]{64})>[^<]+</h4><pre class=ada-code-snippet><code>)(.*?)(</code></pre>)', re.S)


def require(ok, message):
    if not ok:
        raise ValueError("independent type audit: " + message)


def lex(text):
    return [(m[0].lower(), m.start(), m.end()) for m in LEX.finditer(text)
            if not m[0].startswith('--')]


def flat_public_tokens(pkg):
    """Establish a complete flat public specification from masked source tokens."""
    tokens = lex(pkg['source'])
    words = [t[0] for t in tokens]
    header = next((i for i, word in enumerate(words) if word == 'package'), None)
    require(header is not None and 'generic' not in words[:header], 'unsupported generic package scope')
    cursor = header + 1
    parts = []
    while cursor < len(words) and words[cursor] != 'is':
        parts.append(words[cursor])
        cursor += 1
    require(''.join(parts) == pkg['name'].lower() and cursor < len(words), 'unproved package header')
    public = words[cursor + 1:]
    for i, word in enumerate(public):
        if word == 'private' and (i + 1 == len(public) or public[i + 1] != ';'):
            require(public[i + 1:i + 2] != ['with'], 'unsupported partial view with aspects')
            public = public[:i]
            break
    else:
        closing = ['end'] + parts + [';']
        require(public[-len(closing):] == closing, 'unproved package closure')
        public = public[:-len(closing)]
    require(not set(public) & {'package', 'generic', 'task', 'protected', 'begin', 'declare', 'renames'},
            'unsupported nested/alias/instance public scope')
    return public


def type_context(tokens, index, end):
    """Accept a token in a small declaration grammar, not keyword adjacency alone."""
    words = [t[0] for t in tokens]
    require(index < len(words) and IDENT.fullmatch(words[index]), "not an identifier")
    require(index == 0 or words[index - 1] != '.', "partial qualified name")
    cursor = index + 1
    while cursor + 1 < len(words) and words[cursor] == '.' and IDENT.fullmatch(words[cursor + 1]):
        cursor += 2
    require(tokens[cursor - 1][2] == end, "not a complete type mark")
    before = index - 1
    while before >= 0 and words[before] in ('in', 'out', 'not', 'null', 'aliased', 'constant', 'access', 'all'):
        before -= 1
    head = words[0] if words else ''
    if before >= 0 and words[before] == ':':
        # The colon must introduce formal/component/object names, never an
        # expression or default. Reject a default/aspect before the separator
        # in the same semicolon-delimited declaration segment.
        start = before - 1
        require(start >= 0 and IDENT.fullmatch(words[start]), "colon lacks defining name")
        while start > 1 and words[start - 1] == ',' and IDENT.fullmatch(words[start - 2]):
            start -= 2
        boundary = max([-1] + [j for j in range(start)
                              if words[j] in (';', 'record', '=>', '(', ')')])
        segment = words[boundary + 1:start]
        require(':=' not in segment and 'with' not in segment, "type mark in expression/default/aspect")
        return 'colon'
    if index and words[index - 1] == 'return':
        require(head in ('function', 'overriding', 'not', 'type'), "return outside callable")
        depth = 0
        for word in words[:index - 1]:
            if word == '(':
                depth += 1
            elif word == ')':
                depth -= 1
            elif depth == 0:
                require(word not in (':=', 'with'), "return in default/aspect")
        require(depth == 0 or (head == 'type' and 'access' in words[:index]), "return inside expression")
        return 'return'
    if index and words[index - 1] == 'of':
        require(head == 'type' and 'array' in words[:index] and words[index - 2] == ')', "of outside array element")
        return 'array-element'
    prefix = words[:index]
    if len(prefix) == 3 and prefix[0] == 'subtype' and prefix[2] == 'is' and IDENT.fullmatch(prefix[1]):
        require(words[cursor:] == [';'], 'unsupported ordinary subtype indication')
        return 'subtype-base'
    if head in ('type', 'subtype') and 'is' in prefix:
        suffix = prefix[prefix.index('is') + 1:]
        require(suffix in (['new'], ['access'], ['access', 'all'], ['not', 'null', 'access']), "unsupported type definition")
        return 'derived' if suffix == ['new'] else 'access'
    raise ValueError('independent type audit: unsupported/non-type context')


class Authority:
    def __init__(self, root, pristine, ledger=None):
        self.root = root
        self.ledger = (json.loads((root / 'docs/gnatdoc-native-anchors.json').read_text())
                       if ledger is None else ledger)
        self.packages, self.sections, self.types, self.page_packages = {}, {}, {}, {}
        native_sections = {}
        headings = {}
        for path in sorted((pristine / 'api').glob('*.html')):
            if path.name != 'index.html':
                matches = re.findall(r'<h1>([^<>]+)</h1>', path.read_text())
                require(len(matches) == 1, 'missing/duplicate native package heading')
                require(matches[0] not in headings, 'duplicate native package identity')
                headings[matches[0]] = path.name
            for match in SECTION.finditer(path.read_text()):
                key = (path.name, match[2])
                require(key not in native_sections, 'duplicate pristine section')
                native_sections[key] = html.unescape(match[3])
        for package, path in specifications(root)[0].items():
            _, lines, header, boundary, declarations, _ = specification(path)
            key = package.lower()
            source = '\n'.join(lines)
            require(package in headings, 'source package lacks unique native heading')
            self.page_packages[headings[package]] = package
            # Clauses are obtained from source, not enhancer metadata. Literal
            # and comment tokens cannot grant visibility.
            context = ' '.join(t[0] for t in lex('\n'.join(lines[:header - 1])))
            imports = set()
            for m in re.finditer(r'\bwith\s+([^;]+);', context):
                imports.update(re.sub(r'\s*\.\s*', '.', n.strip()) for n in m[1].split(','))
            uses = set()
            for m in re.finditer(r'(?m)^\s*use\s+(?!type\b)([\w., ]+);', '\n'.join(lines[:boundary - 1]), re.I):
                uses.update(n.strip().lower() for n in m[1].split(','))
            self.packages[key] = {'declarations': declarations, 'imports': imports, 'uses': uses,
                                  'source': source, 'name': package}
            for decl in declarations:
                identity = declaration_identity(package, lines, decl)
                native = self.ledger.get(identity)
                if native is None:  # The exact original omission is checked by coverage.
                    continue
                raw = '\n'.join(lines[decl['start'] - 1:decl['end']])
                entry = {'package': key, 'source': path.relative_to(root).as_posix(),
                         'line': decl['start'], 'declaration': raw, 'identity': identity,
                         'page': native['page'], 'anchor': native['anchor'], 'name': decl['name']}
                section_key = (native['page'], native['anchor'])
                require(section_key not in self.sections, 'duplicate source/native section')
                require(section_key in native_sections, 'missing pristine section')
                entry['native'] = native_sections[section_key]
                self.sections[section_key] = entry
                if decl['signature'].startswith(('type ', 'subtype ')):
                    target = key + '.' + decl['name'].lower()
                    require(target not in self.types, 'duplicate source type')
                    self.types[target] = entry

    def current_package(self, spelling, owner, entry):
        """A separate conservative token proof, not suffix matching or a resolver.

        Only flat public record components are admitted. Any standalone use of
        the package's final identifier anywhere in its public specification is a
        potential defining name and blocks this rule. This deliberately covers
        same-line/comma-list objects, components, enumeration literals, labels,
        formals and aliases without relying on the old declaration inventory.
        Comments and literals are masked by tokenization, not regex line scans.
        """
        pkg = self.packages[owner]
        short = owner.rsplit('.', 1)[-1]
        parts = spelling.lower().split('.')
        require(len(parts) == 2 and parts[0] == short, 'not a current-package type mark')
        require(pkg.get('name', '').lower() == owner and entry is not None,
                'current package identity/scope not established')
        words = flat_public_tokens(pkg)
        require(not any(words[i:i + 2] == ['is', 'new'] for i in range(len(words))),
                'unsupported inherited defining names')
        for i, word in enumerate(words):
            if word == short:
                # Header and end names are fully qualified in these child specs;
                # only an actual selected-name token can safely use this name.
                require((i > 0 and words[i - 1] == '.') or
                        (i + 1 < len(words) and words[i + 1] == '.'),
                        'shadowed/ambiguous current-package prefix')
        record = [t[0] for t in lex(entry['declaration'])]
        require(any(words[i:i + len(record)] == record for i in range(len(words))),
                'unsupported/nonpublic current-package containing declaration')
        require(len(record) > 7 and record[:1] == ['type'] and
                record[2:4] == ['is', 'record'] and record[-3:] == ['end', 'record', ';'],
                'unsupported current-package containing scope')
        require(not set(record[4:-3]) & {'record', 'case', 'when', 'access', 'renames', 'new', 'with', ':='},
                'unsupported current-package record grammar')
        target = owner + '.' + parts[1]
        require(target in self.types and self.types[target]['package'] == owner,
                'unknown/private current-package type')
        target_words = [t[0] for t in lex(self.types[target]['declaration'])]
        require(any(words[i:i + len(target_words)] == target_words for i in range(len(words))),
                'unsupported/nonpublic current-package target declaration')
        return target, self.types[target]

    def destination(self, spelling, owner, entry=None):
        """Only ordinary current/ancestor/full-name/use visibility is accepted."""
        name = spelling.lower()
        pkg = self.packages[owner]
        if '.' in name:
            target_owner = name.rpartition('.')[0]
            if target_owner == owner.rsplit('.', 1)[-1] and target_owner != owner:
                return self.current_package(spelling, owner, entry)
            require(target_owner == owner or owner.startswith(target_owner + '.') or target_owner in pkg['imports'],
                    'qualified owner not source-visible')
            candidates = [name] if name in self.types else []
        else:
            candidates = []
            current = owner
            while current:
                scope = self.packages.get(current, {})
                declarations = [d for d in scope.get('declarations', []) if d['name'].lower() == name]
                if declarations:
                    target = current + '.' + name
                    require(target in self.types and len(declarations) == 1, 'shadowed/ambiguous type')
                    candidates = [target]
                    break
                current = current.rpartition('.')[0]
            if not candidates:
                candidates = [p + '.' + name for p in pkg['uses'] & pkg['imports'] if p + '.' + name in self.types]
        require(len(candidates) == 1, 'unknown/ambiguous/alias-dependent target')
        return candidates[0], self.types[candidates[0]]

    def occurrence(self, page, anchor, text, start, end, spelling, href, identity):
        require((page, anchor) in self.sections, 'unqualified containing declaration')
        entry = self.sections[page, anchor]
        require(text == entry['native'], 'changed native declaration')
        require(text[start:end] == spelling, 'changed visible token')
        tokens = lex(text)
        indices = [i for i, token in enumerate(tokens) if token[1] == start]
        require(len(indices) == 1, 'occurrence inside literal/comment/attribute')
        index = indices[0]
        context = type_context(tokens, index, end)
        if context == 'subtype-base':
            flat_public_tokens(self.packages[entry['package']])
        source_tokens = lex(entry['declaration'])
        matcher = SequenceMatcher(None, [t[0] for t in tokens], [t[0] for t in source_tokens], autojunk=False)
        blocks = [b for b in matcher.get_matching_blocks() if b.a <= index and index < b.a + b.size]
        require(len(blocks) == 1, 'native token lacks source ownership')
        source_index = blocks[0].b + index - blocks[0].a
        source_end = source_index + next(i for i in range(index, len(tokens)) if tokens[i][2] == end) - index
        require(source_end < blocks[0].b + blocks[0].size, 'type mark crosses source projection')
        source_start, source_stop = source_tokens[source_index][1], source_tokens[source_end][2]
        require(type_context(source_tokens, source_index, source_stop) == context, 'source context disagrees')
        target_identity, target = self.destination(spelling, entry['package'], entry)
        if context == 'subtype-base':
            require('private' not in [t[0] for t in lex(target['declaration'])],
                    'private ordinary subtype target')
        require(identity == target_identity, 'forged type identity')
        require(href == target['page'] + '#' + target['anchor'], 'wrong existing/native destination')
        return {'containing': entry, 'native_token_start': start, 'native_token_end': end,
                'visible': spelling, 'context': context, 'source_token_start': source_start,
                'source_token_end': source_stop, 'source_line': entry['line'] + entry['declaration'][:source_start].count('\n'),
                'target_identity': target_identity, 'target': target, 'href': href}

    def required_additions(self):
        """Derive eligible new occurrences solely from trusted source/native input.

        This runs before candidate HTML is read. Neither enhancer counters nor
        emitted wrappers supply the expected set. The reviewed boundary is one
        complete ordinary subtype base and current-package flat record fields.
        """
        records = []
        for (page, anchor), entry in self.sections.items():
            tokens = lex(entry['native'])
            for i, token in enumerate(tokens):
                if not IDENT.fullmatch(token[0]) or (i and tokens[i - 1][0] == '.'):
                    continue
                last = i
                while last + 2 < len(tokens) and tokens[last + 1][0] == '.' and IDENT.fullmatch(tokens[last + 2][0]):
                    last += 2
                start, end = token[1], tokens[last][2]
                spelling = entry['native'][start:end]
                try:
                    context = type_context(tokens, i, end)
                    current = (last == i + 2 and token[0] == entry['package'].rsplit('.', 1)[-1])
                    if context != 'subtype-base' and not current:
                        continue
                    identity, target = self.destination(spelling, entry['package'], entry)
                    records.append(self.occurrence(page, anchor, entry['native'], start, end,
                                                   spelling, target['page'] + '#' + target['anchor'], identity))
                except ValueError:
                    continue  # Unsupported source grammar is not eligible.
        require(sum(r['context'] == 'subtype-base' for r in records) == 1 and len(records) == 10,
                'trusted ten-occurrence eligibility changed')
        return records


def occurrence_key(record):
    return (record['containing']['page'], record['containing']['anchor'],
            record['native_token_start'], record['native_token_end'])


def require_complete_additions(audit, expected):
    keys = {occurrence_key(r): r for r in expected}
    actual = [r for r in audit if occurrence_key(r) in keys or r['context'] == 'subtype-base' or
              r['visible'].lower().split('.')[0] == r['containing']['package'].rsplit('.', 1)[-1]]
    require(len(actual) == len(expected) and
            {occurrence_key(r): r for r in actual} == keys,
            'missing/extra/changed required type-link occurrence')


def restore_page(page, enhanced, authority):
    """Remove exact wrappers only in source-qualified native declaration sections."""
    audit = []

    def restore(match):
        # A lexical span inside an anchor is not one of the authorized flat
        # wrappers, even if removing it would leave a valid-looking link.
        for opening in re.finditer(r'<a\b[^>]*>.*?</a>', match[3], re.S):
            require(LINK.fullmatch(opening[0]) is not None, 'unexpected/nested anchor markup')
        raw = SPAN.sub(lambda m: m[1], match[3])
        links = list(LINK.finditer(raw))
        plain = LINK.sub(lambda m: m[3], raw)
        require('<' not in plain, 'unexpected/nested/malformed snippet markup')
        text = html.unescape(plain)
        removed = 0
        for link in links:
            start = len(html.unescape(raw[:link.start()])) - removed
            spelling = html.unescape(link[3])
            audit.append(authority.occurrence(page, match[2], text, start, start + len(spelling),
                                              spelling, link[1], link[2]))
            removed += len(link[0]) - len(link[3])
        return match[1] + plain + match[4]

    restored = SECTION.sub(restore, enhanced)
    require('type-reference' not in restored, 'link outside qualified declaration')
    return restored, audit
