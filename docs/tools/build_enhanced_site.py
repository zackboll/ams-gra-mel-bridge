#!/usr/bin/env python3
"""Build/verify MEL presentation against independently pinned native/source bytes."""

import argparse
import hashlib
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
from local_type_audit import Authority, restore_page

REVISION = "53a321915da2c5a02a54581040673b61f8595b25"
ARCHIVE_SHA256 = "d324735784435176c0f1a7093cf462e15b2bd92be429ef5e1e42e802331ca52e"
PACKAGE_SHA256 = "54d9327e57662a5843198fbc5352d4c5f544a3d5e3047c8269c6b9c68c5e878b"
JAVASCRIPT_SHA256 = "b3fb428b507844d6be76fb40156058d39a5b5f57004bde04e33b0411f68504cd"
SOURCE_SHA256 = "467cd45ef85888bb22ccee2d9e325b6b55e985dadac66c317dbbc653193114c8"
ANCHORS_SHA256 = "06b602439be15557118790a2827bb8bacc45c1b02d055fa3e9502204857f5aed"
PRISTINE_SHA256 = "d076b4e9c69f4ffd7c02e82da711704ca90077a82f0db229176492644e0a903d"
NATIVE_API_SHA256 = "d2026657ddd02d2513514c545633c13fbaf8ce772e1bf35bf51223e16d811983"
ENHANCED_API_SHA256 = "d391e78aca127e3c14cce805b25c17c9ea8638aa21d5d1998be22116c4583b32"
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
    "navigation": {"mode": "packages"},
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
    require(fingerprint(hashes({n: d for n, d in raw.items() if n.startswith("api/")}))
            == NATIVE_API_SHA256, "complete inherited pristine API inventory/bytes changed")
    sources = {"ada/src/" + n: data for n, data in files(root / "ada/src").items()}
    require(fingerprint(hashes(sources)) == SOURCE_SHA256, "approved source-profile identity changed")
    anchors = (root / "docs/gnatdoc-native-anchors.json").read_bytes()
    require(sha(anchors) == ANCHORS_SHA256, "authoritative anchors/profiles changed")
    require(tomllib.loads((root / "docs/gnatdoc-enhance.toml").read_text()) == CONFIG,
            "unapproved presentation configuration")
    require(validate_pristine(pristine) == {"html_pages": 38, "links": 1562}, "pristine metrics changed")
    report = measure(pristine, root, json.loads(anchors))
    ratchet(report, (root / "docs/gnatdoc-coverage-baseline.txt").read_text())
    require((report["source_declarations"], report["native_sections"],
             report["documented_elements"], report["total_elements"],
             len(report["renderer_exceptions"])) == (1238, 1237, 4294, 4295, 1), "coverage changed")
    return raw


def restore_navigation(text):
    """Remove only the exact, uniquely placed index-only exceptions."""
    for insertion, following, label in (
            (NAVIGATION_CONTROL, '<h2 id=compilation-units>', "controls"),
            (NAVIGATION_SCRIPT, '<body class=main>', "script")):
        require(text.count(insertion) == 1 and text.count(insertion + following) == 1,
                "missing/duplicate/altered/misplaced navigation " + label)
        text = text.replace(insertion, "", 1)
    return text


def validate(published, pristine, root=ROOT, audit_path=None):
    """Neither an output-side manifest nor enhancer self-verification is authority."""
    destination = audit_destination(audit_path, root, pristine, published) if audit_path is not None else None
    tool()
    raw = qualified_original(pristine, root)
    actual = files(published)
    require(set(actual) == set(raw) | EXTRAS, "missing/unexpected publication files")
    require(sha(actual["api/gnatdoc-navigation.js"]) == JAVASCRIPT_SHA256,
            "navigation JavaScript asset identity mismatch")
    for name, data in raw.items():
        if not (name.startswith("api/") and name.endswith(".html")):
            require(actual[name] == data, "changed native CSS or authored portal: " + name)
    # Independently accept each occurrence before removing exact type wrappers
    # and lexical spans, only within qualified declaration sections.
    authority = Authority(root, Path(pristine))
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
        safety_text = restore_navigation(text) if name == "api/index.html" else text
        require(not Page(safety_text).issues, "unsafe HTML")
        text = safety_text
        require(text.count(METADATA) == 1, "missing/duplicate viewport/theme metadata")
        require(text.count("gnatdoc-theme.css") == 1, "duplicate theme insertion")
        text = text.replace(METADATA, "", 1)
        text, occurrences = restore_page(Path(name).name, text, authority)
        audit.extend(occurrences)
        require(text.encode() == data, "native declaration/anchor/link/markup changed: " + name)
        restored[name] = text.encode()
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
            "output_sha256": hashes(expected), "counts": counts,
        }
        require(actual["api/presentation.json"] == sorted_json(manifest).encode(), "altered/forged presentation manifest")
        for name, data in expected.items():
            require(actual[name] == data, "not the pinned reconstructed transformation: " + name)
        # The enhancer's independent verifier operates on API-only output too.
        for name, data in actual.items():
            if name.startswith("api/"):
                (isolated / name).write_bytes(data)
        verify(config, isolated)
    links = sum(len(Page(data.decode()).links) for n, data in actual.items() if n.endswith(".html"))
    require(counts["linked_local_types"] == len(audit) == counts["linked_types"], "type-link counts disagree with independent audit")
    require(counts["linked_external_types"] == 0, "external type links not authorized")
    require(links == 1562 + 30 + len(audit) + 1,
            "presentation link delta differs from theme plus audited type links and index script")
    require(fingerprint(hashes({n: d for n, d in actual.items() if n.startswith("api/")}))
            == ENHANCED_API_SHA256, "complete inherited enhanced API inventory/bytes changed")
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
            "source_declarations": 1238, "native_sections": 1237,
            "linked_local_types": len(audit), "linked_external_types": 0,
            "enhancer_revision": REVISION, "archive_sha256": ARCHIVE_SHA256,
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
