"""Chromium acceptance against independently measured MEL body destinations."""

import argparse
from collections import Counter
from functools import partial
from http.server import ThreadingHTTPServer
import json
from pathlib import Path
import threading

from playwright.sync_api import expect, sync_playwright

from search_browser import Quiet, require
from source_view_audit import TOKEN_CLASSES


def source_path(path, line):
    return f'api/source/{path}.html#L{line}'


def link(page, record, entry, body=False, framed=False):
    heading = '[id="' + entry['anchor'] + '"]' if entry['anchor'] else 'h1'
    host = page.frame_locator('iframe[name="document-content"]') if framed else page
    adjacent = ' + a.view-source + a.view-source' if body else ' + a.view-source'
    result = host.locator(heading + adjacent)
    if body and 'body_line' not in entry:
        expect(result).to_have_count(0)
        return None
    path = source_path(record['body'], entry['body_line']) if body else source_path(record['specification'], entry['line'])
    expect(result).to_have_text('View body' if body else 'View specification')
    expect(result).to_have_attribute('href', path.removeprefix('api/'))
    return result


def choose(records, package, name, *, linked=True):
    rec = records[package]
    matches = [d for d in rec['declarations'] if d['name'].rsplit('.', 1)[-1] == name and
               ('body_line' in d) == linked]
    require(len(matches) == 1, 'mandatory authentic declaration is missing or ambiguous: ' + package + '.' + name)
    return rec, matches[0]


def activate(locator):
    locator.focus()
    expect(locator).to_be_focused()
    locator.press('Enter')


def arrive(page, base, target):
    expect(page).to_have_url(base + target)
    expect(page.locator('#' + target.rsplit('#', 1)[1])).to_be_in_viewport(ratio=0.01)


def selected_source(locator):
    return locator.evaluate('''code => {
      const selection = window.getSelection(), range = document.createRange();
      range.selectNodeContents(code); selection.removeAllRanges(); selection.addRange(range);
      return selection.toString();
    }''')


