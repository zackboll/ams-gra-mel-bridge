"""Regenerated, Git-bound feature documentation evidence; no epoch self-approval.

Historical publication continues through build_enhanced_site unchanged. This
independent candidate path consumes Git objects and pinned GNATdoc output rather
than changing any historical fingerprints or accepting a candidate anchor file.
"""

import argparse
import ast
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

import feature_source as source

# No future source is preapproved. Entries live in reviewed target policy, never
# the candidate artifact. Each binds the *post-merge* feature commit/tree, full
# production-source fingerprint and complete pristine/publication fingerprints.
# A subsequent policy-only approval commit can name a feature merge that already
# exists; the feature's first main push remains mechanically testable but blocked
# from publication. PR head identity is never substituted for the merge identity.
REVIEWED_EPOCHS = ()
HISTORICAL_PRODUCTION = (
    (source.HISTORICAL_MAIN, source.HISTORICAL_TREE),
    ("33720d155d99f183229b6ed87037ecbd53a3875b", "76d7988ea723a07ced054c0275e80763cbdcb772"),
)


def production_fingerprint(root, candidate):
    """Complete compiled/binding/provider/vendor/test inputs, not artifact claims."""
    entries = source.tree(root, candidate)
    protected = {name: list(value) for name, value in entries.items()
                 if name.startswith(('native/', 'ada/', 'rust/', 'python/', 'integration/'))
                 or name == 'Makefile' or name.endswith('.gpr')}
    require(protected, 'empty production-source identity')
    return sha(canonical(protected))


def trusted_publication_epochs(root, target):
    """Read a literal registry from the event's trusted target Git object only."""
    entries = source.tree(root, target)
    name = 'docs/tools/source_epoch.py'
    if name not in entries:
        return ()  # Initial policy merge has no previously approved feature epoch.
    statements = ast.parse(source.blob(root, target, name)).body
    definitions = [node for node in statements if isinstance(node, ast.Assign)
                   and any(isinstance(t, ast.Name) and t.id == 'REVIEWED_EPOCHS' for t in node.targets)]
    require(len(definitions) == 1, 'missing/duplicate trusted epoch registry')
    value = ast.literal_eval(definitions[0].value)
    require(isinstance(value, tuple), 'trusted epoch registry must be a literal tuple')
    return value


def publication_authority(root, candidate, *, target=None, pristine=None, published=None):
    """Separate source approval from mechanical success; no manifest permission.

    Missing authority rejects. The historical shortcut requires exact reviewed
    production bytes AND both complete historical publication fingerprints.
    Feature approval comes only from the trusted target's literal registry.
    """
    commit = source.revision(root, candidate)
    tree = source.git(root, "rev-parse", commit + "^{tree}").decode().strip()
    require(target is not None and pristine is not None and published is not None,
            'no independently reviewed publication epoch for candidate')
    import build_enhanced_site as p
    production = production_fingerprint(root, commit)
    if p.fingerprint(pristine) == p.PRISTINE_SHA256 and p.fingerprint(published) == p.PUBLICATION_SHA256:
        for ancestor, expected_tree in HISTORICAL_PRODUCTION:
            require(source.git(root, 'rev-parse', ancestor + '^{tree}').decode().strip() == expected_tree,
                    'historical approved production tree mismatch')
            if production == production_fingerprint(root, ancestor):
                return {'kind': 'historical-preservation', 'commit': commit, 'tree': tree,
                        'reviewed_commit': ancestor, 'reviewed_tree': expected_tree,
                        'production_sha256': production}
    for entry in trusted_publication_epochs(root, target):
        require(set(entry) == {'commit', 'tree', 'production_sha256', 'pristine_sha256', 'published_sha256'},
                'invalid trusted publication approval schema')
        approved = source.revision(root, entry['commit'])
        actual_tree = source.git(root, 'rev-parse', approved + '^{tree}').decode().strip()
        require(approved == entry['commit'] and actual_tree == entry['tree'], 'reviewed post-merge identity mismatch')
        # The approval policy target must descend from the actual feature merge;
        # neither a candidate-provided registry nor a PR-head/merge swap qualifies.
        import subprocess
        ancestral = all(subprocess.run(['git', '-C', str(root), 'merge-base', '--is-ancestor', approved, ref],
                                       capture_output=True).returncode == 0 for ref in (target, commit))
        if (ancestral and production == entry['production_sha256'] == production_fingerprint(root, approved)
                and p.fingerprint(pristine) == entry['pristine_sha256']
                and p.fingerprint(published) == entry['published_sha256']):
            return {'kind': 'reviewed-feature-epoch', 'commit': commit, 'tree': tree,
                    'reviewed_commit': approved, 'reviewed_tree': actual_tree,
                    'production_sha256': production}
    raise ValueError("source epoch: no independently reviewed publication epoch for candidate")


