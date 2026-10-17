# Project context

This document preserves the conceptual rationale formerly spread across the
root README. For current ownership diagrams and API families, start with the
[published architecture guide](https://zackboll.github.io/ams-gra-mel-bridge/guides/architecture.html),
not this background discussion.

## MFA, MEL and Skill are different responsibilities

A Multi-Function Aperture (MFA) is the sensor-side capability: physical or
simulated apertures, electronics, digitizers, firmware and local processing can
contribute to it. The MFA Encapsulation Layer (MEL) is the published software
boundary through which mission-processing software consumes those capabilities.
A Skill is mission-processing software, not another name for the sensor or its
interface. “Skill” is a term, not an acronym.

An IR search-and-track application or RF processing application may consume MEL
data and produce higher-level mission products. OMS/UCI messaging, service
orchestration and those processing algorithms are outside this bridge. Building
the binding alone does not produce a complete MFA or Skill.

## Provider language and consumer language are independent

Reimplementing a simulator or hardware backend in Ada or Rust answers a different
question from enabling an Ada, Rust or Python consumer of an existing provider.
A provider implemented behind a thin C++ layer can use Ada, Rust, firmware or
another backend while preserving the published C++ MEL interfaces. This is a
possible deployment architecture, not an implemented provider supplied here.

Conversely, native C++ consumers can use MEL directly. Other consumers use the
project's C ABI so they need not reproduce C++ virtual-object layouts, templates,
smart-pointer ownership and exception handling in each language. The intermediary
centralizes translation; it does not remove C++ compiler/runtime compatibility
requirements between the adapter and provider or define an official AMS C API.

This separation permits incremental adoption without rewriting a provider or
forcing existing C++ applications through an unnecessary bridge. Strongly typed
Ada application logic, Rust ownership-oriented applications and Python
prototyping or analysis can share that native boundary. Scientific/ML tools are
possible application integrations, not dependencies or implemented processing
features of this repository.

## Process and transport boundaries

The consumer, native adapter and dynamically loaded MEL provider interact in
process. The implementation behind that provider need not share the process,
processor or implementation language. A provider may use IPC, network transport
or device drivers internally; this bridge does not define those transports.

Squall is an example simulated provider, not a required backend or the bridge's
implementation. The pinned integration records describe its C++ MEL front end,
Couloir control path and separate Rust RF/optical backends, including separate
control and sensor-data transports. Consult the
[Squall integration evidence](../integration/squall/README.md) for that specific
deployment rather than generalizing it to other providers.

## Ada and SPARK assurance boundary

SPARK uses the Ada binary interface, not an additional bridge ABI. An application
can keep ordinary-Ada FFI and ownership code at a reviewed trusted boundary and
apply SPARK analysis to selected state machines, command validation, resource
arbitration or other deterministic application logic above it. These are design
options, not proof results for this repository. Neither the C/C++ implementation,
provider, callbacks nor native FFI is asserted to be SPARK-proved.

## Retained background

The [legacy reference notes](history/readme-legacy-notes.md) retain the old
glossary and external reading list. The
[README research matrix](doc-007b-readme-research.md) maps the removed material
to existing implementation reports and distinguishes historical scope from
current source contracts.
