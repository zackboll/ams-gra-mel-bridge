# Implementation coverage

| Area | Status | Evidence / next step |
|---|---|---|
| C ABI header usable from C11 | Implemented | `native/tests/test_c_abi.c` |
| Native facade version query | Implemented | C and C++ tests |
| Private Ada import and public version value | Implemented | `ada/tests/src/ams_mel_smoke.adb` |
| Native CMake package installation | Implemented | Exported target and config files |
| Local Alire dependency manifests | Development only | Relative pins; validate on installed Alire |
| Upstream version inventory | Verified for task 001 | `upstream-provenance.md` exact commits/trees |
| Verified upstream dependency closure | Implemented for IR receive slice | 71 unmodified headers; no upstream compiled source |
| Separately loadable mock C++ MEL provider | Implemented, test-only | Native C and Ada contract tests |
| Real MEL factory/resource adaptation | Control foundation implemented | load/factory/init/version/close only |
| Buffer and callback lifetimes | Implemented for IR image mock | Provider buffer released once; owned copies; conservative teardown |
| IR receive/control | Mono8 receive implemented | Host memory, `IRSTImage`, C polling and `AMS.MEL.IR` |
| Bounded receive queue | Implemented | Caller capacity; DROP-INCOMING; saturating counters |
| Full FrameHeader metadata | Partial by design | Omits contributing sensor and inertial/navigation vectors |
| Real IR provider validation | Not performed | Mock provider only; callback quiescence remains provider requirement |
| RF apertures/jobs/receive/VADB | Not implemented | Later phase |
| OMS/UCI application integration | Not implemented | Separate project concern |
| Rust sys/safe bindings | Not implemented | Later; same native ABI |
| Alire/crates.io publication | Not performed | See packaging checklist |
| Formal verification / GRA compliance | Not claimed | Separate evidence required |

Implementation and verification are different. See `bootstrap-validation.md`
for the commands actually executed when this starter archive was prepared.
