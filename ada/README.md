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
It also declares the safe value types for the `@RequiredIfTrack`
`IRSTTrackReport` callback: `IRST_Track_State` (`State_Idle`/`Detected`/`Coast`/
`Dropped`), `IRST_Track_Mode` (`Mode_Idle`/`Scan`/`Stare`), a Track-owned
`North_East_Down` record that deliberately creates no dependency on
`AMS.MEL.IR.Image`, and the complete `IRST_Track_Report`. The `State_`/`Mode_`
prefixes exist only because both upstream enumerations declare an `Idle`.

`AMS.MEL.IR.Track.Metadata` polls that callback with the same limited-private
`Metadata_Channel` shape used by the other families:
`Open`/`Is_Open`/`Receive`/`Counters`/`Close` with a caller-chosen
`Queue_Capacity`. `Receive` obtains a native event owner, views it, validates
the event kind and both raw enumeration values explicitly rather than through
unchecked enumeration conversion, copies every field into Ada-owned storage,
closes the native owner -- including on a conversion exception -- and returns an
Ada value; no native pointer escapes. Registration is one-shot because upstream
declares no unregister, so the callback state belongs to the `Track_Channel`;
`Close` deactivates public consumption only and provider channel destruction
remains the callback-quiescence boundary.

`AMS.MEL.IR.Track.Updates` implements the separately conditional
(`@RequiredIfTrackUpdate`) `TrackChannel::send (TrackDataUpdate)` and keeps the
`AMS.MEL.IR.Track` parent package focused. It exposes `Track_Status`
(`Create`/`Update`/`Predict`/`Delete`, with the exact upstream 0..3
representation and a 32-bit size), a Track-update-owned `Directional` record
that adds no `AMS.MEL.IR.Image` dependency, a `Track_Covariance` record with all
21 named terms corresponding exactly to the C record, and the complete
`Track_Data_Update` with every upstream field. Both times stay in upstream epoch
seconds and no value is clamped or normalized, because the upstream setters
perform no such validation.

Its `Command_State` and `Cannot_Comply` are defined locally with the same
published numeric representations already used by the C ABI, deliberately
without depending on `AMS.MEL.IR.C2.Metadata` merely to reuse enumerations; a
cross-package neutralization refactor is out of scope. `Command_Status` is an
Ada-owned private value with `Command_ID`/`State`/`Reason`/`Reason_Description`
accessors whose description is copied into Ada-owned storage during `Wait`, so
no C pointer escapes that call. `Update_Result` follows the Instrumentation safe
result model: `Status` returns `Success` or `Rejected`, `Command` is valid only
on `Success`, and `Rejection_Code`/`Description` only on `Rejected`. A successful
provider `CommandStatus` whose own `State` is `Rejected` is still a `Success`
outcome, because that is not an `ErrorOr` rejection.

`Update_Request` is limited private with controlled finalization:
`Submit`/`Is_Open`/`Wait`/`Close`. Submission requires an enabled
`Track_Channel`. A native `AMS_MEL_TIMEOUT` raises the inherited `Timeout_Error`
and never cancels or consumes the request; native `AMS_MEL_OK` yields a `Success`
result, native `AMS_MEL_COMMAND_REJECTED` a `Rejected` result, and any other
native status raises `Provider_Error`. A complete rejection diagnostic is
recovered with a second cached `Wait (0)` using exact storage when the fixed
diagnostic buffer was too small. `Close` and finalization release the public
request owner only; they are not cancellation, and a pending request keeps the
provider future and Track channel alive independently of the public
`Track_Channel` and `Session` owners.

`TrackDataUpdate`, `SystemTrackDataResponse`, `CandidateObjectMessage`,
`CandidateObjectPreProcMessage`, and `RequestSystemTrackData` are not
implemented.

The `AMS` root package is owned here; future companion Ada crates must depend
on its owning crate rather than duplicate `ams.ads`.

This development manifest is not ready for indexing. The relative native pin
is useful in the workspace, not a published dependency resolution mechanism.
