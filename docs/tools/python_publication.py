"""Source-derived Python reference, isolated assembly and deployment verification.

Python documentation is generated without importing the binding or loading a
native library. Its record is evidence, not publication authority: the existing
same-run manifest supplies that authority, and verification also reconstructs
every Python byte from the exported source. Missing prose is informational.
"""

import json
import posixpath
import re
from html.parser import HTMLParser
from pathlib import PurePosixPath
from urllib.parse import urlsplit

import build_enhanced_site as publication
import python_api_reference as reference
import source_epoch as epoch


GENERATOR = 'ams-mel-python-source-reference-v1'
INSERTION = (b'<section aria-label="Python reference"><h2>Python reference</h2>\n'
             b'<p><a href="python/index.html">Safe Python application API and guides</a></p>\n'
             b'<p><a href="python/api-index.html">Search public Python types, functions and members</a></p>\n'
             b'</section>\n')
REQUIRED = {'index.html', 'api-index.html', 'getting-started.html',
            'ownership-and-errors.html', 'ir-workflows.html', 'rf-admin.html',
            'assets/reference.css', 'assets/search.js', 'assets/search-index.js'}
SCRIPTS = {'assets/search.js', 'assets/search-index.js'}
TAGS = {'html', 'head', 'meta', 'title', 'body', 'header', 'nav', 'main', 'footer',
        'section', 'article', 'aside', 'div', 'span', 'h1', 'h2', 'h3', 'h4', 'h5',
        'h6', 'p', 'ul', 'ol', 'li', 'dl', 'dt', 'dd', 'pre', 'code', 'table',
        'caption', 'thead', 'tbody', 'tfoot', 'tr', 'td', 'th', 'a', 'strong',
        'em', 'b', 'i', 'small', 'br', 'hr', 'details', 'summary', 'label',
        'input', 'button', 'noscript', 'script', 'link'}
GLOBAL_ATTRIBUTES = {'id', 'class', 'title', 'lang', 'dir', 'role', 'hidden', 'tabindex'}
ATTRIBUTES = {'meta': {'charset', 'name', 'content'}, 'a': {'href'},
              'link': {'rel', 'href'}, 'script': {'src', 'defer'},
              'input': {'type', 'autocomplete', 'placeholder'}, 'button': {'type'},
              'label': {'for'}, 'details': {'open'}, 'ol': {'start', 'reversed'},
              'th': {'colspan', 'rowspan', 'scope', 'headers'},
              'td': {'colspan', 'rowspan', 'headers'}}
require = epoch.require


def path(name):
    require(isinstance(name, str) and re.fullmatch(r'[A-Za-z0-9_.\-/]+', name)
            and not name.startswith('/')
            and all(part not in {'', '.', '..'} for part in name.split('/')),
            'unsafe Python publication path')
    require(PurePosixPath(name).suffix in {'.html', '.css', '.js', '.json'},
            'unsupported Python publication file')
    return name


def destination(name, value):
    parts = urlsplit(value)
    require(not parts.scheme and not parts.netloc and not parts.query
            and not parts.path.startswith('/')
            and not re.search(r'[\x00-\x20\x7f\\%]', value),
            'unsafe Python documentation URL')
    target = (posixpath.normpath(posixpath.join(posixpath.dirname(name), parts.path))
              if parts.path else name)
    return target, parts.fragment


class Page(HTMLParser):
    """A Python-only HTML policy; Ada and Rust retain their existing policies."""

    def __init__(self, name, data):
        super().__init__(convert_charrefs=True)
        self.name, self.ids, self.links = name, set(), []
        self.in_script = False
        self.feed(data.decode('utf-8'))
        self.close()
        require(not self.in_script, 'unclosed Python script element')

    def handle_starttag(self, tag, attrs):
        require(tag in TAGS, 'unsafe Python documentation element: ' + tag)
        require(len(dict(attrs)) == len(attrs), 'duplicate Python documentation attribute')
        attributes = dict(attrs)
        allowed = ATTRIBUTES.get(tag, set())
        require(all(key in allowed or tag != 'script' and
                    (key in GLOBAL_ATTRIBUTES or re.fullmatch(r'aria-[a-z]+(?:-[a-z]+)*', key))
                    for key in attributes), 'unapproved Python documentation attribute')
        if 'id' in attributes:
            value = attributes['id']
            require(value and value not in self.ids and not re.search(r'\s', value),
                    'duplicate/invalid Python documentation fragment')
            self.ids.add(value)
        if tag == 'script':
            require(not self.in_script and attributes.get('src'), 'inline Python documentation script')
            target, fragment = destination(self.name, attributes['src'])
            require(target in SCRIPTS and not fragment, 'unapproved Python documentation script')
            self.in_script = True
        if tag == 'link':
            require(attributes.get('rel') == 'stylesheet' and attributes.get('href'),
                    'unapproved Python documentation resource')
            target, fragment = destination(self.name, attributes['href'])
            require(target == 'assets/reference.css' and not fragment,
                    'unapproved Python documentation stylesheet')
        if tag == 'input':
            require(attributes.get('type') == 'search', 'unapproved Python documentation input')
        if tag == 'button':
            require(attributes.get('type') == 'button', 'unapproved Python documentation button')
        for key in ('href', 'src'):
            if key in attributes:
                require((tag == 'a' and key == 'href') or
                        (tag == 'link' and key == 'href') or
                        (tag == 'script' and key == 'src'),
                        'unexpected Python documentation URL attribute')
                require(attributes[key], 'empty Python documentation URL')
                self.links.append((tag, key, attributes[key]))

    def handle_startendtag(self, tag, attrs):
        require(tag != 'script', 'self-closing Python script element')
        self.handle_starttag(tag, attrs)

    def handle_endtag(self, tag):
        if tag == 'script':
            require(self.in_script, 'unbalanced Python script element')
            self.in_script = False

    def handle_data(self, data):
        require(not self.in_script or not data.strip(), 'inline Python documentation script content')


