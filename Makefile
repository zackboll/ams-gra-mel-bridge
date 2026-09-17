.PHONY: help native test-native test-ada test-rust test-python check test-squall-ir test-squall-ir-c test-squall-ir-ada test-squall-ir-rust

help:
	@printf '%s\n' \
	  'make native       Build the native C ABI shared library' \
	  'make test-native  Build and run native and mock-provider tests' \
	  'make test-ada     Build/run Ada smoke test using GPRbuild on PATH' \
	  'make test-rust    Test the Rust workspace against native/build' \
	  'make test-python  Test the Python binding against mock providers' \
	  'make check        Native + Ada tests and Git whitespace checks' \
	  'make test-squall-ir       Opt-in real Squall IR C, Ada, and Rust integration' \
	  'make test-squall-ir-c     Opt-in real Squall IR C integration' \
	  'make test-squall-ir-ada   Opt-in real Squall IR Ada integration' \
	  'make test-squall-ir-rust  Opt-in real Squall IR Rust integration'

native:
	@sh native/scripts/build.sh

test-native:
	@sh native/scripts/test.sh

test-ada:
	@sh scripts/test_ada.sh

test-rust: test-native
	@cargo test --manifest-path rust/Cargo.toml --workspace

test-python: test-native
	@PYTHONPATH="$(CURDIR)/python" \
	 AMS_MEL_NATIVE_LIB="$(CURDIR)/native/build/lib/libams_mel_c.so.0" \
	 AMS_MEL_TEST_PROVIDER_DIR="$(CURDIR)/native/build/test-providers" \
	 python3 -W error -m unittest discover -s python/tests -v
	@PYTHONPATH="$(CURDIR)/python" \
	 AMS_MEL_NATIVE_LIB="$(CURDIR)/native/build/lib/libams_mel_c.so.0" \
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
