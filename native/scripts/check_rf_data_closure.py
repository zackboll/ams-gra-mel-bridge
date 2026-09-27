#!/usr/bin/env python3
"""Reject unpinned upstream leakage in the Task 033A RF DataMEL declaration closure.

The configured compiler dependency-probes the declaration-only RF probe. Every
project-owned upstream header it observes is classified by family and must
resolve beneath that family's approved repository vendor root. A header that
resolves anywhere else (for example /usr/include/boost, /usr/local/include/rfmel,
or an installed Common MEL / AMS Math / AMS VITA copy) fails the check, as does a
header from an approved family resolving into a different family's vendor root.
Standard-library and compiler headers are not project-owned and are allowed.
"""
import argparse
import pathlib
import subprocess
import sys
import tempfile

# (family, path-component sequence identifying that family, vendor subdirectory)
FAMILIES = (
    ("rfmel", ("rfmel",), "rf-mel/include"),
    ("mel", ("mel", "library"), "common-mel/include"),
    ("math", ("math",), "ams-math/include"),
    ("boost", ("boost",), "boost-1.83.0"),
    ("vita", ("ams", "iface", "vita"), "ams-vita/include"),
)


def family_of(path: pathlib.Path):
    parts = path.parts
    for name, marker, _ in FAMILIES:
        width = len(marker)
        for index in range(len(parts) - width):
            if parts[index:index + width] == marker:
                return name
    return None


def classify(paths, vendor_root: pathlib.Path):
    """Return ({family: set(paths)}, [violations]) for resolved dependency paths."""
    roots = {name: (vendor_root / sub).resolve() for name, _, sub in FAMILIES}
    observed = {name: set() for name, _, _ in FAMILIES}
    violations = []
    for path in paths:
        inside_vendor = vendor_root in path.parents
        family = family_of(path.relative_to(vendor_root) if inside_vendor else path)
        if family is None:
            if inside_vendor:
                violations.append(f"unclassified vendored dependency: {path}")
            continue
        if roots[family] not in path.parents:
            violations.append(f"{family} dependency outside {roots[family]}: {path}")
            continue
        observed[family].add(path)
    return observed, violations


def dependencies(compiler, source, includes, system_includes):
    with tempfile.TemporaryDirectory() as temporary:
        depfile = pathlib.Path(temporary) / "rf.d"
        command = [compiler, "-std=c++20", "-M", "-MF", str(depfile), "-MT", "rf"]
        command.extend(f"-I{item}" for item in includes)
        for item in system_includes:
            command.extend(["-isystem", item])
        command.append(source)
        subprocess.run(command, check=True)
        text = depfile.read_text(encoding="utf-8").replace("\\\n", " ")
    words = text.split(":", 1)[1].replace("\\ ", "\0").split()
    source_path = pathlib.Path(source).resolve()
    resolved = {pathlib.Path(word.replace("\0", " ")).resolve() for word in words}
    resolved.discard(source_path)
    return resolved


def self_test(vendor_root: pathlib.Path) -> None:
    """Prove the path rule rejects system/unpinned copies of every family."""
    rogue = [
        "/usr/include/boost/config.hpp",
        "/usr/local/include/boost/numeric/ublas/blas.hpp",
        "/usr/include/rfmel/data/DataMEL.h",
        "/usr/local/include/rfmel/mfa/RFMFAInfo.h",
        "/usr/include/mel/library/CommonMEL.h",
        "/usr/local/include/math/units/UTCTime.h",
        "/usr/include/ams/iface/vita/FixedDataPacket.h",
        str(vendor_root / "common-mel/include/rfmel/data/DataMEL.h"),
        str(vendor_root / "ir-mel/include/boost/config.hpp"),
    ]
    for item in rogue:
        _, violations = classify([pathlib.Path(item)], vendor_root)
        if len(violations) != 1:
            raise RuntimeError(f"self-test: leaked path was not rejected: {item}")
    allowed = [
        vendor_root / "rf-mel/include/rfmel/data/DataMEL.h",
        vendor_root / "common-mel/include/mel/library/CommonMEL.h",
        vendor_root / "ams-math/include/math/units/UTCTime.h",
        vendor_root / "boost-1.83.0/boost/config.hpp",
        vendor_root / "ams-vita/include/ams/iface/vita/Primitives.h",
        pathlib.Path("/usr/include/c++/14/memory"),
    ]
    _, violations = classify(allowed, vendor_root)
    if violations:
        raise RuntimeError("self-test: approved path rejected: " + ", ".join(violations))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--vendor-root", required=True)
    parser.add_argument("--include", action="append", default=[])
    parser.add_argument("--isystem", action="append", default=[])
    parser.add_argument("--expect", action="append", default=[],
                        help="family that must appear in the closure")
    args = parser.parse_args()
    vendor_root = pathlib.Path(args.vendor_root).resolve()
    self_test(vendor_root)
    paths = dependencies(args.compiler, args.source, args.include, args.isystem)
    observed, violations = classify(paths, vendor_root)
    if violations:
        raise RuntimeError("unpinned RF closure dependency:\n  " + "\n  ".join(violations))
    missing = [family for family in args.expect if not observed.get(family)]
    if missing:
        raise RuntimeError("expected families absent from closure: " + ", ".join(missing))
    counts = ", ".join(f"{name} {len(observed[name])}" for name, _, _ in FAMILIES)
    total = sum(len(items) for items in observed.values())
    print(f"RF DataMEL closure: {total} upstream headers under {vendor_root} ({counts})")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as error:
        print(f"RF DataMEL closure check failed: {error}", file=sys.stderr)
        raise SystemExit(1)
