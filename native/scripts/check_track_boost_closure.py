#!/usr/bin/env python3
"""Reject system Boost leakage in the pinned TrackChannel declaration closure."""
import argparse
import pathlib
import subprocess
import sys
import tempfile


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--boost-root", required=True)
    parser.add_argument("--include", action="append", default=[])
    args = parser.parse_args()
    root = pathlib.Path(args.boost_root).resolve()
    with tempfile.TemporaryDirectory() as temporary:
        depfile = pathlib.Path(temporary) / "track.d"
        command = [args.compiler, "-std=c++20", "-M", "-MF", str(depfile), "-MT", "track"]
        command.extend(f"-I{item}" for item in args.include)
        command.append(args.source)
        subprocess.run(command, check=True)
        words = depfile.read_text(encoding="utf-8").replace("\\\n", " ").split(":", 1)[1].split()
    boost = [pathlib.Path(word.replace("\\ ", " ")).resolve() for word in words if "/boost/" in word]
    if not boost:
        raise RuntimeError("TrackChannel dependency probe observed no Boost headers")
    bad = [str(path) for path in boost if root not in path.parents]
    if bad:
        raise RuntimeError("system or unpinned Boost dependency: " + ", ".join(bad))
    print(f"TrackChannel Boost closure: {len(set(boost))} headers under {root}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"TrackChannel Boost closure check failed: {error}", file=sys.stderr)
        raise SystemExit(1)