def validate(files):
    """Resolve all local resources/fragments, with one exact portal escape."""
    require(REQUIRED <= files.keys(), 'missing Python documentation entry point')
    require(all(isinstance(data, bytes) for data in files.values()), 'non-byte Python publication')
    for name in files:
        path(name)
    pages = {name: Page(name, data) for name, data in files.items() if name.endswith('.html')}
    links = 0
    for name, page in pages.items():
        for tag, key, value in page.links:
            links += 1
            target, fragment = destination(name, value)
            portal = posixpath.relpath('../index.html', posixpath.dirname(name) or '.')
            if tag == 'a' and key == 'href' and value == portal:
                continue
            require(target != '..' and not target.startswith('../'), 'Python documentation path escape')
            require(target in files, 'missing Python documentation destination: ' + name + ': ' + value)
            require(not fragment or target in pages and fragment in pages[target].ids,
                    'missing Python documentation fragment: ' + name + ': ' + value)
    for name, data in files.items():
        if name.endswith('.css'):
            require(not re.search(r'@import\b|url\s*\(|expression\s*\(|-moz-binding|behavior\s*:',
                                  data.decode('utf-8'), re.I),
                    'external/active Python documentation CSS')
    return {'html': len(pages), 'files': len(files), 'assets': len(files) - len(pages), 'links': links}


def assembled(base, python):
    require(not any(name == 'python' or name.startswith('python/') for name in base),
            'Python publication collision')
    require('index.html' in base and base['index.html'].count(b'</main>') == 1
            and INSERTION not in base['index.html'], 'ambiguous Python portal insertion')
    result = dict(base)
    result['index.html'] = base['index.html'].replace(b'</main>', INSERTION + b'</main>', 1)
    result.update({'python/' + name: data for name, data in python.items()})
    return result


def generate(exported, context, log):
    files, report = reference.generate(exported)
    counts = validate(files)
    record = {'schema': 1, 'identity': context, 'generator': GENERATOR,
              'report': report, 'counts': counts, 'files': publication.hashes(files)}
    log.write_text('Python reference generated from exported source; no binding import or native loading.\n'
                   + json.dumps(counts, sort_keys=True) + '\n'
                   + 'Documentation prose counts are informational.\n', encoding='utf-8')
    return files, record


def verify_record(exported, files, record, context):
    require(record['schema'] == 1 and record['identity'] == context, 'Python documentation source/run mismatch')
    require(record['generator'] == GENERATOR, 'Python documentation generator mismatch')
    require(publication.hashes(files) == record['files'], 'Python documentation artifact inventory mismatch')
    require(validate(files) == record['counts'], 'Python documentation measured inventory mismatch')
    expected, report = reference.generate(exported)
    require(files == expected and record['report'] == report,
            'Python documentation differs from source reconstruction')


def verify_assembly(directory, exported, context, record):
    """Return the exact Ada+Rust projection for its existing independent verifier."""
    actual = publication.files(directory / 'published')
    python = {name.removeprefix('python/'): data for name, data in actual.items()
              if name.startswith('python/')}
    verify_record(exported, python, record, context)
    base = {name: data for name, data in actual.items() if not name.startswith('python/')}
    require('index.html' in base and base['index.html'].count(INSERTION) == 1,
            'Python portal insertion is not exactly reversible')
    base['index.html'] = base['index.html'].replace(INSERTION, b'', 1)
    require(actual == assembled(base, python), 'unauthorized Python publication transformation')
    return base
