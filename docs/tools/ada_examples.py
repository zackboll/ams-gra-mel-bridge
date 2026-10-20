"""Bounded Ada display lexer and source/native-owned example link occurrences.

No overload inference, use-visibility search, suffix matching or output metadata
authorization. Unsupported names are left readable and unlinked.
"""

import hashlib
import html
import re
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

KEYWORDS = frozenset("""abort abs abstract accept access aliased all and array at
begin body case constant declare delay delta digits do else elsif end entry
exception exit for function generic goto if in interface is limited loop mod new
not null of or others out overriding package parallel pragma private procedure
protected raise range record rem renames requeue return reverse select separate
some subtype synchronized tagged task terminate then type until use when while
with xor""".split())
PATTERN = re.compile(
    r'(?P<space>\s+)|(?P<comment>--[^\r\n]*)|'
    r'(?P<string>"(?:""|[^"\r\n])*")|'
    r"(?P<character>'[^\r\n]')|(?P<attribute>'[A-Za-z][A-Za-z0-9_]*)|"
    r'(?P<number>[0-9](?:_?[0-9])*(?:#[0-9A-Fa-f](?:_?[0-9A-Fa-f])*'
    r'(?:\.[0-9A-Fa-f](?:_?[0-9A-Fa-f])*)?#|(?:\.[0-9](?:_?[0-9])*)?)'
    r'(?:[Ee][+-]?[0-9](?:_?[0-9])*)?)|'
    r'(?P<identifier>[^\W\d_](?:\w)*)|'
    r'(?P<operator>:=|=>|\.\.|\*\*|/=|<=|>=|<<|>>|<>|[&()*+,\-./:;<=>\[\]|\'])|'
    r'(?P<plain>.)', re.UNICODE)


@dataclass(frozen=True)
class Token:
    kind: str
    text: str
    start: int
    end: int


def tokenize(text):
    tokens = []
    cursor, previous = 0, None
    while cursor < len(text):
        match = PATTERN.match(text, cursor)
        kind = match.lastgroup
        # A tick after a name/expression is an attribute/qualification tick,
        # not the start of a character literal (e.g. Character'('A')).
        if kind == "character" and previous is not None and (
                previous.kind in {"identifier", "attribute", "number", "string", "character"}
                or previous.text in {")", "]"}):
            tokens.append(Token("operator", "'", cursor, cursor + 1))
            previous = tokens[-1]
            cursor += 1
            continue
        if kind == "identifier" and match[0].lower() in KEYWORDS:
            kind = "keyword"
        tokens.append(Token(kind, match[0], match.start(), match.end()))
        if kind not in {"space", "comment"}:
            previous = tokens[-1]
        cursor = match.end()
    if "".join(t.text for t in tokens) != text:
        raise ValueError("Ada lexer did not preserve source")
    return tokens


