#!/usr/bin/env python3
"""Protect tracked inventory, Ada tokens and production bytes for DOC-001."""

import argparse
import hashlib
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASE = "42648a92eb0cc84b2980d16db6028992c0494ba1"
# Explicit DOC-001 authorization, not a blanket scripts/ or workflow allowance.
DOCUMENTATION_ADDITIONS = frozenset({
    "docs_api.gpr", "docs_internal.gpr", ".github/workflows/documentation.yml",
    "scripts/check_doc_baseline_delta.py", "scripts/check_doc_coverage.py",
    "scripts/check_doc_source_integrity.py", "scripts/classify_gnatdoc_warnings.py",
    "scripts/compare_gnatdoc_warnings.py", "scripts/generate_docs_baseline.sh",
    "scripts/gnatdoc_coverage_model.py", "scripts/gnatdoc_inventory.py",
    "scripts/test_compare_gnatdoc_warnings.py", "scripts/test_doc_foundation.py",
    "scripts/test_doc_source_integrity.py", "scripts/test_gnatdoc_inventory.py",
    "scripts/validate_docs.py",
    "scripts/build_docs_portal.py", "scripts/test_docs_portal.py",
})
# POL-001: only the complete PR #111 pair at
# 048387edee98322f6df2d7dadb14d94f2c022ef9 is approved. These are whole-file
# SHA-256 identities, not a manifest, path exemption or new foundation base.
REVIEWED_NATIVE_TEST_TRANSITION = (
    ("native/tests/mock_rf_provider.cpp",
     "cab91b7cf0901fc5ba6dc7e013b22c9bc88a5062ce9dc324b0f8e9aa359865b0",
     "ef8c1dc59987ea2ff16cb00d6a60880ab560b910127105b6dd7698498be9d957"),
    ("native/tests/test_rf_job_interval_status.c",
     "684e01b689d7cf0464168dda7cca54df2ff0377f65e250a9273d18318c97735e",
     "96f71931cbf3c03937d352997666ad459db9f517d514c7827579bbfa22a62930"),
)
TOKEN = re.compile(
    r'(?P<space>\s+)|(?P<comment>--[^\n]*)|(?P<string>"(?:""|[^"])*")|'
    r"(?P<char>'[^\n]')|(?P<word>[A-Za-z][A-Za-z0-9_]*|[0-9][A-Za-z0-9_#.]*)|"
    r"(?P<op>:=|=>|\.\.|\*\*|/=|<=|>=|<<|>>|<>)|(?P<punct>.)", re.S)


def tokens(text):
    return [match[0] for match in TOKEN.finditer(text)
            if match.lastgroup not in ("space", "comment")]


def native_profile(text):
    result, depth = [], 0
    for token in tokens(text):
        if depth == 0 and token.lower() in ("with", "renames"):
            break
        if token == "(":
            depth += 1
        elif token == ")":
            depth -= 1
        result.append(token)
    while result and result[-1] == ";":
        result.pop()
    return result


def reviewed_native_test_state(root, git, existing, current, base):
    paths = {name for name, _, _ in REVIEWED_NATIVE_TEST_TRANSITION}
    if not paths.intersection(existing):
        # Small committed unit-test fixtures need not contain this real pair.
        # Additions remain subject to the ordinary inventory check.
        return None
    if not paths.issubset(existing):
        raise ValueError("reviewed native-test foundation pair incomplete")
    if not paths.issubset(current):
        raise ValueError("reviewed native-test index pair missing")
    states = {"working tree": [], "index": []}
    for name, original, approved in REVIEWED_NATIVE_TEST_TRANSITION:
        if hashlib.sha256(git("show", f"{base}:{name}")).hexdigest() != original:
            raise ValueError(f"reviewed native-test foundation bytes changed: {name}")
        path = root / name
        if path.is_symlink() or not path.is_file():
            raise ValueError(f"reviewed native-test working tree file missing or symlink: {name}")
        entry = git("ls-files", "--stage", "-z", "--", name).split(b"\0")
        if (len(entry) != 2 or not entry[0].startswith(b"100644 ") or
                entry[0].split(b"\t", 1)[0].split()[-1] != b"0"):
            raise ValueError(f"reviewed native-test index entry is not a regular stage-0 file: {name}")
        for view, content in (("working tree", path.read_bytes()),
                              ("index", git("show", f":{name}"))):
            digest = hashlib.sha256(content).hexdigest()
            if digest not in (original, approved):
                raise ValueError(f"unapproved native-test {view} bytes: {name}")
            states[view].append("original" if digest == original else "approved")
    for view, pair in states.items():
        if len(set(pair)) != 1:
            raise ValueError(f"mixed reviewed native-test pair in {view}")
    if states["working tree"] != states["index"]:
        raise ValueError("reviewed native-test working tree/index state mismatch")
    return states["working tree"][0]


def check(root=ROOT, base=BASE):
    def git(*args):
        return subprocess.check_output(["git", "-C", str(root), *args])
    existing = set(git("ls-tree", "-rz", "--name-only", base).decode().split("\0")) - {""}
    current = set(git("ls-files", "-z", "--cached").decode().split("\0")) - {""}
    unexpected = {name for name in current - existing
                  if not name.startswith("docs/") and name not in DOCUMENTATION_ADDITIONS}
    if unexpected:
        raise ValueError(f"unauthorized tracked files added: {sorted(unexpected)!r}")
    transition = reviewed_native_test_state(root, git, existing, current, base)
    reviewed_paths = {name for name, _, _ in REVIEWED_NATIVE_TEST_TRANSITION}
    original_ada = {name for name in existing if name.startswith(("ada/src/", "ada/tests/src/"))
                    and Path(name).suffix in (".ads", ".adb")}
    current_ada = {path.relative_to(root).as_posix() for directory in ("ada/src", "ada/tests/src")
                   for path in (root / directory).rglob("*")
                   if path.is_file() and path.suffix in (".ads", ".adb")}
    if original_ada != current_ada:
        raise ValueError(f"Ada source inventory changed: {original_ada ^ current_ada}")
    checked = 0
    for name in sorted(existing):
        if name.startswith("docs/") or name in (".gitignore", "README.md"):
            continue
        if name in reviewed_paths:
            # Both views of the complete pair were already checked above.
            checked += 1
            continue
        path = root / name
        old = git("show", f"{base}:{name}")
        if not path.is_file():
            raise ValueError(f"production file removed: {name}")
        new = path.read_bytes()
        if name.startswith(("ada/src/", "ada/tests/src/")) and path.suffix in (".ads", ".adb"):
            if tokens(old.decode()) != tokens(new.decode()):
                raise ValueError(f"executable Ada tokens changed: {name}")
        elif old != new:
            raise ValueError(f"production bytes changed: {name}")
        checked += 1
    return {"base": base, "protected_files": checked,
            "reviewed_native_tests": transition}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", default=BASE, choices=(BASE,))
    args = parser.parse_args()
    print(check(base=args.base))
