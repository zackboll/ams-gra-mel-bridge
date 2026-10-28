#!/usr/bin/env python3
"""Protect tracked inventory, Ada tokens and production bytes for DOC-001."""

import argparse
import hashlib
import re
import stat
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
# IR-CI-001 PROPOSED exact-content authorization, pending independent review.
# A singleton independent of POL-001's approved atomic RF pair. Mechanical
# acceptance of these bytes is not independent approval of this proposal.
PROPOSED_IR_TEST_TRANSITION = (
    "native/tests/test_ir_stream.c",
    "6a2bb092ce231dd794769194476500e9498f50d34bb30f8c290784a76e39d46b",
    "fa724b84a4c0ac06e77ffedd4fea81b225e20a0d5a28e12d02eaa2b1a277d400",
)
# IR-CI-004 successor proposal only; not independent source-policy approval.
# Preserve IR-CI-001 above as historical authority. Only this exact complete
# successor of reviewed main 5c136a04ff64d980df7aaa2bd1ee49e1ed80daaf is added.
PROPOSED_IR_COUNTER_TRANSITION = (
    "native/tests/test_ir_stream.c",
    "fa724b84a4c0ac06e77ffedd4fea81b225e20a0d5a28e12d02eaa2b1a277d400",
    "bad90aacb6e19ea19f965c56e07d7856ae1a04696e1a900c90cf91641ba71e32",
)
# IR-CI-002 R2: atomic Instrumentation proposal, NOT independent approval.
# The failed R1 C-test identity is historical evidence, not a third valid state.
PROPOSED_INSTRUMENTATION_TRANSITION = (
    ("native/tests/mock_provider.cpp",
     "8c41c152f18eb9e96c1219d7c4073324719b5364d3a8fa458a59f396dda51666",
     "5c816a106bd310443620921979a99ccda6ad830102ac9c65a63766ff4fd95b37"),
    ("native/tests/test_ir_instrumentation.c",
     "49deb479383a43f0eba2528b67bc595618dafd7f750de990f6343184b86fa179",
     "617b95eae28527b383b4d05708dfc667e633047b88313a038272b52c2221eb02"),
)
# IR-CI-003: independent exact navigation-test proposal, NOT review approval.
# Retain every RF/IR/Instrumentation transition above without expansion.
PROPOSED_NAVIGATION_TRANSITION = (
    "native/tests/test_ir_navigation.c",
    "56265cf8a44f1b054777b98457896dc338a79bb306b591b6ee8065c23b3ce224",
    "7f5aaa15d731f3b5a8b429d42c39565b999a308e1428c01e3ca7c029fe2a36c5",
)
# IRMEL-POL-001: exact PR #129 source unit, mechanically admissible only.
# Independent review/branch protection, not this checker, authorize merging.
# Verified from main 33720d155d99f183229b6ed87037ecbd53a3875b and candidate
# 47630e61d2631effbbbe3e222e998e33daa90f60. None means absent, not arbitrary.
# (path, Git mode, original (blob, SHA-256), candidate (blob, SHA-256)).
IRMEL_POL_001_TRANSITION = (
    ("Makefile", "100644",
     ("e4c3441a32169c6474dedef00a2db1f798375970",
      "7cf0c004e277af70766685f91663c1acde5980fcd1037b42c1092e1c310a5236"),
     ("7b608a960241c87f88819dbe2cd6bbe42cdbdebd",
      "446ed30875726cc7dd45e2ca59a48254fed65e5036c0de1542dd5d3c38803ba8")),
    ("scripts/test_ada.sh", "100755",
     ("d32e162093a94ba77ab482cb53e98d91e31ee29a",
      "b3d19397440873dbf427fe77663083dc99ab5f13e14cd937e0a250fafb09af66"),
     ("9d35abb1345aa8b6a71a9d7b1229f19c16d33239",
      "21ec1d4545f0ef2e6dbb0af110bfa9083b1e102e9b2f32ee6b1e9dedd5ee9acf")),
    ("scripts/check_ir_api_coverage.py", "100644", None,
     ("6775d2e9986d3b59233e7c464745a1482d047f81",
      "6a23700ea3eaa7de8ed7f57d5dcd43d38fc7c5cdd64f67bbf9ad7494202e9e31")),
    ("scripts/freeze_ir_api_evidence.py", "100644", None,
     ("48af395041dd3babb1a1df378e61643064ff91b0",
      "a8f371013211ffaba2f0723388b50acb79f24b8c4abe5e1748b42a6dc38abc9e")),
    ("scripts/test_ir_api_coverage.py", "100644", None,
     ("fa96e0ceb5c16b911fdb0acdfec15b1b53ca981a",
      "96ff71c641673f36ea90a9c1a7707e2eae367000c0e9a16dd3bd4bc1d621b333")),
)
# CI-002 exact complete-file proposal; mechanical acceptance is NOT approval.
# Superseded CI-002 HEAD-only proposal (historical evidence, NOT admitted):
# e42d0031368885935b5157babe9f9b2063f157842a306a25995130789fb0fe8c
PROPOSED_BOOTSTRAP_TRANSITION = (
    ".github/workflows/ci.yml",
    "47ec6d285239ce84ca89c3ec303561c539158c2fa491abfa1adc0769605df28e",
    "c89a50fab57f9bbb7653cf26a8b0260f72d9436c1ebde188dde4b29f257514f2",
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


def proposed_ir_test_state(root, git, existing, current, base):
    name, original, corrected = PROPOSED_IR_TEST_TRANSITION
    successor_name, predecessor, successor = PROPOSED_IR_COUNTER_TRANSITION
    if successor_name != name or predecessor != corrected:
        raise ValueError("proposed IR-test successor predecessor mismatch")
    if name not in existing:
        # Minimal fixtures can omit IR; ordinary inventory rules still apply.
        return None
    if hashlib.sha256(git("show", f"{base}:{name}")).hexdigest() != original:
        raise ValueError("proposed IR-test foundation bytes changed")
    if name not in current:
        raise ValueError("proposed IR-test index file missing")
    path = root / name
    if path.is_symlink() or not path.is_file():
        raise ValueError("proposed IR-test working tree file missing or symlink")
    if path.stat().st_mode & 0o111:
        raise ValueError("proposed IR-test working tree file executable")
    entry = git("ls-files", "--stage", "-z", "--", name).split(b"\0")
    if (len(entry) != 2 or not entry[0].startswith(b"100644 ") or
            entry[0].split(b"\t", 1)[0].split()[-1] != b"0"):
        raise ValueError("proposed IR-test index entry is not a regular stage-0 file")
    states = []
    for view, content in (("working tree", path.read_bytes()),
                          ("index", git("show", f":{name}"))):
        digest = hashlib.sha256(content).hexdigest()
        if digest not in (original, corrected, successor):
            raise ValueError(f"unlisted IR-test {view} bytes")
        states.append("original" if digest == original else
                      "proposed_pending_independent_review" if digest == corrected else
                      "counter_observation_proposed_pending_independent_review")
    if states[0] != states[1]:
        raise ValueError("proposed IR-test working tree/index state mismatch")
    return states[0]


def proposed_instrumentation_state(root, git, existing, current, base):
    paths = {name for name, _, _ in PROPOSED_INSTRUMENTATION_TRANSITION}
    if not paths.intersection(existing):
        return None  # Ordinary addition rules still deny adding either path.
    if not paths.issubset(existing):
        raise ValueError("instrumentation foundation pair incomplete")
    if not paths.issubset(current):
        raise ValueError("instrumentation index pair missing")
    states = {"working tree": [], "index": []}
    for name, original, proposed in PROPOSED_INSTRUMENTATION_TRANSITION:
        if hashlib.sha256(git("show", f"{base}:{name}")).hexdigest() != original:
            raise ValueError("instrumentation foundation bytes changed: " + name)
        path = root / name
        if path.is_symlink() or not path.is_file():
            raise ValueError("instrumentation working tree missing or nonregular: " + name)
        entry = git("ls-files", "--stage", "-z", "--", name).split(b"\0")
        if (len(entry) != 2 or not entry[0].startswith(b"100644 ") or
                entry[0].split(b"\t", 1)[0].split()[-1] != b"0"):
            raise ValueError("instrumentation index entry is not a regular stage-0 file: " + name)
        for view, data in (("working tree", path.read_bytes()),
                           ("index", git("show", f":{name}"))):
            digest = hashlib.sha256(data).hexdigest()
            if digest not in (original, proposed):
                raise ValueError(f"unlisted instrumentation {view} bytes: {name}")
            states[view].append("original" if digest == original else
                                "proposed_pending_independent_review")
    for view, pair in states.items():
        if len(set(pair)) != 1:
            raise ValueError("mixed instrumentation pair in " + view)
    if states["working tree"] != states["index"]:
        raise ValueError("instrumentation working tree/index state mismatch")
    return states["working tree"][0]


def proposed_navigation_state(root, git, existing, current, base):
    name, original, corrected = PROPOSED_NAVIGATION_TRANSITION
    if name not in existing:
        return None  # Ordinary inventory rules still reject adding this path.
    if hashlib.sha256(git("show", f"{base}:{name}")).hexdigest() != original:
        raise ValueError("proposed Navigation-test foundation bytes changed")
    if name not in current:
        raise ValueError("proposed Navigation-test index file missing")
    path = root / name
    if path.is_symlink() or not path.is_file() or path.stat().st_mode & 0o111:
        raise ValueError("proposed Navigation-test working tree is not a regular nonexecutable file")
    entry = git("ls-files", "--stage", "-z", "--", name).split(b"\0")
    if (len(entry) != 2 or not entry[0].startswith(b"100644 ") or
            entry[0].split(b"\t", 1)[0].split()[-1] != b"0"):
        raise ValueError("proposed Navigation-test index entry is not a regular stage-0 file")
    states = []
    for view, content in (("working tree", path.read_bytes()),
                          ("index", git("show", f":{name}"))):
        digest = hashlib.sha256(content).hexdigest()
        if digest not in (original, corrected):
            raise ValueError(f"unlisted Navigation-test {view} bytes")
        states.append("original" if digest == original else
                      "proposed_pending_independent_review")
    if states[0] != states[1]:
        raise ValueError("proposed Navigation-test working tree/index state mismatch")
    return states[0]


def irmel_pol_001_state(root, git, existing, current, base):
    """Recognize two complete states; never read authority from candidate docs."""
    records = IRMEL_POL_001_TRANSITION
    expected = {"Makefile", "scripts/test_ada.sh", "scripts/check_ir_api_coverage.py",
                "scripts/freeze_ir_api_evidence.py", "scripts/test_ir_api_coverage.py"}
    names = [record[0] for record in records]
    historical = ({name for name, _, _ in REVIEWED_NATIVE_TEST_TRANSITION} |
                  {PROPOSED_IR_TEST_TRANSITION[0], PROPOSED_IR_COUNTER_TRANSITION[0],
                   PROPOSED_NAVIGATION_TRANSITION[0]} |
                  {name for name, _, _ in PROPOSED_INSTRUMENTATION_TRANSITION})
    if (len(names) != 5 or set(names) != expected or
            set(names) & (historical | DOCUMENTATION_ADDITIONS)):
        raise ValueError("IRMEL-POL-001 duplicate or overlapping authorization paths")
    for name, mode, original, candidate in records:
        if (mode != ("100755" if name == "scripts/test_ada.sh" else "100644") or
                (original is None) != (name not in {"Makefile", "scripts/test_ada.sh"}) or
                original == candidate):
            raise ValueError("IRMEL-POL-001 contradictory authorization states")
        for identity in (original, candidate):
            if identity is not None and (len(identity) != 2 or
                    not re.fullmatch(r"[0-9a-f]{40}", identity[0]) or
                    not re.fullmatch(r"[0-9a-f]{64}", identity[1])):
                raise ValueError("IRMEL-POL-001 invalid content identity")
    originals = {name for name, _, original, _ in records if original is not None}
    if not originals.intersection(existing):
        # Preserve historical minimal fixtures; no source-addition exemption.
        return None
    if base != BASE:
        raise ValueError("IRMEL-POL-001 immutable foundation argument changed")
    for name, mode, original, _ in records:
        entry = git("ls-tree", "-z", base, "--", name)
        if original is None:
            if entry:
                raise ValueError("IRMEL-POL-001 foundation addition already exists: " + name)
        elif (entry != f"{mode} blob {original[0]}\t{name}\0".encode() or
              hashlib.sha256(git("show", f"{base}:{name}")).hexdigest() != original[1]):
            raise ValueError("IRMEL-POL-001 immutable foundation identity changed: " + name)
    states = []
    for name, mode, original, candidate in records:
        path = root / name
        entries = git("ls-files", "--stage", "-z", "--", name).split(b"\0")
        if entries == [b""] and original is None:
            if path.exists() or path.is_symlink():
                raise ValueError("IRMEL-POL-001 untracked audit script: " + name)
            states.append("ORIGINAL")
            continue
        if (name not in current or len(entries) != 2 or entries[1] != b"" or
                b"\t" not in entries[0]):
            raise ValueError("IRMEL-POL-001 missing or unmerged index entry: " + name)
        metadata, indexed_name = entries[0].split(b"\t", 1)
        fields = metadata.split()
        if (len(fields) != 3 or fields[0] != mode.encode() or fields[2] != b"0" or
                indexed_name != name.encode()):
            raise ValueError("IRMEL-POL-001 index path/mode/stage mismatch: " + name)
        # lstat before reading prevents symlink, FIFO, directory or device reads.
        if (any((root / parent).is_symlink() for parent in path.relative_to(root).parents
                if parent != Path(".")) or path.is_symlink() or
                not path.exists() or not stat.S_ISREG(path.lstat().st_mode)):
            raise ValueError("IRMEL-POL-001 nonregular working tree path: " + name)
        if bool(path.stat().st_mode & 0o111) != (mode == "100755"):
            raise ValueError("IRMEL-POL-001 working tree executable mode mismatch: " + name)
        working = path.read_bytes()
        indexed = git("show", f":{name}")
        if working != indexed:
            raise ValueError("IRMEL-POL-001 working tree/index bytes differ: " + name)
        identity = (fields[1].decode(), hashlib.sha256(indexed).hexdigest())
        if identity not in (original, candidate):
            raise ValueError("IRMEL-POL-001 unapproved complete bytes: " + name)
        states.append("ORIGINAL" if identity == original else "CANDIDATE")
    if len(set(states)) != 1:
        raise ValueError("IRMEL-POL-001 mixed or partially applied source unit")
    return states[0]


def proposed_bootstrap_state(root, git, existing, current, base):
    name, original, proposed = PROPOSED_BOOTSTRAP_TRANSITION
    if name not in existing:
        return None  # Small fixtures may omit it; additions remain forbidden.
    if hashlib.sha256(git("show", f"{base}:{name}")).hexdigest() != original:
        raise ValueError("Bootstrap foundation bytes changed")
    if name not in current:
        raise ValueError("Bootstrap index file missing")
    path = root / name
    if path.is_symlink() or not path.is_file() or path.stat().st_mode & 0o111:
        raise ValueError("Bootstrap working tree is not regular nonexecutable")
    entries = git("ls-files", "--stage", "-z", "--", name).split(b"\0")
    if (len(entries) != 2 or not entries[0].startswith(b"100644 ") or
            entries[0].split(b"\t", 1)[0].split()[-1] != b"0"):
        raise ValueError("Bootstrap index is not regular stage-0")
    states = []
    for view, content in (("working tree", path.read_bytes()),
                          ("index", git("show", f":{name}"))):
        digest = hashlib.sha256(content).hexdigest()
        if digest not in (original, proposed):
            raise ValueError("unlisted Bootstrap " + view + " bytes")
        states.append("original" if digest == original else
                      "proposed_pending_independent_review")
    if states[0] != states[1]:
        raise ValueError("Bootstrap working tree/index state mismatch")
    return states[0]


def check(root=ROOT, base=BASE):
    def git(*args):
        return subprocess.check_output(["git", "-C", str(root), *args])
    existing = set(git("ls-tree", "-rz", "--name-only", base).decode().split("\0")) - {""}
    current = set(git("ls-files", "-z", "--cached").decode().split("\0")) - {""}
    irmel = irmel_pol_001_state(root, git, existing, current, base)
    irmel_paths = {name for name, _, _, _ in IRMEL_POL_001_TRANSITION} if irmel else set()
    unexpected = {name for name in current - existing
                  if not name.startswith("docs/") and name not in DOCUMENTATION_ADDITIONS
                  and name not in irmel_paths}
    if unexpected:
        raise ValueError(f"unauthorized tracked files added: {sorted(unexpected)!r}")
    transition = reviewed_native_test_state(root, git, existing, current, base)
    ir_transition = proposed_ir_test_state(root, git, existing, current, base)
    instrumentation = proposed_instrumentation_state(root, git, existing, current, base)
    navigation = proposed_navigation_state(root, git, existing, current, base)
    bootstrap = proposed_bootstrap_state(root, git, existing, current, base)
    reviewed_paths = {name for name, _, _ in REVIEWED_NATIVE_TEST_TRANSITION}
    proposed_ir_paths = {PROPOSED_IR_TEST_TRANSITION[0]}
    instrumentation_paths = {name for name, _, _ in PROPOSED_INSTRUMENTATION_TRANSITION}
    navigation_paths = {PROPOSED_NAVIGATION_TRANSITION[0]}
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
        if name in irmel_paths:
            checked += 1  # Exact original/candidate bytes checked in both views.
            continue
        if name in reviewed_paths:
            # Both views of the complete pair were already checked above.
            checked += 1
            continue
        if name in proposed_ir_paths:
            # Both views of this independent singleton were checked above.
            checked += 1
            continue
        if name in instrumentation_paths:
            checked += 1  # Both views of the complete pair were checked above.
            continue
        if name in navigation_paths:
            checked += 1  # Both views of this independent singleton were checked.
            continue
        if name == PROPOSED_BOOTSTRAP_TRANSITION[0]:
            checked += 1  # Exact foundation, index and working bytes checked above.
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
            "reviewed_native_tests": transition,
            "proposed_ir_test": ir_transition,
            "proposed_instrumentation_tests": instrumentation,
            "proposed_navigation_test": navigation,
            "irmel_pol_001": irmel,
            "proposed_bootstrap_workflow": bootstrap}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", default=BASE, choices=(BASE,))
    args = parser.parse_args()
    print(check(base=args.base))