class References:
    """Authority from public source declarations, approved ledger and native HTML."""

    def __init__(self, root=ROOT, native=None):
        from local_type_audit import Authority
        from gnatdoc_inventory import specifications
        self.root = root
        native = root / "_site" if native is None else native
        self.native = Authority(root, native)
        self.packages = {name.lower(): "../api/" + page
                         for page, name in self.native.page_packages.items()}
        self.package_sources = {name.lower(): path.relative_to(root).as_posix()
                                for name, path in specifications(root)[0].items()}
        self.declarations = defaultdict(list)
        for entry in self.native.sections.values():
            # Authority indexes only direct public declarations owned by the
            # library unit. Private completions and nested members are absent.
            expected = self.native.ledger[entry["identity"]]["signature"]
            if " ".join(entry["native"].split()) != " ".join(expected.split()):
                raise ValueError("example authority: native declaration profile changed")
            self.declarations[entry["package"] + "." + entry["name"].lower()].append(entry)

    def occurrences(self, text, name):
        tokens = [t for t in tokenize(text) if t.kind not in {"space", "comment"}]
        words = [t.text.lower() for t in tokens]
        imports, packages, aliases = set(), [], {}
        # Only ordinary context clauses before the compilation unit header.
        cursor = 0
        while cursor < len(tokens) and words[cursor] == "with":
            end = cursor + 1
            while end < len(tokens) and words[end] != ";":
                end += 1
            for start, stop, spelling in chains(tokens, cursor + 1, end):
                if start == cursor + 1 and stop == end:
                    imports.add(spelling)
                    if spelling in self.packages:
                        packages.append((start, stop, spelling))
            cursor = end + 1
        # Conservatively require one ordinary unit, no generic/separate scope.
        if words[cursor:cursor + 1] not in (["procedure"], ["package"], ["function"]):
            return []
        # Unit-level aliases only: an initial sequence of renamings/use clauses.
        # Stop at every other declaration, never scan into a nested package.
        header_end = next((i for i in range(cursor, len(words)) if words[i] == "is"), len(words))
        i = header_end + 1
        while i < len(words):
            if words[i] == "use":
                i = next((j + 1 for j in range(i, len(words)) if words[j] == ";"), len(words))
                continue
            if not (words[i] == "package" and i + 3 < len(tokens)
                    and tokens[i + 1].kind == "identifier" and words[i + 2] == "renames"):
                break
            end = next((j for j in range(i + 3, len(words)) if words[j] == ";"), len(words))
            found = list(chains(tokens, i + 3, end))
            if len(found) == 1 and found[0][:2] == (i + 3, end):
                owner = found[0][2]
                alias = words[i + 1]
                if owner in imports and owner in self.packages:
                    aliases.setdefault(alias, []).append((i + 1, end, owner))
            i = end + 1

        def unshadowed(word, definition=None):
            # Every other standalone occurrence may define/shadow this prefix.
            # Includes parameters, comma lists, loops, labels, nested packages,
            # objects and aliases. No partial scope inference is attempted.
            return all(i == definition or (i > 0 and words[i - 1] == ".") or
                       (i + 1 < len(words) and words[i + 1] == "."
                        and (i == 0 or words[i - 1] != "."))
                       for i, w in enumerate(words) if w == word)

        aliases = {alias: entries[0] for alias, entries in aliases.items()
                   if len(entries) == 1 and unshadowed(alias, entries[0][0])}
        direct = unshadowed("ams")
        if not direct:
            aliases = {}
        output = []

        def record(start, stop, identity, context, href, declaration=None, alias=None, qualification=None):
            first, last = tokens[start], tokens[stop - 1]
            if any(t.kind in {"comment", "string", "character"}
                   for t in tokenize(text[first.start:last.end])):
                return
            owner = identity if context == "context-package" else identity.rpartition(".")[0]
            output.append({"source": name, "source_sha256": hashlib.sha256(text.encode("utf-8")).hexdigest(),
                           "start": first.start, "end": last.end,
                           "line": text[:first.start].count("\n") + 1,
                           "visible": text[first.start:last.end], "context": context,
                           "identity": identity, "href": href,
                           "package_source": self.package_sources[owner],
                           "declaration": declaration, "alias": alias,
                           "qualification": qualification})

        if direct:
            for start, stop, owner in packages:
                record(start, stop, owner, "context-package", self.packages[owner])
        for start, stop, spelling in chains(tokens, cursor, len(tokens)):
            parts = spelling.split(".")
            alias = None
            if parts[0] in aliases and len(parts) == 2:
                definition, after, owner = aliases[parts[0]]
                if start <= after:
                    continue
                alias = {"name": parts[0], "start": tokens[definition].start,
                         "end": tokens[after].end, "owner": owner}
            elif parts[0] == "ams" and direct and len(parts) >= 3:
                owner = ".".join(parts[:-1])
                if not any(p == owner or p.startswith(owner + ".") for p in imports):
                    continue
            else:
                continue
            identity = owner + "." + parts[-1]
            candidates = self.declarations.get(identity, [])
            if len(candidates) != 1:
                continue  # No guessed overloads, even if one looks plausible.
            entry = candidates[0]
            kind = entry["identity"].split(": ")[2]
            before = start - 1
            while before >= 0 and words[before] in {"constant", "aliased", "in", "out", "not", "null", "access"}:
                before -= 1
            type_context = (before >= 0 and words[before] in {":", "return"}
                            or words[max(0, start - 2):start] == ["use", "type"])
            if kind in {"type", "subtype"} and type_context:
                context = "type-mark"
            elif kind in {"function", "procedure"} and stop < len(words) and words[stop] == "(":
                context = "unique-operation"
            else:
                continue
            record(stop - 1, stop, identity, context,
                   "../api/" + entry["page"] + "#" + entry["anchor"], entry, alias,
                   {"start": tokens[start].start, "end": tokens[stop - 1].end,
                    "visible": text[tokens[start].start:tokens[stop - 1].end]})
        return sorted(output, key=lambda r: r["start"])


def chains(tokens, start, stop):
    """Maximal selected names; never resume midway through a selected chain."""
    i = start
    while i < stop:
        if tokens[i].kind != "identifier" or (i and tokens[i - 1].text == "."):
            i += 1
            continue
        end = i + 1
        while end + 1 < stop and tokens[end].text == "." and tokens[end + 1].kind == "identifier":
            end += 2
        yield i, end, "".join(t.text.lower() for t in tokens[i:end])
        i = end


def annotate(text, occurrences=()):
    """One canonical, balanced serialization; no generated metadata is trusted."""
    starts = {r["start"]: r for r in occurrences}
    ends = {r["end"]: r for r in occurrences}
    if len(starts) != len(occurrences) or len(ends) != len(occurrences):
        raise ValueError("duplicate example link span")
    output = []
    for token in tokenize(text):
        if token.start in starts:
            output.append('<a class="ada-reference" href="' + html.escape(starts[token.start]["href"], quote=True) + '">')
        escaped = html.escape(token.text)
        if token.kind not in {"space", "identifier", "plain"}:
            escaped = '<span class="ada-' + token.kind + '">' + escaped + '</span>'
        output.append(escaped)
        if token.end in ends:
            output.append('</a>')
    # Chromium otherwise drops the final source newline from Selection.toString.
    # This empty inline box contributes no text (including to accessible text).
    return "".join(output) + '<span class="ada-source-end" aria-hidden="true"></span>'
