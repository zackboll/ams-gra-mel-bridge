"""Real Chromium source-view acceptance on the complete assembled MEL site."""

import argparse
from collections import Counter
import functools
from http.server import ThreadingHTTPServer
import json
from pathlib import Path
import threading
from urllib.parse import urlsplit

from playwright.sync_api import sync_playwright, expect
from search_browser import Quiet, require


def run(root, site, output):
    manifest = json.loads((site / 'api/presentation.json').read_bytes())['source_view']
    records = manifest['files']
    # Source bytes, not presentation metadata, validate the task's mandatory lines.
    raw = (root / 'ada/src/ams-mel-ir-image.ads').read_text()
    lines = raw.splitlines()
    require('type Navigation_Report is record' in lines[662], 'Navigation_Report moved from task HEAD line 663')
    require('function Submit_Navigation_Report' in lines[774], 'Submit_Navigation_Report moved from task HEAD line 775')
    result = dict(passed=False, journeys=[], sources=[], display=[], errors=[], external_requests=[])
    server = ThreadingHTTPServer(('127.0.0.1', 0), functools.partial(Quiet, directory=str(site)))
    threading.Thread(target=server.serve_forever, daemon=True).start()
    base = f'http://127.0.0.1:{server.server_port}/'
    try:
        with sync_playwright() as pw:
            browser = pw.chromium.launch(headless=True)
            result['chromium'] = browser.version

            def context(**kwargs):
                ctx = browser.new_context(**kwargs)
                def offline(route):
                    if route.request.url.startswith(base):
                        route.continue_()
                    else:
                        result['external_requests'].append(route.request.url)
                        route.abort()
                ctx.route('**/*', offline)
                ctx.on('page', lambda page: page.on('pageerror', lambda error: result['errors'].append(str(error))))
                return ctx

            ctx = context(viewport=dict(width=1280, height=900))
            page = ctx.new_page()
            page.goto(base + 'api/index.html')
            expect(page.locator('#package-results')).to_have_text(
                'Search public API names, signatures, and developer guides.')
            for query, name, line, token in (
                    ('Navigation Report', 'AMS.MEL.IR.Image.Navigation_Report', 663, 'type Navigation_Report is record'),
                    ('Submit_Navigation_Report', 'AMS.MEL.IR.Image.Submit_Navigation_Report', 775,
                     'function Submit_Navigation_Report')):
                search = page.locator('#package-search')
                search.fill(query)
                link = page.locator('#documentation-search-results li a').get_by_text(name, exact=True)
                expect(link).to_have_count(1)
                link.click()
                frame = page.frame_locator('iframe[name="document-content"]')
                heading = frame.get_by_role('heading', name=name.rsplit('.', 1)[1], exact=True)
                expect(heading).to_have_count(1)
                source = heading.locator('xpath=following-sibling::a[1]')
                expected = 'source/ada/src/ams-mel-ir-image.ads.html#L' + str(line)
                expect(source).to_have_attribute('href', expected)
                expect(source).to_have_text('View source')
                source.focus()
                source.press('Enter')
                expect(page).to_have_url(base + 'api/' + expected)
                expect(page.locator('#L' + str(line))).to_contain_text(token)
                result['journeys'].append(dict(query=query, declaration=name, url=page.url, line=line,
                                               text=page.locator('#L' + str(line)).inner_text()))
                page.reload()
                expect(page.locator('#L' + str(line))).to_contain_text(token)
                page.go_back()
                expect(page.locator('iframe[name="document-content"]')).to_be_visible()
                page.go_forward()
                expect(page.locator('#L' + str(line))).to_contain_text(token)
                page.go_back()

            # All source DOMs, all ordered anchors, package links, and all overload
            # source links (not merely a name-only representative).
            for record in records:
                source_url = base + record['page']
                fresh = ctx.new_page()
                fresh.goto(source_url)
                expected_text = (root / record['source']).read_bytes().decode('utf-8')
                require(fresh.locator('pre code').text_content() == expected_text, 'source DOM text differs')
                anchors = fresh.locator('span.source-line').evaluate_all('(nodes) => nodes.map(n => n.id)')
                require(anchors == ['L' + str(i) for i in range(1, record['lines'] + 1)], 'source line anchor order differs')
                selected = fresh.locator('pre code').evaluate('''node => {
                    const range = document.createRange(); range.selectNodeContents(node);
                    const selection = window.getSelection(); selection.removeAllRanges(); selection.addRange(range);
                    return selection.toString(); }''')
                # Chromium Selection.toString omits a terminal block newline;
                # textContent above remains the exact original byte projection.
                require(selected == expected_text or
                        (expected_text.endswith('\n') and selected == expected_text[:-1]),
                        'line numbers contaminate source selection')
                fresh.locator('.source-line-number').first.focus()
                expect(fresh.locator('.source-line-number').first).to_be_focused()
                fresh.close()
                package = record['declarations'][0]
                # Native page identity is independently verified before campaign.
                native_page = next(n for n in (site / 'api').glob('*.html')
                                   if '<h1>' + record['package'] + '</h1>' in n.read_text())
                page.goto(base + 'api/' + native_page.name)
                package_link = page.locator('h1 + a.view-source')
                expect(package_link).to_have_attribute('href', 'source/' + record['source'] + '.html#L' + str(package['line']))
                package_link.click()
                expect(page).to_have_url(source_url + '#L' + str(package['line']))
                groups = Counter(d['name'] for d in record['declarations'][1:])
                overloads = [d for d in record['declarations'][1:] if groups[d['name']] > 1]
                if overloads:
                    page.goto(base + 'api/' + native_page.name)
                    for declaration in overloads:
                        locator = page.locator('[id="' + declaration['anchor'] + '"] + a.view-source')
                        expect(locator).to_have_attribute('href', 'source/' + record['source'] +
                                                         '.html#L' + str(declaration['line']))
                result['sources'].append(dict(source=record['source'], lines=len(anchors),
                                               overload_links=len(overloads), selection=True, package_navigation=True))
            ctx.close()
            for javascript in (True, False):
                for width in (1280, 375):
                    for scheme in ('light', 'dark'):
                        ctx = context(java_script_enabled=javascript, viewport=dict(width=width, height=850),
                                      color_scheme=scheme)
                        page = ctx.new_page()
                        page.goto(base + 'api/source/ada/src/ams-mel-ir-image.ads.html#L663')
                        expect(page.locator('#L663')).to_contain_text('type Navigation_Report is record')
                        page.reload()
                        expect(page.locator('#L663')).to_contain_text('type Navigation_Report is record')
                        page.goto(base + 'api/index.html')
                        expect(page.locator('iframe[name="document-content"]')).to_have_count(1)
                        result['display'].append(dict(javascript=javascript, width=width, color_scheme=scheme))
                        ctx.close()
            browser.close()
        require(not result['errors'] and not result['external_requests'], 'source browser runtime/network failures')
        result['passed'] = True
    finally:
        server.shutdown()
        server.server_close()
        output.write_text(json.dumps(result, sort_keys=True, indent=2) + '\n')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--site', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    run(args.root, args.site, args.output)
