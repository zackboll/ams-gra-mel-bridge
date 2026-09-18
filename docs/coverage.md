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
| IR C2 BIT no-op | Implemented in C, Ada, safe Rust, and safe Python | Command ID plus empty initiate/cancel/clear-fault lists; reusable async Return request |
| Pending C2 lifetime | Hardened and lifecycle-tested | Pre-send allocation; allocation-free emergency roots; launch/allocation failure injection; deferred disable/detach/unload |
| C2/image coexistence | Implemented | One Session, parent-first close, provider unload last |
| Other C2 commands/callbacks | Not implemented | No payload-bearing BIT, scan scheduling, config, camera, or CommandStatus API |
| Bounded receive queue | Implemented | Caller capacity; DROP-INCOMING; saturating counters |
| Full FrameHeader metadata | Partial by design | Omits contributing sensor and inertial/navigation vectors |
| Real IR provider validation | Implemented and passed | Pinned Squall: C/Ada/Rust/Python BIT no-op, TaskSched, and three 320x200 Mono8 frames; opt-in and excluded from ordinary CI |
| RF apertures/jobs/receive/VADB | Not implemented | Later phase |
| OMS/UCI application integration | Not implemented | Separate project concern |
| Rust sys binding | Complete for the current project C ABI | Exactly 19 C functions; C-header signature/layout/constant drift probe includes BIT; not a claim of complete MEL functionality |
| Safe Rust binding | Session + IR Mono8 + C2 Operate/TaskSched + BIT no-op implemented | Typed Return values/results and reusable ReturnRequest; no payload-bearing BIT or additional C2/RF API |
| Real Squall Rust validation | Implemented and passed | Same pinned Task-004 provider/runtime; BIT Success, TaskSched, parent-first close, both cached waits, real 320x200 Mono8 frames, counters, and explicit teardown through safe API |
| Python binding | Session + IR Mono8 + C2 Operate/TaskSched + BIT no-op implemented | Complete current 19-function private façade binding; typed Return completion/rejection and reusable ReturnRequest |
| Real Squall Python validation | Implemented and passed | Same pinned provider/runtime; BIT Success, TaskSched, parent-first close, both cached waits, real 320x200 Mono8 `bytes` frames, counters, and explicit teardown |
| Additional Python C2/RF | Not implemented | No payload-bearing BIT, scan/config/camera commands, callbacks, or RF; no zero-copy/NumPy views |
| Python packaging/publication | Not performed | `PYTHONPATH=python` development use only; no wheel or PyPI dependency |
| Alire/crates.io publication | Not performed | Rust crates also remain unpublished |
| Formal verification / GRA compliance | Not claimed | Separate evidence required |

Implementation and verification are different. See `bootstrap-validation.md`
for the commands actually executed when this starter archive was prepared.
