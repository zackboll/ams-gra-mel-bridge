# IR-COMMS-001 — native inherited CommsTest metadata

## Source and preflight

Freshly fetched base: `1d7cf8a11ee6fc154fee2b577aa7296b5a1fbd0a`, tree
`2866a78de690e24c38065f513c08dcf37b609793`. PR #199 (PY-MEL-014) is merged
at this commit. Open PR inventory at preparation: #200 Rust Control discovery,
#201 Rustdoc foundation; neither advertises native Channel metadata work.
Dedicated branch/worktree: `feature/ir-comms-001-native-callback`.

During qualification, PR #202 appeared (head
`3c8b25579cfa5d54ae4e24b568c26c729a98e899`), independently introducing an
overlapping native foundation as part of IRMEL-ADA-017. It uses these same four
symbol names but adds an Open registration-result argument and rejects C2.
This task retains its specified four-family, six-argument Open proposal. These
branches are **not ABI-interchangeable** and must not be combined mechanically;
the common foundation and Ada follow-up require explicit technical reconciliation.
No changes to PR #202 are made here. Independent review must include this overlap.

The byte-identical pinned `Channel.h` declares:

```cpp
virtual Return registerMetadataCallback(std::function<void(Channel& channel, const ChannelCommsTestRep* const)> callback) = 0;
```

Direction is provider to consumer. `ChannelCommsTestRep` has exactly two getters:
`getCommandID()` and `getRequestID()`, both uint32. There is **no channel ID** in
the report. The callback's `Channel&` identifies its originating provider object;
it is not serialized as an invented payload field. Upstream pins and immutable
Git evidence remain in `docs/reference/irmel-upstream-git-evidence.json`.

Source identities:

- IR commit `8d9224519f12b44e0b28815755c56a32a28d24a0`, tree
  `bfc72a6a2e1929db9a90c0af78328085fb0ae3ec`.
- Common commit `f6908437d8fd2f7fb69896f9eb9cfd272d10c439`, tree
  `f1f146d79f03a2ca0a59412aaf3d2c6d7dfba40f`.
- Channel declaration blob `621238ed2d3512ac92d13d0558587ef875f51414`, SHA-256
  `f8c6eeb4da2e10970dd192243f2bee9ca6918ed640b3ce5359958412db5d537e`.
- Report declaration blob `7e5661a1be32bd25613a8eb4571019bddb7f42ec`, SHA-256
  `d5302b2dfbe486aeb3d17d238677f0b4c403a92870a276c84a67ff82e719ba0b`.

## Existing contracts inspected before implementation

`CommonChannelAccess` contains weak family state and static adapters, not a
Session, Control, typed owner or library pin. Each family has independent request
accounting which protects a temporary provider reference against physical Close.
C2 and Health use Attached/Enabled/Closed/Failed states; Image uses its frame
callback mutex and Attached/Running/Stopping/Stopped/Failed states, with cleanup
deferred for requests and buffer obligations. Stacked uses Attached/Enabling/
Enabled/Closed/Failed and Stacked-only accounting. Physical channel destruction,
not successful `disable()`, is the existing callback-quiescence boundary.
Uncertain detach keeps the graph; orphaned failures use allocation-free permanent
retention. Existing metadata closures own shared queue state without a strong
family back-reference. C2 already has `comms_metadata_attempted` independent of
its three mandatory registrations.

## Implementation contract

Four additive `AMS_MEL_0.1` exports implement Open, Receive, Get_Counters and Close
on `ams_mel_ir_channel_comms_metadata`. No additional event owner is needed.
The report and counters reuse existing records, with unchanged layouts.

Supported families: C2, Health, IRSTImage (acquisition and control-only), and
StackedImage. Instrumentation, Track and Scheduling explicitly refuse callback
registration with INITIALIZATION_FAILED and an unsupported diagnostic. This is
not complete inherited-Channel coverage or real-provider qualification.

The family claims its one-shot and publishes a preallocated bounded ring before
provider entry, reserving existing family operation accounting. Provider
registration runs unlocked and may deliver synchronously. A valid provider
attempt is consumed even on failure/exception. No capability gate is inferred.
Bridge allocations precede the claim. No usable owner is published on failure.

C2's old `c2_metadata_register_comms_test` and common Open use the **same** flag:
first wins, second returns INVALID_ARGUMENT without another provider call. The
old event kind and three mandatory metadata registrations remain unchanged.
There is no broadcast into two queues.

