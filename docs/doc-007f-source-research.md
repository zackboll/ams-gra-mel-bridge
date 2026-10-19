# DOC-007F source research

## Immutable identities and isolation

Starting main is actual PR #117 merge
`a5484514cfae3b7bccde078118b838fa40ce4d0e`, tree
`e8c845e0c3755eda0456094c4c86038114acc409`. Fetch confirmed no intervening
main changes. Branch/worktree/PR inspection found no equivalent DOC-007F work.
Worktree: `/home/zboll/git/ams-mel-doc-007f`; branch:
`docs/007f-merged-enhancer-adoption`; external evidence:
`/home/zboll/git/ams-mel-doc-007f-evidence/r1`. Earlier evidence and worktrees
are preserved. No runtime, public ABI, source, vendor, workflow or TOML change.

Selected upstream actual PR #12 merge:
`3867c8879a3c832fb6e096d7680a8b739a2cffe1`. Its ordered immutable parents are
`23cf51d3765e7b3f68411e25a57f52f4c0efc7e1` and reviewed head
`e25466bd0ffa167dad02120dae2b17ac1d5966e2`. Merge and reviewed head share tree
`74e65bd9c48b10f0e6421c8a14afbf2a22cbe808`. Old MEL pin is an ancestor of
the predecessor. GNATDOC-009A/010 are inherited, not independently cherry-picked.

