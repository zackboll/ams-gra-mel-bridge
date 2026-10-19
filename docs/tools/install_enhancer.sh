#!/bin/sh
# Documentation-only environment, immutable Git archive + distribution digest.
set -eu
test "$#" = 1 || { echo 'usage: install_enhancer.sh VENV' >&2; exit 2; }
test ! -e "$1" || { echo 'VENV must not already exist' >&2; exit 2; }
archive=$(mktemp)
trap 'rm -f "$archive"' EXIT HUP INT TERM
curl --fail --location --output "$archive" https://codeload.github.com/zackboll/ada-gnatdoc-enhancer/tar.gz/3867c8879a3c832fb6e096d7680a8b739a2cffe1
printf '%s  %s\n' 61adae2b0c42fa588d8b1362f7441e27b79fdc0498a819e1f01fffadc53c586a "$archive" | sha256sum --check --status
python3 -m venv "$1"
"$1/bin/python" -m pip install setuptools==80.9.0 wheel==0.45.1
# Give pip an archive suffix while retaining the already verified bytes.
mv "$archive" "$archive.tar.gz"
archive="$archive.tar.gz"
"$1/bin/python" -m pip install --no-build-isolation --no-deps "$archive"
test "$("$1/bin/gnatdoc-enhancer" --version)" = 0.1.0.dev0
"$1/bin/python" -c 'import runpy,sys; m=runpy.run_path(sys.argv[1]); m["tool"]()' "$(dirname "$0")/build_enhanced_site.py"
