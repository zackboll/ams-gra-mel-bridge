# Legacy README reference notes

**Historical implementation notes. Not the current API contract. Not a current
implementation-status statement.**

These background references were retained from the README at starting main
`291e97c4b1495b594b462982822fa48a2f439527`. The conceptual distinctions were
rewritten in [Project context](../project-context.md). Task-specific evidence
already has individual reports; the [research matrix](../doc-007b-readme-research.md)
identifies them rather than duplicating their implementation accounts here.
Outdated RF/Track exclusions and old ownership diagrams are not preserved as
valid statements. Historical export counts remain in the original task reports.

## Background glossary

The former README's surrounding-ecosystem glossary is retained for reference.
Entries naming services or transports do not mean this bridge implements them;
Squall transport descriptions apply only to that example deployment.

| Acronym | Meaning | Relevance |
|---|---|---|
| **AMS** | Agile Mission Suite | Mission-system architecture family. |
| **GRA** | Government Reference Architecture | Government-defined reference architecture; together, **AMS GRA**. |
| **AMS GRA** | Agile Mission Suite Government Reference Architecture | The open mission-system architecture this project targets. |
| **MFA** | Multi-Function Aperture | Sensor/aperture component that interacts with the free-space environment and produces digital sensor data. |
| **MEL** | MFA Encapsulation Layer | Standardized software boundary between an MFA and mission processing. |
| **RF** | Radio Frequency | Radio-frequency sensor domain. |
| **IR** | Infrared | Infrared / optical sensor domain. |
| **EO/IR** | Electro-Optical / Infrared | Optical and infrared sensing domain. |
| **IRST** | Infrared Search and Track | IR sensing/processing function; `IRSTImage` appears in the current receive profile. |
| **I/Q** | In-phase / Quadrature | Complex sample representation commonly used for digitized RF data. |
| **RX** | Receive | Receive-side RF operation. |
| **TX** | Transmit | Transmit-side RF operation. |
| **DIS** | Distributed Interactive Simulation | Simulation protocol used by the Hello World environment to distribute world truth. |
| **OMS** | Open Mission Systems | Mission-system standards used for service/data interoperability. |
| **UCI** | Universal Command and Control Interface | Standard message/data model used for higher-level mission products. |
| **CAL** | Critical Abstraction Layer | OMS software boundary that isolates services from underlying transport details. |
| **LA-CAL** | Language-Agnostic Critical Abstraction Layer | CAL pattern that allows services written in different languages to connect through a language-neutral protocol. |
| **OWP** | OMS WebSocket Protocol | WebSocket protocol used by the Hello World LA-CAL/Sleet implementation. |
| **ASB** | Abstract Service Bus | Logical OMS messaging network carrying UCI data among services. |
| **MASI** | Mission Agnostic Service Infrastructure | Shared mission-system infrastructure such as routing, management, health, and observability services. |
| **MPU** | Minimum Procurable Unit | AMS GRA unit intended to be independently procured/integrated. |
| **OEM** | Original Equipment Manufacturer | Manufacturer of sensor hardware/firmware behind an MFA. |
| **C2** | Command and Control | Mission command/control information and messaging. |
| **API** | Application Programming Interface | Source-level programming interface exposed to callers. |
| **ABI** | Application Binary Interface | Binary contract between compiled components; `ams_mel_c` exposes a C ABI. |
| **FFI** | Foreign Function Interface | Mechanism through which one programming language calls code written in another. |
| **IPC** | Inter-Process Communication | Communication between separate operating-system processes. |
| **RPC** | Remote Procedure Call | Request/response interaction across a process or network boundary. |
| **gRPC** | gRPC remote-procedure-call framework | RPC technology used in Squall's backend control path. |
| **TCP** | Transmission Control Protocol | Reliable byte-stream network transport. |
| **UDP** | User Datagram Protocol | Datagram transport useful for high-rate data paths such as Squall sensor payload delivery. |
| **UDS** | Unix-Domain Socket | Local IPC socket used by Squall/Couloir for backend gRPC communication. |
| **SPARK** | SPARK language/toolset for high-assurance Ada | Ada-based language/toolset used for contract-based analysis and formal proof; a SPARK Skill can use the same Ada MEL binding while keeping native FFI code outside the proved core. |

## Original external reading list

These are historical background citations, not normative API contracts or a
claim that the external resources have remained unchanged or available.

Useful public background material:

- AMS GRA Hello World — Overview and Terminology
  https://open-arsenal.gitlab.io/ams-gra/hello-world-sk/getting-started/tutorials/0-overview.html

- AMS GRA Hello World — How to Build a Multi-Function Aperture
  https://open-arsenal.gitlab.io/ams-gra/hello-world-sk/getting-started/tutorials/3-build-mfa.html

- Open Arsenal — Common MEL
  https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-common-mel

- Open Arsenal — RF MEL
  https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-rf-mel

These references describe the surrounding GRA concepts. The behavior and
supported surface of this repository are defined by this repository's own
source, tests, architecture decisions, and pinned upstream provenance.
