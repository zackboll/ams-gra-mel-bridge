#!/usr/bin/env python3
"""Read-only, fail-closed HTTP acceptance against source-qualified MEL publication.

Commit identity is supplied by deployment metadata, never inferred from HTML.
The exact committed consumer and immutable installed enhancer qualify local
inputs before any HTTP request. Presentation manifests never supply authority.
"""

import argparse
import gzip
import hashlib
import http.client
import io
import json
import posixpath
import re
import sys
import time
import urllib.error
import urllib.request
import zlib
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import quote, unquote, urljoin, urlsplit

# Even interpreter cache creation must not write into the checked source tree.
sys.dont_write_bytecode = True
import build_enhanced_site as publication
from example_sources import References, inventory, occurrence_ledger, source, verify_rendered
import api_catalog


class LiveError(ValueError):
    """An acceptance guard, not an assertion disabled by optimized Python."""


def require(value, message):
    if not value:
        raise LiveError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def safe_path(path, parent=False):
    require(isinstance(path, str) and path != "", "empty URL path")
    require(not re.search(r"[\x00-\x20\x7f\\?#]", path), "unsafe URL path characters")
    # Artifact names and URL spellings are canonical, not encoded aliases.
    # Reject even double-encoded traversal rather than normalize it away.
    require("%" not in path and not path.startswith("/"), "encoded/absolute URL path")
    require(all(p not in {"", "."} and (parent or p != "..") for p in path.split("/")),
            "URL path traversal or noncanonical path")
    return path


def base_url(value, loopback=False):
    url = urlsplit(value)
    require(not re.search(r"[\x00-\x20\x7f\\%]", value), "unsafe base URL")
    require("?" not in value and "#" not in value and not url.username and not url.password,
            "base URL credentials/query/fragment forbidden")
    if loopback:
        require(url.scheme == "http" and url.hostname == "127.0.0.1", "fixture origin must be loopback")
    else:
        require(url.scheme == "https" and re.fullmatch(r"[a-z0-9-]+\.github\.io", url.netloc or ""),
                "expected HTTPS GitHub Pages origin")
    require(re.fullmatch(r"/[A-Za-z0-9_.-]+/", url.path) and url.path not in {"/../", "/./"},
            "expected canonical repository prefix with trailing slash")
    return value


def file_url(base, name):
    safe_path(name)
    destination = base + quote(name, safe="/-._~")
    require(urlsplit(destination).netloc == urlsplit(base).netloc
            and urlsplit(destination).path.startswith(urlsplit(base).path), "URL escaped verified site")
    return destination


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


MIMES = {".html": {"text/html"}, ".css": {"text/css"},
         ".js": {"text/javascript", "application/javascript"}, ".json": {"application/json"}}


def get_entity(url, base, expected, opener=None):
    """GET once; only the documented repository-root slash redirect is allowed."""
    opener = opener or urllib.request.build_opener(urllib.request.ProxyHandler({}), NoRedirect())
    record = {"requested_url": url, "redirect_chain": [], "comparison": False}
    try:
        for unused in range(2):
            request = urllib.request.Request(url, headers={
                "Accept-Encoding": "gzip, identity", "User-Agent": "MEL-live-publication-verifier/1",
            }, method="GET")
            try:
                response = opener.open(request, timeout=30)
            except urllib.error.HTTPError as error:
                response = error
            with response:
                record.update(status=response.status, final_url=response.geturl())
                require(response.geturl() == url, "transport followed an unintended redirect")
                for key in ("Content-Type", "Content-Length", "Content-Encoding", "Location", "Transfer-Encoding"):
                    require(len(response.headers.get_all(key, [])) <= 1, "duplicate response header: " + key)
                require(not (response.headers.get("Content-Length") and response.headers.get("Transfer-Encoding")),
                        "ambiguous response framing")
                if response.status in {301, 302, 303, 307, 308}:
                    location = response.headers.get("Location", "")
                    destination = urljoin(url, location)
                    record["redirect_chain"].append({"status": response.status, "location": location,
                                                     "destination": destination})
                    require(response.status in {301, 308} and url == base[:-1] and destination == base
                            and location in {base, urlsplit(base).path},
                            "unintended redirect outside canonical root normalization")
                    url = destination
                    continue
                require(response.status == 200, "HTTP status must be 200")
                require(not response.headers.get("Location"), "unexpected response Location")
                content_type = response.headers.get("Content-Type", "")
                record["content_type"] = content_type
                suffix = Path(urlsplit(url).path).suffix or ".html"
                require(suffix in MIMES and content_type.split(";", 1)[0].strip().lower() in MIMES[suffix],
                        "incorrect MIME type")
                parameters = content_type.split(";")[1:]
                require(all(re.fullmatch(r'\s*charset\s*=\s*"?utf-8"?\s*', p, re.I) for p in parameters)
                        and len(parameters) <= 1, "incorrect response encoding/content-type parameters")
                wire = response.read(max(len(expected) * 2, 65536) + 1)
                length = response.headers.get("Content-Length")
                require(length is None or (re.fullmatch(r"[0-9]+", length) and int(length) == len(wire)),
                        "malformed/truncated response length")
                encoding = response.headers.get("Content-Encoding", "identity").lower()
                record["content_encoding"] = encoding
                require(encoding in {"identity", "gzip"}, "unsupported transfer content encoding")
                if encoding == "gzip":
                    with gzip.GzipFile(fileobj=io.BytesIO(wire)) as stream:
                        body = stream.read(len(expected) + 1)
                else:
                    body = wire
                record.update(byte_length=len(body), sha256=digest(body))
                require(body == expected, "published entity bytes differ from qualified artifact (stale or changed)")
                record["comparison"] = True
                return record, body
        raise LiveError("redirect limit exceeded")
    except (ValueError, OSError, EOFError, zlib.error, http.client.HTTPException, urllib.error.URLError) as error:
        record["error"] = str(error)
        return record, None


