#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
case "${1-}" in
    format) check= ;;
    check) check=--check ;;
    *) echo "usage: $0 {format|check}" >&2; exit 2 ;;
esac
set -- alr -n
if [ -n "${DOC_EXAMPLES_ALIRE_SETTINGS-}" ]; then
    set -- "$@" -s "$DOC_EXAMPLES_ALIRE_SETTINGS"
fi
"$@" -C "$root/ada/tests" exec -- gnatformat \
    -P "$root/docs/examples/ams_mel_doc_examples.gpr" -U \
    --no-subprojects --charset=utf-8 $check
