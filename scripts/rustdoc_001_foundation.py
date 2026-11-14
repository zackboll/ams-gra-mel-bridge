#!/usr/bin/env python3
"""Standalone, non-deploying Rustdoc qualification (Python standard library only)."""

import argparse
from collections import Counter
import hashlib
from html.parser import HTMLParser
import json
import os
from pathlib import Path
import subprocess
import tempfile
from urllib.parse import unquote, urlsplit


ROOT = Path(__file__).resolve().parents[1]
CRATES = ("ams-mel", "ams-mel-sys")
LANDING = """<!doctype html>
<html lang="en"><meta charset="utf-8"><title>AMS MEL Rust reference</title>
<h1>AMS MEL Rust reference</h1>
<ul><li><a href="ams_mel/index.html">ams-mel: safe public reference</a></li>
<li><a href="ams_mel_sys/index.html">ams-mel-sys: raw, unsafe C ABI reference</a></li></ul>
<p>Local qualification build; not a publication or provider certification.</p>
</html>
"""


def diagnostics(text):
    """Count machine-readable diagnostics, not Cargo's summary warning lines."""
    counts = Counter()
    for line in text.splitlines():
        event = json.loads(line)
        if event.get("reason") == "compiler-message":
            message = event["message"]
            code = (message.get("code") or {}).get("code", "uncoded")
            counts[f"{message['level']}:{code}"] += 1
    return dict(sorted(counts.items()))


class Links(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.urls = []

    def handle_starttag(self, tag, attrs):
        self.urls.extend(value for key, value in attrs
                         if key in ("href", "src") and value)


def verify_site(site):
    """Check generated local file links; do not fetch external Rust std links."""
    site = site.resolve()
    files = sorted(site.rglob("*"))
    for path in files:
        if path.is_symlink():
            raise ValueError(f"symlink in generated reference: {path}")
    for name in ("index.html", "ams_mel/index.html", "ams_mel_sys/index.html",
                 "ams_mel/struct.Session.html", "src-files.js", "crates.js"):
        if not (site / name).is_file():
            raise ValueError(f"missing reference entry point: {name}")
    for name in ("static.files", "src/ams_mel", "src/ams_mel_sys"):
        if not (site / name).is_dir() or not any((site / name).rglob("*")):
            raise ValueError(f"missing reference assets: {name}")
    html_count = 0
    for path in files:
        if path.suffix != ".html":
            continue
        html_count += 1
        parser = Links()
        parser.feed(path.read_text(encoding="utf-8"))
        for url in parser.urls:
            parts = urlsplit(url)
            if parts.scheme or parts.netloc:
                continue
            if parts.path.startswith("/"):
                raise ValueError(f"root-relative link in {path}: {url}")
            target = (path.parent / unquote(parts.path)).resolve() if parts.path else path
            if not target.is_relative_to(site) or not target.exists():
                raise ValueError(f"missing/escaping local link in {path}: {url}")
    return html_count


def run(command, directory, env, output, label):
    print("+ " + " ".join(map(str, command)), flush=True)
    result = subprocess.run(command, cwd=directory, env=env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            check=False)
    (output / f"{label}.stdout.log").write_text(result.stdout, encoding="utf-8")
    (output / f"{label}.stderr.log").write_text(result.stderr, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(f"{label} failed ({result.returncode}); see {output}")
    return result.stdout


def qualify(doctests=False):
    if os.environ.get("CARGO_BUILD_TARGET"):
        raise ValueError("host-only qualification: unset CARGO_BUILD_TARGET")
    parent = ROOT / "rust/target/rustdoc-001"
    parent.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="run-", dir=parent))
    print(f"Qualification output: {output}", flush=True)
    env = os.environ.copy()
    env["CARGO_TARGET_DIR"] = str(output / "target")
    env["CARGO_NET_OFFLINE"] = "true"
    env["AMS_MEL_NATIVE_LIB_DIR"] = str(ROOT / "native/build/lib")
    env["RUSTDOCFLAGS"] = (env.get("RUSTDOCFLAGS", "")
                           + " --deny rustdoc::broken_intra_doc_links --warn missing_docs")
    report = {
        "schema_version": 1,
        "head": run(["git", "rev-parse", "HEAD"], ROOT, env, output, "head").strip(),
        "status": run(["git", "status", "--porcelain"], ROOT, env, output, "status"),
        "rustdoc_flags": env["RUSTDOCFLAGS"],
        "rustflags": env.get("RUSTFLAGS", ""),
        "encoded_rustflags": env.get("CARGO_ENCODED_RUSTFLAGS", ""),
        "native_library_dir": env["AMS_MEL_NATIVE_LIB_DIR"],
        "diagnostics": {},
        "doctests": "not requested",
    }
    for tool in ("cargo", "rustc", "rustdoc"):
        report[tool] = run([tool, "--version"], ROOT, env, output, tool).strip()
    for crate in CRATES:
        command = ["cargo", "doc", "--manifest-path", str(ROOT / "rust/Cargo.toml"),
                   "-p", crate, "--no-deps", "--locked", "--message-format=json"]
        text = run(command, ROOT, env, output, crate)
        report["diagnostics"][crate] = diagnostics(text)
    site = output / "target/doc"
    (site / "index.html").write_text(LANDING, encoding="utf-8")
    report["html_files_verified"] = verify_site(site)
    if doctests:
        # Reuse the hardened, production-only build. Never reconfigure the test tree.
        run(["sh", str(ROOT / "native/scripts/build.sh")], ROOT, env, output, "native")
        for tool in ("cmake", "cc", "c++"):
            report[tool] = run([tool, "--version"], ROOT, env, output, tool).splitlines()[0]
        run(["cargo", "test", "--manifest-path", str(ROOT / "rust/Cargo.toml"),
             "-p", "ams-mel", "--doc", "--locked"], ROOT, env, output, "doctests")
        report["doctests"] = "passed; see doctests.stdout.log for counts"
    report["files"] = {
        str(path.relative_to(site)): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in sorted(site.rglob("*")) if path.is_file()
    }
    (output / "qualification.json").write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({"output": str(output), "diagnostics": report["diagnostics"],
                      "html_files_verified": report["html_files_verified"],
                      "doctests": report["doctests"]}, indent=2))
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--doctests", action="store_true",
                        help="also build production ams_mel_c and qualify safe-crate doctests")
    args = parser.parse_args()
    qualify(args.doctests)


if __name__ == "__main__":
    main()
