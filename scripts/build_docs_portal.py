#!/usr/bin/env python3
"""Consumer-owned portal identities and native destination audit (no renderer)."""

import collections
import json
import re
import sys
from pathlib import Path

from gnatdoc_inventory import ROOT, specifications

sys.path.insert(0, str(ROOT / "docs/tools"))

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


def package_map(site, root=ROOT, ledger=None):
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
    anchors = (json.loads(regular(root / "docs/gnatdoc-native-anchors.json"))
               if ledger is None else ledger)
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


def build(site, root=ROOT):
    """Compatibility entry: official renderer only, preserving native input.

    Production uses source_epoch instead. Kept for isolated portal mutation
    qualification, not as an alternative rendering implementation.
    """
    import shutil
    import tempfile
    import build_enhanced_site as presentation
    import authored_html
    from check_doc_coverage import measure
    from ada_gnatdoc_enhancer.pipeline import enhance, verify
    presentation.tool()
    report = measure(site, root)
    ledger = root / 'docs/gnatdoc-native-anchors.json'
    original = ledger.read_bytes()
    generated = root / 'docs/gnatdoc-authored.json'
    try:
        ledger.write_text(json.dumps(report['native']))
        config = authored_html.bind(root, site, report)
        with tempfile.TemporaryDirectory(prefix='mel-portal-compat-') as temp:
            stage = Path(temp) / 'site'
            shutil.copytree(site / 'api', stage / 'api')
            enhance(config, stage)
            verify(config, stage)
            data = manifest(root)
            for name in [data['home'], *(g['path'] for g in data['guides']), data['stylesheet']]:
                target = site / name
                if any(p.is_symlink() for p in (target, *target.parents)):
                    raise PortalError('symlink in portal output')
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes((stage / name).read_bytes())
    finally:
        ledger.write_bytes(original)
        if generated.is_file() and not generated.is_symlink():
            generated.unlink()
    return package_map(site, root, report['native'])
