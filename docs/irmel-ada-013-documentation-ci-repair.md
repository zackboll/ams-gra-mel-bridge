# PR #182 — independent audit of the operational task subtype

## Failure and scope

Starting HEAD: `e248a3621c720d26780a92b787c4363fa9750814`, existing branch
`feature/irmel-ada-013-scheduling-task-executing`. Hosted documentation job
`42875965779/130039216631` and a local current-Git build both reject the public
declaration at `ada/src/ams-mel-ir-scheduling-metadata.ads:144`:

```ada
subtype Operational_Task_Kind is Task_Kind range No_Task .. IRST_Update;
```

GNATdoc completes successfully. The subsequent path is `docs_ci.build` →
`source_epoch.enhance` → `verify_presentation` → `restore_page` →
`Authority.occurrence` → `type_context`. The old ordinary-subtype grammar accepts
only a semicolon after the base. The pinned enhancer correctly links `Task_Kind`:

```html
<a class="type-reference" href="a5f209fcb5c5f51cbfaff90222ccdee098996be72108bb48a88327bf443e082c.html#beeb1ffbcee3f22588b9eb7263eceda362485250e8552e06cdd6a066a21164f8" data-type="ams.mel.ir.scheduling.metadata.task_kind">Task_Kind</a>
```

Containing native anchor:
`f5ae34b8803ebb98582a337e990b8756782593d816e5afcc2acad2558c931553`.
The range is not a GNATdoc rendering error. The native enumeration snippet does
abbreviate its middle with `…`, so `IRST_Update` must be verified against the
complete source enumeration, not inferred from the shortened snippet.

No Ada, native, ABI, provider, Python or Rust implementation is changed. The
eleven-value operational subtype, reserved-value handling and `Is_Operational`
remain unchanged. Coverage/API inventories need no changes: there is no binding
coverage delta. The historical coverage archive and CI permissions are untouched.

## Bounded repair and completeness

The independent auditor accepts only `Base range Identifier .. Identifier;` in
addition to its existing unconstrained ordinary subtype grammar. For a range it
proves complete source/native subtype token equivalence, flat public source
ownership of both declarations, the existing unique source-visible base target,
a direct same-package enumeration, and membership of both bare bounds in that
enumeration. Shadowing/ambiguous literals fail closed. Selected bounds, numeric
bounds, expressions, attributes, subtype chains, aspects and other constraints
remain unsupported. A null enumeration range remains valid Ada; no artificial
ordering rule is imposed. Native enumeration text must equal source tokens or
match the displayed prefix and suffix around GNATdoc's single list ellipsis.
Existing destination, identity, full type-mark, restoration and security checks
remain mandatory.

`required_additions()` continues deriving every eligible occurrence before
candidate HTML is read. Its historical fixed-ten assertion is replaced by
source-derived completeness, not a new magic production count. The current
source has 30 required occurrences: 21 ordinary subtype bases plus the original
nine current-package references. The subtype bases are five C2 System_Data
aliases; three each in Calibration_Status, Camera, Camera_Protection and Erase;
Navigation_Response; two Line_Of_Sight_Quaternion aliases; and the operational
task subtype. Without the new range there are 29. Complete independent audit
accepts 1,735 current-source links across 54 publication files. The current-Git
`verify_presentation()` now always enforces this required set, even outside deep
qualification; deep qualification still enumerates **all** eligible occurrences.
Historical counts/fingerprints and their tests are not rewritten for today's API.

## Regression coverage

- `test_static_subtype_audit.py`: nine fast source-backed tests, included in
  `make test-docs-ci`, covering the exact range, an unconstrained subtype,
  destination, public ownership, eleven literal positions, full source token
  ownership and native restoration, including abbreviated enumerations.
- Negative controls cover unknown/ambiguous/invisible/private base types,
  unproved public declarations, invalid/ambiguous/shadowed bounds, unsupported
  constraints/expressions, partial type marks, wrong destinations/identity/text,
  non-type-context links, duplicate/unexpected/missing links, and source/native
  disagreement.
- `source_epoch_regression_tests.py` independently checks the actual generated
  current-source required inventory and removes/duplicates/forges the operational
  subtype link while resealing both presentation and outer manifests. This
  exercises the normal non-deep publication verifier, not just deep auditing.
- Existing source/publication-integrity regressions are retained. Historical
  `test_local_type_audit.py` and `test_type_link_completeness.py` run on fresh
  GNATdoc output from their immutable source epoch
  `5c136a04ff64d980df7aaa2bd1ee49e1ed80daaf`, with only the repaired auditor
  substituted. Running those frozen assertions against today's unrelated API
  would not establish their original 1,428-link/ten-addition proof.

## Qualification evidence

Local logs use `/tmp/pr182-*`. Tools: isolated Alire settings under
`.doc-tools/settings`, GNAT 16.1.0, GPRbuild crate 26.0.1 (banner 26.0.0),
GNATdoc 26.0.0 (20260415), immutable pinned enhancer. Commands executed from the
repository root unless Alire's `-C` requires an explicit `cd ../..`.

Completed during repair:

- `python3 -W error -m unittest discover -s docs/tests -p test_static_subtype_audit.py -v`:
  9 passed.
- `make test-docs-ci`: 18 build/security, 9 static-subtype and 13 coverage tests
  passed.
- Pinned-enhancer discovery of `test_scheduling*links.py`: 8 passed, including
  all 3 TaskExecutingRep tests.
- Fresh original-epoch generation with the repaired auditor: 19 independent
  audit and 33 completeness tests passed; original fingerprints retained.
- Actual current-source generation and deep presentation verification: 1,735
  type occurrences and all 54 publication files verified.
- `make test-native`: 555/555 passed; `make test-ada`: passed, including focused
  TaskExecutingRep, ExecuteTaskAck and foundation repetitions.
- `python3 scripts/check_doc_source_integrity.py`: 824 vendor files, 3 workflows,
  passed. `git diff --check`: passed.

The full qualification sequence additionally runs `make check`,
`make check-ada-format`, `alr -n -C ada build`, `alr -n -C ada/tests run`,
`make test-python`, `make test-rust`, `make check-ir-api-coverage`, `make test-policy`,
`make test-documentation-source`, `py_pol_001_tests.py`, and
`sh docs/tools/qualify_examples.sh`. Current-source documentation qualification
uses `docs_ci.py qualify` (two separate Git exports/generations and deep audits).
The exact hosted build command is:

```sh
alr -n -s "$ALIRE_SETTINGS_DIR" -C docs/toolchain exec -- sh -c \
  'cd ../..; .doc-tools/enhancer/bin/python docs/tools/docs_ci.py build --gnatdoc-prefix "$GNATDOC_PREFIX"'
```

It requires a clean committed checkout and fresh output, as intended. Final
local results, exact final commit/tree, and final-HEAD hosted run results belong
in the delivery/PR handoff so recording hosted results does not invalidate the
qualified commit. Until those checks finish, this document is not a readiness
or deployment authorization. Real provider/hardware tests are out of scope.

Investigation failures are not counted as passes: the original current-Git build
reproduced the reported exception; an initial attempt to run historical tests
against stale local `_site` failed at native package ownership; an overly broad
public-scope check on existing unconstrained targets was narrowed back to the
new range grammar; an intermediate regression expected 18 occurrences before
source inspection established all 30. Complete replacement runs are required.