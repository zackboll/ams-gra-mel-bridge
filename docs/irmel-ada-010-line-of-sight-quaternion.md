# IRMEL-ADA-010 — Image LineOfSightQuaternion metadata qualification

## Scope and upstream mapping

Base: `988105bc032bb9b5c59d1878d4433b5e1c2cea90` (fetched `origin/main`, PR #169; tree
`fc94f57b2f262f520cb77fe2b00a7a91fddd65c8`; equal to the verified starting main). This is one
`@RequiredIfLOSQuaternion` Image metadata vertical slice, qualified only with the separate mock
provider.

| Pinned upstream operation | Native C ABI | Safe Ada |
|---|---|---|
| `ImageChannel::registerMetadataCallback(std::function<void(Channel&, const LineOfSightQuaternion* const)>)` | `ams_mel_ir_image_los_quaternion_open`, `_receive`, `_get_counters`, `_close`, `_event_view`, `_event_close` | `AMS.MEL.IR.Image.Metadata.Open_Line_Of_Sight_Quaternion`, `Receive_Line_Of_Sight_Quaternion`, `Line_Of_Sight_Quaternion_Counters`, `Close` (`Line_Of_Sight_Quaternion_Stream`) |

| Upstream getter | Type | C field (`ams_mel_ir_line_of_sight_quaternion_v1`) | Ada (`Line_Of_Sight_Value`) |
|---|---|---|---|
| `getSystemTime().count()` | `chrono::nanoseconds` (signed `rep`) | `system_time_ns` `int64_t` @0 | `System_Time_NS : Long_Long_Integer` |
| `getQ_xyzw().getQuaternionX/Y/Z/W` | `double` ×4 | `q_xyzw`: canonical `ams_mel_ir_quaternion_v1` `x,y,z,w` @8 | `Q_XYZW : Quaternion` (subtype of `AMS.MEL.IR.Image.Quaternion`) `X,Y,Z,W` |
| `getPqr_rps().getForward/getRight/getDown` | `double` ×3 | `pqr_rps`: canonical `ams_mel_ir_forward_right_down_v1` `forward,right,down` @40 | `PQR_RPS : Forward_Right_Down` (subtype of `Optical_Distortion_Map.Forward_Right_Down`) |

The record is 64 bytes on LP64. Native static assertions pin every getter type, the signed
`nanoseconds::rep` range ⊆ `int64_t`, and the reuse of both canonical records. The Ada
conversion pins `Long_Long_Integer` ⊇ `Integer_64` at compile time. Only the upstream getters
are read; no provider object layout is assumed.

## Representation and ownership

- **No attitude processing.** Values are copied component by component in upstream order. The
  bridge does not normalize, canonicalize signs (q and −q stay distinct), reorder to w,x,y,z,
  convert to Euler angles, convert frames, infer a time epoch or clamp rates. A non-unit
  quaternion (norm² = 348 in the fixture) is delivered as-is. The only structural rule is that
  a null payload is malformed. Successful reception does not show that an attitude is
  normalized, accurate or suitable for an independent pointing calculation.
- **Floating point.** Every double is copied by plain IEEE assignment. Finite, negative,
  subnormal (±`denorm_min`), extreme (±`DBL_MAX`) and signed-zero values are bit-exact. Infinity
  and NaN are never replaced and keep their class and sign; NaN payload bits are not promised.
  In Ada they reach `Long_Float` as non-valid scalars. Only the private conversion unit and
  `Receive_Line_Of_Sight_Quaternion` disable the optional floating validity checks
  (`pragma Validity_Checks ("F")`); range and overflow checks stay on. A client compiled with
  `-gnatVa` may raise `Constraint_Error` when it copies such a component as a scalar, so callers
  should test with `'Valid` or IEEE comparisons first. This is the same boundary IRMEL-ADA-009
  documents for ODM.
- **Ownership.** `LosQuaternionEventData` owns one fixed C record by value, with no indirection.
  The callback reads the provider payload only before it returns. The mock overwrites and then
  destroys each payload immediately after the callback, and the event still matches. Ada
  `Receive` copies into the owned `Line_Of_Sight_Value` before the native event is closed.
  Values are re-checked after owner, stream and Session close and after the provider library
  unloads.

