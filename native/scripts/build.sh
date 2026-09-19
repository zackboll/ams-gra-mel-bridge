#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
command -v cmake >/dev/null 2>&1 || {
    echo "CMake is required on PATH to build ams_mel_c." >&2
    exit 1
}
sh "$root/scripts/configure-build.sh" OFF
env -u LD_RUN_PATH cmake --build "$root/build" --parallel 2
