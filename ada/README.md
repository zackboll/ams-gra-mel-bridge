# Ada binding bootstrap

`alr build` builds the Ada library and its pinned native dependency.
`alr -C tests run` builds/runs the separate smoke-test crate.

The public packages expose Ada-native Session, IR Mono8, and required C2 command
APIs. `AMS.MEL.IR.C2` supports general ModeCmd with complete ScanParam, safe
one-choice BIT initiate/cancel/clear-fault operations, BIT no-op, and ConfigSet.
`AMS.MEL.IR.C2.Metadata` safely receives complete CommandStatus,
BIT_Configuration, and BIT_Status values (including nested faults) through a
bounded native queue and can explicitly add ChannelCommsTest events.
`AMS.MEL.IR.Channel` defines reusable Ada-native common values, while
`AMS.MEL.IR.C2.Common` provides pre-enable KeepAlive, asynchronous CommsTest,
and wholly Ada-owned complete ChannelCapability snapshots. Provider callback threads never invoke Ada application
code, and returned events contain only Ada-owned values. C imports and borrowed
spans remain private. Explicit generic buffer ownership, optional C2 operations,
Scheduling, and all other MEL families remain future work.
`AMS.MEL.IR.Image` is the separate full-FrameHeader receive path.  It receives an
owned native snapshot, deep-copies every nested value and pixel, closes the native
owner, and returns an Ada-owned `Full_Frame`; the existing `AMS.MEL.IR.Receive`
remains the source-compatible Mono8 compatibility subset.
`AMS.MEL.IR.Image.Capabilities` reuses the complete internal ChannelCapability
converter used by C2. `AMS.MEL.IR.Image.Metadata` owns BadPixelList,
LineOfSightReport, LineOfSightEuler, and NavigationReportResp values from one bounded native
DROP-INCOMING queue and releases the native event before returning.
`AMS.MEL.IR.Image` also owns the complete published `Navigation_Report` (all 18
`Position_Velocity_Covariance` terms), the limited `Navigation_Request` owner
returned by `Submit_Navigation_Report`, and `Navigation_Result`. Submission is
valid while the stream is logically Attached or Running; `Close` on the request
is not cancellation, and a pending request keeps the underlying Image stream
state (and provider channel) alive independently of the public `Image_Stream`
and `Session` owners. The canonical `Navigation_Response` lives here too;
`AMS.MEL.IR.Image.Metadata.Navigation_Response` is now a source-compatible
subtype. LineOfSightQuaternion and other optional Image metadata are not
implemented.
`AMS.MEL.IR.Instrumentation` implements the conditionally required
Instrumentation family: `Instrumentation_Config`/`Open`/`Is_Open`/`Enable`/
`Capabilities`/`Close`, the idiomatic `Instrumentation_Level_Command` and
`Instrumentation_Report` records, `Priority` as exactly `Normal`/`Debug`, and
the limited `Instrumentation_Request` owner returned by `Submit` with
`Wait`/`Close` and a local `Instrumentation_Result`. Submission requires an
enabled channel; `Close` on the request is not cancellation, and a pending
request keeps the provider channel alive independently of the public
`Instrumentation_Channel` and `Session` owners. `Capabilities` reuses the same
internal ChannelCapability converter as C2 and Image.
`AMS.MEL.IR.Instrumentation.Metadata` polls the `InstrumentationReport`
callback from one bounded native DROP-INCOMING queue and returns wholly
Ada-owned `Instrumentation_Report` values; `Close` deactivates public
consumption only, since upstream has no unregister operation and channel
destruction remains the callback-quiescence boundary. Instrumentation-specific
copies of the inherited generic Channel services are deliberately absent.

`AMS.MEL.IR.Track` implements only the conditionally required Track family's
(`@RequiredIfTrack`) channel ownership/lifecycle foundation:
`Track_Config`/`Create_Config`, and the limited controlled `Track_Channel` with
`Open`/`Is_Open`/`Enable`/`Capabilities`/`Close`. `Capabilities` is valid while
attached or enabled and reuses the one shared Ada `ChannelCapability`
converter; `Enable` is idempotent once enabled. Finalization closes an
un-closed channel through the same non-raising fallback used by the other
families, and a native detach failure deliberately leaves the underlying owner
intact so the retained provider graph is never destroyed. A `Track_Channel`
keeps the provider/session graph alive independently of its parent `Session`.
No `IRST_Track_Report`, Track metadata package, Track enumerations, or NED type
is declared: `IRSTTrackReport`, `TrackDataUpdate`, `SystemTrackDataResponse`,
`CandidateObjectMessage`, `CandidateObjectPreProcMessage`, and
`RequestSystemTrackData` are not implemented.

The `AMS` root package is owned here; future companion Ada crates must depend
on its owning crate rather than duplicate `ams.ads`.

This development manifest is not ready for indexing. The relative native pin
is useful in the workspace, not a published dependency resolution mechanism.
