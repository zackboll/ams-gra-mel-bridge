#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
script="$root/integration/squall/run.sh"

require() {
  grep -F -q "$1" "$script" || {
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

for runtime in podman docker; do
  require 'RUNTIME_COMPOSE_FILE="$SQUALL_SOURCE_DIR/compose.yaml"'
  require 'run_runtime_compose "$project" up -d squall-optical couloir'
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
  printf 'PASS: %s stub runtime topology regression\n' "$runtime"
done

for forbidden in host.containers.internal host.docker.internal podman_network_gateway host-gateway 55010; do
  if grep -F -q "$forbidden" "$script"; then
    printf 'FAIL: obsolete bridge-routing dependency remains: %s\n' "$forbidden" >&2
    exit 1
  fi
done

printf '%s\n' 'PASS: Task-004 host-network stub regressions'
