# DOC-005 coverage checkpoints

## 005A — RF Admin documentation checkpoint

Task base: `d4a4e262eb1404c963c509cf1e5d32f0c6712b99`, independently
fetched and confirmed as the PR #104 merge. No intervening main commits,
equivalent DOC-005 branch or open PR were found at preflight. Existing worktrees
and the clean original checkout were preserved.

Worktree: `/home/zboll/git/ams-mel-doc-005`; branch: `docs/005-rf-admin-c2`.
External evidence: `/home/zboll/git/ams-mel-doc-005-evidence/r1`.

| Inventory | Pristine | 005A | Total |
| --- | ---: | ---: | ---: |
| AMS.MEL.RF.Admin | 0 | 15 | 15 |
| AMS.MEL.RF.C2 | 12 | 12 | 771 |
| Repository | 2,897 | 2,912 | 4,295 |

Remaining repository gaps: 1,383. Exactly 15 Admin gap identities and 15 public
warning identities were removed; zero additions. Full removed identities are
retained in external `005a-delta.json` and the removal-only baseline diff.
All non-Admin package counts and full gap identities remain unchanged.
Warnings: 1,146 raw = 1,073 public + 73 unchanged private-part; no other warning
categories. Native inventory remains 1,238 declarations / 1,237 rendered sections,
with the unchanged single Health renderer exception.

Qualified toolchain outputs: Alire 2.1.1; GNAT 16.1.0; GPRbuild 26.0.1
distribution reporting `GPRBUILD 26.0.0 (2026-04-15)`; GNATdoc 26.0.0
(20260415). Exact environment and outputs are retained externally.

Checkpoint validation passed:

- Required `make format-ada` and `make check-ada-format`.
- Qualified semantic compilation and strict generation twice; generated sites
  byte-identical, HTML structure valid, 31 pages and 1,307 resolved local links.
- 75 documentation unit/mutation tests and six dedicated source-integrity tests.
- Literal-aware source/production integrity against task base: 1,178 protected
  files; baseline removal-only validation and Git whitespace check.

The initial pre-ratchet runs intentionally failed on removed warning identities.
An initial independent coverage assertion also failed: three descriptions were
not credited due to identifier cross-reference rendering. Equivalent source-backed
prose without those underscored identifiers restored all three attachments.
Those failures and their logs are retained; no tooling or policy was changed.

**Incomplete task, not final qualification:** C2 phases 005B–005E have not been
implemented or qualified. No production regression gate is credited for this
checkpoint. No commit, push or PR has been made. The branch must not be presented
as ready for independent review. No hardware or pinned-provider execution was
performed during this checkpoint.

## DOC-005A recovery — Admin-only publication scope

The earlier checkpoint above is preserved as history. This continuation covers
**AMS.MEL.RF.Admin only**; it does not complete DOC-005 or any RF C2 phase.
RF C2 remains byte-identical to the starting main and is deferred to subsequent
work. Only the seven files authorized for DOC-005A are changed.

Starting main: `d4a4e262eb1404c963c509cf1e5d32f0c6712b99`, freshly fetched
and still the PR #104 merge. The original branch `docs/005-rf-admin-c2` remains
at that commit. A normal branch creation retained all seven modified files on
`docs/005a-rf-admin` in the existing worktree. Before any edits, the full tracked
diff and both untracked reports were copied to a new external evidence directory:
`/home/zboll/git/ams-mel-doc-005-evidence/r2-admin-publication`.
All `r1` evidence is preserved unchanged.

Fresh qualification reproduces Admin 15/15, repository 2,912/4,295 and 1,383
remaining gaps. RF C2 remains 12/771. Warning totals are 1,146 raw, 1,073 public,
73 private-part, and zero other categories. Complete untouched-package element
identities and documented flags are unchanged, not merely package counts.
All nonpublic warning identities, native profiles/anchors and the one established
Health renderer omission are unchanged. Native inventory: 1,238 source
declarations / 1,237 rendered sections.

