#!/usr/bin/env python3
"""Reject first-party repository text files without a final newline."""

from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parent.parent
paths = subprocess.check_output(
    ["git", "-C", str(ROOT), "ls-files", "-z", "--cached", "--others",
     "--exclude-standard"], text=False
).split(b"\0")
missing = []
for raw_path in paths:
    if not raw_path:
        continue
    relative = raw_path.decode("utf-8")
    if relative.startswith("native/vendor/"):
        continue
    path = ROOT / relative
    data = path.read_bytes()
    if data and b"\0" not in data and not data.endswith(b"\n"):
        missing.append(relative)

if missing:
    for relative in missing:
        print(f"missing final newline: {relative}", file=sys.stderr)
    sys.exit(1)

print("PASS: all first-party repository text files end with a newline")
