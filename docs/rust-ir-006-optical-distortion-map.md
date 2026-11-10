# RUST-IR-006 — Complete safe Rust OpticalDistortionMap reception

## Source and scope

Started from fetched main `fa91028804f38d3b2bd15ec8e5e2f80cd4a74a7e`, tree
`d48eb8238e18443bd0ad8f668fa11984ddf915b6`. PR #159 (RUST-IR-004) is merged;
RUST-IR-003 NUC remains present. At original implementation time, PR #173 was
separate C2 work, not incorporated or modified. Preflight found no competing open
ODM PR or existing safe implementation.
The final feature commit/tree and exact-head hosted results belong in the delivery
PR, avoiding a recursive source hash in this document.

Only the safe Rust layer and documentation change. The existing IRMEL-ADA-009
native implementation, C ABI, version script, raw Rust bindings, Ada, Python,
mock providers, build scripts and CI policies are unchanged relative to the
incorporated main. The original baseline had 284 function exports; that historical
count is not the current production inventory. See the reconciliation record
below. No hardware, Squall or real-provider qualification is claimed.

## Public API

The crate root reexports `ForwardRightDown`, `RowCol`, `OpticalDistortionMap`,
`OpticalDistortionMapStream`, and `OpticalDistortionMapOpenError`.

```rust,ignore
ImageStream::open_optical_distortion_map(&mut self, queue_capacity: usize)
    -> Result<OpticalDistortionMapStream, OpticalDistortionMapOpenError>;
OpticalDistortionMapStream::is_open(&self) -> bool;
OpticalDistortionMapStream::receive(&mut self, timeout_ms: u32)
    -> Result<OpticalDistortionMap, Error>;
OpticalDistortionMapStream::counters(&mut self)
    -> Result<ImageMetadataCounters, Error>;
OpticalDistortionMapStream::close(&mut self) -> Result<(), Error>;
OpticalDistortionMapOpenError::error(&self) -> &Error;
OpticalDistortionMapOpenError::registration(&self) -> Option<CommandReturn>;
```

The stream is uniquely owned, neither Clone, Send nor Sync. Separate compile-fail
doctests check Send and Sync. Mutable methods serialize access without introducing
Rust callbacks, threads, mutexes or queues. No parent reference or Rust lifetime
is retained. `is_open` reports local ownership, not provider/acquisition liveness.
Values derive Clone, Debug and PartialEq; the component records also derive Copy.
PartialEq follows normal IEEE NaN/zero behavior; fidelity tests use `to_bits()` for
finite values, signed zero and infinity, and classification for NaN.

## Complete field mapping

The immutable native view is exactly `AmsMelIrOpticalDistortionMapV1`:

| Published getter | Native view field | Owned Rust field/type |
| --- | --- | --- |
| getWidth | width | width: u32 |
| getHeight | height | height: u32 |
| getRightSize | right_size | right_size: u32 |
| getDownSize | down_size | down_size: u32 |
| getPixelsPerOdm | pixels_per_odm | pixels_per_odm: u32 |
| getRightMin | right_min | right_min: f64 |
| getRightMax | right_max | right_max: f64 |
| getDownMin | down_min | down_min: f64 |
| getDownMax | down_max | down_max: f64 |
| getFrd | frd | frd: Vec\<ForwardRightDown\> |
| getRowCol | row_col | row_col: Vec\<RowCol\> |

`ForwardRightDown { forward: f64, right: f64, down: f64 }` maps getForward,
getRight and getDown respectively. This reusable type carries no ODM-specific
normalization or unit assumption, so a future quaternion task can reuse it.
`RowCol { row: f64, column: f64 }` maps getRow and getCol respectively.

All five unsigned fields retain the entire u32 domain, including zero and MAX.
Each vector is independently copied using its actual native span length. There
is no length equality, product-of-dimensions constraint, padding, truncation,
deduplication or sorting. All double components are assigned without rounding,
clamping, axis swapping, normalization, coordinate conversion or inferred units.
Signed zero, subnormals, extremes, infinities and NaN are retained. Native code
does not guarantee NaN payload bits. Reception alone says nothing about accuracy
or suitability for a physical computation.

