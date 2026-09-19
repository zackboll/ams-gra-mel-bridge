# Implementation coverage

| Area | Status | Evidence / next step |
|---|---|---|
| C ABI header usable from C11 | Implemented | `native/tests/test_c_abi.c` |
| Native facade version query | Implemented | C and C++ tests |
| Private Ada import and public version value | Implemented | `ada/tests/src/ams_mel_smoke.adb` |
| Native CMake package installation | Implemented | Exported target and config files |
| Local Alire dependency manifests | Development only | Relative pins; validate on installed Alire |
| Upstream version inventory | Verified through task 003 | `upstream-provenance.md` exact commits/trees |
| Verified upstream dependency closure | Implemented for IR receive/C2/Health/Instrumentation slices | 94 unmodified headers; no upstream compiled source |
| Separately loadable mock C++ MEL provider | Implemented, test-only | Native C and Ada contract tests |
| Real MEL factory/resource adaptation | Control foundation implemented | load/factory/init/version/close only |
| Buffer and callback lifetimes | Hardened with non-quiescing mock | Channel destruction before in-flight drain/storage; release checked exactly once |
| IR image receive | Mono8 receive implemented | Host-memory Mono8 only; legacy subset plus complete owned snapshot in C/Ada |
| IR C2 required sends | Complete in native C and safe Ada | General ModeCmd/complete ScanParam, intended BIT choices, and ConfigSet; existing request owners reused |
| IR C2 safe Rust/Python subset | Constrained existing subset | Operate/TaskSched and BIT no-op only; no safe Task-017 expansion |
| Pending C2 lifetime | Hardened and lifecycle-tested | Pre-send allocation; allocation-free emergency roots; launch/allocation failure injection; deferred disable/detach/unload |
| C2/image coexistence | Implemented | One Session, parent-first close, provider unload last |
| IR C2-specific required metadata | Complete in native C and safe Ada | BIT_Configuration, CommandStatus, and BIT_Status; complete ordered nested values through one queue |
| C2 application-facing inherited Channel services | Complete in native C and safe Ada | KeepAlive; CommsTest async reply and callback; complete owned ChannelCapability; valid before Enable |
| IR Health/Status required metadata | Complete in native C and safe Ada | Six required callbacks; complete MFA/BIT/subsystem/name-value/SecurityAudit values; bounded owned polling queue |
| Explicit generic buffer management | Not application-exposed | IRSTImage registration remains adapter-managed; future resource task |
| Optional/conditional C2 commands | Not implemented | No calibration, camera, erase, or system-track-response commands |
| Bounded receive queue | Implemented | Caller capacity; DROP-INCOMING; saturating counters |
| Legacy FrameHeader interface | Compatibility subset retained | `ams_mel_ir_frame_v1` and `AMS.MEL.IR.Receive` unchanged |
| Full FrameHeader snapshot | Complete in native C and safe Ada | Owned snapshot, contributing sensor, ordered flags, inertial/nav vectors and independent orientations |
| ImageChannel metadata callbacks | All required callbacks implemented in native C and safe Ada | BadPixelList, LineOfSightReport, LineOfSightEuler, and NavigationReportResp share one bounded DROP-INCOMING queue; owned events, counters, malformed/allocation recovery |
| NavigationReport send | Implemented in native C and safe Ada | Complete published NavigationReport/all 18 covariance terms; async request/future, deferred teardown while pending, allocation-free emergency retention; ABI 0.1 now 59 exports |
| Real IR provider validation | Implemented and passed | Ada adds required C2-specific metadata with 10/0/0 counters plus general mode/rejection, BIT payload, and ConfigSet; all languages retain BIT no-op, TaskSched, and three 320x200 Mono8 frames |
| IR Instrumentation conditional surface | Complete in native C and safe Ada | send(InstrumentationLevelCmd) and InstrumentationReport callback plus Enable and ChannelCapability; positive behavior mock-validated; pinned Squall explicitly unsupported |
| Instrumentation inherited generic Channel services | Not implemented | KeepAlive/CommsTest/buffers intentionally not cloned per family; generalize later |
| RF apertures/jobs/receive/VADB | Not implemented | Later phase |
| OMS/UCI application integration | Not implemented | Separate project concern |
| Rust sys binding | Complete for the current project C ABI | Exactly 72 C functions; raw Instrumentation declarations/constants synchronized; no safe Instrumentation API |
| Safe Rust binding | Session + IR Mono8 + C2 Operate/TaskSched + BIT no-op implemented | Typed Return values/results and reusable ReturnRequest; no payload-bearing BIT or additional C2/RF API |
| Real Squall Rust validation | Implemented and passed | Same pinned Task-004 provider/runtime; BIT Success, TaskSched, parent-first close, both cached waits, real 320x200 Mono8 frames, counters, and explicit teardown through safe API |
| Python binding | Session + IR Mono8 + C2 Operate/TaskSched + BIT no-op implemented | Complete current 72-function private ctypes binding; no public Image metadata/Navigation/Instrumentation methods |
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
| Image metadata | BadPixelList, LineOfSightReport, LineOfSightEuler, and NavigationReportResp complete |
| Scheduling | Not implemented |
| Track | Not implemented |
| Health/Status | Complete: required channel plus six required callbacks; LFStatus/NUC_TempData excluded |
| Instrumentation | Instrumentation-specific conditional surface complete (send/InstrumentationReport callback plus Enable and ChannelCapability); inherited generic Channel services not implemented |
| StackedImage | Not implemented |
| RF | Not implemented |

Implementation and verification are different. See `bootstrap-validation.md`
for the commands actually executed when this starter archive was prepared.
