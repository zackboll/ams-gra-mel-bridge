"""Bounded Chromium pilot: production site and accepted candidate pages only.

Requires the separately installed Playwright driver/browser. An accepted-page
view is never called a complete candidate publication: the two unsupported
pages are explicitly reported as unavailable.
"""

import argparse
import functools
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import tempfile
import threading
from urllib.parse import urlencode

from playwright.sync_api import sync_playwright


class Quiet(SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass


def run(baseline, probe, executable, output):
    evidence = json.loads((probe / 'evidence.json').read_bytes())
    symbols = evidence['symbols']
    accepted = probe / 'candidate-accepted-only'
    pages = [n for n in evidence['guides'] if n.endswith('.html')]
    report = []
    with tempfile.TemporaryDirectory(prefix='mel-pilot-browser-') as temp:
        # A navigation test view, not renderer output: only accepted pages overlay
        # the independently qualified production API and unsupported guide fallback.
        import shutil
        view = Path(temp) / 'accepted-view-with-production-fallback'
        shutil.copytree(baseline / 'published', view)
        shutil.copytree(accepted, view, dirs_exist_ok=True)
        with sync_playwright() as pw:
            browser = pw.chromium.launch(executable_path=str(executable), headless=True)
            for label, site in [('production', baseline / 'published'),
                                ('accepted-view-with-production-fallback', view)]:
                server = ThreadingHTTPServer(('127.0.0.1', 0),
                    functools.partial(Quiet, directory=str(site)))
                thread = threading.Thread(target=server.serve_forever, daemon=True)
                thread.start()
                url = f'http://127.0.0.1:{server.server_port}/'
                try:
                    for width in (1280, 390):
                        for theme in ('light', 'dark'):
                            for js in (True, False):
                                context = browser.new_context(viewport={'width': width, 'height': 800},
                                    color_scheme=theme, java_script_enabled=js)
                                page = context.new_page()
                                errors = []
                                page.on('pageerror', lambda error: errors.append(str(error)))
                                page.set_default_timeout(8000)
                                checks = 0

                                def check(value):
                                    nonlocal checks
                                    if not value:
                                        raise ValueError((label, width, theme, js, page.url))
                                    checks += 1

                                for name in pages:
                                    check(page.goto(url + name).status == 200)
                                    check(page.locator('h1').is_visible())
                                    check(page.locator('body').evaluate('(e)=>e.scrollWidth<=innerWidth+2'))
                                page.goto(url + 'api/index.html')
                                frame = page.frame_locator('iframe[name="document-content"]')
                                if js:
                                    page.locator('#package-navigation').wait_for(state='visible')
                                    frame.get_by_role('heading', name='AMS.MEL', exact=True).wait_for()
                                    check('package=AMS.MEL' in page.url)
                                    page.locator('#package-search').fill('AMS.MEL.IR')
                                    check('result' in page.locator('#package-results').inner_text())
                                    page.locator('#package-clear').click()
                                    check(page.locator('#package-search').input_value() == '')
                                else:
                                    check(not page.locator('#package-navigation').is_visible())
                                for package in ('AMS.MEL.IR', 'AMS.MEL.RF', 'AMS.MEL'):
                                    page.get_by_role('link', name=package, exact=True).click()
                                    frame.get_by_role('heading', name=package, exact=True).wait_for()
                                    check(True)
                                selected = [next(s for s in symbols if s['canonical'] == name)
                                    for name in ('AMS.MEL.Version', 'AMS.MEL.IR.UUID', 'AMS.MEL.RF.Data_MEL')]
                                if js:
                                    for symbol in selected:
                                        page.goto(url + 'api/index.html?' + urlencode({
                                            'package': symbol['owner'], 'symbol': symbol['canonical']}))
                                        frame.locator('[id="' + symbol['anchor'] + '"]').wait_for(state='visible')
                                        check(page.locator('a[aria-current="page"]').inner_text() == symbol['owner'])
                                    page.goto(url + 'api/index.html?package=AMS.MEL.RF')
                                    frame.get_by_role('heading', name='AMS.MEL.RF', exact=True).wait_for()
                                    page.get_by_role('link', name='AMS.MEL', exact=True).click()
                                    frame.get_by_role('heading', name='AMS.MEL', exact=True).wait_for()
                                    page.go_back()
                                    frame.get_by_role('heading', name='AMS.MEL.RF', exact=True).wait_for()
                                    check(True)
                                for symbol in selected:
                                    page.goto(url + 'api/' + symbol['page'] + '#' + symbol['anchor'])
                                    check(page.locator('[id="' + symbol['anchor'] + '"]').is_visible())
                                    check(page.locator('body').evaluate('(e)=>e.scrollWidth<=innerWidth+2'))
                                page.goto(url + 'guides/examples.html')
                                check(page.locator('pre').count() > 0)
                                check(page.locator('a[href*="../api/"]').count() > 0)
                                check(not errors)
                                report.append(dict(site=label, width=width, theme=theme,
                                                   javascript=js, checks=checks, passed=True))
                                context.close()
                finally:
                    server.shutdown()
                    server.server_close()
                    thread.join()
            browser.close()
    output.write_text(json.dumps(dict(contexts=report, candidate_complete=False,
        candidate_unavailable=['guides/examples.html', 'guides/api-map.html'],
        fallback='production API, Examples and API Map are not candidate qualification'), indent=2) + '\n')
    print(f'{sum(r["checks"] for r in report)} checks / {len(report)} contexts passed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--chromium', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    run(args.baseline, args.probe, args.chromium, args.output)