The final formatted Admin specification SHA-256 is
`e6693ac514c6d72a015b9b62f9598e7e9d9e6806517c8a00f613ca4a955b0c92`.
This is a source-content hash, not a self-referential commit identity. The submitted
commit and PR head are recorded externally and in the publication handoff.

### Exact removed coverage gap identities (15)

```text
AMS.MEL.RF.Admin: AMS.MEL.RF.Admin.Admin_MEL: type: type Admin_MEL is limited private;: description
AMS.MEL.RF.Admin: AMS.MEL.RF.Admin.Close: procedure: procedure Close ( Object : in out Admin_MEL ) ;: description
AMS.MEL.RF.Admin: AMS.MEL.RF.Admin.Close: procedure: procedure Close ( Object : in out Admin_MEL ) ;: parameter 0:Object
AMS.MEL.RF.Admin: AMS.MEL.RF.Admin.Command_State: function: function Command_State ( Object : Admin_MEL ; State : AMS . MEL . Status . MFA_State ) return Boolean ;: description
AMS.MEL.RF.Admin: AMS.MEL.RF.Admin.Command_State: function: function Command_State ( Object : Admin_MEL ; State : AMS . MEL . Status . MFA_State ) return Boolean ;: parameter 0:Object
AMS.MEL.RF.Admin: AMS.MEL.RF.Admin.Command_State: function: function Command_State ( Object : Admin_MEL ; State : AMS . MEL . Status . MFA_State ) return Boolean ;: parameter 1:State
AMS.MEL.RF.Admin: AMS.MEL.RF.Admin.Command_State: function: function Command_State ( Object : Admin_MEL ; State : AMS . MEL . Status . MFA_State ) return Boolean ;: return 0:
AMS.MEL.RF.Admin: AMS.MEL.RF.Admin.Is_Open: function: function Is_Open ( Object : Admin_MEL ) return Boolean ;: description
AMS.MEL.RF.Admin: AMS.MEL.RF.Admin.Is_Open: function: function Is_Open ( Object : Admin_MEL ) return Boolean ;: parameter 0:Object
AMS.MEL.RF.Admin: AMS.MEL.RF.Admin.Is_Open: function: function Is_Open ( Object : Admin_MEL ) return Boolean ;: return 0:
AMS.MEL.RF.Admin: AMS.MEL.RF.Admin.Open: function: function Open ( Library_Path : String ; Configuration : String ) return Admin_MEL ;: description
AMS.MEL.RF.Admin: AMS.MEL.RF.Admin.Open: function: function Open ( Library_Path : String ; Configuration : String ) return Admin_MEL ;: parameter 0:Library_Path
AMS.MEL.RF.Admin: AMS.MEL.RF.Admin.Open: function: function Open ( Library_Path : String ; Configuration : String ) return Admin_MEL ;: parameter 1:Configuration
AMS.MEL.RF.Admin: AMS.MEL.RF.Admin.Open: function: function Open ( Library_Path : String ; Configuration : String ) return Admin_MEL ;: return 0:
AMS.MEL.RF.Admin: package: description
```

### Exact removed public warning identities (15)

