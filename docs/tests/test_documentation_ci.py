"""Additive CI-001 controls: exact identities, job failures and artifact boundaries."""

import copy
import argparse
import io
import json
from pathlib import Path
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


if __name__ == "__main__":
    unittest.main()
