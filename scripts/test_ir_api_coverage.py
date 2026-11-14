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
        # EraseCommand now exists: removing its evidence must still fail closed.
        self.operation("C2Channel", "send(EraseCommand)")["tests"] = []
        self.reject("safe Ada claim lacks native/Ada/test evidence")

    def test_erase_slice_is_complete_and_scoped(self):
        send = self.operation("C2Channel", "send(EraseCommand)")
        self.assertEqual(send["status"], "safe_ada")
        self.assertEqual(send["native"][0]["symbol"], "ams_mel_ir_c2_submit_erase")
        self.assertEqual(send["ada"][0]["symbol"], "Submit_Erase_Command")
        for name in ("EraseCommand", "EraseCommandType"):
            entry = next(t for t in self.coverage["types"] if t["id"].endswith("::" + name))
            self.assertEqual(entry["status"], "safe_ada")
        command = next(t for t in self.coverage["types"] if t["id"].endswith("::EraseCommand"))
        self.assertEqual([f["upstream"] for f in command["representation"]],
                         ["getCommandID", "getEraseCommandType"])
        self.assertTrue(all(f["status"] == "represented" for f in command["representation"]))
        self.assertEqual(sum(e["status"] == "safe_ada" for e in self.coverage["operations"]), 49)

    def test_c2_system_response_slice_is_independent_and_scoped(self):
        # IRMEL-ADA-008: exactly the C2 overload moves to safe_ada with its own
        # C2 export and safe Ada operation; the Track overload keeps its own
        # independent evidence and no data declaration is reclassified.
        c2 = self.operation("C2Channel", "send(SystemTrackDataResponse)")
        track = self.operation("TrackChannel", "send(SystemTrackDataResponse)")
        self.assertEqual(c2["status"], "safe_ada")
        self.assertEqual(c2["native"], [{"path": "native/src/ir_c2.cpp",
                                         "symbol": "ams_mel_ir_c2_submit_system_track_data_response"}])
        self.assertEqual(c2["ada"], [{"path": "ada/src/ams-mel-ir-c2-system_data.ads",
                                      "symbol": "Submit_System_Track_Data_Response"}])
        self.assertIn({"path": "native/tests/test_ir_c2_system_response.c", "symbol": "main"}, c2["tests"])
        self.assertIn({"path": "ada/tests/src/ams_mel_ir_c2_system_data_tests.adb", "symbol": "Run"},
                      c2["tests"])
        self.assertEqual(track["status"], "safe_ada")
        self.assertEqual(track["native"][0]["symbol"], "ams_mel_ir_track_submit_system_track_data_response")
        self.assertNotEqual(c2["native"], track["native"])
        self.assertNotEqual(c2["ada"], track["ada"])
        for name in ("SystemTrackDataResponse", "CommandStatus"):
            entry = next(t for t in self.coverage["types"] if t["id"] == "ir:type:ams::iface::irmel::" + name)
            self.assertEqual(entry["status"], "safe_ada")
        self.assertEqual(sum(e["status"] == "safe_ada" for e in self.coverage["operations"]), 49)

    def test_c2_system_response_evidence_removal_fails_closed(self):
        self.operation("C2Channel", "send(SystemTrackDataResponse)")["tests"] = []
        self.reject("safe Ada claim lacks native/Ada/test evidence")

    def test_optical_distortion_map_slice_is_exact(self):
        # IRMEL-ADA-009: exactly the ImageChannel ODM registration plus the
        # OpticalDistortionMap and ForwardRightDown data declarations move to
        # safe_ada. RowCol was already safe_ada and keeps its Track evidence.
        odm = self.operation("ImageChannel", "OpticalDistortionMap")
        self.assertEqual(odm["status"], "safe_ada")
        self.assertEqual(odm["native"], [{"path": "native/src/ir_image_odm.cpp",
                                          "symbol": "ams_mel_ir_image_odm_open"}])
        self.assertEqual([a["symbol"] for a in odm["ada"]],
                         ["Open_Optical_Distortion_Map", "Receive_Optical_Distortion_Map"])
        self.assertIn({"path": "native/tests/test_ir_image_odm.c", "symbol": "main"}, odm["tests"])
        self.assertIn({"path": "ada/tests/src/ams_mel_ir_image_odm_tests.adb", "symbol": "Run"},
                      odm["tests"])
        # Independent of the four-kind metadata Open and the other opt-in owners.
        for payload in ("BadPixelList", "NUC_TempData", "CameraCommandResp"):
            other = self.operation("ImageChannel", payload)
            self.assertEqual(other["status"], "safe_ada")
            self.assertNotIn("ams_mel_ir_image_odm_open", [n["symbol"] for n in other["native"]])
        types = {t["id"].split("::")[-1]: t for t in self.coverage["types"]}
        self.assertEqual(types["OpticalDistortionMap"]["status"], "safe_ada")
        self.assertEqual([f["upstream"] for f in types["OpticalDistortionMap"]["representation"]],
                         ["getWidth", "getHeight", "getRightSize", "getDownSize", "getPixelsPerOdm",
                          "getRightMin", "getRightMax", "getDownMin", "getDownMax", "getFrd", "getRowCol"])
        self.assertEqual(types["ForwardRightDown"]["status"], "safe_ada")
        self.assertEqual([f["upstream"] for f in types["ForwardRightDown"]["representation"]],
                         ["getForward", "getRight", "getDown"])
        for name in ("OpticalDistortionMap", "ForwardRightDown"):
            self.assertTrue(all(f["status"] == "represented" for f in types[name]["representation"]))
        self.assertEqual(types["RowCol"]["status"], "safe_ada")
        self.assertEqual(types["RowCol"]["native"][0]["symbol"], "ams_mel_ir_track_metadata_open")
        self.assertEqual(sum(e["status"] == "safe_ada" for e in self.coverage["operations"]), 49)

    def test_line_of_sight_quaternion_slice_is_exact(self):
        # IRMEL-ADA-010: exactly the ImageChannel LineOfSightQuaternion registration
        # and the LineOfSightQuaternion declaration move to safe_ada. The canonical
        # Quaternion and ForwardRightDown were already safe_ada and keep their evidence.
        losq = self.operation("ImageChannel", "LineOfSightQuaternion")
        self.assertEqual(losq["status"], "safe_ada")
        self.assertEqual(losq["native"], [{"path": "native/src/ir_image_los_quaternion.cpp",
                                           "symbol": "ams_mel_ir_image_los_quaternion_open"}])
        self.assertEqual([a["symbol"] for a in losq["ada"]],
                         ["Open_Line_Of_Sight_Quaternion", "Receive_Line_Of_Sight_Quaternion"])
        self.assertIn({"path": "native/tests/test_ir_image_los_quaternion.c", "symbol": "main"},
                      losq["tests"])
        self.assertIn({"path": "ada/tests/src/ams_mel_ir_image_los_quaternion_tests.adb",
                       "symbol": "Run"}, losq["tests"])
        # Independent of the four-kind metadata Open and the other opt-in owners.
        for payload in ("BadPixelList", "LineOfSightEuler", "LineOfSightReport", "NUC_TempData",
                        "CameraCommandResp", "OpticalDistortionMap"):
            other = self.operation("ImageChannel", payload)
            self.assertEqual(other["status"], "safe_ada")
            self.assertNotIn("ams_mel_ir_image_los_quaternion_open",
                             [n["symbol"] for n in other["native"]])
        types = {t["id"].split("::")[-1]: t for t in self.coverage["types"]}
        entry = types["LineOfSightQuaternion"]
        self.assertEqual(entry["status"], "safe_ada")
        self.assertEqual([f["upstream"] for f in entry["representation"]],
                         ["getSystemTime", "getQ_xyzw", "getPqr_rps"])
        self.assertTrue(all(f["status"] == "represented" for f in entry["representation"]))
        # Canonical component types are reused, not reclassified.
        self.assertEqual(types["Quaternion"]["status"], "safe_ada")
        self.assertEqual(types["Quaternion"]["native"][0]["symbol"], "ams_mel_ir_stream_receive_snapshot")
        self.assertEqual(types["ForwardRightDown"]["status"], "safe_ada")
        self.assertEqual(types["ForwardRightDown"]["native"][0]["symbol"], "ams_mel_ir_image_odm_open")
        self.assertEqual(self.operation("ImageChannel", "LOS3D_KinematicsType")["status"], "missing")
        self.assertEqual(sum(e["status"] == "safe_ada" for e in self.coverage["operations"]), 49)

    def test_scheduling_foundation_is_owner_only(self):
        # IRMEL-ADA-011 foundation remains unchanged. IRMEL-ADA-012 and IRMEL-ADA-013 permit
        # only the ExecuteTaskAck and TaskExecutingRep deltas: nine other methods and the task
        # submission types stay missing; safe Ada is 49.
        types = {t["id"].split("::")[-1]: t for t in self.coverage["types"]}
        self.assertEqual(types["SchedulingChannel"]["status"], "partial")
        self.assertTrue(any("Nine Scheduling-specific" in x
                            for x in types["SchedulingChannel"]["limitations"]))
        for name in ("AbortTask", "IRSTScanTask", "IRSTStareTask", "IRSTTrackTask",
                     "PrioritizedTrackListCmd",
                     "SchedCreatedRep", "TaskEvents", "ScanPerformanceReport"):
            self.assertEqual(types[name]["status"], "missing", name)
        specific = [o for o in self.upstream["operations"]
                    if o["interface"] == "SchedulingChannel" and o["category"] != "destructor"]
        self.assertEqual(sorted(o["overload"].split("(")[0] for o in specific),
                         ["exec"] * 4 + ["registerMetadataCallback"] * 6 + ["send"])
        entries = {e["id"]: e for e in self.coverage["operations"]}
        for operation in specific:
            self.assertEqual(entries[operation["id"]]["status"],
                             "safe_ada" if ("ExecuteTaskAck" in operation["id"]
                                            or "TaskExecutingRep" in operation["id"]) else "missing",
                             operation["id"])
        inherited = {e["id"].split("Channel::")[-1]: e["status"] for e in self.coverage["inherited"]
                     if e["id"].startswith("SchedulingChannel|")}
        self.assertEqual(inherited, {
            "disable()": "partial", "enable()": "safe_ada",
            "getCapabilities() const": "safe_ada", "send(ChannelCommsTestReq)": "safe_ada",
            "sendKeepAliveRep()": "safe_ada",
            "registerBuffer(std::shared_ptr<Buffer>)": "missing",
            "unregisterBuffer(std::shared_ptr<Buffer>)": "missing",
            "registerMetadataCallback(std::function<void(Channel&,const ChannelCommsTestRep*const)>)":
                "missing"})
        headers = {h["id"]: h for h in self.coverage["headers"]}
        types_closure = {"AbortTask.h", "IRSTScanTask.h", "IRSTStareTask.h", "IRSTTrackTask.h",
                         "PrioritizedTrackListCmd.h", "UpdateTrackListResponse.h", "TaskInfo.h",
                         "StareTaskParam.h", "TrackTaskParam.h", "TrackTaskData.h",
                         "TrackData.h", "TrackListResponse.h"}
        closure = [h for h in self.upstream["headers"]
                   if "/scheduling/" in h["path"]
                   or ("/irmel-types/" in h["path"] and h["path"].rsplit("/", 1)[1] in types_closure)]
        self.assertEqual(len(closure), 20)
        self.assertTrue(all(headers[h["id"]]["vendored"] for h in closure))
        self.assertEqual(sum(e["status"] == "safe_ada" for e in self.coverage["operations"]), 49)

    def test_execute_task_ack_complete_and_only_scheduling_callback(self):
        operation = self.operation("SchedulingChannel", "ExecuteTaskAck")
        self.assertEqual(operation["status"], "safe_ada")
        entry = next(t for t in self.coverage["types"] if t["id"].endswith("::ExecuteTaskAck"))
        self.assertEqual(entry["status"], "safe_ada")
        self.assertEqual([(f["upstream"], f["status"]) for f in entry["representation"]],
                         [("getCommandID", "represented"), ("getReqId", "represented")])
        self.assertTrue(all(f["evidence"] for f in entry["representation"]))
        operations = [e for e in self.coverage["operations"] if "SchedulingChannel::" in e["id"]]
        self.assertEqual(sum(e["status"] == "safe_ada" for e in operations), 2)
        self.assertEqual(sum(e["status"] == "missing" for e in operations), 9)
        self.assertEqual(sum(e["status"] == "safe_ada" for e in self.coverage["operations"]), 49)

    def test_execute_task_ack_needs_c_and_ada_evidence(self):
        self.operation("SchedulingChannel", "ExecuteTaskAck")["tests"] = []
        self.reject("safe Ada claim lacks native/Ada/test evidence")

    def test_execute_task_ack_getter_omission_is_rejected(self):
        entry = next(t for t in self.coverage["types"] if t["id"].endswith("::ExecuteTaskAck"))
        entry["representation"].pop()
        self.reject("field representation inventory mismatch")

    def test_task_executing_complete_and_independent(self):
        # IRMEL-ADA-013: exactly the TaskExecutingRep callback, payload and its two enums move.
        operation = self.operation("SchedulingChannel", "TaskExecutingRep")
        self.assertEqual(operation["status"], "safe_ada")
        self.assertEqual([e["symbol"] for e in operation["native"]],
                         ["ams_mel_ir_scheduling_task_executing_open"])
        self.assertNotEqual(operation["native"], self.operation("SchedulingChannel", "ExecuteTaskAck")["native"])
        types = {t["id"].split("::")[-1]: t for t in self.coverage["types"]
                 if t["id"].startswith("ir:type:ams::iface::irmel::")}
        entry = types["TaskExecutingRep"]
        self.assertEqual(entry["status"], "safe_ada")
        self.assertEqual([(f["upstream"], f["status"]) for f in entry["representation"]],
                         [(g, "represented") for g in (
                             "getCommandID", "getActivityId", "getReqId", "getTask",
                             "getEstimatedDuration", "getTrackId", "getPrevActivityId",
                             "getPrevReqId", "getPrevTask", "getPrevTaskStatus")])
        self.assertTrue(all(f["evidence"] for f in entry["representation"]))
        for name in ("Task", "TaskCompStatus"):
            self.assertEqual(types[name]["status"], "safe_ada", name)
            self.assertTrue(types[name]["limitations"], name)
        self.assertTrue(any("NumTaskTypes" in x for x in types["Task"]["limitations"]))
        self.assertTrue(any("previous task" in x for x in types["TaskCompStatus"]["limitations"]))
        task = next(t for t in self.upstream["types"] if t["id"] == "ir:type:ams::iface::irmel::Task")
        alternatives = [a.split("///")[0].strip() for m in task["members"]
                        for a in m["declaration"].split(",") if a.split("///")[0].strip()]
        self.assertEqual(alternatives[11:], ["Reserved%d" % n for n in range(1, 7)] + ["NumTaskTypes"])
        self.assertEqual(types["SchedulingChannel"]["status"], "partial")
        self.assertEqual(sum(e["status"] == "safe_ada" for e in self.coverage["operations"]), 49)

    def test_task_executing_needs_c_and_ada_evidence(self):
        self.operation("SchedulingChannel", "TaskExecutingRep")["tests"] = []
        self.reject("safe Ada claim lacks native/Ada/test evidence")

    def test_task_executing_getter_omission_is_rejected(self):
        entry = next(t for t in self.coverage["types"] if t["id"].endswith("irmel::TaskExecutingRep"))
        entry["representation"].pop(4)
        self.reject("field representation inventory mismatch")

    def test_task_enum_claim_needs_evidence(self):
        entry = next(t for t in self.coverage["types"] if t["id"] == "ir:type:ams::iface::irmel::Task")
        entry["ada"] = []
        self.reject("safe Ada claim lacks native/Ada/test evidence")

    def test_scheduling_exec_claim_is_rejected(self):
        # A Scheduling exec cannot be claimed safe Ada by borrowing the foundation evidence;
        # the borrowed sources exist, so only the independently hashed review rejects it.
        for relative in ("native/src/ir_scheduling.cpp", "ada/src/ams-mel-ir-scheduling.ads",
                         "ada/src/ams-mel-ir-scheduling.adb",
                         "native/tests/test_ir_scheduling_foundation.c",
                         "ada/tests/src/ams_mel_ir_scheduling_tests.adb"):
            target_path = self.root / relative
            target_path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(audit.ROOT / relative, target_path)
        target = self.operation("SchedulingChannel", "exec(IRSTScanTask)")
        target.update(status="safe_ada",
                      native=[{"path": "native/src/ir_scheduling.cpp",
                               "symbol": "ams_mel_ir_scheduling_open"}],
                      ada=[{"path": "ada/src/ams-mel-ir-scheduling.ads", "symbol": "Open"}],
                      tests=[{"path": "native/tests/test_ir_scheduling_foundation.c",
                              "symbol": "main"},
                             {"path": "ada/tests/src/ams_mel_ir_scheduling_tests.adb",
                              "symbol": "Run"}])
        self.reject("unreviewed binding/classification claim")

    def test_scheduling_vendored_header_presence_is_enforced(self):
        entry = next(h for h in self.coverage["headers"]
                     if h["id"].endswith("scheduling/SchedulingChannel.h"))
        entry["vendored"] = False
        self.reject("vendor presence")

    def test_line_of_sight_quaternion_evidence_removal_fails_closed(self):
        self.operation("ImageChannel", "LineOfSightQuaternion")["tests"] = []
        self.reject("safe Ada claim lacks native/Ada/test evidence")

    def test_line_of_sight_quaternion_field_omission_is_detected(self):
        entry = next(t for t in self.coverage["types"] if t["id"].endswith("::LineOfSightQuaternion"))
        entry["representation"] = [f for f in entry["representation"] if f["upstream"] != "getPqr_rps"]
        self.reject("field representation inventory mismatch")

    def test_optical_distortion_map_evidence_removal_fails_closed(self):
        self.operation("ImageChannel", "OpticalDistortionMap")["tests"] = []
        self.reject("safe Ada claim lacks native/Ada/test evidence")

    def test_optical_distortion_map_field_omission_is_detected(self):
        entry = next(t for t in self.coverage["types"] if t["id"].endswith("::OpticalDistortionMap"))
        entry["representation"] = [f for f in entry["representation"] if f["upstream"] != "getRowCol"]
        self.reject("field representation inventory mismatch")

    def test_forward_right_down_component_omission_is_detected(self):
        entry = next(t for t in self.coverage["types"] if t["id"].endswith("::ForwardRightDown"))
        entry["representation"].pop()
        self.reject("field representation inventory mismatch")

    def test_camera_protection_slice_is_exact(self):
        # IRMEL-ADA-004: exactly the C2 send and the C2 CameraProtectCmdResp callback,
        # with all three command/response fields and all three CmdResp alternatives.
        send = self.operation("C2Channel", "send(CameraProtectCmd)")
        callback = self.operation("C2Channel", "const CameraProtectCmdResp*const")
        for entry in (send, callback):
            self.assertEqual(entry["status"], "safe_ada")
            self.assertIn({"path": "native/tests/test_ir_camera_protect.c", "symbol": "main"},
                          entry["tests"])
            self.assertIn({"path": "ada/tests/src/ams_mel_ir_camera_protection_tests.adb",
                           "symbol": "Run"}, entry["tests"])
        self.assertEqual(send["native"][0]["symbol"], "ams_mel_ir_c2_submit_camera_protect")
        self.assertEqual(callback["native"][0]["symbol"], "ams_mel_ir_c2_camera_protect_open")
        types = {e["id"].split(":type:")[1]: e for e in self.coverage["types"]}
        for name in ("CameraProtectCmd", "CameraProtectCmdResp", "CmdResp"):
            self.assertEqual(types["ams::iface::irmel::" + name]["status"], "safe_ada")
        for name, getters in (("CameraProtectCmd", ["getCommandID", "getResponseTime", "getProtectFlag"]),
                              ("CameraProtectCmdResp", ["getCommandID", "getResponseTime",
                                                        "getCommandResponse"])):
            entry = types["ams::iface::irmel::" + name]
            self.assertEqual([f["upstream"] for f in entry["representation"]], getters)
            self.assertTrue(all(f["status"] == "represented" for f in entry["representation"]))
        enum = next(t for t in self.upstream["types"] if t["id"] == "ir:type:ams::iface::irmel::CmdResp")
        alternatives = [a.strip() for m in enum["members"] for a in m["declaration"].split(",")]
        self.assertEqual(alternatives, ["OK", "Unavailable", "Unsupported"])
        self.assertEqual(sum(e["status"] == "safe_ada" for e in self.coverage["operations"]), 49)

    def test_calibration_configuration_slice_is_complete_and_independent(self):
        send = self.operation("C2Channel", "send(CalibrationConfigurationCmd)")
        callback = self.operation("C2Channel", "CalibrationConfiguration*const")
        self.assertEqual(send["native"][0]["symbol"], "ams_mel_ir_c2_submit_calibration_configuration")
        self.assertEqual(callback["native"][0]["symbol"], "ams_mel_ir_c2_calibration_configuration_open")
        for entry in (send, callback):
            self.assertEqual(entry["status"], "safe_ada")
            self.assertIn({"path": "native/tests/test_ir_calibration_configuration.c", "symbol": "main"}, entry["tests"])
            self.assertIn({"path": "ada/tests/src/ams_mel_ir_calibration_configuration_tests.adb", "symbol": "Run"}, entry["tests"])
        types = {e["id"].split(":type:")[1].split("::")[-1]: e for e in self.coverage["types"]}
        for name in ("CalibrationConfigurationCmd", "CalibrationConfiguration", "SubsystemCalibration", "SubsystemCalibrationControlInterfaces"):
            self.assertEqual(types[name]["status"], "safe_ada")
        for name, getters in (("CalibrationConfigurationCmd", ["getCommandID", "getRequestID"]),
                              ("SubsystemCalibration", ["getCalibrationID", "getAcceptedInterface", "getCalibrationItemName", "getSubsystemComponentID", "getCapabilityID"])):
            self.assertEqual([field["upstream"] for field in types[name]["representation"]], getters)
            self.assertTrue(all(field["status"] == "represented" for field in types[name]["representation"]))
        enum = next(t for t in self.upstream["types"] if t["id"].endswith("::SubsystemCalibrationControlInterfaces"))
        self.assertEqual([a.strip() for m in enum["members"] for a in m["declaration"].split(",")],
                         ["NotSet", "SubsystemCalibrationCommand", "SubsystemStateCommand", "MaxExclusive"])
        self.assertEqual(sum(e["status"] == "safe_ada" for e in self.coverage["operations"]), 49)

    def test_calibration_status_slice_is_complete_and_independent(self):
        send = self.operation("C2Channel", "send(CalibrationStatusCmd)")
        callback = self.operation("C2Channel", "CalibrationStatus*const")
        self.assertEqual(send["native"][0]["symbol"], "ams_mel_ir_c2_submit_calibration_status")
        self.assertEqual(callback["native"][0]["symbol"], "ams_mel_ir_c2_calibration_status_open")
        self.assertEqual(send["ada"][0]["symbol"], "Submit_Calibration_Status")
        self.assertEqual([e["symbol"] for e in callback["ada"]],
                         ["Open_Calibration_Status", "Receive_Calibration_Status", "Calibration_Status_Counters"])
        for entry in (send, callback):
            self.assertEqual(entry["status"], "safe_ada")
            self.assertIn({"path": "native/tests/test_ir_calibration_status.c", "symbol": "main"}, entry["tests"])
            self.assertIn({"path": "ada/tests/src/ams_mel_ir_calibration_status_tests.adb", "symbol": "Run"}, entry["tests"])
        types = {e["id"].split(":type:")[1].split("::")[-1]: e for e in self.coverage["types"]}
        fields = {
            "CalibrationStatusCmd": ["getCommandID", "getRequestID"],
            "CalibrationStatus": ["getActiveCalibrationIDs", "getCompletedCalibrations"],
            "SubsystemCompletedCalibration": ["getCalibrationID", "getTimeTag", "getResult", "getFailReason",
                                              "getCalibrationItems"],
            "SubsystemCompletedCalibrationItemType": ["getItemName", "getResult", "getFailReason"],
            "SubsystemCalibrationResult": [],
        }
        for name, getters in fields.items():
            self.assertEqual(types[name]["status"], "safe_ada", name)
            self.assertEqual([field["upstream"] for field in types[name]["representation"]], getters)
            self.assertTrue(all(field["status"] == "represented" and field["evidence"]
                                for field in types[name]["representation"]))
        enum = next(t for t in self.upstream["types"] if t["id"].endswith("::SubsystemCalibrationResult"))
        self.assertEqual([a.strip() for m in enum["members"] for a in m["declaration"].split(",")],
                         ["NotSet", "Completed", "Failed", "Interrupted", "NotCalibrated", "MaxExclusive"])
        self.assertEqual(sum(e["status"] == "safe_ada" for e in self.coverage["operations"]), 49)

    def test_calibration_status_nested_item_omission_is_detected(self):
        entry = next(t for t in self.coverage["types"] if t["id"].endswith("::SubsystemCompletedCalibration"))
        entry["representation"] = [f for f in entry["representation"] if f["upstream"] != "getCalibrationItems"]
        self.reject("field representation inventory mismatch")

    def test_calibration_status_callback_needs_real_ada_evidence(self):
        entry = self.operation("C2Channel", "CalibrationStatus*const")
        entry["ada"][1]["symbol"] = "Receive_Fictitious_Status"
        self.reject("missing ada symbol")

    def test_calibration_status_needs_c_and_ada_tests(self):
        send = self.operation("C2Channel", "send(CalibrationStatusCmd)")
        send["tests"] = [t for t in send["tests"] if not t["path"].endswith(".adb")]
        self.reject("safe Ada claim lacks C/Ada tests")

    def test_calibration_nested_field_omission_is_detected(self):
        entry = next(t for t in self.coverage["types"] if t["id"].endswith("::SubsystemCalibration"))
        entry["representation"].pop()
        self.reject("field representation inventory mismatch")

    def test_calibration_callback_cannot_borrow_base_metadata_evidence(self):
        entry = self.operation("C2Channel", "CalibrationConfiguration*const")
        entry["ada"][1]["symbol"] = "Receive_Fictitious_Configuration"
        self.reject("missing ada symbol")

    def test_camera_protection_callback_needs_evidence(self):
        callback = self.operation("C2Channel", "const CameraProtectCmdResp*const")
        callback["ada"][1]["symbol"] = "Receive_Fictitious_Protection"
        self.reject("missing ada symbol")

    def test_camera_protection_needs_c_and_ada_tests(self):
        send = self.operation("C2Channel", "send(CameraProtectCmd)")
        send["tests"] = [t for t in send["tests"] if not t["path"].endswith(".c")]
        self.reject("safe Ada claim lacks C/Ada tests")

    def test_camera_command_slice_is_exact(self):
        # IRMEL-ADA-003: exactly the two independent sends and the Image callback;
        # no C2 CameraCommandResp callback exists upstream or is claimed.
        c2 = self.operation("C2Channel", "send(CameraCommand)")
        image = self.operation("ImageChannel", "send(CameraCommand)")
        callback = self.operation("ImageChannel", "const CameraCommandResp*const")
        for entry in (c2, image, callback):
            self.assertEqual(entry["status"], "safe_ada")
        self.assertEqual(c2["native"][0]["symbol"], "ams_mel_ir_c2_submit_camera_command")
        self.assertEqual(image["native"][0]["symbol"], "ams_mel_ir_stream_submit_camera_command")
        self.assertFalse(any(o["interface"] == "C2Channel" and "CameraCommandResp*" in o["overload"]
                             for o in self.upstream["operations"]))
        types = {e["id"].split(":type:")[1]: e for e in self.coverage["types"]}
        for name in ("CameraCommand", "CameraCommandResp"):
            entry = types["ams::iface::irmel::" + name]
            self.assertEqual(entry["status"], "safe_ada")
            self.assertTrue(all(f["status"] == "represented" for f in entry["representation"]))
        self.assertEqual(len(types["ams::iface::irmel::CameraCommand"]["representation"]), 11)
        self.assertEqual(len(types["ams::iface::irmel::CameraCommandResp"]["representation"]), 4)

    def test_camera_callback_needs_evidence(self):
        callback = self.operation("ImageChannel", "const CameraCommandResp*const")
        callback["native"][0]["symbol"] = "ams_mel_ir_fictitious_camera_open"
        self.reject("missing native symbol")

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
        # IRMEL-ADA-002 implemented NUC_TempData, IRMEL-ADA-009 OpticalDistortionMap
        # and IRMEL-ADA-010 LineOfSightQuaternion; LOS3D_KinematicsType remains an
        # unimplemented @Optional Image callback for this borrowed-evidence control.
        target = self.operation("ImageChannel", "LOS3D_KinematicsType")
        self.assertEqual(target["status"], "missing")
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

    def test_current_implementation_may_differ_from_historical_baseline(self):
        path = self.root / "native/src/ir_stream.cpp"
        path.write_text(path.read_text() + "\n")
        audit.validate(self.root)

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
