# IR-CI-002 — Instrumentation lifetime completion handshakes

## Proposal and review boundary

**Policy disposition: PROPOSED_PENDING_INDEPENDENT_REVIEW.** Computing hashes,
passing this proposed checker, or passing hosted CI does not constitute independent
approval of the native test/mock transition. This is a test synchronization
correction, not a production request-semantics change. Both commits require
independent review. No merge, auto-merge, rebase, force-push, tag, release or
deployment is authorized. Documentation PR #117 and its worktree/evidence remain
unchanged; reconciling it is a separate task after independent review and merge.
The separately planned relative-package type-link correction is not imported.

Exact five-file scope:

- `native/tests/mock_provider.cpp`: dedicated Instrumentation held scenario;
- `native/tests/test_ir_instrumentation.c`: both lifetime tests and local controls;
- `scripts/check_doc_source_integrity.py`: independent atomic exact-content proposal;
- `scripts/test_doc_source_integrity.py`: frozen immutable reconstruction/regressions;
- `docs/ir-ci-002-instrumentation-completion-handshake.md`: this record.

No production implementation, ABI, Ada, vendor, workflow, generator, enhancer pin,
presentation config, manifest or published baseline change. Existing approved RF
pair, IR-stream singleton, their digests and mechanical disposition labels remain
unchanged. Native CTest inventory remains 434 tests. Post-send allocation and
worker-launch fail-safe retention cases remain on the legacy scenario at the end
of the process, after the physical-unload tests.
Independent comparison against actual starting base counts **1,341 out-of-scope
tracked paths**, with unchanged Git blobs/modes and working/index bytes.

## Refreshed identities and preserved evidence

Actual starting fetched main is
`8be05e9d0284e81a1769b2e1639a4b96b191324c`, tree
`8158378afbfb59686a396e2f758ae30545610c8a`. No equivalent branch, worktree,
evidence directory or PR existed. The correction starts on main, not unmerged
DOC-007E. Worktree `/home/zboll/git/ams-mel-ir-ci-002`, branch
`corrective/ir-ci-002-instrumentation-completion`, external evidence
`/home/zboll/git/ams-mel-ir-ci-002-evidence/r1`.

PR #117 remains open/non-draft at `d5f397425c907bf27cecc115919cb48a5c65f940`,
tree `50333c5e6885eb7d9c062e7f35284dc01b523e4b`, auto-merge off. Its clean
worktree and preserved handoff/manifest hashes were recorded before this task.
**All native files are byte-identical between starting main and PR #117**.
The timing premise therefore predates package-browser presentation.

Original failed [Bootstrap push 39097077370](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/39097077370),
[job 117585640066](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/39097077370/job/117585640066):
raw complete ZIP, job inventory, workflow metadata, actual source checkout,
compiler configuration and failing assertion are preserved externally. It shows
Clang **18.1.3 Debug**, checkout `d5f397425c907bf27cecc115919cb48a5c65f940`,
ordinary suite **434/434**, then failure in the existing parallel-4,
until-fail:50 repeat gate:

```text
test_ir_instrumentation.c:519:
ams_mel_ir_instrumentation_request_wait(request, 0, &result, NULL, 0, NULL)
    == AMS_MEL_TIMEOUT
test_ir_instrumentation.c:652: test_parent_first_close() == EXIT_SUCCESS
99% tests passed, 1 tests failed out of 434
Process completed with exit code 8
```

That hosted log **does not establish the actual returned status, provider/bridge
terminal publication, or exact thread interleaving**. Controlled evidence below
does not retroactively supply those missing observations. PR #117's failed run
was not retried, and that PR was not otherwise modified by this task.

## Controlled BEFORE mechanism

Separate immutable-main archive copies `baseline-source` and `before-source` have
separate Clang Debug builds. Unmodified baseline Instrumentation execution passes.
The diagnostic copy inserts only public observations before the original first
pending assertion: wait up to 5,000 ms for **real bridge terminal status**, verify
the complete rich report, then print a zero-poll's actual status and execute the
unchanged original `== AMS_MEL_TIMEOUT` assertion. No production instrumentation
or inferred completion from `instrumentation_completed` is required.

Actual controlled output:

