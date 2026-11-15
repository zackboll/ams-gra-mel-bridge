# IR-COMMS-001-R1 — canonical foundation reconciliation and C2 reentry

## Immutable inputs and scope

- Freshly fetched base: `417f4008ff480c68ce8a63e887e70e989dc744b9`;
  tree `d1590eeff5b0e8ca2e63bad6c5545a8ec8c6d2af`.
- PR #202 is merged at `e73830d55af4b060f1c8bab8caadf567042b172e`.
- PR #204 remains open/unmerged at `0cde3ca3935de32b2348a29b32cb3817ae8dce0e`;
  tree `93987fc362f98693e7803fd0f46959d056fb5401`. Its branch, worktree and PR
  are not changed by this correction.
- Dedicated branch `fix/ir-comms-001-r1-c2-reentry`, worktree
  `/home/zboll/git/ams-ir-comms-001-r1`. Final HEAD/tree belong in the PR evidence
  rather than a self-referential commit.

The canonical **seven-argument** common Open, including the registration-result
output, is frozen. C2 continues to use its dedicated metadata owner and independent
CommsTest one-shot; its three base registrations are unchanged. There is no second
queue, request engine, registry, callback cancellation scheme or ownership model.
No ABI header, export map, raw binding, Ada source, coverage classification or
vendored source changes are needed. No safe-language functionality is added.

## Audit method and source keys

This is a behavior comparison, not subtraction of CTest counts. PR #204 reports
93 scenarios (18 parameterizations across five destinations/modes plus three
special cases); #202 reports 69 common receiver scenarios, plus existing C2 and
Ada tests. Fresh base CTest has 656 entries. Counts are not interchangeable.

Every path below is relative to the immutable repository revision specified by
the key; the working checkout root is the absolute path above.

- **P**: #204 `native/tests/test_ir_common_comms_metadata.c`: `exercise`
  lines 217–325, `registration_reentry` 132–145, `inflight` 155–184,
  `held` 187–215, `collision` 328–346, `routed` 349–387,
  `unsupported` 388–416; strict-C signatures 14–25.
- **M**: base `native/tests/test_channel_comms_metadata.c`: argument handling
  93–154, registration 164–173, cleanup failures 177–191, sync/null/overflow
  192–201, async/typed Close 202–221, direct/routed/future/held/saturation
  223–267, final teardown 268–278. CMake 1209–1217 enumerates four modes.
- **C**: base `native/tests/test_ir_c2.c`: `test_common_pre_enable` 100–137,
  `run_comms_registration_failure` 223–236, rich metadata 394–462,
  overflow/malformed 464–500, receiver-first Close 540–556,
  allocation failure 558–581, nonquiescing disable 583–639.
- **A**: base `ada/tests/src/ams_mel_ir_comms_metadata_tests.adb`: `Parity`
  checks eight exact callbacks and independently eight futures over four
  destinations; `Cases` tests future-only and other error/lifecycle scenarios.
- **Q**: base `native/src/channel_comms_metadata.cpp`: `receive`, `deactivate`,
  `drain`, public Receive/GetCounters/Close; `native/src/common_channel_requests.cpp`
  lines 244–304: weak view, temporary family claim, canonical seven-argument Open.
- **R**: corrective `native/tests/test_c2_comms_registration.c` and CMake
  `c2_comms_registration_*`, sixteen opt-in scenarios. Only dedicated C2 APIs.
- **T**: corrective additions to M: `enabled`, `expired`, `concurrent`,
  `owner-first`, argument/preparation checks shared by all common scenarios.

In the matrix, **6?** means whether the *specific behavior* requires #204's
rejected ABI, not merely whether its original test called that signature.
**C2?** describes dedicated C2 API evidence on main or in this correction;
#204 itself uses the rejected common C2 owner except its collision test.

## Differential behavior matrix

