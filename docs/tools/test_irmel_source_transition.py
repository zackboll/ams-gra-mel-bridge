"""Standalone IRMEL-POL-001 controls; deliberately outside frozen discovery.

Actual isolated Git clones use immutable source objects, not candidate manifests.
Run explicitly: python3 docs/tools/test_irmel_source_transition.py
"""

import hashlib
import itertools
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
sys.path.insert(0, str(ROOT / "docs/tools"))
import check_doc_source_integrity as policy
import documentation_ci

MAIN = "33720d155d99f183229b6ed87037ecbd53a3875b"
CANDIDATE = "47630e61d2631effbbbe3e222e998e33daa90f60"
FOUNDATION = "42648a92eb0cc84b2980d16db6028992c0494ba1"
# Independent literals: do not derive fixture identities from the implementation.
FILES = (
    ("Makefile", "100644", "e4c3441a32169c6474dedef00a2db1f798375970",
     "7b608a960241c87f88819dbe2cd6bbe42cdbdebd",
     "446ed30875726cc7dd45e2ca59a48254fed65e5036c0de1542dd5d3c38803ba8"),
    ("scripts/test_ada.sh", "100755", "d32e162093a94ba77ab482cb53e98d91e31ee29a",
     "9d35abb1345aa8b6a71a9d7b1229f19c16d33239",
     "21ec1d4545f0ef2e6dbb0af110bfa9083b1e102e9b2f32ee6b1e9dedd5ee9acf"),
    ("scripts/check_ir_api_coverage.py", "100644", None,
     "6775d2e9986d3b59233e7c464745a1482d047f81",
     "6a23700ea3eaa7de8ed7f57d5dcd43d38fc7c5cdd64f67bbf9ad7494202e9e31"),
    ("scripts/freeze_ir_api_evidence.py", "100644", None,
     "48af395041dd3babb1a1df378e61643064ff91b0",
     "a8f371013211ffaba2f0723388b50acb79f24b8c4abe5e1748b42a6dc38abc9e"),
    ("scripts/test_ir_api_coverage.py", "100644", None,
     "fa96e0ceb5c16b911fdb0acdfec15b1b53ca981a",
     "96ff71c641673f36ea90a9c1a7707e2eae367000c0e9a16dd3bd4bc1d621b333"),
)


def source_git(*args):
    return subprocess.check_output(["git", "-C", str(ROOT), *args])


class IRMELSourceTransitionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if policy.BASE != FOUNDATION:
            raise AssertionError("DOC-001 foundation replaced")
        for commit, tree in ((MAIN, "76d7988ea723a07ced054c0275e80763cbdcb772"),
                             (CANDIDATE, "6b16f462a5072c22b52f13f7b59fa13280f3e252")):
            if source_git("rev-parse", commit + "^{tree}").decode().strip() != tree:
                raise AssertionError("immutable source tree mismatch")
        cls.original, cls.candidate = {}, {}
        for name, mode, old, new, digest in FILES:
            for commit, blob, target in ((MAIN, old, cls.original),
                                         (CANDIDATE, new, cls.candidate)):
                entry = source_git("ls-tree", "-z", commit, "--", name)
                if blob is None:
                    if entry:
                        raise AssertionError("new source existed in original")
                    continue
                data = source_git("show", commit + ":" + name)
                computed = hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
                if entry != f"{mode} blob {blob}\t{name}\0".encode() or computed != blob:
                    raise AssertionError("immutable blob/mode mismatch: " + name)
                if commit == CANDIDATE and hashlib.sha256(data).hexdigest() != digest:
                    raise AssertionError("independent SHA-256 mismatch: " + name)
                target[name] = data

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="irmel-policy-test-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / "repo"
        subprocess.run(["git", "clone", "--quiet", "--shared", "--no-checkout",
                        str(ROOT), str(self.root)], check=True)
        self.git("checkout", "--quiet", "--detach", MAIN)

    def git(self, *args, input=None):
        return subprocess.check_output(["git", "-C", str(self.root), *args], input=input,
                                       stderr=subprocess.STDOUT)

    def reset(self):
        self.git("reset", "--hard", MAIN)
        self.git("clean", "-fdq")

    def install(self, selected=None):
        for name, mode, _, _, _ in FILES:
            if selected is not None and name not in selected:
                continue
            path = self.root / name
            path.write_bytes(self.candidate[name])
            path.chmod(0o755 if mode == "100755" else 0o644)
            self.git("add", "--", name)

    def reject(self, reason="IRMEL-POL-001"):
        with self.assertRaisesRegex(ValueError, reason) as caught:
            policy.check(self.root)
        print("  rejected: " + str(caught.exception), file=sys.stderr)

    def test_original_state_passes(self):
        self.assertEqual(policy.check(self.root)["irmel_pol_001"], "ORIGINAL")

    def test_exact_candidate_state_passes(self):
        self.install()
        self.assertEqual(policy.check(self.root)["irmel_pol_001"], "CANDIDATE")

    def test_each_required_new_script_missing_fails(self):
        for name, _, old, _, _ in FILES:
            if old is None:
                with self.subTest(path=name):
                    self.reset()
                    self.install()
                    (self.root / name).unlink()
                    self.reject("nonregular working tree")
                    self.git("rm", "--cached", name)
                    self.reject("mixed or partially")

    def test_extra_unapproved_script_fails(self):
        self.install()
        path = self.root / "scripts/unapproved_audit.py"
        path.write_text("raise SystemExit(0)\n")
        self.git("add", ".")
        self.reject("unauthorized tracked files added")

    def test_one_changed_byte_in_each_file_in_either_view_fails(self):
        for name, _, _, _, _ in FILES:
            for view in ("working", "index", "both"):
                with self.subTest(path=name, view=view):
                    self.reset()
                    self.install()
                    data = self.candidate[name]
                    (self.root / name).write_bytes(bytes([data[0] ^ 1]) + data[1:])
                    if view != "working":
                        self.git("add", name)
                    if view == "index":
                        (self.root / name).write_bytes(data)
                    self.reject("bytes differ|unapproved complete bytes")

    def test_candidate_makefile_original_ada_script_fails(self):
        self.install({"Makefile"})
        self.reject("mixed or partially")

    def test_candidate_ada_script_original_makefile_fails(self):
        self.install({"scripts/test_ada.sh"})
        self.reject("mixed or partially")

    def test_new_scripts_original_entrypoints_fail(self):
        self.install({name for name, _, old, _, _ in FILES if old is None})
        self.reject("mixed or partially")

    def test_candidate_entrypoints_without_new_scripts_fail(self):
        self.install(set(self.original))
        self.reject("mixed or partially")

    def test_all_thirty_partial_states_fail(self):
        names = [row[0] for row in FILES]
        for bits in itertools.product((False, True), repeat=5):
            if all(bits) or not any(bits):
                continue
            with self.subTest(candidate_members=bits):
                self.reset()
                self.install({name for name, enabled in zip(names, bits) if enabled})
                self.reject("mixed or partially")

    def test_worktree_index_disagreement_both_directions_fails(self):
        for indexed_candidate in (False, True):
            with self.subTest(indexed_candidate=indexed_candidate):
                self.reset()
                if indexed_candidate:
                    self.install()
                for name in self.original:
                    (self.root / name).write_bytes(
                        self.original[name] if indexed_candidate else self.candidate[name])
                self.reject("bytes differ")

    def test_symlink_substitution_each_file_fails(self):
        for name, _, _, _, _ in FILES:
            with self.subTest(path=name):
                self.reset()
                self.install()
                path = self.root / name
                target = self.root / "docs/symlink-target"
                target.write_bytes(path.read_bytes())
                path.unlink()
                path.symlink_to(target)
                self.reject("nonregular working tree")
                self.git("add", name)
                self.reject("index path/mode/stage")

    def test_directory_and_fifo_substitution_fail_without_reading(self):
        for kind in ("directory", "fifo"):
            self.reset()
            self.install()
            path = self.root / FILES[2][0]
            path.unlink()
            path.mkdir() if kind == "directory" else os.mkfifo(path)
            self.reject("nonregular working tree")
            path.rmdir() if kind == "directory" else path.unlink()

    def test_wrong_executable_mode_each_file_and_view_fails(self):
        for name, mode, _, _, _ in FILES:
            for view in ("working", "index"):
                with self.subTest(path=name, view=view):
                    self.reset()
                    self.install()
                    (self.root / name).chmod(0o644 if mode == "100755" else 0o755)
                    if view == "index":
                        self.git("add", name)
                        (self.root / name).chmod(0o755 if mode == "100755" else 0o644)
                    self.reject("mode mismatch|index path/mode/stage")

    def test_unmerged_and_gitlink_index_entries_fail(self):
        name = FILES[2][0]
        for mode in ("unmerged", "gitlink"):
            self.reset()
            self.install()
            self.git("update-index", "--force-remove", name)
            if mode == "unmerged":
                blob = FILES[2][3]
                self.git("update-index", "--index-info", input=(
                    f"100644 {blob} 1\t{name}\n100644 {blob} 2\t{name}\n").encode())
            else:
                self.git("update-index", "--add", "--cacheinfo", "160000", MAIN, name)
            self.reject("unmerged index|index path/mode/stage")

    def test_unrelated_ada_declarations_fail(self):
        self.install()
        path = self.root / "ada/src/ams.ads"
        path.write_text(path.read_text().replace("package AMS is", "package AMS is\n X : Integer;"))
        self.assertIn("X : Integer;", path.read_text())
        self.git("add", ".")
        self.reject("executable Ada tokens changed")

    def test_unrelated_native_production_and_mock_fail(self):
        for name in ("native/src/ir_stream.cpp", "native/tests/mock_provider.cpp",
                     "native/src/exports.map", "native/vendor/ams-math/LICENSE"):
            self.reset()
            self.install()
            path = self.root / name
            path.write_bytes(path.read_bytes() + b"\n")
            self.git("add", name)
            self.reject("production bytes changed|unlisted instrumentation")

    def test_unrelated_production_additions_in_all_languages_fail(self):
        for name in ("ada/src/unapproved.ads", "native/src/unapproved.cpp",
                     "python/ams_mel/unapproved.py", "rust/unapproved.rs"):
            self.reset()
            self.install()
            path = self.root / name
            path.write_text("unapproved\n")
            self.git("add", name)
            self.reject("unauthorized tracked files added")

    def test_protected_documentation_assertions_fail_qualification(self):
        # docs/ additions are historically allowed by the source gate. The
        # independent documentation qualifier owns their frozen test identities.
        # Static immutable method identities suffice for this rejection control;
        # actual discovery/execution remains a separate mandatory gate. Avoid
        # importing enhancer tests into this stdlib-only standalone runner.
        groups = json.loads(source_git("show", documentation_ci.CI001_SHA +
                           ":docs/tests/ci001-inherited-inventory.json"))
        for group, name, revision in (
                ("scripts", "scripts/test_doc_source_integrity.py", documentation_ci.IR_CI003_SHA),
                ("enhanced", "docs/tests/test_navigation_completion_observation.py", documentation_ci.IR_CI003_SHA),
                ("enhanced", "docs/tests/test_documentation_ci.py", MAIN)):
            methods = documentation_ci.source_methods(source_git("show", revision + ":" + name),
                                                       Path(name).stem)
            groups[group] = sorted(set(groups[group]) | set(methods))
        documentation_ci.validate_inventory(groups, root=self.root)
        for name in ("scripts/test_doc_source_integrity.py", "docs/tests/test_documentation_ci.py"):
            self.reset()
            self.install()
            path = self.root / name
            original = path.read_bytes()
            changed = original.replace(b"self.assertEqual(", b"self.assertNotEqual(", 1)
            self.assertNotEqual(changed, original)
            path.write_bytes(changed)
            self.git("add", name)
            with self.assertRaises(ValueError):
                documentation_ci.validate_inventory(groups, root=self.root)

    def test_forged_documentation_manifest_cannot_authorize_bytes(self):
        self.install()
        path = self.root / FILES[2][0]
        path.write_bytes(path.read_bytes() + b"\n")
        (self.root / "docs/irmel-authorization.json").write_text(json.dumps({
            "approved": True, "base": CANDIDATE,
            "source_sha256": {FILES[2][0]: hashlib.sha256(path.read_bytes()).hexdigest()}}) + "\n")
        self.git("add", ".")
        self.reject("unapproved complete bytes")

    def test_historical_rf_ir_states_still_valid(self):
        paths = ("native/tests/mock_rf_provider.cpp", "native/tests/test_rf_job_interval_status.c",
                 "native/tests/test_ir_stream.c", "native/tests/mock_provider.cpp",
                 "native/tests/test_ir_instrumentation.c", "native/tests/test_ir_navigation.c")
        for revision in (FOUNDATION, "5c136a04ff64d980df7aaa2bd1ee49e1ed80daaf", MAIN):
            self.reset()
            self.install()
            self.git("restore", "--source=" + revision, "--staged", "--worktree", "--", *paths)
            result = policy.check(self.root)
            self.assertEqual(result["irmel_pol_001"], "CANDIDATE")
            self.assertEqual(result["reviewed_native_tests"],
                             "original" if revision == FOUNDATION else "approved")

    def test_replaced_foundation_argument_cannot_authorize(self):
        self.install()
        for base in (MAIN, CANDIDATE):
            with self.assertRaisesRegex(ValueError, "immutable foundation argument changed"):
                policy.check(self.root, base)
        result = subprocess.run([sys.executable, str(ROOT / "scripts/check_doc_source_integrity.py"),
                                 "--base", CANDIDATE], capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"invalid choice", result.stderr)

    def test_duplicate_overlapping_contradictory_records_fail(self):
        records = policy.IRMEL_POL_001_TRANSITION
        bad_records = (records + (records[0],),
                       (records[0],) + records[:-1],
                       (("native/tests/test_ir_stream.c", *records[0][1:]),) + records[1:],
                       ((records[0][0], records[0][1], records[0][2], records[0][2]),) + records[1:])
        for bad in bad_records:
            with patch.object(policy, "IRMEL_POL_001_TRANSITION", bad):
                self.reject("duplicate or overlapping|contradictory")

    def test_untracked_required_script_not_silently_ignored(self):
        name = FILES[2][0]
        (self.root / name).write_bytes(self.candidate[name])
        self.reject("untracked audit script")


if __name__ == "__main__":
    unittest.main(verbosity=2)
