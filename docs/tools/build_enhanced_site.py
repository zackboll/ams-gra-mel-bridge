#!/usr/bin/env python3
"""Build/verify MEL presentation against independently pinned native/source bytes."""

import argparse
import hashlib
import html
import importlib.metadata
import json
import re
import shutil
import sys
import tempfile
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
sys.path.insert(0, str(ROOT / "docs/tools"))
from check_doc_coverage import measure, ratchet
from gnatdoc_inventory import specifications
from validate_docs import Page, validate as validate_pristine
from local_type_audit import Authority, restore_page, require_complete_additions, occurrence_key
from example_sources import References, occurrence_ledger
import symbol_audit
import api_catalog

REVISION = "524b7610e21e9a7b87e8e0761580ed59a3b0295c"
ARCHIVE_SHA256 = "8b7c0a0d0957ef2ebb2cc27888173e653db6987c4279444fcffe095ccc099101"
PACKAGE_SHA256 = "af5c5fec03ec3fbc41e3a55a55424a3ce65cf1bb5101d97b0c65ff5614e40632"
JAVASCRIPT_SHA256 = "233cf43211a711d0d64dfba2815bc2d9cc887720faed0ca38f464bc6d29e145f"
SOURCE_SHA256 = "467cd45ef85888bb22ccee2d9e325b6b55e985dadac66c317dbbc653193114c8"
ANCHORS_SHA256 = "06b602439be15557118790a2827bb8bacc45c1b02d055fa3e9502204857f5aed"
# Complete ordered records independently recovered with the DOC-007E auditor
# from control A, not an enhancer manifest or a count-based completeness claim.
ORIGINAL_AUDIT_SHA256 = "c943b0fd5ff0491ed965dac2ff0cbce180b452fca538e6b87adc7192e8420e5c"
ORIGINAL_PRISTINE_SHA256 = "3a853e1a6c06e19f7f21d45622de3135054e7beb711245b1838555246c0d6c49"
PRISTINE_SHA256 = "2ee21243c766d5183587633947f055a1602c4d744040218db13ee01a8eb33291"
NATIVE_API_SHA256 = "d2026657ddd02d2513514c545633c13fbaf8ce772e1bf35bf51223e16d811983"
# Preserve the entire DOC-011 API through an independently restored projection,
# as well as guarding the new complete symbol-enabled API. Both values come from
# qualified complete A/B/C bytes, not enhancer metadata or predicted link counts.
ORIGINAL_ENHANCED_API_SHA256 = "8f5090330038b70905211fd8ee32a2d662ee7902a27c9c2dc61028b19c48385f"
ENHANCED_API_SHA256 = "9790f7666b867250e8c78e998a32eb233c3cbe76f9e83bbda7f4d8978f0b6f37"
PUBLICATION_SHA256 = "2249019abba15ec3107069c18dea5e0934eab25d303524e9a5e9768b476838cb"
ORIGINAL_PUBLICATION_SHA256 = "22e51b0a72744cf4a18991b54433ec802872d6a427d5cee1fe97a8ac33cd83d7"
EXTRAS = {"api/gnatdoc-theme.css", "api/presentation.json", "api/gnatdoc-navigation.js"}
METADATA = ('<meta name="viewport" content="width=device-width, initial-scale=1">'
            '<link rel="stylesheet" href="gnatdoc-theme.css">')
SPAN = re.compile(r'<span class="ada-(?:keyword|number|string|character|attribute|comment|operator)">([^<]*)</span>')
# Consumer-owned byte contracts, deliberately not imported from the enhancer or
# read from a publication manifest. Native index bytes remain the destination
# authority; restoring these two insertions must recover that entire index.
NAVIGATION_CONTROL = ('<div id="package-navigation" hidden><label for="package-search">'
                      'Search documentation</label><input id="package-search" type="search" '
                      'autocomplete="off" spellcheck="false">'
                      '<button id="package-clear" type="button">Clear</button>'
                      '<p id="package-results" role="status" aria-live="polite"></p></div>')
