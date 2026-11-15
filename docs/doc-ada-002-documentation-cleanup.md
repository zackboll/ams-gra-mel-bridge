# DOC-ADA-002 — Ada documentation cleanup

## Purpose and scope

The detailed Ada documentation review found ordinary documentation omissions,
misattached descriptions, stale feature guidance and two false rejections in the
changed-declaration annotation checker. This task corrects those findings
against freshly fetched main `99d7f11bc3e5b19c10c6d9daf0e7fe1c89b6fd41`.

The changes cover comments in 15 public Ada specifications, the Ada-facing
README/portal/guides, and the annotation checker with focused regression tests.
Token comparison against the base confirms that all 15 specifications retain
their complete non-comment token streams. No Ada declarations, profiles,
defaults, aspects, runtime implementation or native ABI changed.

Historical coverage reports, the reviewed IR API inventories, static native
anchor ledgers, warning baselines, enhancer/toolchain pins, workflows and CI
authority remain unchanged. Coverage and GNATdoc warnings remain informational;
this task adds no completeness threshold or CI gate. The checker remains the
explicit local changed-declaration check introduced by DOC-ADA-001.

## Source corrections

The 31 resolved ordinary coverage gaps are accounted for below. Package
attachment corrections separate the package introduction from the first
declaration; the previous prose could already appear in HTML, but its source
association did not satisfy the strict coverage model.

| Location | Correction | Elements recovered |
| --- | --- | ---: |
| `AMS.MEL.IR.C2.System_Data` | Nine operation summaries | 9 |
| `AMS.MEL.IR.Erase` | Six operation summaries, two subtype descriptions and package attachment | 9 |
| `AMS.MEL.IR.Camera` and `Camera_Protection` | Attach each `Outcome` description before its `use type` clause | 2 |
| `AMS.MEL.IR` | Five parameter and two return descriptions for control-only Image configuration/attachment | 7 |
| `AMS.MEL.IR.Discovery` | Describe the public capability-vector instance | 1 |
| `Calibration_Configuration`, `Calibration_Status`, `Channel.Metadata` | Separate package introductions from first declarations | 3 |
| **Total** | | **31** |

Contract prose now distinguishes the four base Image metadata kinds from the
independent opt-in owners, names all four Track metadata kinds, covers both
Scheduling metadata callbacks, and includes Scheduling and StackedImage among
the seven common-channel families. StackedImage common communications metadata
and optional Health NUC reception are described as implemented.

Health NUC documentation distinguishes immediate logical deactivation and
draining of queued copies from deferred physical cleanup, which drains in-flight
bridge NUC callbacks after channel destruction. Erase accessors document their
error on a default command that was not constructed with `Create_Command`.

## Guide and navigation corrections

The portal and API map derive their package directory from current source and
direct readers to the matching build's diagnostics instead of publishing old
package counts or unconditional completeness claims. The task-selection map
now covers all 43 current public library packages.

The guides explain discovery, control-only Image attachment, separate metadata
owners, common communications-test registration, quaternion metadata,
Scheduling/StackedImage foundations and session-wide asynchronous admission.
They retain the distinction between conditional camera support and optional
calibration/erase/system-track-data operations. They also distinguish bridge
`Resource_Exhausted` from provider rejection and explain that timeout or public
handle closure does not itself cancel work or release admission capacity.

Implementation limits remain explicit: this task adds no acquisition buffers,
image-stack reception, Scheduling task execution, generic buffer ownership or
additional callbacks. The foundation document's current-contract paragraph
describes current-source qualification; its historical sections are preserved.

## Annotation-checker correction

The checker now recognizes a generic package instantiation as an instance rather
than an explicit nested package. It validates the instance's selected trailing
documentation without treating generic actuals as local subprogram parameters.

Private types expose only their named public discriminants for `@field`
validation. The shared `discriminant_names()` helper supplies those names;
private-completion payload fields remain private, and an unknown `(<>)` part
declares no field names to document.

Thirteen added regression methods cover valid and invalid instance annotations,
following-declaration boundaries, nested packages containing instances, named
and grouped private discriminants, unknown discriminants, hidden completion
fields, missing/duplicate/stale/placeholder tags and changed-only selection.
The complete suite passes 68 tests, including all 55 existing tests. An
independent old/new comparison over all 1,503 public declarations found exactly
13 diagnostic changes: the 12 legitimate package instances and Health metadata's
legitimate `Event_Kind` discriminant. No other declaration changed result.

