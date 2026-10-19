#!/bin/sh
# Explicit execution gate, intentionally separate from GNATdoc generation.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$root"
sh docs/examples/format.sh check
mkdir -p obj/docs
sh docs/examples/run.sh --report "$root/obj/docs/examples-execution.json"
