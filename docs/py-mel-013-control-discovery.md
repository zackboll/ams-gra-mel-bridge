# PY-MEL-013 — Safe Python Control capability discovery

## Source and scope

Original freshly fetched `main`: `0e4dbd8491ccd13d7dfadbd6ad36766687f5f4ab`,
tree `a6c579aa1f9d71f40971a7e556da8da210f1ba5f`.
PY-MEL-012 merged as PR #192 (`02cdaf61de750a80c461a849a3df4f5e66e5a222`);
IRMEL-ADA-014 merged as PR #183
(`3bc834fee062ae0fa812cb2d7d886a6157ecfd11`). Both are ancestors of the base.
No open PR, including overlapping Python discovery work, existed at preflight.
Dedicated worktree: `/home/zboll/git/ams-py-mel-013`; branch:
`feature/py-mel-013-control-discovery`.

This adds exactly `Session.capabilities() -> tuple[ChannelCapability, ...]`.
It exposes the existing published `Control::getCapabilities() const` operation,
catalog ID `ir:operation:ams::iface::irmel::Control::getCapabilities() const`,
from IR pin `8d9224519f12b44e0b28815755c56a32a28d24a0`. The existing Ada
structured inventory already records native/Ada discovery; no Ada coverage or
reviewed ceiling changes. There is no new public class, ABI, ctypes declaration,
native implementation, provider behavior, or historical coverage update.

## API and executable example

The Session must be open. Results preserve provider enumeration order, including
an empty tuple and unconfigured zero image layouts. No channel family is assumed.
Every returned object is the existing public frozen `ams_mel.ChannelCapability`.
All nested strings, UUID bytes, tuples, enums and frozen dataclasses are Python
owned; no ctypes value, borrowed pointer or native owner is returned.

Save this example as a Python script and pass a provider DSO and instance name:

```python
import sys
from ams_mel import ChannelType, Session

def main():
    provider_path, instance_name = sys.argv[1:]
    with Session.open(provider_path, instance_name, "") as session:
        advertised = session.capabilities()
    # Selection is a value-only application decision, not attachment.
    images = tuple(cap for cap in advertised
                   if ChannelType.IRST_IMAGE in cap.channel_types)
    for capability in images:
        print(capability.channel_id.descriptive_label)

if __name__ == "__main__":
    main()
```

Discovery never attaches/enables a channel, registers callbacks or metadata,
allocates acquisition buffers, or starts acquisition. Advertisement does not
establish operational sensor support, physical readiness or successful future
attachment. Zero-valued fields remain zero: they do not imply Mono8, usable
geometry, or any radiometric interpretation. Existing attached-channel
`.channel_view().capabilities()`, Health and Instrumentation APIs are unchanged.

**Concurrency:** externally serialize discovery against Session closure and
conflicting provider operations. ctypes can release the GIL; the GIL is not
sufficient synchronization. No threads, locks, background cleanup or retries
are added.

## Native mapping and ownership map

`Session.capabilities` checks openness and delegates to private
`python/ams_mel/_discovery.py`. It uses the already-declared native functions:

| Step | Native operation | Ownership |
| --- | --- | --- |
| Create once | `ams_mel_ir_capability_list_create` | Initially-null list output, guarded before acquisition |
| Count once | `ams_mel_ir_capability_list_count` | List remains alive; zero is successful |
| Get in zero-based order | `ams_mel_ir_capability_list_get` | Separate initially-null entry output for each index |
| Borrow | `ams_mel_ir_channel_capability_view` | Non-null, aligned record required while entry lives |
| Deep copy | `_ffi._capability_from_raw` | Existing single complete field mapping, no duplicate converter |
| Release entry | `ams_mel_ir_channel_capability_close` | One attempt after full copy, or during failure unwinding |
| Release list | `ams_mel_ir_capability_list_close` | One attempt before public return, including empty result |

