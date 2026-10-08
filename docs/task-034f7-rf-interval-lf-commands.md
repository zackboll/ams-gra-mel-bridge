# Task 034F7 — RX JobInterval Local Function command construction

## Scope and pinned contract

RF MEL `762ce84c5555dd0f3ea66f36b321fecf8839b89f` defines
`LocalFunctionAddress = uint64_t`, `LocalFunctionValue = uint64_t`, and
`LocalFunctionTypeID = uint32_t`. `LFAddressValue`, `LocalFunctionCommand`, and
`JobInterval::setLfCommands(const std::vector<LocalFunctionCommand>&)` copy values.
The pinned contract says commands execute at interval START, targeting an LF
associated with the selected VA instance; inability to execute may fail the Job.
The conditional `@RequiredIfLFSupport` boundary does not establish MFA support.

No LF owner, callback, thread, child claim, DSO pin or pointer to hardware is added.
No capability, instance-count, writable-address, register-mask, timing or sequence
relationship is validated. Type IDs and address/value pairs accept their entire
unsigned domains without masking, narrowing, endian conversion or arithmetic.
Portable uint64 instances are checked for size_t representability before conversion.

## Additive C ABI, measured LP64 layouts

All six records have alignment 8 on the measured host:

| Record | Size | Field offsets |
| --- | ---: | --- |
| `ams_mel_rf_lf_address_value_v1` | 16 | address 0, value 8 |
| `ams_mel_rf_lf_address_value_span_v1` | 16 | data 0, size 8 |
| `ams_mel_rf_lf_command_v1` | 32 | local_function_type_id 0, local_function_instance 8, address_values 16 |
| `ams_mel_rf_lf_command_span_v1` | 16 | data 0, size 8 |
| `ams_mel_rf_job_interval_config_v6` | 176 | interval 0 (frozen 160-byte v5), local_function_commands 160 |
| `ams_mel_rf_job_interval_config_span_v6` | 16 | data 0, size 8 |

The only production export added is:

```c
ams_mel_status_t ams_mel_rf_job_add_rx_intervals_v6(
    ams_mel_rf_job *job,
    ams_mel_rf_job_interval_config_span_v6 intervals,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required);
```

ABI remains 0.1. ReceiveEvent v1–v4, JobInterval v1–v5 and all older functions
are unchanged. Their LF vectors stay empty. V6 nests v5 rather than copying its
record definition. Native construction extends the existing profile-trait builder,
retaining pulse, execution, polarization, spatial and status behavior.

## Validation, ordering and lifetime

Preflight validates the outer span, complete v5 payload, every LF span and every
write span, multiplication overflow and instance representability. NULL/zero is
valid; NULL/nonzero and overflowing counts are invalid. The invalid final command
prevents the whole invocation from entering provider Add. All construction is local;
only after every interval is complete does one `JobDetail::addJobIntervals` occur.
Empty LF collections retain the default vector without calling its setter.
Commands, writes, intervals and duplicates preserve exact caller order; no maps,
sorting, deduplication or merging occur. Caller arrays are borrowed synchronously
and may be mutated/released after return: the provider sees copied C++ values.
No hidden capability/quantization/data/endpoint/runtime-LF/VADB queries are made.

Malformed input maps to INVALID_ARGUMENT. Bridge value-construction failures and
bad_alloc (including provider Add bad_alloc) map to INTERNAL_ERROR; provider std
and unknown exceptions map to PROVIDER_EXCEPTION. No retry occurs; later explicit
same-Job recovery remains possible. The `interval-lf-allocation` test failpoint
fails preparation before provider entry. Existing status Never/Always/OnException,
Finalize/full-Cancel, Flush, ExtendEvent, CancelRemaining, synchronous Add guard,
parent-first ownership, explicit close and non-raising cleanup remain unchanged.

Safe Ada exposes private `Local_Function_Command`, a constructor, ordered
`Append_Local_Function_Write` and `Append_Interval_Local_Function_Command`.
Normal Ada vector value semantics give independent command copies and interval
copies. Existing constructors default empty and existing Add callers remain source
compatible. Private serialization uses final-sized Ada-owned v6/event-v4/command/
write/threshold/Stokes/Pointing/group/activity/label backing through synchronous Add.
There is no safe Rust or public Python LF command API.

## Provider evidence boundary

