# DOC-PERF-001-R2: source-qualified live mutation fixture

## Starting identity and preserved failure

PR #132 starts at `e4d38d4ef23958aae1236f1181c8e3bc267e077f`, tree
`6e80ac67006c27aad64ea346e288343a0203fe7a`, based on current fetched main
`7d5f88164bacfdc40add4b92c74cfaefbefad0a6`. Local, fetched remote and PR heads
were reconciled before edits. No other task process remained active.

Documentation run `40610613892`, live job `122566462800`, executed 72 tests
and failed two replacement-count assertions: `test_missing_required_type_link`
at original line 330 and `test_wrong_native_anchor` at line 323, both
`AssertionError: 0 != 1`. Original Bootstrap run `40610613801` passed all eight
jobs. These runs are not cancelled, relabeled, overwritten or retrospectively
qualified. Full hosted logs, all downloadable original artifacts, artifact API
identities and hashes are retained externally at
`/home/zboll/git/ams-mel-doc-perf-001-evidence/R2-live-fixture`.

## Reproduction before correction

The original `api_page()` returns the first non-index `Path.glob("*.html")`
member. The genuine AMS page
`5f9de8789b28ed02de7bf3d4f6aa6447445b50a3c927e5e3bb132a2c8efb1779.html`
contains neither a native `<h4 id=...>` declaration nor a local type-reference
link. Presenting that actual file first in a controlled enumeration makes both
unchanged original test methods raise exactly `AssertionError: 0 != 1`, before
publication verification. No publication bytes or filesystem ordering were
changed for this reproduction. The hosted input API bytes compare equal to the
local qualified publication. The artifact's API tar members are sorted, but
filesystem glob order is not guaranteed by tar order; AMS occurs at position 12
among non-index tar API pages. The original hosted log does not identify the
chosen path, so the exact hosted directory enumeration is not invented.
Recreating the actual tar creation sequence locally selected a different,
qualifying page first; it does not prove the hosted filesystem order. The
controlled unfavorable-order reproduction supplies the exact two zero counts.

The immutable CI-001 helper at
`a0900430d3b2db8ac5dd9d065eaa66aa88ed6751` is identical to PR #132's original
helper. Thus this zero-count failure reproduces independently of the performance
optimization. The tests were attempting mutations on an unsuitable fixture;
no defect in the optimized publication validator is established.

## Deterministic source-backed correction

Only `QualifiedArtifactTests.api_page()` changes in the live test module.
It builds the consumer-owned `Authority` from current Ada source and pristine
native GNATdoc input, obtains `required_additions()` before reading candidate
HTML, sorts by `occurrence_key`, and selects the first containing page.
The current source-qualified selection is
`09bd8292c9fb6f9b8644595432b39db4e5b39aba955eceb6cbca789ebcb5efc0.html`,
AMS.MEL.IR.Instrumentation. The required occurrence is
`Instrumentation_Report.Priority`'s `Instrumentation.Priority` type mark,
containing anchor
`17582c8504b21254cd4d675cdeb808a3086cc74076a02e6acba860acfe9514cf`
and target Priority anchor
`2d8778dc79db6a27e1344591d4a342292be5a998d47d19af9642d1e62d5ca632`.

The helper checks the containing section against the independent authority,
requires its exact native anchor, restores approved metadata/snippets, requires
the exact source-owned occurrence in the semantic link audit, and compares the
restored complete page with native bytes. Missing/changed authority, file,
anchor, link or semantic destination fails closed. Candidate markup does not
select the page. The original mutation regexes, replacement-count assertions,
test bodies, identifiers and expected independent rejection guards are unchanged.
All 72 live identities remain required.

## Exact inherited-source proposal: pending independent review

Mechanical acceptance is **not** independent approval of this proposal.
`documentation_ci.live_fixture_source()` reads immutable CI-001 module bytes
via Git, verifies their actual digest, and reconstructs the proposed module by
one exact old-helper/new-helper replacement. It accepts only the exact original
or the reconstructed complete successor. Candidate-controlled digest metadata
cannot authorize another edit; the new digest is checked against independently
reconstructed bytes, not used as the sole authority.

