#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cmake -S "$root" -B "$root/build" \
    -DAMS_MEL_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build "$root/build" --parallel 2 --target \
    ams_mel_c mock_ir_provider missing_symbol_ir_provider