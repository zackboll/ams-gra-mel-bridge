#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
sh "$root/scripts/configure-build.sh" ON
env -u LD_RUN_PATH cmake --build "$root/build" --parallel 2
ctest --test-dir "$root/build" --output-on-failure