```text
ams-mel-rf-admin.ads: AMS.MEL.RF.Admin.Admin_MEL: type Admin_MEL is limited private;: warning: entity `Admin_MEL` is not documented
ams-mel-rf-admin.ads: AMS.MEL.RF.Admin.Close: procedure Close (Object : in out Admin_MEL);: warning: entity `Close` is not documented
ams-mel-rf-admin.ads: AMS.MEL.RF.Admin.Close: procedure Close (Object : in out Admin_MEL);: warning: parameter `Object` is not documented
ams-mel-rf-admin.ads: AMS.MEL.RF.Admin.Command_State: function Command_State (Object : Admin_MEL; State : AMS.MEL.Status.MFA_State) return Boolean;: warning: entity `Command_State` is not documented
ams-mel-rf-admin.ads: AMS.MEL.RF.Admin.Command_State: function Command_State (Object : Admin_MEL; State : AMS.MEL.Status.MFA_State) return Boolean;: warning: parameter `Object` is not documented
ams-mel-rf-admin.ads: AMS.MEL.RF.Admin.Command_State: function Command_State (Object : Admin_MEL; State : AMS.MEL.Status.MFA_State) return Boolean;: warning: parameter `State` is not documented
ams-mel-rf-admin.ads: AMS.MEL.RF.Admin.Command_State: function Command_State (Object : Admin_MEL; State : AMS.MEL.Status.MFA_State) return Boolean;: warning: return value is not documented
ams-mel-rf-admin.ads: AMS.MEL.RF.Admin.Is_Open: function Is_Open (Object : Admin_MEL) return Boolean;: warning: entity `Is_Open` is not documented
ams-mel-rf-admin.ads: AMS.MEL.RF.Admin.Is_Open: function Is_Open (Object : Admin_MEL) return Boolean;: warning: parameter `Object` is not documented
ams-mel-rf-admin.ads: AMS.MEL.RF.Admin.Is_Open: function Is_Open (Object : Admin_MEL) return Boolean;: warning: return value is not documented
ams-mel-rf-admin.ads: AMS.MEL.RF.Admin.Open: function Open (Library_Path : String; Configuration : String) return Admin_MEL;: warning: entity `Open` is not documented
ams-mel-rf-admin.ads: AMS.MEL.RF.Admin.Open: function Open (Library_Path : String; Configuration : String) return Admin_MEL;: warning: parameter `Configuration` is not documented
ams-mel-rf-admin.ads: AMS.MEL.RF.Admin.Open: function Open (Library_Path : String; Configuration : String) return Admin_MEL;: warning: parameter `Library_Path` is not documented
ams-mel-rf-admin.ads: AMS.MEL.RF.Admin.Open: function Open (Library_Path : String; Configuration : String) return Admin_MEL;: warning: return value is not documented
ams-mel-rf-admin.ads: AMS.MEL.RF.Admin: package AMS.MEL.RF.Admin is: warning: entity `AMS.MEL.RF.Admin` is not documented
```

## DOC-005A final local qualification — complete

All required local gates passed under the saved stable compiler environment.
The production gates ran sequentially, never overlapping native build trees:

| Gate | Observed result |
| --- | --- |
| Required format and format validation | PASS |
| Qualified semantic compilation / strict GNATdoc twice | PASS |
| Full gap/warning identities, native profiles/anchors | PASS; no additions |
| Two fresh complete sites | PASS; all 32 generated files byte-identical |
| HTML structure / local links | PASS; 31 pages / 1,307 links |
| Documentation unit and mutation tests | PASS; 75 tests |
| Dedicated integrity tests | PASS; six tests |
| Literal-aware integrity against starting main / DOC-001 | PASS; 1,178 / 1,162 protected files |
| Independent production/vendor byte inventory | PASS; 1,180 non-documentation files, only Admin comments differ |
| make test-native | PASS; 434/434 |
| make test-ada | PASS; full smoke/contracts and focused repeat checks |
| make check | PASS; isolation, 434/434 native, format, Ada and text gates |
| Final make check-ada-format | PASS |
| Final Admin source hash versus tested source | PASS |
| Whitespace and final newlines | PASS |

Full output and actual exit statuses are retained under external
`r2-admin-publication`, with separate `test-native`, `test-ada`, `check`,
`final-format-check` logs and `.exit` files. No inherited test failure occurred
in this sequence; no tests were weakened, suppressed or reclassified. A polling
command exceeded the tool's 30-second timeout; it was not a gate failure and did
not interrupt the background qualification runner. The documentation mutation
suite's expected warning-ratchet error text appears inside a passing 75-test run.

Final measured inventory remains Admin 15/15, repository 2,912/4,295, 1,383 gaps;
15 removed gaps and 15 removed public warnings, zero additions. All other package
identities and private warning identities remain unchanged. No new pinned Squall
or hardware execution was performed. RF C2 remains untouched and deferred.

Local qualification is complete; publication and exact-head hosted statuses are
separate observations recorded in the external handoff. Independent review is
required; no merge, tag, release or feature-branch Pages deployment is authorized.
