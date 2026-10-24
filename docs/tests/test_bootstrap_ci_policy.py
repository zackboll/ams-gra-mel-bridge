"""CI-002 independent Git fixtures, command failures and workflow controls."""

import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "docs/tools"))
sys.path.insert(0, str(ROOT / "scripts"))
import bootstrap_ci_policy as policy
import check_doc_source_integrity as integrity


class PolicyTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix="ci002-policy-")
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.git("init", "-q")
        self.git("config", "user.name", "CI fixture")
        self.git("config", "user.email", "fixture@example.invalid")
        self.write("README.md", "original\n")
        self.write("docs/guide.md", "guide\n")
        self.write("native/source.c", "int value;\n")
        self.base = self.commit()

    def git(self, *args):
        return policy.git(self.root, *args)

    def write(self, name, text):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        return path

    def commit(self):
        self.git("add", "-A")
        self.git("commit", "-qm", "fixture")
        return self.git("rev-parse", "HEAD").decode().strip()

    def classify(self):
        return policy.classify(self.root, "pull_request", self.commit(), self.base)["mode"]

    def test_static_documentation(self):
        self.write("docs/guides/static.md", "static\n")
        self.assertEqual(self.classify(), policy.COMPLETE_SMOKE)

    def test_sensitive_categories(self):
        for name in ("native/src/source.c", "native/tests/test.c", "ada/src/unit.ads",
                     "ada/src/unit.adb", "rust/src/lib.rs", "python/ams_mel/a.py",
                     "CMakeLists.txt", "native/CMakeLists.txt", "project.gpr",
                     "scripts/build.sh", ".github/workflows/ci.yml",
                     ".github/workflows/documentation.yml", "vendor/source.h",
                     "docs/examples/src/main.adb", "docs/tools/check.py",
                     "docs/tests/test.py", "docs/toolchain/alire.toml",
                     "docs/unknown/new.md", "docs/tools/README.md",
                     "generated/interface.md", "unknown/new.md"):
            with self.subTest(path=name):
                self.git("reset", "--hard", self.base)
                self.write(name, "runtime sensitive\n")
                self.assertEqual(self.classify(), policy.FULL_STRESS)

    def test_mixed(self):
        self.write("docs/guide.md", "changed\n")
        self.write("native/source.c", "int changed;\n")
        self.assertEqual(self.classify(), policy.FULL_STRESS)

    def test_special_filenames(self):
        for name in ("docs/a space.md", "docs/line\nbreak.md", "docs/quote'\t$.md"):
            self.write(name, "static\n")
        self.assertEqual(self.classify(), policy.COMPLETE_SMOKE)

    def test_deletion(self):
        (self.root / "docs/guide.md").unlink()
        self.assertEqual(self.classify(), policy.COMPLETE_SMOKE)

    def test_native_deletion(self):
        (self.root / "native/source.c").unlink()
        self.assertEqual(self.classify(), policy.FULL_STRESS)

    def test_static_rename(self):
        self.git("mv", "docs/guide.md", "docs/new guide.md")
        self.assertEqual(self.classify(), policy.COMPLETE_SMOKE)

    def test_cross_category_rename(self):
        for old, new in (("docs/guide.md", "native/new.md"),
                         ("native/source.c", "docs/source.md")):
            self.git("reset", "--hard", self.base)
            self.git("mv", old, new)
            self.assertEqual(self.classify(), policy.FULL_STRESS)

    def test_symlink(self):
        (self.root / "docs/link.md").symlink_to("guide.md")
        self.assertEqual(self.classify(), policy.FULL_STRESS)

    def test_executable_mode(self):
        (self.root / "docs/guide.md").chmod(0o755)
        self.assertEqual(self.classify(), policy.FULL_STRESS)

    def test_submodule_mode(self):
        self.git("update-index", "--add", "--cacheinfo", "160000", self.base, "docs/module.md")
        self.git("commit", "-qm", "gitlink")
        (self.root / "docs/module.md").mkdir()
        head = self.git("rev-parse", "HEAD").decode().strip()
        self.assertEqual(policy.classify(self.root, "pull_request", head, self.base)["mode"],
                         policy.FULL_STRESS)

    def test_invalid_sources(self):
        for value in (None, "", "HEAD", "a" * 39, "A" * 40, "--help", "a" * 40 + "\n"):
            with self.subTest(sha=value), self.assertRaises(ValueError):
                policy.classify(self.root, "pull_request", value, self.base)
        with self.assertRaises(ValueError):
            policy.classify(self.root, "pull_request", self.base, None)

    def test_wrong_checkout(self):
        self.write("docs/guide.md", "new\n")
        self.commit()
        with self.assertRaisesRegex(ValueError, "identity mismatch"):
            policy.classify(self.root, "pull_request", self.base, self.base)

    def test_dirty_checkout(self):
        self.write("README.md", "dirty\n")
        with self.assertRaisesRegex(ValueError, "differs"):
            policy.classify(self.root, "pull_request", self.base, self.base)

    def test_incomplete_history(self):
        (self.root / ".git/shallow").write_text(self.base + "\n")
        with self.assertRaisesRegex(ValueError, "incomplete history"):
            policy.classify(self.root, "pull_request", self.base, self.base)

    def test_missing_history(self):
        with self.assertRaises(subprocess.CalledProcessError):
            policy.classify(self.root, "pull_request", self.base, "f" * 40)

    def test_empty_delta(self):
        self.assertEqual(policy.classify(self.root, "pull_request", self.base, self.base)["mode"],
                         policy.FULL_STRESS)

    def test_events(self):
        for event in ("push", "workflow_dispatch", "unknown"):
            self.assertEqual(policy.classify(self.root, event, self.base)["mode"], policy.FULL_STRESS)

    def test_forged_partial_diff(self):
        self.write("docs/guide.md", "changed\n")
        self.write("native/source.c", "int changed;\n")
        head = self.commit()
        original = policy.git
        def forged(root, *args):
            output = original(root, *args)
            if args[0] == "diff":
                fields = output.split(b"\0")
                return b"\0".join(fields[:2]) + b"\0"
            return output
        with mock.patch.object(policy, "git", side_effect=forged):
            with self.assertRaisesRegex(ValueError, "complete source trees"):
                policy.classify(self.root, "pull_request", head, self.base)

    def test_truncated_and_forged_records(self):
        for data in (b"", b":100644", b"x\0name\0", b"\0", b"record\0"):
            with self.subTest(data=data), self.assertRaises(ValueError):
                policy.raw_diff(data)

    def test_duplicate_records(self):
        record = b":100644 100644 " + b"a" * 40 + b" " + b"b" * 40 + b" M\0docs/a.md\0"
        with self.assertRaises(ValueError):
            policy.raw_diff(record * 2)

    def test_command_sequences(self):
        smoke = policy.native_commands(policy.COMPLETE_SMOKE)
        full = policy.native_commands(policy.FULL_STRESS)
        self.assertEqual(smoke[0], ["cmake", "--build", "build/native", "--parallel", "2"])
        self.assertEqual(smoke[1], ["ctest", "--test-dir", "build/native", "--output-on-failure"])
        self.assertEqual(smoke[2][:4], ["cmake", "--install", "build/native", "--prefix"])
        self.assertEqual(full[:2] + full[3:], smoke)
        self.assertEqual(full[2], ["ctest", "--test-dir", "build/native", "--parallel", "4",
                                   "--repeat", "until-fail:50", "--output-on-failure"])

    def test_real_ctest_failure_propagation(self):
        # Real CTest processes, including a failure only on stress repetition.
        counter = self.root / "counter"
        program = self.write("fail.py", "from pathlib import Path\nimport sys\n"
                             f"p=Path({str(counter)!r})\n"
                             "n=int(p.read_text())+1 if p.exists() else 1\np.write_text(str(n))\n"
                             "sys.exit(n >= int(sys.argv[1]))\n")
        for threshold, mode, expected in ((1, policy.COMPLETE_SMOKE, 1),
                                           (3, policy.FULL_STRESS, 3)):
            counter.unlink(missing_ok=True)
            self.write("CTestTestfile.cmake", f'add_test(real "{sys.executable}" "{program}" "{threshold}")\n')
            commands = [c for c in policy.native_commands(mode, str(self.root)) if c[0] == "ctest"]
            with self.assertRaises(subprocess.CalledProcessError):
                policy.execute(commands)
            self.assertEqual(int(counter.read_text()), expected)

    def test_workflow_contract(self):
        workflow = (ROOT / ".github/workflows/ci.yml").read_text()
        self.assertIn("push:\n    branches: [main]\n  pull_request:\n  workflow_dispatch:", workflow)
        for forbidden in ("paths-ignore:", "pull_request_target", "needs:", "continue-on-error:", "secrets."):
            self.assertNotIn(forbidden, workflow)
        self.assertIn("fetch-depth: 0", workflow)
        self.assertIn("github.event.pull_request.base.sha", workflow)
        self.assertIn("bootstrap_ci_policy.py --fetch --execute", workflow)
        for job in ("native", "ada", "rust", "python", "build-tree-isolation"):
            self.assertIn(f"  {job}:\n", workflow)

    def test_policy_mutations(self):
        # Run isolated mutated implementations against independent expectations.
        source = (ROOT / "docs/tools/bootstrap_ci_policy.py").read_text()
        changes = {b"native/source.c": (None, (b"100644", b"a" * 40))}
        for old, new in (("return COMPLETE_SMOKE", "return FULL_STRESS"),
                         ("if not static_path(name):", "if False:"),
                         ('entry[0] != b"100644"', 'entry[0] != b"100755"')):
            with self.subTest(mutation=old):
                self.assertIn(old, source)
                namespace = {"__name__": "mutant"}
                exec(compile(source.replace(old, new, 1), "mutant", "exec"), namespace)
                outcomes = (namespace["classify_changes"](changes),
                            namespace["classify_changes"]({b"docs/guide.md": (None, (b"100644", b"a" * 40))}))
                self.assertNotEqual(outcomes, (policy.FULL_STRESS, policy.COMPLETE_SMOKE))

    def test_merge_base_not_base_tip(self):
        self.git("checkout", "-qb", "feature")
        self.write("docs/guide.md", "feature docs\n")
        head = self.commit()
        self.git("checkout", "-q", "--detach", self.base)
        self.write("native/source.c", "int base_advanced;\n")
        advanced_base = self.commit()
        self.git("checkout", "-q", "--detach", head)
        audit = policy.classify(self.root, "pull_request", head, advanced_base)
        self.assertEqual(audit["merge_base"], self.base)
        self.assertEqual(audit["mode"], policy.COMPLETE_SMOKE)

    def test_inventory_owner(self):
        import documentation_ci as ci
        ids = [test.id() for test in ci.discover("enhanced")
               if test.id().startswith("test_bootstrap_ci_policy.")]
        self.assertEqual(len(ids), 40)
        self.assertEqual(ci.SHARDS["remaining"].count("test_bootstrap_ci_policy"), 1)

    def test_mode_command_mutation(self):
        full = policy.native_commands(policy.FULL_STRESS)
        with mock.patch.object(policy.subprocess, "run") as run:
            run.side_effect = [None, subprocess.CalledProcessError(1, full[1])]
            with self.assertRaises(subprocess.CalledProcessError):
                policy.execute(full)
            self.assertEqual(run.call_count, 2)
        with self.assertRaises(ValueError):
            policy.native_commands("user-selected-smoke")

    def inventory_fixture(self):
        import documentation_ci as ci
        root = self.root / "inventory"
        path = root / "docs/tests/test_bootstrap_ci_policy.py"
        path.parent.mkdir(parents=True)
        path.write_bytes((ROOT / "docs/tests/test_bootstrap_ci_policy.py").read_bytes())
        ids = list(ci.source_methods(path.read_bytes(), "test_bootstrap_ci_policy"))
        return ci, root, path, {"scripts": [], "enhanced": ids}

    def test_inventory_mutated_source(self):
        ci, root, path, groups = self.inventory_fixture()
        path.write_bytes(path.read_bytes().replace(b"self.assertEqual", b"self.assertNotEqual", 1))
        with self.assertRaisesRegex(ValueError, "control source changed"):
            ci.validate_inventory(groups, root)

    def test_inventory_missing_identity(self):
        ci, root, path, groups = self.inventory_fixture()
        groups["enhanced"].pop()
        with self.assertRaisesRegex(ValueError, "missing or duplicated"):
            ci.validate_inventory(groups, root)

    def test_inventory_duplicate_identity(self):
        ci, root, path, groups = self.inventory_fixture()
        groups["enhanced"].append(groups["enhanced"][0])
        with self.assertRaisesRegex(ValueError, "missing or duplicated"):
            ci.validate_inventory(groups, root)

    def test_inventory_source_required(self):
        import documentation_ci as ci
        with mock.patch.object(ci, "ROOT", self.root):
            with self.assertRaisesRegex(ValueError, "control source missing"):
                ci.validate_inventory({"scripts": [], "enhanced": []})

    def test_unknown_modes(self):
        for mode in (b"120000", b"160000", b"100755", b"100664"):
            self.assertEqual(policy.classify_changes({b"docs/guide.md":
                             (None, (mode, b"a" * 40))}), policy.FULL_STRESS)


