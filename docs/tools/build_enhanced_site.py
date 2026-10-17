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
from check_doc_coverage import measure, ratchet
from gnatdoc_inventory import specifications
from validate_docs import Page, validate as validate_pristine

REVISION = "c1a691452cf436624809f4043b30087de6688052"
ARCHIVE_SHA256 = "d4b422b73df60d6ed41ec534b71bd0248b43827ce54ed4c323455e225be17493"
PACKAGE_SHA256 = "384974fefbc2772ef6c79d43b275b549e5c12758535450a60c852c401e56ba1a"
SOURCE_SHA256 = "467cd45ef85888bb22ccee2d9e325b6b55e985dadac66c317dbbc653193114c8"
ANCHORS_SHA256 = "06b602439be15557118790a2827bb8bacc45c1b02d055fa3e9502204857f5aed"
PRISTINE_SHA256 = "0e6497d6172c918d04ea1c284785f7cfc0fec278e155ab254de0c78e81ac94e2"
EXTRAS = {"api/gnatdoc-theme.css", "api/presentation.json"}
METADATA = ('<meta name="viewport" content="width=device-width, initial-scale=1">'
            '<link rel="stylesheet" href="gnatdoc-theme.css">')
SPAN = re.compile(r'<span class="ada-(?:keyword|number|string|character|attribute|comment|operator)">([^<]*)</span>')
CONFIG = {
    "schema_version": 1, "project": "AMS MEL Bridge",
    "renderer_exceptions": ["AMS.MEL.IR.Health_Status.Metadata.Status"],
    "gnatdoc": {"version": "26.0.0"},
    "sources": {"directories": ["ada/src"], "comment_style": "trailing"},
    "navigation": {"mode": "none"},
    "presentation": {"theme": True, "syntax_highlighting": True, "type_links": False},
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


def tool():
    import ada_gnatdoc_enhancer
    package = Path(ada_gnatdoc_enhancer.__file__).parent
    content = {name: data for name, data in files(package).items()
               if "__pycache__" not in Path(name).parts}
    require(importlib.metadata.version("ada-gnatdoc-enhancer") == "0.1.0.dev0", "enhancer version mismatch")
    require(fingerprint(hashes(content)) == PACKAGE_SHA256, "enhancer implementation/assets mismatch")


def qualified_original(pristine, root=ROOT):
    raw = files(pristine)
    require(len(raw) == 39 and fingerprint(hashes(raw)) == PRISTINE_SHA256,
            "pristine native/portal bytes differ from reviewed baseline")
    sources = {"ada/src/" + n: data for n, data in files(root / "ada/src").items()}
    require(fingerprint(hashes(sources)) == SOURCE_SHA256, "approved source-profile identity changed")
    anchors = (root / "docs/gnatdoc-native-anchors.json").read_bytes()
    require(sha(anchors) == ANCHORS_SHA256, "authoritative anchors/profiles changed")
    require(tomllib.loads((root / "docs/gnatdoc-enhance.toml").read_text()) == CONFIG,
            "unapproved presentation configuration")
    require(validate_pristine(pristine) == {"html_pages": 37, "links": 1516}, "pristine metrics changed")
    report = measure(pristine, root, json.loads(anchors))
    ratchet(report, (root / "docs/gnatdoc-coverage-baseline.txt").read_text())
    require((report["source_declarations"], report["native_sections"],
             report["documented_elements"], report["total_elements"],
             len(report["renderer_exceptions"])) == (1238, 1237, 4294, 4295, 1), "coverage changed")
    return raw


def validate(published, pristine, root=ROOT):
    """Neither an output-side manifest nor enhancer self-verification is authority."""
    tool()
    raw = qualified_original(pristine, root)
    actual = files(published)
    require(set(actual) == set(raw) | EXTRAS, "missing/unexpected publication files")
    for name, data in raw.items():
        if not (name.startswith("api/") and name.endswith(".html")):
            require(actual[name] == data, "changed native CSS or authored portal: " + name)
    # Independently remove ONLY the approved metadata and flat lexical spans.
    # Any other markup, nesting, destination, text or anchor change fails bytes.
    restored = {}
    for name, data in raw.items():
        if not (name.startswith("api/") and name.endswith(".html")):
            restored[name] = actual[name]
            continue
        text = actual[name].decode()
        require(text.count(METADATA) == 1, "missing/duplicate viewport/theme metadata")
        require(text.count("gnatdoc-theme.css") == 1, "duplicate theme insertion")
        text = text.replace(METADATA, "", 1)
        text = SPAN.sub(lambda m: m[1], text)
        require(text.encode() == data, "native declaration/anchor/link/markup changed: " + name)
        restored[name] = text.encode()
        require(not Page(actual[name].decode()).issues, "unsafe HTML")
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
    require(links == 1546, "presentation link delta differs from 30 theme links")
    return {"files": 41, "html_pages": 37, "links": links, "public_packages": 29,
            "source_declarations": 1238, "native_sections": 1237,
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
        # Copy over original API files plus exactly the two approved additions.
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
    args = parser.parse_args()
    result = build(args.pristine, args.site) if args.command == "build" else validate(args.site, args.pristine)
    print(json.dumps(result, indent=2, sort_keys=True))
