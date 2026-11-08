#!/usr/bin/env python3
"""Mock-only C2 SystemTrackDataResponse mutations in an independent temporary tree.

The checkout (including vendor headers) is never modified. Compile failures are
not accepted as mutation detection: each mutant must build and then fail the
release-active compiled C11 contract with the named assertion. Restore and
rerun the baseline afterwards.

Mutants cover loss of an input field (shared conversion), an altered response
state (C2 completion), and incorrect C2-to-Track dispatch/accounting.
"""

from pathlib import Path
import hashlib
import os
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
TARGET = "test_ir_c2_system_response"


def run(command):
    subprocess.run(command, check=True, env={k: v for k, v in os.environ.items()
                                          if k != "LD_RUN_PATH"})


# (name, relative source, before, after, expected diagnostic fragment)
MUTATIONS = [
    ("trackId lost", "src/system_track_data.cpp",
     "value.setTrackId(input.track_id);", "value.setTrackId(0U);",
     "f[3] == v->track_id"),
    ("AzElError pair replaced by inertialAzEl", "src/system_track_data.cpp",
     "value.setAzElError(convert_az_el(input.az_el_error));",
     "value.setAzElError(convert_az_el(input.inertial_az_el));",
     "f[12] == bits(v->az_el_error.azimuth_rad)"),
    ("rangeValid forced true", "src/system_track_data.cpp",
     "value.setRangeValid(range_valid);", "value.setRangeValid(true);",
     "f[9] == v->range_valid"),
    ("response state altered", "src/ir_c2_system_response.cpp",
     "        result.error_code = AMS_MEL_ERROR_NONE;\n        return CompletionKind::Success;",
     "        result.error_code = AMS_MEL_ERROR_NONE;\n"
     "        if (result.status.state == AMS_MEL_IR_COMMAND_REJECTED)\n"
     "            result.status.state = AMS_MEL_IR_COMMAND_ACCEPTED;\n"
     "        return CompletionKind::Success;",
     "result.status.state == state"),
    ("ErrorOr rejection reported as success", "src/ir_c2_system_response.cpp",
     "    message = description;\n    return CompletionKind::Rejected;",
     "    message = description;\n    return CompletionKind::Success;",
     "expect_outcome(\"no-c2-system-response\""),
    ("C2 provider send not dispatched", "src/ir_c2.cpp",
     "        return std::static_pointer_cast<irmel::C2Channel>(channel)->send(std::move(response));",
     "        (void)channel; (void)response;\n"
     "        return ams::iface::mel::RequestFor<irmel::CommandStatus>{};",
     "submit(o, v, &request, NULL, 0) == AMS_MEL_OK"),
    ("C2 request accounting never released", "src/ir_c2.cpp",
     "        if (!claim_c2_submission(state, SubmissionRequirement::Enabled, c2)) return false;\n"
     "        claim = CommonRequestClaim{state, finish_c2_request, \"deferred C2 cleanup failed\"};\n"
     "        channel = std::move(c2);\n"
     "        return true;\n"
     "    },\n"
     "    [](const std::shared_ptr<void>& channel, irmel::SystemTrackDataResponse response) {",
     "        if (!claim_c2_submission(state, SubmissionRequirement::Enabled, c2)) return false;\n"
     "        (void)claim;\n"
     "        channel = std::move(c2);\n"
     "        return true;\n"
     "    },\n"
     "    [](const std::shared_ptr<void>& channel, irmel::SystemTrackDataResponse response) {",
     "C2 requests "),
    ("C2 Enabled requirement weakened", "src/ir_c2.cpp",
     "        if (!claim_c2_submission(state, SubmissionRequirement::Enabled, c2)) return false;\n"
     "        claim = CommonRequestClaim{state, finish_c2_request, \"deferred C2 cleanup failed\"};\n"
     "        channel = std::move(c2);\n"
     "        return true;\n"
     "    },\n"
     "    [](const std::shared_ptr<void>& channel, irmel::SystemTrackDataResponse response) {",
     "        if (!claim_c2_submission(state, SubmissionRequirement::AttachedOrEnabled, c2)) return false;\n"
     "        claim = CommonRequestClaim{state, finish_c2_request, \"deferred C2 cleanup failed\"};\n"
     "        channel = std::move(c2);\n"
     "        return true;\n"
     "    },\n"
     "    [](const std::shared_ptr<void>& channel, irmel::SystemTrackDataResponse response) {",
     "submit(&o, &v, &request, d, sizeof d) == AMS_MEL_PROVIDER_FAILED"),
    ("C2 completion attributed to Track family", "src/internal/ir_c2_system_response.hpp",
     "AMS_MEL_PROBE_OWNER(C2SystemResponse)", "AMS_MEL_PROBE_OWNER(Track)",
     "ams_mel_test_completion_owner(C2_RESPONSE_PROBE, base + 1U, &observed)"),
]


def main():
    digests = {name: hashlib.sha256((ROOT / "native" / name).read_bytes()).hexdigest()
               for name in {m[1] for m in MUTATIONS}}
    with tempfile.TemporaryDirectory(prefix="ams-mel-c2-system-response-mutations-") as temporary:
        root = Path(temporary)
        source = root / "native"
        shutil.copytree(ROOT / "native", source,
                        ignore=shutil.ignore_patterns("build*", "alire", ".git"))
        build = root / "build"
        run(["cmake", "-S", str(source), "-B", str(build),
             "-DAMS_MEL_BUILD_TESTS=ON", "-DCMAKE_BUILD_TYPE=Release"])

        def compile_contract():
            run(["cmake", "--build", str(build), "--parallel", "4", "--target", TARGET])

        def contract():
            return subprocess.run([str(build / TARGET)], text=True,
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

        compile_contract()
        baseline = contract()
        assert baseline.returncode == 0, baseline.stdout
        for name, relative, before, after, diagnostic in MUTATIONS:
            target = source / relative
            pristine = target.read_text()
            assert pristine.count(before) == 1, (name, before)
            try:
                target.write_text(pristine.replace(before, after, 1))
                compile_contract()
                result = contract()
                assert result.returncode != 0 and diagnostic in result.stdout, (name, result.stdout)
                print(f"PASS: mutation detected: {name}: {diagnostic}", flush=True)
            finally:
                target.write_text(pristine)
        compile_contract()
        restored = contract()
        assert restored.returncode == 0, restored.stdout
        print("PASS: restored baseline contract", flush=True)
    for name, digest in digests.items():
        assert hashlib.sha256((ROOT / "native" / name).read_bytes()).hexdigest() == digest
    print("PASS: checkout source unchanged; isolated source/build tree removed", flush=True)


if __name__ == "__main__":
    main()
