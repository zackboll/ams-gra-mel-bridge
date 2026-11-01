#!/usr/bin/env python3
"""Offline, fail-closed IR declaration and binding-evidence audit (stdlib only)."""

import argparse
import base64
from collections import Counter
import hashlib
import json
from functools import lru_cache
from pathlib import Path
import re
import sys
import zlib

ROOT = Path(__file__).resolve().parents[1]
REFERENCE = "docs/reference/irmel-upstream-api.json"
COVERAGE = "docs/irmel-ada-coverage.json"
EVIDENCE = "docs/reference/irmel-upstream-git-evidence.json"
REPORT = "docs/irmel-ada-coverage.md"
REVIEW = "docs/reference/irmel-reviewed-bindings.json"
# Independently reviewed baseline anchors; changing these requires a new audit.
CATALOG_SHA256 = "00f29cb4c523d251f5ca78f829bc2cac7b723b0144754f9bcfdd9a922caab9ac"
REVIEW_SHA256 = "234f2b73ec34143d45c1dc153e96a8efc3f16fd737dbdf7b1dba0525afc5f3bb"
PINS = {
    "ir": ("https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-ir-mel",
           "8d9224519f12b44e0b28815755c56a32a28d24a0",
           "bfc72a6a2e1929db9a90c0af78328085fb0ae3ec"),
    "common": ("https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-common-mel",
               "f6908437d8fd2f7fb69896f9eb9cfd272d10c439",
               "f1f146d79f03a2ca0a59412aaf3d2c6d7dfba40f"),
}
STATUSES = {"safe_ada", "native_only", "partial", "missing", "provider_only"}


class Invalid(ValueError):
    """An audit contract failed."""


def require(condition, message):
    if not condition:
        raise Invalid(message)


def load(path):
    def pairs(items):
        result = {}
        for key, value in items:
            require(key not in result, "duplicate JSON key: " + key)
            result[key] = value
        return result
    return json.loads(path.read_text(), object_pairs_hook=pairs)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def git_hash(kind, data):
    return hashlib.sha1(kind.encode() + b" " + str(len(data)).encode() + b"\0" + data).hexdigest()


def normalized(text):
    return re.sub(r"\s+", " ", text).strip()


@lru_cache(maxsize=256)
def uncomment(text):
    # Retain offsets/newlines for source references. Strings precede comments.
    pattern = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*.*?\*/'
    return re.sub(pattern, lambda m: re.sub(r"[^\n]", " ", m[0])
                  if m[0].startswith(("//", "/*")) else m[0], text, flags=re.S)


def matching(text, start, opening="{", closing="}"):
    depth = 0
    quoted = False
    escape = False
    for index in range(start, len(text)):
        char = text[index]
        if char == '"' and not escape:
            quoted = not quoted
        if not quoted:
            depth += char == opening
            depth -= char == closing
            if depth == 0:
                return index
        escape = char == "\\" and not escape
    raise Invalid("unbalanced declaration")


