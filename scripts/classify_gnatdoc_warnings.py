#!/usr/bin/env python3
"""Classify every raw diagnostic using its verified declaration and scope."""

import argparse
import collections
import json
from pathlib import Path

from compare_gnatdoc_warnings import (
    WarningComparisonError, code_tokens, declaration_index, identities_from_log,
    locate_declaration, parse_warnings, _subject_declaration,
)
from gnatdoc_inventory import ROOT, specifications
from gnatdoc_coverage_model import parameter_names
from check_doc_source_integrity import tokens


def classify(text, root=ROOT):
    public, private = specifications(root)
    units = {path.name: (name, False, path) for name, path in public.items()}
    units.update({path.name: (name, True, path) for name, path in private.items()})
    records = parse_warnings(text, "classification")
    # Reuse the fail-closed identity mapper, retaining the unfiltered stream.
    identities = identities_from_log(text, root / "ada/src", "classification")
    counts = dict.fromkeys(("public", "private-part", "private-unit", "tool", "unclassified"), 0)
    entries = []
    cache = {}
    for record, identity in zip(records, identities, strict=True):
        filename, line, column, message, subject = record
        if filename not in units:
            raise WarningComparisonError(f"unclassified diagnostic source: {filename}")
        package, hidden, path = units[filename]
        if filename not in cache:
            lines = path.read_text().splitlines()
            boundary = next((n for n, value in enumerate(lines, 1)
                             if code_tokens(value).strip().lower() == "private"), len(lines) + 1)
            cache[filename] = lines, declaration_index(lines), boundary
        lines, index, boundary = cache[filename]
        declaration = locate_declaration(lines, index, line, column)
        if message.startswith("parameter ") and subject not in parameter_names(declaration["signature"]):
            raise WarningComparisonError(f"warning parameter does not belong to its full profile: {identity}")
        if subject and message.startswith(("component ", "enumeration literal ", "generic formal ")):
            declaration = _subject_declaration(lines, index, declaration, subject, message)
        if not (declaration["scope"] == package or declaration["scope"].startswith(package + ".")):
            raise WarningComparisonError(f"unclassified declaration scope: {identity}")
        category = "private-unit" if hidden else (
            "private-part" if declaration["start"] >= boundary else "public")
        counts[category] += 1
        entries.append({"identity": identity, "category": category,
                        "scope": declaration["scope"], "source": path.relative_to(root).as_posix(),
                        "source_profile_tokens": tokens("\n".join(
                            lines[declaration["start"] - 1:declaration["end"]]))})
    if sum(counts.values()) != len(records):
        raise WarningComparisonError("raw/classified warning reconciliation failed")
    return {"raw_total": len(records), "counts": counts, "mapping_failures": [],
            "warnings": entries}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    report = classify(args.log.read_text(), args.root)
    args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    print(f"Raw warnings: {report['raw_total']}; classified: {report['counts']}")


if __name__ == "__main__":
    main()
