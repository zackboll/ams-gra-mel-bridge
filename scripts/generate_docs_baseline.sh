#!/bin/sh
# Initial pristine baseline capture only; not a complete site/publication gate.
set -eu
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
test "$(alr --version)" = 'alr 2.1.1'
: "${GNATDOC_PREFIX:?Set GNATDOC_PREFIX to the qualified GNATdoc 26.0.0 installation}"
version=$("$GNATDOC_PREFIX/bin/gnatdoc" --version 2>&1 || true)
test "$version" = 'GNATdoc 26.0.0 (20260415)' || {
    echo "error: unqualified GNATdoc: $version" >&2; exit 1;
}
gnat --version | grep -qx 'GNAT 16.1.0'
gprbuild --version | grep -q '^GPRBUILD 26\.0\.0 (2026-04-15)'
mkdir -p obj/docs obj/docs-internal
python3 scripts/gnatdoc_inventory.py --source-list obj/docs/public-sources.txt \
    --report obj/docs/inventory.json
# Semantic compilation only: specification-only ownership cannot generate code.
gprbuild -p -P docs_api.gpr -u -c -gnatc
# Only delete the fixed task-owned output, never a caller-provided directory.
rm -rf -- "$root/_site"
"$GNATDOC_PREFIX/bin/gnatdoc" -P docs_api.gpr \
    --backend html --generate public --style trailing --warnings \
    -O "$root/_site/api" > obj/docs/gnatdoc.log 2>&1
if grep -Ei '(^|[[:space:]])(error:|fatal:|ignore file due to)' obj/docs/gnatdoc.log; then
    echo 'error: incomplete GNATdoc generation' >&2; exit 1
fi
python3 scripts/compare_gnatdoc_warnings.py --fail-on-removed \
    docs/gnatdoc-warning-baseline.txt obj/docs/gnatdoc.log
echo 'Baseline capture only: rendering/coverage qualification is still pending.'