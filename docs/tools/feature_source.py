"""POL-002 Git-object source qualification; mechanical evidence, never approval.

Run this module from a trusted target checkout, not a feature's policy files.
The CLI binds target authority to origin/main, not a supplied scope manifest.
"""

import argparse
import difflib
import hashlib
import json
from pathlib import Path
import stat
import subprocess


HISTORICAL_MAIN = "5c136a04ff64d980df7aaa2bd1ee49e1ed80daaf"
HISTORICAL_TREE = "0073b8627bab4f8409438fdf7102ba5e461fb439"
# Exact surfaces, not directory exemptions. New filenames need policy review.
RF_PRODUCT_RX = frozenset({
    "native/include/ams_mel/abi.h", "native/src/exports.map",
    "native/src/rf_product_rx.cpp", "native/tests/test_rf_product_rx.c",
    "native/tests/mock_rf_provider.cpp", "native/CMakeLists.txt",
    "native/tests/product_rx_v10_layout_probe.c",
    "native/tests/rf_product_rx_receive_events_contract_probe.cpp",
    "ada/src/ams-mel-rf-product_rx.ads", "ada/src/ams-mel-rf-product_rx.adb",
    "ada/src/ams-mel_c_api.ads",
    "ada/tests/src/ams_mel_rf_product_rx_tests.adb",
    "ada/tests/src/ams-mel-rf-product_rx-testing.ads",
    "ada/tests/src/ams-mel-rf-product_rx-testing.adb",
    "rust/ams-mel-sys/src/lib.rs", "rust/ams-mel-sys/tests/abi.rs",
    "rust/ams-mel-sys/tests/abi_probe.c",
    "python/ams_mel/_native.py", "python/tests/test_abi.py",
    "docs/task-034f26-rf-product-rx-receive-events.md",
    "docs/coverage.md", "docs/architecture.md", "docs/c-abi-policy.md",
    "ada/README.md", "CHANGELOG.md",
})
NEW_RF_FILES = frozenset({
    "native/tests/product_rx_v10_layout_probe.c",
    "native/tests/rf_product_rx_receive_events_contract_probe.cpp",
    "docs/task-034f26-rf-product-rx-receive-events.md",
})
POLICY_FILES = (
    "docs/tools/feature_source.py", "docs/tools/source_epoch.py",
    "docs/tools/qualify_feature_source.py",
    "docs/tools/epoch_ci.py", "docs/tests/pol002_epoch_tests.py",
    "docs/tests/pol002_source_tests.py",
    "docs/tools/build_enhanced_site.py", "docs/tools/documentation_ci.py",
    "docs/tools/local_type_audit.py", "docs/tools/symbol_audit.py",
    "docs/tools/api_catalog.py", "docs/tools/ada_examples.py",
    "scripts/check_doc_source_integrity.py", "scripts/check_doc_coverage.py",
    "scripts/check_doc_baseline_delta.py", "scripts/build_docs_portal.py",
    "scripts/generate_docs_baseline.sh", ".github/workflows/documentation.yml",
)
MOCK = "native/tests/mock_rf_provider.cpp"
PARTNER = "native/tests/test_rf_job_interval_status.c"
APPROVED_PAIR = (
    "ef8c1dc59987ea2ff16cb00d6a60880ab560b910127105b6dd7698498be9d957",
    "96f71931cbf3c03937d352997666ad459db9f517d514c7827579bbfa22a62930",
)


def require(condition, message):
    if not condition:
        raise ValueError("source qualification: " + message)


def git(root, *args):
    return subprocess.check_output(["git", "-C", str(root), *args])


def revision(root, ref):
    return git(root, "rev-parse", "--verify", ref + "^{commit}").decode().strip()


def tree(root, ref):
    result = {}
    for record in git(root, "ls-tree", "-rz", ref).split(b"\0"):
        if record:
            info, name = record.split(b"\t", 1)
            mode, kind, oid = info.decode().split()
            require(kind == "blob", "non-blob tracked object")
            result[name.decode()] = (mode, oid)
    return result


def blob(root, ref, name):
    return git(root, "show", ref + ":" + name)


def exact_checkout(root, candidate, objects):
    require(revision(root, "HEAD") == candidate, "candidate is not checkout HEAD")
    index = {}
    for record in git(root, "ls-files", "--stage", "-z").split(b"\0"):
        if record:
            info, name = record.split(b"\t", 1)
            mode, oid, stage = info.decode().split()
            require(stage == "0", "unmerged index")
            index[name.decode()] = (mode, oid)
    require(index == objects, "index differs from committed candidate")
    for name, (mode, oid) in objects.items():
        path = root / name
        require(mode in {"100644", "100755"}, "symlink/nonregular Git mode: " + name)
        require(not any(p.is_symlink() for p in (path, *path.parents)), "symlink: " + name)
        require(path.is_file() and stat.S_ISREG(path.stat().st_mode), "missing/nonregular: " + name)
        require(bool(path.stat().st_mode & 0o111) == (mode == "100755"), "file mode differs: " + name)
        require(path.read_bytes() == git(root, "cat-file", "blob", oid), "working tree differs: " + name)
    # Ignored build products are not source; untracked source must not affect tools.
    require(not git(root, "ls-files", "--others", "--exclude-standard", "-z"), "untracked candidate input")


