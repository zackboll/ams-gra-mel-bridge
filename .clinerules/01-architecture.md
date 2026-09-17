# Architecture
Read AGENTS.md and docs/architecture.md. Consumer-side C facade over the
published C++ MEL interfaces; one adapter reused by Ada, Rust, and Python.
Do not depend on Squall-private gRPC/UDP contracts. No OMS/UCI, algorithms,
or container orchestration in the generic library. The current implementation
contains only the documented IR vertical slices. Record scope precisely.
