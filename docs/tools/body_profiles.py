"""Independent lexical profile gate, tied to freshly compiled Ada entities.

The source scanner is deliberately MEL-format-specific. GNAT cross-references
independently prove overload identity and prevent a layout coincidence (e.g. a
nested/private helper) from establishing correspondence. No ALI match by itself
is sufficient to authorize a link.
"""

from dataclasses import dataclass
import re

from body_source_inventory import NAME, IDENTIFIER, require, words, xrefs

STANDARD = set('''Boolean Character Wide_Character Wide_Wide_Character String Wide_String
Wide_Wide_String Integer Natural Positive Short_Integer Short_Short_Integer Long_Integer
Long_Long_Integer Float Short_Float Long_Float Long_Long_Float Duration'''.lower().split())
CALLABLES = {'function', 'procedure'}
FOREIGN = {'import', 'interface', 'convention', 'import_function', 'import_procedure', 'interface_name'}


@dataclass(frozen=True)
class Header:
    kind: str
    name: str
    line: int
    defining: tuple
    profile: tuple
    form: str
    public: bool


def flat(items, separators):
    level = 0
    for i, item in enumerate(items):
        if level == 0 and item in separators:
            yield i
        level += (item in {'(', '['}) - (item in {')', ']'})
        require(level >= 0, 'unbalanced Ada profile')


def scan(source):
    """Index headers in MEL's canonical three-column indentation domain."""
    stream = list(words(source.raw))
    values = [t.text if t.kind in {'string', 'character'} else t.text.lower() for t, _, _ in stream]
    headers, bindings, public_types, foreign = [], {}, {}, set()
    is_body = source.path.endswith('.adb')
    public = not is_body
    start = 0
    while start < len(values):
        _, line, column = stream[start]
        word = values[start]
        if not source.opening < line < source.closing:
            start += 1
            continue
        if column == 1 and word == 'begin' and is_body:
            break
        if column != 4:
            start += 1
            continue
        if word == 'private' and not is_body:
            public = False
        if word in {'type', 'subtype', 'package', 'task', 'protected'}:
            name = values[start+1]
            bindings.setdefault(name, set()).add(word)
            if public and word in {'type', 'subtype'}:
                public_types.setdefault(name, line)
        elif word == 'pragma' and values[start+1] in FOREIGN:
            foreign.update(values[start+2:values.index(';', start)])
        elif word not in CALLABLES | {'not', 'overriding'}:
            end = start
            while re.fullmatch(IDENTIFIER, values[end]) and values[end+1] in {':', ','}:
                bindings.setdefault(values[end], set()).add('object')
                if values[end+1] == ':':
                    break
                end += 2
        if word not in CALLABLES | {'not', 'overriding'}:
            start += 1
            continue
        at = start + (2 if word == 'not' else int(word == 'overriding'))
        kind, name = values[at:at+2]
        if kind not in CALLABLES:
            start += 1
            continue
        bindings.setdefault(name, set()).add(kind)
        stop = at + 2 + next(flat(values[at+2:], {';', 'is', 'with', 'renames'}))
        form = values[stop]
        profile = tuple(values[at+2:stop])
        if form == 'with':
            end = stop + 1 + next(flat(values[stop+1:], {';', 'is', 'renames'}))
            aspects = [values[stop+1], *(values[stop+2+i] for i in flat(values[stop+1:end], {','}))]
            form = values[end] if set(aspects) <= {'pre', 'post', 'global', 'depends'} else 'aspects'
            stop = end
        if form == 'is':
            form = {'(': 'expression', 'separate': 'separate', 'abstract': 'abstract',
                    'new': 'instance', 'null': 'null'}.get(values[stop+1], 'body')
        form = {';': 'declaration', 'renames': 'renaming'}.get(form, form)
        headers.append(Header(kind, name, line, stream[at+1][1:], profile, form, public))
        start = stop + 1
    return headers, bindings, public_types, foreign


def split(items, delim):
    bounds = [-1, *flat(items, {delim}), len(items)]
    return [items[start+1:end] for start, end in zip(bounds, bounds[1:])]


