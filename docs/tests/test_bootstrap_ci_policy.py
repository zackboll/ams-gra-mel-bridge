"""CI-002 independent Git fixtures, command failures and workflow controls."""

import hashlib
import json
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
        head = self.commit()
        build = self.merge(self.base, head)
        return policy.classify(self.root, "pull_request", head, self.base, build)["mode"]

    def merge(self, base, head, tree=None, parents=None):
        if tree is None:
            tree = self.git("merge-tree", "--write-tree", "--no-messages", base, head).decode().strip()
        args = ["commit-tree", tree, "-m", "synthetic merge fixture"]
        for parent in ([base, head] if parents is None else parents):
            args.extend(["-p", parent])
        build = self.git(*args).decode().strip()
        self.git("checkout", "-q", "--detach", build)
        return build

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
                self.git("checkout", "-q", "--detach", self.base)
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
            self.git("checkout", "-q", "--detach", self.base)
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
        build = self.merge(self.base, head)
        self.assertEqual(policy.classify(self.root, "pull_request", head, self.base, build)["mode"],
                         policy.FULL_STRESS)

    def test_invalid_sources(self):
        for value in (None, "", "HEAD", "a" * 39, "A" * 40, "--help", "a" * 40 + "\n"):
            with self.subTest(sha=value), self.assertRaises(ValueError):
                policy.classify(self.root, "pull_request", value, self.base, self.base)
        with self.assertRaises(ValueError):
            policy.classify(self.root, "pull_request", self.base, None, self.base)

    def test_wrong_checkout(self):
        self.write("docs/guide.md", "new\n")
        self.commit()
        with self.assertRaisesRegex(ValueError, "identity mismatch"):
            policy.classify(self.root, "pull_request", self.base, self.base, self.base)

    def test_dirty_checkout(self):
        self.write("README.md", "dirty\n")
        with self.assertRaisesRegex(ValueError, "differs"):
            policy.classify(self.root, "pull_request", self.base, self.base, self.base)

    def test_incomplete_history(self):
        (self.root / ".git/shallow").write_text(self.base + "\n")
        with self.assertRaisesRegex(ValueError, "incomplete history"):
            policy.classify(self.root, "pull_request", self.base, self.base, self.base)

    def test_missing_history(self):
        with self.assertRaises(subprocess.CalledProcessError):
            policy.classify(self.root, "pull_request", self.base, "f" * 40, self.base)

    def test_empty_delta(self):
        self.git("commit", "--allow-empty", "-qm", "empty feature")
        head = self.git("rev-parse", "HEAD").decode().strip()
        build = self.merge(self.base, head)
        self.assertEqual(policy.classify(self.root, "pull_request", head, self.base, build)["mode"],
                         policy.FULL_STRESS)

    def test_events(self):
        for event in ("push", "workflow_dispatch", "unknown"):
            self.assertEqual(policy.classify(self.root, event, self.base, build=self.base)["mode"], policy.FULL_STRESS)

    def test_forged_partial_diff(self):
        self.write("docs/guide.md", "changed\n")
        self.write("native/source.c", "int changed;\n")
        head = self.commit()
        build = self.merge(self.base, head)
        original = policy.git
        def forged(root, *args):
            output = original(root, *args)
            if args[0] == "diff":
                fields = output.split(b"\0")
                return b"\0".join(fields[:2]) + b"\0"
            return output
        with mock.patch.object(policy, "git", side_effect=forged):
            with self.assertRaisesRegex(ValueError, "complete source trees"):
                policy.classify(self.root, "pull_request", head, self.base, build)

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
        build = self.merge(advanced_base, head)
        audit = policy.classify(self.root, "pull_request", head, advanced_base, build)
        self.assertEqual(audit["merge_base"], self.base)
        self.assertEqual(audit["mode"], policy.COMPLETE_SMOKE)

    def test_inventory_owner(self):
        import documentation_ci as ci
        ids = [test.id() for test in ci.discover("enhanced")
               if test.id().startswith("test_bootstrap_ci_policy.")]
        self.assertEqual(len(ids), 71)
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

    def test_superseded_proposal_rejected(self):
        previous = subprocess.check_output(["git", "-C", str(ROOT), "show",
                    "cec2a98ee90cbf532c66ddf8c1ff13e86b6b6ae0:" + self.name])
        self.assertEqual(hashlib.sha256(previous).hexdigest(),
                         "e42d0031368885935b5157babe9f9b2063f157842a306a25995130789fb0fe8c")
        self.stage(previous)
        with self.assertRaisesRegex(ValueError, "unlisted Bootstrap"):
            self.state()


