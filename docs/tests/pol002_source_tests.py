"""Independent committed Git fixtures for candidate scope, never feature approval.

Separate explicit discovery preserves the immutable 579-test historical suite.
"""

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / "docs/tools"), str(ROOT / "scripts")]
import feature_source as policy


class SourceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="pol002-git-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.git("init", "-q")
        self.git("config", "user.name", "POL002 Fixture")
        self.git("config", "user.email", "pol002@example.invalid")
        for name in ("native/src/rf_product_rx.cpp", "native/src/ir_stream.cpp",
                     "ada/src/ams-mel-rf-product_rx.ads", "native/vendor/pin.h",
                     "docs/tools/feature_source.py", "docs/coverage.md"):
            self.write(name, "original content\n")
        self.base = self.commit()

    def git(self, *args):
        return subprocess.check_output(["git", "-C", str(self.root), *args], stderr=subprocess.STDOUT).decode().strip()

    def write(self, name, text):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)

    def commit(self):
        self.git("add", ".")
        self.git("commit", "-qm", "committed fixture")
        return self.git("rev-parse", "HEAD")

    def check(self):
        return policy.qualify(self.root, self.base, "HEAD")

    def reject(self):
        with self.assertRaises((ValueError, subprocess.CalledProcessError)):
            self.check()

    def test_scoped_feature_can_undergo_mechanical_qualification(self):
        self.write("native/src/rf_product_rx.cpp", "changed implementation\n")
        self.commit()
        result = self.check()
        self.assertEqual(result["case"], "feature-source")
        self.assertFalse(result["independent_approval"])
        self.assertFalse(result["publication_epoch_approved"])
        self.assertEqual([d["path"] for d in result["delta"]], ["native/src/rf_product_rx.cpp"])

    def test_documentation_only_preserves_source(self):
        self.write("docs/coverage.md", "updated prose\n")
        self.commit()
        self.assertEqual(self.check()["case"], "documentation-only")

    def test_executable_ada_not_misclassified_as_docs(self):
        self.write("ada/src/ams-mel-rf-product_rx.ads", "new executable declaration\n")
        self.commit()
        self.assertEqual(self.check()["case"], "feature-source")

    def test_documentation_only_mode_rejects_executable_change(self):
        self.write("ada/src/ams-mel-rf-product_rx.ads", "new executable declaration\n")
        self.commit()
        with self.assertRaisesRegex(ValueError, "documentation-only"):
            policy.qualify(self.root, self.base, "HEAD", documentation_only=True)

    def test_unrelated_production_rejected(self):
        self.write("native/src/ir_stream.cpp", "unrelated\n")
        self.commit()
        self.reject()

    def test_unknown_native_addition_rejected(self):
        self.write("native/src/new_rf.cpp", "new\n")
        self.commit()
        self.reject()

    def test_unknown_ada_addition_rejected(self):
        self.write("ada/src/ams-mel-new.ads", "new\n")
        self.commit()
        self.reject()

    def test_vendor_rejected(self):
        self.write("native/vendor/pin.h", "changed vendor\n")
        self.commit()
        self.reject()

    def test_checker_cannot_authorize_itself(self):
        self.write("docs/tools/feature_source.py", "RF_PRODUCT_RX = everything\n")
        self.write("native/src/rf_product_rx.cpp", "changed\n")
        self.commit()
        self.reject()

    def test_candidate_manifest_does_not_confer_authority(self):
        self.write("docs/source-authorization.json", '{"allow":"native/src/ir_stream.cpp"}\n')
        self.commit()
        self.reject()

    def test_candidate_label_does_not_confer_authority(self):
        self.git("checkout", "-qb", "feature/034f26-rf-product-rx-receive-events")
        self.write("native/src/ir_stream.cpp", "changed\n")
        self.commit()
        self.reject()

    def test_unstaged_source_disagreement_rejected(self):
        self.write("native/src/rf_product_rx.cpp", "unstaged\n")
        self.reject()

    def test_staged_source_disagreement_rejected(self):
        self.write("native/src/rf_product_rx.cpp", "staged\n")
        self.git("add", ".")
        self.reject()

    def test_partial_staging_rejected(self):
        self.write("native/src/rf_product_rx.cpp", "staged\n")
        self.git("add", ".")
        self.write("native/src/rf_product_rx.cpp", "different working bytes\n")
        self.reject()

    def test_missing_source_rejected(self):
        (self.root / "native/src/rf_product_rx.cpp").unlink()
        self.reject()

    def test_symlink_rejected(self):
        path = self.root / "native/src/rf_product_rx.cpp"
        path.unlink()
        path.symlink_to(self.root / "native/src/ir_stream.cpp")
        self.reject()

    def test_committed_symlink_rejected(self):
        path = self.root / "native/src/rf_product_rx.cpp"
        path.unlink()
        path.symlink_to("ir_stream.cpp")
        self.commit()
        self.reject()

    def test_nonregular_rejected(self):
        path = self.root / "native/src/rf_product_rx.cpp"
        path.unlink()
        os.mkfifo(path)
        self.reject()

    def test_mode_change_rejected(self):
        path = self.root / "native/src/rf_product_rx.cpp"
        path.chmod(0o755)
        self.commit()
        self.reject()

    def test_untracked_source_rejected(self):
        self.write("native/new.c", "untracked\n")
        self.reject()

    def test_source_removal_rejected(self):
        self.git("rm", "native/src/rf_product_rx.cpp")
        self.commit()
        self.reject()

    def test_wrong_target_is_not_ancestor(self):
        self.write("docs/coverage.md", "new\n")
        newer = self.commit()
        with self.assertRaises(ValueError):
            policy.qualify(self.root, newer, self.base, check_checkout=False)

    def test_new_v10_probe_is_explicit_not_wildcard(self):
        self.write("native/tests/product_rx_v10_layout_probe.c", "int main(void) { return 0; }\n")
        self.commit()
        self.assertEqual(self.check()["case"], "feature-source")

    def test_shared_header_historical_line_change_rejected(self):
        name = "native/include/ams_mel/abi.h"
        self.write(name, "old declaration;\n")
        self.base = self.commit()
        self.write(name, "replacement declaration;\n")
        self.commit()
        self.reject()

    def test_unrelated_export_rejected(self):
        name = "native/src/exports.map"
        self.write(name, "AMS_MEL_0.1 { global:\nold_symbol;\n};\n")
        self.base = self.commit()
        self.write(name, "AMS_MEL_0.1 { global:\nold_symbol;\nunrelated_ir_symbol;\n};\n")
        self.commit()
        self.reject()

    def test_exact_v10_export_addition_remains_pending_review(self):
        name = "native/src/exports.map"
        self.write(name, "AMS_MEL_0.1 { global:\nold_symbol;\n};\n")
        self.base = self.commit()
        self.write(name, "AMS_MEL_0.1 { global:\nold_symbol;\nams_mel_rf_data_submit_product_rx_v10;\n};\n")
        self.commit()
        value = self.check()
        self.assertEqual(len(value["review_obligations"]), 1)
        self.assertFalse(value["independent_approval"])

    def test_mock_edit_requires_approved_pair(self):
        self.write(policy.MOCK, "unapproved predecessor\n")
        self.write(policy.PARTNER, "unapproved partner\n")
        self.base = self.commit()
        self.write(policy.MOCK, "feature fixture\n")
        self.commit()
        self.reject()

    def test_mock_future_edit_keeps_exact_approved_predecessor_and_partner(self):
        for name in (policy.MOCK, policy.PARTNER):
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(policy.blob(ROOT, policy.HISTORICAL_MAIN, name))
        self.base = self.commit()
        path = self.root / policy.MOCK
        path.write_bytes(path.read_bytes() + b"\n/* synthetic new observation fixture */\n")
        self.commit()
        self.assertEqual(self.check()["case"], "feature-source")
        self.assertFalse(self.check()["independent_approval"])

    def test_mock_mixed_predecessor_state_is_rejected(self):
        for name in (policy.MOCK, policy.PARTNER):
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(policy.blob(ROOT, policy.HISTORICAL_MAIN, name))
        self.write(policy.PARTNER, "unapproved mixed partner\n")
        self.base = self.commit()
        path = self.root / policy.MOCK
        path.write_bytes(path.read_bytes() + b"\n/* synthetic */\n")
        self.commit()
        self.reject()