def profile(header, resolve):
    items = header.profile
    formals, seen = [], set()
    if items[:1] == ('(',):
        # The return boundary is outside the parameter list; nested returns
        # (e.g. anonymous access-to-function) cannot delimit this list.
        end = next(flat(items, {'return'}), len(items))
        if items[end-1:end] != (')',):
            return None
        for group in split(items[1:end-1], ';'):
            if ':' not in group:
                return None
            names, rest = group[:group.index(':')], group[group.index(':')+1:]
            if not re.fullmatch(IDENTIFIER + r'(?:,' + IDENTIFIER + r')*', ''.join(names)):
                return None
            mode = 'in'
            for prefix in [('in', 'out'), ('in',), ('out',)]:
                if rest[:len(prefix)] == prefix:
                    mode, rest = ' '.join(prefix), rest[len(prefix):]
                    break
            default = ''
            if ':=' in rest:
                index = rest.index(':=')
                literal = rest[index+1:]
                default = literal[0] if len(literal) == 1 and re.fullmatch(r'[0-9]+(?:_[0-9]+)*', literal[0]) else None
                rest = rest[:index]
            if not re.fullmatch(NAME, ''.join(rest)) or len(rest) % 2 != 1:
                return None
            subtype = None if rest[0] in seen else resolve(rest)
            for name in names[::2]:
                if name in seen:
                    return None
                seen.add(name)
                formals.append((name, mode, subtype, default))
        items = items[end:]
    result = ''
    if header.kind == 'function':
        if items[:1] != ('return',) or not re.fullmatch(NAME, ''.join(items[1:])) or len(items) % 2:
            return None
        result = None if items[1] in seen else resolve(items[1:])
    elif items:
        return None
    return tuple(formals), result


def resolve(mark, line, chain, scans, body):
    name = ''.join(mark)
    root = mark[0]
    if body and root in scans[body.path][1]:
        return None
    owner, _, member = name.rpartition('.')
    for source in chain:
        _, bindings, public_types, _ = scans[source.path]
        if owner and root in bindings:
            return None
        if owner == source.name.lower() or not owner and member in bindings:
            if (bindings.get(member, set()) <= {'type', 'subtype'} and member in public_types and
                    (source != chain[0] or body or public_types[member] < line)):
                return source.name.lower() + '.' + member
            return None
    if owner in {'', 'standard'}:
        return 'standard.' + member if member in STANDARD else None
    if owner in chain[0].context and not (body and any(
            item.startswith(owner + '.') and name.startswith(item) for item in body.context)):
        return name
    return None


def resolved(value):
    return value is not None and value[1] is not None and all(None not in formal for formal in value[0])


def overlaps(a, b):
    if a is None or b is None:
        return True
    if len(a[0]) != len(b[0]):
        return False
    pairs = [(x[2], y[2]) for x, y in zip(a[0], b[0])] + [(a[1], b[1])]
    return all(x is None or y is None or x == y for x, y in pairs)


def correspondence(spec, body, declarations, specs, scans, ali):
    if not body:
        return {e['anchor']: (None, 'no-qualified-package-body') for e in declarations}
    chain = [spec]
    key = spec.name.lower()
    while '.' in key:
        key = key.rpartition('.')[0]
        require(key in specs, 'missing lexical ancestor: ' + key)
        chain.append(specs[key])

    def candidates(source, implementation):
        result = {}
        for header in scans[source.path][0]:
            value = profile(header, lambda mark: resolve(mark, header.line, chain, scans, implementation))
            result.setdefault((header.kind, header.name), []).append((header, value))
        return result

    left, right = candidates(spec, None), candidates(body, body)
    refs = xrefs(ali, spec, body)
    result = {}
    for entry in declarations:
        name = entry['name'].rsplit('.', 1)[1].lower()
        kind = entry['kind']
        headers = left.get((kind, name), [])
        declaration = [(h, p) for h, p in headers if h.line == entry['line'] and h.public]
        require(len(declaration) == 1, 'missing/ambiguous direct public source header: ' + entry['name'])
        header, expected = declaration[0]
        linked = None
        if header.form != 'declaration':
            reason = 'specification-' + header.form
        elif name in (scans[spec.path][3] | scans[body.path][3]):
            reason = 'foreign-or-unsupported-convention'
        elif not resolved(expected):
            reason = 'unsupported-or-unresolved-profile'
        elif any(h != header and overlaps(expected, p) for h, p in headers):
            reason = 'ambiguous-specification-candidates'
        else:
            plausible = [(h, p) for h, p in right.get((kind, name), []) if overlaps(expected, p)]
            if not plausible:
                reason = 'no-conformant-direct-body'
            elif len(plausible) != 1:
                reason = 'ambiguous-body-candidates'
            else:
                dest, actual = plausible[0]
                compiler = refs.get((*header.defining, kind, name), [])
                # A candidate of a supported lexical form must also be the
                # compiler-observed completion at that exact defining token.
                if dest.form in {'body', 'expression'}:
                    require(compiler == [dest.defining], 'GNAT profile/completion differs: ' + entry['name'])
                if dest.form != 'body':
                    reason = 'implementation-' + dest.form
                elif actual != expected:
                    reason = 'unsupported-or-nonconformant-body-profile'
                else:
                    linked, reason = dest.line, None
        result[entry['anchor']] = linked, reason
    dests = [line for line, _ in result.values() if line is not None]
    require(len(dests) == len(set(dests)), 'multiple declarations share a body destination')
    return result