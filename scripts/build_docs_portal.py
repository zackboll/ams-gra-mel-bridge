#!/usr/bin/env python3
"""Build a fixed, offline portal around (never inside) native GNATdoc output."""

import argparse
import collections
import html
import json
import re
import sys
from pathlib import Path

from gnatdoc_inventory import ROOT, specifications

sys.path.insert(0, str(ROOT / "docs/tools"))
from example_sources import References, render as render_examples, verify_rendered
import api_catalog

GUIDES = (
    ("getting-started", "Getting Started"),
    ("examples", "Examples"),
    ("architecture", "Architecture"),
    ("ir-workflows", "IR Workflows"),
    ("rf-workflows", "RF Workflows"),
    ("ownership-and-errors", "Ownership and Errors"),
    ("api-map", "API Map"),
)
GROUPS = ("Shared/Core", "IR", "RF")


class PortalError(ValueError):
    pass


def regular(path):
    if any(p.is_symlink() for p in (path, *path.parents)) or not path.is_file():
        raise PortalError(f"missing file or symlink: {path.name}")
    return path.read_text()


def manifest(root=ROOT):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise PortalError("duplicate manifest key")
            result[key] = value
        return result
    data = json.loads(regular(root / "docs/site-map.json"), object_pairs_hook=unique)
    expected = {"schema_version": 1, "home": "index.html",
                "stylesheet": "assets/portal.css", "guides": [
                    {"id": identity, "label": label, "path": f"guides/{identity}.html"}
                    for identity, label in GUIDES]}
    if data != expected:
        raise PortalError("unknown, missing, duplicate or invalid manifest identity/path/label")
    return data


def package_map(site, root=ROOT):
    expected = set(specifications(root)[0])
    packages = {}
    if site.is_symlink() or (site / "api").is_symlink():
        raise PortalError("symlink in native inventory")
    for path in sorted((site / "api").iterdir()):
        text = regular(path)
        if path.name in {"index.html", "gnatdoc.css"}:
            continue
        if not re.fullmatch(r"[a-f0-9]{64}\.html", path.name):
            raise PortalError("unapproved native file")
        names = re.findall(r"<h1>([^<]+)</h1>", text)
        if len(names) != 1 or names[0] not in expected or names[0] in packages:
            raise PortalError("unknown, duplicate or invalid native package")
        packages[names[0]] = "api/" + path.name
    if set(packages) != expected or len(set(packages.values())) != len(expected):
        raise PortalError("missing or duplicate public package destination")
    # Independently reconcile observed package destinations with the qualified
    # source/profile/anchor manifest; headings alone cannot authorize a swap.
    anchors = json.loads(regular(root / "docs/gnatdoc-native-anchors.json"))
    qualified = collections.defaultdict(set)
    for identity, entry in anchors.items():
        qualified[identity.split(":", 1)[0]].add("api/" + entry["page"])
    # AMS is an empty root package: it has no declaration anchor entry.
    if set(qualified) != expected - {"AMS"} or any(
            qualified[name] != {packages[name]} for name in qualified):
        raise PortalError("swapped or fabricated package destination")
    return dict(sorted(packages.items()))


def group(name):
    if name == "AMS.MEL.IR" or name.startswith("AMS.MEL.IR."):
        return "IR"
    if name == "AMS.MEL.RF" or name.startswith("AMS.MEL.RF."):
        return "RF"
    return "Shared/Core"


def render(text, page, data, packages, root=ROOT, references=None):
    prefix = "../" if page.startswith("guides/") else ""

    def package_link(name, directory=False):
        if name not in packages:
            raise PortalError(f"unknown package reference: {name}")
        marker = ' data-package-entry="' + name + '"' if directory else ""
        return f'<a{marker} href="{prefix}{packages[name]}">{html.escape(name)}</a>'

    nav = '<nav aria-label="Documentation"><ul>'
    nav += f'<li><a href="{prefix}index.html">Home</a></li>'
    nav += "".join(f'<li><a href="{entry["path"].removeprefix("guides/") if prefix else entry["path"]}">{entry["label"]}</a></li>'
                   for entry in data["guides"])
    nav += f'<li><a href="{prefix}api/index.html">Complete API Reference</a></li></ul></nav>'
    directory = '<section aria-label="Public package directory">'
    for category in GROUPS:
        directory += f'<h3>{category}</h3><ul>'
        directory += "".join(f'<li>{package_link(name, True)}</li>'
                             for name in packages if group(name) == category)
        directory += '</ul>'
    directory += '</section>'
    text = text.replace("{{navigation}}", nav).replace("{{package-directory}}", directory)
    text = re.sub(r"\{\{package:([A-Za-z0-9_.]+)\}\}",
                  lambda m: package_link(m[1]), text)
    if page == "guides/api-map.html":
        if references is None:
            raise PortalError("native authority required for catalog rendering")
        catalog = api_catalog.inventory(root, references)
        for token, scopes in (("{{public-symbol-index}}", ("stable",)),
                              ("{{example-usage-index}}", ("additional", "operation"))):
            if text.count(token) != 1:
                raise PortalError("missing/duplicate catalog placeholder")
            text = text.replace(token, api_catalog.render(catalog, scopes), 1)
    if page == "guides/examples.html":
        try:
            if references is None:
                raise PortalError("native authority required for example rendering")
            text = render_examples(text, root, references)
            verify_rendered(text, root, references)
        except (ValueError, OSError) as exc:
            raise PortalError(str(exc)) from exc
    if "{{" in text or "}}" in text:
        raise PortalError("unresolved portal placeholder")
    return text


def build(site, root=ROOT):
    data = manifest(root)
    packages = package_map(site, root)
    references = References(root, site)
    paths = [data["home"], *(entry["path"] for entry in data["guides"]), data["stylesheet"]]
    # Validate all inputs and output ancestors before writing any portal file.
    sources = {name: regular(root / "docs" / name) for name in paths}
    for name in paths:
        path = site / name
        if any(p.is_symlink() for p in (path, *path.parents)):
            raise PortalError("symlink in portal output")
    rendered = {name: render(text, name, data, packages, root, references) if name.endswith(".html") else text
                for name, text in sources.items()}
    for name, text in rendered.items():
        target = site / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text)
    return packages


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("site", type=Path)
    args = parser.parse_args()
    print(json.dumps(build(args.site), indent=2, sort_keys=True))
