#!/bin/sh
set -eu

expected_commit=b1015728f904c799fa0c07489fce48e78f67845f
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
mode=${1:-all}
control_port=${AMS_MEL_SQUALL_CONTROL_PORT:-21203}
couloir_metrics_port=${AMS_MEL_SQUALL_COULOIR_METRICS_PORT:-21318}
optical_health_port=${AMS_MEL_SQUALL_OPTICAL_HEALTH_PORT:-21315}
optical_metrics_port=${AMS_MEL_SQUALL_OPTICAL_METRICS_PORT:-21316}
build_dir=${AMS_MEL_SQUALL_BUILD_DIR:-"$root/build/squall"}
case "$build_dir" in
  /*) ;;
  *) build_dir="$root/$build_dir" ;;
esac
project="ams-mel-task004-$$"
project_provider="$project-provider"
provider_image="localhost/${project_provider}:latest"
optical_image="localhost/${project}-optical:latest"
couloir_image="localhost/${project}-couloir:latest"
started=0
provider_container=
provider_image_present=0
runtime_images_present=0
temp_dir="$build_dir/tmp/$project"
profile="$temp_dir/squall-ir-profile.json"
couloir_config="$temp_dir/couloir.toml"
runtime_override="$temp_dir/runtime.compose.override.yaml"
runtime_compose="$temp_dir/runtime.compose.yaml"

fail() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }
need() { command -v "$1" >/dev/null 2>&1 || fail "$1 is required"; }
validate_port() {
  port_name=$1
  port_value=$2

  case "$port_value" in
    ''|*[!0-9]*) fail "$port_name must be an integer in 1..65535" ;;
  esac
  if test "$port_value" -lt 1 || test "$port_value" -gt 65535; then
    fail "$port_name must be an integer in 1..65535"
  fi
}

require_distinct_ports() {
  first_name=$1
  first_value=$2
  second_name=$3
  second_value=$4
  test "$first_value" != "$second_value" || \
    fail "$first_name and $second_name must use distinct host ports (both are $first_value)"
}

print_runtime_port_plan() {
  printf '%s\n' \
    'Task-004 runtime ports:' \
    "  Couloir control:  $control_port" \
    "  Couloir metrics:  $couloir_metrics_port" \
    "  Optical health:   $optical_health_port" \
    "  Optical metrics:  $optical_metrics_port"
}

write_runtime_configuration() {
  cat >"$couloir_config" <<EOF
tcp_addr = "127.0.0.1:$control_port"
metrics_port = $couloir_metrics_port

[routes]
"/squall.rf.control.v1.SquallRfControl/" = "/sockets/squall-rf-control.sock"
"/squall.optical.control.v1.SquallIrControl/" = "/sockets/squall-optical-control.sock"
EOF
  cat >"$runtime_override" <<EOF
services:
  couloir:
    image: $couloir_image
    network_mode: host
    command: --config /task004/couloir.toml
    volumes:
      - backend-sockets:/sockets
      - $couloir_config:/task004/couloir.toml:ro,Z
  squall-optical:
    image: $optical_image
    network_mode: host
    command: --config /task004/optical.toml
    environment:
      SQUALL_OPTICAL_HEALTH_ADDRESS: "127.0.0.1"
      SQUALL_OPTICAL_HEALTH_PORT: "$optical_health_port"
      SQUALL_OPTICAL_METRICS_PORT: "$optical_metrics_port"
      # Pinned Squall defaults this to 21600. The checkerboard sink has no
      # static destinations, so Squall's static-destination override loop
      # validates this port without replacing the runtime-fillable MEL slot.
      SQUALL_IR_DATA_PORT: "21600"
    volumes:
      - backend-sockets:/sockets
      - $SQUALL_SOURCE_DIR/tests/mel-boundary-e2e/config/optical.toml:/task004/optical.toml:ro,Z
EOF
}

case "$mode" in all|c|ada|rust|python) ;; *) fail "mode must be all, c, ada, rust, or python" ;; esac
validate_port AMS_MEL_SQUALL_CONTROL_PORT "$control_port"
validate_port AMS_MEL_SQUALL_COULOIR_METRICS_PORT "$couloir_metrics_port"
validate_port AMS_MEL_SQUALL_OPTICAL_HEALTH_PORT "$optical_health_port"
validate_port AMS_MEL_SQUALL_OPTICAL_METRICS_PORT "$optical_metrics_port"
require_distinct_ports AMS_MEL_SQUALL_CONTROL_PORT "$control_port" \
  AMS_MEL_SQUALL_COULOIR_METRICS_PORT "$couloir_metrics_port"
require_distinct_ports AMS_MEL_SQUALL_CONTROL_PORT "$control_port" \
  AMS_MEL_SQUALL_OPTICAL_HEALTH_PORT "$optical_health_port"
require_distinct_ports AMS_MEL_SQUALL_CONTROL_PORT "$control_port" \
  AMS_MEL_SQUALL_OPTICAL_METRICS_PORT "$optical_metrics_port"
require_distinct_ports AMS_MEL_SQUALL_COULOIR_METRICS_PORT "$couloir_metrics_port" \
  AMS_MEL_SQUALL_OPTICAL_HEALTH_PORT "$optical_health_port"
require_distinct_ports AMS_MEL_SQUALL_COULOIR_METRICS_PORT "$couloir_metrics_port" \
  AMS_MEL_SQUALL_OPTICAL_METRICS_PORT "$optical_metrics_port"
require_distinct_ports AMS_MEL_SQUALL_OPTICAL_HEALTH_PORT "$optical_health_port" \
  AMS_MEL_SQUALL_OPTICAL_METRICS_PORT "$optical_metrics_port"

# Stub-only path: exercise the real defaults, validation, collision checks, and
# renderer without inspecting a checkout or invoking a container runtime.
if test "${AMS_MEL_SQUALL_PREFLIGHT_ONLY:-0}" = 1; then
  test -n "${SQUALL_SOURCE_DIR:-}" || SQUALL_SOURCE_DIR=/task004-squall
  mkdir -p "$temp_dir"
  write_runtime_configuration
  print_runtime_port_plan
  cat "$runtime_override"
  rm -f "$couloir_config" "$runtime_override"
  rmdir "$temp_dir" "$build_dir/tmp" "$build_dir" >/dev/null 2>&1 || true
  exit 0
fi
test -n "${SQUALL_SOURCE_DIR:-}" || fail "SQUALL_SOURCE_DIR must name the pinned Squall checkout"
BUILD_COMPOSE_DIR="$SQUALL_SOURCE_DIR/tests/mel-boundary-e2e"
BUILD_COMPOSE_FILE="$BUILD_COMPOSE_DIR/compose.yaml"
RUNTIME_COMPOSE_DIR="$SQUALL_SOURCE_DIR"
RUNTIME_COMPOSE_FILE="$SQUALL_SOURCE_DIR/compose.yaml"
RUNTIME_BUILD_COMPOSE_FILE="$SQUALL_SOURCE_DIR/compose.build.yaml"
test -f "$BUILD_COMPOSE_FILE" || fail "Squall MEL boundary compose file is missing: $BUILD_COMPOSE_FILE"
verify_checkout() {
  "$root/integration/squall/verify-checkout.sh" "$1" "$2" "$3"
}
verify_checkout "$SQUALL_SOURCE_DIR" \
  open-arsenal/ams-gra-hello-world-sk-sensors-squall "$expected_commit"
actual_commit=$expected_commit
test -f "$SQUALL_SOURCE_DIR/compose.yaml" || fail "Squall compose.yaml is missing"
test -f "$RUNTIME_BUILD_COMPOSE_FILE" || fail "Squall compose.build.yaml is missing"
test -f "$SQUALL_SOURCE_DIR/config/squall-ir-mel-profile.json" || fail "Squall IR MEL profile is missing"
verify_checkout "$SQUALL_SOURCE_DIR/ams-interfaces/common-mel" \
  open-arsenal/ams-gra-hello-world-sk-interfaces-common-mel \
  f6908437d8fd2f7fb69896f9eb9cfd272d10c439
verify_checkout "$SQUALL_SOURCE_DIR/ams-interfaces/ir-mel" \
  open-arsenal/ams-gra-hello-world-sk-interfaces-ir-mel \
  8d9224519f12b44e0b28815755c56a32a28d24a0
verify_checkout "$SQUALL_SOURCE_DIR/ams-interfaces/rf-mel" \
  open-arsenal/ams-gra-hello-world-sk-interfaces-rf-mel \
  762ce84c5555dd0f3ea66f36b321fecf8839b89f
verify_checkout "$SQUALL_SOURCE_DIR/ams-interfaces/ir-mel/ams-math" \
  open-arsenal/ams-gra-hello-world-sk-libraries-ams-math \
  00be45190f0e47d268cece8b8c2f8fb58b5418d2
verify_checkout "$SQUALL_SOURCE_DIR/ams-interfaces/rf-mel/ams-math" \
  open-arsenal/ams-gra-hello-world-sk-libraries-ams-math \
  00be45190f0e47d268cece8b8c2f8fb58b5418d2
verify_checkout "$SQUALL_SOURCE_DIR/ams-interfaces/rf-mel/ams-vita" \
  open-arsenal/ams-gra-hello-world-sk-libraries-ams-vita \
  8e12a4cd7ac8ea8776d40b9d0b22fc4a22adaad8

need git; need cmake; need cc; need file; need readelf; need python3; need awk
if command -v podman >/dev/null 2>&1; then
  runtime=podman
elif command -v docker >/dev/null 2>&1; then
  runtime=docker
else
  fail "Podman or Docker is required"
fi
if command -v podman-compose >/dev/null 2>&1 && test "$runtime" = podman; then
  compose_kind=standalone
  compose_description=podman-compose
elif "$runtime" compose version >/dev/null 2>&1; then
  compose_kind=plugin
  compose_description="$runtime compose"
else
  fail "podman-compose or a Docker/Podman compose plugin is required"
fi

run_compose() {
  compose_project=$1
  shift
  (
    cd "$BUILD_COMPOSE_DIR"
    if test "$compose_kind" = standalone; then
      podman-compose -p "$compose_project" -f "$BUILD_COMPOSE_FILE" "$@"
    else
      "$runtime" compose -p "$compose_project" -f "$BUILD_COMPOSE_FILE" "$@"
    fi
  )
}

render_runtime_compose() {
  compose_project=$1
  (
    cd "$RUNTIME_COMPOSE_DIR"
    if test "$compose_kind" = standalone; then
      podman-compose -p "$compose_project" -f "$RUNTIME_COMPOSE_FILE" \
        -f "$RUNTIME_BUILD_COMPOSE_FILE" -f "$runtime_override" config
    else
      "$runtime" compose -p "$compose_project" -f "$RUNTIME_COMPOSE_FILE" \
        -f "$RUNTIME_BUILD_COMPOSE_FILE" -f "$runtime_override" config
    fi
  ) >"$runtime_compose"
}

run_runtime_compose() {
  compose_project=$1
  shift
  (
    cd "$RUNTIME_COMPOSE_DIR"
    if test "$compose_kind" = standalone; then
      podman-compose -p "$compose_project" -f "$runtime_compose" "$@"
    else
      "$runtime" compose -p "$compose_project" -f "$runtime_compose" "$@"
    fi
  )
}

service_container_id() {
  lookup_project=$1
  lookup_service=$2
  container_ids=

  if test "$runtime" = podman; then
    container_ids=$("$runtime" ps -a \
      --filter "label=io.podman.compose.project=$lookup_project" \
      --filter "label=io.podman.compose.service=$lookup_service" \
      --format '{{.ID}}') || fail "could not query containers for Compose service $lookup_service"
    if test -z "$container_ids"; then
      container_ids=$("$runtime" ps -a \
        --filter "label=com.docker.compose.project=$lookup_project" \
        --filter "label=com.docker.compose.service=$lookup_service" \
        --format '{{.ID}}') || fail "could not query containers for Compose service $lookup_service"
    fi
  else
    container_ids=$("$runtime" ps -a \
      --filter "label=com.docker.compose.project=$lookup_project" \
      --filter "label=com.docker.compose.service=$lookup_service" \
      --format '{{.ID}}') || fail "could not query containers for Compose service $lookup_service"
  fi

  container_ids=$(printf '%s\n' "$container_ids" | awk 'NF && !seen[$0]++')
  container_count=$(printf '%s\n' "$container_ids" | awk 'NF { count++ } END { print count + 0 }')
  case "$container_count" in
    0) fail "no container found for Compose service $lookup_service" ;;
    1) printf '%s\n' "$container_ids" ;;
    *) fail "multiple containers found for Compose service $lookup_service" ;;
  esac
}

print_service_status() {
  diagnostic_service=$1
  if diagnostic_container=$(service_container_id "$project" "$diagnostic_service" 2>&1); then
    printf '%s\n' "[$diagnostic_service]"
    "$runtime" inspect \
      --format '{{.Name}} status={{.State.Status}} running={{.State.Running}} exit={{.State.ExitCode}}' \
      "$diagnostic_container" || \
      printf 'WARNING: could not inspect Compose service %s container %s\n' \
        "$diagnostic_service" "$diagnostic_container" >&2
  else
    printf 'WARNING: %s\n' "$diagnostic_container" >&2
  fi
}

print_service_logs() {
  diagnostic_service=$1
  if diagnostic_container=$(service_container_id "$project" "$diagnostic_service" 2>&1); then
    printf '%s\n' "--- $diagnostic_service logs ---" >&2
    "$runtime" logs "$diagnostic_container" >&2 || \
      printf 'WARNING: could not read Compose service %s logs from container %s\n' \
        "$diagnostic_service" "$diagnostic_container" >&2
  else
    printf 'WARNING: %s\n' "$diagnostic_container" >&2
  fi
}

assert_host_network() {
  network_service=$1
  network_container=$2
  network_mode=$("$runtime" inspect --format '{{.HostConfig.NetworkMode}}' "$network_container") || \
    fail "could not inspect network mode for $network_service"
  test "$network_mode" = host || \
    fail "$network_service must use host networking, found: $network_mode"
}

print_udp_sockets() {
  if command -v ss >/dev/null 2>&1; then
    printf '%s\n' '--- host UDP sockets (ss -lun) ---' >&2
    ss -lun >&2 || printf '%s\n' 'WARNING: could not inspect host UDP sockets' >&2
  fi
}

# Upstream's default compose pins these names. Never replace a user's stack.
for name in couloir squall-optical; do
  if "$runtime" container inspect "$name" >/dev/null 2>&1; then
    fail "container '$name' already exists; refusing to stop or replace a user Squall stack"
  fi
done

cleanup() {
  status=$?
  trap - EXIT INT TERM
  if test -n "$provider_container"; then
    "$runtime" rm -f "$provider_container" >/dev/null 2>&1 || true
  fi
  if test "$provider_image_present" = 1; then
    "$runtime" image rm "$provider_image" >/dev/null 2>&1 || true
  fi
  if test "$started" = 1 && test "$status" -ne 0; then
    printf '%s\n' '--- Squall runtime status ---' >&2
    print_service_status squall-optical >&2 || true
    print_service_status couloir >&2 || true
    printf '%s\n' '--- Squall runtime logs ---' >&2
    print_service_logs squall-optical || true
    print_service_logs couloir || true
    if test -f "$profile"; then
      printf '%s\n' '--- generated IR MEL profile ---' >&2
      cat "$profile" >&2 || true
    fi
  fi
  if test "$started" = 1 && test "${AMS_MEL_KEEP_SQUALL:-0}" != 1; then
    # Only this invocation could have created these names due to the preflight.
    run_runtime_compose "$project" down >/dev/null 2>&1 || true
  elif test "$started" = 1; then
    printf 'Leaving Task-004 Squall stack running (project %s).\n' "$project" >&2
  fi
  if test "$runtime_images_present" = 1 && \
    { test "$started" != 1 || test "${AMS_MEL_KEEP_SQUALL:-0}" != 1; }
  then
    "$runtime" image rm "$optical_image" "$couloir_image" >/dev/null 2>&1 || true
  fi
  rm -f "$profile" "$couloir_config" "$runtime_override" "$runtime_compose"
  rmdir "$temp_dir" "$build_dir/tmp" >/dev/null 2>&1 || true
  exit "$status"
}
trap cleanup EXIT INT TERM

python3 - "$control_port" "$couloir_metrics_port" "$optical_health_port" "$optical_metrics_port" <<'PY'
import socket
import sys

for port in map(int, sys.argv[1:]):
  with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
    try:
        listener.bind(("127.0.0.1", port))
    except OSError:
        raise SystemExit(
            f"ERROR: host port {port} is occupied; select a free "
            "Task-004 runtime port"
        )
PY

mkdir -p "$build_dir/bin" "$build_dir/ada-obj" "$build_dir/provider" "$temp_dir"
export ARTIFACTS_DIR="$build_dir"
export SQUALL_MEL_DATA_CONSUMER_IMAGE="$provider_image"
write_runtime_configuration
render_runtime_compose "$project"
print_runtime_port_plan
printf '%s\n' "Squall source: $SQUALL_SOURCE_DIR" "Squall revision: $actual_commit" \
  "Container runtime: $($runtime --version 2>/dev/null | head -n 1)" \
  "Compose command: $compose_description" \
  "Build compose file: $BUILD_COMPOSE_FILE" \
  "Runtime compose file: $RUNTIME_COMPOSE_FILE" \
  "Runtime build compose file: $RUNTIME_BUILD_COMPOSE_FILE" \
  "Runtime override: $runtime_override" \
  "Rendered runtime compose file: $runtime_compose" \
  "Generated Couloir configuration: $couloir_config"

# The upstream E2E service is the supported image target containing the real MEL
# library. Build it but do not run its application client.
provider_image_present=1
run_compose "$project_provider" build squall-ir-data-consumer
"$runtime" image inspect "$provider_image" >/dev/null 2>&1 || \
  fail "could not identify built Squall MEL consumer image: $provider_image"
provider_container=$($runtime create "$provider_image")
provider="$build_dir/provider/libsquall_ir_mel.so"
"$runtime" cp "$provider_container:/usr/lib64/libsquall_ir_mel.so" "$provider"
"$runtime" rm "$provider_container" >/dev/null
provider_container=
"$runtime" image rm "$provider_image" >/dev/null
provider_image_present=0
test -s "$provider" || fail "extracted Squall IR MEL library is empty"

printf '%s\n' "Squall provider: $provider" 'Provider compiler comments:'
readelf -p .comment "$provider" 2>/dev/null | sed -n '/GCC:/p' || true
printf '%s\n' 'Provider runtime dependencies:'
readelf -d "$provider" | sed -n '/NEEDED/p'

# Build the existing production façade; integration clients are separate and
# are never installed or added to default CTest/Alire tests.
cmake -S "$root/native" -B "$root/native/build" -DAMS_MEL_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build "$root/native/build" --parallel 2
if test "$mode" = all || test "$mode" = c; then
  cc -std=c11 -pedantic-errors -Wall -Wextra -Werror \
    -I"$root/native/include" "$root/integration/squall/squall_ir_c_smoke.c" \
    -L"$root/native/build/lib" -Wl,-rpath,"$root/native/build/lib" -lams_mel_c \
    -o "$build_dir/bin/squall_ir_c_smoke"
fi

if test "$mode" = all || test "$mode" = ada; then
  need alr
  AMS_MEL_SQUALL_ADA_BUILD_DIR="$build_dir" \
  GPR_PROJECT_PATH="$root/native:$root/ada${GPR_PROJECT_PATH:+:$GPR_PROJECT_PATH}" \
    alr -C "$root/ada" exec -- gprbuild -f -p -P "$root/integration/squall/ams_mel_squall_ir.gpr"
fi
if test "$mode" = all || test "$mode" = rust; then
  need cargo
  CARGO_TARGET_DIR="$build_dir/rust-target" \
  AMS_MEL_NATIVE_LIB_DIR="$root/native/build/lib" \
    cargo build --manifest-path "$root/integration/squall/rust/Cargo.toml" \
      --release --locked --offline
fi
if test "$mode" = all || test "$mode" = python; then
  mkdir -p "$build_dir/python-cache"
  PYTHONPYCACHEPREFIX="$build_dir/python-cache" \
    python3 -W error -m py_compile \
      "$root/integration/squall/squall_ir_python.py"
fi

check_client_elf() {
  executable=$1
  client_name=$2
  file "$executable" | grep -Eq 'ELF .* executable' || \
    fail "$client_name Squall integration client is not an ELF executable: $executable"
  readelf -h "$executable" >/dev/null || \
    fail "could not read $client_name Squall integration client ELF header: $executable"
  dynamic_section=$(readelf -d "$executable") || \
    fail "could not read $client_name Squall integration client dependencies: $executable"
  if printf '%s\n' "$dynamic_section" | grep -q 'libsquall_ir_mel'; then
    fail "$executable directly links Squall"
  fi
  if printf '%s\n' "$dynamic_section" | grep -q 'mock.*provider'; then
    fail "$executable directly links a mock provider"
  fi
}
if test "$mode" = all || test "$mode" = c; then
  check_client_elf "$build_dir/bin/squall_ir_c_smoke" C
fi
if test "$mode" = all || test "$mode" = ada; then
  check_client_elf "$build_dir/bin/ams_mel_squall_ir" Ada
fi
if test "$mode" = all || test "$mode" = rust; then
  rust_client="$build_dir/rust-target/release/ams-mel-squall-ir"
  check_client_elf "$rust_client" Rust
  readelf -d "$rust_client" | grep -q 'libams_mel_c' || \
    fail "$rust_client does not link through libams_mel_c"
fi

# Start only the hardware-free optical MFA and Couloir from upstream's
# host-network deployment. The override supplies Task-004-owned configuration.
runtime_images_present=1
run_runtime_compose "$project" build squall-optical couloir
"$runtime" image inspect "$optical_image" "$couloir_image" >/dev/null 2>&1 || \
  fail "could not identify built Task-004 runtime images"
started=1
run_runtime_compose "$project" up -d --no-build squall-optical couloir
optical_container=$(service_container_id "$project" squall-optical)
couloir_container=$(service_container_id "$project" couloir)
assert_host_network squall-optical "$optical_container"
assert_host_network couloir "$couloir_container"
python3 - "$profile" "$project" "$control_port" <<'PY'
import json, sys
with open(sys.argv[1], "w", encoding="utf-8") as output:
    json.dump({"log_level": "info", "control_address": f"127.0.0.1:{sys.argv[3]}",
               "data_host": "127.0.0.1", "client_id": sys.argv[2]}, output)
    output.write("\n")
PY
printf '%s\n' "IR MEL profile: $profile" \
  "  control_address: 127.0.0.1:$control_port" \
  "  data_host: 127.0.0.1" \
  "  client_id: $project"
python3 - "$control_port" "$couloir_metrics_port" "$optical_health_port" <<'PY'
import socket
import sys
import time
import urllib.error
import urllib.request

control_port = int(sys.argv[1])
metrics_port = int(sys.argv[2])
optical_health_port = int(sys.argv[3])
optical_ready_url = f"http://127.0.0.1:{optical_health_port}/ready"
deadline = time.monotonic() + 60.0
optical_ready = False
metrics_ready = False
control_ready = False
while time.monotonic() < deadline:
    if not optical_ready:
        try:
            with urllib.request.urlopen(optical_ready_url, timeout=1.0) as response:
                optical_ready = response.status == 200
        except (OSError, urllib.error.URLError):
            pass
    if not metrics_ready:
        try:
            with socket.create_connection(("127.0.0.1", metrics_port), timeout=1.0):
                metrics_ready = True
        except OSError:
            pass
    if not control_ready:
        try:
            with socket.create_connection(("127.0.0.1", control_port), timeout=1.0):
                control_ready = True
        except OSError:
            pass
    if optical_ready and metrics_ready and control_ready:
        break
    time.sleep(0.5)
if not optical_ready:
    raise SystemExit(f"ERROR: timed out waiting for Squall optical readiness at {optical_ready_url}")
if not metrics_ready:
    raise SystemExit(f"ERROR: timed out waiting for Couloir metrics at 127.0.0.1:{metrics_port}")
if not control_ready:
    raise SystemExit(
        f"ERROR: timed out waiting for Couloir MEL control at "
        f"127.0.0.1:{control_port}"
    )
PY

repeat=${AMS_MEL_SQUALL_REPEAT:-1}
case "$repeat" in ''|*[!0-9]*|0) fail "AMS_MEL_SQUALL_REPEAT must be a positive integer" ;; esac
frames=${AMS_MEL_SQUALL_FRAMES:-3}
timeout=${AMS_MEL_SQUALL_FRAME_TIMEOUT_MS:-10000}
iteration=1
while test "$iteration" -le "$repeat"; do
  printf '\nTask-004 integration iteration %s/%s\n' "$iteration" "$repeat"
  print_udp_sockets
  if test "$mode" = all || test "$mode" = c; then
    if ! LD_LIBRARY_PATH="$root/native/build/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
      "$build_dir/bin/squall_ir_c_smoke" "$provider" "$profile" "$frames" "$timeout"
    then
      print_udp_sockets
      fail "C client failed; see IR counters (if emitted), runtime logs, and profile above"
    fi
  fi
  if test "$mode" = all || test "$mode" = ada; then
    if ! LD_LIBRARY_PATH="$root/native/build/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
      "$build_dir/bin/ams_mel_squall_ir" "$provider" "$profile" "$frames" "$timeout"
    then
      print_udp_sockets
      fail "Ada client failed; see runtime logs and profile above"
    fi
  fi
  if test "$mode" = all || test "$mode" = rust; then
    if ! LD_LIBRARY_PATH="$root/native/build/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
      "$rust_client" "$provider" "$profile" "$frames" "$timeout"
    then
      print_udp_sockets
      fail "Rust client failed; see runtime logs and profile above"
    fi
  fi
  if test "$mode" = all || test "$mode" = python; then
    if ! PYTHONPATH="$root/python" \
      AMS_MEL_NATIVE_LIB="$root/native/build/lib/libams_mel_c.so.0" \
      LD_LIBRARY_PATH="$root/native/build/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
      python3 -W error "$root/integration/squall/squall_ir_python.py" \
        "$provider" "$profile" "$frames" "$timeout"
    then
      print_udp_sockets
      fail "Python client failed; see runtime logs and profile above"
    fi
  fi
  iteration=$((iteration + 1))
done

printf '\nPASS: Squall IR MEL integration (%s iteration(s), mode=%s)\n' "$repeat" "$mode"
