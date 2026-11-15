"""Small provider-free Python publication for assembly and transport tests."""


def site():
    pages = ('index.html', 'api-index.html', 'getting-started.html',
             'ownership-and-errors.html', 'ir-workflows.html', 'rf-admin.html')
    files = {name: (b'<!doctype html><html><head><link rel="stylesheet" '
                    b'href="assets/reference.css"></head><body><main id="main">'
                    b'<a href="api/Session.html#open">Session.open</a>'
                    b'<a href="../index.html">Portal</a></main></body></html>')
             for name in pages}
    files['api-index.html'] += (b'<script src="assets/search-index.js" defer></script>'
                                b'<script src="assets/search.js" defer></script>')
    files.update({
        'api/Session.html': (b'<html><head><link rel="stylesheet" href="../assets/reference.css">'
                             b'</head><body><h1 id="open">Session.open</h1>'
                             b'<a href="../source/_session.html#L1">Source</a>'
                             b'<a href="../../index.html">Portal</a></body></html>'),
        'source/_session.html': b'<html><body><pre id="L1">class Session: pass</pre></body></html>',
        'assets/reference.css': b'body { font-family: system-ui, sans-serif; }\n',
        'assets/search.js': b'"use strict";\n',
        'assets/search-index.js': b'"use strict"; const pythonSearch = [];\n',
    })
    return files
