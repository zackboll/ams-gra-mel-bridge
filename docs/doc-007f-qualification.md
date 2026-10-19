# DOC-007F qualification

The authoritative final commit/PR/hosted results and evidence manifest are in
`/home/zboll/git/ams-mel-doc-007f-evidence/r1/FINAL-HANDOFF.md`, avoiding
self-referential tracked commit identities. Pending checks never count as passed.
See [source research](doc-007f-source-research.md) for immutable dependency,
source/anchor and independent acceptance boundaries.

## Controlled complete publications

All controls use exactly the same current MEL source, pristine GNATdoc input,
native anchor ledger, authored portal and byte-identical production TOML.
Archives are installed separately and non-editably, and all installed source
and asset bytes match immutable Git blobs.

| Measurement | A: DOC-007E | B: predecessor | C: selected merge |
|---|---:|---:|---:|
| Local type links, independently audited | 1,418 | 1,419 | 1,428 |
| Complete href/src references | 2,965 | 2,966 | 2,975 |
| External type links | 0 | 0 | 0 |
| Complete publication files | 42 | 42 | 42 |
| HTML pages | 37 | 37 | 37 |
| Source-owned public packages | 29 | 29 | 29 |

`ABC-complete-file-comparisons.json` proves every relative filename and complete
file SHA-256. B→C changes only Image HTML, Instrumentation HTML and presentation
manifest: **39 other files byte-identical**. A→C changes all 30 API HTML pages
and manifest: **11 other files byte-identical**. Only Image, Instrumentation and
Image.Metadata gain links; all API pages gain metadata. No CSS, JS, portal or
native stylesheet delta. The A audit is recovered with the original immutable
DOC-007E auditor; its entire 1,418-record ordered projection is unchanged in C.

## Determinism, integrity and security

Two fresh GNATdoc generations use GNATdoc 26.0.0 (20260415), GNAT 16.1.0,
GPRbuild 26.0.0 (Alire package 26.0.1), Alire 2.1.1, stable compiler paths.
All 39 pristine filenames/full bytes agree. Two fresh consumer builds agree
with each other and C in all 42 complete filenames/bytes. Idempotence, dry-run
preservation, restored native byte equality and complete output/manifest
reconstruction are exercised on authentic qualified input. External audit tests
retain fresh exclusive creation, containment/aliases/existing-file rejection,
and exact post-return publication hashes.

Authority fingerprints remain:

- SOURCE_SHA256 `467cd45ef85888bb22ccee2d9e325b6b55e985dadac66c317dbbc653193114c8`
- ANCHORS_SHA256 `06b602439be15557118790a2827bb8bacc45c1b02d055fa3e9502204857f5aed`
- PRISTINE_SHA256 `0e6497d6172c918d04ea1c284785f7cfc0fec278e155ab254de0c78e81ac94e2`

Coverage stays **1,238 source / 1,237 native / 4,294 of 4,295**, one existing
renderer omission, zero public gaps. Warning identities stay **73 raw / 0
public / 73 private**. Protected files, vendor checksums, policy identities,
source, native anchors, authored files, TOML and workflow are unchanged.

The expanded suite retains **133 documentation tests**, including **51 source
integrity tests counted once**, and all **100 original presentation tests**.
It adds **75 focused tests**, for **175 presentation / 308 distinct tests**.
New direct semantic and serialized metadata mutations reach their intended
independent guards, not output-hash/setup/tool failures. Required-link removals
with internally consistent manifests reach independent completeness rejection.
Original-link removal reaches the ordered-original-audit guard. All 34 original
navigation tests and all external-audit tests remain present.

The first expanded presentation run recorded two failure-message expectation
errors in navigation mutations; a second run retained one such expectation
error for an event-handler insertion at the exact script/head/body boundary.
All mutations were correctly rejected by exact script placement. Failed logs
are retained; expectations were corrected to the actual guard without changing
production rejection behavior. One foreground
browser harness launch hit the command runner's 30-second timeout; its partial
log/read tree remain preserved. The subsequent campaign uses a separate reader
tree and background execution, not a mutable publication or hidden retry of an
unexplained product failure. A fresh Chromium 156.0.8078.4 campaign passed 5,289 assertions over desktop/mobile,
light/dark and JavaScript on/off, checking actual native profiles after new-link
clicks, titles, charset/language, sidebar/search/history and authored navigation.
A final repetition distinguishes the three separate Image.Orientation component
occurrences rather than repeatedly selecting their shared first href. Screenshots,
served publication hashes and exact final results are recorded externally.

## Review scope and limitations

Only README and documentation-owned installer/consumer/auditor/tests/research/
qualification files are changed. Existing audit-output tests receive only the
new independently established occurrence/reference counts. No workflow change
is needed: its existing discovery finds the added tests. No generated HTML,
archives, full audits, screenshots, tools or logs are tracked.

The current-package proof supports a conservative flat public record subset;
it rejects aliases, generic instances, derived/inherited defining-name scopes,
nested scopes, ambiguous or shadowed short owners, and unknown/imported sibling
or ancestor short names. Ordinary subtype-base support deliberately rejects
constraints and private targets. No blanket Ada semantic resolution claim is
made. Browser qualification is Chromium-only, not comprehensive accessibility
or cross-browser certification. Trusted filesystems/stable audit parent
directories remain required. Stop at independent review: no merge, auto-merge,
force-push, rebase, tag, release or manual deployment.