class MergeResultTests(unittest.TestCase):
    git = PolicyTests.git
    write = PolicyTests.write
    commit = PolicyTests.commit
    merge = PolicyTests.merge

    def setUp(self):
        PolicyTests.setUp(self)
        self.write("native/main.c", '#include <stdio.h>\nint base_value(void);\nint feature_value(void);\n'
                   'int main(void) { printf("%d %d\\n", base_value(), feature_value()); return 0; }\n')
        self.write("native/base.c", "int base_value(void) { return 1; }\n")
        self.write("native/feature.c", "int feature_value(void) { return 2; }\n")
        self.initial = self.commit()
        self.write("docs/guide.md", "feature static documentation\n")
        self.head = self.commit()
        self.git("checkout", "-q", "--detach", self.initial)
        self.write("native/base.c", "int base_value(void) { return 7; }\n")
        self.base = self.commit()
        # Genuine merge through Git's working-tree merge machinery, independent
        # of policy's merge-tree reconstruction and malformed-object fixtures.
        self.git("merge", "--no-ff", "-m", "GitHub-style proposed merge", self.head)
        self.build = self.git("rev-parse", "HEAD").decode().strip()

    def audit(self, **overrides):
        args = dict(root=self.root, event="pull_request", head=self.head,
                    base=self.base, build=self.build)
        args.update(overrides)
        return policy.classify(**args)

    def native_output(self):
        with tempfile.TemporaryDirectory(prefix="ci002-native-execution-") as tmp:
            binary = str(Path(tmp) / "native-fixture")
            policy.execute([["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                             *[str(self.root / "native" / name) for name in
                               ("main.c", "base.c", "feature.c")], "-o", binary]])
            return subprocess.check_output([binary]).decode().strip()

    def malformed(self, parents, tree=None):
        self.build = self.merge(self.base, self.head, tree=tree, parents=parents)

    def test_divergent_static_build_executes_merge_result(self):
        self.assertNotEqual(self.head, self.build)
        self.git("checkout", "-q", "--detach", self.head)
        self.assertEqual(self.native_output(), "1 2")
        self.git("checkout", "-q", "--detach", self.build)
        audit = self.audit()
        self.assertEqual(audit["mode"], policy.COMPLETE_SMOKE)
        self.assertEqual(audit["paths"], ["docs/guide.md"])
        self.assertEqual(audit["build_parents"], [self.base, self.head])
        self.assertEqual(audit["build_tree"], audit["reconstructed_merge_tree"])
        self.assertEqual(self.native_output(), "7 2")

    def test_divergent_native_build_executes_merge_result(self):
        self.git("checkout", "-q", "--detach", self.head)
        self.write("native/feature.c", "int feature_value(void) { return 9; }\n")
        self.head = self.commit()
        self.assertEqual(self.native_output(), "1 9")
        self.git("checkout", "-q", "--detach", self.base)
        self.git("merge", "--no-ff", "-m", "native-sensitive merge", self.head)
        self.build = self.git("rev-parse", "HEAD").decode().strip()
        self.assertEqual(self.audit()["mode"], policy.FULL_STRESS)
        self.assertEqual(self.native_output(), "7 9")

    def test_head_checkout_rejected(self):
        self.git("checkout", "-q", "--detach", self.head)
        with self.assertRaisesRegex(ValueError, "identity mismatch"):
            self.audit()
        with self.assertRaisesRegex(ValueError, "parents"):
            self.audit(build=self.head)

    def test_wrong_build(self):
        with self.assertRaisesRegex(ValueError, "identity mismatch"):
            self.audit(build=self.head)

    def test_wrong_head(self):
        with self.assertRaisesRegex(ValueError, "parents"):
            self.audit(head=self.initial)

    def test_wrong_base(self):
        with self.assertRaisesRegex(ValueError, "parents"):
            self.audit(base=self.initial)

    def test_swapped_parents(self):
        self.malformed([self.head, self.base])
        with self.assertRaisesRegex(ValueError, "parents"):
            self.audit()

    def test_missing_head_parent(self):
        self.malformed([self.base])
        with self.assertRaisesRegex(ValueError, "parents"):
            self.audit()

    def test_unrelated_parent(self):
        unrelated = self.git("commit-tree", self.git("rev-parse", self.initial + "^{tree}").decode().strip(),
                             "-m", "unrelated root").decode().strip()
        self.malformed([self.base, unrelated])
        with self.assertRaisesRegex(ValueError, "parents"):
            self.audit()

    def test_third_parent(self):
        self.malformed([self.base, self.head, self.initial])
        with self.assertRaisesRegex(ValueError, "parents"):
            self.audit()

    def test_no_parents(self):
        self.malformed([])
        with self.assertRaisesRegex(ValueError, "parents"):
            self.audit()

    def test_stale_merge(self):
        self.git("checkout", "-q", "--detach", self.base)
        self.write("native/base.c", "int base_value(void) { return 8; }\n")
        self.base = self.commit()
        self.git("checkout", "-q", "--detach", self.build)
        with self.assertRaisesRegex(ValueError, "parents"):
            self.audit()

    def test_forged_tree(self):
        self.malformed([self.base, self.head], self.git("rev-parse", self.head + "^{tree}").decode().strip())
        with self.assertRaisesRegex(ValueError, "reconstructed merge"):
            self.audit()

    def test_missing_objects(self):
        for field in ("head", "base"):
            with self.subTest(field=field), self.assertRaises(subprocess.CalledProcessError):
                self.audit(**{field: "f" * 40})

    def test_tag_object_not_commit(self):
        self.git("tag", "-a", "forged-source", "-m", "tag object", self.head)
        tag = self.git("rev-parse", "forged-source").decode().strip()
        with self.assertRaisesRegex(ValueError, "not a commit"):
            self.audit(head=tag)

    def test_shallow_merge(self):
        (self.root / ".git/shallow").write_text(self.initial + "\n")
        with self.assertRaisesRegex(ValueError, "incomplete history"):
            self.audit()

    def test_missing_ancestor_object_in_nonshallow_history(self):
        # The shared initial commit still has an older parent. Removing that
        # object leaves the event commits/merge base intact and is not shallow.
        ancestor = self.git("rev-parse", self.initial + "^").decode().strip()
        (self.root / ".git/objects" / ancestor[:2] / ancestor[2:]).unlink()
        self.assertEqual(self.git("rev-parse", "--is-shallow-repository").strip(), b"false")
        with self.assertRaises(subprocess.CalledProcessError):
            self.audit()

    def test_missing_historical_blob_in_nonshallow_history(self):
        # This blob is superseded in the PR, so inspecting only current source
        # trees or the current merge cannot establish complete history.
        blob = self.git("rev-parse", self.initial + ":docs/guide.md").decode().strip()
        (self.root / ".git/objects" / blob[:2] / blob[2:]).unlink()
        self.assertEqual(self.git("rev-parse", "--is-shallow-repository").strip(), b"false")
        with self.assertRaises(subprocess.CalledProcessError):
            self.audit()

    def test_dirty_merge(self):
        self.write("native/base.c", "int base_value(void) { return 99; }\n")
        with self.assertRaisesRegex(ValueError, "differs"):
            self.audit()

    def test_invalid_build_authority(self):
        for build in (None, "", "HEAD", "a" * 39, "A" * 40):
            with self.subTest(build=build), self.assertRaisesRegex(ValueError, "invalid or absent"):
                self.audit(build=build)

    def test_non_pr_head_build_must_match(self):
        for event in ("push", "workflow_dispatch", "unknown"):
            with self.subTest(event=event), self.assertRaisesRegex(ValueError, "non-PR"):
                self.audit(event=event)
            self.assertEqual(self.audit(event=event, head=self.build)["mode"], policy.FULL_STRESS)

    def test_record_checkout_change(self):
        record = self.audit()
        self.git("checkout", "-q", "--detach", self.head)
        with self.assertRaises(ValueError):
            policy.verify_record(self.root, "pull_request", self.head, self.base, self.build, record)

    def test_record_mode_change(self):
        record = self.audit()
        record["mode"] = policy.FULL_STRESS
        with self.assertRaisesRegex(ValueError, "changed since"):
            policy.verify_record(self.root, "pull_request", self.head, self.base, self.build, record)

    def test_record_base_change(self):
        record = self.audit()
        with self.assertRaises(ValueError):
            policy.verify_record(self.root, "pull_request", self.head, self.initial, self.build, record)

    def test_execution_rechecks_source(self):
        record = self.audit()
        def verify():
            return policy.verify_record(self.root, "pull_request", self.head, self.base, self.build, record)
        with self.assertRaisesRegex(ValueError, "differs"):
            policy.execute([[sys.executable, "-c", f"from pathlib import Path; Path({str(self.root / 'README.md')!r}).write_text('dirty')"],
                            [sys.executable, "-c", "raise AssertionError('must not execute')"]], verify=verify)

    def test_cli_preconfiguration_and_execution_record(self):
        with tempfile.TemporaryDirectory(prefix="ci002-audit-") as tmp:
            audit = Path(tmp) / "audit.json"
            env = dict(os.environ, GITHUB_EVENT_NAME="pull_request", BOOTSTRAP_HEAD_SHA=self.head,
                       BOOTSTRAP_BASE_SHA=self.base, BOOTSTRAP_BUILD_SHA=self.build)
            command = [sys.executable, str(ROOT / "docs/tools/bootstrap_ci_policy.py"),
                       "--verify-only", "--audit", str(audit)]
            subprocess.run(command, cwd=self.root, env=env, check=True)
            self.assertEqual(json.loads(audit.read_bytes()), self.audit())
            # Existing records cannot be silently replaced during preparation.
            result = subprocess.run(command, cwd=self.root, env=env, capture_output=True)
            self.assertNotEqual(result.returncode, 0)

    def test_workflow_verifies_before_configure(self):
        workflow = (ROOT / ".github/workflows/ci.yml").read_text()
        native = workflow.split("  native:\n", 1)[1].split("  build-tree-isolation:\n", 1)[0]
        self.assertNotIn("          ref:", native)
        self.assertIn("BOOTSTRAP_BUILD_SHA: ${{ github.sha }}", native)
        self.assertLess(native.index("--verify-only"), native.index("- name: Configure"))
        self.assertIn('--execute --audit "$RUNNER_TEMP/bootstrap-policy.json"', native)

    def test_cli_requires_preconfiguration_record(self):
        env = dict(os.environ, GITHUB_EVENT_NAME="pull_request", BOOTSTRAP_HEAD_SHA=self.head,
                   BOOTSTRAP_BASE_SHA=self.base, BOOTSTRAP_BUILD_SHA=self.build)
        result = subprocess.run([sys.executable, str(ROOT / "docs/tools/bootstrap_ci_policy.py"),
                                 "--execute"], cwd=self.root, env=env, capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"requires the pre-configuration audit", result.stderr)

    def test_truncated_merge_diff(self):
        original = policy.git
        def truncated(root, *args):
            result = original(root, *args)
            return result[:-1] if args[0] == "diff" else result
        with mock.patch.object(policy, "git", side_effect=truncated):
            with self.assertRaisesRegex(ValueError, "truncated"):
                self.audit()

    def test_merge_authority_mutations(self):
        source = (ROOT / "docs/tools/bootstrap_ci_policy.py").read_text()
        for old, new in (("parents == [base, head]", "True"),
                         ("build_tree == reconstructed", "True")):
            with self.subTest(mutation=old):
                namespace = {"__name__": "mutant"}
                exec(compile(source.replace(old, new, 1), "mutant", "exec"), namespace)
                if old.startswith("parents"):
                    self.build = self.merge(self.base, self.head, parents=[self.head, self.base])
                else:
                    self.build = self.merge(self.base, self.head,
                                            tree=self.git("rev-parse", self.head + "^{tree}").decode().strip())
                # Mutant accepts a real malformed commit that the policy rejects.
                self.assertEqual(namespace["classify"](self.root, "pull_request", self.head,
                                                       self.base, self.build)["mode"], policy.COMPLETE_SMOKE)
                with self.assertRaises(ValueError):
                    self.audit()
