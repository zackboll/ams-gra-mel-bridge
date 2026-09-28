#!/usr/bin/env python3
"""Reject unpinned leakage in the Task 033C RF ProductRxEndpoint receive closure.

The configured compiler dependency-probes the declaration-only receive probe
(rooted at rfmel/data/DataMEL.h + rfmel/data/ProductRxEndpoint.h). Every
project-owned upstream header must resolve beneath its family's approved
repository vendor root, using the unchanged Task 033A path rule imported from
check_rf_data_closure.py. On top of that rule this check is stricter:

* the observed RF MEL header set must EQUAL the measured 033C union exactly
  (no silent growth into C2/VADB/RDMA headers, no silent shrinkage);
* the Common MEL / AMS Math / AMS VITA counts must equal the measured counts;
* receive-specific rogue system paths are rejected by a built-in self-test.

Boost is compiler-dependent (GCC 479, Clang 480) and is therefore checked only
by the family path rule, as in Task 033A.
"""
import argparse
import pathlib
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import check_rf_data_closure as base  # noqa: E402  (unchanged 033A path rule)

# Exact GCC == Clang RF MEL union measured from the full pinned RF MEL tree.
EXPECTED_RF = frozenset({
    "rfmel/data/DataMEL.h",
    "rfmel/data/ProductRxEndpoint.h",
    "rfmel/endpoints/BaseEndpoint.h",
    "rfmel/jobs/CachedWaveform.h",
    "rfmel/jobs/CommonModulations.h",
    "rfmel/jobs/DataPipe.h",
    "rfmel/jobs/ElementGroupToEndpointConnections.h",
    "rfmel/jobs/JobEvent.h",
    "rfmel/jobs/JobInterval.h",
    "rfmel/jobs/LocalFunctionCommand.h",
    "rfmel/jobs/ModulationExtensionBase.h",
    "rfmel/jobs/Pointing.h",
    "rfmel/jobs/ProductStreamParams.h",
    "rfmel/jobs/PulseDetectionSettings.h",
    "rfmel/jobs/WaveformStream.h",
    "rfmel/jobs/Weights.h",
    "rfmel/mfa/RFMFAInfo.h",
    "rfmel/mfa/TxPowerModeData.h",
    "rfmel/rfmeltypes/InvalidTxPowerModeException.h",
    "rfmel/rfmeltypes/JobDataFormat.h",
    "rfmel/rfmeltypes/MELComplex.h",
    "rfmel/rfmeltypes/ProductRxMetadata.h",
    "rfmel/rfmeltypes/RFMEL.h",
    "rfmel/rfmeltypes/RFMELTypes.h",
    "rfmel/rfmeltypes/ResourceUseAfterExpiredException.h",
})
EXPECTED_COUNTS = {"mel": 23, "math": 5, "vita": 5}


def self_test(vendor_root: pathlib.Path) -> None:
    """Base 033A self-test plus receive-specific rogue locations."""
    base.self_test(vendor_root)
    rogue = [
        "/usr/include/rfmel/data/ProductRxEndpoint.h",
        "/usr/local/include/rfmel/rfmeltypes/ProductRxMetadata.h",
        "/usr/include/rfmel/jobs/JobEvent.h",
        "/usr/local/include/rfmel/jobs/Pointing.h",
        "/opt/rfmel/include/rfmel/endpoints/BaseEndpoint.h",
        str(vendor_root / "ir-mel/include/rfmel/jobs/JobInterval.h"),
        str(vendor_root / "ams-math/include/rfmel/rfmeltypes/ProductRxMetadata.h"),
    ]
    for item in rogue:
        _, violations = base.classify([pathlib.Path(item)], vendor_root)
        if len(violations) != 1:
            raise RuntimeError(f"self-test: leaked receive path was not rejected: {item}")
    allowed = [vendor_root / "rf-mel/include" / item for item in sorted(EXPECTED_RF)]
    _, violations = base.classify(allowed, vendor_root)
    if violations:
        raise RuntimeError("self-test: approved receive path rejected: " + ", ".join(violations))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--vendor-root", required=True)
    parser.add_argument("--include", action="append", default=[])
    parser.add_argument("--isystem", action="append", default=[])
    args = parser.parse_args()
    vendor_root = pathlib.Path(args.vendor_root).resolve()
    self_test(vendor_root)
    paths = base.dependencies(args.compiler, args.source, args.include, args.isystem)
    observed, violations = base.classify(paths, vendor_root)
    if violations:
        raise RuntimeError("unpinned RF receive closure dependency:\n  " + "\n  ".join(violations))
    rf_root = vendor_root / "rf-mel/include"
    rf = {path.relative_to(rf_root).as_posix() for path in observed["rfmel"]}
    if rf != EXPECTED_RF:
        extra = sorted(rf - EXPECTED_RF)
        missing = sorted(EXPECTED_RF - rf)
        raise RuntimeError(f"RF receive closure drifted: unexpected {extra}, missing {missing}")
    for family, count in EXPECTED_COUNTS.items():
        if len(observed[family]) != count:
            raise RuntimeError(f"{family} closure count {len(observed[family])} != measured {count}")
    if not observed["boost"]:
        raise RuntimeError("expected family absent from closure: boost")
    counts = ", ".join(f"{name} {len(observed[name])}" for name, _, _ in base.FAMILIES)
    total = sum(len(items) for items in observed.values())
    print(f"RF ProductRxEndpoint closure: {total} upstream headers under {vendor_root} ({counts})")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as error:
        print(f"RF ProductRxEndpoint closure check failed: {error}", file=sys.stderr)
        raise SystemExit(1)
