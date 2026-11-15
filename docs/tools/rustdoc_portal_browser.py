"""Explicit local Chromium qualification of the assembled project-prefix portal.

Requires the existing separately installed Playwright/browser environment. It is
not a download, deployment authority, or substitute for complete byte verification.
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
    with tempfile.TemporaryDirectory(prefix='rustdoc-portal-browser-') as temp:
        prefix = 'ams-gra-mel-bridge'
        shutil.copytree(site, Path(temp) / prefix)
        server = ThreadingHTTPServer(('127.0.0.1', 0), functools.partial(Quiet, directory=temp))
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        base = f'http://127.0.0.1:{server.server_port}/{prefix}/'
        errors, requests, responses = [], [], []
        checks = []
        try:
            with sync_playwright() as pw:
                browser = pw.chromium.launch(executable_path=str(executable), headless=True)
                context = browser.new_context()
                # This local navigation must not depend on an external resource.
                context.route('**/*', lambda route: route.continue_() if
                              route.request.url.startswith(base) else route.abort())
                page = context.new_page()
                page.on('pageerror', lambda error: errors.append(str(error)))
                page.on('requestfailed', lambda request: errors.append(request.url + ': ' + str(request.failure)))
                page.on('request', lambda request: requests.append(request.url))
                page.on('response', lambda response: responses.append((response.url, response.status)))
                page.set_default_timeout(15000)

                def check(value, label):
                    if not value:
                        raise ValueError(label + ': ' + page.url + ': ' + repr(errors))
                    checks.append(label)

                check(page.goto(base).status == 200, 'root')
                page.wait_for_load_state('networkidle')
                page.get_by_role('link', name='Safe Rust application API (ams-mel)', exact=True).click()
                page.wait_for_load_state('networkidle')
                check(page.url == base + 'rust/ams_mel/index.html', 'safe destination')
                page.locator('h1').wait_for(state='visible')
                check(page.locator('link[rel="stylesheet"]').count() > 0, 'styles linked')
                page.goto(base + 'rust/ams_mel/struct.Session.html')
                page.wait_for_load_state('networkidle')
                page.get_by_role('link', name='Source', exact=True).first.click()
                page.wait_for_load_state('networkidle')
                check('/rust/src/ams_mel/' in page.url, 'source navigation')
                page.goto(base + 'rust/ams_mel/index.html?search=Session')
                page.wait_for_load_state('networkidle')
                page.locator('#search').wait_for(state='visible')
                page.locator('#search a[href*="struct.Session.html"]').first.wait_for(state='visible')
                page.locator('#search a[href*="struct.Session.html"]').first.click()
                page.wait_for_load_state('networkidle')
                check('struct.Session.html' in page.url, 'client search result navigation')
                page.goto(base)
                page.wait_for_load_state('networkidle')
                page.get_by_role('link', name='Low-level unsafe Rust FFI implementation interface (ams-mel-sys)',
                                 exact=True).click()
                page.wait_for_load_state('networkidle')
                check(page.url == base + 'rust/ams_mel_sys/index.html', 'raw destination')
                page.goto(base + 'api/index.html')
                page.wait_for_load_state('networkidle')
                page.locator('#package-navigation').wait_for(state='visible')
                page.locator('#package-search').fill('AMS.MEL.IR')
                check('result' in page.locator('#package-results').inner_text(), 'Ada filter preserved')
                page.goto(base + 'guides/api-map.html')
                page.wait_for_load_state('networkidle')
                check(page.locator('h1').is_visible(), 'Ada API map preserved')
                check(any('/rust/search.index/' in url for url in requests), 'search shards requested')
                check(any('/rust/static.files/' in url and '.css' in url for url in requests), 'styles loaded')
                check(any('.woff2' in url for url in requests), 'fonts loaded')
                check(all(status == 200 for _, status in responses), 'all resource responses successful')
                check(not errors, 'no browser errors or external resource dependencies')
                context.close()
                browser.close()
        finally:
            server.shutdown()
            server.server_close()
            thread.join()
        report = {'checks': checks, 'errors': errors, 'requests': requests, 'responses': responses,
                  'project_prefix': '/' + prefix + '/', 'passed': True}
        output.write_text(json.dumps(report, indent=2) + '\n')
        return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--site', type=Path, required=True)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(run(args.site, args.executable, args.output), indent=2))