def declarations(repo, path, blob, source):
    """Lexical extraction for the frozen dialect; not a general C++ parser.

    All type bodies and their public/private members are retained, including
    inline getter/setter signatures. Virtual declarations are independently
    counted. See the schema document for the reviewed dialect exceptions.
    """
    text = uncomment(source)
    types, operations = [], []
    spans = []
    namespace_spans = []
    for ns in re.finditer(r"\bnamespace\s+([\w:]+)\s*\{", text):
        namespace_spans.append((ns.end() - 1, matching(text, ns.end() - 1), ns[1]))

    def namespace_at(position):
        return "::".join(n[2] for n in namespace_spans if n[0] < position < n[1])
    pattern = r"\b(enum\s+class|enum|class|struct)\s+(\w+)\s*([^;{}]*?)\{"
    for match in re.finditer(pattern, text):
        # Template type parameters are not declarations with bodies.
        if "<" in match[3] or ">" in match[3]:
            continue
        start = match.end() - 1
        end = matching(text, start)
        parents = [s for s in spans if s[0] < start < s[1]]
        owner = parents[-1][2] + "::" if parents else ""
        namespace = namespace_at(match.start())
        namespace = namespace + "::" if namespace else ""
        symbol = (parents[-1][3] + "::" if parents else namespace) + match[2]
        spans.append((start, end, owner + match[2], symbol))
        body = text[start + 1:end]
        members = []
        member_positions = []
        # Capture depth-zero statements and inline method headers, not bodies.
        pos = 0
        cursor = 0
        access = "private" if match[1] == "class" else "public"
        while cursor < len(body):
            access_match = re.match(r"\s*(public|private|protected)\s*:", body[pos:])
            if access_match:
                access = access_match[1]
                pos += access_match.end()
                cursor = pos
                continue
            char = body[cursor]
            if char in ";{":
                head = normalized(body[pos:cursor])
                if head:
                    members.append({"access": access, "declaration": head})
                    member_positions.append(start + 1 + pos + len(body[pos:cursor]) - len(body[pos:cursor].lstrip()))
                if char == "{":
                    cursor = matching(body, cursor)
                pos = cursor + 1
            cursor += 1
        if normalized(body[pos:]):
            members.append({"access": access, "declaration": normalized(body[pos:])})
            member_positions.append(start + 1 + pos)
        record = {"id": repo + ":type:" + symbol, "symbol": symbol,
                  "kind": match[1], "repository": repo, "path": path,
                  "blob": blob, "line": source.count("\n", 0, match.start()) + 1,
                  "base_declaration": normalized(match[3]), "members": members,
                  "body_sha256": sha(body.encode())}
        types.append(record)
        for member, raw_position in zip(members, member_positions):
            head = member["declaration"]
            if member["access"] != "public":
                continue
            is_virtual = "virtual " in head or " override" in head
            helper = "/factory/" in path or "/utils/" in path
            if (not is_virtual and not helper) or "(" not in head or "= delete" in head or head.startswith("decltype("):
                continue
            # Nested class text is handled independently, not as a method.
            if head.startswith(("class ", "struct ", "enum ")):
                continue
            name_match = re.search(r"(~?\w+)\s*\(", head)
            require(name_match is not None, "unrecognized virtual: " + head)
            name = name_match[1]
            if not is_virtual and name in {match[2], "~" + match[2]}:
                continue
            opening = head.index("(", name_match.start())
            closing = matching(head, opening, "(", ")")
            parameters = head[opening + 1:closing]
            # Drop identifiers, not types; inner callback Channel& channel too.
            parameters = re.sub(r'\s*=\s*""', "", parameters)
            parameters = re.sub(r"(?<=[&*>])\s*\b(?!const\b)[a-z_]\w*\b", "", parameters)
            parameters = re.sub(r"\b([A-Za-z_]\w*(?:::\w+)*)\s+(?:const\s+)?([a-z_]\w*)(?=\s*[,)]|\s*$)", r"\1", parameters)
            parameters = re.sub(r"\s*([<>,&*])\s*", r"\1", parameters).strip()
            identity = name + "(" + parameters + ")"
            suffix = head[closing + 1:]
            if re.search(r"\bconst\b", suffix):
                identity += " const"
            category = ("destructor" if name.startswith("~") else
                        "outgoing" if name in {"send", "exec", "sendKeepAliveRep"} else
                        "metadata" if name == "registerMetadataCallback" else
                        "image_delivery" if name in {"onImage", "onImageStack"} else
                        "buffer_lifecycle" if match[2] == "Buffer" or "Buffer" in name else
                        "provider_infrastructure" if helper else "lifecycle")
            # Requirement tags belong to the immediately preceding doc block.
            before = source[start:raw_position]
            block = before[before.rfind(";") + 1:]
            tags = list(dict.fromkeys(re.findall(r"@(Required\w*|Optional)\b", block)))
            operations.append({"id": repo + ":operation:" + symbol + "::" + identity,
                               "symbol": symbol + "::" + name, "interface": match[2],
                               "overload": identity, "signature": head,
                               "repository": repo, "path": path, "blob": blob,
                               "category": category, "requirements": tags or ["unspecified"]})
    # Aliases are declarations too (forward class declarations are not definitions).
    for match in re.finditer(r"\busing\s+(\w+)\s*=\s*([^;]+);", text):
        parents = [s for s in spans if s[0] < match.start() < s[1]]
        namespace = namespace_at(match.start())
        namespace = namespace + "::" if namespace else ""
        symbol = (parents[-1][3] + "::" if parents else namespace) + match[1]
        types.append({"id": repo + ":type:" + symbol, "symbol": symbol, "kind": "alias",
                      "repository": repo, "path": path, "blob": blob,
                      "line": source.count("\n", 0, match.start()) + 1,
                      "base_declaration": normalized(match[2]), "members": [],
                      "body_sha256": sha(match[2].encode())})
    # Exported factory functions and free utility overloads. These are kept
    # separate from the virtual-channel command totals.
    for match in re.finditer(r'(?m)^[ \t]*(?:extern "C" (?:IR_MEL_EXPORT )?|std::(?:string|ostream&)\s+)([^;{}\n]+\([^;{}]*\))\s*;', text):
        if any(s[0] < match.start() < s[1] for s in spans):
            continue
        signature = normalized(match[0]).rstrip(";")
        symbol_match = re.search(r"(operator<<|\w+)\s*\(", signature)
        require(symbol_match, "unrecognized free declaration")
        name = symbol_match[1]
        opening = signature.index("(", symbol_match.start())
        closing = matching(signature, opening, "(", ")")
        params = signature[opening + 1:closing]
        params = re.sub(r"\s+\b\w+\b(?=\s*[,)]|\s*$)", "", params)
        params = re.sub(r"\s*([<>,&*])\s*", r"\1", params)
        ns = namespace_at(match.start())
        symbol = (ns + "::" if ns else "") + name
        identity = name + "(" + params + ")"
        operations.append({"id": repo + ":operation:" + symbol + "::" + identity,
                           "symbol": symbol, "interface": "FactoryFunctions" if name.startswith("get") else "EnumUtilities",
                           "overload": identity, "signature": signature,
                           "repository": repo, "path": path, "blob": blob,
                           "category": "provider_infrastructure", "requirements": ["unspecified"]})
    return types, operations


