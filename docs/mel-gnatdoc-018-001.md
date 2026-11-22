# MEL-GNATDOC-018-001: official Enhancer v0.1.8

## Release and visibility

The documentation toolchain uses the [official v0.1.8 release][release]:

| Identity | Pin |
| --- | --- |
| Upstream commit | `1245720eb962f8a68a74dae13ab15ce370ef74f5` |
| Upstream tree | `c86e6c015251ce01ab51180b157f81ac98a59df6` |
| Wheel | `ada_gnatdoc_enhancer-0.1.8-py3-none-any.whl` |
| Wheel SHA-256 | `baa45c742383d4057444ba7562bce2a8af1c6ab36ac4523d13305daa8ea1393b` |
| Package fingerprint (53 members) | `4c523e698ae9f839f889d5bd7eee3468f54670884c7d0c62a822c6730ba4039a` |
| `gnatdoc-source.css` SHA-256 | `2e4eb69bd23e1ea97e3877383dc7e75d701429006c25581a0e16a75041823fdc` |

The fingerprint is SHA-256 of the sorted, compact JSON map from each package-
relative filename to its SHA-256, computed directly from the verified wheel ZIP,
not from the installed package's `RECORD` or a publication manifest. The installer
requires the binary wheel hash and exact CLI version; runtime verification checks
distribution/import identity, PEP 610 archive origin, and all package bytes
without the network.

MEL retains the [reviewed schema-1 opt-in](mel-gnatdoc-source-001.md): the complete
public `.ads` specifications, including their private sections, are published.
Private library units and `.adb` bodies are not published. Schema 2 is not enabled.
The tracked evidence digest remains a fail-closed placeholder. Only disposable
Git exports bind that digest to independently measured native and source bytes.

## Qualification

MEL reuses its own `ada_examples.tokenize` to derive the expected ordered display
spans. Upstream adapted that lexer from MEL; the auditor does not import upstream
tokenization, rendering, or destination resolution. Exact serialization checks
all seven lexical classes, payloads, boundaries, line anchors, source SHA-256,
and the local stylesheet. It never interprets comment text as HTML. Stripping
only these verified spans must recover the complete v0.1.7 source page. Native
restoration and full-profile declaration/overload reconciliation remain separate.

`source_view_parity.py` uses an isolated, hash-checked official v0.1.7 interpreter
with the same pristine GNATdoc 26.0.0 output and Git source as v0.1.8. With source
browsing off, only the exact serialized `enhancer_version` may differ. With
schema 1 enabled, the only additional differences permitted are the verified
token spans, pinned stylesheet and corresponding output hashes. It also checks
the v0.1.8 off/on delta, with no unrelated Ada, search, authored, Rustdoc or
Python changes. The v0.1.6 pin and previous campaign records remain historical.

The non-deploying `documentation-source.yml` runs these comparisons, source and
portal mutation suites, repeated native generation, and real Chromium source
and search checks. The source browser measures the `Navigation_Report` and
`Submit_Navigation_Report` locations from the current source and verifies the
search-to-declaration-to-line journeys without hard-coded line numbers. The
workflow runs on main pushes or manual dispatch, not as a PR requirement.
Qualification output is diagnostic, not deployment authority.

[release]: https://github.com/zackboll/ada-gnatdoc-enhancer/releases/tag/v0.1.8
