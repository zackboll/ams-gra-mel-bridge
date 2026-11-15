#!/bin/sh
# Explicit documentation-only downloads; ordinary builds never invoke this.
set -eu
root=$(CDPATH='' cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$root"
archive=$(mktemp --suffix=.zip)
tools=$(mktemp -d)
trap 'rm -f "$archive"; rm -rf "$tools"' EXIT HUP INT TERM
curl --fail --location --output "$archive" https://github.com/alire-project/alire/releases/download/v2.1.1/alr-2.1.1-bin-x86_64-linux.zip
unzip -q "$archive" -d "$tools"
mkdir -p .doc-tools/bin .doc-tools/settings
install -m 0755 "$tools/bin/alr" .doc-tools/bin/alr
export PATH="$root/.doc-tools/bin:$PATH"
test "$(alr --version)" = 'alr 2.1.1'
alr -n -s "$root/.doc-tools/settings" --force toolchain --select gnat_native=16.1.0 gprbuild=26.0.1
alr -n -s "$root/.doc-tools/settings" install --prefix="$root/.doc-tools/gnatdoc-26.0.0" gnatdoc_bin=26.0.0
sh docs/tools/install_enhancer.sh .doc-tools/enhancer
# Documentation-only, explicit immutable Rust release; never follow stable.
rustup toolchain install 1.98.1 --profile minimal --no-self-update
test "$(rustup run 1.98.1 cargo --version)" = 'cargo 1.98.1 (797e8a9bc 2026-08-05)'
test "$(rustup run 1.98.1 rustc --version)" = 'rustc 1.98.1 (48a229cea 2026-09-01)'
test "$(rustup run 1.98.1 rustdoc --version)" = 'rustdoc 1.98.1 (48a229cea 2026-09-01)'
if [ -n "${GITHUB_PATH:-}" ]; then
    printf '%s\n' "$root/.doc-tools/bin" >> "$GITHUB_PATH"
fi
