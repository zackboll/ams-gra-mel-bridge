"""Fail-closed DOC-008 inventory and canonical complete-source HTML rendering."""

import html
import json
import re
from collections import Counter
from html.parser import HTMLParser
from pathlib import Path

from ada_examples import References, annotate

ROOT = Path(__file__).resolve().parents[2]
IDS = ("provider-session", "owned-image", "frame-lease", "common-channel",
       "navigation-report", "track-metadata", "rf-discovery", "rf-rx-job", "rf-product-rx")
CASES = (
    ("ada-explicit", "ada-explicit"), ("full-frame-rich",), ("ada-lease-success",),
    ("keepalive-delayed",), ("navigation-fidelity", "navigation-reject"),
    ("track-metadata-mixed",), ("admin:ok", "admin:reject"), ("c2:job-ok",),
    ("rx:ok:sync", "rx:error-known"),
)


def regular(path):
    if any(p.is_symlink() for p in (path, *path.parents)) or not path.is_file():
        raise ValueError(f"missing/nonregular/symlink example input: {path}")
    return path.read_bytes().decode("utf-8")


def unique(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate example manifest key")
        result[key] = value
    return result


def source(path, root=ROOT):
    if not re.fullmatch(r"src/[a-z][a-z0-9_]*\.(?:adb|ads)", path):
        raise ValueError("invalid canonical example source path")
    return regular(root / "docs/examples" / path)


def inventory(root=ROOT):
    data = json.loads(regular(root / "docs/examples/manifest.json"), object_pairs_hook=unique)
    if set(data) != {"schema_version", "examples"} or data["schema_version"] != 1:
        raise ValueError("example manifest schema")
    entries = data["examples"]
    if tuple(e["id"] for e in entries) != IDS:
        raise ValueError("missing/duplicate/unknown example IDs")
    sources, cases, mains = set(), [], []
    for entry, configurations in zip(entries, CASES):
        if set(entry) != {"id", "title", "purpose", "main", "support", "provider_family",
                          "mock_library", "guide", "packages", "cases", "executable"}:
            raise ValueError("example fields")
        if (entry["provider_family"] not in {"IR", "RF"}
                or entry["mock_library"] != f'libmock_{entry["provider_family"].lower()}_provider.so'
                or entry["guide"] != "guides/examples.html#" + entry["id"]
                or not entry["title"] or not entry["purpose"] or not entry["packages"]
                or not entry["cases"] or len(entry["support"]) != len(set(entry["support"]))):
            raise ValueError("example identity/provider/guide/support")
        if tuple(c["configuration"] for c in entry["cases"]) != configurations:
            raise ValueError("required execution case missing/changed")
        mains.append(Path(entry["main"]).name)
        if entry["executable"] != Path(entry["main"]).stem:
            raise ValueError("executable/main disagreement")
        for name in [entry["main"], *entry["support"]]:
            text = source(name, root)
            # Check tokens after stripping comments/string literals, not teaching prose.
            code = re.sub(r'"(?:[^"]|"")*"|--[^\n]*', '', text)
            if re.search(r"MEL_C_API|\.Testing\b|Interfaces\.C\b|C\.Strings\b|\bSystem\b|"
                         r"Unchecked|\bImport\b|\bAddress\b|dlopen|dlsym", code, re.I):
                raise ValueError("unsafe/private example dependency")
            sources.add(name)
        for case in entry["cases"]:
            if (set(case) != {"id", "configuration", "provider", "exit", "expected"}
                    or not re.fullmatch(r"[a-z][a-z0-9-]*", case["id"])
                    or case["provider"] not in {"normal", "missing"}
                    or case["exit"] not in {0, 1} or not case["expected"]
                    or any(not isinstance(line, str) or not line for line in case["expected"])):
                raise ValueError("invalid execution case")
            missing = case["id"] == "provider-session-missing"
            if case["provider"] != ("missing" if missing else "normal") or case["exit"] != int(missing):
                raise ValueError("changed expected provider/exit")
            cases.append(case["id"])
    if len(cases) != len(set(cases)) or len(mains) != len(set(mains)):
        raise ValueError("duplicate execution/main identity")
    expected_cases = [identity for identity in IDS]
    expected_cases += ["provider-session-missing", "navigation-report-rejected",
                       "rf-discovery-rejected", "rf-product-rx-rejected"]
    if set(cases) != set(expected_cases):
        raise ValueError("execution inventory disagreement")
    readme = regular(root / "docs/examples/README.md")
    documented_cases = re.findall(r"^sh docs/examples/run\.sh ([a-z-]+)$", readme, re.M)
    if Counter(documented_cases) != Counter(cases):
        raise ValueError("manifest/README run case disagreement")
    project = regular(root / "docs/examples/ams_mel_doc_examples.gpr")
    main_list = re.search(r"for Main use\s*\((.*?)\);", project, re.S)
    if main_list is None or Counter(re.findall(r'"([^"]+)"', main_list[1])) != Counter(mains):
        raise ValueError("manifest/GPR main disagreement")
    actual = {p.relative_to(root / "docs/examples").as_posix()
              for p in (root / "docs/examples/src").iterdir()}
    if actual != sources:
        raise ValueError("missing/unlisted example source")
    guide = regular(root / "docs/guides/examples.html")
    if Counter(re.findall(r'\{\{example:([a-z-]+)\}\}', guide)) != Counter(IDS):
        raise ValueError("manifest/guide reference disagreement")
    for entry in entries:
        if guide.count('id="' + entry["id"] + '"') != 1:
            raise ValueError("missing/duplicate guide anchor")
        section = re.search(r'<section id="' + entry["id"] + r'">(.*?)</section>', guide, re.S)
        if section is None or any("{{package:" + name + "}}" not in section[1]
                                  for name in entry["packages"]):
            raise ValueError("missing public API links")
    return entries


def listing(name, root=ROOT, references=None):
    text = source(name, root)
    occurrences = references.occurrences(text, name) if references is not None else ()
    return (f'<p>Complete canonical source: <code>docs/examples/{html.escape(name)}</code></p>'
            f'<pre><code data-example-source="{html.escape(name)}">'
            + annotate(text, occurrences) + '</code></pre>')


def render(text, root=ROOT, references=None):
    entries = {e["id"]: e for e in inventory(root)}
    def replace(match):
        entry = entries[match[1]]
        output = listing(entry["main"], root, references)
        for name in entry["support"]:
            output += '<details><summary>Supporting source: ' + html.escape(name) + '</summary>'
            output += listing(name, root, references) + '</details>'
        output += '<h3>Run cases (from repository root)</h3>'
        for case in entry["cases"]:
            output += '<p>Mock configuration: <code>' + html.escape(case["configuration"]) + '</code></p>'
            output += '<pre><code>sh docs/examples/run.sh ' + case["id"] + '</code></pre>'
            output += '<p>Expected process exit: ' + str(case["exit"]) + '; observable output:</p>'
            output += '<pre>' + html.escape('\n'.join(case["expected"])) + '</pre>'
        return output
    return re.sub(r'\{\{example:([a-z-]+)\}\}', replace, text)


class Listings(HTMLParser):
    def __init__(self, text):
        super().__init__(convert_charrefs=True)
        self.values, self.active, self.stack = [], None, []
        self.feed(text)
        self.close()
        if self.active is not None:
            raise ValueError("unclosed canonical listing")

    def handle_starttag(self, tag, attrs):
        if self.active is not None:
            allowed = (tag == "span" and len(attrs) == 1 and attrs[0][0] == "class"
                       and attrs[0][1] in {"ada-" + kind for kind in
                                          ("keyword", "string", "character", "attribute", "comment", "number", "operator")})
            allowed |= (tag == "a" and len(attrs) == 2 and attrs[0] == ("class", "ada-reference")
                        and attrs[1][0] == "href" and "a" not in self.stack)
            allowed |= (tag == "span" and attrs == [("class", "ada-source-end"), ("aria-hidden", "true")])
            if not allowed:
                raise ValueError("unapproved markup inside canonical listing")
            self.stack.append(tag)
        if tag == "code" and "data-example-source" in dict(attrs):
            if len(attrs) != 1:
                raise ValueError("canonical listing attributes")
            self.active = [dict(attrs)["data-example-source"], ""]

    def handle_data(self, data):
        if self.active is not None:
            self.active[1] += data

    def handle_endtag(self, tag):
        if self.active is not None:
            if self.stack:
                if tag != self.stack.pop():
                    raise ValueError("malformed canonical listing markup")
                return
            if tag != "code":
                raise ValueError("malformed canonical listing")
            self.values.append(tuple(self.active))
            self.active = None

    def handle_comment(self, data):
        if self.active is not None:
            raise ValueError("comment markup inside canonical listing")

    def handle_startendtag(self, tag, attrs):
        if self.active is not None:
            raise ValueError("empty markup inside canonical listing")


def verify_rendered(text, root=ROOT, references=None):
    expected = [(name, source(name, root)) for entry in inventory(root)
                for name in [entry["main"], *entry["support"]]]
    if Listings(text).values != expected:
        raise ValueError("missing/stale/tampered canonical source listing")
    # Exact approved serialization also rejects alternate entities, forged token
    # classes, attributes, links and inconsistent repeated support listings.
    actual = re.findall(r'<code data-example-source="([^"]+)">(.*?)</code>', text, re.S)
    annotated = [(name, annotate(value, references.occurrences(value, name)
                                if references is not None else ())) for name, value in expected]
    if actual != annotated:
        raise ValueError("stale/tampered canonical source listing annotation")
    return len(expected)


def occurrence_ledger(root=ROOT, references=None):
    if references is None:
        raise ValueError("native authority required for example occurrence ledger")
    return [dict(record, listing=index) for index, (name, _) in enumerate(
        (name, entry) for entry in inventory(root) for name in [entry["main"], *entry["support"]])
        for record in references.occurrences(source(name, root), name)]
