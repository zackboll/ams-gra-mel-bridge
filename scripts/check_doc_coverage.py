#!/usr/bin/env python3
"""Fail-closed source/native inventory and rendered documentation gap ratchet.

Descriptions count only when rendered and backed by an attached source block.
Misattached pre-existing prose is recorded as a gap, not accepted as coverage.
No HTML is modified. Renderer omissions are separate source-visible elements.
"""

import argparse
import collections
import html
import json
import re
from pathlib import Path

from compare_gnatdoc_warnings import _normalize, code_tokens
from gnatdoc_inventory import ROOT, specifications
from check_doc_source_integrity import native_profile, tokens
from gnatdoc_coverage_model import (
    CoverageError, SECTION_RE, _kind, _profile, _text, discriminant_names,
    expected_items, rendered_sections, specification, without_aspects,
)

RENDERER = "GNATdoc 26.0.0 (20260415)"


def declaration_identity(package, lines, declaration):
    kind = _kind(declaration["signature"])
    # The inherited warning index masks literals for location scanning. Coverage
    # identities instead retain every literal/default/aspect in full profiles.
    signature = declaration["signature"]
    if kind in ("function", "procedure"):
        signature = " ".join(tokens("\n".join(
            lines[declaration["start"] - 1:declaration["end"]])))
    return f"{package}: {declaration['scope']}: {kind}: {signature}"


def verified_exceptions(root, declarations):
    data = json.loads((root / "docs/gnatdoc-renderer-exceptions.json").read_text())
    if data.get("renderer") != RENDERER:
        raise CoverageError("unqualified renderer exception version")
    expected = {
        "package": "AMS.MEL.IR.Health_Status.Metadata",
        "identity": "AMS.MEL.IR.Health_Status.Metadata.Status",
        "declaration": "package Status renames AMS.MEL.Status;",
        "normalized_signature": "package AMS.MEL.IR.Health_Status.Metadata.Status renames AMS.MEL.Status;",
        "target": "AMS.MEL.Status",
        "source": "ada/src/ams-mel-ir-health_status-metadata.ads",
    }
    exceptions = data.get("exceptions", [])
    if len(exceptions) != 1 or any(exceptions[0].get(k) != v for k, v in expected.items()):
        raise CoverageError("renderer exception must exactly match the verified MEL alias")
    candidates = [d for package, path, d in declarations
                  if package == expected["package"] and d["scope"] == expected["identity"]
                  and d["signature"] == expected["normalized_signature"]
                  and path.relative_to(root).as_posix() == expected["source"]]
    if len(candidates) != 1:
        raise CoverageError("renderer exception does not match exactly one real source declaration")
    return {expected["identity"]: exceptions[0]}


def meaningful(text):
    return len(re.findall(r"[A-Za-z0-9_]+", text)) >= 2


def attached_block(lines, declaration):
    """Only immediately trailing blocks qualify in this pinned trailing run.

    A package-opening comment is handled separately. Comments embedded in a
    declaration or separated by blank/code lines are never used as evidence.
    """
    cursor = declaration["end"]
    block = []
    while cursor < len(lines) and lines[cursor].lstrip().startswith("--"):
        block.append(lines[cursor].split("--", 1)[1].strip())
        cursor += 1
    return block


def source_prose(block, tag=None, name=None):
    sections = {None: []}
    current = None
    for line in block:
        match = re.match(r"@(param|return|field|enum|exception|formal)\b\s*(.*)", line)
        if match:
            category, content = match.groups()
            if category == "return":
                current = (category, None)
            else:
                parts = content.split(maxsplit=1)
                current = (category, parts[0]) if parts else (category, "")
                content = parts[1] if len(parts) == 2 else ""
            sections.setdefault(current, []).append(content)
        else:
            sections.setdefault(current, []).append(line)
    text = " ".join(sections.get(None if tag is None else (tag, name), []))
    text = re.sub(r"[`*_]", "", text)
    return " ".join(text.split())


def evidence_matches(rendered, source):
    # Count no accidental native text, including another declaration's comment.
    return meaningful(rendered) and meaningful(source) and source in rendered


