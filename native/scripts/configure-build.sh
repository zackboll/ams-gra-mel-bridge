#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 {production|tests}" >&2
    exit 2
fi

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$root/scripts/build-tree.sh"
ams_mel_select_tree "$1" || exit 2
build="$ams_mel_build_dir"

# Alire supplies a runtime path through LD_RUN_PATH.  The linker consumes it
# while building, but CMake does not track it as an input.  Do not persist that
# caller-specific path in artifacts shared with ordinary CMake, Rust, and
# Python invocations.  Keep Alire's compiler and library search paths intact.
unset LD_RUN_PATH

cc_path=$(command -v cc)
cxx_path=$(command -v c++)

# Reset only the selected tree.  The other tree belongs to the opposite
# configuration and must never be removed or reconfigured from here.
if [ -f "$build/CMakeCache.txt" ]; then
    cached_cc=$(sed -n 's/^CMAKE_C_COMPILER:FILEPATH=//p' "$build/CMakeCache.txt")
    cached_cxx=$(sed -n 's/^CMAKE_CXX_COMPILER:FILEPATH=//p' "$build/CMakeCache.txt")
    hardened=$(sed -n 's/^AMS_MEL_SANITIZED_BUILD_ENVIRONMENT:BOOL=//p' \
        "$build/CMakeCache.txt")
    cached_tests=$(sed -n 's/^AMS_MEL_BUILD_TESTS:BOOL=//p' "$build/CMakeCache.txt")
    if [ "$cached_cc" != "$cc_path" ] || [ "$cached_cxx" != "$cxx_path" ] || \
       [ "$hardened" != ON ] || [ "$cached_tests" != "$ams_mel_build_tests" ]; then
        rm -rf "$build"
    fi
fi

cmake -S "$root" -B "$build" \
    -DAMS_MEL_BUILD_TESTS="$ams_mel_build_tests" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DAMS_MEL_SANITIZED_BUILD_ENVIRONMENT:BOOL=ON