Explicit nested packages, generic subprogram instances, public variant records,
ambiguous identities and other unsupported shapes retain their prior rejection
behavior. This remains a lexical source check, not an Ada compiler or a
natural-language contract validator.

## Measured documentation result

The before measurement comes from the inspected documentation artifacts for
`8a2d409b53b90161bee01dcdd2f3641480d178d6`, whose Ada surface is unchanged at
the task base. Its source workflow is
[run 43658706022](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/43658706022).
The after measurement is a genuine pinned-toolchain `docs_ci.py build` at
implementation commit `23d8d9a9e958cced3d7213426e52043cbb218fe8`, tree
`a51221786b49521feeb6fc80424030cd0c144dcd`.

| Measure | Before | After |
| --- | ---: | ---: |
| Documented elements | 5,016 / 5,048 | 5,047 / 5,048 |
| Documented percentage | 99.3661% | 99.9802% |
| Ordinary authoring/attachment gaps | 31 | 0 |
| Known renderer exceptions | 1 | 1 |
| Public GNATdoc warnings | 23 | 0 |
| Private-part GNATdoc warnings | 101 | 101 |
| Unclassified/tool warnings | 0 | 0 |
| Public source declarations | 1,503 | 1,503 |
| Native rendered declaration sections | 1,502 | 1,502 |

Shared/Core remains 577/577 and RF remains 1,644/1,644. IR increases from
2,795/2,827 to 2,826/2,827. The regenerated native-anchor mapping is exactly
equal to the inspected baseline mapping, including all 1,502 entries.

The remaining uncredited element is
`AMS.MEL.IR.Health_Status.Metadata.Status`, the package alias for
`AMS.MEL.Status` that GNATdoc 26.0.0 does not render. It remains visible in the
inventory and is explicitly explained in the portal. This task does not claim
100% coverage or hide private-part warnings.

The local manifest SHA-256 is
`454c156be88d6bb3ebcae2c2458e6e3a6c8463decb12f8493f55b62e33d022fb`.
Its identity is `local-not-deployable`; it is evidence of local generation and
verification, not a hosted Pages deployment. Delivery HEAD and hosted results
belong in the PR, avoiding a self-referential commit identity in this report.

## Validation

Actual local commands and results:

- `make format-ada` — PASS before build/test; formatter changes are retained.
- `make check-ada-format` — PASS.
- `make check-ada-docs-changed` — PASS, 27 public declarations inspected and
  zero missing/invalid annotations against `origin/main` at the task base.
- `python3 -W error -m unittest discover -s scripts -p test_ada_doc_annotations.py -v`
  — PASS, 68 tests.
- `make test-policy` — PASS, 244 tests across eight suites, including the
  documentation CI/security/coverage regressions.
- `python3 scripts/check_doc_source_integrity.py` — PASS, 825 vendored files
  and four workflows; existing dependency/security boundaries preserved.
- Under the pinned documentation toolchain,
  `.doc-tools/enhancer/bin/python docs/tools/docs_ci.py build --gnatdoc-prefix "$PWD/.doc-tools/gnatdoc-26.0.0" --directory <fresh-output-directory>`
  — PASS. The real generation, semantic specification compilation, coverage
  measurement, portal/link validation, enhancement and sealed-output
  verification all completed successfully.
- Independent rendered HTML review — PASS: all eight authored HTML pages are
  identical before/after enhancement; 1,186 rendered links/fragments resolve;
  26 new guide facts, 20 native declaration sections and eight package summaries
  were inspected. This is content/structure/link review, not browser visual QA.
- Token equality against the base for all 15 changed Ada specifications — PASS.
- `git diff --check` — PASS.

See the PR for repository-wide native/Ada and Alire validation results and
delivery-HEAD hosted check statuses.

The initially shallow checkout was expanded so the policy tests could read
their historical fixtures. CMake 4.4.5 changed the CTest summary format and the
existing isolation wrapper could not extract its test count, although CTest
reported all 656 tests passing. Validation therefore uses CMake 3.31.10 without
altering the repository's build-isolation check.
