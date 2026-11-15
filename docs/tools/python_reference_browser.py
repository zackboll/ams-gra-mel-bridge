"""Explicit local Chromium qualification of the assembled Python reference.

Uses an already installed Playwright/browser environment. This optional check
does not download tools or authorize deployment. Publication byte verification
and the provider-free documentation build remain independent of a browser.
"""

import argparse
import functools
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import shutil
import tempfile
import threading

from playwright.sync_api import sync_playwright


class Quiet(SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass


def run(site, executable, output):
    exports = len(list((site / 'python/api').glob('*.html')))
    if not exports:
        raise ValueError('missing generated Python reference')
    checks, errors, responses = [], [], []
    with tempfile.TemporaryDirectory(prefix='python-reference-browser-') as temp:
        prefix = 'ams-gra-mel-bridge'
        shutil.copytree(site, Path(temp) / prefix)
        server = ThreadingHTTPServer(('127.0.0.1', 0), functools.partial(Quiet, directory=temp))
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        base = f'http://127.0.0.1:{server.server_port}/{prefix}/'
        try:
            with sync_playwright() as pw:
                browser = pw.chromium.launch(executable_path=str(executable), headless=True)

                def context(**options):
                    result = browser.new_context(**options)
                    result.route('**/*', lambda route: route.continue_() if
                                 route.request.url.startswith(base) else route.abort())
                    result.on('page', lambda page: page.on('pageerror', lambda error: errors.append(str(error))))
                    result.on('requestfailed', lambda request: errors.append(request.url + ': ' + str(request.failure)))
                    result.on('response', lambda response: responses.append((response.url, response.status)))
                    result.set_default_timeout(15000)
                    return result

                desktop = context(viewport={'width': 1440, 'height': 1100})
                page = desktop.new_page()

                def check(value, label):
                    if not value:
                        raise ValueError(label + ': ' + page.url + ': ' + repr(errors))
                    checks.append(label)

                page.goto(base)
                page.get_by_role('link', name='Safe Python application API and guides', exact=True).click()
                check(page.url == base + 'python/index.html', 'portal to Python guide')
                page.get_by_role('link', name='API index', exact=True).first.click()
                check(page.locator('.api-directory > li').count() == exports, 'complete static export directory')
                page.get_by_label('Search the Python API').fill('HealthMetadataStream is_open')
                check(page.locator('#search-results a').count() == 1, 'inherited property search')
                page.locator('#search-results a').click()
                check(page.url == base + 'python/api/HealthMetadataStream.html#is_open',
                      'search result to public owner fragment')
                page.screenshot(path=str(output.with_name(output.stem + '-desktop.png')))

                page.goto(base + 'python/api/ImageConfig.html')
                check('buffer_count' in page.locator('main').inner_text(), 'dataclass field rendering')
                page.get_by_role('link', name='Source definition', exact=True).first.click()
                check('/python/source/' in page.url and '#L' in page.url,
                      'definition to source line navigation')
                check(page.locator('.source').count() == 1, 'escaped source view')

                page.set_viewport_size({'width': 390, 'height': 844})
                for name in ('index.html', 'getting-started.html', 'ownership-and-errors.html',
                             'ir-workflows.html', 'rf-admin.html', 'api-index.html',
                             'api/Session.html', 'api/OpticalDistortionMap.html', 'api/NavigationReport.html'):
                    page.goto(base + 'python/' + name)
                    check(page.evaluate('document.documentElement.scrollWidth <= window.innerWidth'),
                          'mobile width: ' + name)
                page.goto(base + 'python/getting-started.html')
                page.screenshot(path=str(output.with_name(output.stem + '-mobile.png')))

                nojs = context(java_script_enabled=False, viewport={'width': 1440, 'height': 900})
                static = nojs.new_page()
                static.goto(base + 'python/api-index.html')
                check(static.locator('.api-directory > li').count() == exports, 'complete directory without JavaScript')
                static.locator('.api-directory').get_by_role('link', name='Session', exact=True).click()
                check(static.locator('h1').inner_text() == 'ams_mel.Session', 'API navigation without JavaScript')
                static.get_by_role('link', name='Getting started', exact=True).click()
                check(static.locator('h1').inner_text() == 'Getting started with Python',
                      'guide navigation without JavaScript')
                static.get_by_role('link', name='Documentation portal', exact=True).click()
                check(static.url == base + 'index.html', 'return to assembled portal without JavaScript')
                check(all(status == 200 for _, status in responses), 'all resource responses successful')
                check(not errors, 'no browser errors or external resource dependencies')
                nojs.close()
                desktop.close()
                browser.close()
        finally:
            server.shutdown()
            server.server_close()
            thread.join()
    report = {'passed': True, 'checks': checks, 'errors': errors, 'responses': responses,
              'exports': exports, 'project_prefix': '/' + prefix + '/'}
    output.write_text(json.dumps(report, indent=2) + '\n')
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--site', type=Path, required=True)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = run(args.site, args.executable, args.output)
    print(json.dumps({'passed': report['passed'], 'checks': report['checks']}, indent=2))
