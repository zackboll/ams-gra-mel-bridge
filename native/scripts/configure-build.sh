#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 {ON|OFF}" >&2
    exit 2
fi

case "$1" in
    ON|OFF) ;;
    *)
        echo "AMS_MEL_BUILD_TESTS must be ON or OFF." >&2
        exit 2
        ;;
esac

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build="$root/build"

# Alire supplies a runtime path through LD_RUN_PATH.  The linker consumes it
# while building, but CMake does not track it as an input.  Do not persist that
# caller-specific path in artifacts shared with ordinary CMake, Rust, and
# Python invocations.  Keep Alire's compiler and library search paths intact.
unset LD_RUN_PATH

cc_path=$(command -v cc)
cxx_path=$(command -v c++)

if [ -f "$build/CMakeCache.txt" ]; then
    cached_cc=$(sed -n 's/^CMAKE_C_COMPILER:FILEPATH=//p' "$build/CMakeCache.txt")
    cached_cxx=$(sed -n 's/^CMAKE_CXX_COMPILER:FILEPATH=//p' "$build/CMakeCache.txt")
    hardened=$(sed -n 's/^AMS_MEL_SANITIZED_BUILD_ENVIRONMENT:BOOL=//p' \
        "$build/CMakeCache.txt")
    if [ "$cached_cc" != "$cc_path" ] || [ "$cached_cxx" != "$cxx_path" ] || \
       [ "$hardened" != ON ]; then
        rm -rf "$build"
    fi
fi

cmake -S "$root" -B "$build" \
    -DAMS_MEL_BUILD_TESTS="$1" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DAMS_MEL_SANITIZED_BUILD_ENVIRONMENT:BOOL=ON
