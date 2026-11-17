"""Explicit Chromium color/function qualification; no build-time browser dependency.

Read the palette independently from the qualified Ada output. Serve only the
supplied complete publication on loopback, with a project URL prefix. Evidence
and screenshots are external to the immutable publication.
"""

import argparse
import functools
from http.server import ThreadingHTTPServer
import json
from pathlib import Path
import re
import shutil
import tempfile
import threading

from playwright.sync_api import sync_playwright
from rustdoc_portal_browser import Quiet


def contrast(first, second):
    def luminance(color):
        if len(color) == 4:
            color = '#' + ''.join(c * 2 for c in color[1:])
        values = [int(color[i:i + 2], 16) / 255 for i in (1, 3, 5)]
        linear = [v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4 for v in values]
        return sum(a * b for a, b in zip(linear, (.2126, .7152, .0722)))
    light, dark = sorted((luminance(first), luminance(second)), reverse=True)
    return (light + .05) / (dark + .05)


def rgb(value):
    value = value.strip()
    if len(value) == 4:
        value = '#' + ''.join(c * 2 for c in value[1:])
    return 'rgb(' + ', '.join(str(int(value[i:i + 2], 16)) for i in (1, 3, 5)) + ')'


def run(site, executable, output):
    output.mkdir(parents=True, exist_ok=False)
    css = (site / 'api/gnatdoc-theme.css').read_text()
    blocks = re.findall(r':root\s*\{([^}]+)\}', css)
    palettes = {mode: dict(re.findall(r'--([\w-]+):\s*(#[0-9a-f]+)', block))
                for mode, block in zip(('light', 'dark'), blocks)}
    assert set(palettes) == {'light', 'dark'}, 'missing qualified Ada palettes'
    report = {'checks': [], 'colors': [], 'contrast': [], 'errors': [], 'responses': []}

    def check(value, label):
        if not value:
            raise ValueError(label)
        report['checks'].append(label)

    for mode, palette in palettes.items():
        for foreground in ('text', 'muted', 'link', 'keyword', 'string', 'number', 'comment', 'operator', 'focus'):
            for background in ('page', 'nav', 'code', 'active'):
                ratio = contrast(palette[foreground], palette[background])
                threshold = 3 if foreground == 'focus' else 4.5
                report['contrast'].append({'mode': mode, 'foreground': foreground,
                                           'background': background, 'ratio': ratio})
                check(ratio >= threshold, f'{mode} contrast {foreground}/{background}: {ratio:.3f}')

    with tempfile.TemporaryDirectory(prefix='mel-theme-browser-') as temp:
        shutil.copytree(site, Path(temp) / 'ams-gra-mel-bridge')
        server = ThreadingHTTPServer(('127.0.0.1', 0), functools.partial(Quiet, directory=temp))
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        base = f'http://127.0.0.1:{server.server_port}/ams-gra-mel-bridge/'
        try:
            with sync_playwright() as pw:
                browser = pw.chromium.launch(executable_path=str(executable), headless=True)
                report['browser'] = browser.version
                for width, height in ((1440, 1000), (390, 844)):
                    context = browser.new_context(viewport={'width': width, 'height': height})
                    def route(request):
                        if request.request.url.startswith(base):
                            request.continue_()
                        else:
                            report['errors'].append('external request: ' + request.request.url)
                            request.abort()
                    context.route('**/*', route)
                    context.on('page', lambda page: (
                        page.on('pageerror', lambda e: report['errors'].append(str(e))),
                        page.on('console', lambda m: report['errors'].append(m.text) if m.type == 'error' else None),
                        page.on('requestfailed', lambda r: report['errors'].append(r.url + ': ' + str(r.failure))),
                        page.on('response', lambda r: report['responses'].append([r.url, r.status]))))
                    page = context.new_page()
                    page.set_default_timeout(15000)

                    def goto(relative):
                        check(page.goto(base + relative).status == 200, 'HTTP ' + relative)
                        page.wait_for_load_state('networkidle')

                    def color(selector, prop, expected, label):
                        actual = page.locator(selector).first.evaluate('(e,p) => getComputedStyle(e)[p]', prop)
                        report['colors'].append({'mode': mode, 'width': width, 'element': selector,
                                                 'property': prop, 'actual': actual, 'expected': rgb(expected)})
                        check(actual == rgb(expected), label + ': ' + actual + ' != ' + rgb(expected))

                    for mode in ('light', 'dark'):
                        palette = palettes[mode]
                        page.emulate_media(color_scheme=mode)
                        goto('rust/settings.html')
                        page.locator('#theme-' + mode).check()
                        check(page.locator('html').get_attribute('data-theme') == mode, 'native theme control ' + mode)
                        color('body', 'backgroundColor', palette['page'], 'theme switch background')
                        goto('rust/ams_mel_ir/index.html')
                        page.reload(wait_until='networkidle')
                        check(page.locator('html').get_attribute('data-theme') == mode, 'saved theme reload/navigation')
                        color('body', 'backgroundColor', palette['page'], 'main background')
                        color('body', 'color', palette['text'], 'main text')
                        color('.sidebar', 'backgroundColor', palette['nav'], 'sidebar background')
                        if width > 800:  # Native mobile navigation intentionally removes this border.
                            color('.sidebar', 'borderRightColor', palette['border'], 'sidebar border')
                        color('h1', 'color', palette['text'], 'heading text')
                        color('.docblock a', 'color', palette['link'], 'documentation link')
                        color('.docblock pre', 'backgroundColor', palette['code'], 'code background')
                        color('.sidebar .current a', 'backgroundColor', palette['active'], 'selected navigation')
                        page.locator('.docblock a').first.hover()
                        color('.docblock a', 'color', palette['link'], 'hovered documentation link')
                        check(page.evaluate('document.documentElement.scrollWidth <= innerWidth'), 'no document overflow')
                        if width > 800:
                            bar = page.locator('.sidebar-resizer').bounding_box()
                            before = page.locator('.sidebar').bounding_box()['width']
                            page.mouse.move(bar['x'] + bar['width'] / 2, 350)
                            page.mouse.down()
                            page.mouse.move(before + 90, 350, steps=10)
                            page.mouse.up()
                            after = page.locator('.sidebar').bounding_box()['width']
                            check(after > before + 40, 'native sidebar resize')
                            page.reload(wait_until='networkidle')
                            check(abs(page.locator('.sidebar').bounding_box()['width'] - after) < 2,
                                  'sidebar width persists')
                        else:
                            page.locator('.sidebar-menu-toggle').click()
                            check(page.locator('.sidebar').is_visible(), 'mobile navigation opens')
                            page.locator('.sidebar-menu-toggle').click()
                        page.screenshot(path=str(output / f'rust-{mode}-{width}.png'))
                        goto('api/index.html')
                        color('html', 'backgroundColor', palette['page'], 'Ada page authority')
                        color('.side-navigation', 'backgroundColor', palette['nav'], 'Ada sidebar authority')
                        page.screenshot(path=str(output / f'ada-{mode}-{width}.png'))

                        goto('rust/ams_mel/index.html?search=Session')
                        result = page.locator('#search a[href*="struct.Session.html"]').first
                        result.wait_for(state='visible')
                        result.focus()
                        color('#search a[href*="struct.Session.html"]', 'backgroundColor', palette['active'],
                              'focused search selection')
                        page.keyboard.press('Enter')
                        page.wait_for_url('**/struct.Session.html*')
                        page.wait_for_load_state('networkidle')
                        check('struct.Session.html' in page.url, 'keyboard search result selection')
                        color('.item-decl', 'backgroundColor', palette['code'], 'declaration surface')
                        page.get_by_role('link', name='Source', exact=True).first.click()
                        page.wait_for_load_state('networkidle')
                        check('/rust/src/ams_mel_ir/' in page.url, 'source browsing')
                        color('.kw', 'color', palette['keyword'], 'syntax keyword')
                        color('.string', 'color', palette['string'], 'syntax string')
                        color('.comment', 'color', palette['comment'], 'syntax comment')
                        for crate in ('ams_mel_common', 'ams_mel_ir', 'ams_mel_rf', 'ams_mel_sys'):
                            goto('rust/' + crate + '/index.html')
                            color('body', 'backgroundColor', palette['page'], crate + ' background')
                            color('body', 'color', palette['text'], crate + ' text')
                            color('h1', 'color', palette['text'], crate + ' heading')
                        goto('rust/ams_mel/enum.FailureLevel.html')
                        color('.docblock table', 'color', palette['text'], 'table foreground')
                        color('.docblock table tr:nth-child(even)', 'backgroundColor', palette['code'],
                              'table alternate surface')
                        check(page.evaluate('document.documentElement.scrollWidth <= innerWidth'), 'table page no overflow')
                        goto('rust/ams_mel_sys/struct.AmsMelIrBufferInfoV1.html')
                        color('.stab', 'backgroundColor', palette['code'], 'callout surface')
                        color('.stab', 'color', palette['text'], 'callout foreground')
                        goto('rust/ams_mel/index.html')
                        page.keyboard.press('s')
                        search = page.locator('.search-input:visible')
                        check(search.evaluate('e => e === document.activeElement'), 'keyboard search shortcut')
                        color('.search-input:visible', 'borderColor', palette['focus'], 'search focus indicator')
                        search.fill('Session')
                        page.locator('#search a[href*="struct.Session.html"]').first.wait_for(state='visible')
                        goto('rust/settings.html')
                        page.locator('#theme-system-preference').check()
                        page.emulate_media(color_scheme='dark' if mode == 'light' else 'light')
                        system = 'dark' if mode == 'light' else 'light'
                        page.wait_for_function('(theme) => document.documentElement.dataset.theme === theme', arg=system)
                        check(page.locator('html').get_attribute('data-theme') == system, 'system preference change')
                        color('body', 'backgroundColor', palettes[system]['page'], 'system palette')
                        page.locator('#theme-ayu').check()
                        check(page.locator('html').get_attribute('data-theme') == 'ayu', 'Ayu selection')
                        color('body', 'backgroundColor', '#0f1419', 'Ayu remains native')
                    context.close()
                browser.close()
            check(all(status == 200 for _, status in report['responses']), 'all resources return 200')
            check(not report['errors'], 'no console, page, network or external errors: ' + repr(report['errors']))
            report['passed'] = True
        finally:
            server.shutdown()
            server.server_close()
            thread.join()
            (output / 'browser.json').write_text(json.dumps(report, indent=2) + '\n')
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--site', type=Path, required=True)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(run(args.site, args.executable, args.output), indent=2))