def documentation_path(name):
    return (name.startswith("docs/") and name.endswith((".md", ".html"))
            and not name.startswith(("docs/tools/", "docs/tests/", "docs/examples/"))) or name == "README.md"


def shared_surface_delta(root, target, candidate, name):
    """Constrain shared files without approving unknown future implementation.

    Shared binding/ABI files only admit additive v10 records and exports. Existing
    declarations cannot be removed/replaced. Mock/build changes require a later
    exact-content transition because a path alone cannot distinguish RF subsystems.
    """
    if name == MOCK:
        return "requires-exact-content-mock-review"
    shared = {"native/include/ams_mel/abi.h", "native/src/exports.map",
              "ada/src/ams-mel_c_api.ads", "rust/ams-mel-sys/src/lib.rs",
              "python/ams_mel/_native.py", "native/CMakeLists.txt"}
    if name not in shared:
        return None
    old = blob(root, target, name).decode().splitlines(keepends=True)
    new = blob(root, candidate, name).decode().splitlines(keepends=True)
    changes = [op for op in difflib.SequenceMatcher(None, old, new, autojunk=False).get_opcodes() if op[0] != 'equal']
    require(all(op[0] == 'insert' for op in changes), "shared surface must retain every historical line: " + name)
    if name == "native/src/exports.map":
        inserted = ''.join(''.join(new[b:e]) for _, _, _, b, e in changes)
        require(set(inserted.split()) <= {"ams_mel_rf_data_submit_product_rx_v10;",
                                         "ams_mel_rf_product_rx_event_view_v10;"},
                "unrelated production export")
    return "requires-exact-content-shared-interface-review"


def qualify(root, target, candidate, *, check_checkout=True, documentation_only=False):
    """Pure exact-object comparison. Caller must establish trusted target authority.

    Unit fixtures pass their committed target explicitly. Hosted callers must use
    trusted_target(), and execute this implementation from that target's Git blob.
    No result of this function is an independently reviewed source epoch.
    """
    root = Path(root).absolute()
    target, candidate = revision(root, target), revision(root, candidate)
    require(subprocess.run(["git", "-C", str(root), "merge-base", "--is-ancestor",
                            target, candidate], capture_output=True).returncode == 0,
            "target is not candidate ancestor; synchronize by normal merge")
    old, new = tree(root, target), tree(root, candidate)
    if check_checkout:
        exact_checkout(root, candidate, new)
    changed = sorted(n for n in old.keys() | new.keys() if old.get(n) != new.get(n))
    review_obligations = []
    for name in changed:
        require(name in new, "source removal is not authorized: " + name)
        require(new[name][0] == old.get(name, ("100644",))[0], "mode transition: " + name)
        require(documentation_path(name) or name in RF_PRODUCT_RX, "out-of-scope delta: " + name)
        require(name in old or name in NEW_RF_FILES or documentation_path(name), "unauthorized new source: " + name)
        if name in old:
            obligation = shared_surface_delta(root, target, candidate, name)
            if obligation:
                review_obligations.append({"path": name, "obligation": obligation})
    feature = any(not documentation_path(n) for n in changed)
    require(not documentation_only or not feature, "documentation-only qualification changed executable source")
    for name in POLICY_FILES:
        require(old.get(name) == new.get(name), "candidate changes qualification policy: " + name)
    if MOCK in changed:
        for name, expected in zip((MOCK, PARTNER), APPROVED_PAIR):
            require(hashlib.sha256(blob(root, target, name)).hexdigest() == expected,
                    "mock transition needs complete approved POL-001 target pair")
        require(old[PARTNER] == new[PARTNER], "POL-001 partner must remain approved and unchanged")
    # Shared ABI/binding/build/mock files are eligible review surfaces, not proof
    # that a changed hunk belongs to ProductRx. Their complete Git delta remains
    # explicit review input; mechanical qualification cannot grant content approval.
    return {"schema": 1, "case": "feature-source" if feature else "documentation-only",
            "target_commit": target, "target_tree": git(root, "rev-parse", target + "^{tree}").decode().strip(),
            "candidate_commit": candidate, "candidate_tree": git(root, "rev-parse", candidate + "^{tree}").decode().strip(),
            "delta": [{"path": n, "old": list(old[n]) if n in old else None,
                       "new": list(new[n])} for n in changed],
            "review_obligations": review_obligations,
            "independent_approval": False, "publication_epoch_approved": False}


def trusted_target(root):
    """Only fetched origin/main is a CLI authority; an arbitrary SHA is not."""
    target = revision(root, "refs/remotes/origin/main")
    require(subprocess.run(["git", "-C", str(root), "merge-base", "--is-ancestor",
                            HISTORICAL_MAIN, target], capture_output=True).returncode == 0,
            "target lacks historical reviewed main")
    require(git(root, "rev-parse", HISTORICAL_MAIN + "^{tree}").decode().strip() == HISTORICAL_TREE,
            "historical main tree differs")
    require(blob(root, target, "docs/tools/feature_source.py") == Path(__file__).read_bytes(),
            "qualifier is not trusted target implementation")
    return target


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd())
    args = parser.parse_args()
    print(json.dumps(qualify(args.root, trusted_target(args.root), "HEAD"), indent=2))
