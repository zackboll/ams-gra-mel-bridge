# IRMEL-ADA-009 — Image OpticalDistortionMap metadata qualification

## Scope and upstream mapping

Base: `552923e4e2783833680c37455b350e03a069bb96` (fetched `origin/main`, PR #168;
equal to the verified starting main). One optional Image metadata vertical slice;
qualification uses the separate mock provider only.

| Pinned upstream operation | Native C ABI | Safe Ada |
|---|---|---|
| `ImageChannel::registerMetadataCallback(std::function<void(Channel&, const OpticalDistortionMap* const)>)` (`@Optional`) | `ams_mel_ir_image_odm_open`, `_receive`, `_get_counters`, `_close`, `_event_view`, `_event_close` | `AMS.MEL.IR.Image.Metadata.Open_Optical_Distortion_Map`, `Receive_Optical_Distortion_Map`, `Optical_Distortion_Map_Counters`, `Close` (`Optical_Distortion_Map_Stream`) |

| Upstream getter | Type | C field | Ada accessor |
|---|---|---|---|
| `getWidth`, `getHeight` | `uint32_t` | `width`, `height` | `Width`, `Height` |
| `getRightSize`, `getDownSize` | `uint32_t` | `right_size`, `down_size` | `Right_Size`, `Down_Size` |
| `getPixelsPerOdm` | `uint32_t` | `pixels_per_odm` | `Pixels_Per_ODM` |
| `getRightMin`, `getRightMax` | `double` | `right_min`, `right_max` | `Right_Min`, `Right_Max` |
| `getDownMin`, `getDownMax` | `double` | `down_min`, `down_max` | `Down_Min`, `Down_Max` |
| `getFrd` | `vector<ForwardRightDown>` | `frd` span of `ams_mel_ir_forward_right_down_v1` | `FRD_Count`, `FRD_At` |
| `getRowCol` | `vector<RowCol>` | `row_col` span of canonical `ams_mel_ir_row_col_v1` | `RowCol_Count`, `RowCol_At` |
| `ForwardRightDown::getForward/getRight/getDown` | `double` ×3 | `forward`, `right`, `down` | `Forward_Right_Down` record |
| `RowCol::getRow/getCol` | `double` ×2 | `row`, `column` | `Row_Column` record |

Only the upstream getters are read; no provider object layout is assumed (native
static assertions pin every getter type).

## Representation and ownership

- The five `uint32` values are copied unchanged; 0 and UINT32_MAX are tested.
- Both collections are copied completely, in provider order, with duplicates kept.
  Their lengths are independent of each other and of width/height/right_size/
  down_size/pixels_per_odm. No equality or product relation is assumed or enforced,
  and nothing is inferred, padded or truncated. Empty collections are `{NULL, 0}`.
- **Non-finite doubles.** Every double is forwarded by plain IEEE assignment.
  Finite, negative, subnormal, extreme and signed-zero values are bit-exact.
  Infinity and NaN are never clamped, zeroed or replaced, and they keep their class
  and sign. NaN payload bits are not promised. In Ada they reach `Long_Float` as
  non-valid scalar values: the conversion and the owned storage disable only the
  optional floating validity checks, and callers must test with `'Valid` or IEEE
  comparisons before checked arithmetic.
- Native `OdmEventData` owns all scalar and vector storage. Both vectors are sized
  before any view pointer is bound, so a `bad_alloc` publishes nothing. No provider
  object, reference or pointer survives the callback. Ada `Receive` copies everything
  into an owned `Map_Value` before the native event is closed. Values are tested
  after owner, stream and Session close and after the provider library has unloaded.
- Private Ada conversion validates each span (count ≤ Integer_32'Last, non-null data
  for a nonzero count, checked byte size and address range) before reading any element.

## Callback and lifecycle evidence

