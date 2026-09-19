# Task 024 validation

Task 024 keeps façade ABI version 0.1 and grows the export inventory from 49 to
exactly 56. It adds `ams_mel_ir_stream_get_capabilities` and six Image metadata
owner/event operations. The fixed legacy `ams_mel_ir_frame_v1`,
`ams_mel_ir_stream_receive`, full-frame snapshot, existing C2, and existing Health
signatures remain unchanged.

Image capability snapshots reuse the generic native deep copy. Safe Ada uses one
internal native-view-to-value converter for both C2 and Image while each caller retains
exception-safe ownership of its native capability handle. Existing rich C2 capability
tests pass unchanged, and the returned Image capability remains usable after stream,
Session, and provider teardown.

Image metadata infrastructure implements only BadPixelList. Registration is one-shot
and callback-ready before provider registration, including synchronous callbacks. The
bounded FIFO uses DROP-INCOMING with saturating counters. Events preserve reported size,
reported count, actual ordered pixel count, high-bit uint32 row/column values, and the
validated Unknown reason. Native and Ada event values outlive metadata, stream, Session,
and provider teardown. Tests cover malformed reason and allocation-failpoint recovery,
overflow ordering/counters, metadata close before a later in-flight callback, and
channel-destruction quiescence.

Raw Rust and private Python declarations include the new constants, records, opaque
owners, exact function signatures, and size/alignment/offset probes. No safe Rust
metadata API or public Python metadata API is added.

Local validation includes native 8/8, the full native suite repeated 50 times, the
dedicated Image metadata contract repeated 100 times, Ada formatting/build/tests,
Rust and Python ABI probes/tests, and an exact 56-symbol export audit.

Pinned Squall validation uses checkout
`b1015728f904c799fa0c07489fce48e78f67845f`. Ada requires Image capability 320x200,
8-bit, one-band Mono with BadPixelList advertised, opens metadata before image start,
and requires the synchronous initial BadPixelList to report size zero, count zero, and
zero actual pixels. Existing legacy frames, full FrameHeader, C2, and Health evidence is
retained by the same all-language harness.

LineOfSightReport is **not implemented**. LineOfSightEuler is **not implemented**.
NavigationReportResp is **not implemented**. NavigationReport send is **not
implemented**. No vendor-closure expansion was required.
