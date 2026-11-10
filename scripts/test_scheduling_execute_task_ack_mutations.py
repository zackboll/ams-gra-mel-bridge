#!/usr/bin/env python3
"""IRMEL-ADA-012: compile mutants, require named C11 failures, restore and requalify.

Uses an isolated temporary tree; checkout sources are never modified.
"""
from pathlib import Path
import hashlib
import os
import shutil
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
TARGET = "test_ir_scheduling_execute_task_ack"
FOUNDATION = "test_ir_scheduling_foundation"
COPY = "src/ir_scheduling_execute_task_ack.cpp"
OWNER = "src/ir_scheduling.cpp"

def run(command):
    subprocess.run(command, check=True, env={k: v for k, v in os.environ.items()
                                          if k != "LD_RUN_PATH"})

MUTATIONS = [
    ('swap IDs', 'src/ir_scheduling_execute_task_ack.cpp', 'event->view.command_id = input.getCommandID();\n    event->view.request_id = input.getReqId();', 'event->view.command_id = input.getReqId();\n    event->view.request_id = input.getCommandID();', 'v->command_id == command', ['fidelity']),
    ('truncate command', 'src/ir_scheduling_execute_task_ack.cpp', 'event->view.command_id = input.getCommandID();', 'event->view.command_id = static_cast<std::uint16_t>(input.getCommandID());', 'v->command_id == command', ['fidelity']),
    ('truncate request', 'src/ir_scheduling_execute_task_ack.cpp', 'event->view.request_id = input.getReqId();', 'event->view.request_id = static_cast<std::uint16_t>(input.getReqId());', 'v->request_id == request', ['fidelity']),
    ('remove final queued event', 'src/ir_scheduling_execute_task_ack.cpp', 'state.queue.pop_front();', 'state.queue.pop_front();\n            if (state.queue.size() == 1U) state.queue.pop_back();', '_receive(m, 0, &event, NULL, 0, NULL) == AMS_MEL_OK', ['drain-after-close']),
    ('null counted full', 'src/ir_scheduling_execute_task_ack.cpp', 'outcome = Outcome::Malformed;\n            else if (full)', 'outcome = full ? Outcome::Full : Outcome::Malformed;\n            else if (full)', 'c.events_dropped_queue_full == dropped', ['capacity-one']),
    ('full allocates snapshot', 'src/ir_scheduling_execute_task_ack.cpp', '                outcome = Outcome::Full;', '            {\n                event = ams_mel::internal::copy_execute_task_ack(*input);\n                outcome = Outcome::Full;\n            }', 'c.events_dropped_queue_full == dropped', ['capacity-one']),
    ('allow second registration', 'src/ir_scheduling.cpp', '!scheduling->scheduling || scheduling->execute_task_ack_attempted)', '!scheduling->scheduling)', 'NULL) == AMS_MEL_INVALID_ARGUMENT', ['attached-sync']),
    ('omit callback drain', 'src/ir_scheduling.cpp', '    ams_mel::internal::scheduling_execute_task_ack_drained(ack);', '    ams_mel::internal::scheduling_execute_task_ack_stopped(ack);', 'PROBE(3U, 0U) == 1', ['in-flight']),
    ('foundation implicitly registers', 'src/ir_scheduling.cpp', '        /* Publish the owner only after every validation succeeded. */', '        (void)state->scheduling->registerMetadataCallback(\n            [](irmel::Channel&, const irmel::ExecuteTaskAck *const) {});\n        /* Publish the owner only after every validation succeeded. */', 'ack_registrations() == 0U', ['foundation']),
]


def main():
    digests = {name: hashlib.sha256((ROOT / "native" / name).read_bytes()).hexdigest()
               for name in {m[1] for m in MUTATIONS}}
    with tempfile.TemporaryDirectory(prefix="ams-mel-sched-ack-mutations-") as temporary:
        root = Path(temporary)
        source = root / "native"
        shutil.copytree(ROOT / "native", source,
                        ignore=shutil.ignore_patterns("build*", "alire", ".git"))
        build = root / "build"
        run(["cmake", "-S", str(source), "-B", str(build),
             "-DAMS_MEL_BUILD_TESTS=ON", "-DCMAKE_BUILD_TYPE=Release"])

        def compile_contract():
            run(["cmake", "--build", str(build), "--parallel", "8", "--target", TARGET, FOUNDATION])

        def contract(arguments):
            target = FOUNDATION if arguments == ["foundation"] else TARGET
            args = ["open-enable-close"] if arguments == ["foundation"] else arguments
            return subprocess.run([str(build / target), *args], text=True, timeout=120,
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

        compile_contract()
        for arguments in ([scenario] for scenario in (
                "fidelity attached-sync enabled register-not-supported register-fail "
                "register-not-implemented register-unknown register-throw failed-owner invalid "
                "capacity-one fifo close-wakes drain-after-close owner-closed-first parent-first "
                "late common pending pending-detach-fail detach-fail in-flight foundation").split()):
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
        for arguments in ([scenario] for scenario in (
                "fidelity attached-sync enabled register-not-supported register-fail "
                "register-not-implemented register-unknown register-throw failed-owner invalid "
                "capacity-one fifo close-wakes drain-after-close owner-closed-first parent-first "
                "late common pending pending-detach-fail detach-fail in-flight foundation").split()):
            restored = contract(arguments)
            assert restored.returncode == 0, restored.stdout
        print("PASS: restored baseline contracts", flush=True)
    for name, digest in digests.items():
        assert hashlib.sha256((ROOT / "native" / name).read_bytes()).hexdigest() == digest
    print("PASS: checkout source unchanged; isolated source/build tree removed", flush=True)


if __name__ == "__main__":
    main()
