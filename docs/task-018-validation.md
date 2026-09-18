# Task 018 validation — required IR C2 metadata for Ada

## Architecture and scope

Task 018 adds the required C2-specific BIT_Configuration, CommandStatus, and
BIT_Status callbacks to native C and safe Ada. Provider callbacks validate and
deep-copy complete values into one caller-sized bounded FIFO queue. Overflow is
DROP-INCOMING. Provider callback threads never invoke Ada application code.

The 0.1 C ABI grows from 22 to 28 functions: metadata open, receive, counters,
close, event view, and event close. Event views borrow immutable pointers only
from an adapter-owned event snapshot; that snapshot is independent of metadata,
C2, Session, and provider unload. Safe Ada copies all values—including nested
BIT items, UCI IDs, fault data, components, and ambiguity groups—before closing
the native event.

Upstream has no callback unregister. Registration order is BIT_Configuration,
CommandStatus, BIT_Status. Queue/callback state exists before registration and is
retained by C2 `ChannelState` through partial registration failure or public
metadata close. Provider C2 channel destruction is the conservative quiescence
boundary; only afterward does cleanup wait for the adapter's in-flight callback
count, mark metadata stopped, and wake consumers.

## Deterministic validation

The mock provider supplies rich ordered configuration/status values, all four
tested CommandState values, high-bit IDs, long UTF-8 descriptions, malformed
root/nested values, capacity-two overflow, callbacks after public close, partial
registration failure, callback allocation failure, and a deterministic
non-quiescing-disable scenario. The latter uses test-only entry/release marker
files: the adapter creates the entry marker only after its in-flight callback
guard is active; provider `disable()` waits for that marker, records that it is
returning with the callback active; provider channel destruction releases and
joins the callback thread before returning. Its observed strict log
order is `metadata_callback_entered`,
`disable_returned_with_metadata_callback_active`,
`metadata_callback_returned`, `c2_channel_destroyed`, `library_unloaded`.

The callback boundary now encloses guard construction, counter locking,
provider value access/deep copy, queue locking/insertion, and notification in
one exception barrier. `MetadataState::fail()` is itself non-throwing and catches
failure-state mutex/notification exceptions. `CallbackGuard` destruction is
explicitly non-throwing. The deterministic `command-allocation` failpoint still
proves that an already queued valid event drains before receive reports
`AMS_MEL_PROVIDER_FAILED`; malformed provider values remain non-terminal drops.

Partial-registration validation runs in an isolated child so DSO teardown is
observable and establishes `retained_metadata_callback_invoked` before
`c2_channel_destroyed` before `library_unloaded`. C tests also retain a raw event
through metadata/C2/Session/provider teardown. Ada tests verify wholly owned
values and report a distinct required C2 metadata contract group.

## Validation results

- `make test-native`: 6/6 CTest targets passed.
- `ctest --test-dir native/build --repeat until-fail:50 --output-on-failure`:
  all six targets passed 50 consecutive executions (47.03 seconds).
- `alr -C ada build` and `alr -C ada/tests run`: passed; the executable reports
  a distinct `PASS: Ada required IR C2 metadata contract` group.
- `make test-rust`: existing 31 safe tests, one complete raw ABI drift test, and
  doc tests passed. No safe Rust metadata API was added.
- `make test-python`: existing 52 safe/private tests passed. No public Python
  metadata API was added.
- GCC 14.2 Debug and Release warning-as-error builds each passed 6/6 tests.
  Clang was not available locally.
- `alr -C ada exec -- make -C /home/zboll/git/ams-mel check` passed native,
  Ada, whitespace, and newline gates. Plain `make check` passed native 6/6 and
  then stopped because GNAT/GPRbuild is absent from ordinary `PATH`.
- `nm` reports exactly 28 versioned `AMS_MEL_0.1` exports.

## Real-provider evidence

Pinned Squall currently emits empty default BIT_Configuration/BIT_Status values;
this is recorded behavior, not a generic expectation. The correction's Ada-only
and final all-language runs on 2026-09-17 both used ports
21203/21318/21315/21316 with Squall
`b1015728f904c799fa0c07489fce48e78f67845f`, podman 5.4.2, and extracted
`libsquall_ir_mel.so` built with GCC 14.3.1. Ada observed both defaults, seven
correlated command status events (accepted and rejected with complete
reasons/descriptions), the accepted BIT status event, metadata counters 10
received/0 dropped/0 malformed, and three 320x200 Mono8 frames. C, Ada, Rust,
and Python each printed PASS, followed by the combined all-language PASS line;
exit status was zero. The separate Ada-only run also printed PASS with the same
metadata counters and frame results.

Common inherited Channel services, optional/conditional C2 commands, image
metadata/commands, Scheduling, Track, Health/Status, Instrumentation,
StackedImage, RF, and safe Rust/Python metadata APIs remain unsupported.
