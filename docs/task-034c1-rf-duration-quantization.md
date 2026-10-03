# Task 034C1 — RF duration quantization

Starting `origin/main`: `a4341461b30e45937b8ad68ec4674cbd7fba8c73`
(merged PR #62). Baseline: 250 native tests, 135 production exports, 110 Python
tests, 803 vendored files/checksums. No vendor blobs or inventory were changed.

Pinned RF MEL `762ce84c5555dd0f3ea66f36b321fecf8839b89f` defines
`Femtoseconds` as `std::chrono::duration<int64_t, std::femto>`. Compile-time
checks require a signed integral `rep` identical to `int64_t` and of equal
size. The C ABI 0.1 adds exactly one export,
`ams_mel_rf_data_quantize_duration(const ams_mel_rf_data *, int64_t, int64_t *,
char *, size_t, size_t *)`. Counts are signed femtoseconds without conversion
or rounding by the bridge. The output is untouched on every failure. Invalid
arguments are rejected before provider invocation. Bridge/provider allocation
failure maps to `AMS_MEL_INTERNAL_ERROR`; other standard or unknown provider
exceptions map to `AMS_MEL_PROVIDER_EXCEPTION` with UTF-8 diagnostic or
deterministic fallback.

This is a **live** call on the open DataMEL's published `getRFMFAInfo()`
reference, followed by exactly one `quantizeDuration(Femtoseconds{input})`.
The provider's result is authoritative: `schedulerResolution` in an immutable
MFA snapshot is static evidence, not a substitute for runtime quantization.
No snapshot owner or child claim is created. As with other same-owner DataMEL
calls, the caller externally serializes this query with DataMEL Close and
other operations on the same public owner; no mutex spans the provider call.

Safe Ada `AMS.MEL.RF.Quantize_Duration (Data : Data_MEL; Femtoseconds :
Interfaces.Integer_64) return Interfaces.Integer_64` returns the exact result
or raises `Provider_Error`, including for a closed owner. The separate mock
provider's dedicated quantization scenarios truncate signed division by ten
toward zero: 0/1/9 → 0, 10/19 → 10, -1/-9 → 0, -10/-19 → -10.
The dedicated mock scheduler quantum is 10 fs (not a bridge-side rounding
algorithm). Native C11 checks exact call counts and untouched output on
invalid arguments, standard/unknown provider exceptions and allocation
failure. Pinned
Squall `b1015728f904c799fa0c07489fce48e78f67845f` returns its input
unchanged; C and Ada smoke paths verify representative signed inputs. This
identity is **provider-specific**, not a bridge contract.

The new binding is present in raw Ada, Rust sys, and private Python inventories;
no safe Rust or public Python function is added. JobInterval, PhysicalData,
RDMA and VADB remain outside this task. The operation is the timing primitive
for future JobInterval work.
