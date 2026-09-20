#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
command -v gprbuild >/dev/null 2>&1 || {
    echo "GNAT/GPRbuild is required on PATH. Alternatively use: alr -C ada/tests run" >&2
    exit 1
}
# The Ada binding links the production facade (native/build/lib, which
# ams_mel_c.gpr names), while the mock providers come from the separate
# contract-test tree.  Build both; neither configure step touches the other.
sh "$root/native/scripts/build.sh"
sh "$root/native/scripts/build-test-providers.sh"
GPR_PROJECT_PATH="$root/native:$root/ada${GPR_PROJECT_PATH:+:$GPR_PROJECT_PATH}"
export GPR_PROJECT_PATH
gprbuild -p -P "$root/ada/tests/ams_mel_tests.gpr"
# The executable links the production facade but exercises test-only
# failpoints, so resolve the contract-test facade at run time.  The test project
# emits DT_RUNPATH, which LD_LIBRARY_PATH overrides.
LD_LIBRARY_PATH="$root/native/build-tests/lib:$root/native/build/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export LD_LIBRARY_PATH
AMS_MEL_TEST_PROVIDER_DIR="${AMS_MEL_TEST_PROVIDER_DIR:-$root/native/build-tests/test-providers}"
export AMS_MEL_TEST_PROVIDER_DIR
"$root/ada/tests/bin/ams_mel_smoke"
