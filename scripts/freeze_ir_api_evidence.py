#!/usr/bin/env python3
"""Explicit maintainer action: freeze verified full upstream checkouts, offline.

Never invoked by ordinary builds/checks. Does not generate implementation claims
or approve new catalog/review hashes. See docs/reference/irmel-api-schema.md.
"""

import argparse
import base64
import json
from pathlib import Path
import subprocess
import zlib

import check_ir_api_coverage as audit


def freeze(checkouts):
    evidence = {"schema_version": 1, "repositories": {}}
    for name, (url, commit, tree) in audit.PINS.items():
        checkout = checkouts[name]

        def git(*args):
            return subprocess.check_output(["git", "-C", str(checkout), *args])

        audit.require(git("rev-parse", commit + "^{tree}").decode().strip() == tree,
                      "upstream commit/tree mismatch")
        objects = {}

        def store(oid, kind):
            payload = git("cat-file", kind, oid)
            audit.require(audit.git_hash(kind, payload) == oid, "Git object mismatch")
            objects[oid] = {"kind": kind, "zlib_base64": base64.b64encode(zlib.compress(payload, 9)).decode()}

        store(commit, "commit")
        store(tree, "tree")
        for line in git("ls-tree", "-r", "-t", tree).decode().splitlines():
            _, kind, oid, path = line.split(maxsplit=3)
            if (kind == "tree" or path.startswith("include/") and path.endswith((".h", ".hpp"))
                    or path in {"LICENSE", "INTENT.md"}):
                store(oid, kind)
        evidence["repositories"][name] = {"url": url, "commit": commit, "tree": tree, "objects": objects}
    return evidence


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ir-checkout", type=Path, required=True)
    parser.add_argument("--common-checkout", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True,
                        help="Existing directory for candidate evidence/catalog; never auto-approve")
    args = parser.parse_args()
    evidence = freeze({"ir": args.ir_checkout, "common": args.common_checkout})
    for name, value in ((Path(audit.EVIDENCE).name, evidence),
                        (Path(audit.REFERENCE).name, audit.catalog(evidence))):
        (args.output / name).write_text(json.dumps(value, indent=2) + "\n")


if __name__ == "__main__":
    main()
