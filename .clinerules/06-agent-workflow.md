# Agent workflow
Read the assigned task and current coverage before coding. Prefer one small
vertical slice per task. Do not invent API stubs that return success, expand
scope into DSP/services, or weaken tests/contracts to pass a gate. Update docs
and coverage along with public behavior. Stop and report concrete unresolved
upstream semantics rather than guessing. Use docs/tasks/001-provider-foundation.md
as the next implementation task after bootstrap.

Whenever a task changes first-party Ada sources under `ada/src` or
`ada/tests/src`, run `make format-ada` before build/test and
`make check-ada-format` before final validation/commit. Do not manually fight
formatter output; investigate/configure GNATformat when its output is undesirable.
