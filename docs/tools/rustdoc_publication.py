"""Isolated Rustdoc generation and assembly; never an Ada inventory exception.

The generation record is evidence, not authority. docs_ci's externally supplied
same-run manifest and bundle digests authorize its bytes and every generated file.
Only generation from the Git export may create that seal.
"""

import os
from pathlib import Path
import posixpath
import re
import subprocess
import time
from html.parser import HTMLParser
from urllib.parse import unquote, urlsplit

import source_epoch as epoch
import build_enhanced_site as publication
from rustdoc_001_foundation import CRATES, diagnostics

TOOLCHAIN = '1.98.1'
VERSIONS = {
    'cargo': 'cargo 1.98.1 (797e8a9bc 2026-08-05)',
    'rustc': 'rustc 1.98.1 (48a229cea 2026-09-01)',
    'rustdoc': 'rustdoc 1.98.1 (48a229cea 2026-09-01)',
}
# Qualified official 1.98.1 x86_64-unknown-linux-gnu executable bytes.
BINARIES = {
    'cargo': 'da77c8b33849312255ccde3179198ada4c8deb370488d050286146b1d1b27e14',
    'rustc': '859254978c0a0402c32f949f6de0d99aee73be8d15f45aac00ae1448aac51e74',
    'rustdoc': '3ff7fed6d1064d8d75d24ae8beff9fe1d68031d3d819966481d22c3c9002b709',
}
FLAGS = '--deny rustdoc::broken_intra_doc_links --warn missing_docs'
INSERTION = (b'<section aria-label="Rust reference"><h2>Rust reference</h2>\n'
             b'<p><a href="rust/ams_mel/index.html">Safe Rust application API (ams-mel)</a></p>\n'
             b'<p><a href="rust/ams_mel_sys/index.html">Low-level unsafe Rust FFI implementation '
             b'interface (ams-mel-sys)</a>; not the recommended application API.</p>\n'
             b'<p><a href="rust/ams_mel_common/index.html">Shared Common MEL Rust values</a></p>\n'
             b'<p><a href="rust/ams_mel_rf/index.html">Safe RF Admin and DataMEL API</a></p>\n'
             b'<p><a href="rust/ams_mel_ir/index.html">Safe IR MEL API (ams-mel-ir)</a></p>\n'
             b'</section>\n')
LANDING = (b'<!doctype html><html lang="en"><meta charset="utf-8">'
           b'<title>AMS MEL Rust reference</title><h1>AMS MEL Rust reference</h1>'
           b'<p><a href="ams_mel_common/index.html">Shared Common MEL values</a></p>'
           b'<p><a href="ams_mel_rf/index.html">Safe RF Admin and DataMEL API</a></p>'
           b'<p><a href="ams_mel_ir/index.html">Safe IR MEL API</a></p>'
           b'<p><a href="ams_mel/index.html">Safe Rust application API</a></p>'
           b'<p><a href="ams_mel_sys/index.html">Low-level unsafe C ABI bindings: '
           b'FFI implementation interface, not the recommended application API</a></p>'
           b'<p><a href="../index.html">Documentation portal</a></p></html>\n')
require = epoch.require


def insert(home):
    marker = b'</main>'
    require(home.count(marker) == 1 and INSERTION not in home, 'ambiguous portal insertion')
    return home.replace(marker, INSERTION + marker, 1)


def assembled(ada, rust):
    require(not any(n == 'rust' or n.startswith('rust/') for n in ada), 'publication collision')
    result = dict(ada)
    result['index.html'] = insert(ada['index.html'])
    result.update({'rust/' + n: data for n, data in rust.items()})
    return result


class Page(HTMLParser):
    """Rustdoc-only policy. Active content is authorized by exact artifact bytes.

    Generated inline JS/styles, SVG, forms, and javascript:void(0) are deliberate
    Rustdoc UI, not permissions for authored Ada content. Remote resources are
    forbidden; only bounded documentation hyperlinks may leave this subtree.
    """
    def __init__(self, data):
        super().__init__(convert_charrefs=True)
        self.ids, self.links = set(), []
        self.feed(data.decode('utf-8'))
        self.close()

    def handle_starttag(self, tag, attrs):
        require(len(dict(attrs)) == len(attrs), 'duplicate Rustdoc attribute')
        require(tag not in {'base', 'iframe', 'object', 'embed', 'applet'}, 'unsafe Rustdoc element')
        for key, value in attrs:
            if key == 'id':
                require(value and value not in self.ids, 'duplicate Rustdoc fragment')
                self.ids.add(value)
            if key in {'href', 'src'} and value:
                self.links.append((tag, key, value))

    handle_startendtag = handle_starttag


