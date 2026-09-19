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
The `AMS` root package is owned here; future companion Ada crates must depend
on its owning crate rather than duplicate `ams.ads`.

This development manifest is not ready for indexing. The relative native pin
is useful in the workspace, not a published dependency resolution mechanism.
