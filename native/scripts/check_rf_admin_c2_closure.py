#!/usr/bin/env python3
"""Pin the Task 034B1 Admin/C2/VA declaration closure to vendored sources.

The exact family-relative dependency lists were measured against the full pinned
upstream trees. GCC and Clang see different Boost configuration headers; all
other observed lists are identical. Reject any dependency outside the approved
vendor roots, including system Boost and headers from an unpinned RF checkout.
"""
import argparse
import hashlib
import pathlib
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import check_rf_data_closure as base  # noqa: E402

EXPECTED = {
    "rfmel": (51, "aa349e2cf9228e06fe694c073ec5c44964a37115de5674c18a3f0174af29052b"),
    "mel": (24, "5c4f6d22873567ef21a40368d8dc2f79b627c4a8f32ef60aba5828bc94a13c9e"),
    "math": (5, "fcc04a579ab4389560ee0a3832757363a8e2a1f5afe09b6e26e84f720e977f51"),
    "vita": (5, "e2d9ba42f49a4f34f62550fa59841e71295bb452abf03bb2ae72ae2815340aef"),
}
BOOST = {
    "gcc": (625, "d1b47cf6f237f31a92acc2b31bf520dfdd66cce2e8aa11e773e7607a82845d62"),
    "clang": (626, "2fbc7eaee619bd14d1b5d32acb0f64eeddddd6c445e2c28c28ecf5d0ad1124d1"),
}


def verify(observed, vendor_root, compiler):
    # Driver banners are distribution-specific (Ubuntu's /usr/bin/c++ banner
    # does not contain "gcc" or "g++"). Query the same compiler's predefined
    # macros instead; do not infer an unmeasured family's dependency closure.
    macros = subprocess.check_output(
        [compiler, "-dM", "-E", "-x", "c++", "-"], input="", text=True)
    names = {line.split()[1] for line in macros.splitlines() if line.startswith("#define ")}
    family = ("clang" if "__clang__" in names else
              "gcc" if "__GNUC__" in names else None)
    if family is None:
        raise RuntimeError("unmeasured compiler family: " +
                           subprocess.check_output([compiler, "--version"], text=True).splitlines()[0])
    expected = dict(EXPECTED, boost=BOOST[family])
    for name, _, relative_root in base.FAMILIES:
        root = (vendor_root / relative_root).resolve()
        paths = sorted(path.relative_to(root).as_posix() for path in observed[name])
        digest = hashlib.sha256("\n".join(paths).encode()).hexdigest()
        if (len(paths), digest) != expected[name]:
            raise RuntimeError(f"{name} closure drifted: {len(paths)} files, SHA-256 {digest}")
    return sum(len(paths) for paths in observed.values())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--vendor-root", required=True)
    parser.add_argument("--include", action="append", default=[])
    parser.add_argument("--isystem", action="append", default=[])
    args = parser.parse_args()
    root = pathlib.Path(args.vendor_root).resolve()
    base.self_test(root)
    for rogue in ("/usr/include/rfmel/c2/C2MEL.h", "/usr/include/boost/uuid/uuid.hpp",
                  "/usr/local/include/rfmel/admin/AdminMEL.h"):
        _, violations = base.classify([pathlib.Path(rogue)], root)
        if len(violations) != 1:
            raise RuntimeError("rogue header escaped path validation: " + rogue)
    paths = base.dependencies(args.compiler, args.source, args.include, args.isystem)
    observed, violations = base.classify(paths, root)
    if violations:
        raise RuntimeError("unpinned Admin/C2 dependency:\n  " + "\n  ".join(violations))
    count = verify(observed, root, args.compiler)
    print(f"RF Admin/C2/VA closure: {count} pinned upstream headers under {root}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as error:
        print(f"RF Admin/C2 closure check failed: {error}", file=sys.stderr)
        raise SystemExit(1)