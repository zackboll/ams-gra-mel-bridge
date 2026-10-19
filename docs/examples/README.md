# Executable public-Ada examples

From the repository root, with Linux, Python 3, Alire 2.1.1, GNAT Native
16.1.0, GPRbuild 26.0.1, CMake and a C11/C++20 compiler installed:

```sh
sh docs/examples/format.sh format
sh docs/examples/format.sh check
sh docs/examples/run.sh
```

The runner builds `native/build/lib` with the production native build script,
builds mocks separately with `native/scripts/build-test-providers.sh`, and
imports `ada/ams_mel.gpr` from `ams_mel_doc_examples.gpr`. CMake alone compiles
native code. All nine mains use Ada 2022, assertions, validity checks and
warnings-as-errors. Formatting explicitly uses the existing pinned GNATformat
26.0.0, this GPR, UTF-8 and `--no-subprojects`.

Individual cases (each also builds the collection; append `--no-build` after a
successful build to avoid rebuilding):

```sh
sh docs/examples/run.sh provider-session
sh docs/examples/run.sh provider-session-missing
sh docs/examples/run.sh owned-image
sh docs/examples/run.sh frame-lease
sh docs/examples/run.sh common-channel
sh docs/examples/run.sh navigation-report
sh docs/examples/run.sh navigation-report-rejected
sh docs/examples/run.sh track-metadata
sh docs/examples/run.sh rf-discovery
sh docs/examples/run.sh rf-discovery-rejected
sh docs/examples/run.sh rf-rx-job
sh docs/examples/run.sh rf-product-rx
sh docs/examples/run.sh rf-product-rx-rejected
```

`manifest.json` records the exact mock configuration, library family, main,
support files, public packages, guide anchors and expected observations for
every case. `--report /absolute/path/report.json` records each actual exit,
stdout/stderr and resolved production-facade path/SHA-256. The missing-provider
case expects child exit 1; the runner itself returns zero only if the expected
diagnostic and exit were observed. Rejections are normal data, checked before
outcome-specific accessors. Unexpected exceptions or semantic mismatches fail.

Every main accepts `PROVIDER_LIBRARY MOCK_CONFIGURATION`. For example:

```sh
LD_LIBRARY_PATH="$PWD/native/build/lib" docs/examples/bin/rf_product_rx \
  "$PWD/native/build-tests/test-providers/libmock_rf_provider.so" rx:ok:sync
```

Prefer the runner: it clears inherited `LD_*`, `LIBRARY_PATH` and `AMS_MEL_*`
settings, selects the production facade, inspects `ldd`, exported symbols and
each actual child's loader initialization,
checks actual semantic observations and exits, and imposes 15 seconds per
process. All API waits are bounded to 1,000 ms. Native and Ada builds run
sequentially; do not run concurrent writers of these build trees. No hardware,
Squall, private emitters, failpoints, credentials, containers or provider downloads.

Read the rendered **Examples** guide at `guides/examples.html` in the generated
portal for complete listings and ownership/error/fixture explanations. Listings
are generated from these exact compiled files; supporting sources are included
in native HTML `details`, usable without JavaScript. No separately pasted Ada
code or downloadable site assets are maintained. A local checkout contains all
sources, GPR and runner. `docs/tools/example_sources.py` independently decodes
and compares every rendered listing to canonical source.

`mock_fixtures.adb` contains complete navigation, virtual-aperture and Job inputs
required by the existing mocks, drawn from the corresponding public-API Ada
tests. They are not real-provider defaults. RF Job mock identities are distinct
from ProductRx event metadata. The support packages contain no resource lifecycle
operations; acquisition, outcomes, borrows and explicit closure remain in mains.
