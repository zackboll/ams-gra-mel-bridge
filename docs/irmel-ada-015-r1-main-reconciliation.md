# IRMEL-ADA-015-R1 — Image control-only reconciliation with main

## Source identities and merge

This corrects the existing PR #184 on
`feature/irmel-ada-015-image-control-only`. The initial worktree was clean;
unrelated branches and worktrees were left in place. Fetching both requested
remote branches found the feature still at
`08bf40d17895daa170e7acccc19d216610781ce4`, but main had advanced to
`b22ddb64f5deaf9994fcd9478e56dbdbf7e06148`. That main includes the merged
Control discovery PR #183 (`3bc834fee062ae0fa812cb2d7d886a6157ecfd11`), the
newline correction in PR #190, and Rust ConfigSet PR #191. The task's observed
main was therefore not used as a substitute for the live target.

Ordinary merge commit `7c681f562b1072e1a53610264bbe3ee79411704e` has ordered
parents `08bf40d17895daa170e7acccc19d216610781ce4` and
`b22ddb64f5deaf9994fcd9478e56dbdbf7e06148`; its tree is
`531889f075570cd0336f23ad41415d6311a01e08`. The second parent is an ancestor
of the reconciled branch. No rebase, amend, reset, squash, or force push was
used. Final delivery identities belong in the commit/PR evidence rather than
embedding a document's own enclosing tree hash in its contents.

Follow-up commit `0e8278c1778ed416a6e3d6f8df966959505b0205` restores main's
historical source-identity dictionary exactly. A preparation script had
unnecessarily refreshed it while resolving the digest conflict; these hashes
describe the immutable historical baseline, not today's combined implementation.
The final comparison against main verifies that dictionary is unchanged.

## Conflict resolution and contract review

Git reported four textual conflicts:

- `docs/irmel-ada-coverage.json`: retain Scheduling TaskExecutingRep's
  qualification and the Image control-only qualification, correcting the
  latter's combined safe-operation count from 49 to 50.
- `docs/irmel-ada-coverage.md`: regenerate from reconciled authoritative JSON
  with the existing checker, not by selecting one parent's generated text.
- `scripts/check_ir_api_coverage.py`: replace the competing reviewed-binding
  digests with the canonical, sorted-JSON digest of the combined review data.
  The checker rejected an initial file-byte digest; that preparation error
  was corrected without relaxing the check. Historical `source_sha256`
  baseline identities remain unchanged; they are not current-source locks.
- `scripts/test_ir_api_coverage.py`: preserve discovery's 50-operation check,
  its partial generic attachment/detachment assertions, the Image attachment
  evidence regression, and all incoming Scheduling regression controls.

Automatic textual merges were reviewed as contracts. Native declarations,
the export map and CMake retain both discovery and Scheduling. Private Ada
imports and the smoke driver retain TaskExecutingRep and discovery while
adding the Image control-only entry points and tests. Mock-provider scenarios
remain separate: Image-specific zero layouts and unsolicited callbacks do not
replace discovery or Scheduling behavior. The complete safe Rust and Python
mainline implementations are retained; only their private native mirrors and
ABI tests change for this correction. Protected CI policies, workflows,
upstream declarations and the historical `docs/coverage.md` are untouched.

## ABI correction

Freshly fetched main has **305** production exports. The reconciled map has
**306**, with no duplicates or removals. The exact set difference is
`ams_mel_ir_image_control_open`, under the existing `AMS_MEL_0.1` version.
All 313 pre-existing C struct definitions are unchanged; the only added C
record is `ams_mel_ir_image_control_config_v1`.

The private ctypes record and Rust `repr(C)` record contain, in order,
`uint32_t channel_type`, channel UCI identifier, platform UCI identifier and
component location. There is no acquisition count, byte size or queue capacity.
The function borrows a const Session and const configuration, writes through
an Image-stream handle pointer, and uses the existing diagnostic buffer,
capacity, required-size pointer and signed 32-bit status convention. Rust
preserves constness explicitly; ctypes has no const-qualified pointer type,
so its borrowed/read-only input convention is documented privately.

Both language ABI suites require exactly 306 names and complete equality with
the production map, retaining duplicate checks and all existing tests. The new
shared C11 probe type-checks every field and the exact function pointer type,
including constness and output indirection. Python and Rust independently
compare size, alignment and every field offset against its compiled output.
Rust also compile-checks the extern signature, individual field types, and
identity-only prefix size, alignment and offsets.
There is no new public Python or safe Rust attachment API.

## Image semantics and coverage delta

`Create_Image_Control_Config` remains identity/platform/location-only;
`Open_Image_Control` reuses the limited Image owner. A valid Image channel
with zero width, height, depth, pitch, buffer size, image size and bands can
attach. Capabilities, the four required metadata registrations, navigation,
KeepAlive and CommsTest reuse the existing engines. `Start` enables the channel
without resolving or calling `getBuffer`, allocating a sample pool, initializing
buffers, or registering acquisition buffers. Empty pre-attachment callback
ownership slots are not sensor buffers.

