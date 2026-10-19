# DOC-007E source research

## Immutable starting and dependency identities

Starting fetched main and GitHub PR #116 merge are
`8be05e9d0284e81a1769b2e1639a4b96b191324c`, tree
`8158378afbfb59686a396e2f758ae30545610c8a`, ordered parents
`0e64ea0689f2fae360baba78b6ed6f74aeef08ca` and
`1e1849ce1d01e3600256ae15b220c8ac1c5265ea` (reviewed DOC-007D R2).
No existing DOC-007E branch, PR, worktree or evidence directory was found.
The task branch starts there without rewriting prior evidence or history.
Both post-merge runs 39095043552 (Documentation) and 39095043538 (Bootstrap)
completed successfully on attempt 1. The exact-main review artifact passes the
DOC-007D consumer against trusted pristine input and equals reviewed R2's entire
41-file inventory and complete bytes. It is kept separate from all controls.

The selected enhancer PR #9 merge is
`53a321915da2c5a02a54581040673b61f8595b25`, ordered parents
`255e6453d2ec4801e804625f690a13956d62d145` and reviewed head
`6187976c53cf42783b82a15a587ae458adf17a18`. Both reviewed head and merge have
tree `f7e14c5d7b52a4d667ef24511dfb0594167979b7`. Immutable Git objects, fetched
refs and GitHub PR data establish these relationships; moving main is not a pin.

Archive: `https://codeload.github.com/zackboll/ada-gnatdoc-enhancer/tar.gz/53a321915da2c5a02a54581040673b61f8595b25`.
Recomputed SHA-256:
`d324735784435176c0f1a7093cf462e15b2bd92be429ef5e1e42e802331ca52e`.
All 1,089 archive files equal corresponding immutable Git blobs. Fresh non-editable
installation matches all 29 implementation/assets files in the archive exactly.
The complete sorted relative-name/SHA-256 mapping, serialized as compact sorted
JSON and then SHA-256 hashed, is
`54d9327e57662a5843198fbc5352d4c5f544a3d5e3047c8269c6b9c68c5e878b`.
Only runtime `__pycache__` is excluded. Version `0.1.0.dev0` is also checked but
does not distinguish revisions. Installation enforces archive and package hashes.

Recomputed assets:

| Asset | SHA-256 |
|---|---|
| `gnatdoc-navigation.js` | `b3fb428b507844d6be76fb40156058d39a5b5f57004bde04e33b0411f68504cd` |
| `gnatdoc-theme.css` | `cf06637732a8b047a7191bfb991a5c3da7cd31a52076876880d5f8a3f63929bc` |

## Complete implementation/assets delta

The full old-to-new repository diff is retained externally, separately from the
bounded implementation/assets diff (1,230 lines). Runtime changes are confined to:

- `assets/gnatdoc-navigation.js`: exact native sidebar destination allowlist,
  declaration-aware canonical queries, outer-owned history using iframe
  `location.replace`, popstate restoration, same-destination deduplication,
  approved inner-link interception and stale-load handling. Search remains
  case-insensitive substring filtering of existing native sidebar labels.
- `assets/gnatdoc-theme.css`: exactly the four dormant rules below.
- `navigation.py`: opt-in operations categories and source/native destination
  resolution; package mode retains the original groups and only inserts controls
  and the local script. No categories are configured by MEL.
- New `destinations.py`: source/native reconciliation for operation/authored
  features; no synthetic hashes are used to manufacture navigation targets.
- New `generic_instances.py`, extended `config.py`, `pipeline.py` and
  `qualification.py`: optional hash-bound generic qualification and summaries.
  With MEL's absent qualification, summaries return empty output/counts.
- New `authored.py`, extended `guides.py`, `validation.py` and `config.py`:
  optional authored HTML/Markdown contracts and validation. Guides `none`
  returns no pages; MEL continues using its own unchanged portal renderer.
- `util.py`: stricter malformed/double-encoded URL rejection, including when
  optional authored features are absent. No new remote permission is granted.

MEL's consumer compares the **complete parsed configuration** to its own exact
dictionary before loading the enhancer. Only package navigation, theme, lexical
highlighting and local type links are enabled. Operation/category, external
mapping/dependency/security host, generic qualification and authored-guide
configuration mutations are rejected. New-off and packages controls confirm no
extra HTML, generic counters, portal or package-page changes from these features.

The exact shared CSS delta is retained intact, inserted immediately before
`body.main`:

```css
.generic-instance-summary table { width: 100%; border-collapse: collapse; table-layout: fixed; }
.generic-instance-summary th, .generic-instance-summary td { border: 1px solid var(--border); padding: .5rem; text-align: left; vertical-align: top; overflow-wrap: anywhere; }
.generic-instance-summary th:first-child { width: 34%; }
.generic-instance-summary:target { border-left: 3px solid var(--link); padding-left: .75rem; }
```

There is no consumer fork, stripped asset, generic-summary HTML or generic counter.

## Consumer-owned index insertion contract

Exactly one control block, immediately before `<h2 id=compilation-units>`:

```html
<div id="package-navigation" hidden><label for="package-search">Search documentation</label><input id="package-search" type="search" autocomplete="off" spellcheck="false"><button id="package-clear" type="button">Clear</button><p id="package-results" role="status" aria-live="polite"></p></div>
```

Exactly one script, immediately before `<body class=main>`:

```html
<script src="gnatdoc-navigation.js" defer></script>
```

The consumer owns these literal expectations, checks their unique exact placement,
removes only those insertions, and feeds the remaining index to the unchanged
original Page checker. That checker still rejects all scripts globally. Complete
pristine-index byte equality after metadata removal rejects altered labels,
ordering, grouping, iframe attributes, additional resources and wrong-but-existing
destinations even with a consistently forged output manifest. Every package page
still passes independent type-occurrence restoration and complete pristine byte
comparison. Authored files and native CSS must match directly. Full pinned output
and manifest reconstruction remain additional checks, not permission authorities.
Publication JavaScript must equal the independently pinned complete asset hash.

## Native authority and declaration-history boundary

The trusted pristine native index provides all 29 package and eight tagged
destinations. Source inventory and the unchanged anchor ledger remain authority;
no navigation permission manifest is added. Package mode adds no anchor entries.
Most of the 1,418 local type links lead to ordinary declarations not represented
in the sidebar. Those remain native links, not automatically selected/bookmarked
outer destinations. Search covers 37 labels, not 1,237 sections or full text.

The old pin tracks only package identity. In an explicitly experimental old-pin
navigation-enabled copy, the two RF C2 tagged entries `Job_Request` and
`Virtual_Aperture` cannot have distinct declaration-aware outer history. This is
not an observed DOC-007D production failure: production had navigation disabled.
The new pin uses the native package filename **and existing declaration fragment**
as destination identity. Fresh browser controls use the same native URLs,
heading/profile assertions and history sequence; detailed observations and any
failed harness attempts are external qualification evidence.

The R2 fresh-external-audit contract remains unchanged, including resolved checked
destination, exclusive creation, recheck before writing and post-return hashes.
Trusted filesystems and stable parents are required; no hostile-filesystem
sandbox guarantee is added.
