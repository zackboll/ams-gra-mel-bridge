#!/bin/sh
# Regression guard for native production/contract-test build-tree isolation.
#
# Production (native/build, AMS_MEL_BUILD_TESTS=OFF) and contract-test
# (native/build-tests, AMS_MEL_BUILD_TESTS=ON) builds must never share a mutable
# CMake build directory.  Before the split, native/scripts/build.sh silently
# reconfigured the same tree used by CTest, compiling the deterministic
# AMS_MEL_ENABLE_TEST_FAILPOINTS barriers out from under an in-flight test run.
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
# Derive both directories from the shared selector rather than repeating the
# paths here, so this regression always exercises the directories the
# production and test scripts actually use.
. "$root/scripts/build-tree.sh"
ams_mel_select_tree production
production="$ams_mel_build_dir"
ams_mel_select_tree tests
tests="$ams_mel_build_dir"
failures=0

note() { printf '%s\n' "$*"; }
pass() { printf 'ok   %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*" >&2; failures=$((failures + 1)); }

cache_value() {
    sed -n "s/^$2:BOOL=//p" "$1/CMakeCache.txt"
}

expect_cache() {
    actual=$(cache_value "$1" AMS_MEL_BUILD_TESTS)
    if [ "$actual" = "$3" ]; then
        pass "$2 AMS_MEL_BUILD_TESTS=$3"
    else
        fail "$2 AMS_MEL_BUILD_TESTS=$actual (expected $3)"
    fi
}

hash_of() {
    if [ -e "$1" ]; then
        sha256sum "$1" | cut -d' ' -f1
    else
        echo "MISSING:$1"
    fi
}

expect_same() {
    if [ "$2" = "$3" ]; then
        pass "$1 unchanged"
    else
        fail "$1 was rewritten ($2 -> $3)"
    fi
}

note '== A. Test tree first, then a production build =='
sh "$root/scripts/test.sh" >/dev/null
expect_cache "$tests" 'test tree' ON

test_facade="$tests/lib/libams_mel_c.so.0.1.0"
navigation="$tests/test_ir_navigation"
for artifact in "$test_facade" "$navigation"; do
    if [ -f "$artifact" ]; then
        pass "test artifact present: ${artifact#$root/}"
    else
        fail "missing test artifact: ${artifact#$root/}"
    fi
done

before_facade=$(hash_of "$test_facade")
before_navigation=$(hash_of "$navigation")
before_cache=$(hash_of "$tests/CMakeCache.txt")

sh "$root/scripts/build.sh" >/dev/null

expect_cache "$production" 'production tree' OFF
expect_cache "$tests" 'test tree after build.sh' ON
expect_same 'test facade' "$before_facade" "$(hash_of "$test_facade")"
expect_same 'test Navigation contract executable' "$before_navigation" \
    "$(hash_of "$navigation")"
expect_same 'test CMakeCache.txt' "$before_cache" "$(hash_of "$tests/CMakeCache.txt")"

note 'Running CTest from the test tree without rebuilding it'
if ctest --test-dir "$tests" --output-on-failure >"$tests/isolation-ctest.log" 2>&1; then
    total=$(sed -n 's/.*tests passed, .* out of \([0-9]*\)/\1/p' \
        "$tests/isolation-ctest.log" | tail -1)
    if [ "$total" = 20 ]; then
        pass "native suite 20/20"
    else
        fail "native suite reported $total tests (expected 20)"
    fi
else
    fail "native suite failed after a production build; see $tests/isolation-ctest.log"
fi

note ''
note '== B. Production tree first, then a test build =='
sh "$root/scripts/build.sh" >/dev/null
expect_cache "$production" 'production tree' OFF
production_facade="$production/lib/libams_mel_c.so.0.1.0"
before_production_facade=$(hash_of "$production_facade")
before_production_cache=$(hash_of "$production/CMakeCache.txt")

sh "$root/scripts/test.sh" >/dev/null

expect_cache "$production" 'production tree after test.sh' OFF
expect_cache "$tests" 'test tree' ON
expect_same 'production facade' "$before_production_facade" \
    "$(hash_of "$production_facade")"
expect_same 'production CMakeCache.txt' "$before_production_cache" \
    "$(hash_of "$production/CMakeCache.txt")"

note ''
note '== C. Failpoint separation =='
# CMAKE_EXPORT_COMPILE_COMMANDS is not enabled, so read the flags CMake
# generated for the ams_mel_c objects in each tree.  This is deterministic
# target metadata, not a timestamp comparison.
failpoint_flags() {
    find "$1/CMakeFiles/ams_mel_c.dir" -name flags.make -exec \
        grep -l 'AMS_MEL_ENABLE_TEST_FAILPOINTS' {} + 2>/dev/null | wc -l
}

if [ "$(failpoint_flags "$tests")" -gt 0 ]; then
    pass 'test facade compiles with AMS_MEL_ENABLE_TEST_FAILPOINTS'
else
    fail 'test facade is missing AMS_MEL_ENABLE_TEST_FAILPOINTS'
fi
if [ "$(failpoint_flags "$production")" -eq 0 ]; then
    pass 'production facade has no AMS_MEL_ENABLE_TEST_FAILPOINTS'
else
    fail 'production facade was built with AMS_MEL_ENABLE_TEST_FAILPOINTS'
fi

note ''
note '== D. Cross-tree acceptance =='
# Both caches must be readable simultaneously in their opposite modes, and
# neither script may delete the other tree.
if [ -f "$production/CMakeCache.txt" ] && [ -f "$tests/CMakeCache.txt" ]; then
    pass 'both build trees coexist'
else
    fail 'a build script removed the other build tree'
fi
if [ "$production" != "$tests" ]; then
    pass 'production and test trees are physically distinct'
else
    fail 'production and test trees resolve to the same directory'
fi

note ''
if [ "$failures" -eq 0 ]; then
    note 'PASS: native production and contract-test build trees are isolated'
    exit 0
fi
note "FAIL: $failures build-tree isolation check(s) failed"
exit 1
