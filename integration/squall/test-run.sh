#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
script="$root/integration/squall/run.sh"
verifier="$root/integration/squall/verify-checkout.sh"
scratch="/tmp/ams-mel-task004-verifier-$$"
trap 'rm -rf "$scratch"' EXIT INT TERM

require() {
  grep -F -q -- "$1" "$script" || {
    printf 'FAIL: missing Task-004 runtime contract: %s\n' "$1" >&2
    exit 1
  }
}

expect_port_preflight() {
  expected_status=$1
  expected_output=$2
  shift 2
  output=$(env \
    -u AMS_MEL_SQUALL_CONTROL_PORT \
    -u AMS_MEL_SQUALL_COULOIR_METRICS_PORT \
    -u AMS_MEL_SQUALL_OPTICAL_HEALTH_PORT \
    -u AMS_MEL_SQUALL_OPTICAL_METRICS_PORT \
    "$@" \
    AMS_MEL_SQUALL_PREFLIGHT_ONLY=1 \
    AMS_MEL_SQUALL_BUILD_DIR="/tmp/ams-mel-task004-stub-$$" \
    SQUALL_SOURCE_DIR=/task004-squall \
    sh "$script" c 2>&1) && status=0 || status=$?
  test "$status" = "$expected_status" || {
    printf 'FAIL: port preflight status=%s, expected=%s: %s\n' \
      "$status" "$expected_status" "$output" >&2
    exit 1
  }
  printf '%s\n' "$output" | grep -F -q "$expected_output" || {
    printf 'FAIL: port preflight output missing %s: %s\n' "$expected_output" "$output" >&2
    exit 1
  }
}

expect_mode_preflight() {
  expected_status=$1
  expected_output=$2
  selected_mode=$3
  output=$(env \
    AMS_MEL_SQUALL_PREFLIGHT_ONLY=1 \
    AMS_MEL_SQUALL_BUILD_DIR="/tmp/ams-mel-task004-mode-$$-$selected_mode" \
    SQUALL_SOURCE_DIR=/task004-squall \
    sh "$script" "$selected_mode" 2>&1) && status=0 || status=$?
  test "$status" = "$expected_status" || {
    printf 'FAIL: mode %s status=%s, expected=%s: %s\n' \
      "$selected_mode" "$status" "$expected_status" "$output" >&2
    exit 1
  }
  printf '%s\n' "$output" | grep -F -q "$expected_output" || {
    printf 'FAIL: mode %s output missing %s: %s\n' \
      "$selected_mode" "$expected_output" "$output" >&2
    exit 1
  }
}

render_hostile_optical_environment() {
  env \
    -u AMS_MEL_SQUALL_OPTICAL_HEALTH_PORT \
    -u AMS_MEL_SQUALL_OPTICAL_METRICS_PORT \
    SQUALL_OPTICAL_HEALTH_ADDRESS=hostile \
    SQUALL_OPTICAL_HEALTH_PORT=http://127.0.0.1:56000/ready \
    SQUALL_OPTICAL_METRICS_PORT=bad \
    SQUALL_IR_DATA_PORT=not-a-port \
    "$@" \
    AMS_MEL_SQUALL_PREFLIGHT_ONLY=1 \
    AMS_MEL_SQUALL_BUILD_DIR="/tmp/ams-mel-task004-hostile-$$" \
    SQUALL_SOURCE_DIR=/task004-squall \
    sh "$script" c
}

require_rendered_once() {
  rendered=$1
  expected=$2
  count=$(printf '%s\n' "$rendered" | grep -F -c "$expected" || true)
  test "$count" -eq 1 || {
    printf 'FAIL: expected rendered environment exactly once: %s\n%s\n' \
      "$expected" "$rendered" >&2
    exit 1
  }
}

# These run the real initialization, validator, collision checks, and renderer
# under set -u without requiring a container runtime or touching Squall.
expect_mode_preflight 0 'Task-004 runtime ports:' rust
expect_mode_preflight 0 'Task-004 runtime ports:' python
expect_mode_preflight 1 'mode must be all, c, ada, rust, or python' invalid
printf '%s\n' 'PASS: Task-013 language mode validation preflights'

expect_port_preflight 0 'Couloir control:  21203'
expect_port_preflight 0 'Optical health:   21315'
expect_port_preflight 0 'Optical metrics:  21316'
expect_port_preflight 0 'Couloir control:  21203' \
  AMS_MEL_SQUALL_CONTROL_PORT= AMS_MEL_SQUALL_COULOIR_METRICS_PORT=
expect_port_preflight 0 'Couloir control:  21204' AMS_MEL_SQUALL_CONTROL_PORT=21204
expect_port_preflight 0 'Couloir metrics:  21319' AMS_MEL_SQUALL_COULOIR_METRICS_PORT=21319
expect_port_preflight 0 'Optical health:   21315' \
  AMS_MEL_SQUALL_OPTICAL_HEALTH_PORT=
expect_port_preflight 0 'Optical metrics:  21316' \
  AMS_MEL_SQUALL_OPTICAL_METRICS_PORT=
