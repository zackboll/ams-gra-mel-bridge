"""Git-bound source identity for ordinary development, not per-file permission.

Source and tests may evolve together. Historical qualification belongs to
isolated immutable checkouts, never comparisons against current source bytes.
"""

import argparse
import json
from pathlib import Path
import stat
import subprocess

HISTORICAL_MAIN = "5c136a04ff64d980df7aaa2bd1ee49e1ed80daaf"
HISTORICAL_TREE = "0073b8627bab4f8409438fdf7102ba5e461fb439"


def require(condition, message):
    if not condition:
        raise ValueError("source qualification: " + message)


def git(root, *args):
    return subprocess.check_output(["git", "--no-replace-objects", "-C", str(root), *args])


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
    return name.endswith((".md", ".rst", ".html", ".css"))


def qualify(root, target, candidate, *, check_checkout=True, documentation_only=False):
    """Record any regular source delta; tests and code review decide compatibility."""
    target, candidate = revision(root, target), revision(root, candidate)
    old, new = tree(root, target), tree(root, candidate)
    if check_checkout:
        exact_checkout(root, candidate, new)
    changed = sorted(n for n in old.keys() | new.keys() if old.get(n) != new.get(n))
    for name, (mode, _) in new.items():
        require(mode in {"100644", "100755"}, "nonregular source: " + name)
    feature = any(not documentation_path(n) for n in changed)
    require(not documentation_only or not feature, "documentation-only qualification changed executable source")
    return {"schema": 1, "case": "feature-source" if feature else "documentation-only",
            "target_commit": target, "target_tree": git(root, "rev-parse", target + "^{tree}").decode().strip(),
            "candidate_commit": candidate, "candidate_tree": git(root, "rev-parse", candidate + "^{tree}").decode().strip(),
            "delta": [{"path": n, "old": list(old[n]) if n in old else None,
                       "new": list(new[n]) if n in new else None} for n in changed]}


def trusted_target(root):
    """Resolve a comparison baseline, not a source-admission authority."""
    return revision(root, "refs/remotes/origin/main")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd())
    args = parser.parse_args()
    print(json.dumps(qualify(args.root, trusted_target(args.root), "HEAD"), indent=2))
