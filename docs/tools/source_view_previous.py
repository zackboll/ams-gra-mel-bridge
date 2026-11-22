"""Reconstruct the official v0.1.7 compatibility control, never a deployment.

Invoked exclusively with the separately installed hash-required previous wheel.
The source/native/guide inputs are the same disposable inputs as v0.1.8, with
or without the reviewed opt-in. A version label alone is not an installation identity.
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
import source_view_audit as source

VERSION = '0.1.7'
WHEEL_URL = ('https://github.com/zackboll/ada-gnatdoc-enhancer/releases/download/v0.1.7/'
             'ada_gnatdoc_enhancer-0.1.7-py3-none-any.whl')
WHEEL_SHA256 = '957c65797b03b5c6b3779e2a1918730a2e44749670e37535d2c8ff3e109f3ba4'
PACKAGE_SHA256 = 'f6c8557c71092a2ef741f1d534a5948cdcef3805244306fb0b8424f51011ff04'
CSS_SHA256 = '5b1716ddec6cb9afc3576a1628ba5e14002700575542c9afbe831d99a25a8260'


def build(root, site, report, output):
    import ada_gnatdoc_enhancer
    distribution = importlib.metadata.distribution('ada-gnatdoc-enhancer')
    package = Path(ada_gnatdoc_enhancer.__file__).parent
    p.require(distribution.version == VERSION and
              subprocess.check_output([str(Path(sys.executable).parent / 'gnatdoc-enhancer'), '--version'],
                                      text=True).strip() == VERSION, 'wrong previous CLI/distribution')
    content = {n: d for n, d in p.files(package).items() if '__pycache__' not in Path(n).parts}
    p.require(p.fingerprint(p.hashes(content)) == PACKAGE_SHA256, 'previous installed package differs')
    p.require(package.resolve() == Path(distribution.locate_file('ada_gnatdoc_enhancer')).resolve(),
              'previous import origin differs')
    origin = json.loads(distribution.read_text('direct_url.json') or '{}')
    p.require(set(origin) == {'url', 'archive_info'} and origin['url'] == WHEEL_URL and
              origin['archive_info'].get('hashes', {}).get('sha256') == WHEEL_SHA256,
              'previous wheel origin differs')
    from ada_gnatdoc_enhancer.config import load
    from ada_gnatdoc_enhancer.pipeline import enhance, verify
    config = replace(load(root, source.prepare(root, site)),
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
