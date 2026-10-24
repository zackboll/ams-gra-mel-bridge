"""Additive CI-001 controls: exact identities, job failures and artifact boundaries."""

import copy
import argparse
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import tarfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import documentation_ci as ci
import measure_documentation_ci as measurements


class ReconciliationTests(unittest.TestCase):
    def setUp(self):
        self.context = {"source_sha": "a" * 40, "source_tree": "b" * 40,
                        "run_id": "42", "run_attempt": "2", "repository": "owner/repo"}
        self.inv = {"groups": {"scripts": ["script.C.test_one"],
                               "enhanced": [f"{s}.C.test_one" for s in ci.SHARDS]},
                    "shards": {s: [f"{s}.C.test_one"] for s in ci.SHARDS}}
        self.manifest = {"identity": self.context, "payload": {"_site-published/index.html": "abc"}}
        self.qualified = {"artifact_id": "123", "manifest_sha256": ci.digest(self.manifest),
                          "artifact_name": ci.artifact_name("inputs", self.context),
                          "bundle_sha256": "c" * 64}
        self.reports = {shard: self.report(shard, ids) for shard, ids in self.inv["shards"].items()}
        self.needs = {job: {"result": "success"} for job in ("prepare-documentation", "enhanced-tests")}
        patcher = mock.patch.object(ci, "verify_inputs", return_value=(self.manifest, self.inv))
        patcher.start()
        self.addCleanup(patcher.stop)

    def report(self, owner, ids):
        return {"schema": 1, "owner": owner, "identity": copy.deepcopy(self.context),
                "artifact_name": ci.artifact_name("result-" + owner, self.context),
                "inventory_sha256": ci.digest(self.inv), "qualified": self.qualified,
                "successful": True, "tests_run": len(ids),
                "events": [{"event": event, "id": name, **({"seconds": 0.1} if event == "stop" else {})}
                           for name in ids for event in ("start", "success", "stop")]}

    def gate(self):
        return ci.reconcile("unused", self.reports, self.context, self.qualified, self.needs)

    def first(self):
        return self.reports["navigation"]

    def reject(self):
        with self.assertRaises((ValueError, KeyError, TypeError)):
            self.gate()

    def test_complete_gate(self):
        self.assertTrue(self.gate()["qualified_for_deployment"])

    def test_missing_shard(self):
        del self.reports["navigation"]
        self.reject()

    def test_failed_shard(self):
        self.needs["enhanced-tests"]["result"] = "failure"
        self.reject()

    def test_cancelled_shard(self):
        self.needs["enhanced-tests"]["result"] = "cancelled"
        self.reject()

    def test_skipped_shard(self):
        self.needs["enhanced-tests"]["result"] = "skipped"
        self.reject()

    def test_failed_preparation(self):
        self.needs["prepare-documentation"]["result"] = "failure"
        self.reject()

    def test_missing_job(self):
        del self.needs["enhanced-tests"]
        self.reject()

    def test_unknown_job(self):
        self.needs["unqualified"] = {"result": "success"}
        self.reject()

    def test_unexpected_skip(self):
        self.first()["events"][1]["event"] = "skip"
        self.reject()

    def test_test_runs_twice(self):
        self.first()["events"] *= 2
        self.reject()

    def test_new_test_omitted(self):
        self.inv["shards"]["navigation"].append("navigation.C.test_new")
        for report in self.reports.values():
            report["inventory_sha256"] = ci.digest(self.inv)
        self.first()["tests_run"] += 1
        self.reject()

    def test_missing_execution_even_with_correct_total(self):
        self.first()["events"] = []
        self.reject()

    def test_unknown_test(self):
        self.first()["events"][0]["id"] = "unknown.C.test_new"
        self.reject()

    def test_forged_total(self):
        self.first()["tests_run"] = 497
        self.reject()

    def test_boolean_total(self):
        self.first()["tests_run"] = True
        self.reject()

    def test_false_success(self):
        self.first()["successful"] = False
        self.reject()

    def test_wrong_commit(self):
        self.first()["identity"]["source_sha"] = "d" * 40
        self.reject()

    def test_wrong_tree(self):
        self.first()["identity"]["source_tree"] = "d" * 40
        self.reject()

    def test_wrong_run(self):
        self.first()["identity"]["run_id"] = "43"
        self.reject()

    def test_wrong_attempt(self):
        self.first()["identity"]["run_attempt"] = "1"
        self.reject()

    def test_wrong_artifact_name(self):
        self.first()["artifact_name"] = "previous-run-result"
        self.reject()

    def test_wrong_input_artifact_id(self):
        self.first()["qualified"] = dict(self.qualified, artifact_id="999")
        self.reject()

    def test_malformed_report(self):
        del self.first()["events"]
        self.reject()

    def test_reordered_events(self):
        self.first()["events"].reverse()
        self.reject()


