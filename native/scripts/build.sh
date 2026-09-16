#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
command -v cmake >/dev/null 2>&1 || {
    echo "CMake is required on PATH to build ams_mel_c." >&2
    exit 1
}
cmake -S "$root" -B "$root/build" \
    -DAMS_MEL_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build "$root/build" --parallel 2
