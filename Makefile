.PHONY: help native test-native test-ada test-rust check test-squall-ir test-squall-ir-c test-squall-ir-ada

help:
	@printf '%s\n' \
	  'make native       Build the native C ABI shared library' \
	  'make test-native  Build and run native and mock-provider tests' \
	  'make test-ada     Build/run Ada smoke test using GPRbuild on PATH' \
	  'make test-rust    Test the Rust workspace against native/build' \
	  'make check        Native + Ada tests and Git whitespace checks' \
	  'make test-squall-ir  Opt-in real Squall IR C and Ada integration'

native:
	@sh native/scripts/build.sh

test-native:
	@sh native/scripts/test.sh

test-ada:
	@sh scripts/test_ada.sh

test-rust: test-native
	@cargo test --manifest-path rust/Cargo.toml --workspace

check:
	@sh scripts/check.sh

test-squall-ir:
	@integration/squall/run.sh all

test-squall-ir-c:
	@integration/squall/run.sh c

test-squall-ir-ada:
	@integration/squall/run.sh ada
