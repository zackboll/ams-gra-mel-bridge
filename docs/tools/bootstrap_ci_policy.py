#!/usr/bin/env python3
"""Conservative Bootstrap qualification from immutable Git source identities.

No labels, PR text, path filters or user-selected modes authorize smoke tests.
Renames deliberately become delete/add pairs, so both names and modes matter.
"""

import argparse
import json
import os
from pathlib import Path
import re
import subprocess

FULL_STRESS = "FULL_STRESS"
COMPLETE_SMOKE = "COMPLETE_SMOKE"
SHA = re.compile(r"[0-9a-f]{40}")
# Deliberately no docs/** exemption, generated metadata or executable examples.
# New presentation assets require a separate source-reviewed allowlist change.
STATIC_ROOT = frozenset({b"README.md"})
STATIC_PARENTS = frozenset({b"docs", b"docs/guides", b"docs/reference",
                            b"docs/history", b"docs/tasks"})


def require(condition, message):
    if not condition:
        raise ValueError(message)


def git(root, *args):
    return subprocess.check_output(["git", "-C", str(root), *args])


def sha(value):
    require(isinstance(value, str) and SHA.fullmatch(value) is not None,
            "invalid or absent source SHA")
    return value


def records(data):
    require(data.endswith(b"\0"), "truncated NUL-delimited Git output")
    result = data[:-1].split(b"\0")
    require(all(result), "empty Git record")
    return result


def tree(data):
    result = {}
    for record in records(data):
        header, name = record.split(b"\t", 1)
        fields = header.split(b" ")
        require(len(fields) == 3, "invalid tree header")
        mode, kind, oid = fields
        require(re.fullmatch(rb"[0-9]{6}", mode) and
                SHA.fullmatch(oid.decode("ascii")), "invalid tree identity")
        require(kind in (b"blob", b"commit"), "unsupported tree object")
        require(name not in result and not name.startswith(b"/") and
                all(p not in (b"", b".", b"..") for p in name.split(b"/")),
                "duplicate or anomalous tree path")
        result[name] = (mode, oid)
    return result


def delta(before, after):
    return {name: (before.get(name), after.get(name))
            for name in before.keys() | after.keys()
            if before.get(name) != after.get(name)}


def raw_diff(data):
    fields = records(data)
    require(len(fields) % 2 == 0, "incomplete raw diff")
    result = {}
    for header, name in zip(fields[::2], fields[1::2]):
        match = re.fullmatch(rb":([0-9]{6}) ([0-9]{6}) ([0-9a-f]{40}) ([0-9a-f]{40}) ([ADM T])", header)
        require(match is not None, "unsupported raw diff record")
        old_mode, new_mode, old_oid, new_oid, status = match.groups()
        old = None if old_mode == b"000000" else (old_mode, old_oid)
        new = None if new_mode == b"000000" else (new_mode, new_oid)
        require((old is not None or old_oid == b"0" * 40) and
                (new is not None or new_oid == b"0" * 40), "invalid absent object")
        expected = b"A" if old is None else b"D" if new is None else (
            b"T" if old_mode[:2] != new_mode[:2] else b"M")
        require(status == expected and old != new and name not in result,
                "duplicate or anomalous raw diff")
        result[name] = (old, new)
    return result


def static_path(name):
    return name in STATIC_ROOT or (
        name.rpartition(b"/")[0] in STATIC_PARENTS and name.endswith(b".md"))


def classify_changes(changes):
    if not changes:
        return FULL_STRESS
    for name, sides in changes.items():
        if not static_path(name):
            return FULL_STRESS
        if any(entry is not None and entry[0] != b"100644" for entry in sides):
            return FULL_STRESS
    return COMPLETE_SMOKE


def classify(root, event, head, base=None, fetch=False):
    """Failure to establish complete authority raises: the native job fails closed."""
    sha(head)
    require(git(root, "rev-parse", "HEAD").decode().strip() == head,
            "checked-out source identity mismatch")
    require(not git(root, "status", "--porcelain", "--untracked-files=no"),
            "tracked source differs from checked-out identity")
    audit = {"event": event, "head": head, "mode": FULL_STRESS,
             "authority": "immutable event source; stress is the default"}
    if event != "pull_request":
        return audit
    sha(base)
    if fetch:
        # checkout uses fetch-depth: 0; explicitly fetch the event base even if
        # main has advanced. Never substitute the current branch tip.
        git(root, "fetch", "--no-tags", "origin", base)
    require(git(root, "rev-parse", "--is-shallow-repository").strip() == b"false",
            "incomplete history")
    for revision in (head, base):
        require(git(root, "cat-file", "-t", revision).strip() == b"commit",
                "source identity is not a commit")
    bases = git(root, "merge-base", "--all", base, head).decode().splitlines()
    require(len(bases) == 1, "missing or ambiguous merge base")
    ancestor = sha(bases[0])
    before = tree(git(root, "ls-tree", "-rz", "--full-tree", ancestor))
    after = tree(git(root, "ls-tree", "-rz", "--full-tree", head))
    changes = delta(before, after)
    if changes:
        raw = raw_diff(git(root, "diff", "--raw", "-z", "--no-renames",
                           "--no-ext-diff", "--no-textconv", "--abbrev=40",
                           "--ignore-submodules=none", ancestor, head, "--"))
        require(raw == changes, "raw diff does not match complete source trees")
    else:
        require(git(root, "diff", "--raw", "-z", ancestor, head, "--") == b"",
                "anomalous empty source delta")
    audit.update(base=base, merge_base=ancestor, mode=classify_changes(changes),
                 authority="complete merge-base/head trees reconciled with NUL raw diff",
                 paths=[os.fsdecode(name) for name in sorted(changes)])
    return audit


def native_commands(mode, build="build/native", prefix="build/install"):
    require(mode in (FULL_STRESS, COMPLETE_SMOKE), "unknown native mode")
    commands = [["cmake", "--build", build, "--parallel", "2"],
                ["ctest", "--test-dir", build, "--output-on-failure"]]
    if mode == FULL_STRESS:
        commands.append(["ctest", "--test-dir", build, "--parallel", "4",
                         "--repeat", "until-fail:50", "--output-on-failure"])
    commands.append(["cmake", "--install", build, "--prefix", str(Path(prefix).resolve())])
    return commands


def execute(commands):
    for command in commands:
        print(json.dumps({"execute": command}), flush=True)
        subprocess.run(command, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--execute", action="store_true")
    parser.add_argument("--fetch", action="store_true")
    args = parser.parse_args()
    audit = classify(Path.cwd(), os.environ["GITHUB_EVENT_NAME"],
                     os.environ.get("BOOTSTRAP_HEAD_SHA"),
                     os.environ.get("BOOTSTRAP_BASE_SHA"), fetch=args.fetch)
    commands = native_commands(audit["mode"])
    print(json.dumps({**audit, "commands": commands}, sort_keys=True), flush=True)
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a") as stream:
            stream.write("### Native qualification policy\n```json\n" +
                         json.dumps(audit, sort_keys=True, indent=2) + "\n```\n")
    if args.execute:
        execute(commands)


if __name__ == "__main__":
    main()
