# Task 034B1A — Safe Ada RF Admin state control

Starting main: `c4faea0999d38b785c62271a2222535e7a4c1c14`.
Branch: `feature/034b-safe-ada-rf-rx-job-control`. The declaration closure
checkpoint `3be9a7ead51e1c870c7241cb20c51fdae1f1100e` already pinned the
034B1 six-root GCC 710 / Clang 711 / union 712 closure, adding 28 RF MEL and
146 Boost headers. This task adds **no** vendor files. The preceding ProductRx
`Provider_Failed` literal cleanup is a separate behavior-neutral commit.

Pinned RF MEL `762ce84c5555dd0f3ea66f36b321fecf8839b89f` publishes
`fnAdminMEL`, `AdminMEL::getUCIControl()`,
`UCI_Control::getStatusControl()` and `StatusControl::commandState(MFA_State)`.
The bridge loads only `createAdminMEL` with `SharedLibrary`, asserts the exact
factory type, passes configuration as `std::string_view`, and preallocates its
owner/state before invoking the provider factory. Admin does not reuse DataMEL.
Command obtains fresh shared owners for UCI and StatusControl on every call;
neither is cached. Null required objects fail with named diagnostics. Caller
operations on one Admin owner must be externally serialized.

The public owner holds an Admin state containing the provider object and a DSO
reference. Close consumes the public handle before calling shutdown; on success
it destroys AdminMEL **before** dropping the DSO reference. On throwing
shutdown it never retries: an allocation-free intrusive self-reference retains
the complete state, provider, and DSO for the process lifetime. Explicit Ada
Close raises `Provider_Error` with the provider diagnostic; finalization never
raises. This is intentional fail-safe retention, not provider destruction under
an uncertain shutdown boundary.

The C ABI aliases the existing `ams_mel_ir_mfa_state_t` as
`ams_mel_rf_mfa_state_t`, with compile-time checks for **every** pinned Common
MEL MFA state 0–14 and its `uint32_t` underlying type. 15 (`MAX_EXCLUSIVE`) and
all greater values fail before any provider call. Safe Ada reuses
`AMS.MEL.Status.MFA_State`; it does not duplicate the enumeration. A provider
`true` returns Ada `True`, a provider `false` returns Ada `False`, and only a
failure/exception raises `Provider_Error`. Path and configuration NULs are
rejected before temporary exception-safe C string allocation.

Exactly three ABI 0.1 exports were added: `ams_mel_rf_admin_open`,
`ams_mel_rf_admin_command_state`, `ams_mel_rf_admin_close` (115 → 118).
The raw private Ada, Rust sys and Python ctypes declarations remain in sync;
there is no safe Rust or public Python RF Admin API. Mock RF scenarios:
`admin:ok`, `admin:reject`, `admin:command-throw`, `admin:no-uci`,
`admin:no-status`, `admin:factory-null`, `admin:factory-throw`,
`admin:factory-throw-unknown`, and `admin:shutdown-throw`.
The C11 test records shutdown → Admin destruction → DSO unload, and proves no
destruction or unload on throwing shutdown. The safe Ada tests exercise
accepted/rejected commands, failure modes, closure and finalization.

`make test-squall-rf-ada-admin` is opt-in and checks the exact pinned Squall
checkout `b1015728f904c799fa0c07489fce48e78f67845f`. Its safe Ada client
uses the production C facade without linking Squall, the mock, or the job
helper, commanding Standby then Operate_Rx_Only and closing Admin. Record its
actual result separately; ordinary builds never require Squall or containers.

**Deferred:** production C2MEL / asynchronous VirtualAperture (034B1B), and
JobRequest/JobDetail/finalize/cancel/helper removal (034B2).