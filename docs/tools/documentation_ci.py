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
                  "test_examples", "test_documentation_ci",
                  "test_navigation_completion_observation", "test_bootstrap_ci_policy"],
}
INHERITED = ROOT / "docs/tests/ci001-inherited-inventory.json"
CI002_TEST_SHA256 = "7ce1f723cda1a8566078df386eb0765dd846d1996b00a9a9a5b22c05c1f597ba"
BASELINE_SHA = "9288f6acd4cbf807f576d387db390459f0b117c0"
CI001_SHA = "a0900430d3b2db8ac5dd9d065eaa66aa88ed6751"
IR_CI003_SHA = "45f0d147521354777a8ace214172ff268e1c3355"
REVIEWED_COMMITS = {
    BASELINE_SHA: ("78bc1f5b5087847ff385debc60340b14461c20b7",
                   "2dd865ca09a3fb3e106744f7991b115cec227fca f5e5b896b7eeae26ba9131d865764ddd3b63c04d"),
    CI001_SHA: ("24362178fba874029724ee7a645164f6087c2b3e", BASELINE_SHA),
    IR_CI003_SHA: ("158ee2f4e04b1f2ecf9de33bebd1c5edb61866e4",
                  BASELINE_SHA + " 27b26cbf5f8b683deeaa731c74c7e37dd4a429a3"),
}
IR_CI003_SOURCES = {
    "scripts/test_doc_source_integrity.py":
        "1ad3dd314e0b5ef1411a1438b9897a8e7e9c66717b10a4f6776409cb4bfd7e13",
    "docs/tests/test_navigation_completion_observation.py":
        "9155fd124ee69ed0e2a7345d35369c786e885739012d0cd186d940dec2793c72",
}
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


def git_bytes(revision, name):
    return subprocess.check_output(["git", "-C", str(ROOT), "show", f"{revision}:{name}"])


def reviewed_authorities(baseline, transition, implementation):
    require(baseline == "9288f6acd4cbf807f576d387db390459f0b117c0" and
            transition == "45f0d147521354777a8ace214172ff268e1c3355" and
            implementation == "a0900430d3b2db8ac5dd9d065eaa66aa88ed6751",
            "unapproved source authority")
    for revision in (baseline, transition, implementation):
        tree, parents = REVIEWED_COMMITS[revision]
        require(git("show", "-s", "--format=%H%n%T%n%P", revision).splitlines() ==
                [revision, tree, parents], "reviewed commit identity/tree/parents mismatch")
        require(subprocess.run(["git", "-C", str(ROOT), "merge-base", "--is-ancestor",
                                revision, "HEAD"], check=False).returncode == 0,
                "reviewed source authority not in current ancestry: " + revision)


def source_methods(content, module):
    """Static identities for the two reviewed (non-generated) transitions."""
    methods = {}
    classes = set()
    for node in ast.parse(content).body:
        if isinstance(node, ast.ClassDef):
            require(node.name not in classes, "duplicate test class: " + node.name)
            classes.add(node.name)
            for method in node.body:
                if isinstance(method, (ast.FunctionDef, ast.AsyncFunctionDef)) and method.name.startswith("test_"):
                    name = f"{module}.{node.name}.{method.name}"
                    require(name not in methods, "duplicate test source identity: " + name)
                    methods[name] = ast.dump(method)
    return methods


