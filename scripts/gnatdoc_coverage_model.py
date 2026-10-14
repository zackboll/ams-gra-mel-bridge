"""Coverage parsing adapted from opencv_geometry_ada (Apache-2.0).
Immutable reference: 9142e9c3134ded907204ae0304acc28ab602b7a9.
MEL modifications use shared source indexing and retain native anchors.
"""
import collections
import html
import re
from compare_gnatdoc_warnings import code_tokens, declaration_index, WarningComparisonError, _normalize
PACKAGE_RE = re.compile(r"^\s*package\s+([\w.]+)\s+is\b", re.I)
SECTION_RE = re.compile(r"<h4 id=([^ >]+)>([^<]+)</h4><pre class=ada-code-snippet><code>(.*?)</code></pre>(.*?)(?=<h4 |\Z)", re.S)
H5_RE = re.compile(r"<h5>([^<]+)</h5><dl>(.*?)</dl>", re.S)
ENTRY_RE = re.compile(r"<dt>(.*?)<dd>(.*?)(?=<dt>|\Z)", re.S)
class CoverageError(ValueError):
    pass

def _text(fragment):
    fragment = re.sub(r"</?(?:code|em|strong|a|span)\b[^>]*>", "", fragment)
    return " ".join(html.unescape(re.sub(r"<[^>]+>", " ", fragment)).split())

def _kind(signature):
    word = signature.split()[0]
    return word if word in ("function", "procedure", "type", "subtype", "package") else "object"

def _profile(text):
    return _normalize(code_tokens(text)).rstrip(";").strip()

def without_aspects(signature):
    depth = 0
    for match in re.finditer(r"[()]|\bwith\b|\brenames\b", signature):
        if match[0] == "(":
            depth += 1
        elif match[0] == ")":
            depth -= 1
        elif depth == 0:
            return signature[:match.start()].rstrip() + ";"
    return signature

def parameter_names(signature):
    """Parameter names of a normalized subprogram profile, in order."""
    start = signature.find("(")
    head = signature[:start] if start >= 0 else signature
    if start < 0 or " return " in head:
        return []
    depth, part, parts = 0, [], []
    for character in signature[start + 1:]:
        if character == "(":
            depth += 1
        elif character == ")":
            if depth == 0:
                break
            depth -= 1
        elif character == ";" and depth == 0:
            parts.append("".join(part))
            part = []
            continue
        part.append(character)
    parts.append("".join(part))
    names = []
    for item in parts:
        if " : " not in item:
            raise CoverageError(f"unsupported parameter specification: {item}")
        names.extend(n.strip() for n in item.split(" : ", 1)[0].split(","))
    return names


def discriminant_names(signature):
    """Discriminants of `type T (A : X; B : Y) is ...`, in order."""
    match = re.match(r"type \S+ \((.*?)\) is\b", signature)
    if match is None:
        return []
    return [name.strip()
            for item in match[1].split(";")
            for name in item.split(" : ", 1)[0].split(",")]


def specification(path):
    """Return (package, lines, header line, private line, declarations, members)."""
    lines = path.read_text(encoding="utf-8").splitlines()
    code = [code_tokens(line) for line in lines]
    header = next((number for number, text in enumerate(code, start=1)
                   if PACKAGE_RE.match(text)), None)
    if header is None:
        raise CoverageError(f"no package declaration in {path.name}")
    package = PACKAGE_RE.match(code[header - 1])[1]
    private = next((number for number, text in enumerate(code, start=1)
                    if text.strip().lower() == "private"), len(lines) + 1)
    try:
        index = declaration_index(lines)
    except WarningComparisonError as error:
        raise CoverageError(f"{path.name}: {error}") from error
    visible = [d for ds in index.values() for d in ds if d["start"] < private]
    top = sorted((d for d in visible
                  if d["scope"] == f"{package}.{d['name']}"),
                 key=lambda d: d["start"])
    members = collections.defaultdict(list)
    for declaration in sorted(visible, key=lambda d: (d["start"], d["column"])):
        owner = declaration["scope"].rpartition(".")[0]
        if owner != package and owner.startswith(package + "."):
            members[owner].append(declaration)
    return package, lines, header, private, top, members


def expected_items(declaration, members):
    """Return (heading, expected member names) for one declaration."""
    kind = _kind(declaration["signature"])
    scope = declaration["scope"]
    if kind in ("function", "procedure"):
        items = [("Parameters", parameter_names(declaration["signature"]))]
        if kind == "function":
            items.append(("Return Value", [None]))
        return [(heading, names) for heading, names in items if names]
    if kind == "type":
        names = [m["name"] for m in members.get(scope, [])]
        if " is (" in declaration["signature"] or (
                declaration["signature"].endswith(" is") and names):
            if "record" not in declaration["signature"]:
                return [("Enumeration Literal", names)] if names else []
        discriminants = discriminant_names(declaration["signature"])
        fields = discriminants + [n for n in names if n not in discriminants]
        return [("Record fields", fields)] if fields else []
    return []


def rendered_sections(page_html):
    """Map (name, kind, profile) -> list of (description, {heading: entries})."""
    sections = collections.defaultdict(list)
    for match in SECTION_RE.finditer(page_html):
        name = html.unescape(match[2])
        snippet = " ".join(html.unescape(re.sub(r"<[^>]+>", "", match[3])).split())
        if re.match(rf"{re.escape(name)}\s*:", snippet):
            kind = "object"
        else:
            kind = _kind(snippet)
        profile = _profile(snippet) if kind in ("function", "procedure") else None
        body = match[4]
        first = body.find("<h5>")
        description = _text(body if first < 0 else body[:first])
        groups = {}
        for heading, entries in H5_RE.findall(body):
            if heading == "Return Value":
                groups[heading] = [(None, _text(re.sub(r"^\s*<dd>", "", entries)))]
            else:
                groups[heading] = [(_text(dt), _text(dd))
                                   for dt, dd in ENTRY_RE.findall(entries)]
        sections[(name, kind, profile)].append(
            {"description": description, "groups": groups, "text": _text(body),
             "anchor": match[1], "signature": snippet})
    return sections
