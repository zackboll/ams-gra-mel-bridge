#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$root/scripts/build-tree.sh"
# Contract-test tree only.  This must never touch the production tree.
ams_mel_select_tree tests
sh "$root/scripts/configure-build.sh" tests
env -u LD_RUN_PATH cmake --build "$ams_mel_build_dir" --parallel 2
ctest --test-dir "$ams_mel_build_dir" --output-on-failure