NAVIGATION_SCRIPT = '<script src="gnatdoc-navigation.js" defer></script>'
CONFIG = {
    "schema_version": 1, "project": "AMS MEL Bridge",
    "renderer_exceptions": ["AMS.MEL.IR.Health_Status.Metadata.Status"],
    "gnatdoc": {"version": "26.0.0"},
    "sources": {"directories": ["ada/src"], "comment_style": "trailing"},
    "navigation": {"mode": "packages", "symbol_bookmarks": True, "default_package": "AMS.MEL"},
    "presentation": {"theme": True, "syntax_highlighting": True, "type_links": True},
    "guides": {"mode": "none"},
}


class PresentationError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise PresentationError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def fingerprint(mapping):
    return sha(json.dumps(mapping, sort_keys=True, separators=(",", ":")).encode())


def files(directory):
    directory = Path(directory).absolute()
    require(not any(p.is_symlink() for p in (directory, *directory.parents)), "symlink input/output root")
    require(directory.is_dir(), "missing input/output directory")
    result = {}
    for path in sorted(directory.rglob("*")):
        require(not path.is_symlink(), "symlink input/output member")
        if path.is_dir():
            continue
        require(path.is_file(), "nonregular input/output member")
        result[path.relative_to(directory).as_posix()] = path.read_bytes()
    return result


def hashes(mapping):
    return {name: sha(data) for name, data in mapping.items()}


def audit_destination(path, root, pristine, published):
    """Require a fresh external file; never follow an existing output alias.

    Resolve parent aliases before containment checks and use that checked path
    for exclusive creation. As with input qualification, parent directories must
    be trusted and not concurrently replaced while validation runs.
    """
    try:
        supplied = Path(path)
        require(not supplied.exists() and not supplied.is_symlink(),
                "audit destination must be a fresh external file")
        destination = supplied.resolve()
        protected = (Path(tree).resolve() for tree in (root, pristine, published))
        require(not any(destination.is_relative_to(tree) for tree in protected),
                "audit destination must be outside repository, pristine and publication trees")
        require(destination.parent.is_dir(), "audit destination parent must already exist")
        return destination
    except (OSError, RuntimeError) as error:
        raise PresentationError("invalid audit destination: " + str(error)) from error


def tool():
    import ada_gnatdoc_enhancer
    package = Path(ada_gnatdoc_enhancer.__file__).parent
    content = {name: data for name, data in files(package).items()
               if "__pycache__" not in Path(name).parts}
    require(importlib.metadata.version("ada-gnatdoc-enhancer") == "0.1.0.dev0", "enhancer version mismatch")
    require(fingerprint(hashes(content)) == PACKAGE_SHA256, "enhancer implementation/assets mismatch")


def qualified_original(pristine, root=ROOT):
    raw = files(pristine)
    require(len(raw) == 40 and fingerprint(hashes(raw)) == PRISTINE_SHA256,
            "pristine native/portal bytes differ from reviewed baseline")
    # Preserve the entire historical DOC-010/011 publication projection, not just
    # API-only hashes. The independently validated addition has one bounded home.
    original = dict(raw)
    text = original["guides/api-map.html"].decode()
    start, end = '<nav aria-label="Declaration indexes">', '<h2>Required, conditional and represented-only</h2>'
    require(text.count(start) == text.count(end) == 1, "catalog projection boundaries changed")
    text = text[:text.index(start)] + text[text.index(end):]
    original["guides/api-map.html"] = text.encode()
    require(fingerprint(hashes(original)) == ORIGINAL_PRISTINE_SHA256,
            "complete inherited pristine publication projection changed")
    require(fingerprint(hashes({n: d for n, d in raw.items() if n.startswith("api/")}))
            == NATIVE_API_SHA256, "complete inherited pristine API inventory/bytes changed")
    sources = {"ada/src/" + n: data for n, data in files(root / "ada/src").items()}
    require(fingerprint(hashes(sources)) == SOURCE_SHA256, "approved source-profile identity changed")
    anchors = (root / "docs/gnatdoc-native-anchors.json").read_bytes()
    require(sha(anchors) == ANCHORS_SHA256, "authoritative anchors/profiles changed")
    require(tomllib.loads((root / "docs/gnatdoc-enhance.toml").read_text()) == CONFIG,
            "unapproved presentation configuration")
    example_links = len(occurrence_ledger(root, References(root, Path(pristine))))
    catalog = api_catalog.inventory(root, References(root, Path(pristine)), independent=True)
    require(validate_pristine(pristine) == {"html_pages": 38, "links": 1562 + example_links + 3 + api_catalog.link_count(catalog)}, "pristine metrics changed")
    report = measure(pristine, root, json.loads(anchors))
    ratchet(report, (root / "docs/gnatdoc-coverage-baseline.txt").read_text())
    require((report["source_declarations"], report["native_sections"],
             report["documented_elements"], report["total_elements"],
             len(report["renderer_exceptions"])) == (1238, 1237, 4294, 4295, 1), "coverage changed")
    return raw


