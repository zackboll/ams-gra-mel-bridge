#!/usr/bin/env python3
"""Regression controls use isolated audit/evidence/source copies, never the checkout."""

import copy
import json
from pathlib import Path
import shutil
import tempfile
import unittest

import check_ir_api_coverage as audit


class CoverageTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.upstream = audit.load(audit.ROOT / audit.REFERENCE)
        self.coverage = audit.load(audit.ROOT / audit.COVERAGE)
        review = audit.load(audit.ROOT / audit.REVIEW)
        paths = {audit.REFERENCE, audit.COVERAGE, audit.REPORT, audit.REVIEW, audit.EVIDENCE}
        paths.update(review["source_sha256"])
        paths.update(h["vendor_path"] for h in self.upstream["headers"]
                     if (audit.ROOT / h["vendor_path"]).exists())
        for key in ("types", "operations"):
            for entry in self.coverage[key]:
                for kind in ("native", "ada", "tests"):
                    paths.update(e["path"] for e in entry[kind])
                    if kind == "ada":
                        paths.update(e["path"].replace(".ads", ".adb") for e in entry[kind])
        for relative in paths:
            target = self.root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(audit.ROOT / relative, target)

    def write(self, relative, value):
        (self.root / relative).write_text(json.dumps(value, indent=2) + "\n")

    def reject(self, reason):
        self.write(audit.REFERENCE, self.upstream)
        self.write(audit.COVERAGE, self.coverage)
        with self.assertRaisesRegex(audit.Invalid, reason):
            audit.validate(self.root)

    def operation(self, family, payload):
        declaration = next(o for o in self.upstream["operations"]
                           if o["interface"] == family and payload in o["overload"])
        return next(e for e in self.coverage["operations"] if e["id"] == declaration["id"])

    def test_baseline_and_truthful_missing_succeed(self):
        upstream, coverage = audit.validate(self.root)
        self.assertEqual(len(upstream["headers"]), 127)
        for category, total in (("outgoing", 21), ("metadata", 38)):
            self.assertEqual(sum(o["category"] == category for o in upstream["operations"]), total)
        self.assertTrue(any(e["status"] == "missing" for e in coverage["operations"]))

    def test_missing_upstream_header(self):
        self.upstream["headers"].pop()
        self.reject("frozen headers inventory mismatch")

    def test_missing_method_overload(self):
        self.upstream["operations"].pop()
        self.reject("frozen operations inventory mismatch")

    def test_duplicate_method_identity(self):
        self.upstream["operations"].append(copy.deepcopy(self.upstream["operations"][0]))
        self.reject("duplicate upstream operations identity")

    def test_incorrect_pinned_blob(self):
        self.upstream["headers"][0]["blob"] = "0" * 40
        self.reject("frozen headers declaration/blob/signature mismatch")

    def test_changed_signature(self):
        self.upstream["operations"][0]["signature"] += " malformed"
        self.reject("frozen operations declaration/blob/signature mismatch")

    def test_unsupported_classification(self):
        self.coverage["operations"][0]["status"] = "probably_complete"
        self.reject("unsupported classification")

    def test_malformed_coverage_schema(self):
        self.coverage["operations"][0]["complete"] = True
        self.reject("coverage entry schema")

    def test_fake_safe_ada_complete(self):
        self.operation("C2Channel", "send(CameraCommand)")["status"] = "safe_ada"
        self.reject("safe Ada claim lacks native/Ada/test evidence")

    def test_missing_native_export(self):
        e = self.operation("C2Channel", "send(ModeCmd)")
        path = self.root / "native/src/exports.map"
        path.write_text(path.read_text().replace(e["native"][0]["symbol"] + ";", ""))
        self.reject("missing native export")

    def test_missing_ada_package(self):
        self.operation("C2Channel", "send(ModeCmd)")["ada"][0]["path"] = "ada/src/nonexistent.ads"
        self.reject("missing evidence path")

    def test_missing_ada_symbol(self):
        self.operation("C2Channel", "send(ModeCmd)")["ada"][0]["symbol"] = "Fictitious_Submit"
        self.reject("missing ada symbol")

    def test_stale_markdown(self):
        (self.root / audit.REPORT).write_text("stale\n")
        self.reject("stale generated Markdown")

    def test_same_command_different_channels(self):
        c2 = self.operation("C2Channel", "send(SystemTrackDataResponse)")
        track = self.operation("TrackChannel", "send(SystemTrackDataResponse)")
        self.coverage["operations"].remove(c2)
        self.coverage["operations"].append(copy.deepcopy(track))
        self.reject("duplicate coverage operations identity")

    def test_optional_callback_not_implemented(self):
        target = self.operation("ImageChannel", "NUC_TempData")
        source = self.operation("ImageChannel", "BadPixelList")
        for key in ("status", "native", "ada", "tests"):
            target[key] = copy.deepcopy(source[key])
        self.reject("unreviewed binding/classification claim")

    def test_missing_coverage_classification(self):
        self.coverage["operations"].pop()
        self.reject("coverage operations inventory mismatch")

    def test_duplicate_coverage_classification(self):
        self.coverage["operations"].append(copy.deepcopy(self.coverage["operations"][0]))
        self.reject("duplicate coverage operations identity")

    def test_vendor_byte_change(self):
        header = next(h for h in self.upstream["headers"] if (self.root / h["vendor_path"]).exists())
        with (self.root / header["vendor_path"]).open("ab") as stream:
            stream.write(b"\n")
        self.reject("vendored header integrity")

    def test_git_object_tampering(self):
        evidence = audit.load(self.root / audit.EVIDENCE)
        obj = evidence["repositories"]["ir"]["objects"][audit.PINS["ir"][1]]
        import base64
        import zlib
        payload = zlib.decompress(base64.b64decode(obj["zlib_base64"])) + b"\n"
        obj["zlib_base64"] = base64.b64encode(zlib.compress(payload)).decode()
        self.write(audit.EVIDENCE, evidence)
        self.reject("Git object identity")

    def test_lost_inherited_exposure(self):
        self.coverage["inherited"].pop()
        self.reject("inherited inventory mismatch")

    def test_reviewed_source_change(self):
        path = self.root / "native/src/ir_stream.cpp"
        path.write_text(path.read_text() + "\n")
        self.reject("reviewed implementation changed; re-audit")

    def test_field_omission(self):
        entry = next(e for e in self.coverage["types"] if e["representation"])
        entry["representation"].pop()
        self.reject("field representation inventory mismatch")

    def test_reviewed_extraction_exceptions(self):
        operations = self.upstream["operations"]
        # Implicit virtual overrides without the virtual keyword are included.
        self.assertTrue(any(o["interface"] == "StackedImageListener" and o["overload"].startswith("onImage(") for o in operations))
        # decltype factory pointer fields are data, not virtual operations.
        self.assertFalse(any("::decltype(" in o["id"] for o in operations))
        # Both QueryInterface template and integer overload are provider helpers.
        self.assertEqual(sum(o["interface"] == "IRInterfaceBase" and o["overload"].startswith("QueryInterface(") for o in operations), 2)
        # Callback pointer constness and const-reference parameter survive.
        self.assertTrue(any(o["interface"] == "StackedImageChannel" and "*const)>const&" in o["overload"] for o in operations))
        # Requirement classification is per method, not per class.
        optional = next(o for o in operations if o["interface"] == "ImageChannel" and "NUC_TempData" in o["overload"])
        self.assertEqual(optional["requirements"], ["Optional"])
        self.assertTrue(any(t["symbol"] == "library::LibraryLoader" for t in self.upstream["types"]))
        self.assertTrue(any(t["symbol"] == "ams::iface::mel::MFA_SecurityAuditRecord::SecurityEventType" for t in self.upstream["types"]))
        # Constructor-initializer fragments cannot create outgoing operations;
        # exact original syntax remains independently available in blob/body.
        frame = next(t for t in self.upstream["types"] if t["symbol"] == "ams::iface::irmel::FrameHeader")
        self.assertEqual(len(frame["body_sha256"]), 64)
        self.assertFalse(any(o["interface"] == "FrameHeader" for o in operations))

    def test_independent_explicit_virtual_scan(self):
        import re
        for repo, path, _, source in audit.frozen_sources(audit.load(self.root / audit.EVIDENCE)):
            text = audit.uncomment(source.decode())
            explicit_count = len(re.findall(r"\bvirtual\s+(?!public\b|Channel\b)[^;{}]*\(", text))
            extracted_count = sum(o["repository"] == repo and o["path"] == path
                                  and bool(re.search(r"\bvirtual\b", o["signature"]))
                                  for o in self.upstream["operations"])
            self.assertEqual(extracted_count, explicit_count, path)


if __name__ == "__main__":
    unittest.main()