The initially supplied upstream run [39189495819](https://github.com/zackboll/ada-gnatdoc-enhancer/actions/runs/39189495819)
is successful but checks **predecessor `23cf51d…`**, not the selected merge.
The selected merge's actual [39196435839](https://github.com/zackboll/ada-gnatdoc-enhancer/actions/runs/39196435839)
also completed successfully. The supplied MEL post-merge Documentation
[39189424818](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/39189424818)
and Bootstrap [39189424686](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/39189424686)
are successful on exact starting main.

## Three immutable installations

| Control | Revision | Archive SHA-256 | Installed implementation/assets fingerprint |
|---|---|---|---|
| A | `53a321915da2c5a02a54581040673b61f8595b25` | `d324735784435176c0f1a7093cf462e15b2bd92be429ef5e1e42e802331ca52e` | `54d9327e57662a5843198fbc5352d4c5f544a3d5e3047c8269c6b9c68c5e878b` |
| B | `23cf51d3765e7b3f68411e25a57f52f4c0efc7e1` | `daa888dcf7e38d7afaa5fc4c17497c7c05f17ccb921537dba276b97e0cde3e51` | `7671001707aae501c405244e7445ad62645eae662d0c8369f90052785809ddf1` |
| C | `3867c8879a3c832fb6e096d7680a8b739a2cffe1` | `61adae2b0c42fa588d8b1362f7441e27b79fdc0498a819e1f01fffadc53c586a` | `2ca2b3ecadf7832bd6bd29bf1ac52d69679defffff85fc2cd2c832a6f3aede14` |

Independent archive/blob comparison covers all **1,089 / 1,262 / 1,272** files.
Fresh separate non-editable installations match all **29 / 31 / 31** runtime
files, excluding only bytecode. Distribution metadata and direct archive URLs
are retained with full old/new source/assets SHA-256 mappings in `A/B/C-dependency.json`.
No checkout/PYTHONPATH substitution supplies the implementation. All distributions
still say `0.1.0.dev0`; this is not their identity. The fingerprint is SHA-256
of the compact sorted JSON relative-name/whole-file-SHA-256 map.

Navigation JS remains
`b3fb428b507844d6be76fb40156058d39a5b5f57004bde04e33b0411f68504cd`;
theme CSS remains
`cf06637732a8b047a7191bfb991a5c3da7cd31a52076876880d5f8a3f63929bc`.
Upstream runtime delta is syntax/link eligibility, new metadata/browser-metadata
modules, pipeline metadata integration, and guide/validation/qualification
adaptation. No asset delta. Optional features remain unconfigured/unauthorized.

## Independent occurrence authority

The old audit only validated emitted wrappers; it rejected ordinary subtype bases
and current-package short owners, and could miss eligible plain text. The new
consumer derives ten required records **before reading candidate HTML**, using
its own source inventory, token grammar, pristine snippets and unchanged ledger.
No shared resolver, type-position scanner or output counter is an authority.

Ordinary subtype bases require the complete `subtype Name is Complete.Type.Mark;`
token grammar, a flat public source scope and public nonprivate source-owned
destination. Constraints, null exclusions, aliases, expressions, generic scopes
and private targets are outside this boundary. Matching source/native token
blocks must contain the entire mark and agree on context. The actual newly
admitted source is Image.Metadata line 89, `Navigation_Response`, targeting the
Image record anchor
`8a5baab22f0e0f0f1d7abda3d62db52f34629a4026217c428e5e31b025b18783`.

Current-package acceptance requires precisely two identifiers, the real current
package's final identifier, and an actual unique type/subtype of that package.
An independently tokenized complete source header/private boundary/public body
must establish flat scope, without nested packages, aliases, instances, generics
or inherited defining names. Every bare occurrence of the short package name is
treated as potential shadowing, including same-line/comma-list objects, formals,
components and enum literals. Only simple flat record component declarations
are supported here. Both containing and target declarations must occur in the
established public token region. This intentionally permits false negatives;
it is not general Ada name resolution or arbitrary suffix matching.

The complete ledger retains containing native page/anchor, native start/end,
original visible spelling, source declaration/context/offset/line, canonical
identity and actual destination page/anchor. Image lines 98, 122, 124, 126, 128,
666 and 672 supply seven occurrences; Instrumentation lines 87 and 98 supply
two. `ten-addition-ledger.json` contains full records, not fabricated anchors.
Navigation_Report has exactly six local links; the new Attitude_Rate and
Position_Velocity_Covariance targets are real native record declarations.
Long_Float and Long_Long_Integer remain plain.

All original 1,418 records are preserved as an order-preserving projection of C
onto A's native occurrence keys. Their complete compact sorted-JSON SHA-256 is
`c943b0fd5ff0491ed965dac2ff0cbce180b452fca538e6b87adc7192e8420e5c`.
The consumer enforces this original projection separately from the new set and
separately from full output/manifest reconstruction. No old destination change,
new unqualified link or missing eligible new reference can hide behind a count.

## Independent metadata authority and restoration

For all 29 packages, actual complete source package identity is reconciled with
one native h1. That mapping supplies `<package> — AMS MEL Bridge`; the index
gets `API Reference — AMS MEL Bridge`. Neither output title nor filename grants
identity. The project label is independently fixed by the unchanged exact TOML
gate. Metadata is reported as a 30-document inventory by successful validation.

The consumer checks an exact serialized prologue/head: native html class plus
`lang="en"`, explicit head, UTF-8 charset, exact escaped title, native stylesheet,
original viewport/theme link, and (index only) unchanged local deferred script.
It does not rely on HTMLParser title callbacks: byte-level expected title/head
comparison rejects RCDATA nested-looking tags/comments, numeric control/bidi
references, alternative encodings, duplicates, extra attributes and misplaced
metadata. Additional head/title/meta/link/script/root content is forbidden in
the remaining native body. Existing script placement/asset checks run first;
the unchanged native Page checker still rejects all scripts after only that
index exception is removed. Package/portal scripts have no exception.

Only the authorized metadata prefix, navigation insertions, flat type wrappers
and approved lexical spans are removed. Every complete restored file must equal
pristine native bytes, then the original unchanged MEL checker and coverage run.
Full expected output and manifest are separately reconstructed from trusted
pristine/source plus the immutable installed enhancer. Self-consistent forged
manifests grant no authority. Authored portal bytes/metadata are never rewritten.
The fresh external-audit/stable-parent trusted-filesystem boundary is unchanged.
