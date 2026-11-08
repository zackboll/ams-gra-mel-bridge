#!/usr/bin/env python3
"""Mock-only Image LineOfSightQuaternion mutations in an independent temporary tree.

The checkout (including vendor headers) is never modified. Compile failures are
not accepted as mutation detection: each mutant must build and then fail the
release-active compiled C11 contract with the named assertion. The baseline is
restored and rerun afterwards.

Mutants cover the eight IRMEL-ADA-010 controls: swapped quaternion x and w,
zeroed quaternion z, swapped forward and right rates, a corrupted signed
timestamp, the quaternion callback registered through the base four-kind
owner, a snapshot allocated despite a full queue, a null payload counted as
queue-full instead of malformed, and a missing in-flight teardown drain.
"""

from pathlib import Path
import hashlib
import os
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
TARGET = "test_ir_image_los_quaternion"
COPY = "src/ir_image_los_quaternion.cpp"
BASE_OWNER = "src/ir_image.cpp"
STREAM = "src/ir_stream.cpp"
NULL_CHECK = ("            if (!input)\n"
              "                /* A null payload is malformed even while the queue is full. */\n"
              "                outcome = Outcome::Malformed;\n"
              "            else if (full)")


def run(command):
    subprocess.run(command, check=True, env={k: v for k, v in os.environ.items()
                                          if k != "LD_RUN_PATH"})


# (name, relative source, before, after, expected diagnostic fragment, contract arguments)
MUTATIONS = [
    ("quaternion x and w swapped", COPY,
     "    view.q_xyzw.x = q.getQuaternionX();\n"
     "    view.q_xyzw.y = q.getQuaternionY();\n"
     "    view.q_xyzw.z = q.getQuaternionZ();\n"
     "    view.q_xyzw.w = q.getQuaternionW();",
     "    view.q_xyzw.x = q.getQuaternionW();\n"
     "    view.q_xyzw.y = q.getQuaternionY();\n"
     "    view.q_xyzw.z = q.getQuaternionZ();\n"
     "    view.q_xyzw.w = q.getQuaternionX();",
     "same(v->q_xyzw.x, x)", []),
    ("quaternion z zeroed", COPY,
     "    view.q_xyzw.z = q.getQuaternionZ();",
     "    view.q_xyzw.z = 0.0;",
     "same(v->q_xyzw.z, z)", []),
    ("forward and right rates swapped", COPY,
     "    view.pqr_rps.forward = rates.getForward();\n"
     "    view.pqr_rps.right = rates.getRight();",
     "    view.pqr_rps.forward = rates.getRight();\n"
     "    view.pqr_rps.right = rates.getForward();",
     "same(v->pqr_rps.forward, forward)", []),
    ("signed nanosecond timestamp corrupted", COPY,
     "    view.system_time_ns = static_cast<std::int64_t>(input.getSystemTime().count());",
     "    view.system_time_ns = static_cast<std::int64_t>(\n"
     "        static_cast<std::uint64_t>(input.getSystemTime().count()) & 0x7fffffffffffffffULL);",
     "v->system_time_ns == time", []),
    ("quaternion callback registered through the base four-kind owner", BASE_OWNER,
     "        auto owner = std::make_unique<ams_mel_ir_image_metadata>();\n"
     "        owner->state = std::move(state);",
     "        (void)channel->registerMetadataCallback(\n"
     "            [](irmel::Channel&, const irmel::LineOfSightQuaternion *const) {});\n"
     "        auto owner = std::make_unique<ams_mel_ir_image_metadata>();\n"
     "        owner->state = std::move(state);",
     "registrations() == before", []),
    ("full queue allocates a snapshot before dropping", COPY,
     "                outcome = Outcome::Full;",
     "            {\n"
     "                event = ams_mel::internal::copy_line_of_sight_quaternion(*input);\n"
     "                outcome = Outcome::Full;\n"
     "            }",
     "counters_are(m, 2, 1, 0) == EXIT_SUCCESS", []),
    ("null payload counted as queue-full instead of malformed", COPY,
     NULL_CHECK,
     "            if (!input)\n"
     "                outcome = full ? Outcome::Full : Outcome::Malformed;\n"
     "            else if (full)",
     "counters_are(m, 3, 1, 1) == EXIT_SUCCESS", []),
    ("teardown no longer drains in-flight quaternion callbacks", STREAM,
     "        image_los_quaternion_stream_drained(los_quaternion);",
     "        image_los_quaternion_stream_stopped(los_quaternion);",
     "PROBE(3U, 0U) == 1", ["callback-in-flight"]),
]


def main():
    digests = {name: hashlib.sha256((ROOT / "native" / name).read_bytes()).hexdigest()
               for name in {m[1] for m in MUTATIONS}}
    with tempfile.TemporaryDirectory(prefix="ams-mel-image-losq-mutations-") as temporary:
        root = Path(temporary)
        source = root / "native"
        shutil.copytree(ROOT / "native", source,
                        ignore=shutil.ignore_patterns("build*", "alire", ".git"))
        build = root / "build"
        run(["cmake", "-S", str(source), "-B", str(build),
             "-DAMS_MEL_BUILD_TESTS=ON", "-DCMAKE_BUILD_TYPE=Release"])

        def compile_contract():
            run(["cmake", "--build", str(build), "--parallel", "4", "--target", TARGET])

        def contract(arguments):
            return subprocess.run([str(build / TARGET), *arguments], text=True, timeout=120,
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

        compile_contract()
        for arguments in ([], ["callback-in-flight"]):
            baseline = contract(arguments)
            assert baseline.returncode == 0, baseline.stdout
        for name, relative, before, after, diagnostic, arguments in MUTATIONS:
            target = source / relative
            pristine = target.read_text()
            assert pristine.count(before) == 1, (name, before)
            try:
                target.write_text(pristine.replace(before, after, 1))
                compile_contract()
                result = contract(arguments)
                assert result.returncode != 0 and diagnostic in result.stdout, (name, result.stdout)
                print(f"PASS: mutation detected: {name}: {diagnostic}", flush=True)
            finally:
                target.write_text(pristine)
        compile_contract()
        for arguments in ([], ["callback-in-flight"]):
            restored = contract(arguments)
            assert restored.returncode == 0, restored.stdout
        print("PASS: restored baseline contract", flush=True)
    for name, digest in digests.items():
        assert hashlib.sha256((ROOT / "native" / name).read_bytes()).hexdigest() == digest
    print("PASS: checkout source unchanged; isolated source/build tree removed", flush=True)


if __name__ == "__main__":
    main()
