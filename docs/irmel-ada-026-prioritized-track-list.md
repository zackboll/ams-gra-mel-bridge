# IRMEL-ADA-026 — safe Scheduling prioritized track list submission

## Source and scope

Base: fetched `origin/main` commit `3b28def52d98aa1db33794d63fbd560ad77cf220`,
tree `3789f74e34a2894d83cdd40318c2b4c8b5e64787`. PR #248 was merged at
`11ae09639686625564d16180d30a454898d4a7f6`, an ancestor of the base. Searches
for `PrioritizedTrackList` and `IRMEL-ADA-026` found no overlapping PRs;
local and remote prioritized/ADA-026 branch searches found none. No vendor
files or historical `docs/coverage.md` prose changed.

| Pin | Commit | Tree |
|---|---|---|
| IR MEL | `8d9224519f12b44e0b28815755c56a32a28d24a0` | `bfc72a6a2e1929db9a90c0af78328085fb0ae3ec` |
| Common MEL | `f6908437d8fd2f7fb69896f9eb9cfd272d10c439` | `f1f146d79f03a2ca0a59412aaf3d2c6d7dfba40f` |
| AMS Math | `00be45190f0e47d268cece8b8c2f8fb58b5418d2` | `7101189c30017cc25f277db39866156e4ab5cf38` |

Inspected vendored blobs: `SchedulingChannel.h` (`f11932ac09f2644aa47b00ed7623bb3b8ca9ddf2`),
`PrioritizedTrackListCmd.h` (`d8ff82d65f0beca36c8d468cc6cde64175d886ae`),
`TrackData.h` (`d019a525a56b62b820bdc61729c3e3e10be3d6a8`),
`TrackListResponse.h` (`39172d49d3dbbeb85cdb330d792e87b3c563b8d5`),
`UpdateTrackListResponse.h` (`ac4c7f5be4444192e0330d26f685b5713ad0939b`),
`CommonIR_MEL.h` (`0fc9bd51b6b4b08b58be85b65cd925d7d133f3a3`),
`RangeAzEl.h` (`3869b8bc16c5d8947f4834ea2fb20034071dbe38`),
`CommonMEL.h` (`54f591b39e887cddcd138c037eef000661b266be`),
`ErrorOr.h` (`d2b7f1b5768dae0b449ce59efd65a43d4661b45c`) and
`Error.h` (`c85a428e82dfda16b0e22f92e29088f095a9114f`).

## Command fidelity and ABI

`TrackTaskData` is the earlier fixed `TrackTaskParam` element with an ID and
two `AzEl` values. It is not `TrackData`, which has thirteen getters and two
full `RangeAzEl` values. Each has independent range, azimuth, elevation and
range rate. The converter sets all thirteen fields through published setters,
builds `PrioritizedTrackListCmd` with its published constructor, then verifies
all five getters and every nested track getter (binary64 components by bit
comparison). No scheduling policy, ordering, deduplication, time conversions
or association is imposed. The pinned Task values 0–16 retain their meaning
(including reserved values); 17 (`NumTaskTypes`) and unknown values reject.

The upstream constructor and getters store `TrackCount` and `std::vector`
length independently. Ada uses an owned, discriminated record with a
one-based `Track_Data_Array`, including length zero, and an independent
unsigned `Track_Count`. There is no 255-entry limit. Ada marshals into
temporary native-layout heap storage; C++ checks the span domain, safe byte
arithmetic, vector limits, Boolean and Task domains, then copies into its own
vector before provider dispatch. Borrowed C input must be readable and stable
through synchronous submission and is never retained after it. Allocation
failure during preparation is distinct from provider rejection.

Three additive production exports; symbol version remains `AMS_MEL_0.1`:

- `ams_mel_ir_scheduling_send_prioritized_tracks`
- `ams_mel_ir_track_list_request_wait`
- `ams_mel_ir_track_list_request_close`

| LP64 C record (`ams_mel_ir_…_v1`) | Size / alignment | Field offsets, in order |
|---|---:|---|
| `range_az_el` | 32 / 8 | 0, 8, 16, 24 |
| `track_data` | 112 / 8 | 0, 8, 12, 16, 20, 24, 28, 32, 36, 40, 72, 104, 108 |
| `track_data_span` | 16 / 8 | 0, 8 |
| `prioritized_track_list_cmd` | 40 / 8 | 0, 8, 16, 20, 24 |
| `track_list_result` | 8 / 4 | 0, 4 |

Also added: the distinct opaque request and uint32 response discriminator
with its four constants. Python ctypes and Rust sys mirror the ABI only; no
new safe APIs in either language. Strict C11 probes check types, sizes,
alignments, offsets and all three function signatures.

## Typed completion and ownership

`RequestFor<T>` is `std::future<ErrorOr<std::shared_ptr<T>>>`. This request is
instantiated with `TrackListResponse`, not with `Return`. `None`, `Rejected`,
`RejectedExpired` and `Accepted` are all successful future payloads (C
`AMS_MEL_OK`, Ada `Channel.Success`); `Status` reports `Channel.Rejected`
only for a valid MEL error. MEL `Error` stores its description as `std::string`.
For this request's rejected Wait, `diagnostic_required` gives the byte count
including one extra terminator, so Ada can preserve even embedded NUL and
non-UTF-8 bytes. Other diagnostic paths, including Return and Comms, keep
their existing UTF-8 contract. Validated rejection bytes and error code are
cached; a larger diagnostic buffer retrieves the same terminal result without
another provider call. Null/unowned success, unknown enum or error code,
invalid future and future exceptions are failures, never synthetic receipts.

