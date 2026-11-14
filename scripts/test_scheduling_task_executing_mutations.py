#!/usr/bin/env python3
"""IRMEL-ADA-013: compile mutants, require named C11 failures, restore and requalify.

Uses an isolated temporary tree; checkout sources are never modified. A mutant counts only
when it compiles and the identified contract assertion text appears in the failing output.
"""
from pathlib import Path
import hashlib
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
TARGET = "test_ir_scheduling_task_executing"
ACK = "test_ir_scheduling_execute_task_ack"
FOUNDATION = "test_ir_scheduling_foundation"
COPY = "src/ir_scheduling_task_executing.cpp"
OWNER = "src/ir_scheduling.cpp"
SCENARIOS = (
    "fidelity enums invalid-enums attached-sync enabled register-not-supported register-fail "
    "register-not-implemented register-unknown register-throw failed-owner invalid capacity-one "
    "fifo close-wakes drain-after-close owner-closed-first parent-first late common pending "
    "pending-detach-fail detach-fail in-flight both-report-first both-ack-first both-sync "
    "both-close-wakes both-late both-in-flight both-detach-fail").split()
ACK_SCENARIOS = "fidelity attached-sync in-flight".split()
RECEIVE_OK = "_receive(m, 0, &event, NULL, 0, NULL) == AMS_MEL_OK"
RECEIVE_STATUS = "_receive(m, timeout, &event, NULL, 0, NULL) == status"


def run(command):
    subprocess.run(command, check=True, env={k: v for k, v in os.environ.items()
                                             if k != "LD_RUN_PATH"})


def field(name, value):
    return f"output.{name} = {value};"


MUTATIONS = [
    # Field swaps between independent current/previous and identifier fields.
    ("activity reads previous activity", COPY,
     field("activity_id", "input.getActivityId()"),
     field("activity_id", "input.getPrevActivityId()"),
     "v->activity_id == e->activity_id", ["fidelity"]),
    ("previous request reads current request", COPY,
     field("previous_request_id", "input.getPrevReqId()"),
     field("previous_request_id", "input.getReqId()"),
     "v->previous_request_id == e->previous_request_id", ["fidelity"]),
    ("current task reads previous task", COPY,
     field("task", "static_cast<std::uint32_t>(input.getTask())"),
     field("task", "static_cast<std::uint32_t>(input.getPrevTask())"),
     "v->task == e->task", ["enums"]),
    ("command reads request", COPY,
     field("command_id", "input.getCommandID()"),
     field("command_id", "input.getReqId()"),
     "v->command_id == e->command_id", ["fidelity"]),
    # Truncation, unit conversion and signed-value corruption.
    ("duration truncated to 32 bits", COPY,
     field("estimated_duration_ns", "input.getEstimatedDuration().count()"),
     field("estimated_duration_ns",
           "static_cast<std::int32_t>(input.getEstimatedDuration().count())"),
     "v->estimated_duration_ns == e->estimated_duration_ns", ["fidelity"]),
    ("duration converted to microseconds", COPY,
     field("estimated_duration_ns", "input.getEstimatedDuration().count()"),
     field("estimated_duration_ns", "input.getEstimatedDuration().count() / 1000"),
     "v->estimated_duration_ns == e->estimated_duration_ns", ["fidelity"]),
    ("track identifier sign and width lost", COPY,
     field("track_id", "input.getTrackId()"),
     field("track_id",
           "static_cast<std::int32_t>(static_cast<std::uint16_t>(input.getTrackId()))"),
     "v->track_id == e->track_id", ["fidelity"]),
    ("previous activity truncated", COPY,
     field("previous_activity_id", "input.getPrevActivityId()"),
     field("previous_activity_id", "static_cast<std::uint16_t>(input.getPrevActivityId())"),
     "v->previous_activity_id == e->previous_activity_id", ["fidelity"]),
    # Enum validation omissions and policy changes.
    ("omit current task validation", COPY,
     "    if (!defined_task(output.task)) return false;\n", "",
     "c.malformed_or_unsupported == malformed", ["invalid-enums"]),
    ("omit previous task validation", COPY,
     "    if (!defined_task(output.previous_task)) return false;\n", "",
     "c.malformed_or_unsupported == malformed", ["invalid-enums"]),
    ("omit previous status validation", COPY,
     "    if (!defined_status(output.previous_task_status)) return false;",
     "    if (!defined_status(output.previous_task_status) && false) return false;",
     "c.malformed_or_unsupported == malformed", ["invalid-enums"]),
    ("accept NumTaskTypes sentinel", COPY,
     "return value <= AMS_MEL_IR_TASK_RESERVED_6; }",
     "return value <= AMS_MEL_IR_TASK_RESERVED_6 + 1U; }",
     "c.malformed_or_unsupported == malformed", ["invalid-enums"]),
    ("reject reserved tasks", COPY,
     "return value <= AMS_MEL_IR_TASK_RESERVED_6; }",
     "return value <= AMS_MEL_IR_TASK_IRST_UPDATE; }",
     RECEIVE_OK, ["enums"]),
]

