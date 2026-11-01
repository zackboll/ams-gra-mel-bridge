"""PY-POL-001 disposable Git qualification, never policy approval.

Run: python3 -m unittest discover -s docs/tests -p py_pol_001_tests.py -v
Exact feature-object cases require the fetched PR #140 object; no network fetch.
"""

import importlib.util
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import types
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / "docs/tools"), str(ROOT / "scripts")]
import feature_source as feature
import check_doc_source_integrity as integrity

MAIN = "b4a550adf222b99a46b3218d00c4b080ec314a25"
FEATURE = "e738a001e620b85abf2a38cb32b1ffad24713c4c"
FEATURE_PATHS = ("python/README.md", "python/ams_mel/__init__.py",
                 "python/tests/test_rf_admin.py")
POLICIES = ("docs/tools/feature_source.py", "scripts/check_doc_source_integrity.py")


def git(root, *args):
    return subprocess.check_output(["git", "-C", str(root), *args], stderr=subprocess.STDOUT)


def original_module(name, path):
    module = types.ModuleType(name)
    module.__file__ = str(ROOT / path)
    exec(compile(git(ROOT, "show", MAIN + ":" + path), module.__file__, "exec"), module.__dict__)
    return module


class PythonOwnershipTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="py-pol-001-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        git(self.root, "init", "-q")
        git(self.root, "config", "user.name", "Python Policy Fixture")
        git(self.root, "config", "user.email", "py-policy@example.invalid")
        self.names = (
            "python/README.md", "python/ams_mel/__init__.py", "python/ams_mel/_native.py",
            "python/tests/test_abi.py", "python/tests/abi_probe.c",
            "native/include/ams_mel/abi.h", "native/src/ir_stream.cpp",
            "native/src/rf_product_rx.cpp", "ada/src/demo.ads",
            "rust/ams-mel-sys/src/lib.rs", "native/tests/other_mock.cpp",
            ".github/workflows/unrelated.yml", "docs/tools/feature_source.py",
            "scripts/check_doc_source_integrity.py", "native/vendor/pin.h",
            "python/ams_mel/generated/owned.py", "python/other.py",
            "docs/guide.md", "docs/gnatdoc-native-anchors.json",
            "scripts/check_doc_coverage.py", "_site/index.html")
        for name in self.names:
            self.write(name, b"original = 1\n")
        self.write("python/tests/test_existing.py", b"original = 1\n")
        self.base = self.commit()
        self.old_feature = original_module("old_feature", POLICIES[0])
        self.old_integrity = original_module("old_integrity", POLICIES[1])

    def write(self, name, data=b"changed = 2\n"):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        return path

    def commit(self):
        git(self.root, "add", "-A")
        git(self.root, "commit", "-qm", "disposable fixture")
        return git(self.root, "rev-parse", "HEAD").decode().strip()

    def reset(self):
        git(self.root, "reset", "--hard", self.base)
        git(self.root, "clean", "-fdx")

    def qualify(self):
        result = feature.qualify(self.root, self.base, "HEAD")
        self.assertFalse(result["independent_approval"])
        self.assertFalse(result["publication_epoch_approved"])
        integrity.check(self.root, self.base)
        return result

    def test_unchanged_source(self):
        self.assertEqual(self.qualify()["delta"], [])

    def test_safe_implementation_edits_and_nested_additions(self):
        for name in ("python/ams_mel/__init__.py", "python/ams_mel/rf/admin.py"):
            with self.subTest(path=name):
                self.reset()
                self.write(name)
                self.commit()
                self.assertEqual(self.qualify()["case"], "feature-source")

    def test_new_python_unit_tests(self):
        self.write("python/tests/rf/test_future_api.py")
        self.commit()
        self.qualify()

    def test_python_readme(self):
        self.write("python/README.md", b"# Python safe API\n")
        self.commit()
        self.assertEqual(self.qualify()["case"], "documentation-only")
        feature.qualify(self.root, self.base, "HEAD", documentation_only=True)

    def test_combined_python_changes(self):
        for name in FEATURE_PATHS:
            self.write(name)
        self.commit()
        self.assertEqual(len(self.qualify()["delta"]), 3)
        with self.assertRaisesRegex(ValueError, "documentation-only"):
            feature.qualify(self.root, self.base, "HEAD", documentation_only=True)

    @staticmethod
    def decision(call):
        try:
            value = call()
            return (True, value.get("review_obligations"))
        except ValueError:
            return (False, None)

    def test_unrelated_edits_keep_preexisting_decisions_with_python_changes(self):
        # Some RF/ABI/docs surfaces have separate authority. Compare both outcomes
        # and review obligations, rather than incorrectly requiring all to fail.
        for name in self.names[2:]:
            for data in (b"replacement = 2\n", b"original = 1\n# additive observation\n"):
                with self.subTest(path=name, data=data):
                    self.reset()
                    self.write(name, data)
                    self.commit()
                    old_feature = self.decision(lambda: self.old_feature.qualify(self.root, self.base, "HEAD"))
                    old_integrity = self.decision(lambda: self.old_integrity.check(self.root, self.base))
                    self.assertEqual(self.decision(lambda: feature.qualify(self.root, self.base, "HEAD")), old_feature)
                    self.assertEqual(self.decision(lambda: integrity.check(self.root, self.base)), old_integrity)
                    for python in FEATURE_PATHS:
                        self.write(python)
                    self.commit()
                    self.assertEqual(self.decision(lambda: feature.qualify(self.root, self.base, "HEAD")), old_feature)
                    self.assertEqual(self.decision(lambda: integrity.check(self.root, self.base)), old_integrity)

    def test_unauthorized_additions_not_hidden_by_python(self):
        paths = (
            "native/include/ams_mel/new.h", "native/src/new.cpp", "ada/src/new.ads",
            "rust/new.rs", "native/tests/new_mock.cpp", ".github/workflows/new.yml",
            "scripts/new_policy.py", "native/vendor/new.h", "_site/index.html",
            "python/other/new.py", "python/ams_mel_extra/new.py", "python/tests_extra/new.py",
            "python/ams_mel/rf/_native.py", "python/tests/rf/test_abi.py",
            "python/tests/fixture.c", "python/ams_mel/cache.pyc",
            "python/ams_mel/generated/new.py", "python/ams_mel/__pycache__/new.py",
            "python/tests/build/new.py", "python/ams_mel/vendor/new.py")
        for name in paths:
            with self.subTest(path=name):
                self.reset()
                for python in FEATURE_PATHS:
                    self.write(python)
                self.write(name)
                self.commit()
                self.assertFalse(feature.python_safe_path(name))
                with self.assertRaises(ValueError):
                    feature.qualify(self.root, self.base, "HEAD")
                with self.assertRaises(ValueError):
                    integrity.check(self.root, self.base)

    def test_path_classification_parity_and_traversal(self):
        paths = (*self.names, *FEATURE_PATHS, "python/ams_mel/rf/admin.py",
                 "python/ams_mel/../_native.py", "python/ams_mel/./safe.py",
                 "python/tests//safe.py", "/python/tests/safe.py",
                 "python/tests/../../scripts/policy.py", "python/tests/a\nb.py",
                 "python/tests/a\\b.py", "python/tests/.hidden.py")
        for name in paths:
            with self.subTest(path=name):
                self.assertEqual(feature.python_safe_path(name), integrity.python_safe_path(name))
                if ".." in name or "//" in name or "/./" in name or name.startswith("/") or "\n" in name or "\\" in name or "/.hidden" in name:
                    self.assertFalse(feature.python_safe_path(name))

    def test_deletions_and_renames(self):
        for name in (*FEATURE_PATHS[:2], "python/tests/test_existing.py"):
            for rename in (False, True):
                with self.subTest(path=name, rename=rename):
                    self.reset()
                    path = self.root / name
                    if rename:
                        path.rename(path.with_name("renamed.py"))
                    else:
                        path.unlink()
                    self.commit()
                    with self.assertRaisesRegex(ValueError, "removal"):
                        feature.qualify(self.root, self.base, "HEAD")
                    with self.assertRaisesRegex(ValueError, "removal"):
                        integrity.check(self.root, self.base)

    def test_symlink_and_executable_git_modes(self):
        for mode in ("symlink", "executable", "gitlink"):
            with self.subTest(mode=mode):
                self.reset()
                name = "python/tests/test_new.py"
                path = self.root / name
                if mode == "symlink":
                    path.symlink_to("../ams_mel/_native.py")
                    self.commit()
                elif mode == "executable":
                    self.write(name).chmod(0o755)
                    self.commit()
                else:
                    git(self.root, "update-index", "--add", "--cacheinfo", "160000," + self.base + "," + name)
                    git(self.root, "commit", "-qm", "gitlink fixture")
                with self.assertRaises(ValueError):
                    feature.qualify(self.root, self.base, "HEAD")
                with self.assertRaises(ValueError):
                    integrity.check(self.root, self.base)

    def test_nonregular_and_divergent_working_views(self):
        name = "python/ams_mel/__init__.py"
        for kind in ("fifo", "directory", "symlink", "executable", "bytes", "parent-symlink"):
            with self.subTest(kind=kind):
                self.reset()
                path = self.root / name
                path.unlink()
                if kind == "fifo":
                    os.mkfifo(path)
                elif kind == "directory":
                    path.mkdir()
                elif kind == "symlink":
                    path.symlink_to("_native.py")
                elif kind == "executable":
                    self.write(name, b"original = 1\n").chmod(0o755)
                elif kind == "bytes":
                    self.write(name)
                else:
                    parent = path.parent
                    moved = parent.with_name("moved")
                    parent.rename(moved)
                    parent.symlink_to(moved, target_is_directory=True)
                try:
                    with self.assertRaises(ValueError):
                        feature.qualify(self.root, self.base, "HEAD")
                    with self.assertRaises(ValueError):
                        integrity.check(self.root, self.base)
                finally:
                    if kind == "parent-symlink":
                        parent.unlink()
                        moved.rename(parent)
                    elif path.is_dir():
                        path.rmdir()
                    elif path.exists() or path.is_symlink():
                        path.unlink()

    def test_index_only_change_rejected(self):
        name = "python/ams_mel/__init__.py"
        self.write(name)
        git(self.root, "add", name)
        self.write(name, b"original = 1\n")
        with self.assertRaises(ValueError):
            feature.qualify(self.root, self.base, "HEAD")
        with self.assertRaises(ValueError):
            integrity.check(self.root, self.base)

    def test_unmerged_python_index_rejected(self):
        name = "python/ams_mel/__init__.py"
        oid = git(self.root, "rev-parse", self.base + ":" + name).decode().strip()
        git(self.root, "update-index", "--force-remove", name)
        subprocess.run(["git", "-C", str(self.root), "update-index", "--index-info"],
                       input=f"100644 {oid} 2\t{name}\n".encode(), check=True)
        with self.assertRaisesRegex(ValueError, "unmerged"):
            feature.qualify(self.root, self.base, "HEAD")
        with self.assertRaisesRegex(ValueError, "stage-0"):
            integrity.check(self.root, self.base)


class ExactFeatureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="py-pol-exact-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.root = Path(cls.temp.name) / "repo"
        subprocess.run(["git", "clone", "-q", "--shared", "--no-checkout", str(ROOT), str(cls.root)], check=True)
        git(cls.root, "checkout", "-q", "--detach", MAIN)
        git(cls.root, "config", "user.name", "Python Policy Qualification")
        git(cls.root, "config", "user.email", "py-policy@example.invalid")
        # A synthetic future trusted target ONLY inside this disposable clone.
        # No change to real origin/main, approval ledger or feature branch/ref.
        for name in POLICIES:
            (cls.root / name).write_bytes((ROOT / name).read_bytes())
        git(cls.root, "add", *POLICIES)
        git(cls.root, "commit", "-qm", "synthetic corrected target, NOT authorization")
        cls.target = git(cls.root, "rev-parse", "HEAD").decode().strip()
        git(cls.root, "update-ref", "refs/remotes/origin/main", cls.target)

    def test_unchanged_real_main(self):
        self.assertEqual(feature.qualify(self.root, MAIN, MAIN, check_checkout=False)["delta"], [])
        old = original_module("main_integrity", POLICIES[1])
        self.assertEqual(integrity.check(self.root), old.check(self.root))

    def test_exact_pr140_through_both_cli_validators(self):
        names = git(self.root, "diff", "--name-only", MAIN, FEATURE).decode().splitlines()
        self.assertEqual(names, list(FEATURE_PATHS))
        before = git(ROOT, "rev-parse", "refs/remotes/origin/pr-140")
        for name in FEATURE_PATHS:
            (self.root / name).write_bytes(git(self.root, "show", FEATURE + ":" + name))
        git(self.root, "add", *FEATURE_PATHS)
        git(self.root, "commit", "-qm", "exact PR140 Python delta, synthetic qualification only")
        try:
            evidence = None
            for name in POLICIES:
                result = subprocess.run([sys.executable, str(self.root / name)], cwd=self.root,
                                        capture_output=True, text=True, timeout=120)
                self.assertEqual(result.returncode, 0, result.stderr)
                if name == POLICIES[0]:
                    evidence = json.loads(result.stdout)
            self.assertEqual([item["path"] for item in evidence["delta"]], list(FEATURE_PATHS))
            self.assertFalse(evidence["independent_approval"])
            self.assertFalse(evidence["publication_epoch_approved"])
            identities = {}
            for name in FEATURE_PATHS:
                self.assertEqual((self.root / name).read_bytes(), git(ROOT, "show", FEATURE + ":" + name))
                identities[name] = hashlib.sha256((self.root / name).read_bytes()).hexdigest()
            print(json.dumps({"qualification_only_not_authorization": True,
                              "feature_head": FEATURE, "synthetic_target": self.target,
                              "candidate": evidence["candidate_commit"],
                              "tree": evidence["candidate_tree"],
                              "validator_exit_codes": [0, 0], "feature_sha256": identities}, sort_keys=True))
            for name in ("native/src/ir_stream.cpp", "python/ams_mel/_native.py",
                         "native/vendor/unauthorized.h"):
                path = self.root / name
                original = path.read_bytes() if path.exists() else None
                path.write_bytes(b"unauthorized replacement\n")
                git(self.root, "add", name)
                git(self.root, "commit", "-qm", "adversarial exact feature overlay")
                with self.assertRaises(ValueError):
                    feature.qualify(self.root, self.target, "HEAD")
                with self.assertRaises(ValueError):
                    integrity.check(self.root)
                git(self.root, "reset", "--hard", "HEAD^")
                self.assertEqual(path.read_bytes() if path.exists() else None, original)
            self.assertEqual(git(ROOT, "rev-parse", "refs/remotes/origin/pr-140"), before)
        finally:
            git(self.root, "reset", "--hard", self.target)

    def test_policy_cannot_authorize_itself(self):
        path = self.root / POLICIES[0]
        path.write_bytes(path.read_bytes() + b"\n# unapproved policy edit\n")
        git(self.root, "add", POLICIES[0])
        git(self.root, "commit", "-qm", "unapproved policy fixture")
        try:
            with self.assertRaisesRegex(ValueError, "changes qualification policy"):
                feature.qualify(self.root, self.target, "HEAD")
            spec = importlib.util.spec_from_file_location("fixture_policy", self.root / POLICIES[0])
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            with self.assertRaisesRegex(ValueError, "not trusted target"):
                module.trusted_target(self.root)
        finally:
            git(self.root, "reset", "--hard", self.target)


if __name__ == "__main__":
    unittest.main()
