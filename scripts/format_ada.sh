#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

if ! command -v alr >/dev/null 2>&1; then
    echo "Alire (alr) is required to run the pinned GNATformat formatter." >&2
    exit 1
fi

case "${1-}" in
    format)
        check_option=
        ;;
    check)
        check_option=--check
        ;;
    *)
        echo "Usage: $0 {format|check}" >&2
        exit 2
        ;;
esac

run_gnatformat() {
    alr -C "$root/ada/tests" exec -- gnatformat "$@"
}

run_gnatformat -P "$root/ada/ams_mel.gpr" -U --no-subprojects --charset=utf-8 $check_option
run_gnatformat -P "$root/ada/tests/ams_mel_tests.gpr" -U --no-subprojects --charset=utf-8 $check_option