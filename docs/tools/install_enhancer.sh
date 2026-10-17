#!/bin/sh
# Documentation-only environment, immutable Git archive + distribution digest.
set -eu
test "$#" = 1 || { echo 'usage: install_enhancer.sh VENV' >&2; exit 2; }
test ! -e "$1" || { echo 'VENV must not already exist' >&2; exit 2; }
archive=$(mktemp)
trap 'rm -f "$archive"' EXIT HUP INT TERM
curl --fail --location --output "$archive" https://codeload.github.com/zackboll/ada-gnatdoc-enhancer/tar.gz/c1a691452cf436624809f4043b30087de6688052
printf '%s  %s\n' d4b422b73df60d6ed41ec534b71bd0248b43827ce54ed4c323455e225be17493 "$archive" | sha256sum --check --status
python3 -m venv "$1"
"$1/bin/python" -m pip install setuptools==80.9.0 wheel==0.45.1
# Give pip an archive suffix while retaining the already verified bytes.
mv "$archive" "$archive.tar.gz"
archive="$archive.tar.gz"
"$1/bin/python" -m pip install --no-build-isolation --no-deps "$archive"
test "$("$1/bin/gnatdoc-enhancer" --version)" = 0.1.0.dev0
"$1/bin/python" -c 'import runpy,sys; m=runpy.run_path(sys.argv[1]); m["tool"]()' "$(dirname "$0")/build_enhanced_site.py"