def run(root, site, records, output):
    image = 'AMS.MEL.IR.Image'
    commands = 'AMS.MEL.IR.Scheduling.Commands'
    submit = choose(records, image, 'Submit_Navigation_Report')
    navigation = choose(records, image, 'Navigation_Report', linked=False)
    stare = choose(records, commands, 'Exec_Stare')
    common_proc = choose(records, 'AMS.MEL', 'Close')
    common_func = choose(records, 'AMS.MEL', 'ABI_Version')
    rf = choose(records, 'AMS.MEL.RF.Admin', 'Open')
    command = records[commands]
    waits = [d for d in command['declarations'] if d['name'].endswith('.Wait')]
    linked_wait = [d for d in waits if 'body_line' in d]
    require(len({d['body_line'] for d in linked_wait}) >= 2 and
            any(d.get('body_omission') == 'implementation-expression' for d in waits),
            'mandatory distinct ordinary / expression-only Wait profiles are missing')
    server = ThreadingHTTPServer(('127.0.0.1', 0), partial(Quiet, directory=str(site)))
    threading.Thread(target=server.serve_forever, daemon=True).start()
    base = f'http://127.0.0.1:{server.server_port}/'
    output_data = dict(display=[], journeys=[], inventory=[], clipboard=[], errors=0, external_requests=0)
    try:
        with sync_playwright() as pw:
            chromium = pw.chromium.launch(headless=True)
            output_data['chromium'] = chromium.version

            def context(**kwargs):
                browser = chromium.new_context(service_workers='block', **kwargs)

                def offline(route):
                    if route.request.url.startswith(base):
                        route.continue_()
                    else:
                        output_data['external_requests'] += 1
                        route.abort()

                browser.route('**/*', offline)
                browser.on('page', lambda page: page.on('pageerror', lambda _: output_data.update(
                    errors=output_data['errors'] + 1)))
                return browser

            ctx = context(viewport={'width': 1280, 'height': 900}, permissions=['clipboard-read', 'clipboard-write'])
            page = ctx.new_page()
            for query, record, entry in [('Navigation Report', *navigation), ('Submit_Navigation_Report', *submit),
                                         ('Exec_Stare', *stare), ('ABI_Version', *common_func)]:
                page.goto(base + 'api/index.html')
                page.locator('#package-search').fill(query)
                result = page.locator('#documentation-search-results li a').get_by_text(entry['name'], exact=True)
                expect(result).to_have_count(1)
                result.click()
                direct = link(page, record, entry, body=True, framed=True)
                if direct is None:
                    direct = link(page, record, entry, framed=True)
                    target = source_path(record['specification'], entry['line'])
                else:
                    target = source_path(record['body'], entry['body_line'])
                    link(page, record, entry, framed=True)
                activate(direct)
                arrive(page, base, target)
                page.reload()
                arrive(page, base, target)
                page.go_back()
                expect(page.locator('iframe[name="document-content"]')).to_have_count(1)
                page.go_forward()
                arrive(page, base, target)
                output_data['journeys'].append(dict(name=entry['name'], via='search', specification=entry['line'],
                                                    body=entry.get('body_line'), anchor=entry['anchor']))

            counts = Counter()
            for package, record in records.items():
                page.goto(base + record['native'])
                observed = page.locator('a.view-source').evaluate_all('(nodes) => nodes.map(a => [a.textContent, a.getAttribute("href")])')
                wanted = []
                for entry in record['declarations']:
                    wanted.append(['View specification', source_path(record['specification'], entry['line']).removeprefix('api/')])
                    if 'body_line' in entry:
                        wanted.append(['View body', source_path(record['body'], entry['body_line']).removeprefix('api/')])
                require(Counter(map(tuple, observed)) == Counter(map(tuple, wanted)), 'browser native link inventory differs: ' + package)
                package_heading = record['declarations'][0]
                if record['body']:
                    activate(link(page, record, package_heading, body=True))
                    arrive(page, base, source_path(record['body'], package_heading['body_line']))
                else:
                    link(page, record, package_heading, body=True)
                for reverse, path in [(False, record['specification']), (True, record['body'])]:
                    if not path:
                        continue
                    page.goto(base + 'api/source/' + path + '.html')
                    code = page.locator('pre.source-code > code')
                    text = (root / path).read_bytes().decode()
                    line_count = text.count('\n') + bool(text and not text.endswith('\n'))
                    require(code.text_content() == text, 'source DOM text differs: ' + path)
                    anchors = code.locator('.source-line').evaluate_all('(nodes) => nodes.map(n => n.id)')
                    require(anchors == [f'L{i}' for i in range(1, line_count + 1)], 'browser physical line anchors differ')
                    token_counts = Counter(code.locator('.source-line > span').evaluate_all('(nodes) => nodes.map(n => n.className)'))
                    require(token_counts and token_counts.keys() <= {'ada-' + t for t in TOKEN_CLASSES}, 'browser highlight classes differ')
                    counts.update(token_counts)
                    selected = selected_source(code)
                    require(selected in (text, text.removesuffix('\n')), 'source selection contains non-source text: ' + path)
                    paired = [d for d in record['declarations'] if 'body_line' in d]
                    observed = page.locator('nav a').evaluate_all('(nodes) => nodes.map(a => a.getAttribute("href"))')
                    expected = []
                    if paired:
                        local, remote = ('body_line', 'line') if reverse else ('line', 'body_line')
                        other = Path(record['specification'] if reverse else record['body']).name + '.html'
                        expected = [f'{other}#L{paired[0][remote]}']
                        for item in sorted(paired[1:], key=lambda d: d[local]):
                            expected.extend([f'#{"L"}{item[local]}', f'{other}#L{item[remote]}'])
                    require(observed == expected, 'browser correspondence inventory differs: ' + path)
                    if reverse:
                        nav = page.locator('nav > p > a')
                        activate(nav)
                        arrive(page, base, source_path(record['specification'], package_heading['line']))
                    if path in {submit[0]['body'], submit[0]['specification']}:
                        page.goto(base + 'api/source/' + path + '.html')
                        selection = selected_source(page.locator('pre.source-code > code'))
                        page.bring_to_front()
                        page.keyboard.press('Control+c')
                        page.wait_for_function('async text => await navigator.clipboard.readText() === text', arg=selection)
                        output_data['clipboard'].append(dict(path=path, characters=len(selection)))
                    output_data['inventory'].append(dict(path=path, lines=line_count, correspondence_links=len(expected),
                                                         selection=True, highlights=sum(token_counts.values())))
            output_data['highlight_classes'] = dict(counts)
            for record, entry in [common_proc, rf, *((command, d) for d in waits)]:
                page.goto(base + record['native'] + '#' + entry['anchor'])
                activate(link(page, record, entry))
                arrive(page, base, source_path(record['specification'], entry['line']))
                result = dict(name=entry['name'], anchor=entry['anchor'], specification=entry['line'],
                              body=entry.get('body_line'), omission=entry.get('body_omission'))
                if 'body_line' in entry:
                    page.locator('nav details > summary').click()
                    row = page.locator('nav li').filter(has=page.locator(f'a[href="#L{entry["line"]}"]'))
                    activate(row.locator('a').nth(1))
                    arrive(page, base, source_path(record['body'], entry['body_line']))
                    page.locator('nav details > summary').click()
                    row = page.locator('nav li').filter(has=page.locator(f'a[href="#L{entry["body_line"]}"]'))
                    activate(row.locator('a').nth(1))
                    arrive(page, base, source_path(record['specification'], entry['line']))
                    result['bidirectional'] = True
                output_data['journeys'].append(result)
            ctx.close()

            record, entry = submit
            spec_target = source_path(record['specification'], entry['line'])
            body_target = source_path(record['body'], entry['body_line'])
            for javascript in (True, False):
                for width in (1280, 375):
                    for scheme in ('light', 'dark'):
                        ctx = context(java_script_enabled=javascript, viewport=dict(width=width, height=850), color_scheme=scheme)
                        page = ctx.new_page()
                        page.goto(base + 'api/index.html')
                        expect(page.locator('iframe[name="document-content"]')).to_have_count(1)
                        if not javascript:
                            expect(page.locator('#package-search')).to_be_hidden()
                        page.goto(base + record['native'] + '#' + entry['anchor'])
                        activate(link(page, record, entry, body=True))
                        arrive(page, base, body_target)
                        expect(page.locator('#L' + str(entry['body_line']) + ' .ada-keyword').first).to_have_css('font-weight', '700')
                        expect(page.locator('body')).to_have_css('background-color', 'rgb(20, 27, 38)' if scheme == 'dark' else 'rgb(255, 255, 255)')
                        page.reload()
                        arrive(page, base, body_target)
                        page.locator('nav details > summary').click()
                        row = page.locator('nav li').filter(has=page.locator(f'a[href="#L{entry["body_line"]}"]'))
                        activate(row.locator('a').nth(1))
                        arrive(page, base, spec_target)
                        page.go_back()
                        arrive(page, base, body_target)
                        page.go_forward()
                        arrive(page, base, spec_target)
                        output_data['display'].append(dict(javascript=javascript, width=width, scheme=scheme))
                        ctx.close()
            chromium.close()
        require(not output_data['errors'] and not output_data['external_requests'], 'browser runtime error or external request')
        output_data['passed'] = True
    finally:
        server.shutdown()
        server.server_close()
        output.write_text(json.dumps(output_data, indent=2, sort_keys=True) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ('root', 'site', 'evidence', 'output'):
        parser.add_argument('--' + option, type=Path, required=True)
    args = parser.parse_args()
    run(args.root, args.site, json.loads(args.evidence.read_bytes()), args.output)