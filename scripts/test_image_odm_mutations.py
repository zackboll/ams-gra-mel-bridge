#!/usr/bin/env python3
"""Mock-only Image OpticalDistortionMap mutations in an independent temporary tree.

The checkout (including vendor headers) is never modified. Compile failures are
not accepted as mutation detection: each mutant must build and then fail the
release-active compiled C11 contract with the named assertion. The baseline is
restored and rerun afterwards.

Mutants cover the five required contract controls (final FRD element dropped,
final RowCol element dropped, swapped forward/right and row/column components,
one replaced extremum, and the ODM callback registered through the base
four-kind owner) plus full-queue misclassification and a missing teardown drain.
"""

from pathlib import Path
import hashlib
import os
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
TARGET = "test_ir_image_odm"
ODM_COPY = "src/ir_image_odm.cpp"
BASE_OWNER = "src/ir_image.cpp"
STREAM = "src/ir_stream.cpp"
FULL_CHECK = ("                outcome = ams_mel::internal::valid_optical_distortion_map(*input) ?\n"
              "                              Outcome::Full : Outcome::Malformed;")


def run(command):
    subprocess.run(command, check=True, env={k: v for k, v in os.environ.items()
                                          if k != "LD_RUN_PATH"})


# (name, relative source, before, after, expected diagnostic fragment, contract arguments)
MUTATIONS = [
    ("final FRD element dropped", ODM_COPY,
     "    for (const auto& value : frd)\n"
     "        event->frd.push_back({value.getForward(), value.getRight(), value.getDown()});",
     "    for (std::size_t index = 0; index + 1U < frd.size(); ++index)\n"
     "        event->frd.push_back({frd[index].getForward(), frd[index].getRight(),\n"
     "                              frd[index].getDown()});",
     "v->frd.size == 4U && v->row_col.size == 3U", []),
    ("final RowCol element dropped", ODM_COPY,
     "    for (const auto& value : row_col)\n"
     "        event->row_col.push_back({value.getRow(), value.getCol()});",
     "    for (std::size_t index = 0; index + 1U < row_col.size(); ++index)\n"
     "        event->row_col.push_back({row_col[index].getRow(), row_col[index].getCol()});",
     "v->frd.size == 4U && v->row_col.size == 3U", []),
    ("forward/right swapped", ODM_COPY,
     "{value.getForward(), value.getRight(), value.getDown()}",
     "{value.getRight(), value.getForward(), value.getDown()}",
     "same(v->frd.data[index].forward, f)", []),
    ("row/column swapped", ODM_COPY,
     "{value.getRow(), value.getCol()}",
     "{value.getCol(), value.getRow()}",
     "same(v->row_col.data[index].row, row)", []),
    ("down_max extremum replaced by down_min", ODM_COPY,
     "    view.down_max = input.getDownMax();",
     "    view.down_max = input.getDownMin();",
     "same(v->down_max, dmax)", []),
    ("ODM registered through the base four-kind owner", BASE_OWNER,
     "        auto owner = std::make_unique<ams_mel_ir_image_metadata>();\n"
     "        owner->state = std::move(state);",
     "        (void)channel->registerMetadataCallback(\n"
     "            [](irmel::Channel&, const irmel::OpticalDistortionMap *const) {});\n"
     "        auto owner = std::make_unique<ams_mel_ir_image_metadata>();\n"
     "        owner->state = std::move(state);",
     "registrations() == before", []),
    ("full queue misclassifies malformed maps as queue-full", ODM_COPY,
     FULL_CHECK, "                outcome = Outcome::Full;",
     "counters_are(m, 4, 1, 2) == EXIT_SUCCESS", []),
    ("full queue builds a snapshot before dropping", ODM_COPY,
     FULL_CHECK,
     "                outcome = ams_mel::internal::copy_optical_distortion_map(*input) ?\n"
     "                              Outcome::Full : Outcome::Malformed;",
     "counters_are(m, 2, 1, 0) == EXIT_SUCCESS", []),
    ("teardown no longer drains in-flight ODM callbacks", STREAM,
     "        image_odm_stream_drained(odm);",
     "        image_odm_stream_stopped(odm);",
     "ams_mel_test_image_odm_probe(3U, 0U) == 1", ["callback-in-flight"]),
]


def main():
    digests = {name: hashlib.sha256((ROOT / "native" / name).read_bytes()).hexdigest()
               for name in {m[1] for m in MUTATIONS}}
    with tempfile.TemporaryDirectory(prefix="ams-mel-image-odm-mutations-") as temporary:
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