def measure(site, root=ROOT, anchor_manifest=None):
    public, private = specifications(root)
    pages = {}
    for path in sorted((site / "api").glob("*.html")):
        if path.name == "index.html":
            continue
        text = path.read_text()
        headings = re.findall(r"<h1>([^<]+)</h1>", text)
        if len(headings) != 1 or headings[0] in pages:
            raise CoverageError(f"duplicate or unknown native package page: {path.name}")
        pages[headings[0]] = (path, text)
    if set(pages) != set(public):
        raise CoverageError(f"public page inventory mismatch: {set(pages) ^ set(public)}")
    models = {name: specification(path) for name, path in public.items()}
    declarations = [(name, public[name], d) for name, model in models.items() for d in model[4]]
    exceptions = verified_exceptions(root, declarations)
    elements, native, attachment_gaps = [], {}, []
    abbreviated_signatures = []

    def record(identity, kind, documented, package):
        elements.append({"identity": identity, "kind": kind, "documented": bool(documented),
                         "package": package})

    for package, model in models.items():
        _, lines, header, boundary, top, members = model
        path, text = pages[package]
        sections = rendered_sections(text)
        if sum(map(len, sections.values())) != len(SECTION_RE.findall(text)):
            raise CoverageError(f"unmapped native sections: {package}")
        used = set()
        description = text.split("<h2>Description</h2>")
        if len(description) != 2:
            raise CoverageError(f"missing package Description: {package}")
        package_text = _text(description[1].split("<h4 ", 1)[0])
        opening = []
        cursor = header
        while cursor < len(lines) and lines[cursor].lstrip().startswith("--"):
            opening.append(lines[cursor].split("--", 1)[1].strip())
            cursor += 1
        # A comment immediately before the first declaration is not evidence
        # for the package, even when the renderer wrongly places it there.
        if cursor >= len(lines) or lines[cursor].strip():
            opening = []
        record(f"{package}: package: description", "package", evidence_matches(
            package_text, source_prose(opening)), package)
        for d in top:
            kind = _kind(d["signature"])
            profile = _profile(without_aspects(d["signature"])) if kind in ("procedure", "function") else None
            key = (d["name"], kind, profile)
            identity = declaration_identity(package, lines, d)
            if d["scope"] in exceptions:
                if key in sections:
                    raise CoverageError("known renderer omission unexpectedly has a native section")
                record(identity + ": description", "renderer-exception", False, package)
                continue
            found = sections.get(key, [])
            if len(found) != 1 or key in used:
                raise CoverageError(f"missing/duplicate native declaration: {identity}")
            used.add(key)
            section = found[0]
            if kind in ("function", "procedure"):
                source_profile = "\n".join(lines[d["start"] - 1:d["end"]])
                if native_profile(source_profile) != native_profile(section["signature"]):
                    raise CoverageError(f"source/native literal or full profile mismatch: {identity}")
            elif kind != "package":
                raw = "\n".join(lines[d["start"] - 1:d["end"]])
                source_tokens, rendered_tokens = tokens(raw), tokens(section["signature"])
                while source_tokens and source_tokens[-1] == ";":
                    source_tokens.pop()
                while rendered_tokens and rendered_tokens[-1] == ";":
                    rendered_tokens.pop()
                if "…" in rendered_tokens:
                    position = rendered_tokens.index("…")
                    prefix, suffix = rendered_tokens[:position], rendered_tokens[position + 1:]
                    if source_tokens[:len(prefix)] != prefix or source_tokens[-len(suffix):] != suffix:
                        raise CoverageError(f"source/native abbreviated signature mismatch: {identity}")
                    abbreviated_signatures.append(identity)
                elif source_tokens != rendered_tokens:
                    raise CoverageError(f"source/native declaration tokens differ: {identity}")
            if not re.fullmatch(r"[0-9a-f]{64}", section["anchor"]):
                raise CoverageError(f"invalid native anchor: {identity}")
            native[identity] = {"page": path.name, "anchor": section["anchor"],
                                "signature": section["signature"]}
            block = attached_block(lines, d)
            source = source_prose(block)
            documented = evidence_matches(section["description"], source)
            if meaningful(section["description"]) and not documented:
                attachment_gaps.append(identity)
            item_kind = "exception" if kind == "object" and " : exception" in d["signature"] else kind
            record(identity + ": description", item_kind, documented, package)
            expected = expected_items(d, members)
            wanted = {heading for heading, _ in expected}
            if set(section["groups"]) - wanted - {"Raised exceptions"}:
                raise CoverageError(f"unexpected rendered member category: {identity}")
            for heading, names in expected:
                entries = section["groups"].get(heading)
                if entries is None or [name for name, _ in entries] != names:
                    raise CoverageError(f"source/native member mismatch: {identity}: {heading}: "
                                        f"expected {names}, got {entries}")
                tag, category = {"Parameters": ("param", "parameter"),
                                 "Return Value": ("return", "return"),
                                 "Enumeration Literal": ("enum", "literal"),
                                 "Record fields": ("field", "component")}[heading]
                discriminants = discriminant_names(d["signature"])
                for position, (name, prose) in enumerate(entries):
                    member_kind = "discriminant" if name in discriminants else category
                    record(f"{identity}: {member_kind} {position}:{name or ''}", member_kind,
                           evidence_matches(prose, source_prose(block, tag, name)), package)
        if set(sections) != used:
            raise CoverageError(f"unexpected/private native declarations: {package}: {set(sections) - used}")
    identities = [element["identity"] for element in elements]
    if len(set(identities)) != len(identities):
        raise CoverageError("duplicate source element identities")
    if anchor_manifest is not None and native != anchor_manifest:
        raise CoverageError("native anchor/signature manifest differs; independently review source-backed changes")
    totals = collections.Counter(element["kind"] for element in elements)
    documented = collections.Counter(element["kind"] for element in elements if element["documented"])
    package_totals = collections.Counter(element["package"] for element in elements)
    package_documented = collections.Counter(element["package"] for element in elements if element["documented"])
    return {"renderer": RENDERER, "source_declarations": len(declarations),
            "native_sections": len(native), "renderer_exceptions": list(exceptions.values()),
            "total_elements": len(elements), "documented_elements": sum(documented.values()),
            "undocumented_elements": len(elements) - sum(documented.values()),
            "documented_percentage": round(100 * sum(documented.values()) / len(elements), 4),
            "kinds": {kind: {"total": count, "documented": documented[kind]}
                      for kind, count in sorted(totals.items())},
            "packages": {name: {"total": count, "documented": package_documented[name]}
                         for name, count in sorted(package_totals.items())},
            "attachment_gaps": attachment_gaps,
            "native_abbreviated_signatures": abbreviated_signatures,
            "elements": elements, "native": native}


