#!/usr/bin/env python3
"""Independently check native site structure, local resources and fragments."""

import argparse
import collections
import re
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlsplit

from build_docs_portal import ROOT, PortalError, manifest, package_map, regular


class SiteError(ValueError):
    pass


class Page(HTMLParser):
    def __init__(self, text):
        super().__init__(convert_charrefs=True)
        self.ids, self.links, self.issues, self.package_entries = [], [], [], []
        self.feed(text)
        self.close()

    def handle_starttag(self, tag, attrs):
        if tag in {"script", "style", "base", "object", "embed", "applet", "form", "svg", "math"}:
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
            if key in {"srcset", "action", "formaction", "poster", "data", "background"}:
                self.issues.append(f"unapproved resource attribute: {key}")
        if "data-package-entry" in dict(attrs):
            self.package_entries.append((dict(attrs)["data-package-entry"], dict(attrs).get("href")))

    handle_startendtag = handle_starttag


def validate(site):
    if site.is_symlink():
        raise SiteError("symlink site root")
    site = site.resolve()
    pages = {}
    allowed = {"index.html", "api/index.html", "api/gnatdoc.css"}
    try:
        data = manifest()
        packages_verified = package_map(site)
        for name in (data["home"], data["stylesheet"], *(e["path"] for e in data["guides"])):
            regular(ROOT / "docs" / name)
    except (PortalError, ValueError, OSError) as exc:
        raise SiteError(str(exc)) from exc
    allowed.update(e["path"] for e in data["guides"])
    allowed.add(data["stylesheet"])
    for path in site.rglob("*"):
        if path.is_symlink():
            raise SiteError("symlink in published output")
        if not path.is_file():
            continue
        relative = path.relative_to(site).as_posix()
        if relative not in allowed and not re.fullmatch(r"api/[a-f0-9]{64}\.html", relative):
            raise SiteError(f"unexpected asset: {relative}")
        text = path.read_text()
        if "{{" in text or "}}" in text:
            raise SiteError("unresolved placeholder")
        if re.search(r"/(?:home|tmp|Users)/|file:/{2}|[A-Z]:\\", text):
            raise SiteError(f"machine-local path: {relative}")
        if path.suffix == ".css":
            css = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
            if "\\" in css or re.search(r"@import|url\s*\(|expression\s*\(|-moz-binding|behavior\s*:", css, re.I):
                raise SiteError("unapproved CSS resource or escape")
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
            # The only parent-relative links authorized are canonical guide
            # links to fixed root pages/assets or verified native destinations.
            guide_targets = {"index.html", "api/index.html", data["stylesheet"],
                             *packages_verified.values()}
            parent_link = (path.parent == site / "guides" and decoded.startswith("../")
                           and decoded[3:] in guide_targets)
            if ".." in Path(decoded).parts and not parent_link:
                raise SiteError(f"path traversal: {link}")
            target = (path.parent / decoded).resolve() if decoded else path
            if not target.is_relative_to(site) or not target.is_file():
                raise SiteError(f"broken internal link: {path.name}: {link}")
            if url.fragment and (target not in pages or unquote(url.fragment) not in pages[target].ids):
                raise SiteError(f"broken fragment: {path.name}: {link}")
    home = pages[site / "index.html"]
    for entry in data["guides"]:
        if home.links.count(entry["path"]) != 1:
            raise SiteError("home guide navigation missing or duplicated")
        guide = pages[site / entry["path"]]
        if "../index.html" not in guide.links or "../api/index.html" not in guide.links:
            raise SiteError("guide missing Home/API navigation")
    for name in ("index.html", "guides/api-map.html"):
        prefix = "../" if name.startswith("guides/") else ""
        expected = collections.Counter((package, prefix + destination)
                                       for package, destination in packages_verified.items())
        if collections.Counter(pages[site / name].package_entries) != expected:
            raise SiteError("package directory missing, duplicated or swapped destinations")
    # Compare every portal package link, not just the directory entries, against
    # deterministic resolution of its authored package name.
    from build_docs_portal import render
    from example_sources import verify_rendered
    try:
        verify_rendered((site / "guides/examples.html").read_text())
    except ValueError as exc:
        raise SiteError(str(exc)) from exc
    for name in ("index.html", *(e["path"] for e in data["guides"])):
        expected_text = render(regular(ROOT / "docs" / name), name, data, packages_verified)
        authored = Page(expected_text)
        def api_links(page):
            return collections.Counter(link for link in page.links
                                       if re.fullmatch(r"(?:\.\./)?api/[a-f0-9]{64}\.html", link))
        if api_links(pages[site / name]) != api_links(authored):
            raise SiteError("portal package links differ from verified authored references")
        if (site / name).read_text() != expected_text:
            raise SiteError("portal output differs from deterministic authored page")
    if (site / data["stylesheet"]).read_text() != regular(ROOT / "docs" / data["stylesheet"]):
        raise SiteError("portal stylesheet differs from authorized source")
    return {"html_pages": len(pages), "links": sum(len(p.links) for p in pages.values())}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("site", type=Path)
    args = parser.parse_args()
    print(validate(args.site))