def frozen_sources(evidence):
    require(evidence.get("schema_version") == 1, "evidence schema")
    result = []
    for name, (url, commit, tree) in PINS.items():
        entry = evidence["repositories"][name]
        require((entry["url"], entry["commit"], entry["tree"]) == (url, commit, tree), "upstream pin")
        objects = {}
        for key, obj in entry["objects"].items():
            payload = zlib.decompress(base64.b64decode(obj["zlib_base64"], validate=True))
            require(git_hash(obj["kind"], payload) == key, "Git object identity: " + key)
            objects[key] = (obj["kind"], payload)
        require(objects[commit][0] == "commit", "commit object missing")
        require(objects[commit][1].startswith(("tree " + tree + "\n").encode()), "commit/tree identity")

        def walk(oid, prefix):
            require(objects[oid][0] == "tree", "tree object missing")
            data = objects[oid][1]
            cursor = 0
            while cursor < len(data):
                space = data.index(b" ", cursor)
                nul = data.index(b"\0", space)
                mode = data[cursor:space]
                path = prefix + data[space + 1:nul].decode()
                blob = data[nul + 1:nul + 21].hex()
                cursor = nul + 21
                if mode == b"40000":
                    walk(blob, path + "/")
                elif path.startswith("include/") and path.endswith((".h", ".hpp")):
                    require(objects[blob][0] == "blob", "header blob missing: " + path)
                    result.append((name, path, blob, objects[blob][1]))
        walk(tree, "")
    return sorted(result)


