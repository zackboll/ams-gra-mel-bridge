# Agent workflow
Read the assigned task, relevant task documents and applicable structured coverage
inventories before coding. Prefer one small vertical slice per task. Do not invent
API stubs that return success, expand scope into DSP/services, or weaken
tests/contracts to pass a gate. Stop and report concrete unresolved upstream
semantics rather than guessing. Use docs/tasks/001-provider-foundation.md as the
next implementation task after bootstrap.

Treat `docs/coverage.md` as a historical archive: preserve existing information;
do not prepend feature sections or add new feature status. Each feature requires
a uniquely named, task-owned Markdown file (for example,
`docs/py-mel-009-<feature>.md`, `docs/irmel-ada-012-<feature>.md` or
`docs/rust-ir-007-<feature>.md`) recording its coverage delta, implementation
boundaries, actual test commands/results, limitations and unresolved semantics.
Do not introduce a shared feature-status index that every PR must edit.

Ada, Python and Rust tasks should modify only their own implementation/test files
and uniquely owned feature documentation wherever practical. Avoid shared
`CHANGELOG.md`, generic README sections, `docs/architecture.md`,
`docs/c-abi-policy.md` and `docs/coverage.md` edits merely to add feature status
prose. Correctness still requires necessary native ABI, actual shared-interface,
security-contract and authoritative structured coverage/API inventory updates,
including generated reports. For IR Ada use `docs/reference/irmel-api-schema.md`;
do not substitute task prose for implementation evidence or leave inventories
stale. Keep ordinary Git merge behavior; no blanket `merge=union` attributes or
custom conflict resolvers. Real shared-contract conflicts still require review.

Whenever a task changes first-party Ada sources under `ada/src` or
`ada/tests/src`, run `make format-ada` before build/test and
`make check-ada-format` before final validation/commit. Do not manually fight
formatter output; investigate/configure GNATformat when its output is undesirable.
