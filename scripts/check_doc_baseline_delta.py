#!/usr/bin/env python3
"""Reject automatic authorization of new gaps by rewriting a prior baseline."""

import argparse
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FILES = ("docs/gnatdoc-coverage-baseline.txt", "docs/gnatdoc-warning-baseline.txt")


def delta(old, new):
    from collections import Counter
    return list((Counter(new.splitlines()) - Counter(old.splitlines())).elements())


def check(base):
    for filename in FILES:
        result = subprocess.run(["git", "-C", str(ROOT), "show", f"{base}:{filename}"],
                                capture_output=True, text=True)
        if result.returncode:
            # Initial foundation introduces an absent baseline; source/native
            # qualification still applies. Verify absence, not an unknown ref.
            subprocess.run(["git", "-C", str(ROOT), "rev-parse", "--verify", base], check=True,
                           capture_output=True)
            listed = subprocess.check_output(["git", "-C", str(ROOT), "ls-tree", "-r", "--name-only",
                                              base, "--", filename], text=True)
            if listed.strip():
                raise ValueError(f"cannot read prior baseline: {filename}")
            continue
        added = delta(result.stdout, (ROOT / filename).read_text())
        if added:
            raise ValueError(f"{filename}: baseline rewrite authorizes {len(added)} new gaps")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base")
    args = parser.parse_args()
    check(args.base)
    print("No existing baseline expanded")