def catalog(evidence):
    headers, types, operations = [], [], []
    for repo, path, blob, data in frozen_sources(evidence):
        vendor = "native/vendor/" + ("ir-mel/" if repo == "ir" else "common-mel/") + path
        headers.append({"id": repo + ":header:" + path, "repository": repo,
                        "path": path, "blob": blob, "sha256": sha(data),
                        "section": "utility" if "/utils/" in path else "library",
                        "vendor_path": vendor,
                        "includes": re.findall(r'^\s*#include\s*[<"]([^>"\n]+)', data.decode(), re.M)})
        found_types, found_operations = declarations(repo, path, blob, data.decode())
        types.extend(found_types)
        operations.extend(found_operations)
    # Explicit declaration links for operation inputs/results and nested data
    # dependencies. Prefer the declaring namespace over an identically named
    # type in the other MEL (notably the two BIT_Command types).
    by_symbol = {t["symbol"]: t for t in types}
    by_name = {}
    for declaration in types:
        by_name.setdefault(declaration["symbol"].split("::")[-1], []).append(declaration)

    def references(text, repository, own_id):
        candidates = {}
        for token in set(re.findall(r"\b[A-Za-z_]\w*(?:::\w+)*", text)):
            if token in by_symbol:
                declaration = by_symbol[token]
                candidates[declaration["id"]] = declaration
            elif token in by_name:
                same_name = by_name[token]
                preferred = [t for t in same_name if t["repository"] == repository]
                for declaration in preferred or same_name:
                    candidates[declaration["id"]] = declaration
        return sorted(key for key in candidates if key != own_id)
    for declaration in types:
        declaration["dependencies"] = references(declaration["base_declaration"] + " " + " ".join(m["declaration"] for m in declaration["members"]), declaration["repository"], declaration["id"])
    for operation in operations:
        operation["required_types"] = references(operation["signature"], operation["repository"], operation["id"])
    return {"schema_version": 1,
            "repositories": {name: dict(zip(("url", "commit", "tree"), pin)) for name, pin in PINS.items()},
            "headers": headers, "types": sorted(types, key=lambda x: x["id"]),
            "operations": sorted(operations, key=lambda x: x["id"])}


def indexed(entries, label):
    require(isinstance(entries, list), label + " must be an array")
    result = {}
    for entry in entries:
        require(isinstance(entry, dict) and isinstance(entry.get("id"), str), label + " identity")
        require(entry["id"] not in result, "duplicate " + label + " identity: " + entry["id"])
        result[entry["id"]] = entry
    return result


@lru_cache(maxsize=None)
def file_text(root, path):
    require(isinstance(path, str) and not Path(path).is_absolute() and ".." not in Path(path).parts,
            "unsafe evidence path")
    target = root / path
    require(target.is_file(), "missing evidence path: " + path)
    return target.read_text()


def evidence_check(root, evidence, kind):
    require(isinstance(evidence, list), "evidence must be array")
    for item in evidence:
        require(set(item) == {"path", "symbol"}, "evidence schema")
        single_evidence_check(root, item["path"], item["symbol"], kind)


@lru_cache(maxsize=None)
def single_evidence_check(root, path, symbol, kind):
    item = {"path": path, "symbol": symbol}
    text = uncomment(file_text(root, item["path"])) if kind != "ada" else re.sub(r"--[^\n]*", "", file_text(root, item["path"]))
    symbol = item["symbol"]
    require(isinstance(symbol, str) and re.fullmatch(r"[A-Za-z_]\w*", symbol), "evidence symbol schema")
    require(re.search(r"\b" + re.escape(symbol) + r"\b", text, re.I if kind == "ada" else 0),
            "missing " + kind + " symbol: " + item["path"] + ":" + symbol)
    if kind == "native":
        require(item["path"].startswith("native/src/") and item["path"].endswith(".cpp"), "native implementation path")
        require(re.search(r'extern\s+"C"\s+ams_mel_status_t\s+' + symbol + r"\s*\([^;]*?\)\s*noexcept\s*\{", text, re.S), "native definition evidence: " + symbol)
        require(re.search(r"\b" + symbol + r"\s*;", file_text(root, "native/src/exports.map")), "missing native export: " + symbol)
        require(re.search(r"\b" + symbol + r"\s*\(", file_text(root, "native/include/ams_mel/abi.h")), "missing C declaration: " + symbol)
    if kind == "ada":
        require(item["path"].startswith("ada/src/") and item["path"].endswith(".ads") and "c_api" not in item["path"], "safe Ada package evidence")
        public = text.split("\nprivate\n")[0]
        require(re.search(r"\b(?:function|procedure|type|package)\s+" + symbol + r"\b", public, re.I), "missing public Ada symbol: " + symbol)
        body = root / item["path"].replace(".ads", ".adb")
        require(body.is_file() and re.search(r"\b" + symbol + r"\b", re.sub(r"--[^\n]*", "", body.read_text()), re.I), "missing Ada implementation: " + symbol)


