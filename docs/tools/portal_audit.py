"""Validation-only inverse of authored tokens; never writes or renders a site."""

import collections
import html
import re

import api_catalog
import example_sources
from build_docs_portal import regular, PortalError

TOKEN = re.compile(r'\{\{([^{}]+)\}\}')


def verify_portal(site, root, data, packages, references):
    from validate_docs import Page
    api_catalog.verify((site / 'guides/api-map.html').read_text(), root, references)
    example_sources.verify_rendered((site / 'guides/examples.html').read_text(), root, references)
    for name in [data['home'], *(g['path'] for g in data['guides'])]:
        source = regular(root / 'docs' / name)
        text = regular(site / name)
        tokens = list(TOKEN.finditer(source))
        pattern, end = '', 0
        for token in tokens:
            pattern += re.escape(source[end:token.start()]) + '(.*?)'
            end = token.end()
        match = re.fullmatch(pattern + re.escape(source[end:]), text, re.S)
        if match is None or '{{' in text or '}}' in text:
            raise PortalError('authored static bytes changed: ' + name)
        prefix = '../' if name.startswith('guides/') else ''
        for token, block in zip(tokens, match.groups()):
            kind = token[1]
            page = Page(block)
            if page.issues:
                raise PortalError('unsafe generated token block')
            if kind.startswith('package:'):
                owner = kind.split(':', 1)[1]
                if owner not in packages or block != ('<a href="' + prefix + packages[owner] + '">'
                                                       + html.escape(owner) + '</a>'):
                    raise PortalError('wrong package token destination')
            elif kind == 'navigation':
                links = [prefix + 'index.html']
                links += [g['path'].removeprefix('guides/') if prefix else g['path'] for g in data['guides']]
                links += [prefix + 'api/index.html']
                if page.links != links or not block.startswith('<nav aria-label="Documentation"><ul>') or not block.endswith('</ul></nav>'):
                    raise PortalError('wrong portal navigation')
                labels = ['Home', *(g['label'] for g in data['guides']), 'Complete API Reference']
                if re.findall(r'<a href="[^"]+">([^<]+)</a>', block) != labels:
                    raise PortalError('wrong navigation labels')
            elif kind == 'package-directory':
                expected = collections.Counter((p, prefix + dest) for p, dest in packages.items())
                if collections.Counter(page.package_entries) != expected or len(page.links) != len(packages):
                    raise PortalError('wrong package directory')
                from build_docs_portal import GROUPS, group
                sections = re.findall(r'<h3>([^<]+)</h3><ul>(.*?)</ul>', block, re.S)
                if [label for label, _ in sections] != list(GROUPS):
                    raise PortalError('wrong package grouping')
                for label, content in sections:
                    owners = re.findall(r'<a data-package-entry="([^"]+)"', content)
                    if owners != [p for p in packages if group(p) == label]:
                        raise PortalError('wrong package order or group ownership')
            elif kind.startswith('example:'):
                owner = kind.split(':', 1)[1]
                entries = {e['id']: e for e in example_sources.inventory(root)}
                if name != 'guides/examples.html' or owner not in entries:
                    raise PortalError('wrong executable token ownership')
                entry = entries[owner]
                listings = example_sources.Listings(block).values
                if [p for p, _ in listings] != [entry['main'], *entry['support']]:
                    raise PortalError('wrong supporting source ownership')
                # Retained independent block serializer is an audit oracle only;
                # it cannot render navigation, authored pages or a publication.
                if block != example_sources.render(token[0], root, references):
                    raise PortalError('changed executable case or listing serialization')
            elif kind in ('public-symbol-index', 'example-usage-index'):
                scopes = ('stable',) if kind == 'public-symbol-index' else ('additional', 'operation')
                expected = api_catalog.render(api_catalog.inventory(root, references, independent=True), scopes)
                if name != 'guides/api-map.html' or block != expected:
                    raise PortalError('wrong source catalog token')
            else:
                raise PortalError('unknown authored token: ' + kind)
    if regular(site / data['stylesheet']) != regular(root / 'docs' / data['stylesheet']):
        raise PortalError('portal stylesheet differs from authorized source')