def destination(name, value):
    parts = urlsplit(value)
    require(not parts.scheme and not parts.netloc and not parts.path.startswith('/') and
            not re.search(r'[\x00-\x20\x7f\\]', value), 'unsafe Rustdoc local URL')
    path = unquote(parts.path)
    require('%' not in path and '\\' not in path, 'encoded Rustdoc path')
    target = posixpath.normpath(posixpath.join(posixpath.dirname(name), path)) if path else name
    require(target != '..' and not target.startswith('../'), 'Rustdoc path escape')
    return target, parts.fragment


def graph(files, portal=False):
    pages = {n: Page(d) for n, d in files.items() if n.endswith('.html')}
    for name, page in pages.items():
        for tag, key, value in page.links:
            parts = urlsplit(value)
            if parts.scheme or parts.netloc:
                allowed = (tag == 'a' and key == 'href' and (
                    value == 'javascript:void(0)' or
                    (parts.scheme == 'https' and not parts.username and not parts.password and
                     (parts.netloc == 'doc.rust-lang.org' and parts.path.startswith('/1.98.1/') or
                      parts.netloc == 'github.com' and
                      re.fullmatch(r'/rust-lang/rust/issues/[0-9]+', parts.path)))))
                require(allowed, 'unapproved Rustdoc external resource/link')
                continue  # Never fetch external documentation or follow its redirects.
            if portal and name == 'index.html' and value == '../index.html':
                continue  # Exact consumer-owned landing; assembly compares its bytes.
            target, fragment = destination(name, value)
            require(target in files, 'missing Rustdoc destination: ' + name + ': ' + value)
            # Rustdoc source ranges are virtual fragments interpreted by source JS.
            source_range = (target.startswith('src/') and re.fullmatch(r'[0-9]+(?:-[0-9]+)?', fragment))
            if source_range:
                first, *last = fragment.split('-')
                require(first in pages[target].ids and (not last or last[0] in pages[target].ids),
                        'missing Rustdoc source range')
            else:
                require(not fragment or target in pages and
                        (fragment in pages[target].ids or unquote(fragment) in pages[target].ids),
                        'missing Rustdoc fragment: ' + name + ': ' + value)
    for name, data in files.items():
        if name.endswith('.css'):
            for value in re.findall(r'url\([\s]*[\"\']?([^\"\')]+)[\"\']?\)', data.decode()):
                if value.startswith('data:image/svg+xml,'):
                    continue  # Exact qualified Rustdoc CSS embeds its UI icons.
                target, _ = destination(name, value)
                require(target in files, 'missing Rustdoc CSS resource')
    return {'html': len(pages), 'files': len(files), 'assets': len(files) - len(pages)}


def validate(files):
    required = ('index.html', 'ams_mel/index.html', 'ams_mel_sys/index.html',
                'ams_mel_common/index.html', 'ams_mel_rf/index.html',
                'ams_mel_rf/struct.RfAdmin.html', 'ams_mel/struct.RfAdmin.html',
                'ams_mel_ir/index.html', 'ams_mel_ir/struct.Session.html',
                'ams_mel/struct.Session.html', 'ams_mel_sys/fn.ams_mel_session_close.html',
                'src-files.js', 'crates.js', 'help.html', 'settings.html')
    require(all(n in files for n in required), 'missing Rustdoc entry point')
    for prefix in ('static.files/', 'src/ams_mel/', 'src/ams_mel_sys/',
                   'src/ams_mel_common/', 'src/ams_mel_rf/', 'src/ams_mel_ir/',
                   'search.index/',
                   'trait.impl/', 'type.impl/'):
        require(any(n.startswith(prefix) for n in files), 'missing Rustdoc asset family')
    require(files['index.html'] == LANDING, 'unauthorized Rust landing')
    return graph(files, portal=True)