Native lists contain independently owned bridge snapshots, not provider graph
references. Retrieved entry owners independently retain the snapshot. Python
does not depend on those lifetimes after copying: every entry and the list is
closed before the method returns. Repeated calls create distinct complete value
graphs without mutating previous results or retaining prior owners.

The lexical `_temporary_owner` helper has no finalizer. In particular it does
**not** reuse `_CapabilitySnapshot`, whose existing attached-channel abandon/
finalization behavior can retry an ownership-uncertain Close. Existing behavior
is left untouched. Each discovery owner gets at most one native Close attempt,
even if a failing status leaves a stale non-null slot after consuming ownership.
An unexpectedly published output on acquisition failure is still cleaned up.
Cleanup exceptions never replace the original failure; without an earlier
failure, entry/list Close errors propagate instead of returning a partial result.
If diagnostic allocation fails before Close is attempted, a diagnostic-free
Close is attempted once, while the allocation exception is preserved. No native
Close is retried after its invocation begins. An ownership-uncertain broken ABI
can therefore leak its owner; guessing and retrying would risk a double free.

Counts exceeding `sys.maxsize // sizeof(py_object)` are rejected before iteration
as protocol inconsistency. This is a representability check, not an arbitrary
provider-size limit. Representable collections may still exhaust Python memory;
allocation/conversion exceptions unwind all acquired owners. As elsewhere in the
binding, pointer/length/alignment checks cannot prove arbitrary non-null native
addresses are mapped memory. Conforming live native views remain an ABI premise.

## Complete conversion evidence

`_ffi._capability_from_raw` and its existing checked string/span/band helpers
copy all **19** fields; `_ffi.py` and `_types.py` are unchanged:

| Python fields | Preservation and independent regression evidence |
| --- | --- |
| `channel_id` | All 16 bytes (`0x10 + entry + 7 * byte_index`), exact `discovery-N-α` label |
| `height`, `width`, `bit_depth` | Rich: 1080, 1920, 12; unconfigured: all zero |
| `row_pitch`, `buffer_size`, `image_size` | 4096, 8388608, 4147200; unconfigured: zero |
| `number_of_bands`, `pixel_format` | 3 and RGB in rich; no band-count/geometry inference |
| `sensor_types` | Ordered GIMBAL_HORIZONTAL, STEP_STARE |
| `platform_id` | All 16 bytes from seed `0x31`, exact `platform-€` label |
| `sensor_location` | 1.25, -2.5, 3.75 metres, `sensor-key`, `system-β` |
| `channel_types` | Ordered C2/Health/IRSTImage/StackedImage entries; each rich entry additionally Instrumentation, Reserved2 |
| `task_schedule_depth` | 17 unchanged, not a hardware readiness predicate |
| `odc_available`, `nuc_available` | Both true in rich; existing strict 0/1 validation retained |
| `metadata_capabilities` | Ordered numeric 0, 9, 17, Reserved10, from native set enumeration |
| `image_bands` | Indices 2 and 9; independent nested lengths 2 and 1; longwave 8–12 µm, midwave 3–5 µm, red 620–750 nm stored in metres |
| `nav_frames` | Ordered NED_SENSOR, ECEF |

Expected values are literals from `native/tests/test_ir_discovery.c` and
`rich_channel_capability` in `native/tests/mock_provider.cpp`, not generated from
the Python converter. Three advertised bands, two image-band descriptions and
their independently sized spectral lists stay distinct. Native map/set ordering
is preserved, not re-sorted by Python. Injection into actual live mock-owned
records additionally verifies duplicate vector elements, unknown uint32 enums
using existing `UNKNOWN_0x...` conventions, signed zero, infinities and NaN
without normalization. No NaN payload-bit portability guarantee is added.

## Qualification evidence