def ratchet(report, baseline):
    current = {e["identity"] for e in report["elements"] if not e["documented"]}
    lines = baseline.splitlines()
    if len(lines) != len(set(lines)) or any(not line or ": " not in line for line in lines):
        raise CoverageError("malformed or duplicate coverage baseline")
    added, removed = current - set(lines), set(lines) - current
    if added or removed:
        raise CoverageError(f"coverage ratchet: {len(added)} new gaps; {len(removed)} fixed/stale gaps; "
                            "baseline updates require explicit reviewed edits")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("site", type=Path)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--capture-pristine", action="store_true",
                        help="Create absent initial baselines only; never overwrite")
    args = parser.parse_args()
    manifest = args.root / "docs/gnatdoc-native-anchors.json"
    baseline = args.root / "docs/gnatdoc-coverage-baseline.txt"
    report = measure(args.site, args.root, None if args.capture_pristine else json.loads(manifest.read_text()))
    args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    if args.capture_pristine:
        if manifest.exists() or baseline.exists():
            raise CoverageError("refusing to rewrite an existing baseline")
        manifest.write_text(json.dumps(report["native"], indent=2, sort_keys=True) + "\n")
        baseline.write_text("".join(e["identity"] + "\n" for e in report["elements"] if not e["documented"]))
    else:
        ratchet(report, baseline.read_text())
    print(f"Coverage {report['documented_elements']}/{report['total_elements']} "
          f"({report['documented_percentage']}%); {report['source_declarations']} source / "
          f"{report['native_sections']} native / {len(report['renderer_exceptions'])} renderer omission")


if __name__ == "__main__":
    main()
