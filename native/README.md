# Native C ABI bootstrap

Public include: `ams_mel/abi.h` (C11).
Implementation: C++20, compiled exclusively by CMake.

`sh scripts/test.sh` builds and runs provider-free C and C++ tests.
The only current operation is a version query for this experimental C ABI.
No MEL provider or upstream interface is included yet.

`ams_mel_c.gpr` imports `build/lib/libams_mel_c.so` as an externally built
library. The Alire pre-build action creates it. It does not ask GPR to compile
C++ again. Native installation exports a CMake target for C/C++ consumers.

This development manifest is not ready for an alire-index submission. See
`../docs/packaging.md` in the development repository before publishing.
