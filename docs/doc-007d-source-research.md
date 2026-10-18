# DOC-007D source-verified local type hyperlinks

## Immutable baseline and publication

Fetched MEL main is `0e64ea0689f2fae360baba78b6ed6f74aeef08ca`, actual PR #114
merge, tree `20a711e6c0a3853eee8aca9dd04ae881cef11806`. Ordered parents are
`4c72639579891c563425fd9c6c687ce64f892892` and reviewed head
`2a30057b425169e68e084e84cecf78c528e27aa0`. No intervening main commits were
present at task start. Work is isolated in branch `docs/007d-local-type-links`,
worktree `/home/zboll/git/ams-mel-doc-007d`; earlier worktrees/evidence are untouched.

Post-merge Documentation run [39062734043](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/39062734043)
has successful build and deploy jobs. Bootstrap
[39062734023](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/39062734023)
completed successfully in all eight jobs; it is no longer pending. Run/job JSON
and logs are preserved externally. Both the 41-file review artifact and extracted
Pages artifact have exactly the qualified DOC-007C R4 filenames and complete-file
bytes. Separate HTTP observations of the published portal and API index returned
the same landing-page bytes. This bounded live smoke is not whole-site artifact
verification and is not a new deployment.

## Minimal shared dependency change

| Identity | Old | Selected |
|---|---|---|
| Commit | `c1a691452cf436624809f4043b30087de6688052` | `478d0bbff0be1b9141b5ad39f07a71f0386d86e9` |
| Archive SHA-256 | `d4b422b73df60d6ed41ec534b71bd0248b43827ce54ed4c323455e225be17493` | `a55cad3e588c7a4e13f19f46d164b2661e97ec546e97a163d2106d04d4851109` |
| Implementation/assets aggregate | `384974fefbc2772ef6c79d43b275b549e5c12758535450a60c852c401e56ba1a` | `4e719d06a60074684144efbb444b75e4eef413ab30410cf29e628f79a32124c9` |