Both native receive paths reject with
`image receive unavailable on control-only attachment`; Ada owned frames and
leases therefore cannot publish pixels. Explicit navigation `InvalidState`
remains a rejected result with the provider's description, not a bridge
exception. Unsolicited callbacks are rejected without sample getters and use
the serialized, reentry-safe release executor. Pending requests retain their
provider graph. Failed/throwing releases, attach exceptions after listener
exposure and uncertain detach preserve conservative graph/code retention.
Neither disable nor timeout is treated as callback quiescence or cancellation.

The ordinary Mono8 attachment still requires a configured supported format;
its strict receive validation and buffer/snapshot/lease ownership are unchanged.
The control-only C test explicitly rejects an acquisition open on the zero-layout
provider. No frames or metadata are manufactured by the binding.

The reviewed binding ceiling and mutable coverage manifest agree exactly.
There are **50 safe Ada upstream operations**, unchanged from current main:
this is an attachment profile, not another upstream overload. All mainline
operation, type and inherited-exposure IDs and statuses are preserved. Six
existing operation evidence entries gain Image control-only evidence; no type
or inherited-exposure classification changes. The generated report retains
Control discovery and both Scheduling metadata slices. The coverage checker
reports 127 headers and 148 operations; its combined regression suite has
60 controls.

## Qualification

Commands run from `/home/zboll/git/ams-gra-mel-bridge`, sequentially through
`alr -C ada/tests exec -- bash /tmp/irmel015r1/qualify.sh`. The pinned environment
selects GNAT native 16.1.0, GPRbuild crate 26.0.1 and GNATformat 26.0.0. Native
C/C++ uses GNU 14.2.0; Rust uses rustc/cargo 1.98.1. No two commands rebuild the
same artifacts concurrently. Earlier feature qualification is historical
evidence only, not qualification of this combined source.

| Command | Combined-source result |
| --- | --- |
| `make format-ada` | Passed; no source formatting delta. |
| `make check-ada-format` | Passed. |
| `make test-native` | Passed, 558/558. Current main has 557; CMake differs only by the new control-only contract. |
| `make test-python` | Passed, 517/517 plus compileall; includes 59 ABI tests. |
| `make test-rust` | Passed, 441 Rust tests/doctests across the workspace; all native prerequisite tests passed too. |
| `make test-ada` | Passed, complete smoke/focused suite, including discovery, Image control-only, legacy Mono8 and both Scheduling metadata slices. |
| `make check-ir-api-coverage` | Passed, 60 regression controls; 127 headers, 148 operations, 50 safe Ada operations. |

The additional Release build uses a separate temporary tree, after the ordinary
sequence, with the caller's `LD_RUN_PATH` and `LD_LIBRARY_PATH` unset. It retains
`-DNDEBUG` and the repository's warnings-as-errors policy. Its commands are:

```sh
cmake -S native -B /tmp/irmel015r1/release \
  -DAMS_MEL_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/irmel015r1/release --parallel 2 --target \
  test_ir_image_control test_ir_stream test_ir_image_metadata test_ir_navigation \
  test_ir_discovery test_ir_discovery_allocation test_ir_scheduling_layout \
  test_ir_scheduling_execute_task_ack_layout test_ir_scheduling_task_executing_layout \
  test_ir_scheduling_foundation test_ir_scheduling_execute_task_ack \
  test_ir_scheduling_task_executing
ctest --test-dir /tmp/irmel015r1/release --output-on-failure \
  -R '^(ir_image_control_contract|ir_stream_contract|ir_image_metadata_contract|ir_navigation_contract|test_ir_discovery|test_ir_discovery_allocation|ir_scheduling_.*)$'
ctest --test-dir /tmp/irmel015r1/release --output-on-failure \
  -R '^ir_image_control_contract$' --repeat until-fail:50
```

Both GCC and Clang passed explicit
`-std=c11 -Wall -Wextra -Wpedantic -Werror -fsyntax-only` checks of the new
control-only contract and layout probe. The compiled probe reports size 128,
alignment 8 and offsets 0, 8, 40, 72 on this LP64 host. Independent source and
production-ELF audits confirm exact equality of all 306 C/Python/Rust/map/DSO
symbols, each production function exported as `@@AMS_MEL_0.1`, and no changed
pre-existing C records or coverage classifications.

The first combined native run passed 558/558. Its following Python run caught
one incomplete update: the explicit expected-name tuple still omitted Image
control open even though the binding, new signature/layout test, inventory count
and export-equality checks were present. The correction is recorded in the
branch history as "Include Image control open in explicit Python ABI snapshot."
The assertion was retained and its tuple corrected. The focused ABI suite then
passed 59/59, and the complete qualification sequence was restarted. The
final-newline gate also caught missing EOF newlines in the newly added probe
and this report; both were corrected.

## Limitations

Qualification uses separate mock-provider DSOs. No minimal-example provider,
real sensor, hardware, Squall transport or private service integration is
claimed. Provider C++ ABI compatibility and destruction-time callback safety
remain prerequisites. Uncertain cleanup may intentionally retain memory and
provider code until process exit; it is not reported as successful cleanup.
Lifecycle calls remain externally serialized. StackedImage, general Buffer
support and new acquisition formats remain outside this correction.

Hosted Bootstrap must qualify the applicable PR merge result; documentation
explicitly checks out the PR head. A successful head-only workflow is not
merge-result qualification. PR #184 is to remain open, non-draft and unmerged,
with auto-merge disabled, for independent review.
