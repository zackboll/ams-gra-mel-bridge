# Implementation coverage

| Area | Status | Evidence / next step |
|---|---|---|
| C ABI header usable from C11 | Implemented | `native/tests/test_c_abi.c` |
| Native facade version query | Implemented | C and C++ tests |
| Private Ada import and public version value | Implemented | `ada/tests/src/ams_mel_smoke.adb` |
| Native CMake package installation | Implemented | Exported target and config files |
| Local Alire dependency manifests | Development only | Relative pins; validate on installed Alire |
| Upstream version inventory | Verified through task 003 | `upstream-provenance.md` exact commits/trees |
| Verified upstream dependency closure | Implemented for IR receive/C2 slices | 85 unmodified headers; no upstream compiled source |
| Separately loadable mock C++ MEL provider | Implemented, test-only | Native C and Ada contract tests |
| Real MEL factory/resource adaptation | Control foundation implemented | load/factory/init/version/close only |
| Buffer and callback lifetimes | Hardened with non-quiescing mock | Channel destruction before in-flight drain/storage; release checked exactly once |
| IR image receive | Mono8 receive implemented | Host memory, `IRSTImage`, C polling and `AMS.MEL.IR` |
| IR C2 Operate/TaskSched | Implemented | Async C request and `AMS.MEL.IR.C2`; timeout/rejection/exception distinct |
| Pending C2 lifetime | Hardened and lifecycle-tested | Pre-send allocation; allocation-free emergency roots; launch/allocation failure injection; deferred disable/detach/unload |
| C2/image coexistence | Implemented | One Session, parent-first close, provider unload last |
| Other C2 commands/callbacks | Not implemented | No scan scheduling, BIT, config, camera, or CommandStatus API |
| Bounded receive queue | Implemented | Caller capacity; DROP-INCOMING; saturating counters |
| Full FrameHeader metadata | Partial by design | Omits contributing sensor and inertial/navigation vectors |
| Real IR provider validation | Implemented and passed | Pinned Squall simulator: C2 TaskSched plus three 320x200 Mono8 frames in both C and Ada; opt-in and excluded from ordinary CI |
| RF apertures/jobs/receive/VADB | Not implemented | Later phase |
| OMS/UCI application integration | Not implemented | Separate project concern |
| Rust sys/safe bindings | Not implemented | Later; same native ABI |
| Alire/crates.io publication | Not performed | See packaging checklist |
| Formal verification / GRA compliance | Not claimed | Separate evidence required |

Implementation and verification are different. See `bootstrap-validation.md`
for the commands actually executed when this starter archive was prepared.
