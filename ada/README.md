# Ada binding bootstrap

`alr build` builds the Ada library and its pinned native dependency.
`alr -C tests run` builds/runs the separate smoke-test crate.

The public packages expose Ada-native Session, IR Mono8, and required C2 command
APIs. `AMS.MEL.IR.C2` supports general ModeCmd with complete ScanParam, safe
one-choice BIT initiate/cancel/clear-fault operations, BIT no-op, and ConfigSet.
C imports and borrowed spans remain private. Required C2 metadata callbacks and
all other MEL families remain future work.
The `AMS` root package is owned here; future companion Ada crates must depend
on its owning crate rather than duplicate `ams.ads`.

This development manifest is not ready for indexing. The relative native pin
is useful in the workspace, not a published dependency resolution mechanism.