## Registration and lifecycle

The six reused exports are `ams_mel_ir_image_odm_{open,receive,get_counters,close,
event_view,event_close}`. No new export or test hook is added to the facade.

Zero capacity fails locally with InvalidArgument and does not consume the native
one-shot. A closed Image owner fails locally first with ProviderFailed. An actual
registration attempt is one-shot even on failure or consumer Close. Registration
is eligible on Attached or Running Image, not after Stop; it does not start
acquisition or retry. The native one-shots are independent of ordinary four-kind
Image metadata, NUC_TempData, CameraCommandResp and LineOfSightQuaternion.

An unpublished u32::MAX sentinel detects whether native code published a known
Return. Success requires both a non-null owner and Return::Success. Open errors
retain normal Error category/diagnostic plus these optional Returns:

| Provider result | Error category | registration() |
| --- | --- | --- |
| Success | successful owner | not an error |
| NotSupported | ProviderFailed | Some(NotSupported) |
| Fail | ProviderFailed | Some(Fail) |
| NotImplemented | ProviderFailed | Some(NotImplemented) |
| Unknown Return | ProviderFailed | None |
| Exception | ProviderException | None |

Local/eligibility failures have no fabricated Return. Diagnostic text is not
parsed. Open validates both outputs; even an unexpectedly published owner on
failure has a Drop guard.

Receive calls native exactly once. Zero polls; every other timeout passes
unchanged. Timeout is not cancellation. Active-empty receives report Timeout.
Queued maps drain before StreamStopped after Stop/parent teardown, or before
ProviderFailed after uncertain detach. Events remain alive throughout conversion,
are closed before returning a map, and are guarded on malformed diagnostics,
native failure, conversion failure and unwind. Values can outlive the native
event, consumer, Image, Session and provider DSO.

Close discards consumption/queued maps, does not unregister callbacks, close
Image/Session, wait for quiescence or invalidate copied maps. Close is idempotent
after the handle is consumed. It consumes local ownership before a release
attempt, including a failed or malformed release, so Drop never retries an
uncertain release. This can leak state for a hostile/uncertain native release;
retrying could double-free. The actual facade clears consumed handles. Drop is
allocation-free, non-panicking, best-effort cleanup. Native physical teardown
still owns callback quiescence and safe provider unload.

## Queue, counters and memory boundaries

The native bounded FIFO is DROP-INCOMING. Full valid callbacks increment
events_dropped_queue_full without snapshot allocation; null/invalid callbacks
increment malformed_or_unsupported even when full. Allocation failures reject
the whole callback and permit recovery. Closing a consumer leaves registered
callbacks safe; they may still count as received without publishing new maps.

The existing `ImageMetadataCounters` is reused because its three native u64
fields and saturating semantics are identical: events_received,
events_dropped_queue_full and malformed_or_unsupported. These are callback
activity snapshots, not Rust Receive counts. Rust does not narrow or recompute
them. Full-width values are checked through injected native counter output.

The existing checked-span and fallible-copy helpers validate non-null successful
events, non-null/aligned outer views, nonempty span pointers, alignment and the
isize::MAX byte-size bound before dereference. Empty spans may have null pointers.
Both spans are checked before copying either. Allocation failure while copying
either vector returns InternalError, with no partial map. Malformed native
structure is ProtocolInconsistency; invalid diagnostic UTF-8 is InvalidUtf8;
other native status categories remain distinct. View/Close failures cannot
publish a map. No native pointer or borrowed slice is public.

Structural checks do **not** prove that arbitrary addresses are mapped or have
valid provenance. The native facade remains the trusted memory-provenance
boundary. This is not a blanket memory-safety claim about an arbitrary malicious
DSO. Abort/OOM outside fallible vector reservations is not recoverable unwind.

## Independent test evidence

Integration tests use existing compiled mock fixtures and subprocess-isolated
global state, with dynamically resolved `mock_odm_deliver` and
`mock_odm_registrations`. No production Rust dependency on these hooks exists.
Independent expected maps compare every field and element: empty, rich,
FRD-only, RowCol-only, nonfinite, tagged, high unsigned, unequal lengths,
duplicates, signed zeros, subnormals and double extremes. Copied maps are checked
again after logged provider unload.

