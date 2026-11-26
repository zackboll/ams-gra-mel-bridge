"""MEL's qualification-only Ada ownership and GNAT cross-reference evidence."""

from dataclasses import dataclass
import re
import subprocess
import tomllib

from ada_examples import tokenize
import build_enhanced_site as publication
import source_view_audit as specification_view

require = publication.require
IDENTIFIER = r'[A-Za-z][A-Za-z0-9]*(?:_[A-Za-z0-9]+)*'
NAME = IDENTIFIER + r'(?:\.' + IDENTIFIER + r')*'
CONTEXT = re.compile(r'(?:(limited |private |limited private )?with) (' + NAME + r'(?: , ' + NAME + r')*)')


@dataclass(frozen=True)
class Source:
    path: str
    name: str
    opening: int
    closing: int
    raw: bytes
    private: bool
    context: frozenset

    @property
    def page(self):
        return 'api/source/' + self.path + '.html'

    def summary(self):
        return dict(package=self.name, sha256=publication.sha(self.raw), bytes=len(self.raw),
                    lines=self.raw.count(b'\n') + bool(self.raw and not self.raw.endswith(b'\n')))


def words(raw):
    """Significant Ada tokens with physical line and column, not display HTML."""
    line, column = 1, 1
    for token in tokenize(raw.decode('utf-8')):
        if token.kind not in {'space', 'comment'}:
            yield token, line, column
        breaks = token.text.count('\n')
        line += breaks
        column = len(token.text.rsplit('\n', 1)[-1]) + 1 if breaks else column + len(token.text)


def unit(path, raw):
    require(b'\x00' not in raw and b'\r' not in raw.replace(b'\r\n', b''), 'unsupported Ada source bytes')
    tokens = list(words(raw))
    values = [t.text for t, _, _ in tokens]
    folded = [v.lower() for v in values]
    at, context = 0, set()
    while at < len(tokens) and folded[at] in {'with', 'limited', 'private', 'use', 'pragma'}:
        if folded[at:at+2] == ['private', 'package']:
            break
        end = folded.index(';', at)
        clause = ' '.join(folded[at:end]).replace(' . ', '.')
        match = CONTEXT.fullmatch(clause)
        require(match or folded[at] in {'use', 'pragma'}, 'unsupported context clause: ' + path)
        if match and not (match[1] or '').startswith('limited'):
            context.update(match[2].split(' , '))
        at = end + 1
    body = path.endswith('.adb')
    prefix = ['package', 'body'] if body else ['package']
    private = folded[at:at+1] == ['private']
    if private:
        at += 1
    require(not (private and body) and folded[at:at+len(prefix)] == prefix,
            'unsupported library unit header: ' + path)
    opening = tokens[at][1]
    first = at + len(prefix)
    end = first + 1
    while folded[end:end+1] == ['.'] and end + 1 < len(tokens):
        end += 2
    spelling = ''.join(values[first:end])
    require(re.fullmatch(NAME, spelling) and folded[end:end+1] == ['is'] and
            folded[end+1:end+2] not in (['new'], ['separate']), 'unsupported package form: ' + path)
    tail = ['end', *folded[first:end], ';']
    boundaries = [i for i in range(end + 1, len(tokens)) if folded[i:i+len(tail)] == tail]
    require(boundaries == [len(tokens) - len(tail)], 'unsupported single-library-unit boundary: ' + path)
    return Source(path, spelling, opening, tokens[boundaries[0]][1], raw, private, frozenset(context))


