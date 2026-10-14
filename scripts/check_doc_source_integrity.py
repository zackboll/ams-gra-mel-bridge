#!/usr/bin/env python3
"""Compare Ada lexical tokens and byte-protect production files for DOC-001."""

import argparse
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASE = "42648a92eb0cc84b2980d16db6028992c0494ba1"
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


def check(root=ROOT, base=BASE):
    def git(*args):
        return subprocess.check_output(["git", "-C", str(root), *args])
    existing = git("ls-tree", "-r", "--name-only", base).decode().splitlines()
    original_ada = {name for name in existing if name.startswith(("ada/src/", "ada/tests/src/"))
                    and Path(name).suffix in (".ads", ".adb")}
    current_ada = {path.relative_to(root).as_posix() for directory in ("ada/src", "ada/tests/src")
                   for path in (root / directory).rglob("*")
                   if path.is_file() and path.suffix in (".ads", ".adb")}
    if original_ada != current_ada:
        raise ValueError(f"Ada source inventory changed: {original_ada ^ current_ada}")
    checked = 0
    for name in existing:
        if name.startswith("docs/") or name in (".gitignore", "README.md"):
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
    return {"base": base, "protected_files": checked}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", default=BASE)
    args = parser.parse_args()
    print(check(base=args.base))
