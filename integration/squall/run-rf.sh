#!/bin/sh
# Task 033B opt-in real Squall RF DataMEL C smoke. Not part of make check,
# ordinary CTest, or hosted CI. It builds the RF provider library and the RF
# runtime from the exact pinned Squall checkout (never a registry `latest`
# image), starts only squall-rf (simulated rf_environment) and Couloir, and
# runs integration/squall/squall_rf_c_smoke.c against the production facade.
# No ProductRxEndpoint, UDP IQ, jobs, or VADB are involved.
#
# Task 033D: `run-rf.sh rx` (make test-squall-rf-rx) instead runs
# integration/squall/squall_rf_rx_c.c: a real ComplexINT16 ProductRxEndpoint
# receive through the production facade. Pinned Squall drops ProductRx data
# unless an RX job is active, so this mode also builds the TEST-ONLY
# squall_rf_job_helper.cpp inside the pinned Squall builder stage (same
# toolchain and exact pinned headers as the provider) and loads it with dlopen.
# The default mode (no argument) is the unchanged Task 033B smoke.
set -eu

mode=${1:-smoke}
case "$mode" in smoke|rx|ada) ;; *) printf 'usage: %s [smoke|rx|ada]\n' "$0" >&2; exit 2 ;; esac

expected_commit=b1015728f904c799fa0c07489fce48e78f67845f
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
control_port=${AMS_MEL_SQUALL_RF_CONTROL_PORT:-21203}
couloir_metrics_port=${AMS_MEL_SQUALL_RF_COULOIR_METRICS_PORT:-21318}
rf_health_port=${AMS_MEL_SQUALL_RF_HEALTH_PORT:-21313}
rf_metrics_port=${AMS_MEL_SQUALL_RF_METRICS_PORT:-21314}
rf_data_port=${AMS_MEL_SQUALL_RF_DATA_PORT:-21601}
build_dir=${AMS_MEL_SQUALL_BUILD_DIR:-"$root/build/squall"}
case "$build_dir" in /*) ;; *) build_dir="$root/$build_dir" ;; esac
project="ams-mel-task033b-$$"
provider_image="localhost/${project}-provider:latest"
builder_image="localhost/${project}-builder:latest"
builder_image_present=0
rf_image="localhost/${project}-rf:latest"
couloir_image="localhost/${project}-couloir:latest"
temp_dir="$build_dir/tmp/$project"
profile="$temp_dir/squall-rf-profile.json"
couloir_config="$temp_dir/couloir.toml"
rf_config="$temp_dir/rf.toml"
runtime_override="$temp_dir/runtime.compose.override.yaml"
runtime_compose="$temp_dir/runtime.compose.yaml"
provider_container=
provider_image_present=0
runtime_images_present=0
started=0

fail() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }
need() { command -v "$1" >/dev/null 2>&1 || fail "$1 is required"; }
for pair in "CONTROL:$control_port" "COULOIR_METRICS:$couloir_metrics_port" \
    "HEALTH:$rf_health_port" "METRICS:$rf_metrics_port" "DATA:$rf_data_port"; do
  value=${pair#*:}
  case "$value" in ''|*[!0-9]*) fail "AMS_MEL_SQUALL_RF_${pair%%:*}_PORT must be an integer" ;; esac
  { test "$value" -ge 1 && test "$value" -le 65535; } ||
    fail "AMS_MEL_SQUALL_RF_${pair%%:*}_PORT must be in 1..65535"
done
test "$(printf '%s\n' "$control_port" "$couloir_metrics_port" "$rf_health_port" \
  "$rf_metrics_port" "$rf_data_port" | sort -u | wc -l)" -eq 5 ||
  fail "RF runtime ports must be distinct"

test -n "${SQUALL_SOURCE_DIR:-}" || fail "SQUALL_SOURCE_DIR must name the pinned Squall checkout"
verify_checkout() { "$root/integration/squall/verify-checkout.sh" "$1" "$2" "$3"; }
verify_checkout "$SQUALL_SOURCE_DIR" \
  open-arsenal/ams-gra-hello-world-sk-sensors-squall "$expected_commit"
verify_checkout "$SQUALL_SOURCE_DIR/ams-interfaces/common-mel" \
  open-arsenal/ams-gra-hello-world-sk-interfaces-common-mel \
  f6908437d8fd2f7fb69896f9eb9cfd272d10c439
verify_checkout "$SQUALL_SOURCE_DIR/ams-interfaces/rf-mel" \
  open-arsenal/ams-gra-hello-world-sk-interfaces-rf-mel \
  762ce84c5555dd0f3ea66f36b321fecf8839b89f
verify_checkout "$SQUALL_SOURCE_DIR/ams-interfaces/rf-mel/ams-math" \
  open-arsenal/ams-gra-hello-world-sk-libraries-ams-math \
  00be45190f0e47d268cece8b8c2f8fb58b5418d2
verify_checkout "$SQUALL_SOURCE_DIR/ams-interfaces/rf-mel/ams-vita" \
  open-arsenal/ams-gra-hello-world-sk-libraries-ams-vita \
  8e12a4cd7ac8ea8776d40b9d0b22fc4a22adaad8
BUILD_COMPOSE_DIR="$SQUALL_SOURCE_DIR/tests/mel-boundary-e2e"
BUILD_COMPOSE_FILE="$BUILD_COMPOSE_DIR/compose.yaml"
for file in "$BUILD_COMPOSE_FILE" "$SQUALL_SOURCE_DIR/compose.yaml" \
    "$SQUALL_SOURCE_DIR/compose.build.yaml" "$SQUALL_SOURCE_DIR/config/rf-simulated.toml"; do
  test -f "$file" || fail "pinned Squall file is missing: $file"
done

need cmake; need cc; need readelf; need python3
if command -v podman >/dev/null 2>&1; then runtime=podman
elif command -v docker >/dev/null 2>&1; then runtime=docker
else fail "Podman or Docker is required"; fi
if test "$runtime" = podman && command -v podman-compose >/dev/null 2>&1; then
  compose() { podman-compose "$@"; }
elif "$runtime" compose version >/dev/null 2>&1; then
  compose() { "$runtime" compose "$@"; }
else fail "podman-compose or a compose plugin is required"; fi

for name in couloir squall-rf; do
  if "$runtime" container inspect "$name" >/dev/null 2>&1; then
    fail "container '$name' already exists; refusing to replace a user Squall stack"
  fi
done

cleanup() {
  status=$?
  trap - EXIT INT TERM
  test -z "$provider_container" || "$runtime" rm -f "$provider_container" >/dev/null 2>&1 || true
  test "$provider_image_present" != 1 || "$runtime" image rm "$provider_image" >/dev/null 2>&1 || true
  test "$builder_image_present" != 1 || "$runtime" image rm "$builder_image" >/dev/null 2>&1 || true
  if test "$started" = 1; then
    if test "$status" -ne 0; then
      (cd "$SQUALL_SOURCE_DIR" && compose -p "$project" -f "$runtime_compose" logs) >&2 2>&1 || true
      test ! -f "$profile" || cat "$profile" >&2
    fi
    (cd "$SQUALL_SOURCE_DIR" && compose -p "$project" -f "$runtime_compose" down) >/dev/null 2>&1 || true
  fi
  test "$runtime_images_present" != 1 ||
    "$runtime" image rm "$rf_image" "$couloir_image" >/dev/null 2>&1 || true
  rm -f "$profile" "$couloir_config" "$rf_config" "$runtime_override" "$runtime_compose"
  rmdir "$temp_dir" "$build_dir/tmp" >/dev/null 2>&1 || true
  exit "$status"
}
trap cleanup EXIT INT TERM

python3 - "$control_port" "$couloir_metrics_port" "$rf_health_port" "$rf_metrics_port" <<'PY'
import socket, sys
for port in map(int, sys.argv[1:]):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        try:
            listener.bind(("127.0.0.1", port))
        except OSError:
            raise SystemExit(f"ERROR: host port {port} is occupied")
PY

mkdir -p "$build_dir/bin" "$build_dir/provider" "$temp_dir"
export ARTIFACTS_DIR="$build_dir"
export SQUALL_MEL_DATA_CONSUMER_IMAGE="$provider_image"

# Extract libsquall_rf_mel.so from the supported E2E image target, built from
# this exact checkout. The consumer application itself is never run.
provider_image_present=1
(cd "$BUILD_COMPOSE_DIR" && compose -p "$project-provider" -f "$BUILD_COMPOSE_FILE" \
  build squall-rf-data-consumer)
"$runtime" image inspect "$provider_image" >/dev/null 2>&1 ||
  fail "could not identify built Squall MEL consumer image: $provider_image"
provider_container=$("$runtime" create "$provider_image")
provider="$build_dir/provider/libsquall_rf_mel.so"
"$runtime" cp "$provider_container:/usr/lib64/libsquall_rf_mel.so" "$provider"
"$runtime" rm "$provider_container" >/dev/null
provider_container=
"$runtime" image rm "$provider_image" >/dev/null
provider_image_present=0
test -s "$provider" || fail "extracted Squall RF MEL library is empty"
readelf --dyn-syms --wide "$provider" | grep -q ' createDataMEL$' ||
  fail "extracted provider does not export createDataMEL"
printf '%s\n' "Squall RF provider: $provider" 'Provider runtime dependencies:'
readelf -d "$provider" | sed -n '/NEEDED/p'

# Production facade and the selected integration client; never installed or added to CTest.
cmake -S "$root/native" -B "$root/native/build" -DAMS_MEL_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build "$root/native/build" --parallel 2
if test "$mode" = rx || test "$mode" = ada; then
  client=squall_rf_rx_c
else
  client=squall_rf_c_smoke
fi
if test "$mode" = ada; then
  if command -v gprbuild >/dev/null 2>&1; then
    GPR_PROJECT_PATH="$root/native:$root/ada${GPR_PROJECT_PATH:+:$GPR_PROJECT_PATH}" \
      AMS_MEL_SQUALL_ADA_BUILD_DIR="$build_dir" \
      gprbuild -p -P "$root/integration/squall/ams_mel_squall_rf.gpr"
  else
    need alr
    AMS_MEL_SQUALL_ADA_BUILD_DIR="$build_dir" \
      alr -C "$root/ada" exec -- gprbuild -p \
        -P "$root/integration/squall/ams_mel_squall_rf.gpr"
  fi
  client=ams_mel_squall_rf
else
  cc -std=c11 -pedantic-errors -Wall -Wextra -Werror \
    -I"$root/native/include" "$root/integration/squall/$client.c" \
    -L"$root/native/build/lib" -Wl,-rpath,"$root/native/build/lib" -lams_mel_c -ldl \
    -o "$build_dir/bin/$client"
fi
readelf -h "$build_dir/bin/$client" | grep -Eq 'Type:.*(EXEC|DYN)' || fail "RF client is not ELF"
readelf -d "$build_dir/bin/$client" | grep -q 'libams_mel_c' || fail "RF client does not link facade"
if readelf -d "$build_dir/bin/$client" | grep -q 'libsquall_rf_mel'; then
  fail "RF client directly links Squall"
fi
if readelf -d "$build_dir/bin/$client" | grep -q 'libmock_'; then
  fail "RF client directly links mock provider"
fi

helper="$build_dir/provider/libsquall_rf_job_helper.so"
if test "$mode" = rx || test "$mode" = ada; then
  # TEST-ONLY job helper, compiled in the pinned builder stage (the provider's
  # own toolchain, boost-devel, and the exact verified checkout headers).
  builder_image_present=1
  "$runtime" build -f "$SQUALL_SOURCE_DIR/Containerfile" --target builder \
    -t "$builder_image" "$SQUALL_SOURCE_DIR"
  rm -f "$helper"
  "$runtime" run --rm --network none \
    -v "$root/integration/squall:/task033d:ro,Z" -v "$build_dir/provider:/out:Z" \
    "$builder_image" c++ -std=c++20 -O2 -shared -fPIC -Wall -Wextra -Werror \
    -isystem /build/ams-interfaces/rf-mel/include \
    -isystem /build/ams-interfaces/common-mel/include \
    -isystem /build/ams-interfaces/rf-mel/ams-math/include \
    -isystem /build/ams-interfaces/rf-mel/ams-vita/include \
    /task033d/squall_rf_job_helper.cpp -o /out/libsquall_rf_job_helper.so
  if test "$mode" = ada; then
    # Podman may still be removing its transient --rm helper container. The
    # EXIT trap retries removal; never force-remove an image in use.
    if "$runtime" image rm "$builder_image" >/dev/null 2>&1; then
      builder_image_present=0
    fi
  else
    "$runtime" image rm "$builder_image" >/dev/null
    builder_image_present=0
  fi
  test -s "$helper" || fail "TEST-ONLY RF job helper was not built"
  if readelf -d "$root/native/build/lib/libams_mel_c.so" | grep -q 'squall_rf_job_helper'; then
    fail "production facade links the test-only job helper"
  fi
  if nm -D --defined-only "$root/native/build/lib/libams_mel_c.so" | grep -q 'squall_rf_test_job'; then
    fail "production facade exports test-only job helper symbols"
  fi
fi

# The smoke mounts pinned config/rf-simulated.toml unchanged. Its emitters are
# DIS-driven and it has zero floor and zero noise, so without DIS traffic every
# IQ sample is 0. For receive evidence that cannot pass vacuously, rx mode
# derives a copy that changes ONLY noise_std_dev (Gaussian AWGN, so values are
# random and never asserted). Every other key stays pinned.
cp "$SQUALL_SOURCE_DIR/config/rf-simulated.toml" "$rf_config"
if test "$mode" = rx || test "$mode" = ada; then
  python3 - "$rf_config" <<'PY'
import re, sys
path = sys.argv[1]
text = open(path, encoding="utf-8").read()
text, count = re.subn(r"(?m)^noise_std_dev = .*$", "noise_std_dev = 0.05", text)
if count != 1:
    raise SystemExit("ERROR: pinned rf-simulated.toml has no unique noise_std_dev")
open(path, "w", encoding="utf-8").write(text)
PY
fi

# Only the simulated RF MFA (rf_environment) and Couloir, host networking.
cat >"$couloir_config" <<EOF
tcp_addr = "127.0.0.1:$control_port"
metrics_port = $couloir_metrics_port

[routes]
"/squall.rf.control.v1.SquallRfControl/" = "/sockets/squall-rf-control.sock"
EOF
cat >"$runtime_override" <<EOF
services:
  couloir:
    image: $couloir_image
    network_mode: host
    command: --config /task033b/couloir.toml
    volumes:
      - backend-sockets:/sockets
      - $couloir_config:/task033b/couloir.toml:ro,Z
  squall-rf:
    image: $rf_image
    network_mode: host
    command: --config /task033b/rf.toml
    environment:
      SQUALL_RF_HEALTH_ADDRESS: "127.0.0.1"
      SQUALL_RF_HEALTH_PORT: "$rf_health_port"
      SQUALL_RF_METRICS_PORT: "$rf_metrics_port"
      SQUALL_RF_DATA_PORT: "$rf_data_port"
    volumes:
      - backend-sockets:/sockets
      - $rf_config:/task033b/rf.toml:ro,Z
EOF
(cd "$SQUALL_SOURCE_DIR" && compose -p "$project" -f compose.yaml -f compose.build.yaml \
  -f "$runtime_override" config) >"$runtime_compose"
runtime_images_present=1
(cd "$SQUALL_SOURCE_DIR" && compose -p "$project" -f "$runtime_compose" build squall-rf couloir)
started=1
(cd "$SQUALL_SOURCE_DIR" && compose -p "$project" -f "$runtime_compose" \
  up -d --no-build squall-rf couloir)

python3 - "$profile" "$project" "$control_port" <<'PY'
import json, sys
with open(sys.argv[1], "w", encoding="utf-8") as output:
    json.dump({"log_level": "info", "control_address": f"127.0.0.1:{sys.argv[3]}",
               "data_host": "127.0.0.1", "client_id": sys.argv[2], "face_id": 0,
               "va_definition_id": 0, "va_instance_id": 0,
               "rx_element_group_label": "0", "rx_stream_id": 0}, output)
    output.write("\n")
PY
printf 'RF MEL profile: %s\n' "$profile"
cat "$profile"

# Finite readiness wait: RF health /ready and the Couloir control port.
python3 - "$rf_health_port" "$control_port" <<'PY'
import socket, sys, time, urllib.error, urllib.request
health, control = int(sys.argv[1]), int(sys.argv[2])
deadline = time.monotonic() + 90.0
ready = control_ready = False
while time.monotonic() < deadline and not (ready and control_ready):
    if not ready:
        try:
            with urllib.request.urlopen(f"http://127.0.0.1:{health}/ready", timeout=1.0) as r:
                ready = r.status == 200
        except (OSError, urllib.error.URLError):
            pass
    if not control_ready:
        try:
            with socket.create_connection(("127.0.0.1", control), timeout=1.0):
                control_ready = True
        except OSError:
            pass
    if not (ready and control_ready):
        time.sleep(0.5)
if not ready:
    raise SystemExit(f"ERROR: timed out waiting for squall-rf /ready on port {health}")
if not control_ready:
    raise SystemExit(f"ERROR: timed out waiting for Couloir control on port {control}")
PY

repeat=${AMS_MEL_SQUALL_REPEAT:-1}
case "$repeat" in ''|*[!0-9]*|0) fail "AMS_MEL_SQUALL_REPEAT must be a positive integer" ;; esac
iteration=1
while test "$iteration" -le "$repeat"; do
  if test "$mode" = ada; then
    printf '\nTask-034A safe Ada RF iteration %s/%s\n' "$iteration" "$repeat"
  else
    printf '\nTask-033%s RF iteration %s/%s\n' "$(test "$mode" = rx && echo D || echo B)" \
      "$iteration" "$repeat"
  fi
  if test "$mode" = rx || test "$mode" = ada; then
    LD_LIBRARY_PATH="$root/native/build/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
      "$build_dir/bin/$client" "$provider" "$profile" "$helper" ||
      fail "RF ProductRx receive failed; see runtime logs and profile"
  else
    LD_LIBRARY_PATH="$root/native/build/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
      "$build_dir/bin/$client" "$provider" "$profile" ||
      fail "RF C smoke failed; see runtime logs and profile"
  fi
  iteration=$((iteration + 1))
done
if test "$mode" = ada; then
  printf '\nPASS: Squall safe Ada RF ProductRx ComplexINT16 receive (%s iteration(s), Squall %s)\n' \
    "$repeat" "$expected_commit"
elif test "$mode" = rx; then
  printf '\nPASS: Squall RF ProductRxEndpoint ComplexINT16 receive (%s iteration(s), Squall %s)\n' \
    "$repeat" "$expected_commit"
else
  printf '\nPASS: Squall RF DataMEL C integration (%s iteration(s), Squall %s)\n' \
    "$repeat" "$expected_commit"
fi
