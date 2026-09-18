# Task 020 validation

Task 020 adds the Ada Health/Status channel and required metadata vertical slice.
The C ABI remains version 0.1 and contains exactly 46 exported functions. The ten
new operations own Health channels, metadata streams, immutable events, and the
existing complete ChannelCapability snapshot explicitly.

The native adapter registers exactly MFA_Status, BIT_Status,
SubsystemStatusResp, DiscreteStatus, MFA_SecurityAuditRecord, and
MFA_StatusDetailed. It uses a bounded DROP-INCOMING FIFO, saturating counters,
native callback deep copies, no direct Ada callback, retained partial
registrations, and provider channel destruction as the quiescence boundary.
LFStatus and NUC_TempData are excluded.

`AMS.MEL.Status` owns reusable Common-MEL MFA, BIT, name/value, and security
values. `AMS.MEL.IR.Health_Status.Metadata` owns Health-specific subsystem values
and copies every returned graph into Ada storage before closing the native event.
Task 018's existing public C2 metadata API remains source-compatible.

Native mock tests cover open/enable/capabilities/close, all six registrations,
rich MFA/BIT/subsystem/pair payloads, every SecurityAudit variant, malformed
nested UTF-8 recovery, queue overflow, callback allocation failure, partial
registration, non-quiescing disable, metadata-first and parent-first teardown,
event lifetime after provider unload, and retryable detach. The Ada suite prints:

```text
PASS: Ada IR Health/Status contract
```

Rust sys and private Python ctypes declarations track the raw 46-function ABI.
No safe Rust or public Python Health API is added.

The six added vendored headers are byte-identical to IR MEL commit
`8d9224519f12b44e0b28815755c56a32a28d24a0`; hashes are recorded in
`upstream-files.sha256.md`. Ordinary builds remain provider-free.

## Pinned Squall runtime acceptance

On 2026-09-18, `make test-squall-ir-ada` started the real pinned Squall
provider/container stack from clean checkout
`b1015728f904c799fa0c07489fce48e78f67845f` using Podman 5.4.2 and
`podman-compose`. The provider reported API version 1, library version 1,
vendor `Squall`, and description `Squall Simulator IR MEL`. The Ada-only
runtime ports were Couloir control `39203`, Couloir metrics `39318`, optical
health `39315`, and optical metrics `39316`.

The client opened Session, Image, C2, and Health/Status. Health capability
contained `HealthAndStatus` and the currently enumerable
`ChannelCommsTestRep`, `MFAStatus`, `BITStatus`, `SubsystemStatusResp`, and
`MFAStatusDetailed` capabilities. `DiscreteStatus` and
`MFA_SecurityAuditRecord` are required registered callbacks but are not
enumerable capability values in this pinned interface. All six callback
registrations succeeded.

The unordered polling receive observed `MFA_Status`, `BIT_Status`,
`SubsystemStatusResp`, `DiscreteStatus`, and `MFA_StatusDetailed`. After the
accepted Standby/Unused C2 command, MFA_Status was Standby, `Squall OK`,
`IR backend status available`, NotTransitioning, model `Squall IR MFA`, and
software version `unknown`; serial number, bootloader version, and hardware
version were empty. BIT counts were active=0, completed=0, fault=0.
SubsystemStatusResp was id=0, criticality=0, sequence=0, Available, with zero
subsystems and CSCIs. DiscreteStatus and MFA_StatusDetailed each had zero
pairs. Health counters were received=5, dropped=0, malformed=0. These are
pinned-Squall observations, not generic MEL requirements.

SecurityAudit callback registration succeeded, but the pinned Squall simulator
did not emit a SecurityAudit event; rich mock tests provide complete
SecurityAudit fidelity coverage. The same run retained C2 accepted/rejected
mode, BIT, ConfigSet, CommandStatus, pre-enable KeepAlive, CommsTest,
capability, three Mono8-frame, and clean C2/Image-counter checks.

Final one-startup all-language acceptance used ports `40203`, `40318`, `40315`,
and `40316` respectively and passed C, Ada, Rust, Python, and the combined
one-iteration integration result. Ada Health counters were received=10,
dropped=0, malformed=0, reflecting an allowed additional polling cycle.
C/Rust/Python retained their existing safe subset; only Ada exercised the
Health behavior above.