def live_fixture_source(content):
    """Exact DOC-PERF-001-R2 helper proposal; pending independent review.

    Reconstruct from immutable CI-001 bytes, not a candidate-supplied digest.
    Both historical fixtures and the proposed successor retain every other byte,
    including all test methods and the reviewed WorkflowTests implementation.
    Mechanical acceptance is not approval of this proposed transition.
    """
    original = git_bytes(CI001_SHA, "docs/tests/test_live_docs_site.py")
    require(hashlib.sha256(original).hexdigest() ==
            "0658a2e40b8e815e71a9de61e077b7b0a122f74f23ba385d363f80ebfc0b6164",
            "live fixture historical source mismatch")
    old = '''    def api_page(self):
        return next(p for p in (self.site / "api").glob("*.html") if p.name != "index.html")
'''.encode()
    new = '''    def api_page(self):
        authority = live.publication.Authority(ROOT, ROOT / "_site")
        required = sorted(authority.required_additions(), key=live.publication.occurrence_key)
        self.assertTrue(required, "missing trusted fixture occurrence")
        record = required[0]
        entry = record["containing"]
        self.assertEqual(authority.sections[(entry["page"], entry["anchor"])], entry)
        path = self.site / "api" / entry["page"]
        native = (ROOT / "_site/api" / entry["page"]).read_text()
        text = live.publication.restore_document(path.read_text(), native,
                                                "api/" + entry["page"], authority)
        restored, audit = live.publication.restore_page(entry["page"], text, authority)
        self.assertIn("<h4 id=" + entry["anchor"] + ">", native)
        self.assertIn(record, audit, "missing trusted fixture type-reference link")
        self.assertEqual(restored, native, "changed trusted fixture native declaration")
        return path
'''.encode()
    require(original.count(old) == 1, "live fixture historical helper mismatch")
    proposed = original.replace(old, new, 1)
    require(hashlib.sha256(proposed).hexdigest() ==
            "4693fe458c1fae6dffc5d9d115e21dc9a5ff3a74380a0cd16ac22c4181444c74",
            "live fixture proposed reconstruction mismatch")
    require(content in (original, proposed), "unapproved inherited live fixture source")
    return original


