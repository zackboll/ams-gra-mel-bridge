#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
sh "$root/scripts/configure-build.sh" ON
env -u LD_RUN_PATH cmake --build "$root/build" --parallel 2 --target \
    ams_mel_c mock_ir_provider missing_symbol_ir_provider
