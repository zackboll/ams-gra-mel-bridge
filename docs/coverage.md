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
| IR C2 required sends | Complete in native C and safe Ada | General ModeCmd/complete ScanParam, intended BIT choices, and ConfigSet; existing request owners reused |
| IR C2 safe Rust/Python subset | Constrained existing subset | Operate/TaskSched and BIT no-op only; no safe Task-017 expansion |
| Pending C2 lifetime | Hardened and lifecycle-tested | Pre-send allocation; allocation-free emergency roots; launch/allocation failure injection; deferred disable/detach/unload |
| C2/image coexistence | Implemented | One Session, parent-first close, provider unload last |
| IR C2-specific required metadata | Complete in native C and safe Ada | BIT_Configuration, CommandStatus, and BIT_Status; complete ordered nested values through one queue |
| C2 application-facing inherited Channel services | Complete in native C and safe Ada | KeepAlive; CommsTest async reply and callback; complete owned ChannelCapability; valid before Enable |
| Explicit generic buffer management | Not application-exposed | IRSTImage registration remains adapter-managed; future resource task |
| Optional/conditional C2 commands | Not implemented | No calibration, camera, erase, or system-track-response commands |
| Bounded receive queue | Implemented | Caller capacity; DROP-INCOMING; saturating counters |
| Full FrameHeader metadata | Partial by design | Omits contributing sensor and inertial/navigation vectors |
| Real IR provider validation | Implemented and passed | Ada adds required C2-specific metadata with 10/0/0 counters plus general mode/rejection, BIT payload, and ConfigSet; all languages retain BIT no-op, TaskSched, and three 320x200 Mono8 frames |
| RF apertures/jobs/receive/VADB | Not implemented | Later phase |
| OMS/UCI application integration | Not implemented | Separate project concern |
| Rust sys binding | Complete for the current project C ABI | Exactly 36 C functions; raw common-channel declarations synchronized; not safe feature parity |
| Safe Rust binding | Session + IR Mono8 + C2 Operate/TaskSched + BIT no-op implemented | Typed Return values/results and reusable ReturnRequest; no payload-bearing BIT or additional C2/RF API |
| Real Squall Rust validation | Implemented and passed | Same pinned Task-004 provider/runtime; BIT Success, TaskSched, parent-first close, both cached waits, real 320x200 Mono8 frames, counters, and explicit teardown through safe API |
| Python binding | Session + IR Mono8 + C2 Operate/TaskSched + BIT no-op implemented | Complete current 36-function private ctypes binding; common-channel declarations have no public safe methods |
| Real Squall Python validation | Implemented and passed | Same pinned provider/runtime; BIT Success, TaskSched, parent-first close, both cached waits, real 320x200 Mono8 `bytes` frames, counters, and explicit teardown |
| Additional Python C2/RF | Not implemented | No payload-bearing BIT, scan/config/camera commands, callbacks, or RF; no zero-copy/NumPy views |
| Python packaging/publication | Not performed | `PYTHONPATH=python` development use only; no wheel or PyPI dependency |
| Alire/crates.io publication | Not performed | Rust crates also remain unpublished |
| Formal verification / GRA compliance | Not claimed | Separate evidence required |

## Ada IR feature-family progress

| Feature family | Ada status |
|---|---|
| C2 required sends | Complete |
| C2-specific required metadata callbacks | Complete: BIT_Configuration, CommandStatus, BIT_Status |
| Common inherited Channel services | Complete: KeepAlive, CommsTest send/reply/callback, complete ChannelCapability |
| Explicit generic buffer management | Not application-exposed |
| Image receive | Partial: host-memory Mono8 |
| Image metadata | Not implemented beyond current frame subset |
| Scheduling | Not implemented |
| Track | Not implemented |
| Health/Status | Not implemented |
| Instrumentation | Not implemented |
| StackedImage | Not implemented |
| RF | Not implemented |

Implementation and verification are different. See `bootstrap-validation.md`
for the commands actually executed when this starter archive was prepared.