def restore_navigation(text, symbols):
    """Remove only the exact, uniquely placed index-only exceptions."""
    try:
        text = symbol_audit.restore(text, symbols)
    except ValueError as error:
        raise PresentationError(str(error)) from error
    for insertion, following, label in (
            (NAVIGATION_CONTROL, '<h2 id=compilation-units>', "controls"),
            (NAVIGATION_SCRIPT, '</head><body class=main>', "script")):
        require(text.count(insertion) == 1 and text.count(insertion + following) == 1,
                "missing/duplicate/altered/misplaced navigation " + label)
        text = text.replace(insertion, "", 1)
    return text


def restore_document(text, native, name, authority):
    """Independent serialized boundary: exact source title, not HTMLParser RCDATA.

    Compare the complete head bytes, including ordering, attributes and title
    serialization. No character-reference equivalence, nested-looking markup,
    comments, controls or alternative encodings are authorized. After removing
    this bounded prefix, reject any remaining metadata element before restoring
    snippets; complete native byte comparison remains a separate requirement.
    The caller has already removed the uniquely placed index-only script.
    """
    index = name == 'api/index.html'
    package = authority.page_packages.get(Path(name).name)
    require(index or package is not None, 'document metadata lacks source package identity')
    title = ('API Reference' if index else package) + ' — ' + CONFIG['project']
    prologue = '<!DOCTYPE html>\n<html class=main>\n'
    require(native.startswith(prologue + '<link rel=stylesheet href=gnatdoc.css>'),
            'unqualified native document layout')
    prefix = ('<!DOCTYPE html>\n<html class=main lang="en">\n<head><meta charset="utf-8">'
              '<title>' + html.escape(title) + '</title><link rel=stylesheet href=gnatdoc.css>' +
              METADATA + '</head>')
    require(text.startswith(prefix), 'incorrect/missing/duplicate/misplaced document metadata: ' + name)
    rest = text[len(prefix):]
    require(rest.startswith('<body class=' + ('main' if index else 'content') + '>'),
            'misplaced document head/body: ' + name)
    require(re.search(r'</?(?:html|head|title|meta|link|script)\b', rest, re.I) is None,
            'extra/misplaced document metadata or script: ' + name)
    return prologue + '<link rel=stylesheet href=gnatdoc.css>' + rest