def validate_inventory(groups, root=None):
    root = ROOT if root is None else Path(root)
    # CI-002 is additive. Reconcile its exact source-derived identities before
    # applying the unchanged historical 579-test reconciliation below. Minimal
    # CI-001R2 fixtures omit this module and continue testing that boundary.
    ci002_name = "docs/tests/test_bootstrap_ci_policy.py"
    ci002_path = root / ci002_name
    ci002 = set()
    if ci002_path.exists():
        content = ci002_path.read_bytes()
        require(hashlib.sha256(content).hexdigest() == CI002_TEST_SHA256,
                "CI-002 control source changed")
        ci002 = set(source_methods(content, "test_bootstrap_ci_policy"))
        require(len(ci002) == 71, "CI-002 control inventory changed")
        original_ci002 = source_methods(git_bytes(
            "cec2a98ee90cbf532c66ddf8c1ff13e86b6b6ae0", ci002_name), "test_bootstrap_ci_policy")
        require(len(original_ci002) == 40 and set(original_ci002) <= ci002,
                "original CI-002 identity removed")
        require(set(groups["enhanced"]) >= ci002 and
                all(groups["enhanced"].count(name) == 1 for name in ci002),
                "CI-002 control missing or duplicated")
    else:
        require(root != ROOT, "CI-002 control source missing")
    groups = {group: [name for name in names if name not in ci002]
              for group, names in groups.items()}
    reviewed_authorities(BASELINE_SHA, IR_CI003_SHA, CI001_SHA)
    inventory_name = "docs/tests/ci001-inherited-inventory.json"
    inherited_bytes = git_bytes(CI001_SHA, inventory_name)
    require((root / inventory_name).read_bytes() == inherited_bytes, "inherited inventory changed")
    inherited = json.loads(inherited_bytes)
    approved = {}
    for name, expected_hash in IR_CI003_SOURCES.items():
        original = git_bytes(IR_CI003_SHA, name)
        require(hashlib.sha256(original).hexdigest() == expected_hash,
                "reviewed IR-CI-003 source hash mismatch: " + name)
        require((root / name).read_bytes() == original, "approved IR-CI-003 assertions changed: " + name)
        approved[name] = source_methods(original, Path(name).stem)
    policy_name = "scripts/test_doc_source_integrity.py"
    policy_original = source_methods(git_bytes(BASELINE_SHA, policy_name), Path(policy_name).stem)
    require(len(policy_original) == 51 and len(approved[policy_name]) == 60 and
            policy_original.items() <= approved[policy_name].items(),
            "original 51 source-integrity assertions changed")
    observer_name = "docs/tests/test_navigation_completion_observation.py"
    require(len(approved[observer_name]) == 12, "reviewed observer inventory changed")
    additions = {"scripts": set(approved[policy_name]) - set(policy_original),
                 "enhanced": set(approved[observer_name])}
    require(len(additions["scripts"]) == 9, "reviewed nine policy tests changed")
    # Keep the exact inherited test sources and assertions, not only their names.
    baseline_sources = git("ls-tree", "-r", "--name-only", BASELINE_SHA).splitlines()
    protected = set()
    for name in baseline_sources:
        path = Path(name)
        if ((path.parent == Path("docs/tests") and path.match("test_*.py")) or
                (path.parent == Path("scripts") and path.match("test_*doc*.py"))):
            protected.add(name)
            original = git_bytes(BASELINE_SHA, name)
            current = (root / name).read_bytes()
            if name in IR_CI003_SOURCES:
                continue  # Exact approved bytes and original methods verified above.
            if name == "docs/tests/test_live_docs_site.py":
                current = live_fixture_source(current)
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
    controls_name = "docs/tests/test_documentation_ci.py"
    original_controls = source_methods(git_bytes(CI001_SHA, controls_name), Path(controls_name).stem)
    controls = source_methods((root / controls_name).read_bytes(), Path(controls_name).stem)
    require(len(original_controls) == 49 and original_controls.items() <= controls.items(),
            "original CI-001 controls changed")
    original_tree = ast.parse(git_bytes(CI001_SHA, controls_name))
    current_tree = ast.parse((root / controls_name).read_bytes())
    current_tree.body = [node for node in current_tree.body if not (
        (isinstance(node, ast.ClassDef) and node.name == "ReviewedTransitionTests") or
        (isinstance(node, ast.Import) and len(node.names) == 1 and
         node.names[0].name == "subprocess" and node.names[0].asname is None))]
    require(ast.dump(current_tree) == ast.dump(original_tree), "original CI-001 control helpers changed")
    require(len(controls) == 61, "CI-001R2 control inventory changed")
    source_names = {p.relative_to(root).as_posix()
                    for directory, pattern in (("scripts", "test_*doc*.py"), ("docs/tests", "test_*.py"))
                    for p in (root / directory).glob(pattern)}
    require(source_names - ({ci002_name} if ci002 else set()) ==
            protected | {controls_name, observer_name}, "unauthorized test source addition/removal")
    require(set(groups) == {"scripts", "enhanced"}, "unexpected inventory groups")
    for group, ids in groups.items():
        require(len(ids) == len(set(ids)), "duplicate discovered identity: " + group)
        require(len(inherited[group]) == len(set(inherited[group])), "duplicate inherited identity")
        require(set(inherited[group]) <= set(ids), "inherited test removed: " + group)
        expected = set(inherited[group]) | additions[group]
        if group == "enhanced":
            expected |= set(controls)
        require(set(ids) == expected, "reviewed test identity missing or unauthorized: " + group)
    require({key: len(value) for key, value in groups.items()} ==
            {"scripts": 142, "enhanced": 437}, "combined inventory changed")
    require(len(set(groups["scripts"] + groups["enhanced"])) == 579,
            "duplicate cross-group identity")
    return inherited


def inventory():
    groups = {name: sorted(test.id() for test in discover(name))
              for name in ("scripts", "enhanced")}
    validate_inventory(groups)
    shards = assign(groups["enhanced"])
    require({key: len(value) for key, value in shards.items()} == {
        "navigation": 34, "publication": 35, "live": 72,
        "symbols-catalog": 57, "types": 52, "remaining": 258}, "reviewed shard inventory changed")
    return {"schema": 1, "groups": groups, "shards": shards,
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
