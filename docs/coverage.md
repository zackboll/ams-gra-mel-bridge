# Implementation coverage

| Area | Status | Evidence / next step |
|---|---|---|
| C ABI header usable from C11 | Implemented | `native/tests/test_c_abi.c` |
| Native facade version query | Implemented | C and C++ tests |
| Private Ada import and public version value | Implemented | `ada/tests/src/ams_mel_smoke.adb` |
| Native CMake package installation | Implemented | Exported target and config files |
| Local Alire dependency manifests | Development only | Relative pins; validate on installed Alire |
| Upstream version inventory | Verified for task 001 | `upstream-provenance.md` exact commits/trees |
| Verified upstream dependency closure | Implemented for Control slice | 44 unmodified headers; no upstream compiled source |
| Separately loadable mock C++ MEL provider | Implemented, test-only | Native C and Ada contract tests |
| Real MEL factory/resource adaptation | Control foundation implemented | load/factory/init/version/close only |
| Async requests, buffer and callback lifetimes | Not implemented | Must precede sensor binding |
| IR receive/control | Not implemented | Host-memory Mono8 first |
| RF apertures/jobs/receive/VADB | Not implemented | Later phase |
| OMS/UCI application integration | Not implemented | Separate project concern |
| Rust sys/safe bindings | Not implemented | Later; same native ABI |
| Alire/crates.io publication | Not performed | See packaging checklist |
| Formal verification / GRA compliance | Not claimed | Separate evidence required |

Implementation and verification are different. See `bootstrap-validation.md`
for the commands actually executed when this starter archive was prepared.
