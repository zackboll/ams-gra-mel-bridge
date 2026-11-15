# RUSTDOC-003 — safe Rust IR API documentation

## Identity, scope and implementation boundaries

Freshly fetched starting `origin/main`: commit
`8892834fb41f3929083df898e472e20160212f79`, tree
`08ec4970f9a6f581a10b0a1e5eda83218220dd02`.
Branch: `docs/rustdoc-003-ir-api`; independent worktree:
`/home/zboll/git/ams-rustdoc-003`. Native production/test trees and Cargo output
are local to this worktree, not shared with other development. Final commit,
tree, clean-checkout qualification report paths and hosted run identities are
recorded in the PR/delivery evidence rather than attempting a self-referential
commit hash in this document.

Only the twelve assigned IR Rust source files and this document change. Rust
changes are Rustdoc comments, with whitespace-only macro layout to attach a
comment to each existing generated variant. Macro matchers, repetitions,
declarations, discriminants, expressions, ordinary implementation comments and
behavior are unchanged. Inline test modules remain byte-identical, as do all
external tests. No dependency, native ABI, implementation inventory, `lib.rs`,
RF module, raw sys, Ada, Python, workflow or publication/security policy changes.

Reviewed AGENTS, architecture/C ABI policies, provider foundation, IR inventory
schema, existing Rust IR feature evidence, native lifecycle/queue/conversion
code and pinned published headers. The structured IR inventory measures native
and Ada implementation, not Rustdoc: no classification change is warranted.
The unchanged read-only inventory audit reports 127 headers, 148 operations,
50 safe Ada. Historical coverage and shared feature-status files stay untouched.

## Public declaration coverage

All public declarations, fields and variants in the assigned files now have
Rustdoc; all fallible public IR methods have `# Errors`, including numeric input
conversions. Existing accurate comments and examples are preserved. Generated
enum variants point to per-enum published numeric/meaning tables, not inferred
provider policies. The coverage delta eliminates **280** safe missing-doc
diagnostics; this is documentation coverage, not new implementation support.

- Lifecycle/discovery: `HealthConfig`, `HealthStatusChannel`, their constructor,
  attach, enable, capabilities, weak view, metadata/NUC registration and close;
  `Session::capabilities`. Discovery copies complete provider-ordered snapshots
  without attachment or enablement. Advertisement is not usable-operation proof.
- C2: `MfaState`, `CoordinateFrameReference`, `DegradationMethod`, `ScanType`,
  `ScanParameters`, `ModeCommand`, `ConfigSetCommand`, `BitAction`, `BitCommand`,
  and all three submission methods. Full unsigned scan controls are not Booleans;
  BIT selectors are not UCI IDs. Acceptance, asynchronous completion, explicit
  rejection, command metadata and physical effects remain distinct.
- C2 metadata: `CommandState`, `CannotComply`, `BitControlInterface`,
  `CommandStatus`, `BitConfiguration`, `BitTypeDefinition`, `C2MetadataEvent`,
  `C2MetadataCounters`, `C2MetadataStream`, all methods and opt-in CommsTest events.
- Health metadata: `HealthMetadataKind`, `StateTransitionStatus`, `ComponentState`,
  `TemperatureState`, `BitResult`, `FaultSeverity`, `FaultState`, `FailureLevel`,
  `CsciMode`, `SecurityOutcome`, `SecuritySeverity`, all six typed security
  categories, `SecurityEventKind`, `SecurityEvent` and its variant fields;
  `Euler`, `ForeignKey`, `InstallationDetails`, `TemperatureStatus`, `MfaComponent`,
  `About`, `MfaStatus`, `ActiveBit`, `CompletedBitItem`, `CompletedBit`, `FaultData`,
  `FaultAmbiguityGroup`, `Fault`, `BitStatus`, `SubsystemDependency`, `CsciVersion`,
  `CsciInfo`, `SubsystemStatus`, `NameValuePair`, `SecurityArtifact`,
  `SecurityAuditRecord`, `HealthMetadataEvent`, counters and stream methods.
- Image metadata: `BadPixelReason`, `BadPixel`, `BadPixelList`,
  `AzimuthElevation`, `LineOfSightReport`, `LineOfSightEuler`, `NavigationResponse`,
  `ImageMetadataEvent`, counters and stream methods.
- Optional metadata: `NucPixelValue`, `NucPointType`, `NucTempPoint`, `NucTempData`,
  `NucCounters`, `NucOpenError`, `NucStream`; `ForwardRightDown`, `RowCol`,
  `OpticalDistortionMap`, its open error and stream; `Quaternion`,
  `LineOfSightQuaternion`, its open error and stream, with all fields/methods.
- Navigation submission: `PositionSolutionState`, `NorthEastDown`, `AttitudeRate`,
  all 18 named `PositionVelocityCovariance` terms, `NavigationReport`,
  `NavigationResult` and variant fields, `NavigationRequest`, submit/wait/close.
  Solution states are published categories, not an invented quality ranking.

HTML inspection covers **96** public type pages, **51** ordinary public method
anchors and **226** public struct-field anchors, including pre-existing docs.
Enum variant fields and macro-generated constants/conversions are additionally
covered by source review and Rustdoc diagnostics. Module-public types such as
`NucPixelValue` are checked at their canonical module pages, not assumed to have
an inlined crate-root page.

## Ownership and semantic precision

Independent native owners are distinguished from deep-copied Rust values and
weak views. Returned snapshots/metadata survive parent closure and provider
unload; an owner's `is_open` is only local handle state. Health weak-view docs
now correctly distinguish expired work operations from local view close/state.

