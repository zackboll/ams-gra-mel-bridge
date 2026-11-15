"""Chromium regression of the current source-qualified official search site.

This is a test harness, not a search implementation. Every clicked API result
must arrive at its independently reconciled native section in the existing
iframe router. Run only against locally reconstructed, verified inputs.
"""

import argparse
from collections import Counter
import functools
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
import re
from pathlib import Path
import threading
from urllib.parse import urlsplit, unquote

from playwright.sync_api import sync_playwright, expect


class Quiet(SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass


def require(value, message):
    if not value:
        raise ValueError(message)


def run(site, output):
    records = json.loads((site / 'api/gnatdoc-search-index.json').read_bytes())['records']
    required = {
        'AMS.MEL.IR.Image.Navigation_Report': 'symbol',
        'AMS.MEL.IR.Image.Submit_Navigation_Report': 'operation'}
    selected = {r['name']: r for r in records if r['name'] in required}
    require(set(selected) == set(required), 'missing mandatory current declarations')
    result = {'queries': [], 'arrivals': [], 'display': [], 'cache_rejection': [],
              'errors': [], 'external_requests': [], 'record_counts': dict(Counter(r['kind'] for r in records))}
    server = ThreadingHTTPServer(('127.0.0.1', 0), functools.partial(Quiet, directory=str(site)))
    threading.Thread(target=server.serve_forever, daemon=True).start()
    base = f'http://127.0.0.1:{server.server_port}/'
    try:
        with sync_playwright() as pw:
            browser = pw.chromium.launch(headless=True)
            result['chromium'] = browser.version
            context = browser.new_context(viewport={'width': 1280, 'height': 900})
            def offline(route):
                if not route.request.url.startswith(base):
                    result['external_requests'].append(route.request.url)
                    route.abort()
                else:
                    route.continue_()
            context.route('**/*', offline)
            page = context.new_page()
            page.on('pageerror', lambda error: result['errors'].append(str(error)))
            page.set_default_timeout(15000)
            search = page.locator('#package-search')
            links = page.locator('#documentation-search-results li a')
            status = page.locator('#package-results')

            def ready():
                page.goto(base + 'api/index.html')
                expect(status).to_have_text('Search public API names, signatures, and developer guides.')

            def arrival(record):
                path, anchor = record['href'].split('#')
                frame = page.frame_locator('iframe[name="document-content"]')
                target = frame.locator('[id="' + anchor + '"]')
                expect(target).to_have_text(record['name'].rsplit('.', 1)[1])
                expect(target).to_be_in_viewport()
                inner = page.locator('iframe[name="document-content"]').element_handle().content_frame()
                require(urlsplit(inner.url).path == '/api/' + path and
                        unquote(urlsplit(inner.url).fragment) == anchor, 'incorrect native arrival')
                require('package=AMS.MEL.IR.Image' in page.url or
                        record['name'].replace('.', '%2E') in page.url or 'symbol=' in page.url,
                        'missing canonical shell state')
                evidence = {'name': record['name'], 'kind': record['kind'],
                            'native_url': inner.url, 'shell_url': page.url,
                            'anchor': anchor, 'heading': target.inner_text(),
                            'in_viewport': True}
                result['arrivals'].append(evidence)
                return page.url

            ready()
            for query in ('Navigation Report', 'Navigation_Report', 'Submit_Navigation_Report',
                          'AMS.MEL.IR.Image.Navigation_Report'):
                search.fill(query)
                expect(links.first).to_be_visible()
                names = links.all_text_contents()
                expected_names = (list(required) if query in ('Navigation Report', 'Navigation_Report')
                                  else ['AMS.MEL.IR.Image.' + query] if query == 'Submit_Navigation_Report'
                                  else [query])
                for name in expected_names:
                    record = selected[name]
                    require(record['kind'] == required[name], 'wrong result category')
                    link = links.filter(has_text=re.compile('^' + re.escape(name) + '$'))
                    expect(link).to_have_count(1)
                    expect(link.locator('..').locator('.search-kind')).to_have_text(required[name])
                    before = page.url
                    link.click()
                    destination = arrival(record)
                    page.reload()
                    arrival(record)
                    fresh = context.new_page()
                    fresh.goto(destination)
                    expect(fresh.frame_locator('iframe[name="document-content"]').locator(
                        '[id="' + record['href'].split('#')[1] + '"]')).to_be_in_viewport()
                    fresh.close()
                    page.go_back()
                    require(page.url != destination or before == destination, 'history back failed')
                    page.go_forward()
                    arrival(record)
                    ready()
                    search.fill(query)
                result['queries'].append({'query': query, 'results': names, 'mandatory': expected_names})

            # Multiple result keyboard movement, Enter activation and clearing.
            search.fill('Navigation_Report')
            require(links.count() >= 2, 'multiple results required')
            search.press('ArrowDown')
            expect(links.first).to_be_focused()
            page.keyboard.press('End')
            expect(links.last).to_be_focused()
            page.keyboard.press('Home')
            expect(links.first).to_be_focused()
            page.keyboard.press('ArrowDown')
            expect(links.nth(1)).to_be_focused()
            page.keyboard.press('ArrowUp')
            expect(links.first).to_be_focused()
            page.keyboard.press('Escape')
            expect(search).to_be_focused()
            search.press('Enter')
            page.keyboard.press('Enter')
            arrival(selected[links.first.inner_text()])
            page.locator('#package-clear').click()
            expect(search).to_be_focused()
            expect(search).to_have_value('')
            expect(links).to_have_count(0)
            search.fill('unknown-mel-search-001')
            expect(status).to_have_text('No matching documentation.')
            expect(links).to_have_count(0)
            search.fill('<img src=x onerror=alert(1)>')
            expect(status).to_have_text('No matching documentation.')
            expect(page.locator('#documentation-search-results img')).to_have_count(0)
            search.fill('x' * 257)
            expect(status).to_contain_text('at most 256 characters')
            search.press('Escape')
            expect(search).to_have_value('')
            search.fill('AMS.MEL')
            expect(links).to_have_count(50)
            expect(status).to_contain_text('showing first 50')
            result['interaction'] = 'keyboard, Enter, history, reload, fresh direct link, clear, no-match, malicious input, query/result bounds passed'

            # Exhaustive declaration navigation, including every overload and
            # legitimate current-source addition, not merely result text.
            for record in records:
                if record['kind'] not in ('symbol', 'operation'):
                    continue
                search.fill(record['name'])
                link = links.filter(has_text=re.compile('^' + re.escape(record['label']) + '$'))
                expect(link).to_have_count(1)
                link.click()
                path, anchor = record['href'].split('#')
                frame = page.frame_locator('iframe[name="document-content"]')
                target = frame.locator('[id="' + anchor + '"]')
                expect(target).to_have_text(record['name'].rsplit('.', 1)[1])
                expect(target).to_be_in_viewport()
                inner = page.locator('iframe[name="document-content"]').element_handle().content_frame()
                require(urlsplit(inner.url).path == '/api/' + path and
                        unquote(urlsplit(inner.url).fragment) == anchor, 'exhaustive native arrival mismatch')
                result['arrivals'].append({'name': record['name'], 'label': record['label'],
                    'kind': record['kind'], 'native': record['href'], 'in_viewport': True})
            for record in records:
                if record['kind'] == 'guide':
                    search.fill(record['name'])
                    link = links.filter(has_text=re.compile('^' + re.escape(record['label']) + '$'))
                    expect(link).to_have_count(1)
                    link.click()
                    require(urlsplit(page.url).path == '/' + record['href'].removeprefix('../'),
                            'guide incorrectly routed')
                    ready()
            result['guides'] = 'all approved guides clicked as top-level pages'
            context.close()

            for width in (1280, 390):
                for theme in ('light', 'dark'):
                    context = browser.new_context(viewport={'width': width, 'height': 900}, color_scheme=theme)
                    page = context.new_page()
                    page.goto(base + 'api/index.html')
                    expect(page.locator('#package-results')).to_contain_text('Search public API')
                    page.locator('#package-search').fill('Navigation Report')
                    require(page.locator('#documentation-search-results li').count() >= 2,
                            'missing display results')
                    require(page.locator('body').evaluate('(e)=>e.scrollWidth<=innerWidth+2'),
                            'horizontal overflow')
                    result['display'].append({'width': width, 'theme': theme, 'passed': True})
                    context.close()

            original = (site / 'api/gnatdoc-search-index.json').read_bytes()
            for mutation in ('missing', 'corrupt', 'forged'):
                context = browser.new_context()
                def mutate(route, mode=mutation):
                    if mode == 'missing':
                        route.fulfill(status=404, body='missing')
                    else:
                        payload = original + b' ' if mode == 'corrupt' else original.replace(
                            selected['AMS.MEL.IR.Image.Navigation_Report']['href'].encode(),
                            b'https://example.invalid/forged')
                        route.fulfill(status=200, content_type='application/json', body=payload)
                context.route('**/gnatdoc-search-index.json', mutate)
                page = context.new_page()
                page.goto(base + 'api/index.html')
                expect(page.locator('#package-results')).to_contain_text('Documentation search unavailable')
                page.locator('#package-search').fill('Navigation Report')
                expect(page.locator('#documentation-search-results li')).to_have_count(0)
                expect(page.locator('a[target="document-content"]:visible').first).to_be_visible()
                result['cache_rejection'].append({'mutation': mutation, 'rejected': True})
                context.close()

            context = browser.new_context(java_script_enabled=False)
            page = context.new_page()
            page.goto(base + 'api/index.html')
            link = page.locator('a[target="document-content"]:visible').filter(
                has_text=re.compile('^AMS\\.MEL\\.IR\\.Image$')).first
            link.click()
            expect(page.frame_locator('iframe[name="document-content"]').locator('h1')).to_have_text('AMS.MEL.IR.Image')
            result['javascript_disabled'] = 'native package navigation works'
            context.close()
            browser.close()
        require(not result['errors'] and not result['external_requests'], 'runtime/external request failure')
        result['passed'] = True
    finally:
        server.shutdown()
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(result, indent=2, sort_keys=True) + '\n')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--site', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    run(args.site, args.output)