```text
CONTROL bridge-terminal wait status=0
CONTROL first-zero-poll actual=0 expected-timeout=9
FAIL ... ams_mel_ir_instrumentation_request_wait(...) == AMS_MEL_TIMEOUT
```

CTest exits **8**. Positive public terminal wait means the original zero-poll
cannot run before actual bridge completion. Success is legitimately cached:
`native/src/ir_instrumentation.cpp:791–814` returns terminal state when already
published. The old mock's `instr-lifetime`/`instr-delayed` producer waits at most
40 ms, discards the wait result and records completion **before** promise
publication. There is no handshake holding it pending through the test's closes.
This source-backed controlled schedule proves the mechanism, not the original
hosted schedule or a production defect. Exact patches, build commands/caches,
sources/binaries and statuses remain external evidence.

## Dedicated held scenario and lifetime assertions

`instr-lifetime-held` preserves the original lifetime scenario's rich command
getter validation, exact rich report, registration-time synchronous metadata
callback, send-time synchronous callback, real provider promise/future, actual
bridge completion worker, disable, destruction and DSO unload.
`instr-lifetime` and `instr-delayed` retain their original behavior for all other
users, including deliberate fail-safe retention tests.

Each lifetime test creates a private `mkdtemp` control directory with exclusive
log/marker creation, installs the path before send, and uses no observer dlopen,
strong channel observer, or request owner beyond the real contract. The producer
captures its own path string before launch, refuses existing entered/closed/release
markers, exclusively acknowledges `entered`, and cannot fulfil success before
the controller creates `release`. It also requires the controller's post-close
`closed` marker. A missing setting fails submission; destructor activity cannot
silently release the held future. The held-scenario destructor guard aborts if it
runs while its producer is unreleased. The legacy destructor condition does not
confer held-scenario success.

Both tests bound acknowledgement/teardown waits by explicit predicates with
monotonic deadlines and diagnostics. The 10-second producer watchdog **aborts**,
never succeeds. Poll pauses are not correctness oracles. Ordinary ownership of the
fresh control directory prevents stale/cross-test releases; this is not an
adversarial filesystem sandbox. On assertion failure the process fails, preserving
its files for diagnosis; it never reports cleanup as success or detaches a
successful orphan controller. On success files and environment survive until
channel destruction has joined the producer and `library_unloaded` is observed.

`test_parent_first_close` now establishes:

1. Rich held scenario, metadata registration and submission, producer acknowledgement;
2. no release/completion/physical teardown and the original zero-poll timeout;
3. registration/send callbacks returned synchronously;
4. metadata, channel, session closes in original order, consumed handles NULL,
   no premature completion, disable, channel destruction or unload;
5. another zero-poll timeout **after all those closes**, before explicit release;
6. controller closed/release events and exclusive markers;
7. real successful request wait, all rich report fields, cached zero-poll success
   and rich report recheck, consumed request handle;
8. bounded actual teardown/unload, completion-before-destruction-before-unload.

`test_request_close_while_pending` establishes producer acknowledgement and the
original pending timeout, closes/consumes the request while still held, closes/
consumes channel and session, verifies no completion/teardown/unload, and only then
releases. Actual completion/destruction/unload still follow in order, preserving
request-close-is-not-cancellation. The former 200/300 ms teardown sleeps are gone
from these two tests, replaced by bounded event predicates.

Controller log order is `public_closes_completed` < `controller_released` <
`instrumentation_completed` < `instrumentation_channel_destroyed` <
`library_unloaded`; disable is required too. The completion marker still precedes
promise publication and **is not described as bridge terminal publication**.
Parent-first result/cached waits supply that independent terminal observation.
For the consumed-request case, actual deferred physical teardown/unload supplies
the lifetime consequence, not an invented public result after close.

## AFTER diagnostics and decisive negative controls

Uninstrumented candidate execution passes. Separate compiled diagnostic copies
retain the real provider/bridge path and all relevant lifetime assertions:

- Acknowledged hold is extended by 120 ms (beyond the former 40 ms), with absent
  release and event predicates still the oracle. Actual held zero-polls print
  **9 / TIMEOUT**, intended closes finish held, explicit release follows, and
  actual lifetime logs show completion, disable/detach/destruction and unload.
- Completion before release: provider mutation bypasses gate; held-state/pending
  assertions reject, exit 1.