MUTATIONS += [
    # Queue-full and malformed classification.
    ("invalid enum counted full", COPY,
     "            else if (!ams_mel::internal::read_task_executing_rep(*input, value))\n"
     "                outcome = Outcome::Malformed;\n"
     "            else if (full)",
     "            else if (!full && !ams_mel::internal::read_task_executing_rep(*input, value))\n"
     "                outcome = Outcome::Malformed;\n"
     "            else if (full)",
     "c.events_dropped_queue_full == dropped", ["capacity-one"]),
    ("full queue allocates snapshot", COPY,
     "                outcome = Outcome::Full;",
     "            {\n                event = ams_mel::internal::snapshot_task_executing_rep(value);\n"
     "                outcome = Outcome::Full;\n            }",
     "c.events_dropped_queue_full == dropped", ["capacity-one"]),
    ("remove final queued event", COPY,
     "            state.queue.pop_front();",
     "            state.queue.pop_front();\n"
     "            if (state.queue.size() == 1U) state.queue.pop_back();",
     RECEIVE_OK, ["drain-after-close"]),
    # Registration, family independence and teardown.
    ("allow second registration", OWNER,
     "!scheduling->scheduling || scheduling->task_executing_attempted)",
     "!scheduling->scheduling)",
     "NULL) == AMS_MEL_INVALID_ARGUMENT", ["attached-sync"]),
    ("share the ExecuteTaskAck one shot", OWNER,
     "!scheduling->scheduling || scheduling->task_executing_attempted)",
     "!scheduling->scheduling || scheduling->task_executing_attempted ||\n"
     "            scheduling->execute_task_ack_attempted)",
     "NULL) == AMS_MEL_OK", ["both-ack-first"]),
    ("omit TaskExecutingRep callback drain", OWNER,
     "    ams_mel::internal::scheduling_task_executing_drained(executing);",
     "    ams_mel::internal::scheduling_task_executing_stopped(executing);",
     "PROBE(3U, 0U) == 1", ["in-flight"]),
    ("omit Scheduling Close stop", OWNER,
     "        ams_mel::internal::scheduling_task_executing_stopped(executing);\n",
     "",
     RECEIVE_STATUS, ["pending"]),
    ("omit failed-detach notification", OWNER,
     "        ams_mel::internal::scheduling_task_executing_failed(executing);\n        return false;",
     "        return false;",
     "_receive(m, 10U, &event, d, sizeof d, NULL) == AMS_MEL_PROVIDER_FAILED", ["detach-fail"]),
    ("foundation implicitly registers", OWNER,
     "        /* Publish the owner only after every validation succeeded. */",
     "        (void)state->scheduling->registerMetadataCallback(\n"
     "            [](irmel::Channel&, const irmel::TaskExecutingRep *const) {});\n"
     "        /* Publish the owner only after every validation succeeded. */",
     "rep_registrations() == 0U", ["foundation"]),
]


def main():
    digests = {name: hashlib.sha256((ROOT / "native" / name).read_bytes()).hexdigest()
               for name in {m[1] for m in MUTATIONS}}
    with tempfile.TemporaryDirectory(prefix="ams-mel-sched-rep-mutations-") as temporary:
        root = Path(temporary)
        source = root / "native"
        shutil.copytree(ROOT / "native", source,
                        ignore=shutil.ignore_patterns("build*", "alire", ".git"))
        build = root / "build"
        run(["cmake", "-S", str(source), "-B", str(build),
             "-DAMS_MEL_BUILD_TESTS=ON", "-DCMAKE_BUILD_TYPE=Release"])

        def compile_contract():
            run(["cmake", "--build", str(build), "--parallel", "8", "--target", TARGET, ACK,
                 FOUNDATION])

        def contract(arguments):
            if arguments == ["foundation"]:
                target, arguments = FOUNDATION, ["open-enable-close"]
            elif arguments[0].startswith("ack:"):
                target, arguments = ACK, [arguments[0][4:]]
            else:
                target = TARGET
            return subprocess.run([str(build / target), *arguments], text=True, timeout=120,
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

        every = ([[s] for s in SCENARIOS] + [["ack:" + s] for s in ACK_SCENARIOS] +
                 [["foundation"]])
        compile_contract()
        for arguments in every:
            baseline = contract(arguments)
            assert baseline.returncode == 0, (arguments, baseline.stdout)
        print(f"PASS: {len(every)} unmutated baseline scenarios", flush=True)
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
        for arguments in every:
            restored = contract(arguments)
            assert restored.returncode == 0, (arguments, restored.stdout)
        print(f"PASS: {len(MUTATIONS)} compiled mutants detected; restored baseline contracts",
              flush=True)
    for name, digest in digests.items():
        assert hashlib.sha256((ROOT / "native" / name).read_bytes()).hexdigest() == digest
    print("PASS: checkout source unchanged; isolated source/build tree removed", flush=True)


if __name__ == "__main__":
    main()