expect_port_preflight 0 'Optical health:   21320' \
  AMS_MEL_SQUALL_OPTICAL_HEALTH_PORT=21320
expect_port_preflight 0 'Optical metrics:  21321' \
  AMS_MEL_SQUALL_OPTICAL_METRICS_PORT=21321
for invalid in 0 65536 abc; do
  expect_port_preflight 1 'AMS_MEL_SQUALL_CONTROL_PORT must be an integer in 1..65535' \
    AMS_MEL_SQUALL_CONTROL_PORT="$invalid"
  expect_port_preflight 1 'AMS_MEL_SQUALL_COULOIR_METRICS_PORT must be an integer in 1..65535' \
    AMS_MEL_SQUALL_COULOIR_METRICS_PORT="$invalid"
  expect_port_preflight 1 'AMS_MEL_SQUALL_OPTICAL_HEALTH_PORT must be an integer in 1..65535' \
    AMS_MEL_SQUALL_OPTICAL_HEALTH_PORT="$invalid"
  expect_port_preflight 1 'AMS_MEL_SQUALL_OPTICAL_METRICS_PORT must be an integer in 1..65535' \
    AMS_MEL_SQUALL_OPTICAL_METRICS_PORT="$invalid"
done
rendered=$(render_hostile_optical_environment)
require_rendered_once "$rendered" 'SQUALL_OPTICAL_HEALTH_ADDRESS: "127.0.0.1"'
require_rendered_once "$rendered" 'SQUALL_OPTICAL_HEALTH_PORT: "21315"'
require_rendered_once "$rendered" 'SQUALL_OPTICAL_METRICS_PORT: "21316"'
require_rendered_once "$rendered" 'SQUALL_IR_DATA_PORT: "21600"'
rendered=$(render_hostile_optical_environment \
  AMS_MEL_SQUALL_OPTICAL_HEALTH_PORT=21320 \
  AMS_MEL_SQUALL_OPTICAL_METRICS_PORT=21321)
require_rendered_once "$rendered" 'SQUALL_OPTICAL_HEALTH_ADDRESS: "127.0.0.1"'
require_rendered_once "$rendered" 'SQUALL_OPTICAL_HEALTH_PORT: "21320"'
require_rendered_once "$rendered" 'SQUALL_OPTICAL_METRICS_PORT: "21321"'
require_rendered_once "$rendered" 'SQUALL_IR_DATA_PORT: "21600"'

expect_port_preflight 1 'must use distinct host ports' AMS_MEL_SQUALL_COULOIR_METRICS_PORT=21203
expect_port_preflight 1 'must use distinct host ports' AMS_MEL_SQUALL_OPTICAL_HEALTH_PORT=21203
expect_port_preflight 1 'must use distinct host ports' AMS_MEL_SQUALL_OPTICAL_METRICS_PORT=21203
expect_port_preflight 1 'must use distinct host ports' AMS_MEL_SQUALL_OPTICAL_HEALTH_PORT=21318
expect_port_preflight 1 'must use distinct host ports' AMS_MEL_SQUALL_OPTICAL_METRICS_PORT=21318
expect_port_preflight 1 'must use distinct host ports' AMS_MEL_SQUALL_OPTICAL_METRICS_PORT=21315
printf '%s\n' 'PASS: Task-004 port default/validation preflights'

# Language builds and executions remain conditional while all selects each one.
require 'if test "$mode" = all || test "$mode" = c; then'
require 'if test "$mode" = all || test "$mode" = ada; then'
require 'if test "$mode" = all || test "$mode" = rust; then'
require 'if test "$mode" = all || test "$mode" = python; then'
require 'need alr'
require 'need cargo'
require 'CARGO_TARGET_DIR="$build_dir/rust-target"'
require 'AMS_MEL_NATIVE_LIB_DIR="$root/native/build/lib"'
require '--release --locked --offline'
require 'rust_client="$build_dir/rust-target/release/ams-mel-squall-ir"'
require 'check_client_elf "$rust_client" Rust'
require 'readelf -d "$rust_client" | grep -q '\''libams_mel_c'\'''
require 'grep -q '\''libsquall_ir_mel'\'''
require 'grep -q '\''mock.*provider'\'''
require '"$rust_client" "$provider" "$profile" "$frames" "$timeout"'
require 'PYTHONPYCACHEPREFIX="$build_dir/python-cache"'
require 'python3 -W error -m py_compile'
require '"$root/integration/squall/squall_ir_python.py"'
require 'PYTHONPATH="$root/python"'
require 'AMS_MEL_NATIVE_LIB="$root/native/build/lib/libams_mel_c.so.0"'
require 'python3 -W error "$root/integration/squall/squall_ir_python.py"'
require '"$provider" "$profile" "$frames" "$timeout"'
printf '%s\n' 'PASS: Task-013 language mode/build/link contracts'

