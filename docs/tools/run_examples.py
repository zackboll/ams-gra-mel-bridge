#!/usr/bin/env python3
"""Build real public Ada examples, inspect the loader and qualify every run case."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

from example_sources import ROOT, inventory


def environment():
    # Alire's test crate prepends the test-enabled facade. Never inherit it.
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(("LD_", "AMS_MEL_")) and key != "LIBRARY_PATH"}
    env["LD_LIBRARY_PATH"] = str(ROOT / "native/build/lib")
    return env


def command(args, env, timeout=600):
    subprocess.run(args, cwd=ROOT, env=env, check=True, timeout=timeout)


def loader(executable, env):
    result = subprocess.run(["ldd", str(executable)], env=env, text=True,
                            capture_output=True, check=True, timeout=15)
    matches = re.findall(r"libams_mel_c\.so(?:\.[0-9]+)* => (\S+)", result.stdout)
    production = (ROOT / "native/build/lib/libams_mel_c.so").resolve(strict=True)
    if len(matches) != 1 or Path(matches[0]).resolve(strict=True) != production:
        raise ValueError("execution does not resolve the production facade: " + result.stdout)
    symbols = subprocess.run(["nm", "-D", "--defined-only", str(production)], env=env,
                             capture_output=True, text=True, check=True, timeout=15).stdout
    if re.search(r"\bams_mel_test_|\bams_mel_\w*failpoint", symbols):
        raise ValueError("test-enabled facade loaded")
    return {"path": str(production), "sha256": hashlib.sha256(production.read_bytes()).hexdigest()}


def qualify(case, result):
    if result.returncode != case["exit"]:
        raise ValueError(f'{case["id"]}: exit {result.returncode}, expected {case["exit"]}')
    output = result.stdout + result.stderr
    start = 0
    for expected in case["expected"]:
        found = output.find(expected, start)
        if found < 0:
            raise ValueError(f'{case["id"]}: missing semantic observation: {expected!r}\n{output}')
        start = found + len(expected)


def execute(executable, provider, configuration, env, identity):
    # Inspect the actual child loader too, not just ldd's prediction. glibc writes
    # per-process diagnostics outside the source/build trees; never inherit these.
    with tempfile.TemporaryDirectory(prefix="mel-example-loader-") as temp:
        child_env = dict(env, LD_DEBUG="libs", LD_DEBUG_OUTPUT=str(Path(temp) / "loader"))
        result = subprocess.run([str(executable), str(provider), configuration], cwd=ROOT,
                                env=child_env, capture_output=True, text=True, timeout=15)
        logs = list(Path(temp).glob("loader.*"))
        initialized = [path.strip() for log in logs
                       for path in re.findall(r"calling init:\s*([^\n]*libams_mel_c[^\n]*)",
                                              log.read_text())]
        if len(initialized) != 1 or Path(initialized[0]).resolve(strict=True) != Path(identity["path"]):
            raise ValueError("actual child did not initialize the qualified production facade")
        identity["runtime_initialized_path"] = initialized[0]
        return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("case", nargs="?", default="all")
    parser.add_argument("--no-build", action="store_true", help="use an already built collection")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    entries = inventory()
    selected = [(entry, case) for entry in entries for case in entry["cases"]
                if args.case in {"all", case["id"]}]
    if not selected:
        parser.error("unknown run case: " + args.case)
    env = environment()
    if not args.no_build:
        command(["sh", "native/scripts/build.sh"], env)
        command(["sh", "native/scripts/build-test-providers.sh"], env)
        command(["alr", "-n", "-C", "ada", "exec", "--", "gprbuild", "-p", "-P",
                 str(ROOT / "docs/examples/ams_mel_doc_examples.gpr")], env)
    report = []
    for entry, case in selected:
        executable = ROOT / "docs/examples/bin" / entry["executable"]
        identity = loader(executable, env)
        provider = ROOT / "native/build-tests/test-providers" / entry["mock_library"]
        if case["provider"] == "missing":
            provider = ROOT / "docs/examples/bin/nonexistent-provider.so"
            if provider.exists():
                raise ValueError("negative provider path unexpectedly exists")
        result = execute(executable, provider, case["configuration"], env, identity)
        print(f'[{case["id"]}] exit={result.returncode}', flush=True)
        print(result.stdout + result.stderr, end="", flush=True)
        qualify(case, result)
        report.append({"case": case["id"], "main": entry["main"], "exit": result.returncode,
                       "configuration": case["configuration"], "facade": identity,
                       "stdout": result.stdout, "stderr": result.stderr})
    if len(report) != len(selected) or (args.case == "all" and
            {r["main"] for r in report} != {e["main"] for e in entries}):
        raise ValueError("incomplete execution inventory")
    if args.report:
        args.report.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n")
    print(f'Qualified {len(report)} run cases / {len({r["main"] for r in report})} programs; '
          f'production facade {report[0]["facade"]["path"]} '
          f'SHA256={report[0]["facade"]["sha256"]}')


if __name__ == "__main__":
    main()