Selected [PR #5](https://github.com/zackboll/ada-gnatdoc-enhancer/pull/5) reviewed
head `af531cf6553dd36099d6f541736db9e3f0beac67` and actual merge share tree
`9d3c14c795a92d46edc89e0b19d697db4ca802fb`. Merge parents are old pin then
reviewed head. Exact-merge [run 39047243258](https://github.com/zackboll/ada-gnatdoc-enhancer/actions/runs/39047243258)
passed Python 3.11/3.12/3.13 and wheel jobs. The archive was independently
downloaded and hashed, then installed non-editably in a fresh venv with the
existing archive check, pinned setuptools/wheel, no dependencies/build isolation,
version check and complete implementation/assets fingerprint check. Version
`0.1.0.dev0` alone is not authority; hashes are proposed dependency identities,
not independent approval.

Implementation delta is `config.py`, `pipeline.py`, `syntax.py`, `validation.py`,
`qualification.py`, plus new `external_types.py`. All bundled assets, including
theme CSS, are byte-identical. External mapping/dependency/host defaults are empty
and remain unauthorized by the consumer's exact TOML gate. The selected pipeline
adds local/external counters even with type links off. Source/native/profile
reconciliation is unchanged. The complete upstream delta, package inventories,
archive and installed metadata are external evidence, not vendored MEL files.

### Concrete scanner regression

Both immutable implementations were exercised separately with visible local `T`:

```ada
procedure F (S : String := "return T");
procedure F (S : String); -- return T
```

The old implementation links `T` inside each literal/comment; selected revision
emits neither link. `literal-comment-before-after.json` preserves exact positions,
markup and process results. This is a synthetic defect reproduction, **not** a
claim of an observed false link in current MEL output.

### Types-off compatibility

An external control configuration retains theme/highlighting on, type links off,
navigation/guides none. The production gate is not patched to accept this control.
All 40 non-manifest files are identical to DOC-007C. Independently parsing both
manifests and adding exactly two zero-valued count keys to the old one yields the
new manifest, with no other field difference. False all-41 equality is not claimed.

## Independent semantic acceptance

`docs/tools/local_type_audit.py` uses unchanged MEL `specification`,
`declaration_identity`, public package headers and the pinned native anchor ledger.
Each containing source declaration maps to an actual pristine page/section.
Token-preserving source/native alignment accounts for established renderer
projections without importing enhancer reconciliation or resolution functions.
The bounded lexical grammar identifies the exact original token offset and
checks colon type marks, function results, array elements and supported
derived/access definitions. Literal/comment/attribute tokens and defining names
cannot grant type-reference authority. Only ordinary current/ancestor/full-name
and unique imported-use visibility can establish a source-owned type/subtype.

The independently selected identity must agree with `data-type`; its actual
ledger page and unique native section must agree with `href`. A short-name match,
shared manifest/count, output metadata or successful shared self-verification is
not proof. Direct tests mutate wrong-but-existing destinations, identity pairs,
contexts, spelling, attributes and nested markup without relying on package
fingerprint or output-reconstruction failure. Removal/completeness is separately
guarded by full expected-output reconstruction: occurrence acceptance cannot
prove that a missing link was emitted.

Only exact flat link wrappers in qualified sections and approved lexical spans
are removed; all other markup remains rejected. Restored complete files must
equal trusted pristine bytes, and unchanged original site/coverage validation
runs on the restored complete tree. The complete enhanced output and entire
manifest are independently reconstructed from trusted inputs and fingerprinted
tool, as before. Source, anchor and pristine authority hashes are unchanged.

The external `type-link-occurrence-audit.json` records every containing source
file/declaration/line, original native declaration/offset, source token offsets,
supported context, target source declaration, exact native page/anchor and href.
It contains 1,418 occurrences: 1,080 colon marks, 331 returns, seven array elements.
There are 26 legitimate linked bases before `'Class`; no attribute token links.
The historical shared-tool count 1,418 is a comparison, not an input quota.

## Real source acceptance and conservative omissions

- `AMS.MEL.RF.Product_Rx.Complex_I16_Array` links its element to the existing
  `Complex_I16` record anchor `87f0aabcddc10ff4a8ff2227982865d2553dde4492b207e55e0e91a15a1cec9b`.
  The corresponding `Complex_I8_Array` links to record anchor
  `963062b3c78b3ea6f55686a8714e0e6eaf33a064e45ebb8fc6eb141904d8e98f`.
  Both targets live on observed native page
  `6ed8ea2a1a59f5d24924546adaf2f81b35ead370e48aebb921cc771ad72242c5.html`.
- Image `Orientation.Euler_Value` and `Attitude_Rate.Value` components link the
  full spelling `AMS.MEL.Status.Euler` to page
  `d04e5b8974a5cfebc85dae0fe57d43c261fd4378e9fe4888d5d2d9b7e2c97573.html`,
  anchor `33b57c0e76aa9291b0fca560f9a3f240ff93c4f2a6f72e65a52118af806aa837`.
  **Task wording qualification:** the exact `Value` parameter is `Raw_Euler`
  in `ada/src/ams-mel-ir-image.adb:461`, not a public specification declaration.
  Its source was inspected; no native section or hyperlink is manufactured.
  Public `Attitude_Rate.Value` is a component, not falsely called a parameter.
- Health/Status Metadata's exact `package Status renames AMS.MEL.Status;` remains
  the one native renderer omission. `Status.MFA_Status`, `Status.BIT_Status`,
  `Status.Discrete_Status`, `Status.Security_Audit_Record` and
  `Status.MFA_Status_Detailed` return marks remain plain. No general alias
  resolution or extra package section is added.
- Current-package, ancestor-package and qualified references are audited on real
  MEL input. Focused fixtures reject ambiguous/shadowed/unknown/alias-dependent
  destinations and non-type contexts. Unsupported grammar remains unlinked or
  causes emitted-link qualification failure; this is not a general Ada resolver.

Navigation/search, external libraries, generic-instance summaries, guide
rendering, JavaScript and production/API changes remain out of scope.