| #204 scenario/assertion | Mainline evidence | 6? | C2? | Finding and disposition |
|---|---|---|---|---|
| P signatures/layout: two uint32 fields, request offset 4, three uint64 counters | M 17–31; unchanged public records and raw ABI probes | Yes for Open's six arguments only | C event uses existing tagged record | **Intentionally unsupported** signature; retain seven-argument probes and explicit registration output, not P declarations. |
| P basic: weak view does not own graph; exact high-bit IDs; no fabricated identity | M parity/direct and routed loop 234–265; Q weak adapters; C pre-enable; A Parity | No | Yes | **Already covered**; no queue or identity changes. |
| P sync/overflow: synchronous callbacks, null report counted, FIFO accepts first two and drops incoming | M sync, null, overflow 192–201; mock `MockCommonComms::install`; C overflow/malformed | No | Queue policy via C, exact CommsTest via R | **Already covered**, with fuller mainline refusal semantics. |
| P zero capacity, NULL output/diagnostic, SIZE_MAX preparation failure; no consumed one-shot | M checks zero capacity; Q allocates before provider entry, but not all P assertions tested | No | C has its own input contract | **Port test**: T checks NULL output/registration, malformed diagnostic, SIZE_MAX INTERNAL_ERROR and zero provider registrations, then successful Open. |
| P expired: typed owner closed, weak view cannot register | Q rejects expired/lifecycle claim; M lacked receiver-specific assertion | No | Dedicated owner remains distinct | **Port test**: T expired, checks no provider entry, across all four modes. |
| P enabled: register after Enable/Start | M disable-fail registers after enable, but only with failure teardown | No | C pre-enable; R close-enabled adds enabled Close reentry | **Port test**: T enabled ordinary path, all four modes. |
| P fail/throw: retained callback after registration failure; one-shot never retried | M refuse/fail/partial/throw 164–173 and destructor assertion 273–275; C retained-fail | No | Yes; R adds exception/refusal plus deferred cleanup combinations | **Already covered** for common owner; **corrective tests** for C2 error/reentry interactions. |
| P reentry: registration synchronously closes view and typed owner | M register-reentry 119–124, 174–176 and mock install 2008–2014 | No | Not on base | **Corrective implementation + R**: demonstrated dedicated C2 deadlock, not a reason to enable common C2 registration. |
| P collision: common C2 and old C2 consume shared one-shot in either order | M 229–233 rejects common C2, then dedicated registration; C 114–115 repeats dedicated registration | Yes, common C2 owner itself rejected | Yes via dedicated API only | **Intentionally unsupported** dual-owner collision; R tests recursive/repeated dedicated one-shot and at-most-once provider entry. |
| P inflight: held callback versus typed Close, destructor joins, unload delayed | M async-typed 202–221, probe before queue lock, producer joined in destructor; C nonquiescing-disable | No | C tests shared C2 metadata mechanism | **Already covered**; R additionally proves synchronous registration claim defers detach without a test DSO pin. |
| P held: callback delivery precedes held future; typed/receiver/Session Close cannot end pending operation | M held 241–251 uses existing completion release family; C common held completion tests; A Parity | No | C completion claims, A | **Already covered**; no callback-to-future synthesis. |
| P no-event: successful future does not manufacture callback | M future-only 240, 258–265; A negative future-only | No | A uses dedicated C2 | **Already covered**; keep mock distinction. |
| P direct versus C2-routed requests reach only selected destination | M loop uses direct then C2-routed requests, exact reports and empty C2 queue for other destinations; A all-four parity | No | Yes, A routes to C2 itself | **Already covered**; do not infer channel ID from report. |
| P concurrent: eight simultaneous submissions yield eight received, six dropped, two intact reports | M async covers a held callback but not competing common producers | No | Existing shared C2 queue locking; not common API | **Port test**: T concurrent, joins eight workers, exact correlated IDs and counter totals; no order imposed on competing producers. |
| P owner-first: receiver Close before typed Close; retained callback safe | M async closes receiver with callback entered; C receiver-first test | No | Yes | **Port test**: T ordinary owner-first teardown extends common-owner coverage beyond in-flight case; R invokes late C2 callback after receiver Close. |
| P drain: accepted reports drain after typed Close, then stopped | M sync explicitly closes typed then receives accepted report and stopped | No | C rich metadata draining | **Already covered**; shared queue semantics unchanged. |
| P saturate: received/drop counters never wrap | M saturation 253–257 seeds all three at MAX-1, exercises null and overflow, checks UINT64_MAX | No | C uses same saturating increment for its counters | **Already covered**, mainline additionally checks malformed saturation. |
| P wait-close: indefinite Receive awakened by typed Close (or Image Stop) | Q Receive waits on size/inactive predicate; deactivate notifies; M async-typed establishes stopped; C nonquiescing cleanup | No | C ready notification | **Already covered at implementation boundary**, not an identical blocked-wait test. Do not import P's global wait-entry probe/queue replacement merely to reproduce its scheduling shape. No new cancellation or Stop promise. |
| P poison: injected queue-copy exception yields failed receiver | P injects into its alternative queue; canonical Q copies only two scalars into a preallocated ring, catches failures and deactivates | Depends on rejected implementation hook, not ABI | C allocation-failure 558–581 | **Intentionally unsupported** artificial poison hook; canonical C2 allocation failure already tested. No per-report allocation to fail in common ring. |
| P detach-fail: failed detach retains graph; Image retry retains sticky error | M detach-fail 177–191 asserts failed receiver, no unload, retained typed owner, sticky Image result on retry | No | C detach lifecycle tests | **Already covered**; R extends to orphaned deferred C2 cleanup, graph permanently retained on uncertain detach. |
| P unsupported: Instrumentation/Track/Scheduling reject common receiver | Q `CommonChannelAccess` has no metadata adapter for these families; no provider entry | No; exact status differs | Common C2 deliberately rejected | **Intentionally unsupported** families, not missing functionality. Do not port P's INITIALIZATION_FAILED expectation; canonical unavailable status is PROVIDER_FAILED. |
| P Close idempotence, timeout/poll, diagnostics and output unchanged on timeout | M timeout 223–225, null-handle checks, double Close 271; Q only writes report when size nonzero | No | C repeated Close and timeouts | **Already covered** by tests/source; retain canonical statuses, not P's duplicate-Open INVALID_ARGUMENT contract. |