def inventory(root):
    config = tomllib.loads((root / specification_view.CONFIG).read_text())
    require(config['sources']['directories'] == ['ada/src'], 'unexpected MEL source ownership domain')
    directory = root / 'ada/src'
    require(not any(p.is_symlink() for p in (directory, *directory.parents)), 'symlink source domain')
    specs, bodies, seen = {}, {}, set()
    for path in sorted(directory.rglob('*')):
        require(not path.is_symlink(), 'symlink source member')
        if path.suffix.lower() not in {'.ads', '.adb'}:
            continue
        name = path.relative_to(root).as_posix()
        require(path.parent == directory and path.is_file() and
                re.fullmatch(r'ada/src/[A-Za-z0-9_][A-Za-z0-9_-]*\.ad[bs]', name),
                'unsafe or cross-directory Ada source path: ' + name)
        require(name.casefold() not in seen, 'case-colliding Ada source path: ' + name)
        seen.add(name.casefold())
        source = unit(name, path.read_bytes())
        owners = bodies if path.suffix == '.adb' else specs
        key = source.name.casefold()
        require(key not in owners, 'duplicate/case-colliding package owner: ' + name)
        owners[key] = source
    for key, body in bodies.items():
        require(key in specs and specs[key].name == body.name, 'body/specification ownership or spelling differs: ' + body.path)
    public, private = specification_view.specifications(root)
    require({s.name: root / s.path for s in specs.values() if not s.private} == public and
            {s.name: root / s.path for s in specs.values() if s.private} == private,
            'independent specification visibility differs')
    return specs, bodies


def compile_units(root, directory, specs, bodies):
    """Check all units, including bodyless and excluded ones, without native linking."""
    directory.mkdir()
    paths = [root / source.path for key, source in sorted({**specs, **bodies}.items())]
    process = subprocess.run(['gnatmake', '-q', '-gnat2022', '-gnatc', '-c', '-I' + str(root / 'ada/src'),
                              *map(str, paths)], cwd=directory, capture_output=True)
    # Compiler diagnostics may include source excerpts; never print or retain them in hosted artifacts.
    require(process.returncode == 0, 'Ada compile-only single-unit verification failed')
    result = {}
    for key, spec in specs.items():
        body = bodies.get(key)
        compiled = body or spec
        file = directory / (compiled.path.rsplit('/', 1)[-1][:-4] + '.ali')
        require(file.is_file(), 'missing GNAT cross-reference file: ' + compiled.path)
        text = file.read_text()
        for source in (spec, body) if body else (spec,):
            marker = '%b' if source is body else '%s'
            require(re.search(r'^U ' + re.escape(key + marker) + r'\s+' +
                              re.escape(source.path.rsplit('/', 1)[-1]) + r'\s+\S+.*\bPK\b', text, re.M),
                    'GNAT compilation-unit identity differs: ' + source.path)
        result[key] = text
    return result


def xrefs(text, spec, body):
    """Read GNAT 16 ALI D-table file indices and U/V entity body references.

    X records may omit a body with no referenced entities of its own. The
    dependency table (not the X table) provides the complete file numbering.
    """
    names = [m[1] for m in re.finditer(r'^D (\S+)\s', text, re.M)]
    spec_name = spec.path.rsplit('/', 1)[-1]
    body_name = body.path.rsplit('/', 1)[-1]
    require(names.count(spec_name) == names.count(body_name) == 1, 'ambiguous ALI source dependency')
    spec_index, body_index = names.index(spec_name) + 1, names.index(body_name) + 1
    sections = re.split(r'^X (\d+) (\S+)[^\n]*\n', text, flags=re.M)
    matches = [sections[i+2] for i in range(1, len(sections), 3)
               if int(sections[i]) == spec_index and sections[i+1] == spec_name]
    require(len(matches) == 1, 'missing/duplicate ALI specification cross-references')
    result = {}
    for entity in re.finditer(r'^(\d+)([UV])(\d+)\*?([^\s{<\[]+)([^\n]*(?:\n\. [^\n]*)*)', matches[0], re.M):
        line, kind, column, name, references = entity.groups()
        key = (int(line), int(column), 'procedure' if kind == 'U' else 'function', name.lower())
        active, locations = spec_index, []
        for ref in re.finditer(r'\s(?:(\d+)\|)?(\d+)([a-z<>=^])(\d+)(?=\s|$)', references):
            if ref[1]:
                active = int(ref[1])
            if active == body_index and ref[3] == 'b':
                locations.append((int(ref[2]), int(ref[4])))
        require(key not in result, 'duplicate GNAT callable identity')
        result[key] = locations
    return result