class WorkflowTransitionTests(unittest.TestCase):
    git = PolicyTests.git
    write = PolicyTests.write
    commit = PolicyTests.commit

    def setUp(self):
        PolicyTests.setUp(self)
        self.name, self.old_sha, self.new_sha = integrity.PROPOSED_BOOTSTRAP_TRANSITION
        self.old = subprocess.check_output(["git", "-C", str(ROOT), "show", integrity.BASE + ":" + self.name])
        self.new = (ROOT / self.name).read_bytes()
        self.path = self.root / self.name
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.path.write_bytes(self.old)
        self.base = self.commit()

    def state(self):
        return integrity.proposed_bootstrap_state(self.root, self.git, {self.name}, {self.name}, self.base)

    def stage(self, content):
        self.path.write_bytes(content)
        self.git("add", self.name)

    def test_exact_identities(self):
        self.assertEqual(hashlib.sha256(self.old).hexdigest(), self.old_sha)
        self.assertEqual(hashlib.sha256(self.new).hexdigest(), self.new_sha)
        self.assertEqual(self.state(), "original")
        self.stage(self.new)
        self.assertEqual(self.state(), "proposed_pending_independent_review")

    def test_parity(self):
        for indexed, working in ((self.old, self.new), (self.new, self.old)):
            self.stage(indexed)
            self.path.write_bytes(working)
            with self.assertRaisesRegex(ValueError, "state mismatch"):
                self.state()

    def test_unknown_bytes(self):
        for indexed in (False, True):
            self.stage(self.old)
            self.path.write_bytes(self.new + b"# unknown\n")
            if indexed:
                self.git("add", self.name)
                self.path.write_bytes(self.new)
            with self.assertRaisesRegex(ValueError, "unlisted Bootstrap"):
                self.state()

    def test_wrong_foundation(self):
        self.stage(self.old + b"# wrong foundation\n")
        self.base = self.commit()
        self.stage(self.new)
        with self.assertRaisesRegex(ValueError, "foundation bytes"):
            self.state()

    def test_workflow_symlink(self):
        self.path.unlink()
        self.path.symlink_to(self.root / "README.md")
        with self.assertRaisesRegex(ValueError, "not regular"):
            self.state()

    def test_workflow_modes(self):
        self.path.chmod(0o755)
        with self.assertRaisesRegex(ValueError, "not regular"):
            self.state()
        self.git("add", self.name)
        self.path.chmod(0o644)
        with self.assertRaisesRegex(ValueError, "stage-0"):
            self.state()
    def test_unmerged_index(self):
        oid = self.git("hash-object", "-w", str(self.path)).decode().strip()
        self.git("update-index", "--force-remove", self.name)
        subprocess.run(["git", "-C", str(self.root), "update-index", "--index-info"],
                       input=f"100644 {oid} 1\t{self.name}\n100644 {oid} 2\t{self.name}\n".encode(), check=True)
        with self.assertRaisesRegex(ValueError, "stage-0"):
            self.state()
