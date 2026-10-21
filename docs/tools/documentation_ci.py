#!/usr/bin/env python3
"""Fail-closed, standard-library documentation inventory, runner and artifact gate."""

import argparse
import ast
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tarfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]
SHARDS = {
    "navigation": ["test_navigation"],
    "publication": ["test_enhanced_site"],
    "live": ["test_live_docs_site"],
    "symbols-catalog": ["test_symbol_bookmarks", "test_api_catalog"],
    "types": ["test_type_link_completeness", "test_local_type_audit"],
    "remaining": ["test_ada_examples", "test_document_metadata", "test_audit_output",
                  "test_examples", "test_documentation_ci"],
}
INHERITED = ROOT / "docs/tests/ci001-inherited-inventory.json"
BASELINE_SHA = "9288f6acd4cbf807f576d387db390459f0b117c0"
PAYLOAD = ("_site", "_site-published", "obj/docs")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def canonical(value):
    return (json.dumps(value, sort_keys=True, separators=(",", ":")) + "\n").encode()


def digest(value):
    return hashlib.sha256(canonical(value)).hexdigest()


def file_sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def load(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, "duplicate JSON key: " + key)
            result[key] = value
        return result
    return json.loads(Path(path).read_text(), object_pairs_hook=unique)


def write(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(canonical(value))


def git(*args):
    return subprocess.check_output(["git", "-C", str(ROOT), *args], text=True).strip()


def identity():
    sha = os.environ["DOC_SOURCE_SHA"]
    run = os.environ["GITHUB_RUN_ID"]
    attempt = os.environ["GITHUB_RUN_ATTEMPT"]
    require(re.fullmatch(r"[0-9a-f]{40}", sha), "invalid source SHA")
    require(run.isdigit() and attempt.isdigit(), "invalid run/attempt")
    require(git("rev-parse", "HEAD") == sha, "checkout source identity mismatch")
    context = {"source_sha": sha, "source_tree": git("rev-parse", "HEAD^{tree}"),
               "run_id": run, "run_attempt": attempt,
               "repository": os.environ["GITHUB_REPOSITORY"], "qualification_mode": "hosted"}
    if os.environ.get("DOC_LOCAL_QUALIFICATION") == "1":
        require(os.environ.get("GITHUB_ACTIONS") != "true", "local mode forbidden in hosted CI")
        names = git("ls-files", "--cached").splitlines() + git(
            "ls-files", "--others", "--exclude-standard", "--", "docs", "scripts", ".github").splitlines()
        context.update(qualification_mode="local-not-deployable", working_tree_sha256=digest(
            {name: file_sha(ROOT / name) for name in names if name}))
    else:
        require(not git("diff", "HEAD", "--"), "tracked checkout is dirty")
    return context


def artifact_name(kind, context):
    return "documentation-{}-{}-{}-{}".format(
        kind, context["run_id"], context["run_attempt"], context["source_sha"])


def flatten(suite):
    for test in suite:
        if isinstance(test, unittest.TestSuite):
            yield from flatten(test)
        else:
            yield test


def discover(group):
    directory, pattern = (("scripts", "test_*doc*.py") if group == "scripts"
                          else ("docs/tests", "test_*.py"))
    loader = unittest.TestLoader()
    suite = loader.discover(str(ROOT / directory), pattern=pattern)
    require(not loader.errors, "test discovery failed: " + "\n".join(loader.errors))
    tests = list(flatten(suite))
    ids = [test.id() for test in tests]
    require(ids and len(ids) == len(set(ids)), "empty or duplicate discovered test identities")
    return tests


def assign(ids, groups=SHARDS):
    require(len(ids) == len(set(ids)), "duplicate assignment identity")
    assignments = {shard: [] for shard in groups}
    for test in sorted(ids):
        owners = [shard for shard, modules in groups.items() if test.split(".")[0] in modules]
        require(len(owners) == 1, "test must have exactly one shard owner: " + test)
        assignments[owners[0]].append(test)
    require(all(assignments.values()), "empty shard")
    return assignments


def inventory():
    groups = {name: sorted(test.id() for test in discover(name))
              for name in ("scripts", "enhanced")}
    inherited = load(INHERITED)
    # Keep the exact inherited test sources and assertions, not only their names.
    baseline_sources = git("ls-tree", "-r", "--name-only", BASELINE_SHA).splitlines()
    for name in baseline_sources:
        path = Path(name)
        if ((path.parent == Path("docs/tests") and path.match("test_*.py")) or
                (path.parent == Path("scripts") and path.match("test_*doc*.py"))):
            original = subprocess.check_output(["git", "-C", str(ROOT), "show", f"{BASELINE_SHA}:{name}"])
            current = (ROOT / name).read_bytes()
            if name == "docs/tests/test_live_docs_site.py":
                # Only the two workflow-shape tests follow the reviewed topology
                # change. All HTTP/publication/security assertions stay frozen.
                def invariant_ast(content):
                    tree = ast.parse(content)
                    tree.body = [node for node in tree.body
                                 if not (isinstance(node, ast.ClassDef) and node.name == "WorkflowTests")]
                    return ast.dump(tree)
                require(invariant_ast(current) == invariant_ast(original),
                        "inherited live verifier assertions changed")
            else:
                require(current == original, "inherited test assertions changed: " + name)
    require({key: len(value) for key, value in inherited.items()} ==
            {"scripts": 133, "enhanced": 364}, "inherited inventory changed")
    for group, ids in groups.items():
        require(len(inherited[group]) == len(set(inherited[group])), "duplicate inherited identity")
        require(set(inherited[group]) <= set(ids), "inherited test removed: " + group)
    return {"schema": 1, "groups": groups, "shards": assign(groups["enhanced"]),
            "inherited_sha256": file_sha(INHERITED),
            "test_sources": {p.relative_to(ROOT).as_posix(): file_sha(p)
                             for directory, pattern in (("scripts", "test_*doc*.py"),
                                                        ("docs/tests", "test_*.py"))
                             for p in sorted((ROOT / directory).glob(pattern))}}


def check_inventory(path):
    value = load(path)
    require(value == inventory(), "source-derived inventory/assignment mismatch")
    return value


class EvidenceResult(unittest.TextTestResult):
    """Record actual unittest callbacks, not self-reported counts or selected names."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.events = []
        self.started = {}

    def startTest(self, test):
        super().startTest(test)
        self.started[test.id()] = time.monotonic()
        self.events.append({"event": "start", "id": test.id()})

    def stopTest(self, test):
        self.events.append({"event": "stop", "id": test.id(),
                            "seconds": time.monotonic() - self.started.pop(test.id())})
        super().stopTest(test)

    def outcome(self, test, status):
        self.events.append({"event": status, "id": test.id()})

    def addSuccess(self, test):
        super().addSuccess(test)
        self.outcome(test, "success")

    def addFailure(self, test, err):
        super().addFailure(test, err)
        self.outcome(test, "failure")

    def addError(self, test, err):
        super().addError(test, err)
        self.outcome(test, "error")

    def addSkip(self, test, reason):
        super().addSkip(test, reason)
        self.outcome(test, "skip")

    def addExpectedFailure(self, test, err):
        super().addExpectedFailure(test, err)
        self.outcome(test, "expected-failure")

    def addUnexpectedSuccess(self, test):
        super().addUnexpectedSuccess(test)
        self.outcome(test, "unexpected-success")

    def addSubTest(self, test, subtest, err):
        super().addSubTest(test, subtest, err)
        if err is not None:
            self.outcome(test, "subtest-failure")


def validate_report(report, owner, expected, context, inv_digest, qualified=None):
    require(report["schema"] == 1 and report["owner"] == owner, "wrong report owner/schema")
    require(report["identity"] == context, "report source/run/attempt mismatch")
    require(report["inventory_sha256"] == inv_digest, "report inventory mismatch")
    require(report["qualified"] == qualified, "report qualified artifact mismatch")
    require(report["artifact_name"] == artifact_name("result-" + owner, context),
            "result artifact name mismatch")
    require(report["successful"] is True, "unsuccessful runner")
    require(type(report["tests_run"]) is int and report["tests_run"] == len(expected),
            "forged reported totals")
    events = report["events"]
    require(isinstance(events, list), "malformed events")
    for event in events:
        require(event["event"] in {"start", "success", "stop"}, "non-success test event")
        require(event["id"] in expected, "unknown executed identity")
        if event["event"] == "stop":
            require(type(event["seconds"]) in (int, float) and event["seconds"] >= 0,
                    "invalid test timing")
    for kind in ("start", "success", "stop"):
        require(Counter(e["id"] for e in events if e["event"] == kind) == Counter(expected),
                "missing or repeated test execution: " + kind)
    active = None
    state = None
    for event in events:
        if event["event"] == "start":
            require(active is None, "overlapping test events")
            active, state = event["id"], "start"
        elif event["event"] == "success":
            require(active == event["id"] and state == "start", "invalid success event order")
            state = "success"
        else:
            require(active == event["id"] and state == "success", "invalid stop event order")
            active = state = None
    require(active is None, "unfinished test")


def payload_hashes():
    result = {}
    for directory in PAYLOAD:
        base = ROOT / directory
        require(base.is_dir() and not base.is_symlink(), "missing payload: " + directory)
        for path in sorted(base.rglob("*")):
            require(not path.is_symlink(), "symlink in qualified payload")
            if path.is_file():
                result[path.relative_to(ROOT).as_posix()] = file_sha(path)
            else:
                require(path.is_dir(), "non-regular payload entity")
    require(result, "empty qualified payload")
    return result


def verify_inputs(directory, context, expected_manifest=None):
    directory = Path(directory)
    manifest = load(directory / "manifest.json")
    require(manifest["schema"] == 1, "unknown manifest schema")
    require(manifest["identity"] == context, "qualified input source/run/attempt mismatch")
    if expected_manifest is not None:
        require(digest(manifest) == expected_manifest, "qualified manifest digest mismatch")
    require(manifest["payload"] == payload_hashes(), "qualified publication/input bytes changed")
    inv = check_inventory(directory / "inventory.json")
    require(manifest["inventory_sha256"] == digest(inv), "qualified inventory digest mismatch")
    script = load(directory / "scripts.json")
    require(manifest["scripts_sha256"] == digest(script), "script report digest mismatch")
    validate_report(script, "scripts", inv["groups"]["scripts"], context, digest(inv))
    return manifest, inv


def run_tests(args):
    context = identity()
    directory = Path(args.directory)
    inv = check_inventory(directory / "inventory.json")
    qualified = None
    if args.owner != "scripts":
        manifest, inv = verify_inputs(directory, context, args.manifest_sha)
        require(args.artifact_id and args.artifact_id.isdigit(), "missing qualified artifact ID")
        require(re.fullmatch(r"[0-9a-f]{64}", args.bundle_sha or ""), "missing qualified bundle digest")
        qualified = {"artifact_id": args.artifact_id, "manifest_sha256": digest(manifest),
                     "artifact_name": artifact_name("inputs", context),
                     "bundle_sha256": args.bundle_sha}
    group = "scripts" if args.owner == "scripts" else "enhanced"
    expected = inv["groups"]["scripts"] if group == "scripts" else inv["shards"][args.owner]
    selected = {test.id(): test for test in discover(group)}
    suite = unittest.TestSuite(selected[name] for name in expected)
    started = time.monotonic()
    result = unittest.TextTestRunner(verbosity=2, resultclass=EvidenceResult).run(suite)
    report = {"schema": 1, "owner": args.owner, "identity": context,
              "artifact_name": artifact_name("result-" + args.owner, context),
              "inventory_sha256": digest(inv), "qualified": qualified,
              "tests_run": result.testsRun, "successful": result.wasSuccessful(),
              "seconds": time.monotonic() - started, "events": result.events}
    write(args.output, report)
    validate_report(report, args.owner, expected, context, digest(inv), qualified)
    require(identity() == context, "test execution changed qualified source bytes")
    if group == "enhanced":
        verify_inputs(directory, context, args.manifest_sha)


def seal(args):
    context = identity()
    directory = Path(args.directory)
    inv = check_inventory(directory / "inventory.json")
    script = load(directory / "scripts.json")
    validate_report(script, "scripts", inv["groups"]["scripts"], context, digest(inv))
    manifest = {"schema": 1, "identity": context, "payload": payload_hashes(),
                "inventory_sha256": digest(inv), "scripts_sha256": digest(script)}
    write(directory / "manifest.json", manifest)
    # Only explicit, regular payload files and three metadata records are shipped.
    with tarfile.open(args.output, "w") as archive:
        for name in manifest["payload"]:
            archive.add(ROOT / name, arcname=name, recursive=False)
        for name in ("manifest.json", "inventory.json", "scripts.json"):
            archive.add(directory / name, arcname="qualification/" + name, recursive=False)
    print(json.dumps({"bundle_sha256": file_sha(args.output), "manifest_sha256": digest(manifest)}))
    if os.environ.get("GITHUB_OUTPUT"):
        with open(os.environ["GITHUB_OUTPUT"], "a") as out:
            out.write(f"bundle_sha={file_sha(args.output)}\nmanifest_sha={digest(manifest)}\n")


def unpack(args):
    require(file_sha(args.bundle) == args.bundle_sha, "qualified bundle digest mismatch")
    with tarfile.open(args.bundle) as archive:
        members = archive.getmembers()
        names = [m.name for m in members]
        require(len(names) == len(set(names)), "duplicate archive member")
        for member in members:
            path = Path(member.name)
            require(member.isfile() and not path.is_absolute() and ".." not in path.parts,
                    "unsafe qualified archive member")
            require(any(member.name.startswith(prefix + "/") for prefix in (*PAYLOAD, "qualification")),
                    "unexpected qualified archive path")
            require(not (ROOT / path).exists(), "qualified input destination already exists")
        manifest_file = archive.extractfile("qualification/manifest.json")
        require(manifest_file is not None, "missing archive manifest")
        manifest = json.loads(manifest_file.read())
        require(digest(manifest) == args.manifest_sha, "qualified manifest digest mismatch")
        require(set(names) == set(manifest["payload"]) | {
            "qualification/manifest.json", "qualification/inventory.json", "qualification/scripts.json"},
            "unexpected or missing qualified archive members")
        for member in members:
            target = ROOT / member.name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(archive.extractfile(member).read())
    verify_inputs(ROOT / "qualification", identity(), args.manifest_sha)


def reconcile(directory, reports, context, qualified, needs):
    require(set(needs) == {"prepare-documentation", "enhanced-tests"}, "missing required jobs")
    require(all(job["result"] == "success" for job in needs.values()),
            "required job failed, missing, cancelled or skipped")
    require(qualified["artifact_id"].isdigit() and
            re.fullmatch(r"[0-9a-f]{64}", qualified["bundle_sha256"]), "invalid qualified artifact identity")
    manifest, inv = verify_inputs(directory, context, qualified["manifest_sha256"])
    require(set(reports) == set(SHARDS), "missing or unknown shard result")
    for shard in SHARDS:
        validate_report(reports[shard], shard, inv["shards"][shard], context, digest(inv), qualified)
    return {"schema": 1, "identity": context, "qualified": qualified,
            "inventory_sha256": digest(inv), "manifest_sha256": digest(manifest),
            "script_tests": len(inv["groups"]["scripts"]),
            "enhanced_tests": len(inv["groups"]["enhanced"]),
            "report_sha256": {key: digest(value) for key, value in reports.items()},
            "qualified_for_deployment": True}


def aggregate(args):
    context = identity()
    files = list(Path(args.reports).rglob("*.json"))
    reports = {}
    for path in files:
        report = load(path)
        require(report["owner"] not in reports, "duplicate shard report artifact")
        reports[report["owner"]] = report
    qualified = {"artifact_id": args.artifact_id, "manifest_sha256": args.manifest_sha,
                 "artifact_name": artifact_name("inputs", context), "bundle_sha256": args.bundle_sha}
    gate = reconcile(args.directory, reports, context, qualified, json.loads(os.environ["DOC_NEEDS"]))
    write(args.output, gate)
    if os.environ.get("GITHUB_OUTPUT"):
        with open(os.environ["GITHUB_OUTPUT"], "a") as out:
            out.write(f"gate_sha={file_sha(args.output)}\n")


def deployment(args):
    context = identity()
    require(context.get("qualification_mode") == "hosted", "local qualification cannot deploy")
    require(file_sha(args.gate) == args.gate_sha, "deployment gate digest mismatch")
    gate = load(args.gate)
    manifest, inv = verify_inputs(args.directory, context, args.manifest_sha)
    require(gate["qualified_for_deployment"] is True and gate["identity"] == context,
            "unqualified deployment gate")
    require(gate["manifest_sha256"] == digest(manifest) and
            gate["inventory_sha256"] == digest(inv), "unqualified deployment inputs")
    require(gate["qualified"] == {
        "artifact_id": args.artifact_id, "manifest_sha256": args.manifest_sha,
        "artifact_name": artifact_name("inputs", context), "bundle_sha256": args.bundle_sha},
        "deployment artifact identity mismatch")
    require(set(gate["report_sha256"]) == set(SHARDS), "unqualified shard gate")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("inventory", "run", "seal", "unpack", "aggregate", "deployment"))
    parser.add_argument("--directory", default="qualification")
    parser.add_argument("--output")
    parser.add_argument("--owner", choices=("scripts", *SHARDS))
    parser.add_argument("--bundle")
    parser.add_argument("--bundle-sha")
    parser.add_argument("--manifest-sha")
    parser.add_argument("--artifact-id")
    parser.add_argument("--reports")
    parser.add_argument("--gate")
    parser.add_argument("--gate-sha")
    args = parser.parse_args()
    if args.command == "inventory":
        write(Path(args.directory) / "inventory.json", inventory())
    else:
        {"run": run_tests, "seal": seal, "unpack": unpack,
         "aggregate": aggregate, "deployment": deployment}[args.command](args)


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, TypeError, OSError, json.JSONDecodeError) as error:
        sys.exit("documentation qualification rejected: " + str(error))