- Controller release before intended closes: retained post-close hold check rejects,
  exit 1. The controller's early release cannot pass as valid sequencing.
- Missing release: shortened diagnostic watchdog aborts, SIGABRT, not success.
- Premature channel-destruction/unload markers: held-state predicates reject, exit 1.
- Actual premature held-scenario destructor: destructor guard aborts, SIGABRT.
- Wrong rich report: provider returns changed command ID; report fidelity rejects,
  exit 1.
- Wrong cached terminal status: diagnostic-only isolated bridge object returns
  timeout at the cached-success poll; the retained `== AMS_MEL_OK` assertion rejects,
  exit 1. No such instrumentation is in the candidate implementation.
- Existing entered/closed/release markers: held send rejects, exit 1 each.
- Release in another unique test directory: original captured producer remains
  held; diagnostic watchdog aborts. Missing environment also rejects.

No standalone-helper-only result substitutes for the real provider/bridge runs.
The test-only diagnostics remaining in the final pair are failure messages,
producer/destructor watchdogs and controller lifetime-event recording, not
production failpoints or scheduling overrides. Exact diagnostic patches,
controllers/commands, runtime identities, binary hashes, outputs and exits remain
external evidence. Diagnostics are separated from uninstrumented qualification.

## Frozen native proposal and exact atomic policy

Immutable DOC-001 foundation stays `42648a92eb0cc84b2980d16db6028992c0494ba1`.
Originals recomputed from it also equal starting main exactly:

| File | Original SHA-256 | Frozen proposed SHA-256 |
|---|---|---|
| `native/tests/mock_provider.cpp` | `8c41c152f18eb9e96c1219d7c4073324719b5364d3a8fa458a59f396dda51666` | `5c816a106bd310443620921979a99ccda6ad830102ac9c65a63766ff4fd95b37` |
| `native/tests/test_ir_instrumentation.c` | `49deb479383a43f0eba2528b67bc595618dafd7f750de990f6343184b86fa179` | `8848888d729a083149cfb6c5e42492143ffa9e71aad06123da934d544850f5f9` |

First normal native-only commit:
`6702c6c60f3731d5c39ce5fb83f97156f28d9543`, tree
`0f2b761da1fb5ef12057d3eb37b6a65a32d89d8d`. The second normal commit contains
only the policy proposal, immutable fixtures and this record. Final identities,
PR, actual current base and synthetic merge are recorded externally after commit.

`PROPOSED_INSTRUMENTATION_TRANSITION` is disjoint from existing transitions and
accepts only complete ORIGINAL or complete PROPOSED_PENDING_INDEPENDENT_REVIEW
states, identical in index and worktree. It verifies immutable foundation bytes
first; both paths must exist in real foundation/index, ordinary nonsymlink working
files, exactly regular 100644 stage-0 entries. Mixed pairs, unlisted bytes,
concealed staged mutations and complete-view disagreement fail. Both files remain
protected, preserving 1,162-file accounting. Fixed-base CLI, additions/inventory,
Ada-token/vendor protection and all historical exact-content identities remain.
No mutable approval manifest, wildcard, branch/environment exception or automatic
hash regeneration is added.

Fixture construction reads only immutable original bytes and applies frozen
literal edits from the first immutable native commit, independently checking both
whole-file proposed digests and exact checker literals. Current candidate files
do not authorize themselves. All **34 existing integrity tests/contracts** remain;
**13 new tests** cover eight crossed valid states (new pair × existing RF pair ×
IR singleton), both mixed directions in both views, concealed staged mix, complete
view disagreement, unlisted either-file/either-view, missing members, symlink/
directory working files, executable/symlink/gitlink/unmerged index, altered/
incomplete/already-corrected foundation, absent-pair minimal fixtures without
addition permission, existing RF/IR rejection, unrelated protected changes,
unauthorized additions, fixed CLI and disjoint/stable accounting.

## Local qualification and unchanged documentation

Fresh uninstrumented **Clang 19.1.7 Debug** and **GCC/G++ 14.2.0 Release** builds:
each ordinary suite **434/434**, and each existing
`ctest --parallel 4 --repeat until-fail:50 --output-on-failure` passes. No gate,
test inventory, assertion or warning policy was reduced. Local Clang differs
from hosted 18.1.3; final hosted matrix must qualify that environment independently.
Build writers use distinct trees; no mutable-tree writers overlap.

