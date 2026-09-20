# Shared native build-tree selection.  Source this file after setting "root" to
# the native/ directory.
#
# Production and contract-test builds must never share a CMake build tree.  The
# test configuration compiles AMS_MEL_ENABLE_TEST_FAILPOINTS into ams_mel_c and
# adds the mock providers and CTest targets; the production configuration does
# not.  Reconfiguring one tree between those two modes silently rewrites the
# facade underneath any in-flight CTest or stress run, and would otherwise let a
# production consumer link a failpoint-enabled library.
#
# ams_mel_select_tree {production|tests} sets:
#   ams_mel_build_dir    absolute CMake binary directory for that mode
#   ams_mel_build_tests  matching AMS_MEL_BUILD_TESTS value

ams_mel_select_tree() {
    case "${1-}" in
        production)
            ams_mel_build_dir="$root/build"
            ams_mel_build_tests=OFF
            ;;
        tests)
            ams_mel_build_dir="$root/build-tests"
            ams_mel_build_tests=ON
            ;;
        *)
            echo "Build tree must be 'production' or 'tests'." >&2
            return 2
            ;;
    esac
}
