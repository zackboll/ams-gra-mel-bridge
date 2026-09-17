#!/bin/sh
set -eu

fail() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }

test "$#" -eq 3 || fail "usage: verify-checkout.sh PATH REPO_FRAGMENT EXPECTED_SHA"
checkout=$1
repo_fragment=$2
expected_sha=$3

git -C "$checkout" rev-parse --is-inside-work-tree >/dev/null 2>&1 ||
  fail "not a Git checkout: $checkout"
actual_sha=$(git -C "$checkout" rev-parse HEAD 2>/dev/null) ||
  fail "cannot read checkout revision: $checkout"
test "$actual_sha" = "$expected_sha" ||
  fail "revision mismatch for $checkout: expected $expected_sha, found $actual_sha"
remote_urls=$(git -C "$checkout" remote -v 2>/dev/null || true)
printf '%s\n' "$remote_urls" | grep -F -q "$repo_fragment" ||
  fail "$checkout does not identify expected repository $repo_fragment"
changes=$(git -C "$checkout" status --porcelain --untracked-files=all 2>/dev/null) ||
  fail "cannot inspect checkout status: $checkout"
test -z "$changes" ||
  fail "checkout has tracked or untracked non-ignored changes: $checkout"
