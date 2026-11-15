#!/bin/sh
# Explicitly opt-in: no ordinary build invokes this script or downloads sources.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
pin=cf22a80ec98398eb361df758ada7bfb4024710a3
logs=${MINIMAL_MEL_LOG_DIR:-${TMPDIR:-/tmp}/irmel-minimal-ada-logs-$$}
mkdir -p "$logs"
logs=$(CDPATH= cd -- "$logs" && pwd)
reference=${MINIMAL_MEL_REFERENCE_DIR:-}
if [ -z "$reference" ]; then
    for candidate in "$root/../minimal-mel-example" /tmp/irmel-ada-minimal-001-reference; do
        if [ -d "$candidate/.git" ]; then
            reference=$candidate
            break
        fi
    done
fi
if [ -z "$reference" ]; then
    reference=$(mktemp -d /tmp/irmel-ada-minimal-reference.XXXXXX)
    git clone https://github.com/zackboll/minimal-mel-example.git "$reference" >"$logs/clone.log" 2>&1
    git -C "$reference" checkout --detach "$pin" >>"$logs/clone.log" 2>&1
fi
reference=$(CDPATH= cd -- "$reference" && pwd)
[ "$(git -C "$reference" rev-parse HEAD)" = "$pin" ] || {
    echo "Reference checkout must already be at $pin: $reference" >&2; exit 1;
}
[ -z "$(git -C "$reference" status --porcelain --untracked-files=no)" ] || {
    echo "Reference checkout has modified tracked sources" >&2; exit 1;
}
command -v gprbuild >/dev/null || {
    echo "Use the pinned Alire environment: alr -n -C ada exec -- make -C $root test-minimal-ir-ada" >&2
    exit 1
}
run() {
    name=$1; shift
    set +e
    "$@" >"$logs/$name.stdout" 2>"$logs/$name.stderr"
    code=$?
    set -e
    echo "$code" >"$logs/$name.exit"
    cat "$logs/$name.stdout"
    cat "$logs/$name.stderr" >&2
    [ "$code" -eq 0 ] || { echo "$name failed (exit $code); logs: $logs" >&2; exit "$code"; }
}
git -C "$root" rev-parse HEAD 'HEAD^{tree}' >"$logs/bridge.identity"
git -C "$root" diff --binary >"$logs/bridge.diff"
git -C "$reference" rev-parse HEAD 'HEAD^{tree}' >"$logs/reference.identity"
run compilers c++ --version
run ada-compiler gnatls -v
# Match the entire reference include closure. The bridge also vendors unrelated
# optional Scheduling/Track declarations that are absent from this stub closure.
python3 - "$root" "$reference" <<'PY'
from pathlib import Path
import sys
bridge, reference = map(Path, sys.argv[1:])
for component in ("ir-mel", "common-mel", "ams-math"):
    ours = bridge / "native" / "vendor" / component / "include"
    theirs = reference / "third_party" / component / "include"
    if not ours.is_dir() or not theirs.is_dir():
        raise SystemExit(f"Cannot locate vendored {component} declarations")
    for header in theirs.rglob("*"):
        if header.is_file():
            other = ours / header.relative_to(theirs)
            if not other.is_file() or header.read_bytes() != other.read_bytes():
                raise SystemExit(f"Incompatible upstream declaration: {header}")
PY
export GPR_PROJECT_PATH="$root/ada:$root/native${GPR_PROJECT_PATH:+:$GPR_PROJECT_PATH}"
for config in Debug Release; do
    # Separate provider output trees; do not alter reference sources.
    build="$reference/build"
    [ "$config" = Debug ] || build="$reference/build-release"
    run "$config-reference-configure" cmake -S "$reference" -B "$build" -DCMAKE_BUILD_TYPE="$config"
    reference_cxx=$(sed -n 's/^CMAKE_CXX_COMPILER:FILEPATH=//p' "$build/CMakeCache.txt")
    run "$config-reference-compiler" "$reference_cxx" --version
    run "$config-reference-build" cmake --build "$build" --parallel 2
    run "$config-reference-tests" ctest --test-dir "$build" --output-on-failure
    python3 - "$build" <<'PY'
import json, subprocess, sys
tests = json.loads(subprocess.check_output(["ctest", "--test-dir", sys.argv[1], "--show-only=json-v1"]))
if {t["name"] for t in tests["tests"]} != {"provider_contract", "dynamic_load", "consumer_required_api", "upstream_declarations"}:
    raise SystemExit("Reference must qualify all four expected CTests")
PY
    provider=${MINIMAL_MEL_PROVIDER:-$build/libminimal_ir_mel_provider.so}
    case "$provider" in /*) ;; *) echo "Provider path must be absolute" >&2; exit 1;; esac
    [ -f "$provider" ] || { echo "Provider library missing: $provider" >&2; exit 1; }
    # An externally supplied path must be this qualified build's actual binary.
    cmp "$provider" "$build/libminimal_ir_mel_provider.so" || {
        echo "Provider is not the qualified $config binary" >&2; exit 1;
    }
    sha256sum "$provider" >"$logs/$config-provider.sha256"
    run "$config-native-configure" cmake -S "$root/native" -B "$root/native/build" \
        -DAMS_MEL_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE="$config" -DAMS_MEL_SANITIZED_BUILD_ENVIRONMENT:BOOL=ON
    run "$config-native-build" cmake --build "$root/native/build" --parallel 2
    native_cxx=$(sed -n 's/^CMAKE_CXX_COMPILER:FILEPATH=//p' "$root/native/build/CMakeCache.txt")
    run "$config-native-compiler" "$native_cxx" --version
    cp "$build/CMakeCache.txt" "$logs/$config-reference-CMakeCache.txt"
    cp "$root/native/build/CMakeCache.txt" "$logs/$config-native-CMakeCache.txt"
    opt=-O0
    [ "$config" = Debug ] || opt=-O2
    run "$config-ada-build" gprbuild -f -p -P "$root/ada/tests/ams_mel_tests.gpr" \
        ams_mel_ir_minimal_consumer.adb ams_mel_ir_minimal_negative.adb -cargs "$opt"
    sha256sum "$root/native/build/lib/libams_mel_c.so.0" \
        "$root/ada/tests/bin/ams_mel_ir_minimal_consumer" >"$logs/$config-consumer.sha256"
    # Never inherit Alire's contract-test facade override for the real run.
    run "$config-consumer" env LD_LIBRARY_PATH="$root/native/build/lib" \
        "$root/ada/tests/bin/ams_mel_ir_minimal_consumer" "$provider"
    run "$config-negative" env LD_LIBRARY_PATH="$root/native/build/lib" \
        python3 "$root/integration/minimal-mel-example/test-ada-cli.py" \
        "$root/ada/tests/bin/ams_mel_ir_minimal_consumer" \
        "$root/ada/tests/bin/ams_mel_ir_minimal_negative" "$logs" "$config"
done
echo "Debug and Release real-provider Ada qualification passed; logs: $logs"
