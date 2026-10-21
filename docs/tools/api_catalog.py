"""DOC-012 static catalog, authorized only by public source and native sections.

The occurrence ledger supplies rendering input. Validation separately walks each
manifest-owned main/support source, re-resolves occurrences, and compares parsed
entries, not generated counts or presentation metadata. No Ada overload inference.
"""

import hashlib
import html
import json
import re
from html.parser import HTMLParser

import example_sources as examples
import symbol_audit

EXAMPLES_SHA256 = "a2b9bac08b9ffc508fde4d9040f54d5b963a89bb9d98b4c8e19c50e7aea08af2"
SCOPES = ("stable", "additional", "operation")
GROUPS = ("Shared/Core", "IR", "RF")
TITLES = ("Public types and subtypes", "Additional types used by examples",
          "Operations used in examples")
IDS = ("public-symbol-index", "additional-example-types", "example-usage-index")


def require(ok, message):
    if not ok:
        raise ValueError("independent API catalog: " + message)


def family(owner):
    for name in ("IR", "RF"):
        if owner == "AMS.MEL." + name or owner.startswith("AMS.MEL." + name + "."):
            return name
    return "Shared/Core"


def example_identity(root, entries):
    mapping = {name: hashlib.sha256(examples.source(name, root).encode()).hexdigest()
               for entry in entries for name in [entry["main"], *entry["support"]]}
    mapping["manifest.json"] = hashlib.sha256(
        (root / "docs/examples/manifest.json").read_bytes()).hexdigest()
    digest = hashlib.sha256(json.dumps(mapping, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
    require(digest == EXAMPLES_SHA256, "canonical example/support/manifest identity changed")
    return mapping


def declaration(entry, native):
    owner = native.packages[entry["package"]]["name"]
    canonical = owner + "." + entry["name"]
    ledger = native.ledger.get(entry["identity"], {})
    kind = entry["identity"].split(": ")[2]
    require(kind in {"type", "subtype", "function", "procedure"}, "unsupported declaration kind")
    require(entry["identity"].split(": ")[:2] == [owner, canonical], "forged source identity")
    require(native.page_packages.get(entry["page"]) == owner, "wrong qualified native package")
    require((entry["page"], entry["anchor"]) == (ledger.get("page"), ledger.get("anchor")),
            "destination differs from approved ledger")
    require(re.fullmatch(r"[a-f0-9]{64}\.html", entry["page"]) is not None and
            re.fullmatch(r"[a-f0-9]{64}", entry["anchor"]) is not None, "unsafe native destination")
    require(native.sections.get((entry["page"], entry["anchor"])) == entry,
            "declaration is not a source-owned public native section")
    return dict(canonical=canonical, owner=owner, kind=kind, source=entry["source"],
                line=entry["line"], ledger_identity=entry["identity"],
                page=entry["page"], anchor=entry["anchor"])


def inventory(root, references, independent=False):
    native = references.native
    stable = symbol_audit.inventory(native, references.native_site)
    entries = examples.inventory(root)
    source_hashes = example_identity(root, entries)
    uses, proofs, declarations = {}, [], {}
    if independent:
        # Deliberately do not use the listing number or ledger's containing claim.
        occurrences = [(entry, record) for entry in entries
                       for name in [entry["main"], *entry["support"]]
                       for record in references.occurrences(examples.source(name, root), name)]
    else:
        listings = [entry for entry in entries for _ in [entry["main"], *entry["support"]]]
        occurrences = [(listings[record["listing"]], record)
                       for record in examples.occurrence_ledger(root, references)]
    for example, record in occurrences:
        if record["context"] not in {"type-mark", "unique-operation"}:
            continue
        entry = record["declaration"]
        identity = entry["identity"]
        row = declaration(entry, native)
        require(record["identity"] == row["canonical"].lower(), "mismatched occurrence identity")
        require(record["href"] == "../api/" + row["page"] + "#" + row["anchor"],
                "mismatched occurrence destination")
        require((record["context"] == "type-mark") == (row["kind"] in {"type", "subtype"}),
                "mismatched occurrence classification")
        if record["context"] == "unique-operation":
            require(len(references.declarations[record["identity"]]) == 1, "ambiguous operation")
        require(record["source"] in [example["main"], *example["support"]], "unrelated containing example")
        require(record["source_sha256"] == source_hashes[record["source"]], "changed occurrence source")
        require(identity not in declarations or declarations[identity] == row, "duplicate mismatched destination")
        declarations[identity] = row
        uses.setdefault(identity, set()).add(example["id"])
        proofs.append(dict({k: v for k, v in record.items() if k != "listing"}, example=example["id"]))
    stable_ids = {row["ledger_identity"] for row in stable}
    rows = []
    for symbol in stable:
        row = {k: symbol[k] for k in ("canonical", "owner", "kind", "source", "line",
                                      "ledger_identity", "page", "anchor")}
        rows.append(dict(row, scope="stable"))
    rows.extend(dict(row, scope="additional" if row["kind"] in {"type", "subtype"} else "operation")
                for identity, row in declarations.items() if identity not in stable_ids)
    for row in rows:
        row["examples"] = [dict(id=e["id"], title=e["title"], href=e["guide"].removeprefix("guides/"))
                           for e in entries if e["id"] in uses.get(row["ledger_identity"], set())]
    rows.sort(key=lambda r: (SCOPES.index(r["scope"]), GROUPS.index(family(r["owner"])),
                             r["owner"].lower(), r["line"], r["canonical"].lower()))
    require(len({r["canonical"].lower() for r in rows}) == len(rows), "duplicate catalog identity")
    require(len({(r["page"], r["anchor"]) for r in rows}) == len(rows), "duplicate catalog destination")
    return dict(entries=rows, occurrences=proofs, example_source_sha256=source_hashes)


def render(catalog, scopes):
    output = []
    for scope in scopes:
        i = SCOPES.index(scope)
        output.append('<section id="' + IDS[i] + '"><h2>' + TITLES[i] + '</h2>')
        for group in GROUPS:
            rows = [r for r in catalog["entries"] if r["scope"] == scope and family(r["owner"]) == group]
            if not rows:
                continue
            output.append('<h3>' + group + '</h3>')
            for owner in sorted({r["owner"] for r in rows}, key=str.lower):
                output.append('<h4><code>' + html.escape(owner) + '</code></h4><ul>')
                for row in rows:
                    if row["owner"] != owner:
                        continue
                    output.append('<li data-catalog-entry="' + html.escape(row["canonical"], quote=True) +
                                  '" data-kind="' + row["kind"] + '" data-scope="' + scope + '">')
                    output.append('<a href="../api/' + row["page"] + '#' + row["anchor"] + '"><code>' +
                                  html.escape(row["canonical"]) + '</code></a> — ' + row["kind"].capitalize() + '.')
                    if row["examples"]:
                        output.append(' Used in: ' + ', '.join(
                            '<a href="' + e["href"] + '">' + html.escape(e["title"]) + '</a>'
                            for e in row["examples"]) + '.')
                    else:
                        output.append(' No verified example use.')
                    output.append('</li>')
                output.append('</ul>')
        output.append('</section>')
    return ''.join(output)


class CatalogPage(HTMLParser):
    """Recover actual entries/associations without trusting counts or manifests."""

    def __init__(self, text):
        super().__init__(convert_charrefs=True)
        self.rows, self.active, self.link = [], None, None
        self.feed(text)
        self.close()
        require(self.active is None and self.link is None, "unclosed catalog entry")

    def handle_starttag(self, tag, attrs):
        require(len(attrs) == len(dict(attrs)), "duplicate attribute")
        attrs = dict(attrs)
        if "data-catalog-entry" in attrs:
            require(self.active is None and tag == "li" and set(attrs) ==
                    {"data-catalog-entry", "data-kind", "data-scope"}, "unauthorized entry markup")
            self.active = dict(canonical=attrs["data-catalog-entry"], kind=attrs["data-kind"],
                               scope=attrs["data-scope"], links=[])
        elif self.active is not None:
            require(tag in {"a", "code"}, "unauthorized catalog markup")
            require(attrs == {} if tag == "code" else set(attrs) == {"href"}, "unauthorized catalog attributes")
        if self.active is not None and tag == "a":
            require(self.link is None, "nested catalog link")
            self.link = [attrs["href"], ""]

    def handle_data(self, data):
        if self.link is not None:
            self.link[1] += data

    def handle_endtag(self, tag):
        if self.active is not None and tag == "a":
            require(self.link is not None, "malformed catalog link")
            self.active["links"].append(tuple(self.link))
            self.link = None
        if self.active is not None and tag == "li":
            require(self.link is None, "unclosed catalog link")
            self.rows.append(self.active)
            self.active = None


def verify(text, root, references):
    expected = inventory(root, references, independent=True)
    rows = []
    for row in expected["entries"]:
        rows.append(dict(canonical=row["canonical"], kind=row["kind"], scope=row["scope"],
                         links=[("../api/" + row["page"] + "#" + row["anchor"], row["canonical"])] +
                         [(e["href"], e["title"]) for e in row["examples"]]))
    require(CatalogPage(text).rows == rows, "missing/duplicate/reordered/forged entry or example association")
    # Exact serialization also rejects entity aliases, hidden/active content,
    # altered classification labels, unapproved headings and injected markup.
    for scopes in (("stable",), ("additional", "operation")):
        block = render(expected, scopes)
        require(text.count(block) == 1, "altered or noncanonical catalog markup")
    return expected


def link_count(catalog):
    return sum(1 + len(row["examples"]) for row in catalog["entries"])
