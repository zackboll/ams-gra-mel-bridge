"""Reconstruct the official v0.1.6 compatibility control, never a deployment.

Invoked exclusively with the separately installed hash-required previous wheel.
The source/native/guide inputs are the same disposable inputs as the v0.1.7
disabled control. A version label alone is not an installation identity.
"""

import argparse
from dataclasses import replace
import importlib.metadata
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / 'docs/tools'), str(ROOT / 'scripts')]
import build_enhanced_site as p
import authored_html
import source_epoch as epoch


def build(root, site, report, output):
    import ada_gnatdoc_enhancer
    from ada_gnatdoc_enhancer.config import load
    from ada_gnatdoc_enhancer.pipeline import enhance, verify
    distribution = importlib.metadata.distribution('ada-gnatdoc-enhancer')
    package = Path(ada_gnatdoc_enhancer.__file__).parent
    p.require(distribution.version == '0.1.6' and
              subprocess.check_output([str(Path(sys.executable).parent / 'gnatdoc-enhancer'), '--version'],
                                      text=True).strip() == '0.1.6', 'wrong previous CLI/distribution')
    content = {n: d for n, d in p.files(package).items() if '__pycache__' not in Path(n).parts}
    p.require(p.fingerprint(p.hashes(content)) ==
              'ec58fd6e393bf2f88fdeddccb590d7c9e95319e3440d132ffd19b5b64e0a8377',
              'previous installed package differs')
    p.require(package.resolve() == Path(distribution.locate_file('ada_gnatdoc_enhancer')).resolve(),
              'previous import origin differs')
    origin = json.loads(distribution.read_text('direct_url.json'))
    p.require(origin['url'] == 'https://github.com/zackboll/ada-gnatdoc-enhancer/releases/download/v0.1.6/'
              'ada_gnatdoc_enhancer-0.1.6-py3-none-any.whl' and
              origin['archive_info']['hashes']['sha256'] ==
              'adfec71188a868e3fb70a6da71a124c937f9e63e26a2f2e31af45364f49ffb27',
              'previous wheel origin differs')
    config = replace(load(root, 'docs/gnatdoc-enhance.toml'),
                     guide_sha256=p.sha(epoch.canonical(authored_html.contract(root, site, report))))
    p.require(not output.exists(), 'previous output must be fresh')
    output.mkdir()
    shutil.copytree(site / 'api', output / 'api')
    enhance(config, output)
    verify(config, output)
    before = p.files(output)
    enhance(config, output)
    verify(config, output)
    p.require(p.files(output) == before, 'previous renderer not idempotent')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--site', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    build(args.root, args.site, json.loads(args.report.read_bytes()), args.output)