def readonly_qualification_inputs(root, target, candidate):
    """Policy inputs must come from the exact trusted target, not candidate edits."""
    protected = (
        "docs/gnatdoc-coverage-baseline.txt", "docs/gnatdoc-warning-baseline.txt",
        "docs/gnatdoc-renderer-exceptions.json", "docs/gnatdoc-native-anchors.json",
        "docs/gnatdoc-enhance.toml", "docs/toolchain/alire.toml",
        "docs_api.gpr", "docs_internal.gpr", "docs/examples/manifest.json",
    )
    old, new = source.tree(root, target), source.tree(root, candidate)
    require(all(old.get(p) == new.get(p) for p in protected), "candidate changes source/publication authority inputs")


def require(condition, message):
    if not condition:
        raise ValueError("source epoch: " + message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def canonical(value):
    return (json.dumps(value, sort_keys=True, separators=(",", ":")) + "\n").encode()


def export(root, commit, destination):
    objects = source.tree(root, commit)
    for name, (mode, oid) in objects.items():
        require(mode in {"100644", "100755"}, "nonregular source object: " + name)
        path = destination / name
        require(not path.is_absolute() or path.is_relative_to(destination), "unsafe source path")
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(source.git(root, "cat-file", "blob", oid))
        path.chmod(0o755 if mode == "100755" else 0o644)


def run(command, cwd, log):
    result = subprocess.run(command, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    with log.open("ab") as stream:
        stream.write(("$ " + " ".join(map(str, command)) + "\n").encode() + result.stdout)
    require(result.returncode == 0, "command failed; see " + str(log))


def toolchain(prefix):
    prefix = Path(prefix).resolve()
    version = subprocess.run([str(prefix / "bin/gnatdoc"), "--version"], capture_output=True, text=True)
    require((version.stdout + version.stderr).strip() == "GNATdoc 26.0.0 (20260415)", "unqualified GNATdoc")
    require(subprocess.check_output(["gnat", "--version"], text=True).splitlines()[0] == "GNAT 16.1.0", "unqualified GNAT")
    require(subprocess.check_output(["gprbuild", "--version"], text=True).startswith("GPRBUILD 26.0.0 (2026-04-15)"), "unqualified GPRbuild")
    return prefix


def generate(root, commit, directory, prefix, log):
    """Generate in a fresh exported tree, using trusted Python consumers only."""
    import sys
    trusted = Path(__file__).resolve().parents[2]
    sys.path.insert(0, str(trusted / "scripts"))
    from gnatdoc_inventory import specifications
    from check_doc_coverage import measure, ratchet
    from build_docs_portal import manifest, package_map, regular, render
    from ada_examples import References

    export(root, commit, directory)
    public, _ = specifications(directory)
    obj = directory / "obj/docs"
    obj.mkdir(parents=True)
    (obj / "public-sources.txt").write_text("".join(p.name + "\n" for p in public.values()))
    run(["gprbuild", "-p", "-P", "docs_api.gpr", "-u", "-c", "-gnatc"], directory, log)
    site = directory / "_site"
    native = subprocess.run([str(prefix / "bin/gnatdoc"), "-P", "docs_api.gpr", "--backend", "html",
                             "--generate", "public", "--style", "trailing", "--warnings", "-O", str(site / "api")],
                            cwd=directory, capture_output=True, text=True)
    warning_text = native.stdout + native.stderr
    (obj / "gnatdoc.log").write_text(warning_text)
    with log.open("ab") as stream:
        stream.write(warning_text.encode())
    require(native.returncode == 0 and not re.search(r"(^|\s)(error:|fatal:|ignore file due to)", warning_text, re.I), "incomplete native generation")
    # Same original warning comparator/ratchet; no candidate baseline expansion.
    run(["python3", str(trusted / "scripts/compare_gnatdoc_warnings.py"), "--fail-on-removed",
         "--sources", str(directory / "ada/src"),
         str(directory / "docs/gnatdoc-warning-baseline.txt"), str(obj / "gnatdoc.log")], directory, log)
    report = measure(site, directory)
    ratchet(report, (directory / "docs/gnatdoc-coverage-baseline.txt").read_text())
    ledger = report["native"]
    references = References(directory, site, ledger=ledger)
    data = manifest(directory)
    packages = package_map(site, directory)
    for name in [data["home"], *(e["path"] for e in data["guides"]), data["stylesheet"]]:
        text = regular(directory / "docs" / name)
        output = render(text, name, data, packages, directory, references) if name.endswith(".html") else text
        path = site / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(output)
    return site, report, references


def inherited_declarations(old_site, old_report, new_site, new_report):
    """Unchanged source identities keep exact destinations and complete sections."""
    from gnatdoc_inventory import specifications
    from gnatdoc_coverage_model import specification
    from check_doc_coverage import declaration_identity, attached_block

    def source_blocks(site):
        result = {}
        for package, path in specifications(site.parent)[0].items():
            _, lines, _, _, declarations, _ = specification(path)
            for declaration in declarations:
                result[declaration_identity(package, lines, declaration)] = attached_block(lines, declaration)
        return result

    old_blocks, new_blocks = source_blocks(old_site), source_blocks(new_site)
    old, new = old_report["native"], new_report["native"]
    require(set(old) <= set(new), "historical declaration removed or profile changed")
    for identity, entry in old.items():
        require(new[identity] == entry, "historical destination/profile changed: " + identity)
        name, anchor = entry["page"], entry["anchor"]
        marker = "<h4 id=" + anchor + ">"
        def section(site):
            text = (site / "api" / name).read_text()
            require(text.count(marker) == 1, "missing/duplicate historical anchor")
            tail = text.split(marker, 1)[1]
            return marker + tail.split("<h4 ", 1)[0].split("<h3", 1)[0].split("<h2", 1)[0].removesuffix("</body></html>")
        if old_blocks[identity] == new_blocks[identity]:
            require(section(old_site) == section(new_site), "complete historical declaration documentation changed: " + identity)
        # Deliberately edited source prose must already pass measure()/ratchet.
        # Its new complete rendered bytes come only from fresh pinned GNATdoc,
        # not output metadata. Unchanged declaration profiles/destinations above
        # remain mandatory, and every unchanged attached block remains byte-exact.
    return len(new) - len(old)


def enhance(site, root, report, references, output):
    """Pinned renderer plus consumer-owned restoration and independent catalog."""
    import build_enhanced_site as p
    from local_type_audit import Authority, restore_page, lex, IDENT, occurrence_key
    from ada_gnatdoc_enhancer.config import load
    from ada_gnatdoc_enhancer.pipeline import enhance as renderer, verify
    from ada_gnatdoc_enhancer.util import sorted_json
    import api_catalog
    import symbol_audit
    p.tool()
    # Renderer needs a native ledger; supply only the independently measured one
    # in the disposable export, never authorize/modify the candidate's ledger.
    (root / "docs/gnatdoc-native-anchors.json").write_text(json.dumps(report["native"]))
    config = load(root, "docs/gnatdoc-enhance.toml")
    api_stage = output.parent / (output.name + "-api")
    api_stage.mkdir()
    shutil.copytree(site / "api", api_stage / "api")
    renderer(config, api_stage)
    verify(config, api_stage)
    shutil.copytree(site, output)
    shutil.copytree(api_stage / "api", output / "api", dirs_exist_ok=True)
    authority = Authority(root, site, ledger=report["native"])
    symbols = symbol_audit.inventory(authority, site)
    raw, actual = p.files(site), p.files(output)
    require(set(actual) == set(raw) | p.EXTRAS, "unexpected/missing publication files")
    require(p.sha(actual["api/gnatdoc-navigation.js"]) == p.JAVASCRIPT_SHA256, "router asset changed")
    audit = []
    for name, data in raw.items():
        if name.startswith("api/") and name.endswith(".html"):
            text = actual[name].decode()
            safe = p.restore_navigation(text, symbols) if name == "api/index.html" else text
            require(not p.Page(safe).issues, "unsafe publication HTML")
            restored = p.restore_document(safe, data.decode(), name, authority)
            restored, occurrences = restore_page(Path(name).name, restored, authority)
            require(restored.encode() == data, "incomplete/fabricated native publication: " + name)
            audit.extend(occurrences)
        else:
            require(actual[name] == data, "changed CSS or authored page: " + name)
            if name.endswith(".html"):
                require(not p.Page(data.decode()).issues, "unsafe authored publication HTML")
            elif name.endswith(".css"):
                css = re.sub(r"/\*.*?\*/", "", data.decode(), flags=re.S)
                require("\\" not in css and not re.search(
                    r"@import|url\s*\(|expression\s*\(|-moz-binding|behavior\s*:", css, re.I),
                    "unsafe authored publication CSS")
    # Theme CSS is an additional renderer asset, not native input. Its complete
    # immutable installed bytes remain authority even for a changed source epoch.
    from ada_gnatdoc_enhancer.theme import asset
    require(actual['api/gnatdoc-theme.css'] == asset('gnatdoc-theme.css'), 'theme asset identity mismatch')
    manifest = {
        "schema_version": 1, "enhancer_version": "0.1.0.dev0", "gnatdoc_version": "26.0.0",
        "project": "AMS MEL Bridge", "original_api_html": {n: d.decode() for n, d in raw.items() if n.startswith("api/") and n.endswith(".html")},
        "native_css_sha256": sha(raw["api/gnatdoc.css"]),
        "source_sha256": {p.relative_to(root).as_posix(): sha(p.read_bytes()) for p in __import__('gnatdoc_inventory').specifications(root)[0].values()},
        "output_sha256": p.hashes({n: d for n, d in actual.items() if n.startswith("api/") and n not in {"api/gnatdoc.css", "api/presentation.json"}}),
        "counts": {"declaration_snippets": len(authority.sections), "linked_external_types": 0,
                   "linked_local_types": len(audit), "linked_types": len(audit),
                   "public_packages": len(authority.packages), "unrendered_renderer_exceptions": 1},
    }
    # Enumerate every eligible occurrence from plain native snippets, independently
    # of renderer wrappers and counts, including newly added declarations.
    expected_occurrences = []
    for (page, anchor), entry in authority.sections.items():
        words = lex(entry['native'])
        for i, token in enumerate(words):
            if not IDENT.fullmatch(token[0]) or (i and words[i - 1][0] == '.'):
                continue
            end = i
            while end + 2 < len(words) and words[end + 1][0] == '.' and IDENT.fullmatch(words[end + 2][0]):
                end += 2
            spelling = entry['native'][token[1]:words[end][2]]
            try:
                identity, destination = authority.destination(spelling, entry['package'], entry)
                expected_occurrences.append(authority.occurrence(
                    page, anchor, entry['native'], token[1], words[end][2], spelling,
                    destination['page'] + '#' + destination['anchor'], identity))
            except ValueError:
                continue  # Ineligible grammar cannot acquire a type link.
    require(len(audit) == len(expected_occurrences) and
            {occurrence_key(r): r for r in audit} == {occurrence_key(r): r for r in expected_occurrences},
            "missing/extra source-derived type-link occurrence")
    require(actual["api/presentation.json"] == sorted_json(manifest).encode(), "source/renderer manifest mismatch")
    api_catalog.verify(actual["guides/api-map.html"].decode(), root, references)
    # Every old/new link must resolve; allowed navigation script is already pinned.
    pages = {n: p.Page(d.decode()) for n, d in actual.items() if n.endswith('.html')}
    from urllib.parse import urlsplit, unquote
    import posixpath
    for name, page in pages.items():
        require(len(page.ids) == len(set(page.ids)), "duplicate publication anchor")
        for link in page.links:
            parts = urlsplit(link)
            require(not parts.scheme and not parts.netloc, "external publication link")
            dest = posixpath.normpath(posixpath.join(posixpath.dirname(name), unquote(parts.path))) if parts.path else name
            require(dest in actual, "missing publication destination")
            if parts.fragment:
                require(dest in pages and unquote(parts.fragment) in pages[dest].ids, "missing publication fragment")
    return {"symbols": symbols, "type_occurrences": audit, "files": p.hashes(actual)}


def qualify(root, target, candidate, output, prefix):
    root, output = Path(root).absolute(), Path(output).absolute()
    require(not output.exists(), "output must be fresh")
    evidence = source.qualify(root, target, candidate)
    return qualify_evidence(root, evidence, output, prefix)


def qualify_evidence(root, evidence, output, prefix):
    """Generate from an already established trusted policy/source classification.

    Only the orchestrator's Git/event checks may establish this classification;
    this Python entry is not an artifact manifest authorization interface.
    """
    root, output = Path(root).absolute(), Path(output).absolute()
    # Candidate generation must never mutate the caller's source classification;
    # the outer seal binds that small Git-derived record independently.
    evidence = __import__('copy').deepcopy(evidence)
    require(not output.exists(), "output must be fresh")
    target, candidate = evidence['target_commit'], evidence['candidate_commit']
    readonly_qualification_inputs(root, target, candidate)
    prefix = toolchain(prefix)
    output.parent.mkdir(parents=True, exist_ok=True)
    retained_log = output.parent / (output.name + "-generation.log")
    with tempfile.TemporaryDirectory(prefix="pol002-epoch-") as temp:
        workspace = Path(temp)
        log = retained_log
        old_site, old_report, _ = generate(root, target, workspace / "target", prefix, log)
        new_site, report, references = generate(root, candidate, workspace / "candidate", prefix, log)
        additions = inherited_declarations(old_site, old_report, new_site, report)
        publication = enhance(new_site, workspace / "candidate", report, references, workspace / "published")
        # A second completely separate export and native generation is mandatory.
        second_site, second_report, second_refs = generate(root, candidate, workspace / "second", prefix, log)
        second = enhance(second_site, workspace / "second", second_report, second_refs, workspace / "published-second")
        require(publication == second, "nondeterministic source/native/presentation regeneration")
        import build_enhanced_site as p
        require(p.files(new_site) == p.files(second_site), "nondeterministic pristine generation")
        evidence.update(native=report["native"], documented_elements=report["documented_elements"],
                        total_elements=report["total_elements"], added_declarations=additions,
                        pristine=p.hashes(p.files(new_site)), publication=publication,
                        status="mechanically-qualified-pending-independent-review")
        output.mkdir(parents=True)
        shutil.copytree(new_site, output / "pristine")
        shutil.copytree(workspace / "published", output / "published")
        shutil.copyfile(log, output / "generation.log")
        (output / "epoch.json").write_bytes(canonical(evidence))
    return evidence


def verify(root, directory, target, candidate):
    """Artifacts cannot change source identity or grant approval to themselves."""
    import build_enhanced_site as p
    directory = Path(directory)
    value = json.loads((directory / "epoch.json").read_bytes())
    expected = source.qualify(root, target, candidate)
    require(all(value.get(k) == v for k, v in expected.items()), "source/manifest epoch mismatch")
    require(value.get("status") == "mechanically-qualified-pending-independent-review", "unapproved epoch status")
    require(p.hashes(p.files(directory / "pristine")) == value["pristine"], "pristine artifact mismatch")
    require(p.hashes(p.files(directory / "published")) == value["publication"]["files"], "publication artifact mismatch")
    return value


def verify_regenerated(root, directory, target, candidate, prefix):
    """Independently rebuild; a self-consistent artifact/manifest is not authority."""
    value = verify(root, directory, target, candidate)
    with tempfile.TemporaryDirectory(prefix="pol002-independent-") as temp:
        regenerated = qualify(root, target, candidate, Path(temp) / "epoch", prefix)
        require(value == regenerated, "artifact differs from independently regenerated source epoch")
    return value


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("qualify", "verify"))
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--gnatdoc-prefix", type=Path)
    args = parser.parse_args()
    target = source.trusted_target(args.root)
    result = (qualify(args.root, target, "HEAD", args.directory, args.gnatdoc_prefix)
              if args.command == "qualify" else verify_regenerated(
                  args.root, args.directory, target, "HEAD", args.gnatdoc_prefix))
    print(json.dumps({k: result[k] for k in ("target_commit", "candidate_commit", "candidate_tree", "status")}, indent=2))