for runtime in podman docker; do
  require 'RUNTIME_COMPOSE_FILE="$SQUALL_SOURCE_DIR/compose.yaml"'
  require 'RUNTIME_BUILD_COMPOSE_FILE="$SQUALL_SOURCE_DIR/compose.build.yaml"'
  require '-f "$RUNTIME_BUILD_COMPOSE_FILE" -f "$runtime_override" config'
  require 'podman-compose -p "$compose_project" -f "$runtime_compose" "$@"'
  require 'run_runtime_compose "$project" build squall-optical couloir'
  require 'run_runtime_compose "$project" up -d --no-build squall-optical couloir'
  require 'image: $optical_image'
  require 'image: $couloir_image'
  require '"$runtime" image inspect "$optical_image" "$couloir_image"'
  require 'assert_host_network squall-optical "$optical_container"'
  require 'assert_host_network couloir "$couloir_container"'
  require 'network_mode: host'
  require 'data_host": "127.0.0.1"'
  require 'tcp_addr = "127.0.0.1:$control_port"'
  require 'metrics_port = $couloir_metrics_port'
  require 'SQUALL_OPTICAL_HEALTH_PORT: "$optical_health_port"'
  require 'SQUALL_OPTICAL_METRICS_PORT: "$optical_metrics_port"'
  require 'SQUALL_IR_DATA_PORT: "21600"'
  require 'optical_ready_url = f"http://127.0.0.1:{optical_health_port}/ready"'
  require '"/squall.rf.control.v1.SquallRfControl/" = "/sockets/squall-rf-control.sock"'
  require '"/squall.optical.control.v1.SquallIrControl/" = "/sockets/squall-optical-control.sock"'
  require 'run_compose "$project_provider" build squall-ir-data-consumer'
  require '"$runtime" image inspect "$provider_image"'
  require '"$runtime" image rm "$provider_image"'
  require '"$runtime" image rm "$optical_image" "$couloir_image"'
  printf 'PASS: %s stub runtime topology regression\n' "$runtime"
done

for forbidden in host.containers.internal host.docker.internal podman_network_gateway host-gateway 55010; do
  if grep -F -q "$forbidden" "$script"; then
    printf 'FAIL: obsolete bridge-routing dependency remains: %s\n' "$forbidden" >&2
    exit 1
  fi
done

if grep -E -q 'image[[:space:]]+prune|system[[:space:]]+prune|image[[:space:]]+rm.*registry\.gitlab\.com' "$script"; then
  printf '%s\n' 'FAIL: Task-004 cleanup may prune layers or remove an upstream image tag' >&2
  exit 1
fi

printf '%s\n' 'PASS: Task-004 host-network stub regressions'

make_checkout() {
  checkout=$1
  remote=$2
  mkdir -p "$checkout"
  git -C "$checkout" init -q
  git -C "$checkout" config user.name Task004
  git -C "$checkout" config user.email task004@example.invalid
  printf '%s\n' clean >"$checkout/input.txt"
  git -C "$checkout" add input.txt
  git -C "$checkout" commit -q -m initial
  git -C "$checkout" remote add origin "$remote"
}

expect_verifier_failure() {
  expected=$1
  shift
  output=$(sh "$verifier" "$@" 2>&1) && status=0 || status=$?
  test "$status" -ne 0 || {
    printf 'FAIL: checkout verifier unexpectedly accepted %s\n' "$*" >&2
    exit 1
  }
  printf '%s\n' "$output" | grep -F -q "$expected" || {
    printf 'FAIL: verifier output missing %s: %s\n' "$expected" "$output" >&2
    exit 1
  }
}

mkdir -p "$scratch"
make_checkout "$scratch/root" \
  https://github.com/open-arsenal/ams-gra-hello-world-sk-sensors-squall.git
root_sha=$(git -C "$scratch/root" rev-parse HEAD)
sh "$verifier" "$scratch/root" \
  open-arsenal/ams-gra-hello-world-sk-sensors-squall "$root_sha"
expect_verifier_failure 'revision mismatch' "$scratch/root" \
  open-arsenal/ams-gra-hello-world-sk-sensors-squall \
  0000000000000000000000000000000000000000
expect_verifier_failure 'does not identify expected repository' "$scratch/root" \
  open-arsenal/wrong-repository "$root_sha"
printf '%s\n' dirty >"$scratch/root/untracked.txt"
expect_verifier_failure 'tracked or untracked non-ignored changes' "$scratch/root" \
  open-arsenal/ams-gra-hello-world-sk-sensors-squall "$root_sha"
rm "$scratch/root/untracked.txt"

make_checkout "$scratch/dependency" \
  https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-common-mel.git
dependency_sha=$(git -C "$scratch/dependency" rev-parse HEAD)
expect_verifier_failure 'revision mismatch' "$scratch/dependency" \
  open-arsenal/ams-gra-hello-world-sk-interfaces-common-mel \
  1111111111111111111111111111111111111111
expect_verifier_failure 'does not identify expected repository' "$scratch/dependency" \
  open-arsenal/ams-gra-hello-world-sk-interfaces-ir-mel "$dependency_sha"
printf '%s\n' dirty >>"$scratch/dependency/input.txt"
expect_verifier_failure 'tracked or untracked non-ignored changes' "$scratch/dependency" \
  open-arsenal/ams-gra-hello-world-sk-interfaces-common-mel "$dependency_sha"

printf '%s\n' 'PASS: Task-004 pinned checkout verifier regressions'