## Callback and lifecycle evidence

- **Independent opt-in owner.** `claim_image_los_quaternion` keeps its own
  `image_los_quaternion_attempted` one-shot under the frame-callback mutex. Eligible streams are
  Attached or Running; Stopping/Stopped/Failed streams are refused before any provider call.
  The four-kind `ams_mel_ir_image_metadata_open`, NUC, CameraCommandResp and ODM owners never
  register this overload, and the quaternion owner never registers ODM. Both mock registration
  counters prove this in both directions. The base four-kind mix (6 received / 2 dropped) is
  unchanged, and quaternion delivery never reaches the base owner's counters.
- **Registration** holds no bridge lock, and synchronous delivery during registration is
  exercised. Results map to distinct outcomes: Success, NotSupported (`out_registration`
  NOT_SUPPORTED → Ada `Line_Of_Sight_Quaternion.Not_Supported`), Fail, NotImplemented
  (PROVIDER_FAILED with the mapped Return), unknown Return (PROVIDER_FAILED, Return untouched)
  and provider exception (PROVIDER_EXCEPTION). Each has its own diagnostic. Every attempt,
  including a failed one, consumes the one-shot. Failure publishes no owner and deactivates the
  retained callback state.
- **Bounded reception.** DROP-INCOMING with saturating received / queue-full / malformed
  counters. Null is malformed, including when the queue is full. A full queue counts a non-null
  payload queue-full without allocating; the armed allocation failure stays armed and the copy
  counter is unchanged. An allocation failure rejects the event atomically (malformed) and the
  next delivery recovers. Capacity 1 and capacity 3 (FIFO, two excess dropped) are both tested,
  as are a finite TIMEOUT and a blocked Receive woken by delivery.
- **Lifecycle.** A blocked Receive is woken by Image Stop (STREAM_STOPPED). Stop/Close preserve
  queued values for draining. Public owner Close only deactivates (no upstream unregister). Late
  callbacks after owner, stream or Session close are counted and never queued; ordered lifetime
  logs prove they return before `library_unloaded`. Physical teardown calls `drain_and_stop`
  after provider channel destruction. In an isolated process with a non-quiescing provider
  thread, `ams_mel_ir_stream_close` is shown to block in that drain until the parked callback is
  released (`entered → channel_destroyed → teardown waiting → released → returned → unloaded`).
  On a failed detach, the stream retains the provider graph, the owner fails, queued values still
  drain, and Receive then reports PROVIDER_FAILED. The retained callback stays memory-safe.
  Image buffer ownership, deferred cleanup and post-drain protections are untouched; the only
  stream-graph change is the three hook calls next to the existing ODM hooks.

## ABI and mirrors

Six exports extend `AMS_MEL_0.1` 278 → 284. No existing export, signature, record or enum
changed, and no duplicate quaternion or FRD record was added.
`image_los_quaternion_layout_probe.c` (C11) pins every signature via `_Generic`, fails closed on
LP64 drift, and prints every size/offset. The private Python ctypes and Rust sys mirrors
compare against it and reuse their canonical quaternion/FRD mirrors; no safe Python or Rust
quaternion API exists. The test-only `ams_mel_test_image_los_quaternion_probe` exists only in
the contract-test facade.

## Coverage

Exactly two classifications change, missing → safe_ada:
`ImageChannel::registerMetadataCallback(LineOfSightQuaternion)` and `LineOfSightQuaternion`
(all three getters represented). `Quaternion` and `ForwardRightDown` remain safe_ada with
their existing evidence. Safe Ada operations go **46 → 47**. The borrowed-evidence control moved
to the still-missing `LOS3D_KinematicsType`. Vendor sources, pinned hashes, the catalog anchor
and every other classification are unchanged; only `REVIEW_SHA256` is re-anchored.

## Qualification commands and results

All checks ran from this worktree. Production (`native/build`) and contract-test
(`native/build-tests`) trees stay independent and were built sequentially; Alire compiler
setup never overlapped a CMake build. Mutation and baseline-export builds used isolated
`/tmp` trees. Ada used the pinned Alire toolchain (`alr -C ada/tests exec -- sh
scripts/test_ada.sh`), because GPRbuild is not on the bare `PATH`.