class HistoricalTests(unittest.TestCase):
    def test_full_existing_source_gate(self):
        import check_doc_source_integrity as old
        # The legacy gate protects its immutable epoch, not a future feature's
        # deliberately changed source. Retain every original assertion against
        # a real committed historical checkout, never candidate manifest bytes.
        import epoch_ci
        with tempfile.TemporaryDirectory(prefix="pol002-original-gate-") as temp:
            historical = Path(temp) / "historical"
            epoch_ci.historical_checkout(historical)
            result = old.check(historical)
        self.assertEqual(result["protected_files"], 1162)
        self.assertEqual(result["reviewed_native_tests"], "approved")

    def test_historical_pins_unchanged(self):
        import check_doc_source_integrity as old
        self.assertEqual(old.BASE, "42648a92eb0cc84b2980d16db6028992c0494ba1")
        self.assertEqual(tuple(entry[2] for entry in old.REVIEWED_NATIVE_TEST_TRANSITION), policy.APPROVED_PAIR)


class GeneratedEpochTests(unittest.TestCase):
    """Use actual pinned native output, never fabricated declaration HTML."""

    @classmethod
    def setUpClass(cls):
        import source_epoch as epoch
        cls.epoch = epoch
        cls.workspace = tempfile.TemporaryDirectory(prefix="pol002-generated-")
        cls.addClassCleanup(cls.workspace.cleanup)
        cls.directory = Path(cls.workspace.name)
        cls.repo = cls.directory / "git"
        cls.repo.mkdir()
        epoch.export(ROOT, policy.HISTORICAL_MAIN, cls.repo)
        def git(*args):
            return subprocess.check_output(["git", "-C", str(cls.repo), *args], stderr=subprocess.STDOUT).decode().strip()
        cls.git = staticmethod(git)
        git("init", "-q")
        git("config", "user.name", "POL002 Fixture")
        git("config", "user.email", "pol002@example.invalid")
        git("add", ".")
        git("commit", "-qm", "trusted synthetic target")
        cls.target = git("rev-parse", "HEAD")
        path = cls.repo / "ada/src/ams-mel-rf-product_rx.ads"
        cls.original = path.read_text()
        cls.addition = ("\n   subtype Qualification_Observation_Count is Natural;\n"
                        "   --  Count observed values without retaining a native owner.\n")
        path.write_text(cls.original.replace("\nprivate\n", cls.addition + "\nprivate\n"))
        git("add", ".")
        git("commit", "-qm", "synthetic documented public declaration")
        cls.candidate = git("rev-parse", "HEAD")
        cls.prefix = epoch.toolchain(os.environ["GNATDOC_PREFIX"])
        cls.output = cls.directory / "epoch"
        cls.value = epoch.qualify(cls.repo, cls.target, cls.candidate, cls.output, cls.prefix)

    def test_real_additive_declaration_fully_qualified(self):
        self.assertEqual(self.value["added_declarations"], 1)
        self.assertEqual((self.value["documented_elements"], self.value["total_elements"]), (4295, 4296))
        self.assertEqual(len(self.value["publication"]["symbols"]), 239)
        self.assertFalse(self.value["independent_approval"])

    def test_native_new_anchor_and_catalog_derived_from_source(self):
        symbols = self.value["publication"]["symbols"]
        added = next(e for e in symbols if e["canonical"].endswith(".Qualification_Observation_Count"))
        self.assertEqual(added["owner"], "AMS.MEL.RF.Product_Rx")
        text = (self.output / "published/guides/api-map.html").read_text()
        self.assertIn(added["page"] + "#" + added["anchor"], text)
        self.assertEqual(len(self.value["publication"]["type_occurrences"]), 1428)

    def test_old_native_destinations_retained(self):
        old = json.loads((ROOT / "docs/gnatdoc-native-anchors.json").read_text())
        self.assertTrue(all(self.value["native"].get(k) == v for k, v in old.items()))

    def test_no_automatic_publication_approval(self):
        with self.assertRaisesRegex(ValueError, "no independently reviewed"):
            self.epoch.publication_authority(self.repo, self.candidate)

    def test_wrong_source_commit_rejected(self):
        with self.assertRaises(ValueError):
            self.epoch.verify(self.repo, self.output, self.target, self.target)

    def test_wrong_manifest_source_rejected(self):
        copy = self.directory / "wrong-manifest"
        shutil.copytree(self.output, copy)
        value = json.loads((copy / "epoch.json").read_text())
        value["candidate_tree"] = "a" * 40
        (copy / "epoch.json").write_text(json.dumps(value))
        with self.assertRaisesRegex(ValueError, "source/manifest"):
            self.epoch.verify(self.repo, copy, self.target, self.candidate)

    def test_changed_html_rejected(self):
        copy = self.directory / "wrong-html"
        shutil.copytree(self.output, copy)
        page = next((copy / "published/api").glob("[a-f0-9]" * 64 + ".html"))
        page.write_text("<html><body>Fabricated incomplete page</body></html>")
        with self.assertRaisesRegex(ValueError, "artifact mismatch"):
            self.epoch.verify(self.repo, copy, self.target, self.candidate)

    def test_forged_html_and_manifest_cannot_pass_regeneration(self):
        import build_enhanced_site as p
        copy = self.directory / "forged"
        shutil.copytree(self.output, copy)
        page = next((copy / "published/api").glob("[a-f0-9]" * 64 + ".html"))
        page.write_text("<html><body>Fabricated incomplete page</body></html>")
        value = json.loads((copy / "epoch.json").read_text())
        value["publication"]["files"] = p.hashes(p.files(copy / "published"))
        (copy / "epoch.json").write_text(json.dumps(value))
        with self.assertRaisesRegex(ValueError, "independently regenerated"):
            self.epoch.verify_regenerated(self.repo, copy, self.target, self.candidate, self.prefix)

    def test_undocumented_new_declaration_rejected(self):
        # A separate committed candidate, not a mutation of the accepted export.
        repo = self.directory / "undocumented-git"
        shutil.copytree(self.repo, repo)
        path = repo / "ada/src/ams-mel-rf-product_rx.ads"
        path.write_text(self.original.replace("\nprivate\n", "\n   subtype Undocumented_Count is Natural;\n\nprivate\n"))
        subprocess.run(["git", "-C", str(repo), "add", "."], check=True)
        subprocess.run(["git", "-C", str(repo), "commit", "-qm", "undocumented candidate"], check=True)
        with self.assertRaises(ValueError):
            self.epoch.generate(repo, "HEAD", self.directory / "undocumented-export", self.prefix,
                                self.directory / "undocumented.log")

    def test_failed_skipped_incomplete_shards_never_gate(self):
        import documentation_ci as ci
        from unittest import mock
        context = {"source_sha": self.candidate, "source_tree": self.value["candidate_tree"]}
        inv = {"groups": {"scripts": [], "enhanced": []}, "shards": {s: [] for s in ci.SHARDS}}
        qualified = {"artifact_id": "1", "manifest_sha256": "a" * 64,
                     "artifact_name": "invalid", "bundle_sha256": "b" * 64}
        for state in ("failure", "skipped", "cancelled", None):
            with self.subTest(state=state), mock.patch.object(ci, "verify_inputs", return_value=({}, inv)):
                with self.assertRaises(ValueError):
                    ci.reconcile("unused", {}, context, qualified,
                                 {"prepare-documentation": {"result": "success"},
                                  "enhanced-tests": {"result": state}})


