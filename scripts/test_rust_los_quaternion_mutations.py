#!/usr/bin/env python3
"""Compile and runtime-kill focused safe Rust LOS quaternion mutations.

Requires an already built native contract-test tree. Mutations run in a private
copy of the Rust workspace; the checkout and vendor code are never modified.
Compile failures are invalid, not kills. Each test must execute and fail a Rust
assertion. The original source is restored after every experiment, then the
restored unit and integration baselines must pass. No downloads are required.
"""

import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SOURCE = "ams-mel/src/ir_los_quaternion.rs"
UNIT = "ir_los_quaternion::tests::"
# name, unique old source, replacement, exact runtime test
MUTATIONS = [
    ("swap_x_w", "x: raw.q_xyzw.x,\n            y: raw.q_xyzw.y,\n"
     "            z: raw.q_xyzw.z,\n            w: raw.q_xyzw.w,",
     "x: raw.q_xyzw.w,\n            y: raw.q_xyzw.y,\n"
     "            z: raw.q_xyzw.z,\n            w: raw.q_xyzw.x,", "fidelity"),
    ("zero_z", "z: raw.q_xyzw.z,", "z: 0.0,", "fidelity"),
    ("swap_forward_right", "forward: raw.pqr_rps.forward,\n            right: raw.pqr_rps.right,",
     "forward: raw.pqr_rps.right,\n            right: raw.pqr_rps.forward,", "fidelity"),
    ("time_sign", "system_time_ns: raw.system_time_ns,",
     "system_time_ns: raw.system_time_ns & i64::MAX,", "fidelity"),
    ("time_width", "system_time_ns: raw.system_time_ns,",
     "system_time_ns: i64::from(raw.system_time_ns as i32),", "fidelity"),
    ("normalize", "    Ok(LineOfSightQuaternion {",
     "    let mut raw = *raw;\n"
     "    if raw.q_xyzw.x == 3.0 {\n"
     "        let norm = (raw.q_xyzw.x.powi(2) + raw.q_xyzw.y.powi(2) + "
     "raw.q_xyzw.z.powi(2) + raw.q_xyzw.w.powi(2)).sqrt();\n"
     "        raw.q_xyzw.x /= norm; raw.q_xyzw.y /= norm;\n"
     "        raw.q_xyzw.z /= norm; raw.q_xyzw.w /= norm;\n"
     "    }\n    Ok(LineOfSightQuaternion {",
     "fidelity"),
    ("fabricate_registration", "let registration = command_return(raw_registration).ok();",
     "let registration = Some(command_return(raw_registration).unwrap_or(CommandReturn::Fail));",
     UNIT + "failed_open_preserves_status_and_registration_and_drops_unexpected_owner"),
    ("omit_event_cleanup", "close_owned(&mut self.raw, self.ops.close)?;",
     "self.raw = ptr::null_mut();",
     UNIT + "event_guard_cleanup_on_view_conversion_close_failure_and_unwind"),
    ("retry_uncertain_release", "unsafe { close(&mut released, d, c, r) }",
     "unsafe { let status = close(&mut released, d, c, r); "
     "if status != sys::AMS_MEL_OK { *raw = released; } status }",
     UNIT + "uncertain_stream_close_is_attempted_exactly_once"),
]


def main():
    environment = dict(os.environ)
    environment["AMS_MEL_NATIVE_LIB_DIR"] = str(
        Path(environment.get("AMS_MEL_NATIVE_LIB_DIR", ROOT / "native/build-tests/lib")).resolve())
    environment["AMS_MEL_TEST_PROVIDER_DIR"] = str(
        Path(environment.get("AMS_MEL_TEST_PROVIDER_DIR",
                             ROOT / "native/build-tests/test-providers")).resolve())
    if not (Path(environment["AMS_MEL_NATIVE_LIB_DIR"]) / "libams_mel_c.so").is_file():
        raise SystemExit("Build the native contract-test tree first: make test-native")
    original = (ROOT / "rust" / SOURCE).read_bytes()
    digest = hashlib.sha256(original).hexdigest()
    results = []
    with tempfile.TemporaryDirectory(prefix="ams-rust-losq-mutations-") as temporary:
        workspace = Path(temporary) / "rust"
        shutil.copytree(ROOT / "rust", workspace,
                        ignore=shutil.ignore_patterns("target"))
        # Never share Cargo artifacts with other qualification commands.
        environment["CARGO_TARGET_DIR"] = str(Path(temporary) / "target")
        source = workspace / SOURCE

        def run(arguments):
            process = subprocess.run(
                ["cargo", "test", "--offline", "--manifest-path",
                 str(workspace / "Cargo.toml"), "-p", "ams-mel", *arguments],
                env=environment, text=True, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, timeout=120, check=False)
            print(process.stdout, end="", flush=True)
            return process

        # A valid baseline is mandatory before any mutations.
        for selection in [["--lib", "ir_los_quaternion"],
                          ["--test", "ir_los_quaternion"]]:
            if run(selection).returncode:
                raise SystemExit("Unmodified baseline failed; no mutation claim")
        for name, before, after, test in MUTATIONS:
            text = original.decode()
            if text.count(before) != 1:
                raise SystemExit(f"{name}: mutation anchor is not unique")
            result = dict(name=name, applied=True, compiled=False,
                          executed=False, killed=False, invalid=False, surviving=False)
            try:
                source.write_text(text.replace(before, after))
                selection = ["--test", "ir_los_quaternion"] if test == "fidelity" else ["--lib"]
                result["compiled"] = run([*selection, test, "--no-run"]).returncode == 0
                if result["compiled"]:
                    execution = run([*selection, test, "--", "--exact", "--nocapture"])
                    result["executed"] = "running 1 test" in execution.stdout
                    result["killed"] = (result["executed"] and execution.returncode != 0
                                        and "test result: FAILED" in execution.stdout
                                        and "assertion `left == right` failed" in execution.stdout)
                    result["surviving"] = result["executed"] and execution.returncode == 0
                result["invalid"] = not result["compiled"] or not result["executed"]
            finally:
                source.write_bytes(original)
            results.append(result)
            print(json.dumps(result), flush=True)
        for selection in [["--lib", "ir_los_quaternion"],
                          ["--test", "ir_los_quaternion"]]:
            if run(selection).returncode:
                raise SystemExit("Restored baseline failed")
        if source.read_bytes() != original:
            raise SystemExit("Temporary source was not restored")
    if hashlib.sha256((ROOT / "rust" / SOURCE).read_bytes()).hexdigest() != digest:
        raise SystemExit("Checkout source changed during qualification")
    totals = {key: sum(result[key] for result in results)
              for key in ("applied", "compiled", "executed", "killed", "invalid", "surviving")}
    print(json.dumps(dict(source_sha256=digest, totals=totals, results=results), indent=2))
    if len(results) != len(MUTATIONS) or not all(result["killed"] for result in results):
        raise SystemExit("Mutation qualification incomplete")


if __name__ == "__main__":
    main()