class GraphPage(HTMLParser):
    def __init__(self, text, name):
        super().__init__(convert_charrefs=True)
        self.ids, self.references = set(), []
        self.name = name
        self.feed(text)
        self.close()

    def handle_starttag(self, tag, attributes):
        attrs = dict(attributes)
        require(len(attrs) == len(attributes), "duplicate HTML attribute")
        if tag == "script":
            require(self.name == "api/index.html" and attrs == {"src": "gnatdoc-navigation.js", "defer": None},
                    "unapproved active content")
        else:
            require(tag not in {"style", "base", "object", "embed", "applet", "form", "svg", "math"},
                    "unapproved active content")
        for key, value in attributes:
            require(not key.startswith("on") and key not in {"srcdoc", "style", "http-equiv", "srcset", "action",
                                                            "formaction", "poster", "data", "background"},
                    "unapproved active/resource attribute")
            if key == "id":
                require(value and value not in self.ids, "duplicate/empty fragment target")
                self.ids.add(value)
            if key in {"href", "src"}:
                self.references.append(value)

    handle_startendtag = handle_starttag


def graph(files):
    """Inspect served bytes, not manifest counts or data-type claims."""
    pages = {name: GraphPage(data.decode("utf-8"), name) for name, data in files.items() if name.endswith(".html")}
    records = []
    for name, page in pages.items():
        for reference in page.references:
            require(isinstance(reference, str) and reference, "empty internal reference")
            parsed = urlsplit(reference)
            require(not parsed.scheme and not parsed.netloc and not parsed.query and "?" not in reference,
                    "unsupported scheme/origin/query in internal link")
            if parsed.path:
                safe_path(parsed.path, parent=True)
            target = posixpath.normpath(posixpath.join(posixpath.dirname(name), parsed.path)) if parsed.path else name
            require(target != ".." and not target.startswith("../"), "internal link directory escape")
            require(target in files, "internal link destination missing")
            fragment = unquote(parsed.fragment)
            require(not parsed.fragment or (target in pages and fragment in pages[target].ids),
                    "internal link fragment missing")
            records.append({"page": name, "reference": reference, "target": target, "fragment": fragment})
    return records


def qualified(site, pristine):
    report = publication.validate(site, pristine)
    files = publication.files(site)
    require(report["output_sha256"] == publication.hashes(files), "artifact changed during qualification")
    for name in files:
        safe_path(name)
        require(Path(name).suffix in MIMES, "unexpected asset kind")
    directories = {p.as_posix() for name in files for p in Path(name).parents if p.as_posix() != "."}
    require(all(p.relative_to(site).as_posix() in directories for p in Path(site).rglob("*") if p.is_dir()),
            "unexpected artifact directory")
    return files, report


