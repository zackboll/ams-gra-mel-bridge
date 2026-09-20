.PHONY: help native test-native test-build-isolation test-ada test-rust test-python format-ada check-ada-format check test-squall-ir test-squall-ir-c test-squall-ir-ada test-squall-ir-rust test-squall-ir-python

TEST_TREE := $(CURDIR)/native/build-tests

help:
	@printf '%s\n' \
	  'make native       Build the production C ABI library in native/build' \
	  'make test-native  Build/run native tests in native/build-tests' \
	  'make test-build-isolation Prove the production/test build trees stay separate' \
	  'make test-ada     Build/run Ada smoke test using GPRbuild on PATH' \
	  'make format-ada       Format Ada crate sources with GNATformat' \
	  'make check-ada-format Verify Ada crate sources are GNATformat-clean' \
	  'make test-rust    Test the Rust workspace against native/build-tests' \
	  'make test-python  Test the Python binding against native/build-tests' \
	  'make check        Native + Ada formatting/tests and Git whitespace checks' \
	  'make test-squall-ir       Opt-in real Squall IR C, Ada, Rust, and Python integration' \
	  'make test-squall-ir-c     Opt-in real Squall IR C integration' \
	  'make test-squall-ir-ada   Opt-in real Squall IR Ada integration' \
	  'make test-squall-ir-rust  Opt-in real Squall IR Rust integration' \
	  'make test-squall-ir-python Opt-in real Squall IR Python integration'

native:
	@sh native/scripts/build.sh

test-native:
	@sh native/scripts/test.sh

test-ada:
	@sh scripts/test_ada.sh

format-ada:
	@sh scripts/format_ada.sh format

check-ada-format:
	@sh scripts/format_ada.sh check

test-build-isolation:
	@sh native/scripts/test-build-tree-isolation.sh

# Repository contract tests run against the test-enabled facade and the mock
# providers in the test tree.  Ordinary downstream builds keep the production
# default in native/build.
test-rust: test-native
	@AMS_MEL_NATIVE_LIB_DIR="$(TEST_TREE)/lib" \
	 AMS_MEL_TEST_PROVIDER_DIR="$(TEST_TREE)/test-providers" \
	 cargo test --manifest-path rust/Cargo.toml --workspace

test-python: test-native
	@PYTHONPATH="$(CURDIR)/python" \
	 AMS_MEL_NATIVE_LIB="$(TEST_TREE)/lib/libams_mel_c.so.0" \
	 AMS_MEL_TEST_PROVIDER_DIR="$(TEST_TREE)/test-providers" \
	 python3 -W error -m unittest discover -s python/tests -v
	@PYTHONPATH="$(CURDIR)/python" \
	 AMS_MEL_NATIVE_LIB="$(TEST_TREE)/lib/libams_mel_c.so.0" \
	 python3 -W error -m compileall -q python/ams_mel

check:
	@sh scripts/check.sh

test-squall-ir:
	@integration/squall/run.sh all

test-squall-ir-c:
	@integration/squall/run.sh c

test-squall-ir-ada:
	@integration/squall/run.sh ada

test-squall-ir-rust:
	@integration/squall/run.sh rust

test-squall-ir-python:
	@integration/squall/run.sh python
