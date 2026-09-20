#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$root/scripts/build-tree.sh"
command -v cmake >/dev/null 2>&1 || {
    echo "CMake is required on PATH to build ams_mel_c." >&2
    exit 1
}
# Production facade only.  This must never touch the contract-test tree.
ams_mel_select_tree production
sh "$root/scripts/configure-build.sh" production
env -u LD_RUN_PATH cmake --build "$ams_mel_build_dir" --parallel 2