def validate(root):
    file_text.cache_clear()
    single_evidence_check.cache_clear()
    upstream = load(root / REFERENCE)
    expected = catalog(load(root / EVIDENCE))
    for key in ("headers", "types", "operations"):
        actual_ids = indexed(upstream.get(key), "upstream " + key)
        expected_ids = indexed(expected[key], "frozen " + key)
        require(actual_ids.keys() == expected_ids.keys(), "frozen " + key + " inventory mismatch")
        require(actual_ids == expected_ids, "frozen " + key + " declaration/blob/signature mismatch")
    require(upstream == expected, "upstream schema/pins mismatch")
    require(sha(json.dumps(upstream, sort_keys=True).encode()) == CATALOG_SHA256, "reviewed catalog anchor mismatch")
    coverage = load(root / COVERAGE)
    require(set(coverage) == {"schema_version", "base", "headers", "operations", "types", "inherited", "image_capabilities", "priorities", "qualifications"}, "coverage schema")
    require(coverage["schema_version"] == 1, "coverage schema version")
    require(coverage["base"] == "5c136a04ff64d980df7aaa2bd1ee49e1ed80daaf", "baseline identity")
    sources = {repo + ":header:" + path: data for repo, path, _, data in frozen_sources(load(root / EVIDENCE))}
    known_vendor = {h["vendor_path"] for h in upstream["headers"]}
    for family in ("ir-mel", "common-mel"):
        for path in (root / "native/vendor" / family / "include").rglob("*"):
            if path.suffix in {".h", ".hpp"}:
                require(str(path.relative_to(root)) in known_vendor, "unaccounted vendored header: " + str(path))
    for key in ("headers", "types", "operations"):
        entries = indexed(coverage[key], "coverage " + key)
        require(entries.keys() == indexed(upstream[key], key).keys(), "coverage " + key + " inventory mismatch")
        for entry in entries.values():
            if key == "headers":
                require(set(entry) == {"id", "vendored", "reason"} and isinstance(entry["vendored"], bool) and entry["reason"], "header coverage schema")
                header = next(h for h in upstream[key] if h["id"] == entry["id"])
                path = root / header["vendor_path"]
                require(path.exists() == entry["vendored"], "vendor presence: " + str(path))
                if path.exists():
                    require(path.read_bytes() == sources[entry["id"]], "vendored header integrity: " + str(path))
                continue
            require(set(entry) == {"id", "status", "native", "ada", "tests", "notes", "limitations", "representation"}, "coverage entry schema: " + entry["id"])
            require(entry["status"] in STATUSES, "unsupported classification: " + str(entry["status"]))
            require(isinstance(entry["notes"], str) and entry["notes"], "classification rationale")
            require(isinstance(entry["limitations"], list) and isinstance(entry["representation"], list), "representation schema")
            for kind in ("native", "ada", "tests"):
                evidence_check(root, entry[kind], kind)
            if entry["status"] == "safe_ada":
                require(entry["native"] and entry["ada"] and entry["tests"], "safe Ada claim lacks native/Ada/test evidence: " + entry["id"])
                require(any(t["path"].endswith(".c") for t in entry["tests"]) and any(t["path"].endswith(".adb") for t in entry["tests"]), "safe Ada claim lacks C/Ada tests")
            if entry["status"] == "native_only":
                require(entry["native"] and not entry["ada"] and entry["tests"], "native-only evidence")
            if entry["status"] in {"missing", "provider_only"}:
                require(not entry["native"] and not entry["ada"], "unimplemented/provider-only has implementation evidence")
            if key == "types":
                declaration = next(t for t in upstream["types"] if t["id"] == entry["id"])
                getters = list(dict.fromkeys(re.findall(r"\b((?:get|is|has)[A-Z]\w*)\s*\(", " ".join(m["declaration"] for m in declaration["members"]))))
                require([f["upstream"] for f in entry["representation"]] == getters, "field representation inventory mismatch")
                for field in entry["representation"]:
                    require(set(field) == {"upstream", "status", "evidence"} and field["status"] in {"represented", "not_exposed", "see_notes"}, "field classification schema")
                    evidence_check(root, field["evidence"], "ada")
    # Independently require every Channel operation at every derived family.
    common = [o for o in upstream["operations"] if o["interface"] == "Channel" and o["category"] != "destructor"]
    families = sorted(t["symbol"].split("::")[-1] for t in upstream["types"] if "Channel" in t["base_declaration"] and t["kind"] == "class")
    required = {family + "|" + operation["id"] for family in families for operation in common}
    inherited = indexed(coverage["inherited"], "inherited")
    require(inherited.keys() == required, "inherited inventory mismatch")
    for item in inherited.values():
        require(set(item) == {"id", "status", "coverage_id", "notes"}, "inherited schema")
        require(item["status"] in STATUSES and item["notes"], "inherited classification")
        require(item["coverage_id"] in indexed(coverage["operations"], "operation"), "inherited mapping")
        if item["status"] == "safe_ada":
            require(next(e for e in coverage["operations"] if e["id"] == item["coverage_id"])["status"] in {"safe_ada", "partial"}, "inherited safe claim")
    for item in coverage["image_capabilities"]:
        require(set(item) == {"name", "status", "notes"} and item["status"] in STATUSES and item["notes"], "image classification schema")
    for key in ("priorities", "qualifications"):
        require(isinstance(coverage[key], list) and all(isinstance(x, str) and x for x in coverage[key]), "report narrative schema")
    review = load(root / REVIEW)
    require(sha(json.dumps(review, sort_keys=True).encode()) == REVIEW_SHA256, "reviewed binding anchor mismatch")
    for key in ("operations", "types", "inherited", "image_capabilities"):
        require(coverage[key] == review[key], "unreviewed binding/classification claim: " + key)
    # The baseline's evidence is tied to actual reviewed implementation bytes,
    # not merely a coincidental symbol name in an unrelated source file.
    for path, digest in review["source_sha256"].items():
        require(sha((root / path).read_bytes()) == digest, "reviewed implementation changed; re-audit: " + path)
    require((root / REPORT).read_text() == render(upstream, coverage), "stale generated Markdown")
    return upstream, coverage