class AuditTrustedTargetTests(unittest.TestCase):
    """Real immutable audit objects through the target CLI, never feature helpers.

    Synthetic target commits are local policy-review fixtures, not assertions of
    independent approval. All mutations are committed in isolated Git clones.
    """

    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory(prefix="pol002-audit-authority-")
        cls.addClassCleanup(cls.directory.cleanup)
        cls.workspace = Path(cls.directory.name)
        cls.template = cls.workspace / "template"
        subprocess.run(["git", "clone", "-q", "--shared", "--no-checkout", str(ROOT),
                        str(cls.template)], check=True)
        def git(*args):
            return subprocess.check_output(["git", "-C", str(cls.template), *args],
                                           stderr=subprocess.STDOUT).decode().strip()
        git("config", "user.name", "POL002 Audit Fixture")
        git("config", "user.email", "pol002-audit@example.invalid")
        git("checkout", "-q", "-b", "trusted-target", policy.AUDIT_TARGET)
        (cls.template / "docs/tools/feature_source.py").write_bytes(Path(policy.__file__).read_bytes())
        git("add", "docs/tools/feature_source.py")
        git("commit", "-qm", "synthetic independently separate policy target")
        cls.target = git("rev-parse", "HEAD")
        git("update-ref", "refs/remotes/origin/main", cls.target)
        git("checkout", "-q", "-b", "audit-candidate", policy.AUDIT_COMMIT)
        git("merge", "-q", "--no-ff", "--no-edit", cls.target)
        cls.candidate = git("rev-parse", "HEAD")
        cls.runner = cls.workspace / "trusted-target" / "feature_source.py"
        cls.runner.parent.mkdir()
        cls.runner.write_bytes(policy.blob(cls.template, cls.target, "docs/tools/feature_source.py"))

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="pol002-audit-case-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name) / "repo"
        subprocess.run(["git", "clone", "-q", "--shared", str(self.template), str(self.root)], check=True)
        self.git("config", "user.name", "POL002 Audit Fixture")
        self.git("config", "user.email", "pol002-audit@example.invalid")
        self.git("update-ref", "refs/remotes/origin/main", self.target)

    def git(self, *args):
        return subprocess.check_output(["git", "-C", str(self.root), *args],
                                       stderr=subprocess.STDOUT).decode().strip()

    def commit(self):
        self.git("add", ".")
        self.git("commit", "-qm", "committed adversarial audit fixture")

    def evaluate(self, error=None):
        result = subprocess.run([sys.executable, str(self.runner), "--root", str(self.root)],
                                capture_output=True, text=True)
        if error is not None:
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertIn(error, result.stderr)
            return None
        self.assertEqual(result.returncode, 0, result.stderr)
        return json.loads(result.stdout)

    def variant(self, label):
        self.git("checkout", "-q", "-b", label, self.candidate)

    def test_exact_audit_requires_feature_gates_not_policy_shortcut(self):
        value = self.evaluate()
        self.assertEqual(value["case"], "feature-source")
        self.assertFalse(value["independent_approval"])
        self.assertFalse(value["publication_epoch_approved"])
        self.assertEqual(len(value["delta"]), 13)
        import epoch_ci
        self.assertEqual(epoch_ci.classify(self.root, self.target, "HEAD"), value)
        epoch_ci.authority(self.root, value)

    def test_documentation_only_through_trusted_target(self):
        self.git("checkout", "-q", "-b", "documentation", self.target)
        (self.root / "docs/coverage.md").write_text("Documentation-only fixture.\n")
        self.commit()
        self.assertEqual(self.evaluate()["case"], "documentation-only")

    def test_rf_product_rx_through_trusted_target(self):
        self.git("checkout", "-q", "-b", "rf", self.target)
        path = self.root / "native/src/rf_product_rx.cpp"
        path.write_bytes(path.read_bytes() + b"\n// RF fixture\n")
        self.commit()
        self.assertEqual(self.evaluate()["case"], "feature-source")

    def test_each_missing_audit_member_rejected(self):
        for i, (name, _, _, _) in enumerate(policy.AUDIT_TRANSITION):
            with self.subTest(path=name):
                self.variant("missing-" + str(i))
                self.git("rm", name)
                self.commit()
                self.evaluate("audit member: " + name)

    def test_each_modified_audit_blob_rejected(self):
        for i, (name, _, _, _) in enumerate(policy.AUDIT_TRANSITION):
            with self.subTest(path=name):
                self.variant("modified-" + str(i))
                path = self.root / name
                path.write_bytes(path.read_bytes() + b"\n")
                self.commit()
                self.evaluate("audit member: " + name)

    def test_each_wrong_audit_mode_rejected(self):
        for i, (name, mode, _, _) in enumerate(policy.AUDIT_TRANSITION):
            with self.subTest(path=name):
                self.variant("mode-" + str(i))
                (self.root / name).chmod(0o644 if mode == "100755" else 0o755)
                self.commit()
                self.evaluate("audit member: " + name)

    def test_incomplete_original_candidate_group_rejected(self):
        for i, (name, _, before, _) in enumerate(policy.AUDIT_TRANSITION[:5]):
            with self.subTest(path=name):
                self.variant("mixed-" + str(i))
                if before is None:
                    self.git("rm", name)
                else:
                    (self.root / name).write_bytes(policy.blob(self.root, policy.AUDIT_TARGET, name))
                self.commit()
                self.evaluate("audit member: " + name)

    def test_extra_script_json_rf_ada_native_and_removal_rejected(self):
        cases = (("scripts/unreviewed.py", b"print('unreviewed')\n"),
                 ("docs/reference/extra-evidence.json", b"{}\n"),
                 ("native/src/rf_product_rx.cpp", b"// changed RF\n"),
                 ("ada/src/ams-mel.ads", b"-- changed Ada\n"),
                 ("native/src/ir_stream.cpp", b"// changed IR\n"),
                 ("scripts/check.sh", b"#!/bin/sh\nexit 0\n"),
                 ("native/src/ir_stream.cpp", None))
        for i, (name, content) in enumerate(cases):
            with self.subTest(path=name, removal=content is None):
                self.variant("extra-" + str(i))
                if content is None:
                    self.git("rm", name)
                else:
                    (self.root / name).write_bytes(content)
                self.commit()
                self.evaluate("audit mixed with unrelated source/policy change")

    def test_candidate_cannot_modify_policy_or_approval_registry(self):
        for i, name in enumerate(("docs/tools/feature_source.py", "docs/tools/source_epoch.py",
                                  "scripts/check_doc_source_integrity.py")):
            with self.subTest(path=name):
                self.variant("policy-" + str(i))
                path = self.root / name
                path.write_bytes(path.read_bytes() + b"\nREVIEWED_EPOCHS = ('forged',)\n")
                self.commit()
                self.evaluate("candidate changes qualification policy: " + name)
                import epoch_ci
                with self.assertRaises(ValueError):
                    epoch_ci.classify(self.root, self.target, "HEAD")

    def test_forged_manifest_cannot_grant_approval(self):
        (self.root / "docs/source-authorization.json").write_text(
            '{"independent_approval":true,"publication_epoch_approved":true}\n')
        self.commit()
        self.evaluate("audit mixed with unrelated source/policy change")
        import source_epoch
        with self.assertRaisesRegex(ValueError, "no independently reviewed publication epoch"):
            source_epoch.publication_authority(self.root, "HEAD")

    def test_incorrect_target_ancestry_rejected(self):
        self.git("update-ref", "refs/remotes/origin/main", "42648a92eb0cc84b2980d16db6028992c0494ba1")
        self.evaluate("target lacks historical reviewed main")

    def test_incomplete_target_group_and_pol001_disagreement_rejected(self):
        cases = (("Makefile", "incomplete/modified audit member"),
                 ("scripts/check_doc_source_integrity.py", "reviewed POL-001 authorization"))
        for i, (name, error) in enumerate(cases):
            with self.subTest(path=name):
                self.git("checkout", "-q", "-b", "incomplete-target-" + str(i), self.target)
                path = self.root / name
                if name == "Makefile":
                    path.write_bytes(policy.blob(self.root, policy.AUDIT_COMMIT, name))
                else:
                    path.write_text(path.read_text().replace('("Makefile", "100644",',
                                                            '("Makefile", "100755",'))
                self.commit()
                target = self.git("rev-parse", "HEAD")
                self.git("update-ref", "refs/remotes/origin/main", target)
                self.git("checkout", "-q", "-b", "incomplete-candidate-" + str(i), self.candidate)
                self.git("merge", "-q", "--no-ff", "--no-edit", target)
                self.evaluate(error)

    def test_target_must_be_candidate_ancestor(self):
        self.git("checkout", "-q", "-b", "later-target", self.target)
        (self.root / "docs/coverage.md").write_text("Later target fixture\n")
        self.commit()
        later = self.git("rev-parse", "HEAD")
        self.git("update-ref", "refs/remotes/origin/main", later)
        self.git("checkout", "-q", "audit-candidate")
        self.evaluate("target is not candidate ancestor")

    def test_duplicate_or_extra_registry_member_rejected(self):
        # Deliberately malformed *target* registry: still fails closed through CLI.
        for i, suffix in enumerate(("AUDIT_TRANSITION[:1]", "(('scripts/extra.py',),)")):
            with self.subTest(suffix=suffix):
                self.git("checkout", "-q", "-b", "bad-target-" + str(i), self.target)
                path = self.root / "docs/tools/feature_source.py"
                text = Path(policy.__file__).read_text().replace(
                    "def audit_transition(", "AUDIT_TRANSITION += " + suffix + "\n\n\ndef audit_transition(")
                path.write_text(text)
                self.commit()
                target = self.git("rev-parse", "HEAD")
                self.git("update-ref", "refs/remotes/origin/main", target)
                self.git("checkout", "-q", "-b", "bad-candidate-" + str(i), self.candidate)
                self.git("merge", "-q", "--no-ff", "--no-edit", target)
                runner = Path(self.directory.name) / ("runner-" + str(i) + ".py")
                runner.write_text(text)
                result = subprocess.run([sys.executable, str(runner), "--root", str(self.root)],
                                        capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("duplicate/incomplete audit registry", result.stderr)


if __name__ == "__main__":
    import documentation_ci as ci
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("-v", action="store_true")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    suite = unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__])
    identities = sorted(test.id() for test in ci.flatten(suite))
    result = unittest.TextTestRunner(verbosity=2 if args.v else 1, resultclass=ci.EvidenceResult).run(suite)
    # Skips, expected failures, class setup omissions and incomplete callbacks are
    # failures even when unittest.wasSuccessful() alone would accept them.
    expected = {"start", "success", "stop"}
    from collections import Counter
    passed = result.wasSuccessful() and all(e["event"] in expected for e in result.events)
    passed = passed and all(Counter(e["id"] for e in result.events if e["event"] == kind)
                            == Counter(identities) for kind in expected)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_bytes(ci.canonical({"schema": 1, "tests": identities,
                                             "events": result.events, "successful": passed}))
    sys.exit(0 if passed else 1)