class InventoryTests(unittest.TestCase):
    def test_source_discovery_retains_all_inherited_tests(self):
        inv = ci.inventory()
        original = ci.load(ci.INHERITED)
        self.assertEqual(len(original["scripts"]), 133)
        self.assertEqual(len(original["enhanced"]), 364)
        for group in original:
            self.assertLessEqual(set(original[group]), set(inv["groups"][group]))
        assigned = [name for ids in inv["shards"].values() for name in ids]
        self.assertCountEqual(assigned, inv["groups"]["enhanced"])
        self.assertEqual(len(assigned), len(set(assigned)))

    def test_new_unowned_module(self):
        with self.assertRaisesRegex(ValueError, "exactly one"):
            ci.assign(["test_new.C.test_new"])

    def test_duplicate_assignment(self):
        with self.assertRaisesRegex(ValueError, "exactly one"):
            ci.assign(["test_new.C.test_new"], {"one": ["test_new"], "two": ["test_new"]})

    def test_empty_shard(self):
        with self.assertRaisesRegex(ValueError, "empty shard"):
            ci.assign(["test_new.C.test_new"], {"one": ["test_new"], "two": ["other"]})

    def test_duplicate_test_identity(self):
        with self.assertRaisesRegex(ValueError, "duplicate assignment"):
            ci.assign(["test_new.C.test_new"] * 2, {"one": ["test_new"]})

    def test_missing_result_artifact(self):
        with self.assertRaises(FileNotFoundError):
            ci.load(Path(tempfile.gettempdir()) / "ci001-nonexistent-report.json")

    def test_malformed_json(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "report.json"
            path.write_text("{broken")
            with self.assertRaises(json.JSONDecodeError):
                ci.load(path)

    def test_duplicate_json_keys(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "report.json"
            path.write_text('{"successful":false,"successful":true}')
            with self.assertRaisesRegex(ValueError, "duplicate JSON"):
                ci.load(path)


class ArtifactTests(unittest.TestCase):
    def test_changed_publication_artifact(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            context = {"source_sha": "a" * 40}
            ci.write(root / "manifest.json", {"schema": 1, "identity": context,
                                               "payload": {"index.html": "old"}})
            with mock.patch.object(ci, "payload_hashes", return_value={"index.html": "new"}):
                with self.assertRaisesRegex(ValueError, "bytes changed"):
                    ci.verify_inputs(root, context)

    def test_changed_bundle(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "bundle.tar"
            path.write_bytes(b"changed")
            args = type("Args", (), {"bundle": path, "bundle_sha": "wrong"})()
            with self.assertRaisesRegex(ValueError, "bundle digest"):
                ci.unpack(args)

    def test_unqualified_deployment_artifact(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "gate.json"
            ci.write(path, {"qualified_for_deployment": True})
            args = type("Args", (), {"gate": path, "gate_sha": "wrong"})()
            with mock.patch.object(ci, "identity", return_value={"qualification_mode": "hosted"}):
                with self.assertRaisesRegex(ValueError, "gate digest"):
                    ci.deployment(args)

    def test_symlink_payload_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "_site").mkdir()
            (root / "_site/link").symlink_to("/etc/passwd")
            with mock.patch.object(ci, "ROOT", root):
                with self.assertRaisesRegex(ValueError, "symlink"):
                    ci.payload_hashes()

    def test_archive_traversal_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "bundle.tar"
            with tarfile.open(path, "w") as archive:
                member = tarfile.TarInfo("_site/../../escape")
                member.size = 1
                archive.addfile(member, io.BytesIO(b"x"))
            args = argparse.Namespace(bundle=path, bundle_sha=ci.file_sha(path))
            with self.assertRaisesRegex(ValueError, "unsafe"):
                ci.unpack(args)

    def test_archive_symlink_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "bundle.tar"
            with tarfile.open(path, "w") as archive:
                member = tarfile.TarInfo("_site/link")
                member.type = tarfile.SYMTYPE
                member.linkname = "/etc/passwd"
                archive.addfile(member)
            args = argparse.Namespace(bundle=path, bundle_sha=ci.file_sha(path))
            with self.assertRaisesRegex(ValueError, "unsafe"):
                ci.unpack(args)

    def test_valid_digest_cannot_authorize_wrong_deployment_identity(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "gate.json"
            ci.write(path, {"qualified_for_deployment": True, "identity": {"source_sha": "wrong"}})
            args = argparse.Namespace(gate=path, gate_sha=ci.file_sha(path), directory=temp,
                                      manifest_sha="expected")
            with mock.patch.object(ci, "identity", return_value={"source_sha": "expected", "qualification_mode": "hosted"}), \
                    mock.patch.object(ci, "verify_inputs", return_value=({}, {})):
                with self.assertRaisesRegex(ValueError, "unqualified deployment gate"):
                    ci.deployment(args)

    def test_local_qualification_cannot_authorize_deployment(self):
        with mock.patch.object(ci, "identity", return_value={"qualification_mode": "local-not-deployable"}):
            with self.assertRaisesRegex(ValueError, "local qualification cannot deploy"):
                ci.deployment(argparse.Namespace())


class RunnerTests(unittest.TestCase):
    def execute(self, method):
        case = type("Case", (unittest.TestCase,), {"test_control": method})
        result = unittest.TextTestRunner(stream=io.StringIO(), resultclass=ci.EvidenceResult).run(
            unittest.TestSuite([case("test_control")]))
        return result

    def test_success_has_actual_start_success_stop(self):
        result = self.execute(lambda self: self.assertEqual(1, 1))
        self.assertEqual([e["event"] for e in result.events], ["start", "success", "stop"])

    def test_actual_skip_is_recorded(self):
        result = self.execute(lambda self: self.skipTest("unexpected"))
        self.assertEqual([e["event"] for e in result.events], ["start", "skip", "stop"])

    def test_actual_subtest_failure_is_recorded(self):
        def test(case):
            with case.subTest(control="fail"):
                case.assertEqual(1, 2)
        result = self.execute(test)
        self.assertIn("subtest-failure", [e["event"] for e in result.events])

    def test_class_setup_failure_has_no_forged_execution(self):
        class Broken(unittest.TestCase):
            @classmethod
            def setUpClass(cls):
                raise RuntimeError("setup failed")

            def test_never(self):
                self.fail("should not execute")
        result = unittest.TextTestRunner(stream=io.StringIO(), resultclass=ci.EvidenceResult).run(
            unittest.TestSuite([Broken("test_never")]))
        self.assertEqual(result.testsRun, 0)
        self.assertEqual([e["event"] for e in result.events], ["error"])


class WorkflowTests(unittest.TestCase):
    def test_required_build_always_evaluates(self):
        text = (ci.ROOT / ".github/workflows/documentation.yml").read_text()
        self.assertIn("  build:\n    if: always()\n    needs: [prepare-documentation, enhanced-tests]", text)
        self.assertIn("fail-fast: false", text)

    def test_deploy_is_main_only_and_aggregate_authorized(self):
        text = (ci.ROOT / ".github/workflows/documentation.yml").read_text()
        deploy = text.split("\n  deploy:\n")[1].split("\n  verify-live:\n")[0]
        self.assertIn("if: github.ref == 'refs/heads/main' && github.event_name != 'pull_request'", deploy)
        self.assertIn("needs: [prepare-documentation, build]", deploy)
        self.assertLess(deploy.index("documentation_ci.py deployment"), deploy.index("actions/upload-pages-artifact"))
        prepare = text.split("\n  enhanced-tests:\n")[0]
        self.assertNotIn("actions/upload-pages-artifact", prepare)
        self.assertNotIn("actions/deploy-pages", prepare)


class MeasurementTests(unittest.TestCase):
    def test_duration_and_skipped_timestamp_clamping(self):
        self.assertEqual(measurements.seconds("2026-10-21T00:00:00Z", "2026-10-21T00:01:00Z"), 60)
        self.assertEqual(measurements.seconds("2026-10-21T00:00:01Z", "2026-10-21T00:00:00Z"), 0)
        self.assertIsNone(measurements.seconds(None, "2026-10-21T00:00:00Z"))

    def test_complete_measurement_records_critical_path_and_runner_minutes(self):
        def job(name, start, end):
            return {"id": name, "name": name, "status": "completed", "conclusion": "success",
                    "created_at": "2026-10-21T00:00:00Z", "started_at": start,
                    "completed_at": end, "steps": []}
        jobs = [job("prepare-documentation", "2026-10-21T00:01:00Z", "2026-10-21T00:06:00Z"),
                job("build", "2026-10-21T00:17:00Z", "2026-10-21T00:18:00Z")]
        jobs += [job("enhanced-tests (" + name + ")", "2026-10-21T00:06:00Z", "2026-10-21T00:16:00Z")
                 for name in ci.SHARDS]
        run = {"run_attempt": 1, "head_sha": "a" * 40, "status": "completed", "conclusion": "success",
               "html_url": "https://github.com/owner/repo/actions/runs/42", "created_at": "2026-10-21T00:00:00Z"}
        with mock.patch.object(measurements, "api", side_effect=[run, {"jobs": jobs}, {"artifacts": []}]):
            result = measurements.collect("owner/repo", 42)
        self.assertEqual(result["qualification_critical_path_seconds_excluding_queues"], 960)
        self.assertEqual(result["completed_runner_minutes"], 66)
        self.assertEqual(result["observed_wall_seconds"], 1080)
        self.assertTrue(result["qualification_complete"])

    def test_pending_measurement_cannot_claim_complete_qualification(self):
        run = {"run_attempt": 1, "head_sha": "a" * 40, "status": "queued", "conclusion": None,
               "html_url": "https://github.com/owner/repo/actions/runs/42", "created_at": "2026-10-21T00:00:00Z"}
        with mock.patch.object(measurements, "api", side_effect=[run, {"jobs": []}, {"artifacts": []}]):
            result = measurements.collect("owner/repo", 42)
        self.assertFalse(result["qualification_complete"])
        self.assertIsNone(result["baseline_speedup"])
        self.assertIsNone(result["qualification_critical_path_seconds_excluding_queues"])


class ReviewedTransitionTests(unittest.TestCase):
    """Isolated candidates; authorities always come from immutable Git objects."""

    @classmethod
    def setUpClass(cls):
        cls.trusted = {}
        for revision in (ci.BASELINE_SHA, ci.CI001_SHA, ci.IR_CI003_SHA):
            names = subprocess.check_output(
                ["git", "-C", str(ci.ROOT), "ls-tree", "-r", "--name-only", revision],
                text=True).splitlines()
            cls.trusted[revision] = {
                name: subprocess.check_output(["git", "-C", str(ci.ROOT), "show", f"{revision}:{name}"])
                for name in names if ((name.startswith("docs/tests/test_") or
                                       name.startswith("scripts/test_")) and name.endswith(".py"))
            }
        cls.inherited_bytes = subprocess.check_output(
            ["git", "-C", str(ci.ROOT), "show",
             ci.CI001_SHA + ":docs/tests/ci001-inherited-inventory.json"])
        cls.discovered = {group: sorted(test.id() for test in ci.discover(group))
                          for group in ("scripts", "enhanced")}

    def setUp(self):
        workspace = tempfile.TemporaryDirectory(prefix="ci001r2-source-")
        self.addCleanup(workspace.cleanup)
        self.root = Path(workspace.name)
        sources = dict(self.trusted[ci.BASELINE_SHA])
        sources.update({name: self.trusted[ci.IR_CI003_SHA][name] for name in ci.IR_CI003_SOURCES})
        sources["docs/tests/test_live_docs_site.py"] = self.trusted[ci.CI001_SHA]["docs/tests/test_live_docs_site.py"]
        sources["docs/tests/test_documentation_ci.py"] = (ci.ROOT / "docs/tests/test_documentation_ci.py").read_bytes()
        sources["docs/tests/ci001-inherited-inventory.json"] = self.inherited_bytes
        for name, content in sources.items():
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)
        self.groups = copy.deepcopy(self.discovered)

    def validate(self):
        return ci.validate_inventory(self.groups, self.root)

    def test_original_497_remain_admitted(self):
        inherited = self.validate()
        self.assertEqual({g: len(ids) for g, ids in inherited.items()}, {"scripts": 133, "enhanced": 364})
        for group, ids in inherited.items():
            self.assertLessEqual(set(ids), set(self.groups[group]))

    def test_exact_reviewed_transition_admitted(self):
        self.validate()
        for name in ci.IR_CI003_SOURCES:
            self.assertEqual((self.root / name).read_bytes(), self.trusted[ci.IR_CI003_SHA][name])
        self.assertEqual(sum(map(len, self.groups.values())), 579)

    def test_nine_policy_additions_discovered(self):
        inherited = json.loads(self.inherited_bytes)
        additions = set(self.groups["scripts"]) - set(inherited["scripts"])
        self.assertEqual(len(additions), 9)
        self.assertTrue(all(name.startswith("test_doc_source_integrity.") for name in additions))
        self.validate()

    def test_twelve_c_observers_discovered(self):
        observers = [name for name in self.groups["enhanced"]
                     if name.startswith("test_navigation_completion_observation.")]
        self.assertEqual(len(observers), 12)
        self.validate()

    def test_new_identities_have_exactly_one_owner(self):
        assignments = ci.assign(self.groups["enhanced"])
        self.assertEqual({s: len(ids) for s, ids in assignments.items()}, {
            "navigation": 34, "publication": 35, "live": 72,
            "symbols-catalog": 57, "types": 52, "remaining": 187})
        owners = self.groups["scripts"] + [name for ids in assignments.values() for name in ids]
        self.assertEqual(len(owners), len(set(owners)))
        observers = {name for name in self.groups["enhanced"]
                     if name.startswith("test_navigation_completion_observation.")}
        self.assertLessEqual(observers, set(assignments["remaining"]))

    def test_missing_reviewed_test_rejected(self):
        for group, prefix in (("scripts", "test_doc_source_integrity.NavigationTransitionTests."),
                              ("enhanced", "test_navigation_completion_observation.")):
            with self.subTest(group=group):
                saved = self.groups[group][:]
                name = next(name for name in saved if name.startswith(prefix))
                self.groups[group].remove(name)
                with self.assertRaisesRegex(ValueError, "identity missing"):
                    self.validate()
                self.groups[group] = saved

    def test_duplicate_reviewed_test_rejected(self):
        self.groups["enhanced"].append(next(name for name in self.groups["enhanced"]
                                           if name.startswith("test_navigation_completion_observation.")))
        with self.assertRaisesRegex(ValueError, "duplicate discovered"):
            self.validate()
        name = "docs/tests/test_navigation_completion_observation.py"
        path = self.root / name
        path.write_bytes(path.read_bytes() + b"\nclass NavigationObservationTests: pass\n")
        with self.assertRaisesRegex(ValueError, "duplicate test class"):
            ci.source_methods(path.read_bytes(), Path(name).stem)

    def test_changed_doc012_assertion_rejected(self):
        path = self.root / "docs/tests/test_ada_examples.py"
        original = path.read_bytes()
        changed = original.replace(b"self.assertEqual(", b"self.assertNotEqual(", 1)
        self.assertNotEqual(original, changed)
        path.write_bytes(changed)
        with self.assertRaisesRegex(ValueError, "inherited test assertions changed"):
            self.validate()

    def test_changed_approved_assertion_rejected(self):
        for name in ci.IR_CI003_SOURCES:
            with self.subTest(source=name):
                path = self.root / name
                original = path.read_bytes()
                changed = original.replace(b"self.assertEqual(", b"self.assertNotEqual(", 1)
                self.assertNotEqual(original, changed)
                path.write_bytes(changed)
                with self.assertRaisesRegex(ValueError, "approved IR-CI-003 assertions changed"):
                    self.validate()
                path.write_bytes(original)

    def test_self_consistent_forged_inventory_rejected(self):
        name = "docs/tests/test_navigation_completion_observation.py"
        path = self.root / name
        path.write_bytes(path.read_bytes().replace(b"test_normal_delayed_observation", b"test_forged_observation"))
        self.groups["enhanced"] = [identity.replace("test_normal_delayed_observation", "test_forged_observation")
                                   for identity in self.groups["enhanced"]]
        forged = {"groups": self.groups, "shards": ci.assign(self.groups["enhanced"]),
                  "test_sources": {name: ci.file_sha(path)}}
        ci.write(self.root / "forged.json", forged)
        self.assertEqual(ci.load(self.root / "forged.json"), forged)
        with self.assertRaisesRegex(ValueError, "approved IR-CI-003 assertions changed"):
            self.validate()
        path.write_bytes(self.trusted[ci.IR_CI003_SHA][name])
        (self.root / "docs/tests/ci001-inherited-inventory.json").write_bytes(ci.canonical(self.groups))
        with self.assertRaisesRegex(ValueError, "inherited inventory changed"):
            self.validate()

    def test_wrong_authority_or_unapproved_ancestry_rejected(self):
        with self.assertRaisesRegex(ValueError, "unapproved source authority"):
            ci.reviewed_authorities(ci.IR_CI003_SHA, ci.IR_CI003_SHA, ci.CI001_SHA)
        with tempfile.TemporaryDirectory(prefix="ci001r2-unapproved-") as temp:
            subprocess.run(["git", "init", "-q", temp], check=True)
            subprocess.run(["git", "-C", temp, "fetch", "--quiet", str(ci.ROOT),
                            ci.BASELINE_SHA, ci.IR_CI003_SHA, ci.CI001_SHA], check=True)
            subprocess.run(["git", "-C", temp, "checkout", "--quiet", "--detach", ci.BASELINE_SHA], check=True)
            with mock.patch.object(ci, "ROOT", Path(temp)):
                with self.assertRaisesRegex(ValueError, "not in current ancestry"):
                    ci.reviewed_authorities(ci.BASELINE_SHA, ci.IR_CI003_SHA, ci.CI001_SHA)

    def test_non_successful_dependencies_cannot_authorize_aggregate(self):
        for job in ("prepare-documentation", "enhanced-tests"):
            for result in ("failure", "cancelled", "skipped", None):
                with self.subTest(job=job, result=result):
                    needs = {name: {"result": "success"} for name in ("prepare-documentation", "enhanced-tests")}
                    needs[job]["result"] = result
                    with self.assertRaisesRegex(ValueError, "required job failed"):
                        ci.reconcile(self.root, {}, {}, {}, needs)
            needs = {name: {"result": "success"} for name in ("prepare-documentation", "enhanced-tests") if name != job}
            with self.assertRaisesRegex(ValueError, "missing required jobs"):
                ci.reconcile(self.root, {}, {}, {}, needs)


if __name__ == "__main__":
    unittest.main()