| Check | Command / evidence | Result |
|---|---|---|
| Production native | `make native` (`-Wall -Wextra -Wpedantic -Werror`) | PASS, 0 warnings |
| Focused quaternion C11 | `ctest -R ir_image_los_quaternion` (contract, isolated in-flight teardown, layout probe) | PASS, 3/3; contract and in-flight case also 5/5 repeated runs |
| Existing Euler/LOS report, ODM/NUC/CameraCommandResp | within the full native suite (`ir_image_metadata*`, `ir_image_odm*`, `ir_nuc*`, `ir_camera_command*`) | PASS, unchanged tests |
| Full native | `make test-native` | PASS, 472/472 (469 baseline + 3) |
| Focused Ada quaternion | `ada/tests/bin/ams_mel_ir_image_los_quaternion_focused <mock>` | PASS |
| Full Ada | `gprbuild -P ada/tests/ams_mel_tests.gpr`; `ams_mel_smoke` | PASS, 44 suites (43 + quaternion); coverage audit 46 tests |
| Python | `make test-python` (incl. `test_private_image_los_quaternion_abi`, inventory 284) | PASS, 354 tests |
| Rust | `make test-rust` (incl. `image_los_quaternion_abi`, `abi` inventory 284) | PASS, 281 passed, 0 failed |
| Exports | isolated `988105b` production build vs head, `nm -D --defined-only` | PASS, 278 → 284; 0 removed; 6 added, all `@@AMS_MEL_0.1`; no unversioned or `ams_mel_test_*` symbols |
| Policy | `make test-policy` | PASS, all suites OK |
| Coverage | `make check-ir-api-coverage` | PASS, 46 tests (43 + 3); safe_ada 46 → 47 |
| Formatting | `make format-ada`; `make check-ada-format`; `cargo fmt --check`; `git diff --check`; `check_final_newlines.py` | PASS |
| Documentation | `check_doc_source_integrity.py`; pinned enhancer `test_*links.py` (29 incl. 3 new) | PASS |
| Native mutations | `python3 scripts/test_image_los_quaternion_mutations.py` | PASS, 8/8 detected; baseline restored; checkout unchanged |
| Ada mutations | isolated copy of `ada/`, focused Ada contract, forced rebuild per mutant | PASS, 4/4 detected; baseline restored; checkout unchanged |

Native mutants (each builds and fails a named release-active C11 assertion):

1. Quaternion x/w swapped — `same(v->q_xyzw.x, x)`.
2. z zeroed — `same(v->q_xyzw.z, z)`.
3. Forward/right swapped — `same(v->pqr_rps.forward, forward)`.
4. Signed time corrupted (sign bit cleared) — `v->system_time_ns == time`.
5. Quaternion registered through the base four-kind owner — `registrations() == before`.
6. Snapshot allocated despite a full queue. The armed allocation failure fires, so the valid event
   is miscounted malformed — `counters_are(m, 2, 1, 0) == EXIT_SUCCESS`.
7. Null counted queue-full instead of malformed — `counters_are(m, 3, 1, 1) == EXIT_SUCCESS`.
8. In-flight teardown drain removed — `PROBE(3U, 0U) == 1`, isolated process.

Ada conversion mutants:

- X/W swapped (`first quaternion x`).
- Z zeroed (`first quaternion z`).
- Forward/right swapped (`first forward rate`).
- Time halved (`first system time`).

An earlier mutant draft used `abs (time + 1)`. It was rejected by an Ada overflow check rather
than the named assertion, so it was replaced and not counted.

The repeated smoke runs exposed one observation. During the first full Ada run, the
pre-existing IRMEL-ADA-004 `Test_Held` raised `RESOURCE_EXHAUSTED : async request limit
reached` once. That test reuses a `Max_Async_Requests => 1` Session after an earlier request,
without completion-owner quiescence. The failure did not recur in 20 consecutive standalone
`ams_mel_smoke` runs or in the final sequential run. It is outside this slice, which adds no
asynchronous request path, and is not modified here.

Not run: historical documentation replay, GNATdoc site regeneration and stress campaigns (out
of scope). No physical IR camera or Squall was used; all positive evidence is deterministic
mock-provider evidence. `feature_source.py` requires a clean checkout and is run on the final
committed head.