The existing `submit_common` acquires the Session completion permit and
Enabled-only Scheduling claim and establishes a gated worker before the
provider call. The provider call runs unlocked. The resulting typed future
is moved into the pre-established worker, even if public observer publication
then fails. The enum completion worker shares the Return engine's get,
validation, exception, claim release, terminal cache and conservative
retention logic. Wait takes a snapshot of the cache; it does not consume the
future. Observer Close/finalization releases observation, not the work.
Serialize Wait vs observer Close and channel Close vs the pre-admission portion
of submission. After admission, the worker holds the graph across channel
and Session closure. Provider results and the future are retired while their
code is still retained. Uncertain detach conservatively retains the graph;
safe retirement allows eventual provider unload.

`TrackListResponse` is the receipt/status carried by the future.
`UpdateTrackListResponse` is a separately registered metadata payload with a
command ID and receipt field. This slice does not register, synthesize or
correlate it. Receipt acceptance is not evidence of physical tracking or
schedule execution. Qualification uses the mock only.

## Coverage

`send(PrioritizedTrackListCmd)`, `PrioritizedTrackListCmd`, `TrackData` and
`TrackListResponse`: `missing` → `safe_ada`. Unique safe Ada operations:
61 → 62. Missing Scheduling operations: 5 → 4. Scheduling remains `partial`.
`UpdateTrackListResponse` and its callback remain `missing`. The pinned
catalog, inherited coverage and metadata callbacks are unchanged.

## Public-only example

After building the Ada contract tests, run the standalone example with the
mock provider:

```sh
ada/tests/bin/ams_mel_ir_prioritized_example \
  "$PWD/native/build-tests/test-providers/libmock_ir_provider.so"
```

The example deliberately uses different vector length and `Track_Count`,
closes channel and Session before waiting, and checks the exact receipt.

## Local qualification

The following completed against the mock-provider build. Ignored Rust cases
are not counted as passes (they include subprocess entry points and opt-in
reference-provider checks).

| Command or evidence | Result |
|---|---|
| `make format-ada`; `make check-ada-format`; `make check-ada-docs-changed` | Passed; 14 checked public declarations, no missing annotations. |
| `make test-native` | 978 passed, including 25 prioritized-list cases (layout and conversion included) and the existing Scheduling, Return and Comms regressions. |
| `make test-ada` | Passed; the smoke suite includes exact receipts, MEL error bytes, timeout, finalization and admission exhaustion, and the public-only example ran. |
| `make test-python` | 796 passed, including the compiled C11 ctypes layout/signature and complete export checks. |
| `make test-rust` | 643 passed (including doctests), 0 failed, 15 ignored. The new raw sys layout/signature test passed. |
| `make check-ir-api-coverage` | 82 regression cases passed; 127 headers and 148 operations audited. |
| `make check` | Passed; production/test build-tree isolation, 978 native cases, Ada formatting and smoke tests, and text hygiene. |
| `alr -n -C ada build` | Passed with GNAT 16.1.0 / GPRbuild 26.0.1. |
| `alr -n -C ada/tests run` | Passed; built and ran the Ada smoke suite, including the typed prioritized-list tests. |
| Standalone Ada example | Passed against the production facade and mock provider; reported the exact Accepted receipt. |
| GCC 14.2 and Clang 19.1 strict C11/C++20 probes | Passed with `-Wall -Wextra -Wpedantic -Werror` (C additionally used `-pedantic-errors`). The C++ probe covers each binary64 component and all 17 valid task values. |
| `ctest --repeat until-fail:50` | 300 executions across six deterministic reentry, admission, publication, observer-close, concurrent-close and detach-retention cases. |
| Clang ASan/UBSan with leak detection | 35 passed: the 25 prioritized-list cases, two TrackTask lifetime cases and eight public C2/Scheduling common-channel controls. |
| Production `nm -D --defined-only` vs export map and base | Exactly 338 `AMS_MEL_0.1` exports vs 335 at base: the three listed above, no test symbols or removals. |
| `python3 scripts/check_doc_source_integrity.py`; `git diff --check` | Passed; 825 vendor files and seven workflows checked. |
| Rust workspace `cargo clippy --all-targets -- -D warnings` and `cargo fmt --all -- --check` | Passed. |

Earlier iterations caught missing Ada operator visibility, stale export and
coverage expectations, and a stale mock release-family bound. A Clang
sanitizer build initially failed to link until the C test executable was linked
with the C++ UBSan runtime and `libstdc++`. These attempts are not passes.

Hosted CI and independent technical approval are separate gates. Review
should verify field and span fidelity; independent count and length; typed
response versus MEL error; shared completion; and reentrant cleanup, provider
unload and regression isolation. No hardware or real-provider evidence is
claimed.