Mock `latest_intervals` getter observations establish positive payload fidelity,
ordered duplicates, boundary values and caller-buffer/Ada-copy independence.
The safe Ada Squall fixture adds type 0/instance 0 with [0,1], [8,0x12345678].
Pinned Squall `b1015728f904c799fa0c07489fce48e78f67845f` implements
`JobDetail::addJobIntervals` as a no-op. Successful invocation proves only normal
return, not LF existence/resolution, address validity, register writes, hardware
write ordering, execution timing or invalid-hardware-command failure reporting.

Deferred: TX intervals/TransmitEvent, Modulation/CommonModulations, Weights,
ProductStreamParams, endpoint mappings, context/std::any, CachedWaveform/TX
endpoints, actual standalone LF execution/status/discovery, RDMA, VADB, and the
inconsistent applicable-group setter/getter. No expansion into these was started.

## Baseline and validation record

Starting origin/main: `92093e09e1107ae622baad219511c856166417e5`; PR #81 merged.
Initial worktree clean; branch `feature/034f7-rf-interval-lf-commands`.
Measured baseline: native 279/279, Python 126/126, Release exports 195,
vendor SHA-256 804/804. Fresh Release additions audit: 196 exports, all original
195 retained and exactly v6 added. GCC and Clang 19 actual-header compile/runtime
probes pass (const getter overloads explicitly selected). The pinned header also
has a mutable command getter, not used by bridge/probes. LP64 size_t is 8 and
SIZE_MAX/UINT64_MAX instance forwarding is checked. A `cc -m32 -fsyntax-only` C
layout probe passes; no 32-bit runtime validation is claimed.

Failure evidence is retained outside the repository in the task evidence directory.
Initial fixture scenario and Ada span-type errors were corrected in scope. Running
build-owning gates concurrently caused generated-tree missing-executable/corrupt
object failures; these were not passes. Subsequent gates serialize shared-tree
owners and clean/rebuild generated native trees. No source contracts were weakened,
no unrelated IR source or abandonment regression was changed, and no unrelated
files or containers were removed.

### Completed local gates

- `make test-native`: 280/280; dedicated C F7 fixture internally runs 50/50.
- `make test-build-isolation`: pass; both complete 280/280 suites and separation.
- `make format-ada`, `make check-ada-format`: pass.
- `alr -C ada build`, `alr -C ada/tests run`: pass; safe Ada F7 50/50.
- `make test-rust`: workspace pass, sys ABI 24/24; workspace check, Clippy
  `--workspace --all-targets -- -D warnings`, and rustfmt check pass.
- `make test-python`: 127/127; C-compiled six-record layout/signature/inventory.
- `make check`: pass using installed GPRbuild/GNAT toolchain paths in PATH;
  direct-GPR safe F7 50/50 and earlier suites pass. Initial plain-PATH attempt
  stopped for unavailable GPRbuild and is not counted as a pass.
- Full native `--parallel 4 --repeat until-fail:50`: 280/280, 144.80 seconds,
  each test 50 repetitions, no Failed or Not Run. No abandonment or unrelated
  IR observation failure recurred. Complete native tree rebuilt after Alire.
- All four pinned Squall targets pass. VA rerun uses 29403/29418/29413/29414/29501;
  Job 29103/29118/29113/29114/29701; direct C RX
  29203/29218/29213/29214/29801; safe Ada RX
  29303/29318/29313/29314/29901 (control/metrics/health/RF metrics/data).
  Direct C ProductRx remains 8/8 with zero drops/malformed/allocation failures;
  safe Ada retains its eight-product assertions. Job LF fixture normal return
  establishes acceptance only. No TIME_WAIT workaround or unrelated stop needed.
- Fresh Release 196 exports, original 195 retained, map parity, no test symbols;
  runtime ABI 0.1. Prior C records and function declarations are byte-identical.
- Vendor SHA-256: 804/804. GCC and Clang 19 actual-header runtime probes pass.
- `git diff --check` and final-newline validation pass.

Normal implementation commits: `b1c24eb1be2b15d023f2f481250c5cd99dfbf9ce`
(native/raw) and `1db69f3324c11f5917d689ff8762a743c747fbdb` (safe Ada).
The initially missed Python secondary inventory count was updated to 196;
its first 127-test failure is retained separately from the passing rerun.
