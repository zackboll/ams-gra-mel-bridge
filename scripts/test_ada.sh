#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
command -v gprbuild >/dev/null 2>&1 || {
    echo "GNAT/GPRbuild is required on PATH. Alternatively use: alr -C ada/tests run" >&2
    exit 1
}
sh "$root/native/scripts/build-test-providers.sh"
GPR_PROJECT_PATH="$root/native:$root/ada${GPR_PROJECT_PATH:+:$GPR_PROJECT_PATH}"
export GPR_PROJECT_PATH
gprbuild -p -P "$root/ada/tests/ams_mel_tests.gpr"
LD_LIBRARY_PATH="$root/native/build/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export LD_LIBRARY_PATH
"$root/ada/tests/bin/ams_mel_smoke"
