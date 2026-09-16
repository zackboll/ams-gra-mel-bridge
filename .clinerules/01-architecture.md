# Architecture
Read AGENTS.md and docs/architecture.md. Consumer-side C facade over the
published C++ MEL interfaces; one adapter reused by Ada and later Rust.
Do not depend on Squall-private gRPC/UDP contracts. No OMS/UCI, algorithms,
container orchestration, or Rust in the generic library. Current bootstrap
contains no provider implementation. Record scope precisely.
