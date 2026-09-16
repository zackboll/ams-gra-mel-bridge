#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
sh "$root/native/scripts/test.sh"
sh "$root/scripts/test_ada.sh"
python3 "$root/scripts/check_final_newlines.py"
if git -C "$root" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    git -C "$root" diff --check
    git -C "$root" diff --cached --check
fi