def render(upstream, coverage):
    lines = ["# IR MEL Ada coverage baseline", "", "<!-- Generated by scripts/check_ir_api_coverage.py --write-report; do not edit. -->", "",
             "Audit-only baseline: `" + coverage["base"] + "`. No production binding changes.", "",
             "Schema, extraction limits, evidence trust and update procedure: [inventory schema](reference/irmel-api-schema.md).", "", "## Pinned upstream sources", ""]
    for name, pin in upstream["repositories"].items():
        lines += [f"- {name}: {pin['url']}; commit `{pin['commit']}`; tree `{pin['tree']}`."]
    lines += ["", "## Header and declaration totals", ""]
    for repo in PINS:
        for section in ("library", "utility"):
            headers = [h for h in upstream["headers"] if h["repository"] == repo and h["section"] == section]
            present = sum(next(e for e in coverage["headers"] if e["id"] == h["id"])["vendored"] for h in headers)
            if headers:
                lines += [f"- {repo} {section}: {len(headers)} upstream; {present} vendored; {len(headers)-present} not vendored."]
    lines += [f"- {len(upstream['types'])} type/alias declarations (including nested types); {len(upstream['operations'])} operation declarations (virtual methods/overrides, destructors, factory functions and public helper methods).",
              f"- {len(coverage['inherited'])} inherited Channel exposures, classified separately, not added to unique-overload totals.", "", "## Unique operations by interface", "",
              "| Interface | Safe Ada | Native only | Partial | Missing | Provider only |", "|---|---:|---:|---:|---:|---:|"]
    entries = indexed(coverage["operations"], "operation")
    for family in sorted({o["interface"] for o in upstream["operations"]}):
        count = Counter(entries[o["id"]]["status"] for o in upstream["operations"] if o["interface"] == family)
        lines += ["| " + family + " | " + " | ".join(str(count[s]) for s in ("safe_ada", "native_only", "partial", "missing", "provider_only")) + " |"]
    for category, title in (("outgoing", "Outgoing commands/requests"), ("metadata", "Metadata registration overloads")):
        operations = [o for o in upstream["operations"] if o["category"] == category]
        counts = Counter(entries[o["id"]]["status"] for o in operations)
        lines += ["", "## " + title, "", f"{len(operations)} unique declarations; {counts['safe_ada']} safe Ada; {counts['partial']} partial; {counts['native_only']} native only; {counts['missing']} missing.", "",
                  "Counted once at the declaring interface; channel-specific overloads remain distinct. Destructors, buffers and image delivery are excluded.", "",
                  "| Interface | Overload | Status | Qualification |", "|---|---|---|---|"]
        for o in operations:
            e = entries[o["id"]]
            lines += [f"| {o['interface']} | `{o['overload']}` | {e['status']} | {e['notes']} |"]
    lines += ["", "## Missing upstream headers", "", "Not vendoring a header is not itself a missing consumer operation.", ""]
    header_coverage = indexed(coverage["headers"], "header")
    for h in upstream["headers"]:
        e = header_coverage[h["id"]]
        if not e["vendored"]:
            lines += [f"- `{h['repository']}:{h['path']}` — blob `{h['blob']}` — {e['reason']}"]
    lines += ["", "## Data representation audit", "", "All types/aliases are classified below. Exact member declarations, enum alternatives and body hashes are in the frozen catalog; field/collection mappings and limitations are in the coverage JSON. A represented enum is not an implemented operation.", "",
              "| Declaration | Status | Representation / gaps |", "|---|---|---|"]
    for e in coverage["types"]:
        lines += [f"| `{e['id'].split(':type:')[1]}` | {e['status']} | {e['notes']} " + "; ".join(e["limitations"]) + " |"]
    lines += ["", "## Partial and lifecycle functionality", ""]
    for o in upstream["operations"]:
        e = entries[o["id"]]
        if e["status"] == "partial":
            lines += [f"- `{o['interface']}::{o['overload']}`: {e['notes']} " + "; ".join(e["limitations"])]
    lines += ["", "## Inherited Channel exposure", "", "| Family | Safe Ada | Partial | Missing | Provider only |", "|---|---:|---:|---:|---:|"]
    for family in sorted({e["id"].split("|")[0] for e in coverage["inherited"]}):
        counts = Counter(e["status"] for e in coverage["inherited"] if e["id"].startswith(family + "|"))
        lines += [f"| {family} | " + " | ".join(str(counts[s]) for s in ("safe_ada", "partial", "missing", "provider_only")) + " |"]
    lines += ["", "## Image capability gaps", ""]
    lines += [f"- **{e['name']}** — {e['status']}: {e['notes']}" for e in coverage["image_capabilities"]]
    lines += ["", "## Provider qualifications and count derivation", ""] + ["- " + x for x in coverage["qualifications"]]
    lines += ["", "## Recommended implementation order", ""] + [f"{i}. {x}" for i, x in enumerate(coverage["priorities"], 1)]
    return "\n".join(line.rstrip() for line in lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--write-report", action="store_true")
    args = parser.parse_args()
    try:
        if args.write_report:
            (args.root / REPORT).write_text(render(load(args.root / REFERENCE), load(args.root / COVERAGE)))
        upstream, coverage = validate(args.root)
        counts = Counter(e["status"] for e in coverage["operations"])
        print("IR API coverage: " + str(len(upstream["headers"])) + " headers; " + str(len(upstream["operations"])) + " operations; " + ", ".join(f"{s}={counts[s]}" for s in sorted(STATUSES)))
    except (Invalid, KeyError, TypeError, ValueError, OSError, zlib.error) as error:
        print("IR API coverage: FAIL: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