Native bounded drop-incoming queues, saturating counters, registration one-shots,
partial failure retention and cleanup are documented. Owners remain non-Send,
non-Sync, without a new cross-owner concurrency promise. Timeout is not
cancellation; Close/Drop is not unregister, callback quiescence or immediate
unload. Retryable Health closure and one-attempt request/metadata release are
not conflated. No ownership safety or physical execution claim is added.

Established details include Celsius Health temperature, Kelvin NUC temperature,
signed nanosecond counts, scan radians/radians-per-second/seconds/metres,
navigation latitude/longitude/wander radians and NED speed/acceleration units,
Euler angular sequence, LOS Euler rates in radians/second, quaternion XYZW
navigation NED relative to wander North, image-axis P/Q/R radians/second, and
ODM forward/backward sample counts and direction-cosine extents.

Preserved limitations/ambiguities:

- No timestamp clock or epoch is invented. Navigation attitude-rate/covariance
  units, magnetic-heading/barometric-altitude units, datum and additional
  reference conventions remain unspecified where the pin is silent.
- INS states have no inferred accuracy thresholds, blend algorithm or quality
  ordering. No physical input validation, scan feasibility, state/mode rule,
  normalization or geometry algorithm is added.
- LOS azimuth/elevation pointing-rate time denominator, individual validity-bit
  positions, pixel-coordinate origins and map vector relationships are not guessed.
- NUC's private optional `tempError` has no accessor and remains unavailable;
  its omission does not imply absence in a provider. Camera-command responses
  have no safe Rust receive interface in this slice. Other unbridged callback
  families, hardware behavior and real-provider conformance remain unavailable.
- Published BIT interface values 2/3 keep their names/numbers despite inconsistent
  adjacent upstream descriptions; documentation does not resolve that ambiguity.
- The remaining safe missing-doc warnings are RF-owned; the raw sys reference
  is unchanged. Counts are informational, not a repository-wide coverage gate.

## Warning baseline, examples and local validation

Same local toolchain throughout: `rustc/rustdoc 1.98.1 (48a229cea 2026-09-01)`,
`cargo 1.98.1 (797e8a9bc 2026-08-05)`. Exact starting-base report, before tracked
edits and with empty Git status:
`rust/target/rustdoc-001/run-t0jyqx60/qualification.json`.

| Diagnostic | Exact starting main | Documented source |
|---|---:|---:|
| Safe `ams-mel` missing_docs | 333 | 53 (all in excluded RF files) |
| Raw `ams-mel-sys` missing_docs | 2,389 | 2,389 |
| Broken intra-documentation links | 0 | 0 |

The raw baseline is re-measured, **not** the task's 2,384 reference assumption.
All **1,177** generated raw crate/source HTML-reference files have identical
SHA-256 hashes before and after. All **1,435** generated HTML files pass the
foundation local-link verifier. Rustdoc denies broken intra-doc links. This is
not a browser, external-link or exhaustive HTML-fragment/search verification.

Eight additional doctests bring the safe total from 40 to **48**: four runnable
provider-free construction examples (Health config, NavigationReport, NUC
variant preservation, quaternion LOS), plus four compiled `no_run` operation
examples (Health lifecycle, C2 metadata, navigation request, ODM). All executable
examples run successfully; provider-dependent operations are not falsely run
without a provider. Existing compile-fail thread/ownership examples still pass.

Commands, run from the independent worktree (Cargo checks use worktree-local
`CARGO_TARGET_DIR` and `CARGO_NET_OFFLINE=true`):

```sh
python3 scripts/rustdoc_001_foundation.py
python3 scripts/rustdoc_001_foundation.py --doctests
sh native/scripts/build.sh
sh native/scripts/test.sh
AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build-tests/lib" \
AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers" \
  cargo test --manifest-path rust/Cargo.toml --workspace --locked
cargo check --manifest-path rust/Cargo.toml --workspace --all-targets --locked
cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets --locked -- -D warnings
cargo fmt --manifest-path rust/Cargo.toml --all -- --check
python3 -m unittest discover -s scripts -p 'test_rustdoc_001_foundation.py' -v
python3 -m unittest discover -s scripts -p 'test_doc_source_integrity.py' -v
python3 docs/tests/test_documentation_ci.py ReconciliationTests MeasurementTests PrerequisiteTests -v
python3 scripts/check_ir_api_coverage.py
python3 scripts/check_doc_source_integrity.py
python3 scripts/check_final_newlines.py
git diff --check
```

Source-iteration results: native CTest **656/656**, including C-compiled tests;
production build passes; all-target check, warnings-denied Clippy, formatting,
inventory/integrity/newline/whitespace checks pass. Rustdoc foundation **8/8**,
source-integrity regressions **18/18**, focused documentation CI reconciliation/
measurement/prerequisite regressions **28/28**. Final clean committed-checkout
workspace counts and qualification identities are recorded on delivery.

Additional, non-required broad documentation probes were attempted and are not
claimed as passing: `test_doc_foundation.py` needs a pristine generated Ada site
(11 tests ran, SiteMutations setup unavailable). Broad documentation CI discovery
needs the separate Ada enhancer and also exposes two legacy WorkflowTests that
expect the old unsharded workflow. Running those two WorkflowTests on the exact
starting-base detached checkout reproduces both failures. The required focused
Rustdoc foundation suite passes; no test, workflow or policy was weakened or
changed to conceal unrelated failures. Ada/Alire, Pages deployment and hardware
qualification are not claimed for this documentation-only slice.

Final qualification must identify a clean committed HEAD; dirty iteration
reports (`run-o9m2mii1`, `run-ml77fbcv`, `run-zjxshqlq`) are not final artifact
evidence. Hosted results and review disposition are reported against the final
PR HEAD. No self-approval, merge, auto-merge or deployment is authorized here.
