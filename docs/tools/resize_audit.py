"""Consumer-owned authorization of the official v0.1.6 resize shell.

No renderer restoration helpers or publication-supplied hashes authorize bytes.
After this inverse, search/navigation restoration must recover the entire native
index. Constants are independently reviewed against the pinned release wheel.
"""

import json

import build_enhanced_site as p

ASSETS = {
    'api/gnatdoc-resize.css': 'c36b8069d1b4f50a53042cfc68107d275501a5e36e6ff98f0d7b7344676beb5b',
    'api/gnatdoc-resize.js': '0c6b03de45812d60d9cbaf9a8c11553cb57bd66b6563dfceea1ffa24b58eb415',
}
EXTRAS = set(ASSETS)
HEAD = ('<link rel="stylesheet" href="gnatdoc-resize.css">'
        '<script src="gnatdoc-resize.js" defer></script>')
SIDEBAR = '<div class=side-navigation id="resizable-navigation">'
DIVIDER = ('<div id="navigation-resize" role="separator" aria-orientation="vertical" '
           'aria-label="Navigation width" aria-controls="resizable-navigation" '
           'tabindex="0" hidden></div>')


def restore(text):
    """Remove only exact, unique, ordered additions at their approved boundaries."""
    for value, before, after, replacement in (
            (HEAD, '<link rel="stylesheet" href="gnatdoc-search.css">',
             '</head><body class=main>', ''),
            (SIDEBAR, '<body class=main>', '<ul class=tab-bar>', '<div class=side-navigation>'),
            (DIVIDER, '</div>', '<div class=document-content>', '')):
        p.require(text.count(value) == 1 and text.count(before + value + after) == 1,
                  'missing/duplicate/altered/misplaced resize shell addition')
        text = text.replace(value, replacement, 1)
    p.require(not any(token in text.lower() for token in
                      ('gnatdoc-resize', 'resizable-navigation', 'navigation-resize')),
              'unauthorized residual resize markup')
    return text


def qualify(actual):
    """Check exact release bytes and manifest identities, not just self-consistency."""
    for name, digest in ASSETS.items():
        p.require(name in actual and p.sha(actual[name]) == digest,
                  'official resize asset identity mismatch: ' + name)
    restored = restore(actual['api/index.html'].decode('utf-8'))
    manifest = json.loads(actual['api/presentation.json'])
    for name in (*ASSETS, 'api/index.html'):
        p.require(manifest['output_sha256'].get(name) == p.sha(actual[name]),
                  'resize output-manifest identity mismatch: ' + name)
    return restored
