"""Ordinary feature regression entry point.

Run without deployment credentials. Feature code is reviewed and tested by
ordinary CI; no per-feature scope or publication approval ledger is consulted.
"""

import argparse
import json
from pathlib import Path
import subprocess

import feature_source as source
import source_epoch


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--gnatdoc-prefix", type=Path, required=True)
    args = parser.parse_args()
    import epoch_ci
    epoch_ci.global_root(args.candidate)
    context = epoch_ci.context()
    target = context['target_sha']
    require = source.require
    evidence = source.qualify(args.candidate, target, "HEAD")
    args.output.mkdir(parents=True, exist_ok=False)
    (args.output / "source.json").write_bytes(source_epoch.canonical(evidence))
    if evidence["case"] == "feature-source":
        # These are candidate-source executable tests, not trusted policy. They
        # run without credentials/Pages permission on the ordinary isolated runner.
        for command in (["make", "test-native"], ["make", "test-ada"], ["make", "check"],
                        ["make", "check-ada-format"], ["alr", "-n", "-C", "ada", "build"],
                        ["alr", "-n", "-C", "ada/tests", "run"],
                        ["make", "test-python"], ["make", "test-rust"]):
            result = subprocess.run(command, cwd=args.candidate, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            name = "-".join(command).replace("/", "-")
            (args.output / (name + ".log")).write_bytes(result.stdout)
            require(result.returncode == 0, "candidate language qualification failed: " + name)
        source.qualify(args.candidate, target, "HEAD")
        value = source_epoch.qualify(args.candidate, target, "HEAD", args.output / "epoch", args.gnatdoc_prefix)
        source_epoch.verify_regenerated(args.candidate, args.output / "epoch", target, "HEAD", args.gnatdoc_prefix)
        print(json.dumps({"status": value["status"], "candidate": value["candidate_commit"],
                          "qualification": "ordinary-development"}))
    else:
        print(json.dumps(evidence, indent=2))


if __name__ == "__main__":
    main()