def validate(published, pristine, root=ROOT, audit_path=None):
    """Neither an output-side manifest nor enhancer self-verification is authority."""
    destination = audit_destination(audit_path, root, pristine, published) if audit_path is not None else None
    tool()
    raw = qualified_original(pristine, root)
    authority = Authority(root, Path(pristine))
    symbols = symbol_audit.inventory(authority, Path(pristine))
    expected_additions = authority.required_additions()
    actual = files(published)
    require(set(actual) == set(raw) | EXTRAS, "missing/unexpected publication files")
    require(sha(actual["api/gnatdoc-navigation.js"]) == JAVASCRIPT_SHA256,
            "navigation JavaScript asset identity mismatch")
    for name, data in raw.items():
        if not (name.startswith("api/") and name.endswith(".html")):
            require(actual[name] == data, "changed native CSS or authored portal: " + name)
    # Independently accept each occurrence before removing exact type wrappers
    # and lexical spans, only within qualified declaration sections.
    audit = []
    restored = {}
    for name, data in raw.items():
        if not (name.startswith("api/") and name.endswith(".html")):
            restored[name] = actual[name]
            continue
        text = actual[name].decode()
        # The original Page checker still rejects ALL scripts. Remove only the
        # independently approved index insertions before invoking it. Package
        # pages and authored HTML receive no active-content exception.
        safety_text = restore_navigation(text, symbols) if name == "api/index.html" else text
        require(not Page(safety_text).issues, "unsafe HTML")
        text = restore_document(safety_text, data.decode(), name, authority)
        text, occurrences = restore_page(Path(name).name, text, authority)
        audit.extend(occurrences)
        require(text.encode() == data, "native declaration/anchor/link/markup changed: " + name)
        restored[name] = text.encode()
    require_complete_additions(audit, expected_additions)
    new_keys = {occurrence_key(r) for r in expected_additions}
    original_projection = [r for r in audit if occurrence_key(r) not in new_keys]
    require(len(original_projection) == 1418 and fingerprint(original_projection) == ORIGINAL_AUDIT_SHA256,
            "original ordered type-link audit changed")
    # Run the original MEL validator and full source/anchor coverage on the
    # independently restored site. Its allowlist and all original tests stay intact.
    with tempfile.TemporaryDirectory(prefix="mel-native-verification-") as temp:
        base = Path(temp)
        for name, data in restored.items():
            path = base / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        validate_pristine(base)
        measure(base, root, json.loads((root / "docs/gnatdoc-native-anchors.json").read_bytes()))
    from ada_gnatdoc_enhancer.config import load
    from ada_gnatdoc_enhancer.pipeline import expected_output, verify
    from ada_gnatdoc_enhancer.util import sorted_json
    config = load(root, "docs/gnatdoc-enhance.toml")
    with tempfile.TemporaryDirectory(prefix="mel-api-verification-") as temp:
        isolated = Path(temp)
        shutil.copytree(Path(pristine) / "api", isolated / "api")
        _, expected, counts = expected_output(config, isolated)
        # Reconstruct the entire presentation manifest from trusted originals,
        # current authoritative source, and pinned transformations, not itself.
        manifest = {
            "schema_version": 1, "enhancer_version": "0.1.0.dev0",
            "gnatdoc_version": "26.0.0", "project": "AMS MEL Bridge",
            "original_api_html": {n: d.decode() for n, d in raw.items()
                                  if n.startswith("api/") and n.endswith(".html")},
            "native_css_sha256": sha(raw["api/gnatdoc.css"]),
            "source_sha256": {p.relative_to(root).as_posix(): sha(p.read_bytes())
                              for p in specifications(root)[0].values()},
            "output_sha256": hashes({n: d for n, d in actual.items()
                                     if n.startswith("api/") and n not in
                                     {"api/gnatdoc.css", "api/presentation.json"}}),
            "counts": {"declaration_snippets": len(authority.sections),
                       "linked_external_types": 0, "linked_local_types": len(audit),
                       "linked_types": len(audit), "public_packages": len(authority.packages),
                       "unrendered_renderer_exceptions": 1},
        }
        for name, data in expected.items():
            require(actual[name] == data, "not the pinned reconstructed transformation: " + name)
        require(actual["api/presentation.json"] == sorted_json(manifest).encode(), "altered/forged presentation manifest")
        # Recover the complete previously approved enhanced API, including its
        # original manifest. This keeps the inherited complete-byte guard active
        # rather than replacing it with only the new symbol-enabled fingerprint.
        from ada_gnatdoc_enhancer import __file__ as enhancer_file
        original_router = (Path(enhancer_file).parent / "assets/gnatdoc-navigation.js").read_bytes()
        require(sha(original_router) == "b3fb428b507844d6be76fb40156058d39a5b5f57004bde04e33b0411f68504cd",
                "inherited default navigation asset changed")
        projection = {n: d for n, d in actual.items() if n.startswith("api/")}
        projection["api/index.html"] = symbol_audit.restore(
            actual["api/index.html"].decode(), symbols).encode()
        projection["api/gnatdoc-navigation.js"] = original_router
        original_manifest = {**manifest, "output_sha256": hashes({
            n: d for n, d in projection.items() if n not in {"api/gnatdoc.css", "api/presentation.json"}})}
        projection["api/presentation.json"] = sorted_json(original_manifest).encode()
        require(fingerprint(hashes(projection)) == ORIGINAL_ENHANCED_API_SHA256,
                "complete inherited enhanced API projection changed")
        # The enhancer's independent verifier operates on API-only output too.
        for name, data in actual.items():
            if name.startswith("api/"):
                (isolated / name).write_bytes(data)
        verify(config, isolated)
    links = sum(len(Page(data.decode()).links) for n, data in actual.items() if n.endswith(".html"))
    require(counts["linked_local_types"] == len(audit) == counts["linked_types"], "type-link counts disagree with independent audit")
    require(counts["linked_external_types"] == 0, "external type links not authorized")
    example_audit = occurrence_ledger(root, References(root, Path(pristine)))
    catalog = api_catalog.verify(actual["guides/api-map.html"].decode(), root, References(root, Path(pristine)))
    require(links == 1562 + len(example_audit) + 30 + len(audit) + 1 + len(symbols) + 3 + api_catalog.link_count(catalog),
            "presentation link delta differs from independently audited type, example and symbol links")
    require(fingerprint(hashes({n: d for n, d in actual.items() if n.startswith("api/")}))
            == ENHANCED_API_SHA256, "complete inherited enhanced API inventory/bytes changed")
    require(fingerprint(hashes(actual)) == PUBLICATION_SHA256, "complete qualified publication bytes changed")
    inherited = dict(actual)
    text = inherited["guides/api-map.html"].decode()
    start, end = '<nav aria-label="Declaration indexes">', '<h2>Required, conditional and represented-only</h2>'
    inherited["guides/api-map.html"] = (text[:text.index(start)] + text[text.index(end):]).encode()
    require(fingerprint(hashes(inherited)) == ORIGINAL_PUBLICATION_SHA256,
            "complete inherited final publication projection changed")
    if destination is not None:
        # Recheck freshness at write time; exclusive creation also rejects a
        # newly introduced file/hardlink/symlink instead of overwriting it.
        destination = audit_destination(destination, root, pristine, published)
        try:
            with destination.open("x", encoding="utf-8") as stream:
                stream.write(json.dumps(audit, indent=2, sort_keys=True) + "\n")
        except OSError as error:
            raise PresentationError("cannot create audit destination: " + str(error)) from error
    return {"files": 43, "html_pages": 38, "links": links, "public_packages": 29,
            "example_inline_links": len(example_audit),
            "api_catalog": catalog,
            "public_symbols": symbols,
            "source_declarations": 1238, "native_sections": 1237,
            "linked_local_types": len(audit), "linked_external_types": 0,
            "enhancer_revision": REVISION, "archive_sha256": ARCHIVE_SHA256,
            "document_metadata": {
                "api/" + page: {"title": package + " — " + CONFIG["project"],
                                 "lang": "en", "charset": "utf-8"}
                for page, package in sorted({**authority.page_packages, "index.html": "API Reference"}.items())},
            "output_sha256": hashes(actual)}


