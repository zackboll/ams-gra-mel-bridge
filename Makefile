.PHONY: help native test-native test-ada check

help:
	@printf '%s\n' \
	  'make native       Build the native C ABI shared library' \
	  'make test-native  Build and run provider-free native tests' \
	  'make test-ada     Build/run Ada smoke test using GPRbuild on PATH' \
	  'make check        Native + Ada tests and Git whitespace checks'

native:
	@sh native/scripts/build.sh

test-native:
	@sh native/scripts/test.sh

test-ada:
	@sh scripts/test_ada.sh

check:
	@sh scripts/check.sh
