"""Bounded public type bookmarks, independently owned by the MEL consumer.

Source visibility and declaration profiles, pristine native sections and the
approved anchor ledger confer authority. Enhancer inventories and publication
metadata never confer it. This is not general Ada semantic resolution.
"""

import html
import re

from check_doc_source_integrity import native_profile
from gnatdoc_coverage_model import specification
from local_type_audit import lex


def require(condition, message):
    if not condition:
        raise ValueError("independent symbol audit: " + message)


def inventory(authority, pristine):
    """Return source-spelled symbols in package/source declaration order."""
    result = []
    # One immutable public-token projection per source file in this call only.
    # Do not retain this on Authority: callers may change source/native inputs
    # between inventories, including when reusing an existing Authority object.
    public_sources, native_pages = {}, {}
    for key, entry in sorted(authority.types.items()):
        owner = authority.packages[entry["package"]]
        path = authority.root / entry["source"]
        if path not in public_sources:
            _, lines, header, boundary, _, _ = specification(path)
            public_sources[path] = tuple(t[0] for t in lex("\n".join(lines[header:boundary - 1])))
        public = public_sources[path]
        # Unsupported scopes cannot promote nested/private declarations. Public
        # limited-private views remain public; private completions do not.
        if set(public) & {"package", "generic", "task", "protected", "begin", "declare"}:
            continue
        require("internal" not in entry["package"].split("."), "private owner")
        declaration = tuple(t[0] for t in lex(entry["declaration"]))
        require(declaration[0] in {"type", "subtype"}, "unsupported declaration kind")
        require(any(public[i:i + len(declaration)] == declaration for i in range(len(public))),
                "declaration lacks complete public source identity")
        source = [t.lower() for t in native_profile(entry["declaration"])]
        native = [t.lower() for t in native_profile(entry["native"])]
        reconciliation = "complete-token-equivalence"
        if "…" in native:
            require(native.count("…") == 1 and source[:1] == ["type"], "unsupported abbreviation")
            i = native.index("…")
            suffix = native[i + 1:]
            require(bool(suffix) and source[:i] == native[:i] and source[-len(suffix):] == suffix,
                    "abbreviated native profile differs from source")
            path = pristine / "api" / entry["page"]
            if path not in native_pages:
                native_pages[path] = path.read_text()
            text = native_pages[path]
            marker = "<h4 id=" + entry["anchor"] + ">"
            require(text.count(marker) == 1, "missing/duplicate native anchor")
            section = text.split(marker, 1)[1].split("<h4 ", 1)[0]
            literals = [t for t in source[source.index("(") + 1:source.index(")")] if t != ","]
            rendered = [html.unescape(t).strip().lower()
                        for t in re.findall(r"<dt>([^<]+)<dd>", section)]
            require(literals == rendered, "incomplete/changed ordered native enumeration literals")
            reconciliation = "enumeration-prefix-suffix-and-complete-ordered-literal-list"
        else:
            require(source == native, "native profile differs from public source")
        require(authority.page_packages[entry["page"]] == owner["name"], "wrong native owner")
        ledger = authority.ledger.get(entry["identity"], {})
        require((ledger.get("page"), ledger.get("anchor")) == (entry["page"], entry["anchor"]),
                "destination differs from approved native anchor ledger")
        canonical = owner["name"] + "." + entry["name"]
        require(canonical.lower() == key, "wrong canonical source identity")
        require(re.fullmatch(r"[a-f0-9]{64}\.html", entry["page"]) is not None
                and re.fullmatch(r"[a-f0-9]{64}", entry["anchor"]) is not None,
                "unsafe native destination")
        result.append(dict(canonical=canonical, owner=owner["name"], kind=declaration[0],
                           visibility="public", source=entry["source"], line=entry["line"],
                           declaration=entry["declaration"], native_profile=entry["native"],
                           page=entry["page"], anchor=entry["anchor"], ledger_identity=entry["identity"],
                           reconciliation=reconciliation))
    require(len({e["canonical"].lower() for e in result}) == len(result), "duplicate symbol identity")
    require(len({(e["page"], e["anchor"]) for e in result}) == len(result), "duplicate symbol destination")
    require(authority.packages.get("ams.mel", {}).get("name") == "AMS.MEL"
            and "AMS.MEL" in authority.page_packages.values(), "default lacks public source/native owner")
    return sorted(result, key=lambda e: (e["owner"].lower(), e["line"]))


def metadata(symbols):
    return ('<div id="type-navigation" hidden data-default-package="AMS.MEL">' +
            ''.join('<a href="' + e["page"] + '#' + e["anchor"] + '" data-symbol="' +
                    html.escape(e["canonical"], quote=True) + '"></a>' for e in symbols) + '</div>')


def restore(text, symbols):
    """Remove only the exact, unique, source-ordered, correctly placed metadata."""
    insertion = metadata(symbols)
    require(text.count(insertion) == 1 and
            text.count(insertion + '<h2 id=compilation-units>') == 1,
            "missing/duplicate/altered/misplaced symbol metadata")
    return text.replace(insertion, "", 1)
