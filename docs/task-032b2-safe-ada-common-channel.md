# Task 032B2 — safe Ada common Channel façade

Starting `origin/main`: `b77b8f0372916335dde77a31cf6e08d5b1275540` (Task 032B1,
PR #51, merged). Branch: `feature/032b2-safe-ada-common-channel`.

032B2 exposes the 032B1 public C common Channel ABI through the safe Ada API
for all five typed families (C2, Image, Health, Instrumentation, Track). No
production native source, public header, or export map changes; ABI stays
**0.1** with exactly **100** production exports.

## Removing the Channel -> C2 dependency

`AMS.MEL.IR.Channel` used to `with AMS.MEL.IR.C2` only because
`Comms_Test_Report.Command_ID` was typed as `C2.Command_ID`. That inverse
dependency made it impossible for C2 to depend on `Channel` (for
`As_Channel`) without a cycle.

The canonical identifier now lives in the common package:

```ada
package AMS.MEL.IR.Channel is
   type Command_ID is mod 2**32 with Size => 32;
   type Comms_Test_Report is record
      Command_ID : AMS.MEL.IR.Channel.Command_ID;
      Request_ID : Comms_Request_ID;
   end record;
```

and `AMS.MEL.IR.C2` keeps the old name as a compatibility subtype:

```ada
subtype Command_ID is AMS.MEL.IR.Channel.Command_ID;
```

`AMS.MEL.IR.Channel` no longer withs `AMS.MEL.IR.C2` (the only remaining
mentions of C2 in it are comments). A subtype with no constraint names the same
type, so `ID : C2.Command_ID`, every C2 operation signature that spells
`Command_ID`, and `use type C2.Command_ID` compile unchanged. Values move
between the two names with no conversion. The new test checks this at compile
time (assignments in both directions plus `pragma Compile_Time_Error` on
`'Size = 32` and equal ranges) and at run time (a canonical ID passed to
`C2.Submit_Operate`, a legacy ID passed to `Channel.Submit_Comms_Test`, and the
echoed report compared through both names).

## Parent-private `Channel_View`

```ada
package AMS.MEL.IR is
   type Channel_View is limited private;
private
   type Channel_View is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Channel_Handle := AMS.MEL_C_API.Null_Channel;
   end record;
   overriding procedure Finalize (Object : in out Channel_View);
end AMS.MEL.IR;

package AMS.MEL.IR.Channel is
   subtype View is AMS.MEL.IR.Channel_View;
```

Every typed-family package (`C2`, `Image`, `Health_Status`, `Instrumentation`,
`Track`) and `Channel` itself is a child of `AMS.MEL.IR`. Under Ada visibility
rules, a child's body (and private part) can see the parent's private part. So
each family body can read its own owner's private native handle and write
`Channel_View.Handle`, while application code sees only a limited private type.

The early stop gate passed: GNAT accepted `C2.As_Channel` initializing
`Result.Handle` inside an extended `return ... do` block with no special
pragmas. A negative probe compiled outside the hierarchy got these errors:

```text
probe.adb:8:19: error: invalid prefix "V" in selected component     -- V.Handle
probe2.adb:1:06: error: unit in with clause is private child unit    -- with AMS.MEL_C_API
```

No public API exposes `System.Address`, `AMS.MEL_C_API.*_Handle`, or any
"native handle" accessor. `AMS.MEL_C_API` is a private package, and every
public spec reaches it only through `private with`.

## Five conversions

```ada
function C2.As_Channel              (Channel : Control_Channel)          return Channel.View;
function Image.As_Channel           (Object  : AMS.MEL.IR.Image_Stream) return Channel.View;
function Health_Status.As_Channel   (Channel : Health_Channel)           return Channel.View;
function Instrumentation.As_Channel (Channel : Instrumentation_Channel)  return Channel.View;
function Track.As_Channel           (Channel : Track_Channel)            return Channel.View;
```

Each one calls the matching raw 032B1 conversion (`IR_Channel_From_C2`,
`_From_Stream`, `_From_Health`, `_From_Instrumentation`, `_From_Track`). A
closed typed owner is checked explicitly in Ada first and raises
`AMS.MEL.Provider_Error` with a family-specific message ("IR C2 channel is
closed", ...). If the native conversion fails, the build-in-place result is
finalized (a NULL-handle close is harmless), so no partially-open View ever
escapes.

## Weak View semantics

`Channel.View` owns exactly one native `ams_mel_ir_channel *`, and that native
object is itself weak. A View therefore does **not** own the typed family state,
Session, provider Channel, Control, or provider library. When the typed owner
and its Session are closed, a still-open View reports `Is_Open = True`, but
`Send_Keep_Alive`, `Submit_Comms_Test`, and `Capabilities` raise
`Provider_Error`, and `Channel.Close (View)` still succeeds. Closing or
finalizing a View never closes, enables, disables, or detaches the typed owner,
and never cancels a pending request. Admitted requests own their native request
handles, and the native request graph retains the family lifetime only after
admission.

`Close (View)` calls `IR_Channel_Close`, is idempotent, clears the handle, and
raises `Provider_Error` only on an unexpected native failure. `Finalize` calls
the same close without raising; on any unexpected exception it suppresses the
exception and clears the handle.

## Common request/result types

`AMS.MEL.IR.Channel` declares its own `Outcome`, `Error_Code`, and
`Command_Return`, using the same order and representation as the native mapping
and the legacy C2 types (`None .. Unsupported` = 0..8,
`Return_Success .. Not_Implemented` = 0..4).

- `Return_Request` (limited, controlled owner of `Return_Request_Handle`):
  `Send_Keep_Alive`, `Is_Open`, `Wait`, `Close`; `Return_Result` with `Status`,
  `Value`, `Rejection_Code`, `Description` (with the same preconditions as C2).
- `Comms_Request` (limited, controlled owner of `Comms_Request_Handle`):
  `Submit_Comms_Test`, `Is_Open`, `Wait`, `Close`; `Comms_Result` with
  `Status`, `Report`, `Rejection_Code`, `Description`.
- `Capabilities (View)`: `IR_Channel_Get_Capabilities`, then
  `IR_Capability_View` and the existing
  `Capability_Conversion.To_Channel_Capability`. The native capability owner
  is closed on success, on a conversion exception, and on a provider failure
  after allocation. The returned `Channel_Capability` holds only Ada-owned
  values, so it remains valid after View Close, typed Close, Session Close,
  and provider unload.

These wrap the **same** native request handles and engines as the legacy C2
types. Keeping a small duplicate Ada owner was preferred over aliasing or
moving the legacy public types.

| Native | Safe Ada |
|---|---|
| submit `AMS_MEL_RESOURCE_EXHAUSTED` | `AMS.MEL.Resource_Exhausted` (inherited `Check_Submission`) |
| other submit failure (incl. expired or closed view) | `AMS.MEL.Provider_Error` |
| Wait `AMS_MEL_OK` | `Success` (+ `Value` or `Report`) |
| Wait `AMS_MEL_COMMAND_REJECTED` | `Rejected` + `Error_Code` + complete diagnostic |
| Wait `AMS_MEL_TIMEOUT` | `AMS.MEL.IR.Timeout_Error` (request not consumed) |
| other Wait failure / unknown enum value | `AMS.MEL.Provider_Error` |

Upstream `Return::Fail` is a *successful* completion: `Status = Success`,
`Value = Fail`. It is never `Provider_Error`.

A rejection diagnostic longer than the fixed 512-byte buffer takes the same
path as legacy C2/C2.Common. The code allocates exactly the required Ada
buffer, repeats the cached Wait with timeout zero, checks that the status,
error code, and required length are unchanged, and fails closed with
`Provider_Error` if the cached result changed. The description is never
truncated. Timeout never cancels; a later Wait returns the terminal result,
and repeated Waits return the identical cached result. Close must not race
Wait on the same request. Request finalizers close the native request without
raising and clear the handle on any exception, and explicit Close is
idempotent.

## Legacy `C2.Common` compatibility

`ada/src/ams-mel-ir-c2-common.ads`, `ada/src/ams-mel-ir-c2-common.adb`,
`ada/tests/src/ams_mel_ir_c2_common_tests.ads`, and
`ada/tests/src/ams_mel_ir_c2_common_tests.adb` are **byte-identical** to the
starting SHA. They compile against the new `Command_ID` subtype unchanged, and
`AMS_MEL_IR_C2_Common_Tests` stays registered and passes. The legacy
`C2.Return_Request`, `Return_Result`, `Outcome`, `Error_Code`,
`Command_Return`, `C2.Common.Comms_Request`, and `Comms_Result` were not moved
or aliased.

## Test-only mock support

The 032A Health/Instrumentation/Track mock common operations use held futures
that are released through `dlsym` gates. The safe Ada tests avoid those gates
and use three new ready scenarios in `native/tests/mock_provider.cpp` (test
provider only, no production change):

```text
health-ada-common   instr-ada-common   track-ada-common
```

In these scenarios inherited KeepAlive and CommsTest return already-completed
futures, and CommsTest requires exactly `command_id = 0x80000001`,
`channel_id = 0xf0000002`, `request_id = 0xe0000003` (Health already required
this in every scenario). All other behavior is the family's normal behavior.
C2 and Image use the existing `comms-high` scenario, which also validates the
high IDs.

## Safe Ada tests (`AMS_MEL_IR_Channel_Tests`)

A generic `Family_Tests` is instantiated for each of the five families:

- **Lifecycle**: KeepAlive (cached repeat Wait), high-ID CommsTest (exact
  `Command_ID`/`Request_ID` plus cached repeat), and Capabilities (compared
  with the typed capability), both while Attached and after Enable/Start.
  Instrumentation `Submit` and Track `TrackDataUpdate`/
  `SystemTrackDataResponse` still raise `Provider_Error` while only Attached.
  Typed C2 Operate and Instrumentation Submit work after Enable.
- **Weak**: typed Close + Session Close with the View kept alive. KeepAlive,
  CommsTest, and Capabilities raise `Provider_Error`; View Close succeeds and
  is idempotent.
- **Close first**: View Close leaves the typed owner open; typed Capabilities
  are unchanged; Enable/Start and typed Close work. The later View
  finalization is harmless.
- **Multiple**: two Views, one closed; the other stays usable and expires
  after typed/Session Close.
- **Closed owner**: `As_Channel` raises `Provider_Error`.

C2-scenario tests: `keepalive-fail` (Success/Fail), `keepalive-reject` and
`comms-reject` (Rejected, expected code, full 613-byte UTF-8 diagnostic,
cached), `keepalive-delayed`/`comms-delayed` (Wait 0 -> `Timeout_Error`, later
terminal result, identical cached result), `keepalive-lifetime` parent-first
(View, typed C2, and Session all close while the KeepAlive is pending, and the
safe request still completes), `capability-rich` snapshot lifetime (the full
legacy rich assertions after View/typed/Session Close), scope-only
finalization of View/Return_Request/Comms_Request plus Close-then-Finalize,
and `Command_ID` compatibility.

A mutation check that flipped one bit of the outbound Command_ID made the
suite fail with `CommsTest request conversion mismatch`. The source was then
restored byte-for-byte.

### Resource exhaustion boundary

No safe-Ada `Resource_Exhausted` test was added for the common façade. Only
the held/gated `completion-scale` scenario produces deterministic admission
refusal, and using it from Ada would reintroduce the `dlsym` gate this task
avoids. The mapping relies on: (1) native `test_public_common_channel`
`pending` cases, which prove Session admission refusal through the public view
for all five families; (2) the façade's use of the shared
`AMS.MEL.Check_Submission`, which the existing Ada C2 bounded-admission test
exercises; and (3) the existing Ada Session-limit tests.

## ABI and bindings

No change to `native/include/ams_mel/abi.h`, `native/src/exports.map`, or any
production native source. ABI **0.1**, exactly **100** production exports,
with the symbol set identical to the starting SHA. The Rust sys and Python
ctypes raw inventories stay at 100 with no signature change, and the private
Ada imports from 032B1 are used unchanged.

## Deferred

- Safe Rust common Channel façade: not added.
- Public Python common Channel API: not added.
- Generic Enable/Disable, buffer registration, and common metadata callbacks
  remain unexposed, as in 032B1.