def verify(site, pristine, base, commit, attempts=1, interval=15, loopback=False):
    base_url(base, loopback)
    require(re.fullmatch(r"[0-9a-f]{40}", commit), "expected deployed commit must be explicit full Git SHA")
    require(1 <= attempts <= 20 and 0 <= interval <= 60, "unbounded readiness policy")
    files, authority = qualified(site, pristine)
    catalog = api_catalog.verify(files["guides/api-map.html"].decode("utf-8"),
                                 publication.ROOT, References(publication.ROOT, Path(pristine)))
    expected_graph = graph(files)
    evidence = {"schema_version": 1, "expected_deployed_commit": commit,
                "commit_authority": "caller-supplied deployment workflow/metadata, not HTTP content",
                "base_url": base, "authority": authority, "publication_sha256": publication.hashes(files),
                "qualified_api_catalog": catalog,
                "publication_fingerprint": publication.fingerprint(publication.hashes(files)),
                "attempts": [], "passed": False}
    for attempt in range(attempts):
        observation = {"number": attempt + 1, "observed_at_unix": time.time(), "files": {}}
        served = {}
        for name, expected in files.items():
            record, body = get_entity(file_url(base, name), base, expected)
            observation["files"][name] = record
            if body is not None:
                served[name] = body
        root, body = get_entity(base[:-1], base, files["index.html"])
        if body is not None and root["final_url"] != base:
            root.update(comparison=False, error="repository root did not normalize to canonical trailing slash")
            body = None
        observation["root_normalization"] = root
        evidence["attempts"].append(observation)
        if len(served) == len(files) and body is not None:
            require(graph(served) == expected_graph, "served link graph differs from qualified graph")
            evidence["internal_references"] = expected_graph
            native = publication.Authority(publication.ROOT, Path(pristine))
            additions = native.required_additions()
            audit = []
            for name, data in served.items():
                if name.startswith("api/") and name.endswith(".html"):
                    _, records = publication.restore_page(Path(name).name, data.decode("utf-8"), native)
                    audit.extend(records)
            publication.require_complete_additions(audit, additions)
            added_keys = {publication.occurrence_key(r) for r in additions}
            original = [r for r in audit if publication.occurrence_key(r) not in added_keys]
            require(len(original) == 1418 and publication.fingerprint(original) == publication.ORIGINAL_AUDIT_SHA256,
                    "served original ordered type destinations changed")
            require(len(audit) == 1428, "served type destination count changed")
            evidence["verified_local_type_destinations"] = audit
            symbols = publication.symbol_audit.inventory(native, Path(pristine))
            publication.restore_navigation(served["api/index.html"].decode("utf-8"), symbols)
            require(symbols == authority["public_symbols"], "served symbol authority changed")
            evidence["verified_symbol_destinations"] = symbols
            evidence["ten_required_additions"] = additions
            evidence["original_1418_audit_sha256"] = publication.fingerprint(original)
            references = References(publication.ROOT, Path(pristine))
            evidence["api_catalog"] = api_catalog.verify(
                served["guides/api-map.html"].decode("utf-8"), publication.ROOT, references)
            evidence["canonical_source_listings"] = verify_rendered(
                served["guides/examples.html"].decode("utf-8"), references=references)
            evidence["example_inline_destinations"] = occurrence_ledger(references=references)
            entries = inventory()
            evidence["example_programs"] = len(entries)
            evidence["example_execution_cases"] = sum(len(e["cases"]) for e in entries)
            evidence["example_source_sha256"] = {name: digest(source(name).encode("utf-8"))
                                                  for e in entries for name in [e["main"], *e["support"]]}
            evidence["passed"] = True
            break
        if attempt + 1 < attempts:
            time.sleep(interval)
    # Input trees are read-only; detect concurrent replacement of qualified bytes.
    require(publication.files(site) == files, "artifact changed during HTTP verification")
    return evidence


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact", required=True, type=Path)
    parser.add_argument("--pristine", required=True, type=Path)
    parser.add_argument("--base-url", required=True)
    parser.add_argument("--expected-commit", required=True)
    parser.add_argument("--attempts", type=int, default=1)
    parser.add_argument("--interval", type=int, default=15)
    parser.add_argument("--evidence", type=Path, help="fresh external JSON file; existing trusted parent required")
    args = parser.parse_args()
    destination = publication.audit_destination(args.evidence, publication.ROOT, args.pristine, args.artifact) if args.evidence else None
    try:
        result = verify(args.artifact, args.pristine, args.base_url, args.expected_commit, args.attempts, args.interval)
    except (ValueError, OSError) as error:
        result = {"passed": False, "error": str(error)}
    if destination:
        with destination.open("x", encoding="utf-8") as stream:
            json.dump(result, stream, indent=2, sort_keys=True)
            stream.write("\n")
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