| Identity | SHA-256 |
| --- | --- |
| Original complete module | `0658a2e40b8e815e71a9de61e077b7b0a122f74f23ba385d363f80ebfc0b6164` |
| Proposed complete module | `4693fe458c1fae6dffc5d9d115e21dc9a5ff3a74380a0cd16ac22c4181444c74` |
| Original helper bytes | `eb39eadb1a65b525fce922c1c0fbb2814ec4ae6ee6d6d3e2b257955879261be4` |
| Proposed helper bytes | `5e89d2be484da2bce0673138e242798f772bdedc52b276519db672c6330fae9c` |

The existing historical WorkflowTests AST projection still executes after
normalizing the exact successor back to its immutable original. WorkflowTests
itself must match the reviewed CI-001 complete module. Every other helper,
import, class, assertion and byte is preserved. Additional edits, weakened test
assertions, missing/renamed/duplicated methods, alternate helper implementations
and mixed unauthorized source states reject. The historical
`ReviewedTransitionTests` fixture is unchanged and continues supplying original
CI-001 live-test bytes. Original CI-001, CI-001R2 and CI-002 inventory checks
remain active: 650 identities = 142 script + 508 enhanced, all original 579
plus the 71 merged CI-002 controls.

## Standalone regression controls

`docs/tools/test_live_fixture_transition.py` supplements the frozen official
inventory without adding or removing its identities. It reproduces the original
failure on the actual AMS page, exercises ascending and descending enumeration,
checks native/source/ledger ownership, runs both original mutation test methods
under both orderings, tests required-link removal and anchor corruption with
recomputed forged manifest hashes, fails closed on missing/changed fixture
authority, and tests exact-source policy rejection of unrelated helpers,
assertions, WorkflowTests, duplicate/missing tests and alternate replacements.
It also exercises the complete inventory validator with the historical fixture
and proposed successor, and verifies current/historical identities.

The first development control run failed because its glob mock omitted Python
3.13 keyword arguments and its semantic-destination negative used malformed
markup. Those diagnostics are preserved. The control now forwards glob keywords
and substitutes another well-formed actual native destination, so its expected
semantic rejection is specific rather than a generic exception escape.
The corrected development suite passed all nine controls in 32.534 seconds;
the full source-frozen qualification repeats them as mandatory standalone gates.

## Performance, output and frozen qualification

`build_enhanced_site.py` and `symbol_audit.py` remain byte-identical to reviewed
PR HEAD, retaining all invocation-local parsing/native-page/reference/hash reuse,
no cross-call acceptance cache, and all independent semantic and byte checks.
Historical three-repeat measurements remain evidence for their historical
source: navigation 479.16 → 88.27 s (81.58%), aggregate CPU 2,019.62 → 476.19 s
(76.42%), simulated six-shard critical path 479.16 → 112.03 s (76.62%). They are
not represented as measurements of this corrected final source. R2 records its
actual official workload runtime separately and checks for material regressions.

Pristine remains 40 files, fingerprint
`2ee21243c766d5183587633947f055a1602c4d744040218db13ee01a8eb33291`.
Enhanced remains 43 files, fingerprint
`2249019abba15ec3107069c18dea5e0934eab25d303524e9a5e9768b476838cb`.
No workflow, Bootstrap policy, deployment authority, output, native/Ada/binding
source, example, coverage/warning baseline or optimized module is edited.

R2 stages the complete implementation/report, records staged tree plus full
file-level SHA-256 inventory and index/worktree parity, then freezes all source,
tests and report bytes. New bounded-command logs, UTC timestamps, CPU/RSS and
results go outside the repository. The seven repository commands rerun
sequentially, including native 434/434, Ada/format/Alire and all 13 example cases.
The official inventory/scripts/seal/six-shard/aggregate must qualify all 650
identities exactly once with no non-success outcomes in local-not-deployable
mode. All standalone controls and independent source/native/publication checks
remain mandatory. Any source change requires a new freeze and qualification.

Actual completion, frozen/committed tree equality and exact-head hosted results
belong in external evidence and PR #132 comments, not edits during qualification.
Only after mandatory local success may a normal corrective commit/push update
the existing PR. No replacement PR, rebase, amend, force-push, auto-merge,
approval, merge, release, tag or deployment is authorized. A passing proposed
policy still requires independent review. Rollback is a normal reviewed revert,
never a baseline/fingerprint edit or weakened rejection.