Scenarios cover odm-sync, odm-running, odm-capacity, all four odm-register
failures, unsupported ordinary providers, odm-late, odm-late-closed,
odm-detach-fail and odm-in-flight. Tests cover base/ODM ordering, NUC/ODM
independence, capacity-one/drop-incoming/null accounting, native snapshot
allocation failure/recovery, active-empty poll/timed timeout, Stop/drain,
parent-first and Session-first teardown, consumer Close/Drop, late delivery and
in-flight physical teardown. Native test probes establish barrier ordering, not
sleeps. Safe owners never cross threads; an observer releases the native test
barrier while Close remains on the owning thread.

Unit tests inject View/Close operations and hostile C records, proving exact
one-attempt cleanup on null/misaligned/impossible records, malformed diagnostics,
error outputs, conversion failure/unwind and uncertain release. A test-only
thread-local one-shot allocator rejects the exact reservation layout for each
collection, verifying real fallible-copy failure and recovery without modifying
production helpers or native code. Native C11 contracts independently qualify
the CameraCommandResp/quaternion registration independence and counter saturation
implementation that safe Rust only observes.

## Original measured local qualification

Commands are run from the dedicated worktree. Detailed results and source
identities are also recorded in the delivery PR. The existing native contracts
pass 472/472; the explicit `^ir_image_odm` selection passes 3/3. Production build
and build-tree isolation pass. Focused Rust tests pass with 1, 4 and 16 test
threads (11 unit tests and 22 scenario tests each; the ignored child entry is
explicitly executed by every scenario, not a skipped qualification).

Workspace Cargo tests pass **316 tests**, including 20 doctests (18 compile-fail
ownership checks and two compiled examples). Seven ignored subprocess entry
functions are explicitly run by their parent scenario tests. Cargo check, Clippy
(`-D warnings`), formatting check and rustdoc (`RUSTDOCFLAGS='-D warnings'`) pass.
Python qualification passes 406 tests; policy qualification passes 234.
`alr -n -C ada build` and `alr -n -C ada/tests run` pass, with 44 smoke PASS reports,
including the original ODM contract. Native CTest executables were then restored
by `make test-native`: 472 passed, zero failed or Not Run; the explicit ODM
selection passes 3/3 again.

An independent Cargo consumer builds against `native/build/lib`, receives the
empty/rich maps, explicitly closes all owners and checks the surviving value.
Running with `LD_LIBRARY_PATH` naming the production directory succeeds; `ldd`
confirms that DSO, and `nm -D` shows all six ODM imports with no ams_mel_test_ import.
The consumer initially needed this explicit runtime library search path: a
dependency crate's link arguments do not give the downstream binary an RPATH.
No build-script change was made. At the original baseline, the production symbol
inventory was identical before/after (284 function exports), and
native/raw/Ada/Python sources were byte-unchanged. Integrity, newline and
whitespace checks pass. Hosted exact-head
acceptance and final source identities are recorded in the PR; a passing earlier
source is not a substitute for final qualification.

### Compiled mutation campaign

Each mutation is independently applied, compiled with Cargo --no-run, then
executed against focused unit and integration assertions. Original source is
restored after each experiment. Compilation failures never count as kills.

| Mutation | Runtime detector |
| --- | --- |
| FRD final-element omission | independent fixture lengths/components |
| RowCol final-element omission | independent fixture lengths/components |
| Forward/right swap | rich/FRD-only/tagged component assertions |
| Row/column swap | rich/RowCol-only/tagged component assertions |
| down_max from right_max | independent extrema assertions |
| element count treated as byte size | span lengths and fixture assertions |
| omit event release on conversion error | injected Close count |
| remove event Drop cleanup | failure/unwind Close count |
| remove stream Drop cleanup | unique-owner Drop Close count |
| fabricate Return on unknown registration | sentinel/registration assertions |

Measured campaign: **10 applied, 10 compiled, 10 executed, 10 killed, 0 invalid,
0 surviving**. None is credited solely for compiler rejection. No mutation is
left in production source.

## RUST-IR-006R1 reconciliation

