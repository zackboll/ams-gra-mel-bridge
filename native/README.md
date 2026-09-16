# Native C ABI

Public include: `ams_mel/abi.h` (C11).
Implementation: C++20, compiled exclusively by CMake.

`sh scripts/test.sh` builds the façade, C/C++ bootstrap tests, and separately
loaded test-only IR MEL providers. Production targets never link a provider.
The implemented provider slice is load/init/version/close only.

`ams_mel_c.gpr` imports `build/lib/libams_mel_c.so` as an externally built
library. The Alire pre-build action creates it. It does not ask GPR to compile
C++ again. Native installation exports a CMake target for C/C++ consumers.

This development manifest is not ready for an alire-index submission. See
`../docs/packaging.md` in the development repository before publishing.
