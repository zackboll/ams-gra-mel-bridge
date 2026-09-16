# Implementation coverage

| Area | Status | Evidence / next step |
|---|---|---|
| C ABI header usable from C11 | Implemented | `native/tests/test_c_abi.c` |
| Native facade version query | Implemented | C and C++ tests |
| Private Ada import and public version value | Implemented | `ada/tests/src/ams_mel_smoke.adb` |
| Native CMake package installation | Implemented | Exported target and config files |
| Local Alire dependency manifests | Development only | Relative pins; validate on installed Alire |
| Upstream version inventory | Candidate only | Prior review snapshot |
| Verified upstream dependency closure | Not implemented | First provider-foundation task |
| Separately loadable mock C++ MEL provider | Not implemented | First provider-foundation task |
| Real MEL factory/resource adaptation | Not implemented | Requires pinned interfaces |
| Async requests, buffer and callback lifetimes | Not implemented | Must precede sensor binding |
| IR receive/control | Not implemented | Host-memory Mono8 first |
| RF apertures/jobs/receive/VADB | Not implemented | Later phase |
| OMS/UCI application integration | Not implemented | Separate project concern |
| Rust sys/safe bindings | Not implemented | Later; same native ABI |
| Alire/crates.io publication | Not performed | See packaging checklist |
| Formal verification / GRA compliance | Not claimed | Separate evidence required |

Implementation and verification are different. See `bootstrap-validation.md`
for the commands actually executed when this starter archive was prepared.