def build(pristine, output, root=ROOT):
    tool()
    qualified_original(pristine, root)
    pristine, output = Path(pristine).absolute(), Path(output).absolute()
    require(not output.exists() and not output.is_symlink(), "output must be a fresh directory")
    require(not any(p.is_symlink() for p in output.parents), "symlink output ancestor")
    require(not output.is_relative_to(pristine), "output inside pristine site")
    from ada_gnatdoc_enhancer.config import load
    from ada_gnatdoc_enhancer.pipeline import enhance, verify
    with tempfile.TemporaryDirectory(prefix="mel-presentation-") as temp:
        stage = Path(temp) / "api-only"
        stage.mkdir()
        shutil.copytree(pristine / "api", stage / "api")
        config = load(root, "docs/gnatdoc-enhance.toml")
        enhance(config, stage)
        verify(config, stage)
        complete = Path(temp) / "complete"
        shutil.copytree(pristine, complete)
        # Copy over original API files plus exactly the three approved additions.
        shutil.copytree(stage / "api", complete / "api", dirs_exist_ok=True)
        result = validate(complete, pristine, root)
        output.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(complete, output)
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("build", "verify"))
    parser.add_argument("--pristine", type=Path, default=ROOT / "_site")
    parser.add_argument("--site", type=Path, default=ROOT / "_site-published")
    parser.add_argument("--audit", type=Path, help="fresh file outside repository/pristine/publication; existing parent required (verify only)")
    args = parser.parse_args()
    require(args.audit is None or args.command == "verify", "--audit requires verify")
    result = build(args.pristine, args.site) if args.command == "build" else validate(args.site, args.pristine, audit_path=args.audit)
    print(json.dumps(result, indent=2, sort_keys=True))
