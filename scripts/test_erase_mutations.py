#!/usr/bin/env python3
"""Mock-only EraseCommand fidelity mutations in an independent temporary source/build tree.

The checkout (including vendor headers) is never modified. Compile failures are
not accepted as mutation detection: each mutant must build and then fail the
release-active compiled C11 command-fidelity check. Restore and rerun baseline.
"""

from pathlib import Path
import hashlib
import os
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def run(command):
    subprocess.run(command, check=True, env={k: v for k, v in os.environ.items()
                                          if k != "LD_RUN_PATH"})


def main():
    original = ROOT / "native/src/ir_erase.cpp"
    digest = hashlib.sha256(original.read_bytes()).hexdigest()
    with tempfile.TemporaryDirectory(prefix="ams-mel-erase-mutations-") as temporary:
        root = Path(temporary)
        source = root / "native"
        shutil.copytree(ROOT / "native", source,
                        ignore=shutil.ignore_patterns("build*", "alire", ".git"))
        build = root / "build"
        run(["cmake", "-S", str(source), "-B", str(build),
             "-DAMS_MEL_BUILD_TESTS=ON", "-DCMAKE_BUILD_TYPE=Release"])
        target = source / "src/ir_erase.cpp"
        pristine = target.read_text()

        def compile_contract():
            run(["cmake", "--build", str(build), "--parallel", "2", "--target", "test_ir_erase"])

        def contract():
            return subprocess.run([str(build / "test_ir_erase")],
                                  text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

        compile_contract()
        baseline = contract()
        assert baseline.returncode == 0, baseline.stdout
        mutations = [
            ("erase type altered", "output.setEraseCommandType(static_cast<irmel::EraseCommandType>(input.erase_type));",
             "output.setEraseCommandType(irmel::EraseCommandType::Disable);", "values[1] == c->erase_type"),
            ("command ID lost", "output.setCommandID(input.command_id);",
             "output.setCommandID(0U);", "values[0] == c->command_id"),
        ]
        try:
            for name, before, after, diagnostic in mutations:
                assert pristine.count(before) == 1, before
                target.write_text(pristine.replace(before, after, 1))
                compile_contract()
                result = contract()
                assert result.returncode != 0 and diagnostic in result.stdout, result.stdout
                print(f"PASS: mutation detected: {name}: {diagnostic}", flush=True)
        finally:
            target.write_text(pristine)
        compile_contract()
        restored = contract()
        assert restored.returncode == 0, restored.stdout
        print("PASS: restored baseline contract", flush=True)
    assert hashlib.sha256(original.read_bytes()).hexdigest() == digest
    print("PASS: checkout source unchanged; isolated source/build tree removed", flush=True)


if __name__ == "__main__":
    main()
