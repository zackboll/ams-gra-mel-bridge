# Task 034F9 — RF RX JobInterval endpoint association map

## Starting state and scope

Fetched origin; PR #83 was merged, no open PR existed, and the initial worktree
was clean. Branch `feature/034f9-rf-interval-endpoint-map` starts at verified
origin/main `185eca9febd8add8e0b048b462caea827032d60f`.
Baseline: native 281/281, Python 128/128, production exports 197,
vendor checksums 804/804. The initial Python run overlapped native relinking
and had one library-load failure; its post-build rerun passed 128/128.

This is copied value-only JobInterval configuration. It is distinct from F8
ProductStreamParams and does not configure applicable-element-group labels.
Endpoint creation, DataPipe association calls, RDMA, memory registration,
hardware routing, product transfer and capability discovery are not implemented.
No relationship validation against JobRequest, ProductStreamParams, VA pipes or
hardware is attempted; caller/provider retain semantic responsibility.

## Pinned contract and bridge semantics

RF MEL `762ce84c5555dd0f3ea66f36b321fecf8839b89f` publishes
`EndpointID = uint64_t`, with both labels `std::string`.
ElementGroupToEndpointConnections contains a nested unordered map:
element-group label -> data-pipe label -> endpoint ID. Actual-header GCC and
Clang probes check indexing, iterators, size, clear, insert_or_assign, copy/move,
and JobInterval's addEndpoint/addEndpoints/setEndpoints signatures. The const
getEndpoints overload is selected explicitly; a mutable overload also exists.

The sole production map setter is `JobInterval::setEndpoints`. All entries are
prepared before calling it exactly once per active interval, including empty.
The bridge uses inner-map insert_or_assign: **last occurrence wins** for each
composite label key. This deliberately differs from upstream addEndpoint's
emplace/first-insertion behavior. Unordered iteration is never caller order.

Presence 0 ignores the entire span, including poisoned inactive pointers and
counts, without invoking the setter. Presence 1 validates the span, multiplication
overflow and every label using existing valid_view, then copies with copy_view.
Other presence encodings reject INVALID_ARGUMENT before provider Add. NULL/zero
is valid. Exact accepted UTF-8 bytes, whitespace, case and empty labels remain
unchanged; empty pipe and explicit `default` are distinct. IDs preserve the entire
uint64 domain, including zero, one, high bit and UINT64_MAX, as identifiers only.

The pinned getApplicableElementGroups incorrectly returns dataPaths rather than
the applicableElementGroupLabels written by its setter. F9 uses getEndpoints for
map fidelity. Both getters can observe the same map; this is not evidence that
the applicable-label vector was set. The earlier mismatch probe is retained.

## Additive ABI and language bindings

Exactly one export is added: `ams_mel_rf_job_add_rx_intervals_v8`, accepting the
v8 interval span and the existing Job/diagnostic arguments. ABI remains 0.1.
Frozen ReceiveEvent v1–v4 and JobInterval v1–v7 declaration text is unchanged.
V8 nests v7; the existing templated builder gains an endpoint-map profile and
inherits spatial, execution/polarization, pulse, LF and product configuration.
Older profiles retain upstream empty endpoint maps.

Measured LP64 layout (alignment 8 throughout):

| C record (`ams_mel_rf_` prefix) | Size | Field offsets |
| --- | ---: | --- |
| interval_endpoint_connection_v1 | 40 | element_group_label 0, data_pipe_label 16, endpoint_id 32 |
| interval_endpoint_connection_span_v1 | 16 | data 0, size 8 |
| job_interval_config_v8 | 240 | interval 0, has_endpoint_connections 216, endpoint_connections 224 |
| job_interval_config_span_v8 | 16 | data 0, size 8 |

Safe Ada exposes copyable Endpoint_Connections with Create, Append, interval Set
and Clear. It owns unbounded labels and IDs, rejects embedded NUL using existing
Valid_String, and leaves submitted-byte UTF-8 validation to native. Set copies;
Clear restores absence; setting a newly created empty value expresses explicit
empty. Final-sized native arrays and controlled string owners live through the
synchronous Add and clean up on exceptions. Raw representations remain private.
Raw Ada, Rust sys and private Python mirror all four new records and the export.
Rust/Python layouts are checked against an actual C11 layout probe.

## Validation evidence

Logs are `/tmp/034f9-*.log` in the task environment.

- `make test-native`: 282/282.
- GCC Release CMake/CTest in `build/034f9-release`: 282/282.
- Clang 19 Debug CMake/CTest in `build/034f9-clang`: 282/282.
- Strengthened C11 map test rerun in both trees: passed, 50 repetitions each.
  Exercises duplicate last-wins, multiple/empty/UTF-8 labels, exact ID domain,
  inactive poison, explicit empty, malformed spans/labels, allocation failure
  before Add, same-Job recovery, provider exceptions, status gates, parent-first
  lifetime, Finalize/Cancel gates and F7/F8 coexistence. The F8 fixture also runs
  its full prior-profile regression suite.
- Python unittest discovery with warnings as errors: 129/129.
- Rust workspace cargo test against the mock/test facade: passed.
- `alr -C ada build` and `alr -C ada/tests run`: passed. Full Ada smoke invokes
  the new focused suite; map suite passed 50/50 with F4–F8 fidelity, copies,
  replacement/Clear, empty presence, NUL rejection, status and exception paths.
- `make format-ada` and `make check-ada-format`: passed.
- Aggregate `make check` in the existing Alire test environment: passed,
  including production/test build isolation, native 282/282, formatting, full
  Ada smoke (map focused 50/50), final-newline and Git whitespace checks.
- Production Release symbol audit: exactly 198 exports under AMS_MEL_0.1.
- Frozen declaration/export audit and vendor SHA-256 804/804: passed.
- Pinned Squall `b1015728f904c799fa0c07489fce48e78f67845f`:
  `SQUALL_SOURCE_DIR=/home/zboll/git/squall make test-squall-rf-ada-job`
  with isolated `build/034f9-squall`: passed. Real Add accepts completed map,
  spatial and ProductStreamParams configuration; payload fidelity remains mock
  evidence. No hardware routing, registration or transfer claim is made.

Ordinary `make test-ada` could not find GNAT/GPRbuild on PATH; existing Alire
toolchains supplied the successful build/test above. A timed-out initial Squall
launch left task-owned runtime containers; ownership labels were verified before
removing only those containers and rerunning the integration successfully.