- **Independent opt-in owner.** `claim_image_odm` keeps its own `image_odm_attempted`
  one-shot under the frame-callback mutex. Eligible streams are Attached or Running;
  Stopping/Stopped/Failed streams are refused before any provider call. The four-kind
  `ams_mel_ir_image_metadata_open`, NUC and CameraCommandResp owners never register
  ODM. The mock counts every ODM registration to prove this in both directions.
- **Registration** holds no bridge lock, and synchronous delivery during registration
  is exercised. Success, NotSupported (Ada `Optical_Distortion_Map.Not_Supported`),
  Fail, NotImplemented, an unknown Return and a provider exception are distinct.
  Failed attempts publish no owner, consume the one-shot and leave the retained
  callback inactive.
- **Bounded reception.** One DROP-INCOMING queue of caller-supplied capacity, with
  saturating received/queue-full/malformed counters. A shared allocation-free
  validator classifies full-queue input without building a snapshot, verified by a
  copy-attempt probe and an armed allocation failpoint that stays armed. Null and
  over-limit maps are still counted malformed. The copy path validates once.
  Allocation failure rejects the whole event, then reception recovers. The
  INT32_MAX boundary is tested with a lowered test-only limit using the same check.
- **Stop/Close.** Logical Stop moves the owner to Stopped and wakes a blocked Receive,
  ordered by a probe rather than a sleep. Queued maps drain after Stop and after
  Close, then `STREAM_STOPPED`. Public owner Close deactivates consumption only; it
  does not unregister or wait.
- **Teardown.** After provider channel destruction, `image_stream_cleanup` calls
  `drain_and_stop`, which waits for ODM callbacks already inside the bridge. An
  isolated process proves this with a non-quiescing provider thread that channel
  destruction does not join: teardown is observed waiting, Close cannot return until
  the parked callback is released, and the callback returns before `library_unloaded`.
  Late callbacks after owner/stream close are counted, never queued, and memory-safe.
- **Detach failure** follows the existing retention rules. The graph is restored for
  retry and the ODM owner is marked failed. Queued maps drain, then `PROVIDER_FAILED`.
  Image-buffer ownership, deferred cleanup, callback draining and the PR #42 post-drain
  recheck are unchanged.

## ABI and mirrors

Six additive `AMS_MEL_0.1` exports: **272 → 278**. A full `nm -D` comparison of an
isolated baseline build against this head shows no removals and exactly six
additions, all `@@AMS_MEL_0.1`, and no test hook in production. New records:
`ams_mel_ir_forward_right_down_v1`, `_span_v1`, `ams_mel_ir_row_col_span_v1` and
`ams_mel_ir_optical_distortion_map_v1` (LP64 size 88). Existing records,
signatures and enum values are unchanged. `native/tests/image_odm_layout_probe.c` (C11)
checks the signatures and records every size/offset. The private Python ctypes and
Rust sys mirrors compare against it; no safe Python or Rust ODM API exists. The
test-only `ams_mel_test_image_odm_probe` exists only in the contract-test facade.

## Coverage

Exactly three classifications change, missing → safe_ada:
`ImageChannel::registerMetadataCallback(OpticalDistortionMap)`, `OpticalDistortionMap`
(all eleven getters represented) and `ForwardRightDown` (three getters). `RowCol`
remains safe_ada with its Track evidence. Safe Ada operations go **45 → 46**. The
borrowed-evidence control moved to the still-missing `LineOfSightQuaternion`. The
vendor sources, pinned hashes, catalog anchor and every other classification are
unchanged; only `REVIEW_SHA256` is re-anchored.

## Qualification commands and results

Run from this worktree; production (`native/build`) and contract-test
(`native/build-tests`) trees stay independent, built sequentially; mutation and
baseline-export builds use isolated `/tmp` trees.

TBD_RESULTS

Not run: historical documentation replay, GNATdoc site regeneration and stress
campaigns (out of scope). No physical IR camera or Squall was used; all positive
evidence is deterministic mock-provider evidence.