An ordinary merge incorporates main `c1f916cb09eafd27a939ccbba4ee478d7aa34597`
into the existing feature branch, retaining original feature commit
`d3be5368f04f2483ef11c6bc8b61bf1cc28d88cf` as an ancestor. The original starting
main/tree above remain historical source identifiers. Main now includes PR #173's
safe Rust C2 metadata, Python C2 metadata and the native Scheduling slices.
The C2 module/reexports, ordinary Image metadata, NUC and ODM APIs coexist.

`docs/coverage.md` is retained byte-for-byte from incorporated main: its historical
archive policy prohibits the feature section originally prepended by this PR.
This task document retains the ODM coverage delta and implementation boundaries.
The current production library has **295 function exports**, verified with
`nm -D --defined-only native/build/lib/libams_mel_c.so.0` against every function
in main's `native/src/exports.map`. All six ODM functions remain present; this
reconciliation adds no native ABI exports or raw bindings.

The first merged Rust unit-test build exposed two `#[global_allocator]`
declarations: C2's allocation-count injector and ODM's allocation-size injector.
The corrective change moves both thread-local controls into one private
`#[cfg(test)]` allocator module, retaining the existing allocation-failure
assertions in both modules. Three additional tests check selective/one-shot size
failure, one-shot countdown failure and thread-local isolation. Production ODM
code is byte-identical to the original feature; production C2 code is
byte-identical to incorporated main. Native, raw Rust, provider, Ada, Python and
workflow files have no feature delta against incorporated main.

Reconciled qualification completed before the merge commit:

- `make native` and `make test-native`: production build and **523/523** native
  tests pass. Explicit `ctest --test-dir native/build-tests -R '^ir_image_odm'
  --output-on-failure` passes **3/3**; the same selection with `--parallel 3
  --repeat until-fail:50` passes every repetition.
- Focused Cargo tests at `--test-threads=1`, `4` and `16`: **11 ODM unit tests**,
  **22 ODM scenarios**, **15 C2 unit tests** and **3 allocator tests** pass each
  time. The ignored subprocess entry is executed by its parent scenarios.
- `cargo test --manifest-path rust/Cargo.toml --workspace`: **344 passed**,
  including **22 doctests**; eight ignored subprocess entry functions are
  exercised by parent tests. `cargo check --manifest-path rust/Cargo.toml
  --workspace`, `cargo clippy --manifest-path rust/Cargo.toml --workspace
  --all-targets -- -D warnings`, `RUSTDOCFLAGS='-D warnings' cargo doc
  --manifest-path rust/Cargo.toml --workspace --no-deps` and `cargo fmt
  --manifest-path rust/Cargo.toml --all -- --check` pass.
- Python regression discovery with `PYTHONPATH=python`, the explicit test facade
  and provider directory, and `python3 -W error -m unittest discover -s
  python/tests -v`: **456 passed**. Package compileall passes.
- `make test-policy`: **234 passed**. Dependency integrity verifies **824 vendor
  files / 3 workflows**. IR API coverage reproduction and **52 audit tests** pass.
- `make format-ada`, `alr -n -C ada build` and `alr -n -C ada/tests run` pass;
  the smoke executable emits **46 PASS reports**, including ODM and Scheduling.
  Formatting changes no Ada source bytes relative to incorporated main.

Build-isolation/final validation, the allocator-sensitive compiled mutation
rerun, the downstream production consumer and current-head hosted acceptance
are recorded in the delivery PR. Original qualification and mutation evidence above are not
substitutes for the new qualification. The allocator correction requires
independent review; a local review-client attempt failed authentication with
`OAuth session expired and could not be refreshed`. No independent review or
approval is claimed on that basis, and merge remains gated on obtaining one.

## Remaining gates and limitations

The non-draft PR remains open for independent review with auto-merge disabled.
All nine exact-head Bootstrap jobs and the applicable Documentation build are
required; native 50-repeat stress and existing policy gates are unchanged.
Incomplete hosted checks are HOSTED_CI_PENDING, not acceptance. No real hardware
or Squall testing, new native/raw/Ada/Python behavior, acquisition automation,
physical coordinate interpretation or callback unregistration is provided.