Required commands execute sequentially and their complete outcomes are external:

```sh
make test-native
make test-ada
make check
make check-ada-format
alr -n -C ada build
alr -n -C ada/tests run
```

Native compilers use absolute `/usr/bin/gcc`, `/usr/bin/g++` to avoid GNAT compiler/
C++ runtime selection drift. Tool identities: Debian Clang 19.1.7 (3+b1), GCC/G++
14.2.0-19, glibc 2.41, CMake/CTest 3.31.6, Python 3.13.5, GNAT 16.1.0,
GPRbuild distribution 26.0.1 / binary 26.0.0 (2026-04-15), Alire 2.1.1.
No Ada source edit or formatting rewrite; required format checks still execute.

Documentation discovery: **116 retained tests including 34 existing integrity
tests + 13 new integrity cases = 129 total**. Separate integrity execution is
47 cases, not additional cases in an inflated total. Existing actual-main
presentation/semantic/audit suite is **66 tests**, not DOC-007E's 100.

Same pinned GNATdoc 26.0.0 (20260415) and unchanged DOC-007D enhancer/config
generate starting-main and candidate pristine/publication trees. Every filename
and complete byte equals: **41 files, 37 HTML pages, 1,418 local type links,
zero external type links, 2,964 references**. This is main's DOC-007D output,
not PR #117's 42-file navigation publication. Coverage/warnings/anchor/source
authorities remain intact. No browser campaign is rerun: exact complete-file
equality, not an invented new browser observation, is the basis for reuse.
Independent final-head hosted artifact validation/equality is still required.

Source-integrity, staged/working views, independent exact-five-path blob/mode
comparison, 1,162 protected files, 804/804 vendor checksums, baseline delta,
whitespace and final-newline checks must all pass before ready disposition.

## Related timing-risk inventory — read only

These are source-level follow-up risks, **not additional observed failures in
PR #117's job**, and are not modified here:

| Native test location on starting main | Scenario / mock behavior |
|---|---|
| `test_ir_c2.c:269–271` | Comms delayed, mock `2036–2045`: 40 ms wait, result discarded |
| `test_ir_c2.c:284–286` | KeepAlive delayed, mock `2001–2009`: same bounded delay |
| `test_ir_c2.c:917–920` | Mode/Operate delayed, mock `2228–2241`: 40 ms or release |
| `test_ir_c2.c:1186–1189` | BIT delayed, mock `2093–2103`: 40 ms or release |
| `test_ir_navigation.c:386–390` | navigation-lifetime, mock `1245–1257`: 40 ms or release |

Track Update pending tests explicitly supply a barrier and release it only after
their pending/close assertions (`test_ir_track_update.c:508–569`, mock original
`3394–3413`). They are not classified as the same delay-only premise merely for
using Wait(0). Their helper tolerates missing paths and has its own historical
limits; this task borrows the external-control idea, not its absent-setting
success fallback or fixed teardown sleeps. Wider correction needs separate review.

## Preserved failed development attempts and final-head boundary

- An initial search used plural `test_ir_track_updates.c`; actual singular file
  was read. No source was invented from a missing filename.
- First diagnostic runner reached successful controls, then stopped because a
  replacement pattern matched both legacy and held promise publication. The
  revised isolated runner targets the unique held release point; earlier evidence
  remains. A later cached-status diagnostic link used the wrong library directory;
  the corrected link command and actual failing cached assertion are retained.
- First new integrity run failed fixture setup because reused IR setup expected
  class identity attributes; new fixture class now declares those existing
  immutable attributes. No old test or checker contract was weakened.
- A hosted compiler-log assertion initially searched `clang version` rather than
  CMake's actual `Clang 18.1.3` identity line; corrected extraction preserves raw log.

Final-head runs must include Bootstrap push, Bootstrap PR and Documentation PR,
all attempts/conclusions/raw checkout identities, native matrix ordinary/repeats,
synthetic ordered parents/tree and artifact equality. A pending or failed gate
does not count as passed; any new failure is preserved/diagnosed rather than
silently expanding this five-file proposal or rerunning to green. External final
handoff records actual outcomes, complete identities and evidence-manifest hash.
Stop for independent review; computed acceptance is not independent approval.