The queue is a bounded FIFO with DROP-INCOMING overflow. Callback entry copies
the two uint32 identifiers and never retains the report pointer. Received,
queue-full and malformed counters saturate at uint64 maximum; null is malformed.
All uint32 timeout values are finite milliseconds; zero polls. Unsuccessful
Receive does not change the caller's report. Logical Stop/Close wakes receivers,
preserving accepted events; drained inactive queues return STREAM_STOPPED.
Uncertain teardown/poisoned callbacks report PROVIDER_FAILED after queued data
drains, without claiming successful shutdown.

The public owner controls consumption only. Close does not unregister, cancel a
future, close parents, or establish quiescence. Provider closures and family state
retain queue state until destruction. The queue owns no provider/family graph,
so there is no callback ownership cycle. Temporary channel references are dropped
before the accounting claim is finished. Destruction occurs with Session/library
retained; callback drains precede releasing that graph. Failed detach retains it.

CommsTest request completion and metadata delivery are independent. No callback
completes a request; no future completion manufactures metadata. No new request
engine, buffer registration, Stacked acquisition, transport, or upstream change
is included. Safe Python, Rust and Ada owners are follow-up tasks; this task only
adds raw Python/Rust mirrors and ABI probes.

## Qualification

The focused strict-C11 executable uses active `CHECK` checks (not `assert`) and
is registered once per scenario/process. Five profiles cover C2, Health, Image
control-only, StackedImage and Image acquisition. Its 93 cases cover:

- Attached and enabled/running registration; synchronous callbacks; exact
  full-width two-field copies; provider payload overwritten after callback.
- Zero and unrepresentable capacity; null arguments/diagnostic buffers; populated
  output slots; expired weak views; second attempts through independent views.
- FIFO ordering, DROP-INCOMING, null reports, saturation, concurrent callbacks,
  unchanged unsuccessful Receive output, polls, finite timeout and UINT32_MAX.
- Logical Stop while Receive waits (condition-variable observation), accepted
  reports draining after Close, parent Session closure, public owner closed first.
- Retained callback on registration failure/exception, late destructor callback,
  reentrant Close during registration, poisoned callback and detach failure.
- Callback parked in-flight while Close reaches a provider destructor barrier;
  library unload absent while parked and observed only after producer return/join.
- Held request future while metadata is already available; closing consumption
  does not cancel the future. Future-only completion produces no metadata.
- Both C2 old/new collision orders, existing C2 event-kind compatibility in the
  unchanged C2 suite, and explicit refusal on Instrumentation/Track/Scheduling.
- Explicit mock C2 routing to four concurrently attached originating channels;
  only the target queue gets that report. Routing is not inferred for providers.

No arbitrary scheduling sleeps establish correctness. Existing provider send
barriers, lifecycle logs, and test-facade condition-variable probes establish
interleavings. Test probes are absent from production exports.

Initial qualification on the fetched base:

| Command | Actual result |
| --- | --- |
| `make test-native` | PASS: 680/680, including 93 new cases |
| `make test-python` | PASS: 584 tests; native prerequisites 680/680 |
| `make test-rust` | PASS: 484 passed, 9 pre-existing ignored; native prerequisites 680/680 |
| `make test-policy` | PASS |
| `make check-ir-api-coverage` | First run failed: old Stacked missing-count expectation; updated to require exactly one native-only callback exposure, two missing buffer operations and a new seven-family scope test. Final rerun recorded below. |

The unique callback operation remains partial because only C2 has safe Ada
consumption and three other inherited families remain unsupported. Health,
Image and Stacked inherited exposure become native_only. Safe Ada unique
operation count stays 50. The reviewed binding candidate and its exact digest
are updated without weakening the integrity comparisons; independent technical
review must approve that candidate. Historical `docs/coverage.md` is unchanged.

The first focused development run passed 40/41; its Image detach-retry test
incorrectly expected OK despite the existing sticky Failed lifecycle. The test
now checks the preserved PROVIDER_FAILED result after successful physical release.
Subsequent focused runs passed 71/71, 87/87 and 92/92 while cases were added.

Further gates, final merged-source identities, release/compiler matrix evidence
and hosted CI results remain to be recorded. No independent approval or
READY_FOR_INDEPENDENT_REVIEW claim is made yet. No hardware, private provider,
transport fidelity, formal proof, or complete MEL compliance is claimed.