The original package was reconstructed from base Git objects in a temporary
directory and imported against the compiled façade: `Session.capabilities` was
absent (baseline-negative pass). The new focused suite passes **23 test methods**:
7 integration/lifecycle methods and 16 hostile-boundary methods, including
parameterized failures at every acquisition/view/count/conversion/Close boundary.
Five provider failure scenarios preserve exact structured categories, native
statuses and complete diagnostics. Hostile tests use actual mock-owned handles
and live record storage; never fabricated invalid dereferences. Tests rescue
deliberately unconsumed owners outside the operation after asserting no retry.

The isolated unload test runs with warnings treated as errors, observes
`library_unloaded` in the existing lifetime log, and independently requires
`dlopen(RTLD_NOW | RTLD_NOLOAD)` to return null after Session Close. Only then
does it inspect the entire graph, compare every field to independent expectations,
and pickle/unpickle it. The exact lifecycle contains no attachment, callback or
acquisition activity. Discovery mock scenarios abort if attachment is attempted.
Call-count tests require all entry/list Close calls before public return.

Pre-commit focused plus compatibility regression command (208 methods passed):

```sh
cd /home/zboll/git/ams-py-mel-013
export PYTHONPATH="$PWD/python:$PWD/python/tests"
export AMS_MEL_NATIVE_LIB="$PWD/native/build-tests/lib/libams_mel_c.so.0"
export AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers"
python3 -W error -m unittest -v test_ir_discovery test_common_channel \
  test_ir_health test_instrumentation test_session test_abi \
  test_import_identity test_package_layout
```

This includes existing attached common-channel/Health/Instrumentation capability
behavior, Session lifecycle, C11 ABI/layout probes, original public API compatibility,
module identities and pickling. Initial focused qualification caught incorrect
test-only numeric status expectations (PROVIDER_FAILED=11, INTERNAL_ERROR=8);
the independent expectations were corrected to the existing C ABI constants.
No production contract was weakened.

Pre-commit qualification passed `make test-native` (558/558), `make test-python`
(560 methods before the final tuple-allocation test was added), `make test-policy`,
`make check-ir-api-coverage` (60 regression controls), `make test-build-isolation`,
source-integrity checks, compilation and whitespace checks. The repository-wide
newline check identified two **inherited base-main failures**:
`docs/mel-gnatdoc-015-001-official-release.md` and
`docs/tests/test_enhancer_release.py`. Reading both directly from the original
base Git objects confirmed that neither ends in a newline. They are unrelated
to this Python task and are intentionally not modified. This blocks a claim of
complete qualification until independently corrected upstream and reconciled.
All newly added files have final newlines.

Final committed-source qualification and exact HEAD/tree identities are recorded
in the PR handoff (a commit cannot embed its own SHA). Required commands run
sequentially, never overlapping native builds sharing output directories:

```sh
make test-native
make test-python
make test-policy
make check-ir-api-coverage
make test-build-isolation
python3 scripts/check_doc_source_integrity.py
python3 scripts/check_final_newlines.py
python3 -W error -m compileall -q python/ams_mel python/tests
git diff --check
```

Local `gprbuild` is unavailable on PATH (an auxiliary SPARK installation was
found outside PATH), so no local Ada success is claimed;
hosted Ada qualification must be reported separately for the final PR source.
While hosted required checks are incomplete, delivery is `HOSTED_CI_PENDING`,
not `READY_FOR_INDEPENDENT_REVIEW`. Independent technical review remains required;
no self-approval, merge or auto-merge is authorized.

## Limitations

All integration evidence is from the existing compiled native mock provider,
not Squall, a real sensor, credentials, containers or hardware. No StackedImage
attachment, generic attachment, acquisition/buffer or metadata registration,
capability-to-configuration policy, camera-specific discovery, hardware probing
or background workers are implemented. Advertised channel families need not
be attachable by this Python binding. Existing native validation and upstream
unit/physical-meaning limitations remain unchanged; no unresolved upstream
semantics were guessed to complete value-only discovery.
