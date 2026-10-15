#!/usr/bin/env python3
"""Independently check native site structure, local resources and fragments."""

import argparse
import collections
import re
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlsplit


class SiteError(ValueError):
    pass


class Page(HTMLParser):
    def __init__(self, text):
        super().__init__(convert_charrefs=True)
        self.ids, self.links, self.issues = [], [], []
        self.feed(text)
        self.close()

    def handle_starttag(self, tag, attrs):
        if tag in {"script", "base", "object", "embed", "applet", "form"}:
            self.issues.append(f"executable/unapproved element: {tag}")
        for key, value in attrs:
            if key.lower().startswith("on") or key in {"srcdoc", "style"}:
                self.issues.append(f"active attribute: {key}")
            if key == "http-equiv":
                self.issues.append("meta redirect")
            if key == "id":
                self.ids.append(value)
            if key in {"src", "href"}:
                self.links.append(value)


def validate(site):
    site = site.resolve()
    pages = {}
    allowed = {"index.html", "api/index.html", "api/gnatdoc.css"}
    for path in site.rglob("*"):
        if path.is_symlink():
            raise SiteError("symlink in published output")
        if not path.is_file():
            continue
        relative = path.relative_to(site).as_posix()
        if relative not in allowed and not re.fullmatch(r"api/[a-f0-9]{64}\.html", relative):
            raise SiteError(f"unexpected asset: {relative}")
        text = path.read_text()
        if re.search(r"/(?:home|tmp|Users)/|file:/{2}|[A-Z]:\\", text):
            raise SiteError(f"machine-local path: {relative}")
        if path.suffix == ".css" and re.search(r"@import|url\s*\(", text, re.I):
            raise SiteError("unapproved CSS resource")
        if path.suffix == ".html":
            page = Page(text)
            if page.issues or len(page.ids) != len(set(page.ids)):
                raise SiteError(f"unsafe structure or duplicate IDs: {relative}: {page.issues}")
            pages[path] = page
    if not all((site / name).is_file() for name in allowed):
        raise SiteError("missing required landing/index/stylesheet")
    index = (site / "api/index.html").read_text()
    packages = {}
    for path in pages:
        if path.parent == site / "api" and path.name != "index.html":
            names = re.findall(r"<h1>([^<]+)</h1>", path.read_text())
            if len(names) != 1:
                raise SiteError("invalid native package heading")
            packages[path.name] = names[0]
    navigation = re.findall(r"<a href=([^ >]+) target=document-content>([^<]+)</a>",
                            index.split('<h2 id=tagged-types>', 1)[0])
    if collections.Counter(navigation) != collections.Counter(packages.items()):
        raise SiteError("native navigation missing, duplicated or swapped package destinations")
    if not index.rstrip().endswith('<div class=document-content><iframe name=document-content></iframe></div>'):
        raise SiteError("malformed native navigation frame")
    for path, page in pages.items():
        for link in page.links:
            if not link:
                raise SiteError("empty URL")
            url = urlsplit(link)
            decoded = unquote(url.path)
            if url.scheme or url.netloc or url.query or "\\" in decoded or decoded.startswith("/"):
                raise SiteError(f"unsafe/external URL: {link}")
            if ".." in Path(decoded).parts:
                raise SiteError(f"path traversal: {link}")
            target = (path.parent / decoded).resolve() if decoded else path
            if not target.is_relative_to(site) or not target.is_file():
                raise SiteError(f"broken internal link: {path.name}: {link}")
            if url.fragment and (target not in pages or unquote(url.fragment) not in pages[target].ids):
                raise SiteError(f"broken fragment: {path.name}: {link}")
    return {"html_pages": len(pages), "links": sum(len(p.links) for p in pages.values())}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("site", type=Path)
    args = parser.parse_args()
    print(validate(args.site))
