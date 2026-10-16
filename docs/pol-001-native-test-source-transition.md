# POL-001 — Exact reviewed native-test source transition

## Purpose and immutable foundation

DOC-001 protects source bytes and literal-preserving executable Ada tokens so
documentation changes cannot silently introduce behavior, ABI, ownership or
vendor changes. A clean Git diff alone is insufficient: unauthorized content
can already be committed. The checker therefore compares the tracked inventory
and protected source against foundation commit
`42648a92eb0cc84b2980d16db6028992c0494ba1`. **That base is unchanged.**

POL-001 starts from main `7e0b7c4314978a609fa6075f9816827c2d6233b1`, after
DOC-007A PR #110 merged. The independent native-test correction in
[PR #111](https://github.com/zackboll/ams-gra-mel-bridge/pull/111), reviewed head
`048387edee98322f6df2d7dadb14d94f2c022ef9`, legitimately changes two protected
test files. The old gate rejects those bytes even though no production behavior
changes. POL-001 authorizes that precise content, not the branch or its status.
No native test is changed by this policy PR.

## Independently verified complete-file identities

Both original files were extracted from the foundation and compared byte for
byte with starting main. Approved files were fetched from PR #111 and hashed
from actual bytes; supplied expected hashes were checked, not assumed.

| Exact path | State | SHA-256 |
| --- | --- | --- |
| `native/tests/mock_rf_provider.cpp` | Original | `cab91b7cf0901fc5ba6dc7e013b22c9bc88a5062ce9dc324b0f8e9aa359865b0` |
| `native/tests/mock_rf_provider.cpp` | Approved | `ef8c1dc59987ea2ff16cb00d6a60880ab560b910127105b6dd7698498be9d957` |
| `native/tests/test_rf_job_interval_status.c` | Original | `684e01b689d7cf0464168dda7cca54df2ff0377f65e250a9273d18318c97735e` |
| `native/tests/test_rf_job_interval_status.c` | Approved | `96f71931cbf3c03937d352997666ad459db9f517d514c7827579bbfa22a62930` |

PR #111 has exactly these two native-test paths plus its research/coverage
documentation. Comparison with current main and the foundation finds no change
to production implementation, headers, exports, Ada or vendor content. The
owner-counting diff is narrowly test-only: an immutable creation index selects
shutdown/destruction counters under the existing mock cleanup mutex. Records
retain no provider objects; vector growth and reads share that mutex. One record
per created mock C2 grows with the finite test workload, not a production queue.
The C regression captures identity after serialized creation, preserves exact
equality and exception/lifetime assertions, and demonstrates that unrelated
shutdown cannot satisfy the selected owner's count. This policy is not new
production lifetime or arbitrary-provider qualification; PR #111 retains its
own independent review and negative-control evidence.

## Allowed states and atomic pairing

`REVIEWED_NATIVE_TEST_TRANSITION` in the checker is a literal, immutable tuple
of exactly two paths and four whole-file SHA-256 identities. The checker first
verifies that the pair's foundation blobs match the recorded originals. It then
checks **both working-tree contents and stage-0 Git index blobs**, independently:

1. **ORIGINAL:** both files equal their exact foundation bytes.
2. **APPROVED:** both files equal their exact reviewed PR #111 bytes.

Both views must select the same complete state. A mixed pair, single-file
transition, one-byte mutation, staged mutation hidden by restored working-tree
bytes, or disagreement between two otherwise valid complete states fails.
Missing members, working-tree symlinks, nonregular index entries and unmerged
index stages fail. There is no need to stage a partial correction: install and
stage the complete pair before running the gate.

The CLI retains `--base` only for the fixed foundation value; it cannot redirect
qualification to an arbitrary commit. The Python `check(root, base)` API remains
available for isolated, committed regression fixtures. A fixture without either
eligible path still uses ordinary inventory/byte/token protection; adding an
eligible path to such a foundation is not an exemption.

All other protected files continue to use the existing exact foundation-byte
or Ada-token comparisons. Unexpected tracked additions, missing protected
files, executable Ada changes, unrelated tests, native implementation, headers,
exports and vendor changes remain forbidden. Warning/coverage baselines, native
anchor/profile identities, renderer exceptions, workflows and publication
policy are unchanged. There is no directory wildcard, editable approval
manifest, environment override, branch-name exemption, status waiver or
allow-all switch. **Any future edit to either approved file needs another
explicit policy review; this approval is not a future test-edit permission.**

## Qualification and negative controls

`scripts/test_doc_source_integrity.py` retains the six original integrity tests
unchanged and adds real temporary Git repositories with commits. Fixtures read
the original foundation blobs and reconstruct the literal reviewed PR #111 diff;
their final complete-file hashes must equal the approved constants. They do not
mock hashing, need a network fetch, require PR objects to exist in a CI clone,
or trust a self-authorizing external manifest.

Controls cover both-original and both-approved positive states, each partial
replacement, a one-byte mutation in either approved file, unrelated native test,
production implementation, executable Ada and vendor mutations, unexpected
workflow additions, staged mutations concealed by valid working-tree content,
partial staged transitions, complete-view disagreement, missing files, symlinks,
unmerged stages and attempted foundation replacement. The original committed
inventory/addition/removal tests continue to run, including NUL-safe unusual
paths and unauthorized native/Ada/Rust/Python/integration/script additions.

An end-to-end positive check imports the proposed policy checker but points it
at the untouched PR #111 worktree at the fetched reviewed head. Working-tree and
index hashes qualify as APPROVED while all 1,162 protected files remain checked;
that checkout stays clean. Starting main qualifies as ORIGINAL. This does not
copy policy files into PR #111 or change its branch, index or commits.

External evidence directory:
`/home/zboll/git/ams-mel-policy-001-evidence/r1`.
It preserves extracted original/approved bytes, diffs, SHA-256 and Git blob
identities, exact commands, complete output, actual exits, tool identities and
the cross-worktree result. Documentation and sequential native/Ada regression
results are recorded there; queued/running hosted CI is never counted as passed.

Completed local results:

| Gate | Result |
| --- | --- |
| Integrity unit/committed mutation fixtures | 23/23, including all six unchanged original tests |
| Complete documentation unit/mutation suite | 105/105 |
| Starting-main check | ORIGINAL; 1,162 protected files |
| Untouched fetched PR #111 worktree | APPROVED; 1,162 protected files |
| Two independent complete site generations | Byte-identical to each other and starting main |
| Documented elements | 4,294/4,295; one exact Health metadata renderer exception |
| Warnings | 73 raw / 0 public / 73 private-part; no identity delta |
| Native declarations | 1,238 source / 1,237 rendered; unchanged anchors/profiles |
| Portal inventory | 39 files / 37 HTML pages / 1,516 links / 29 unique public package destinations |
| Vendor checksums | 804/804 |
| `make test-native` | Exit 0; 434/434 |
| `make test-ada` | Exit 0; existing contract execution passed |
| `make check` | Exit 0; isolation/native/format/Ada/hygiene passed |
| `make check-ada-format` | Exit 0 |

Documentation used Alire 2.1.1, GNAT 16.1.0, GPRbuild's 26.0.1 distribution
(binary reports 26.0.0, 2026-04-15), and GNATdoc 26.0.0 (20260415).
Native scripts resolved Debian GCC 14.2.0 `cc` and GNAT 16.1.0 `c++` under
the stable recorded PATH; all mutable native build commands ran sequentially.
Existing source-less external C-project GPR warnings remain unchanged.
All baselines and renderer/native-identity manifests are byte-identical to
starting main. The literal fixture reconstruction was additionally compared
byte for byte with the independently fetched approved files. Final qualified
commit/head, synthetic merge parents/tree and hosted conclusions are recorded
externally after publication, not inferred from local success.

## Required review, merge order and synchronization

1. Independently review and qualify this **policy-only PR on main**, while main
   still contains the original pair. Do not fold PR #111 into it.
2. Only after authorization, merge the policy PR first. This task does **not**
   authorize or perform that merge.
3. Then, in a separate authorized action, synchronize PR #111 with the new main
   using a normal merge or reviewed rebase under repository policy. Preserve the
   two approved native-test files byte-identically; do not merely rerun its old
   head, which still carries the old checker. Reverify both file hashes, source
   scope, local/remote/PR head equality and all required CI on the new head.
4. Review PR #111 independently before any later merge. No merge or auto-merge
   of either PR is performed here; PR #111 is not updated by POL-001.

## Security limitations

This is an exact-content review gate, not an adversarial sandbox or external
attestation. CI executes checker/test/workflow code from the branch; a malicious
branch can modify that code or its approval constants. The existing explicit
documentation-tool additions are not independently signed content. Git and the
local Python environment must be trusted; concurrent writers during checking
are outside the guarantee. SHA-256 matching assumes collision resistance and
does not prove that approved code is safe. Fixed hashes also make byte-changing
line-ending conversion fail intentionally. Independent diff review, protected
required checks and review of policy/workflow changes remain necessary. This
task changes no repository protection settings and does not claim CI alone can
enforce its own immutability.
