#!/usr/bin/env python3
"""Inventory MEL library-unit headers independently of GNATdoc output.

The renderer does not emit sections for package renamings. Report them as
unsupported, not covered, until an independently validated representation is
qualified. This is an initial campaign diagnostic, not a publication gate.
"""

import argparse
import collections
import hashlib
import json
import re
from pathlib import Path

from compare_gnatdoc_warnings import code_tokens, declaration_index

ROOT = Path(__file__).resolve().parent.parent
HEADER = re.compile(r"^(private[ \t]+)?package[ \t]+([\w.]+)[ \t]+is\b", re.M | re.I)


def specifications(root=ROOT):
    """Derive visibility from the library-unit header, never a nested package."""
    public, private = {}, {}
    for source in sorted((root / "ada/src").glob("*.ads")):
        code = "\n".join(code_tokens(line) for line in source.read_text().splitlines())
        match = HEADER.search(code)
        if match is None:
            raise ValueError(f"unsupported library-unit header: {source.name}")
        destination = private if match[1] else public
        if match[2] in public or match[2] in private:
            raise ValueError(f"duplicate library unit: {match[2]}")
        destination[match[2]] = source
    if not public:
        raise ValueError("no public specifications")
    return public, private


def inventory(root=ROOT):
    public, private = specifications(root)
    result = {"schema_version": 1, "status": "initial diagnostic; not coverage qualification",
              "public_packages": {}, "private_packages": sorted(private),
              "unsupported_generator_constructs": []}
    totals = collections.Counter()
    for package, source in public.items():
        lines = source.read_text().splitlines()
        boundary = next((n for n, line in enumerate(lines, 1)
                         if code_tokens(line).strip().lower() == "private"), len(lines) + 1)
        declarations = [d for ds in declaration_index(lines).values() for d in ds
                        if d["start"] < boundary and d["scope"] == f"{package}.{d['name']}"]
        kinds = collections.Counter()
        for declaration in declarations:
            signature = declaration["signature"]
            kind = signature.split()[0]
            if kind not in ("type", "subtype", "procedure", "function", "package"):
                kind = "exception" if " : exception" in signature else "object"
            kinds[kind] += 1
            if kind == "package" and " renames " in signature:
                result["unsupported_generator_constructs"].append({
                    "identity": declaration["scope"], "signature": signature,
                    "reason": "GNATdoc 26.0.0 omits the public package-renaming section"})
        result["public_packages"][package] = {
            "source": source.relative_to(root).as_posix(),
            "sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
            "declarations": len(declarations), "kinds": dict(sorted(kinds.items()))}
        totals.update(kinds)
    result["totals"] = {"packages": len(public), "declarations": sum(totals.values()),
                        "kinds": dict(sorted(totals.items()))}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--source-list", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    public, _ = specifications(args.root)
    if args.source_list:
        args.source_list.parent.mkdir(parents=True, exist_ok=True)
        args.source_list.write_text("".join(p.name + "\n" for p in public.values()))
    report = json.dumps(inventory(args.root), indent=2, sort_keys=True) + "\n"
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(report)
    else:
        print(report, end="")


if __name__ == "__main__":
    main()
