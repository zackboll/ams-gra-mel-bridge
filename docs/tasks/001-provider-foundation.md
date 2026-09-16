# Cline task 001 — establish the first real MEL provider boundary

Work in this repository. Follow `AGENTS.md` and `.clinerules/`. Do not implement
IR processing, RF DSP, OMS messaging, Rust support, or registry publication.

## Goal

Advance from the bootstrap ABI-version smoke test to a minimal, separately
loaded mock C++ provider using a reviewed, pinned IR/Common MEL header subset.
C and Ada clients must exercise the same C facade. This is a library foundation,
not a complete Skill or a claim of full MEL compliance.

## First inspect

Read `docs/reference/AMS_GRA_MEL_Design_Review.md` and the candidate inventory.
Confirm the relevant upstream revisions and their actual transitive source
requirements at those revisions. Do not copy default-branch files and label
them as matching a candidate commit. Record all selected sources and licenses.
Identify the exact provider factory signature and consumer responsibilities.
No silent changes to the upstream public interface are permitted.

## Implement one small vertical slice

1. Populate only the verified dependency closure needed for loading an IR
   control instance and obtaining version/capability information. Keep normal
   builds network-free and do not require Squall images or provider downloads.
2. Build a mock shared-library target separately from the adapter and test
   executables. Use the real pinned C++ factory/interface signature for the
   selected slice. Unsupported operations must fail explicitly.
3. Design an opaque C provider/session handle, explicit close semantics, and
   a query that does not leak C++ strings, references, or shared pointers.
   Distinguish facade version from provider version. Do not add a second
   provider-facing protocol to avoid the published C++ boundary.
4. Implement provider loading in private C++ code. Retain the shared-library
   owner for every object that can invoke provider code. Catch ordinary C++
   exceptions at the C boundary. Do not promise safety for ABI-incompatible
   providers; test only deliberately compatible mock builds.
5. Add a limited Ada owner, explicit Close, safe finalization fallback, and
   idiomatic query. Keep foreign representation/imports in private packages.
6. Add real C-compiled and Ada consumers exercising the same adapter path.

## Required tests

Successful open/query/close; null/invalid inputs under the documented contract;
missing library; missing required export; provider initialization failure;
repeated open/close; explicit close before finalization; and an attempted close
with a live child if a child resource is introduced. Do not add artificial
children solely to check a box. Tests must stay active in release builds.

## Validation and reporting

Run `make test-native`, `make test-ada`, `alr -C ada build`, and
`alr -C ada/tests run` when available. Report unavailable tools rather than
silently skipping or reporting success. Update coverage, decisions, and source
provenance. Report files changed, exact test commands/results, and unresolved
contract questions. Do not commit, push, publish, or create a release unless
explicitly requested.

Stop after this load/query/lifetime slice. IR image buffering is task 002.
