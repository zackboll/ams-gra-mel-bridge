"""Dedicated Chromium qualification of the complete MEL publication, no overlays."""

import argparse
from collections import Counter
import functools
from http.server import ThreadingHTTPServer
import json
from pathlib import Path
import re
import threading
from urllib.parse import urlsplit, unquote

from playwright.sync_api import sync_playwright, expect
from search_browser import Quiet, require


def run(site, output):
    records = json.loads((site / 'api/gnatdoc-search-index.json').read_bytes())['records']
    require((site / 'rust/ams_mel/index.html').is_file() and
            (site / 'python/index.html').is_file(), 'complete assembled site required')
    result = {'checks': [], 'arrivals': [], 'errors': [], 'external_requests': []}

    class Scoped(Quiet):
        # Serve identical complete bytes at two project paths on the same origin.
        # No content, script, CSS or index is synthesized or replaced.
        def translate_path(self, path):
            for prefix in ('/project-a/', '/project-b/'):
                if path.startswith(prefix):
                    path = '/' + path[len(prefix):]
                    break
            return super().translate_path(path)

    server = ThreadingHTTPServer(('127.0.0.1', 0), functools.partial(Scoped, directory=str(site)))
    threading.Thread(target=server.serve_forever, daemon=True).start()
    base = f'http://127.0.0.1:{server.server_port}'
    url = base + '/project-a/api/index.html'
    key = 'gnatdoc:sidebar-width:v1:/project-a/api/'

    def width(page):
        return round(page.locator('.side-navigation').bounding_box()['width'])

    def clean(page):
        require(not page.locator('body').evaluate('(e)=>e.classList.contains("navigation-resizing")'),
                'drag class leaked')
        require(page.locator('iframe').evaluate('(e)=>getComputedStyle(e).pointerEvents') != 'none',
                'iframe shield leaked')
        require(not page.locator('#navigation-resize').evaluate('(e)=>e.hasPointerCapture(1)'),
                'pointer capture leaked')

    def start(page):
        box = page.locator('#navigation-resize').bounding_box()
        page.mouse.move(box['x'] + box['width'] / 2, box['y'] + 100)
        page.mouse.down()

    def drag(page, x):
        start(page)
        page.mouse.move(x, 200, steps=12)
        page.mouse.up()
        clean(page)

    def viewport(page, value):
        page.set_viewport_size({'width': value, 'height': 900})
        page.evaluate('()=>new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r)))')

    def arrival(page, record):
        path, anchor = record['href'].split('#')
        frame = page.locator('iframe').element_handle().content_frame()
        target = page.frame_locator('iframe').locator('[id="' + anchor + '"]')
        expect(target).to_have_text(record['name'].rsplit('.', 1)[1])
        expect(target).to_be_in_viewport()
        require(urlsplit(frame.url).path == '/project-a/api/' + path and
                unquote(urlsplit(frame.url).fragment) == anchor, 'wrong native destination')
        result['arrivals'].append({'name': record['name'], 'href': record['href'],
                                   'in_viewport': True, 'width': width(page)})

    def search(page, record, query):
        page.locator('#package-search').fill(query)
        link = page.locator('#documentation-search-results a').filter(
            has_text=re.compile('^' + re.escape(record['label']) + '$'))
        expect(link).to_have_count(1)
        link.click()
        arrival(page, record)

    try:
        with sync_playwright() as pw:
            browser = pw.chromium.launch(headless=True)
            result['chromium'] = browser.version
            for theme in ('light', 'dark'):
                context = browser.new_context(viewport={'width': 1280, 'height': 900}, color_scheme=theme)
                def offline(route):
                    if not route.request.url.startswith(base + '/'):
                        result['external_requests'].append(route.request.url)
                        route.abort()
                    else:
                        route.continue_()
                context.route('**/*', offline)
                page = context.new_page()
                page.set_default_timeout(15000)
                page.on('pageerror', lambda error: result['errors'].append(str(error)))
                page.goto(url)
                divider = page.get_by_role('separator', name='Navigation width')
                expect(divider).to_be_visible()
                initial = width(page)
                for attribute, value in {'role': 'separator', 'aria-orientation': 'vertical',
                                         'aria-label': 'Navigation width',
                                         'aria-controls': 'resizable-navigation', 'tabindex': '0',
                                         'aria-valuemin': '240', 'aria-valuemax': '952'}.items():
                    expect(divider).to_have_attribute(attribute, value)
                start(page)
                page.mouse.move(1250, 200, steps=12)
                require(width(page) == 952, 'maximum drag clamp failed')
                frame = page.locator('iframe').bounding_box()
                require(frame['x'] < 1250 < frame['x'] + frame['width'], 'pointer not over actual iframe')
                require(divider.evaluate('(e)=>e.hasPointerCapture(1)'), 'missing pointer capture')
                require(page.locator('iframe').evaluate('(e)=>getComputedStyle(e).pointerEvents') == 'none',
                        'missing iframe shield')
                page.mouse.up()
                clean(page)
                drag(page, 640)
                selected = width(page)
                expect(divider).to_have_attribute('aria-valuenow', str(selected))
                expect(divider).to_have_attribute('aria-valuetext', str(selected) + ' pixels')
                page.reload()
                expect(divider).to_be_visible()
                require(width(page) == selected, 'reload lost width')
                for button, expected in (('Home', 240), ('ArrowRight', 256), ('ArrowLeft', 240), ('End', 952)):
                    divider.focus()
                    page.keyboard.press(button)
                    require(width(page) == expected, 'keyboard resize failed: ' + button)
                    require(page.locator('.document-content').bounding_box()['width'] >= 320,
                            'minimum document width violated')
                viewport(page, 900)
                require(width(page) == 572, 'constrained desktop width incorrect')
                viewport(page, 1280)
                require(width(page) == 952, 'viewport clamping overwrote preference')
                divider.dblclick()
                require(width(page) == initial and page.evaluate('(k)=>localStorage.getItem(k)', key) is None,
                        'double click reset failed')
                for event in ('pointercancel', 'lostpointercapture', 'blur', 'resize', 'mobile'):
                    start(page)
                    page.mouse.move(700, 200, steps=8)
                    if event in ('resize', 'mobile'):
                        viewport(page, 1000 if event == 'resize' else 390)
                    elif event == 'blur':
                        page.evaluate('window.dispatchEvent(new Event("blur"))')
                    elif event == 'lostpointercapture':
                        divider.evaluate('(e)=>e.releasePointerCapture(1)')
                    else:
                        divider.evaluate('(e,t)=>e.dispatchEvent(new PointerEvent(t,{pointerId:1}))', event)
                    page.mouse.up()
                    clean(page)
                    require(page.evaluate('(k)=>localStorage.getItem(k)', key) is None,
                            'cancelled gesture saved preference')
                    viewport(page, 1280)
                for raw, expected in (('NaN', initial), ('Infinity', initial), ('-20', initial),
                                      ('{}', initial), ('1e3', initial), ('0', initial),
                                      ('9999999', 952), ('1', 240), ('320.5', 321)):
                    page.evaluate('([k,v])=>localStorage.setItem(k,v)', [key, raw])
                    page.reload()
                    expect(divider).to_be_visible()
                    require(width(page) == expected, 'storage validation failed: ' + raw)
                divider.dblclick()
                drag(page, 600)
                selected = width(page)
                page.goto(base + '/project-b/api/index.html')
                expect(divider).to_be_visible()
                require(width(page) == initial, 'preference leaked across project paths')
                page.goto(url)
                expect(divider).to_be_visible()
                require(width(page) == selected, 'path-scoped preference not restored')

                # Mandatory regression: actual search, native heading scroll,
                # resize, history, reload, then repeat the original search.
                record = next(r for r in records if r['name'] == 'AMS.MEL.IR.Image.Navigation_Report')
                search(page, record, 'Navigation Report')
                bookmark = page.url
                drag(page, 700)
                selected = width(page)
                arrival(page, record)
                page.go_back()
                page.go_forward()
                arrival(page, record)
                require(page.url == bookmark and width(page) == selected, 'history changed bookmark/width')
                page.reload()
                arrival(page, record)
                search(page, record, 'Navigation Report')
                require(width(page) == selected, 'search/reload changed width')
                submit = next(r for r in records if r['name'] == 'AMS.MEL.IR.Image.Submit_Navigation_Report')
                search(page, submit, 'Submit_Navigation_Report')
                counts = Counter(r['name'] for r in records if r['kind'] == 'operation')
                overloaded = next(name for name, count in counts.items() if count > 1)
                for operation in (r for r in records if r['kind'] == 'operation' and r['name'] == overloaded):
                    search(page, operation, operation['name'])
                    require(width(page) == selected, 'overload navigation changed width')
                fresh = context.new_page()
                fresh.goto(bookmark)
                arrival(fresh, record)
                require(width(fresh) == selected, 'bookmark lost preference')
                fresh.goto(base + '/project-a/api/' + record['href'])
                expect(fresh.locator('[id="' + record['href'].split('#')[1] + '"]')).to_be_in_viewport()
                expect(fresh.locator('#navigation-resize')).to_have_count(0)
                fresh.close()
                for size in (811, 810, 390):
                    viewport(page, size)
                    if size > 810:
                        expect(divider).to_be_visible()
                    else:
                        expect(divider).to_be_hidden()
                        nav = page.locator('.side-navigation').bounding_box()
                        doc = page.locator('.document-content').bounding_box()
                        require(doc['y'] >= nav['y'] + nav['height'] - 1, 'mobile layout not stacked')
                        clean(page)
                    require(page.locator('body').evaluate('(e)=>e.scrollWidth<=innerWidth+1'),
                            'responsive horizontal overflow')
                    page.locator('#package-search').fill('Navigation Report')
                    expect(page.locator('#documentation-search-results a').first).to_be_visible()
                viewport(page, 1280)
                require(width(page) == selected, 'mobile layout overwrote desktop width')
                result['checks'].append({'theme': theme, 'passed': True,
                    'interactions': ['drag-over-iframe', 'capture-cleanup', 'keyboard', 'aria', 'bounds',
                        'double-click', 'storage-validation', 'path-scope', 'search-native-scroll',
                        'resize-history-reload-repeat-search', 'submit', 'overloads', 'bookmark',
                        'direct-native-url', 'mobile-search', 'responsive-preference']})
                context.close()

            for storage in ('getter-denied', 'methods-denied', 'javascript-disabled'):
                context = browser.new_context(java_script_enabled=storage != 'javascript-disabled')
                if storage == 'getter-denied':
                    context.add_init_script('Object.defineProperty(window,"localStorage",{get(){throw new Error("denied")}})')
                elif storage == 'methods-denied':
                    context.add_init_script('for(const m of ["getItem","setItem","removeItem"]) Storage.prototype[m]=()=>{throw new Error("denied")};')
                page = context.new_page()
                page.on('pageerror', lambda error: result['errors'].append(str(error)))
                page.goto(url)
                divider = page.locator('#navigation-resize')
                if storage != 'javascript-disabled':
                    expect(divider).to_be_visible()
                    drag(page, 650)
                    require(width(page) > 600, 'storage failure disabled resize')
                    search(page, record, 'Navigation Report')
                    divider.dblclick()
                else:
                    expect(divider).to_be_hidden()
                    page.get_by_role('link', name='AMS.MEL.IR.Image', exact=True).click()
                    expect(page.frame_locator('iframe').locator('h1')).to_have_text('AMS.MEL.IR.Image')
                result['checks'].append({'fallback': storage, 'passed': True})
                context.close()
            browser.close()
        require(not result['errors'] and not result['external_requests'], 'runtime or external request failure')
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