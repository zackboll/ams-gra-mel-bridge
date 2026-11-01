#!/bin/sh
# Compatibility entry point: current Git source, not a frozen baseline build.
set -eu
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
: "${GNATDOC_PREFIX:?Set GNATDOC_PREFIX to the qualified GNATdoc 26.0.0 installation}"
exec "${DOC_PYTHON:-$root/.doc-tools/enhancer/bin/python}" \
    docs/tools/docs_ci.py build --gnatdoc-prefix "$GNATDOC_PREFIX"
