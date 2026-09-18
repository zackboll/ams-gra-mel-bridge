# Ada binding bootstrap

`alr build` builds the Ada library and its pinned native dependency.
`alr -C tests run` builds/runs the separate smoke-test crate.

The public packages expose Ada-native Session, IR Mono8, and required C2 command
APIs. `AMS.MEL.IR.C2` supports general ModeCmd with complete ScanParam, safe
one-choice BIT initiate/cancel/clear-fault operations, BIT no-op, and ConfigSet.
`AMS.MEL.IR.C2.Metadata` safely receives complete CommandStatus,
BIT_Configuration, and BIT_Status values (including nested faults) through a
bounded native queue. Provider callback threads never invoke Ada application
code, and returned events contain only Ada-owned values. C imports and borrowed
spans remain private. Inherited Common Channel services, optional C2 operations,
and all other MEL families remain future work.
The `AMS` root package is owned here; future companion Ada crates must depend
on its owning crate rather than duplicate `ams.ads`.

This development manifest is not ready for indexing. The relative native pin
is useful in the workspace, not a published dependency resolution mechanism.