## Mock and lifecycle comparison

#204's provider introduced a second `comms-meta-*` scenario family, router and
queue-probe API (`ams_mel_test_comms_metadata`, operations 0–7: barriers, poison,
saturation). Main's `common-meta-*` fixture already has the required destination
router, partial-install destructor callbacks, synchronous install hook,
condition-variable callback barrier and saturation probe
`ams_mel_test_comms_metadata_probe`. Main's queue is a preallocated ring and uses
family lifecycle adapters with temporary claims; its weak views remain weak.
Its dedicated C2 metadata is a tagged, separately owned existing queue, not an
instance of the common receiver. These are intentional API boundaries.

Only the opt-in `c2-comms-r1-*` provider scenarios are new. They reuse
`mock_comms_reentry`, invoke exact full-width callbacks before and after reentry,
retain the callback even on refusal/throw, invoke it again during destruction,
and optionally fail the existing detach path. They do not alter ordinary mocks.
No new facade test export is added. Production and test libraries remain distinct.

## Demonstrated defect and correction

The baseline `ams_mel_ir_c2_metadata_register_comms_test` at lines 1811–1847
held `ChannelState::mutex` across `registerMetadataCallback`. Provider reentry
into either duplicate registration or typed Close attempted the same mutex.

Actual negative controls, using the new mock/test with **unchanged baseline
production DSO** selected by `LD_LIBRARY_PATH`:

```text
open:  TIMEOUT after 3 seconds; child killed and reaped
close: TIMEOUT after 3 seconds; child killed and reaped
stderr: provider registration entered; reentering C2 lifecycle
```

Executed with Python `subprocess.run(..., timeout=3, capture_output=True)`;
TimeoutExpired kills and reaps the child. The same two controls also timed out
against the unchanged baseline test-enabled DSO before correction. These are
actual reproductions, not theoretical mutex inspection. Baseline `make
test-native` passed **656/656**; baseline production DSO has exactly **315**
versioned functions. The sixteen common-receiver port cases all passed against
the unchanged baseline production DSO: these are missing tests, not defects.

The correction prepares callback storage first, then under the family mutex
validates lifecycle, consumes the independent one-shot and reserves existing
request accounting while capturing a strong C2 reference. Provider entry runs
unlocked. Reentrant Close logically closes/releases the public owner, but
physical cleanup is deferred until registration exits. The temporary provider
reference is reset before `CommonRequestClaim::finish` invokes existing
`finish_c2_request`; deferred cleanup failures cannot replace registration
refusal/exception diagnostics. Unknown exceptions retain the original mapping.
Uncertain detach follows existing allocation-free graph retention. No attempt
after provider entry can retry registration.

R's sixteen scenarios check both exact synchronous reports, event kind, recursive
one-shot, success/Fail/NotSupported/std exception/unknown exception, enabled C2,
deferred typed/Session Close, receiver-first Close, exactly one detach/destructor,
no request/future synthesis, late callback before destruction before unload,
and uncertain-detach retention. The test pin is dropped **before** registration.
Each CTest has a ten-second watchdog and active CHECKs in Release.

## Qualification record

Initial focused correction: **16/16** passed. After test porting: combined
`ctest --test-dir native/build-tests -R
'^(comms_metadata_|c2_comms_registration_)' --output-on-failure` passed
**101/101**. Both C translation units use C11 with extensions disabled and the
repository warning-as-error policy.

Final sequential full qualification is pending. Earlier intermediate-source
native **672/672**, Python **584**, Rust **503 passed / 9 existing ignored**,
and policy passed before the four common-receiver test ports. That intermediate
driver was stopped during coverage checks to avoid overlapping final runs;
the interrupted coverage invocation is not a pass.

ABI evidence: base production symbol names/versions saved independently;
`native/include/ams_mel/abi.h`, `native/src/exports.map`, Python ctypes, Rust sys
and Ada imports remain byte-identical to base. Final production comparison and
Ada compilation/execution will be recorded after completion. This preserves
function signatures and all record layouts by source identity, not symbol names
alone. There is no six-argument production Open declaration.

## Limits and delivery disposition

Mock-provider qualification only. No hardware, real-provider callback lifetime,
transport fidelity, provider-private contract, ABI compatibility across arbitrary
C++ toolchains or formal proof is claimed. Upstream has no unregister or
disable-quiescence guarantee. Permanent graph retention on uncertain detach is
deliberate, not leak-free cleanup. The three base C2 registrations retain their
existing implementation; this correction qualifies only CommsTest registration
reentry, not arbitrary reentry through every provider method.

Not yet READY_FOR_INDEPENDENT_REVIEW: final-source local/hosted qualification and
independent review remain pending. After corrective review/merge, recommend
closing #204 as superseded rather than merging its duplicate foundation. Do not
close #204 automatically.