def generate(exported, output, context, log):
    """Foundation cargo-doc approach, fresh target and controlled host environment.

    No doctests, linker invocation, native facade build, network or caller flags.
    Cargo runs outside the checkout, with an isolated CARGO_HOME to exclude user
    configuration; this workspace has no registry dependencies to download.
    """
    started = time.monotonic()
    require(not output.exists(), 'Rustdoc target must be fresh')
    output.mkdir()
    env = {k: v for k, v in os.environ.items() if k in {'PATH', 'HOME', 'RUSTUP_HOME', 'TMPDIR'}}
    env.update(CARGO_HOME=str(output / 'cargo-home'), CARGO_TARGET_DIR=str(output / 'target'),
               CARGO_NET_OFFLINE='true', RUSTDOCFLAGS=FLAGS,
               AMS_MEL_NATIVE_LIB_DIR=str(output / 'absent-native-library'))
    tools = {}
    for tool, expected in VERSIONS.items():
        result = subprocess.check_output(['rustup', 'run', TOOLCHAIN, tool, '--version'], env=env, text=True).strip()
        require(result == expected, 'unqualified Rust documentation toolchain: ' + tool)
        binary = Path(subprocess.check_output(['rustup', 'which', '--toolchain', TOOLCHAIN, tool],
                                            env=env, text=True).strip())
        tools[tool] = {'version': result, 'binary_sha256': epoch.sha(binary.read_bytes())}
        require(tools[tool]['binary_sha256'] == BINARIES[tool], 'unqualified Rust executable bytes')
    counts = {}
    for crate in CRATES:
        command = ['rustup', 'run', TOOLCHAIN, 'cargo', 'doc', '--manifest-path',
                   str(exported / 'rust/Cargo.toml'), '-p', crate, '--no-deps', '--locked',
                   '--offline', '--message-format=json']
        result = subprocess.run(command, cwd=exported, env=env, capture_output=True, text=True)
        with log.open('a') as stream:
            stream.write('$ ' + ' '.join(command) + '\n' + result.stdout + result.stderr)
        require(result.returncode == 0, 'Rustdoc generation failed; see ' + str(log))
        counts[crate] = diagnostics(result.stdout)
        require(not any('broken_intra_doc_links' in n or n.startswith('error:') for n in counts[crate]),
                'Rustdoc link/error diagnostic')
    site = output / 'target/doc'
    (site / 'index.html').write_bytes(LANDING)
    files = publication.files(site)
    measured = validate(files)
    record = {'schema': 1, 'identity': context, 'toolchain': TOOLCHAIN, 'tools': tools,
              'flags': FLAGS, 'diagnostics': counts, 'counts': measured,
              'files': publication.hashes(files), 'seconds': time.monotonic() - started}
    return files, record


def verify_record(files, record, context):
    require(record['schema'] == 1 and record['identity'] == context, 'Rustdoc source/run mismatch')
    require(record['toolchain'] == TOOLCHAIN and record['flags'] == FLAGS and
            set(record['tools']) == set(VERSIONS) and all(
                record['tools'][t]['version'] == v and
                record['tools'][t]['binary_sha256'] == BINARIES[t]
                for t, v in VERSIONS.items()), 'Rustdoc toolchain mismatch')
    require(publication.hashes(files) == record['files'], 'Rustdoc artifact inventory mismatch')
    require(validate(files) == record['counts'], 'Rustdoc measured inventory mismatch')


def verify_assembly(directory, context, record):
    ada = publication.files(directory / 'ada-qualified')
    actual = publication.files(directory / 'published')
    rust = {n.removeprefix('rust/'): d for n, d in actual.items() if n.startswith('rust/')}
    verify_record(rust, record, context)
    require(actual == assembled(ada, rust), 'assembled publication differs from authorized transformation')
    # Reversal must recover the complete original qualified Ada publication.
    home = actual['index.html']
    require(home.count(INSERTION) == 1 and home.replace(INSERTION, b'', 1) == ada['index.html'],
            'portal insertion is not exactly reversible')
