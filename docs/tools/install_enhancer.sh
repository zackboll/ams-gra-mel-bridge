#!/bin/sh
# Documentation-only environment, official release wheel + distribution digest.
set -eu
test "$#" = 1 || { echo 'usage: install_enhancer.sh VENV' >&2; exit 2; }
test ! -e "$1" || { echo 'VENV must not already exist' >&2; exit 2; }
requirements=$(mktemp)
trap 'rm -f "$requirements"' EXIT HUP INT TERM
builder="$(dirname "$0")/build_enhanced_site.py"
python3 - "$builder" > "$requirements" <<'PY'
import runpy
import sys
pin = runpy.run_path(sys.argv[1])
print('ada-gnatdoc-enhancer @ ' + pin['WHEEL_URL'] + ' --hash=sha256:' + pin['WHEEL_SHA256'])
PY
python3 -m venv "$1"
# pip verifies the release bytes before installation and records the exact URL
# and digest in PEP 610 direct_url.json. No source build or index dependency.
"$1/bin/python" -m pip install --no-deps --only-binary=:all: --require-hashes -r "$requirements"
test "$("$1/bin/gnatdoc-enhancer" --version)" = 0.1.5
"$1/bin/python" -c 'import runpy,sys; m=runpy.run_path(sys.argv[1]); m["tool"]()' "$builder"
