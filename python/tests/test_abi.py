from __future__ import annotations

import ctypes
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from ams_mel import AbiVersion, abi_version
from ams_mel import _native


class AbiTests(unittest.TestCase):
    def test_private_interval_spatial_signature(self) -> None:
        function = _native.ams_mel_rf_job_add_rx_intervals_v3
        self.assertEqual(function.argtypes, [
            _native.RfJobHandle, _native.RfJobIntervalConfigSpanV3,
            _native.CharPointer, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)])
        self.assertIs(function.restype, ctypes.c_int32)
        import ams_mel
        for name in ("ams_mel_rf_job_add_rx_intervals_v3", "RfJobIntervalConfigV3",
                     "RfJobIntervalConfigSpanV3", "RfReceiveEventConfigV2",
                     "RfReceiveEventConfigSpanV2"):
            self.assertNotIn(name, ams_mel.__all__)
    def test_private_job_request_v2_signature(self) -> None:
        function = _native.ams_mel_rf_virtual_aperture_submit_job_v2
        self.assertEqual(function.argtypes, [
            _native.RfVaHandle, ctypes.POINTER(_native.RfJobRequestConfigV2),
            ctypes.POINTER(_native.RfJobRequestHandle), _native.CharPointer,
            ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)])
        self.assertIs(function.restype, ctypes.c_int32)
        import ams_mel
        for name in ("ams_mel_rf_virtual_aperture_submit_job_v2", "RfJobRequestConfigV2",
                     "RfUtcTimeV1", "RfRxElementGroupConfigV2", "RfRxElementGroupConfigSpanV2",
                     "RfRxDataPipeEndpointConfigV1", "RfRxDataPipeEndpointConfigSpanV1"):
            self.assertNotIn(name, ams_mel.__all__)

    def test_private_job_request_v3_signature(self) -> None:
        function = _native.ams_mel_rf_virtual_aperture_submit_job_v3
        self.assertEqual(function.argtypes, [
            _native.RfVaHandle, ctypes.POINTER(_native.RfJobRequestConfigV3),
            ctypes.POINTER(_native.RfJobRequestHandle), _native.CharPointer,
            ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)])
        self.assertIs(function.restype, ctypes.c_int32)
        import ams_mel
        for name in ("ams_mel_rf_virtual_aperture_submit_job_v3", "RfJobRequestConfigV3",
                     "RfUtcTimeV1", "RfRxElementGroupConfigV3", "RfRxElementGroupConfigSpanV3",
                     "RfPointingV1", "RfPointingSpanV1", "RfVector3V1", "RfAzElV1",
                     "RfEcefPointingV1", "RfLlaPointingV1", "RfPointingKind"):
            self.assertNotIn(name, ams_mel.__all__)
        self.assertEqual([_native.RF_POINTING_ECEF, _native.RF_POINTING_LLA,
                          _native.RF_POINTING_PLATFORM_RELATIVE, _native.RF_POINTING_FACE_RELATIVE,
                          _native.RF_POINTING_BASELINE_RELATIVE], list(range(5)))

    def test_private_job_request_v4_signature(self) -> None:
        function = _native.ams_mel_rf_virtual_aperture_submit_job_v4
        self.assertEqual(function.argtypes, [
            _native.RfVaHandle, ctypes.POINTER(_native.RfJobRequestConfigV4),
            ctypes.POINTER(_native.RfJobRequestHandle), _native.CharPointer,
            ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)])
        self.assertIs(function.restype, ctypes.c_int32)
        import ams_mel
        for name in ("ams_mel_rf_virtual_aperture_submit_job_v4", "RfJobRequestConfigV4",
                     "RfUtcTimeV1", "RfTxElementGroupConfigV1", "RfJobElementGroupConfigV4", "RfJobElementGroupConfigSpanV4",
                     "RfPointingV1", "RfPointingSpanV1", "RfVector3V1", "RfAzElV1",
                     "RfEcefPointingV1", "RfLlaPointingV1", "RfPointingKind"):
            self.assertNotIn(name, ams_mel.__all__)

    def test_private_transmit_power_signatures(self) -> None:
        diagnostic = [_native.CharPointer, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
        va, u64, u32, f64 = _native.RfVaHandle, ctypes.c_uint64, ctypes.c_uint32, ctypes.c_double
        output = ctypes.POINTER(f64)
        expected = {
            "ams_mel_rf_virtual_aperture_get_tx_radiated_power": [va, u64, u32, f64, u64, f64, f64, f64, u32, output],
            "ams_mel_rf_virtual_aperture_get_tx_peak_radiated_power": [va, u64, u32, f64, f64, u32, output],
            "ams_mel_rf_virtual_aperture_get_tx_aperture_gain": [va, u64, u32, u64, f64, f64, f64, u32, output],
            "ams_mel_rf_virtual_aperture_get_max_tx_attenuation": [va, u64, u32, u32, output],
        }
        import ams_mel
        for name, prefix in expected.items():
            function = getattr(_native, name)
            self.assertEqual(function.argtypes, [*prefix, *diagnostic])
            self.assertIs(function.restype, ctypes.c_int32)
            self.assertNotIn(name, ams_mel.__all__)
    def test_private_va_data_pipe_signatures(self) -> None:
        diagnostic = [_native.CharPointer, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
        owner = _native.RfVaDataPipeConnectionsSnapshotHandle
        expected = {
            "ams_mel_rf_virtual_aperture_get_data_pipes": [_native.RfVaHandle, ctypes.POINTER(owner)],
            "ams_mel_rf_va_data_pipe_connections_snapshot_view": [owner, ctypes.POINTER(ctypes.POINTER(_native.RfVaDataPipeConnectionsSnapshotV1))],
            "ams_mel_rf_va_data_pipe_connections_snapshot_close": [ctypes.POINTER(owner)],
            "ams_mel_rf_virtual_aperture_associate_data_pipe_endpoint": [_native.RfVaHandle, _native.StringViewV1, _native.StringViewV1, ctypes.c_uint64, ctypes.POINTER(ctypes.c_uint32)],
            "ams_mel_rf_virtual_aperture_associate_data_pipe_endpoints": [_native.RfVaHandle, _native.StringViewV1, _native.StringViewV1, _native.U64SpanV1, ctypes.POINTER(ctypes.c_uint32)],
        }
        import ams_mel
        for name, prefix in expected.items():
            function = getattr(_native, name)
            self.assertEqual(function.argtypes, [*prefix, *diagnostic])
            self.assertIs(function.restype, ctypes.c_int32)
            self.assertNotIn(name, ams_mel.__all__)
        for name in ("RfVaDataPipeConnectionsSnapshotHandle", "RfVaDataPipeGroupV1",
                     "RfVaDataPipeGroupSpanV1", "RfVaDataPipeConnectionsSnapshotV1"):
            self.assertNotIn(name, ams_mel.__all__)
    def test_private_local_function_signatures(self) -> None:
        diagnostic = [_native.CharPointer, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
        l = _native.RfVaLocalFunctionListHandle
        s = _native.RfVaLocalFunctionStatusHandle
        expected = {
            "ams_mel_rf_virtual_aperture_is_cached_waveform_supported": [_native.RfVaHandle, ctypes.POINTER(ctypes.c_uint32)],
            "ams_mel_rf_virtual_aperture_dynamic_weights_supported": [_native.RfVaHandle, ctypes.POINTER(ctypes.c_uint32)],
            "ams_mel_rf_virtual_aperture_get_local_functions": [_native.RfVaHandle, ctypes.POINTER(l)],
            "ams_mel_rf_va_local_function_list_view": [l, ctypes.POINTER(_native.RfVaLocalFunctionInfoSpanV1)],
            "ams_mel_rf_va_local_function_list_close": [ctypes.POINTER(l)],
            "ams_mel_rf_virtual_aperture_get_local_function_status": [_native.RfVaHandle, ctypes.c_uint32, ctypes.c_uint32, ctypes.POINTER(s)],
            "ams_mel_rf_va_local_function_status_view": [s, ctypes.POINTER(_native.U32SpanV1)],
            "ams_mel_rf_va_local_function_status_close": [ctypes.POINTER(s)],
        }
        import ams_mel
        for name, prefix in expected.items():
            function = getattr(_native, name)
            self.assertEqual(function.argtypes, [*prefix, *diagnostic])
            self.assertIs(function.restype, ctypes.c_int32)
            self.assertNotIn(name, ams_mel.__all__)
        for name in ("RfVaLocalFunctionListHandle", "RfVaLocalFunctionStatusHandle",
                     "RfVaLocalFunctionInfoV1", "RfVaLocalFunctionInfoSpanV1"):
            self.assertNotIn(name, ams_mel.__all__)
    def test_private_element_group_signatures(self) -> None:
        diagnostic = [_native.CharPointer, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
        owner = _native.RfElementGroupSnapshotHandle
        expected = {
            "ams_mel_rf_virtual_aperture_get_element_groups": [_native.RfVaHandle, ctypes.POINTER(_native.RfElementGroupSnapshotOptionsV1), ctypes.POINTER(owner)],
            "ams_mel_rf_element_group_snapshot_view": [owner, ctypes.POINTER(ctypes.POINTER(_native.RfElementGroupSnapshotV1))],
            "ams_mel_rf_element_group_snapshot_close": [ctypes.POINTER(owner)],
        }
        import ams_mel
        for name, prefix in expected.items():
            function = getattr(_native, name)
            self.assertEqual(function.argtypes, [*prefix, *diagnostic])
            self.assertIs(function.restype, ctypes.c_int32)
            self.assertNotIn(name, ams_mel.__all__)
        self.assertEqual([_native.RF_ELEMENT_GROUP_MODE_RX, _native.RF_ELEMENT_GROUP_MODE_TX], [0, 1])
        for name in ("RfElementGroupSnapshotHandle", "RfElementGroupMode",
                     "RfElementGroupSnapshotOptionsV1", "RfDataPipeInfoV1",
                     "RfDataPipeInfoSpanV1", "RfElementGroupDescriptorV1",
                     "RfElementGroupDescriptorSpanV1", "RfElementGroupSnapshotV1"):
            self.assertNotIn(name, ams_mel.__all__)
    def test_private_va_notification_signatures(self) -> None:
        diagnostic = [_native.CharPointer, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
        s = _native.RfVaSubscriptionHandle
        expected = {
            "ams_mel_rf_va_status_subscription_open": [_native.RfVaHandle, ctypes.POINTER(s)],
            "ams_mel_rf_va_status_subscription_wait": [s, ctypes.c_uint32],
            "ams_mel_rf_va_status_subscription_get_statistics": [s, ctypes.POINTER(_native.RfVaSubscriptionStatisticsV1)],
            "ams_mel_rf_va_status_subscription_unsubscribe": [_native.RfVaHandle, s],
            "ams_mel_rf_va_status_subscription_close": [ctypes.POINTER(s)],
        }
        import ams_mel
        for name, prefix in expected.items():
            function = getattr(_native, name)
            self.assertEqual(function.argtypes, [*prefix, *diagnostic])
            self.assertIs(function.restype, ctypes.c_int32)
            self.assertNotIn(name, ams_mel.__all__)
        for name in ("RfVaSubscriptionHandle", "RfVaSubscriptionStatisticsV1"):
            self.assertNotIn(name, ams_mel.__all__)

    def test_private_va_query_signatures(self) -> None:
        diagnostic = [_native.CharPointer, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
        va = _native.RfVaHandle
        owner = ctypes.POINTER(_native.RfVaInstanceListHandle)
        report = ctypes.POINTER(_native.RfVaInstanceStatusReportHandle)
        scalar = ctypes.POINTER(ctypes.c_uint32)
        expected = {
            "ams_mel_rf_virtual_aperture_get_id": [va, scalar],
            "ams_mel_rf_virtual_aperture_get_status": [va, scalar],
            "ams_mel_rf_virtual_aperture_get_instance_status": [va, ctypes.c_uint32, scalar],
            "ams_mel_rf_virtual_aperture_get_all_instances": [va, owner],
            "ams_mel_rf_virtual_aperture_get_instances": [va, ctypes.c_uint32, owner],
            "ams_mel_rf_va_instance_list_view": [_native.RfVaInstanceListHandle, ctypes.POINTER(_native.U32SpanV1)],
            "ams_mel_rf_va_instance_list_close": [owner],
            "ams_mel_rf_virtual_aperture_get_instance_status_report": [va, ctypes.c_uint32, report],
            "ams_mel_rf_va_instance_status_report_view": [_native.RfVaInstanceStatusReportHandle, ctypes.POINTER(ctypes.POINTER(_native.RfVaInstanceStatusReportV1))],
            "ams_mel_rf_va_instance_status_report_close": [report],
        }
        import ams_mel
        for name, prefix in expected.items():
            function = getattr(_native, name)
            self.assertEqual(function.argtypes, [*prefix, *diagnostic], name)
            self.assertIs(function.restype, ctypes.c_int32)
            self.assertNotIn(name, ams_mel.__all__)
        for name in ("RfVaInstanceListHandle", "RfVaInstanceStatusReportHandle",
                     "RfVirtualApertureStatus", "RfVaLocalFunctionStatusV1",
                     "RfVaLocalFunctionStatusSpanV1", "RfVaInstanceStatusReportV1"):
            self.assertNotIn(name, ams_mel.__all__)
        self.assertEqual([_native.RF_VA_STATUS_NONE, _native.RF_VA_STATUS_OPERATIONAL,
                          _native.RF_VA_STATUS_DEGRADED, _native.RF_VA_STATUS_FAILED], [0, 1, 2, 3])
    def test_private_job_extension_signature(self) -> None:
        diagnostic = [ctypes.POINTER(ctypes.c_char), ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
        self.assertEqual(_native.ams_mel_rf_job_extend_event.argtypes,
                         [_native.RfJobHandle, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_int64, *diagnostic])
        self.assertIs(_native.ams_mel_rf_job_extend_event.restype, ctypes.c_int32)

    def test_private_interval_status_signatures(self) -> None:
        diagnostic = [ctypes.POINTER(ctypes.c_char), ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
        self.assertEqual(_native.ams_mel_rf_job_interval_status_open.argtypes, [_native.RfJobHandle, ctypes.POINTER(_native.RfJobIntervalStatusOptionsV1), ctypes.POINTER(_native.RfJobIntervalStatusHandle), *diagnostic])
        self.assertIs(_native.ams_mel_rf_job_interval_status_open.restype, ctypes.c_int32)
        self.assertEqual(_native.ams_mel_rf_job_interval_status_receive.argtypes, [_native.RfJobIntervalStatusHandle, ctypes.c_uint32, ctypes.POINTER(_native.RfJobIntervalStatusEventHandle), *diagnostic])
        self.assertIs(_native.ams_mel_rf_job_interval_status_receive.restype, ctypes.c_int32)
        self.assertEqual(_native.ams_mel_rf_job_interval_status_get_counters.argtypes, [_native.RfJobIntervalStatusHandle, ctypes.POINTER(_native.RfJobIntervalStatusCountersV1), *diagnostic])
        self.assertIs(_native.ams_mel_rf_job_interval_status_get_counters.restype, ctypes.c_int32)
        self.assertEqual(_native.ams_mel_rf_job_interval_status_close.argtypes, [ctypes.POINTER(_native.RfJobIntervalStatusHandle), *diagnostic])
        self.assertIs(_native.ams_mel_rf_job_interval_status_close.restype, ctypes.c_int32)
        self.assertEqual(_native.ams_mel_rf_job_interval_status_event_view.argtypes, [_native.RfJobIntervalStatusEventHandle, ctypes.POINTER(ctypes.POINTER(_native.RfJobIntervalStatusV1)), *diagnostic])
        self.assertIs(_native.ams_mel_rf_job_interval_status_event_view.restype, ctypes.c_int32)
        self.assertEqual(_native.ams_mel_rf_job_interval_status_event_close.argtypes, [ctypes.POINTER(_native.RfJobIntervalStatusEventHandle), *diagnostic])
        self.assertIs(_native.ams_mel_rf_job_interval_status_event_close.restype, ctypes.c_int32)
        self.assertEqual(_native.ams_mel_rf_job_add_rx_intervals_v2.argtypes, [_native.RfJobHandle, _native.RfJobIntervalConfigSpanV2, *diagnostic])
        self.assertIs(_native.ams_mel_rf_job_add_rx_intervals_v2.restype, ctypes.c_int32)

    def test_private_tx_power_mode_signatures(self) -> None:
        diagnostic = [ctypes.POINTER(ctypes.c_char), ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
        owner = ctypes.POINTER(_native.RfTxPowerModeSnapshotHandle)
        self.assertEqual(_native.ams_mel_rf_data_get_tx_power_modes.argtypes,
                         [_native.RfDataHandle, ctypes.c_uint32, owner, *diagnostic])
        self.assertEqual(_native.ams_mel_rf_data_get_tx_power_mode.argtypes,
                         [_native.RfDataHandle, ctypes.c_uint32, ctypes.c_uint32, owner, *diagnostic])
        self.assertEqual(_native.ams_mel_rf_tx_power_mode_snapshot_view.argtypes,
                         [_native.RfTxPowerModeSnapshotHandle, ctypes.POINTER(_native.RfTxPowerModeSpanV1), *diagnostic])
        self.assertEqual(_native.ams_mel_rf_tx_power_mode_snapshot_close.argtypes, [owner, *diagnostic])
        for operation in (_native.ams_mel_rf_data_get_tx_power_modes, _native.ams_mel_rf_data_get_tx_power_mode,
                          _native.ams_mel_rf_tx_power_mode_snapshot_view, _native.ams_mel_rf_tx_power_mode_snapshot_close):
            self.assertIs(operation.restype, ctypes.c_int32)

    def test_private_physical_data_signatures(self) -> None:
        self.assertEqual(_native.ams_mel_rf_data_get_physical_data.argtypes,
                         [_native.RfDataHandle, ctypes.c_uint32,
                          ctypes.POINTER(_native.RfPhysicalDataHandle), ctypes.POINTER(ctypes.c_char),
                          ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)])
        self.assertEqual(_native.ams_mel_rf_physical_data_view.argtypes,
                         [_native.RfPhysicalDataHandle,
                          ctypes.POINTER(ctypes.POINTER(_native.RfPhysicalDataV1)), ctypes.POINTER(ctypes.c_char),
                          ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)])
        self.assertEqual(_native.ams_mel_rf_physical_data_close.argtypes,
                         [ctypes.POINTER(_native.RfPhysicalDataHandle), ctypes.POINTER(ctypes.c_char),
                          ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)])
        for operation in (_native.ams_mel_rf_data_get_physical_data,
                          _native.ams_mel_rf_physical_data_view,
                          _native.ams_mel_rf_physical_data_close):
            self.assertEqual(operation.restype, ctypes.c_int32)

    def test_reports_exact_facade_version(self) -> None:
        self.assertEqual(abi_version(), AbiVersion(major=0, minor=1))

    def test_private_layer_binds_complete_current_facade(self) -> None:
        self.assertEqual(
            _native.BOUND_FUNCTION_NAMES,
            (
                "ams_mel_rf_virtual_aperture_submit_job_v4",
                "ams_mel_rf_virtual_aperture_submit_job_v3",
                "ams_mel_rf_virtual_aperture_submit_job_v2",
                "ams_mel_rf_virtual_aperture_get_tx_radiated_power",
                "ams_mel_rf_virtual_aperture_get_tx_peak_radiated_power",
                "ams_mel_rf_virtual_aperture_get_tx_aperture_gain",
                "ams_mel_rf_virtual_aperture_get_max_tx_attenuation",
                "ams_mel_rf_virtual_aperture_get_data_pipes",
                "ams_mel_rf_va_data_pipe_connections_snapshot_view",
                "ams_mel_rf_va_data_pipe_connections_snapshot_close",
                "ams_mel_rf_virtual_aperture_associate_data_pipe_endpoint",
                "ams_mel_rf_virtual_aperture_associate_data_pipe_endpoints",
                "ams_mel_rf_virtual_aperture_is_cached_waveform_supported",
                "ams_mel_rf_virtual_aperture_dynamic_weights_supported",
                "ams_mel_rf_virtual_aperture_get_local_functions",
                "ams_mel_rf_va_local_function_list_view",
                "ams_mel_rf_va_local_function_list_close",
                "ams_mel_rf_virtual_aperture_get_local_function_status",
                "ams_mel_rf_va_local_function_status_view",
                "ams_mel_rf_va_local_function_status_close",
                "ams_mel_rf_virtual_aperture_get_element_groups",
                "ams_mel_rf_element_group_snapshot_view",
                "ams_mel_rf_element_group_snapshot_close",
                "ams_mel_rf_va_status_subscription_open",
                "ams_mel_rf_va_status_subscription_wait",
                "ams_mel_rf_va_status_subscription_get_statistics",
                "ams_mel_rf_va_status_subscription_unsubscribe",
                "ams_mel_rf_va_status_subscription_close",
                "ams_mel_get_abi_version",
                "ams_mel_session_open",
                "ams_mel_session_open_with_options",
                "ams_mel_session_get_provider_version",
                "ams_mel_session_close",
                "ams_mel_ir_stream_open",
                "ams_mel_ir_stream_start",
                "ams_mel_ir_stream_receive",
                "ams_mel_ir_stream_get_capabilities",
                "ams_mel_ir_image_metadata_open",
                "ams_mel_ir_image_metadata_receive",
                "ams_mel_ir_image_metadata_get_counters",
                "ams_mel_ir_image_metadata_close",
                "ams_mel_ir_image_metadata_event_view",
                "ams_mel_ir_image_metadata_event_close",
                "ams_mel_ir_stream_submit_navigation_report",
                "ams_mel_ir_navigation_request_wait",
                "ams_mel_ir_navigation_request_close",
                "ams_mel_ir_stream_receive_snapshot",
                "ams_mel_ir_frame_snapshot_view",
                "ams_mel_ir_frame_snapshot_close",
                "ams_mel_ir_stream_get_counters",
                "ams_mel_ir_stream_stop",
                "ams_mel_ir_stream_close",
                "ams_mel_ir_c2_open",
                "ams_mel_ir_c2_enable",
                "ams_mel_ir_c2_submit_operate",
                "ams_mel_ir_c2_submit_mode",
                "ams_mel_ir_mode_request_wait",
                "ams_mel_ir_mode_request_close",
                "ams_mel_ir_c2_submit_bit_noop",
                "ams_mel_ir_c2_submit_bit",
                "ams_mel_ir_c2_submit_config_set",
                "ams_mel_ir_c2_send_keepalive",
                "ams_mel_ir_c2_submit_comms_test",
                "ams_mel_ir_channel_comms_request_wait",
                "ams_mel_ir_channel_comms_request_close",
                "ams_mel_ir_c2_get_capabilities",
                "ams_mel_ir_channel_capability_view",
                "ams_mel_ir_channel_capability_close",
                "ams_mel_ir_return_request_wait",
                "ams_mel_ir_return_request_close",
                "ams_mel_ir_c2_close",
                "ams_mel_ir_c2_metadata_open",
                "ams_mel_ir_c2_metadata_register_comms_test",
                "ams_mel_ir_c2_metadata_receive",
                "ams_mel_ir_c2_metadata_get_counters",
                "ams_mel_ir_c2_metadata_close",
                "ams_mel_ir_c2_metadata_event_view",
                "ams_mel_ir_c2_metadata_event_close",
                "ams_mel_ir_health_open",
                "ams_mel_ir_health_enable",
                "ams_mel_ir_health_get_capabilities",
                "ams_mel_ir_health_close",
                "ams_mel_ir_health_metadata_open",
                "ams_mel_ir_health_metadata_receive",
                "ams_mel_ir_health_metadata_get_counters",
                "ams_mel_ir_health_metadata_close",
                "ams_mel_ir_health_metadata_event_view",
                "ams_mel_ir_health_metadata_event_close",
                "ams_mel_ir_instrumentation_open",
                "ams_mel_ir_instrumentation_enable",
                "ams_mel_ir_instrumentation_get_capabilities",
                "ams_mel_ir_instrumentation_submit_level",
                "ams_mel_ir_instrumentation_request_wait",
                "ams_mel_ir_instrumentation_request_close",
                "ams_mel_ir_instrumentation_metadata_open",
                "ams_mel_ir_instrumentation_metadata_receive",
                "ams_mel_ir_instrumentation_metadata_get_counters",
                "ams_mel_ir_instrumentation_metadata_close",
                "ams_mel_ir_instrumentation_metadata_event_view",
                "ams_mel_ir_instrumentation_metadata_event_close",
                "ams_mel_ir_instrumentation_close",
                "ams_mel_ir_track_open",
                "ams_mel_ir_track_enable",
                "ams_mel_ir_track_get_capabilities",
                "ams_mel_ir_track_close",
                "ams_mel_ir_track_metadata_open",
                "ams_mel_ir_track_metadata_receive",
                "ams_mel_ir_track_metadata_get_counters",
                "ams_mel_ir_track_metadata_close",
                "ams_mel_ir_track_metadata_event_view",
                "ams_mel_ir_track_metadata_event_view_v2",
                "ams_mel_ir_track_metadata_event_view_v3",
                "ams_mel_ir_track_metadata_event_close",
                "ams_mel_ir_track_submit_update",
                "ams_mel_ir_track_update_request_wait",
                "ams_mel_ir_track_update_request_close",
                "ams_mel_ir_track_submit_system_track_data_response",
                "ams_mel_ir_track_system_response_request_wait",
                "ams_mel_ir_track_system_response_request_close",
                "ams_mel_ir_channel_from_c2",
                "ams_mel_ir_channel_from_stream",
                "ams_mel_ir_channel_from_health",
                "ams_mel_ir_channel_from_instrumentation",
                "ams_mel_ir_channel_from_track",
                "ams_mel_ir_channel_send_keepalive",
                "ams_mel_ir_channel_submit_comms_test",
                "ams_mel_ir_channel_get_capabilities",
                "ams_mel_ir_channel_close",
                "ams_mel_rf_admin_open",
                "ams_mel_rf_admin_command_state",
                "ams_mel_rf_admin_close",
                "ams_mel_rf_c2_open",
                "ams_mel_rf_c2_close",
                "ams_mel_rf_c2_submit_virtual_aperture",
                "ams_mel_rf_virtual_aperture_request_wait",
                "ams_mel_rf_virtual_aperture_request_claim",
                "ams_mel_rf_virtual_aperture_request_close",
                "ams_mel_rf_virtual_aperture_view",
                "ams_mel_rf_virtual_aperture_close",
                "ams_mel_rf_virtual_aperture_get_id",
                "ams_mel_rf_virtual_aperture_get_status",
                "ams_mel_rf_virtual_aperture_get_instance_status",
                "ams_mel_rf_virtual_aperture_get_all_instances",
                "ams_mel_rf_virtual_aperture_get_instances",
                "ams_mel_rf_va_instance_list_view",
                "ams_mel_rf_va_instance_list_close",
                "ams_mel_rf_virtual_aperture_get_instance_status_report",
                "ams_mel_rf_va_instance_status_report_view",
                "ams_mel_rf_va_instance_status_report_close",
                "ams_mel_rf_virtual_aperture_submit_job",
                "ams_mel_rf_job_request_wait",
                "ams_mel_rf_job_request_claim",
                "ams_mel_rf_job_request_close",
                "ams_mel_rf_job_view",
                "ams_mel_rf_job_add_rx_intervals",
                "ams_mel_rf_job_interval_status_open",
                "ams_mel_rf_job_interval_status_receive",
                "ams_mel_rf_job_interval_status_get_counters",
                "ams_mel_rf_job_interval_status_close",
                "ams_mel_rf_job_interval_status_event_view",
                "ams_mel_rf_job_interval_status_event_close",
                "ams_mel_rf_job_add_rx_intervals_v2",
                "ams_mel_rf_job_add_rx_intervals_v3",

                "ams_mel_rf_job_flush",
                "ams_mel_rf_job_cancel_remaining_intervals",
                "ams_mel_rf_job_extend_event",
                "ams_mel_rf_job_finalize",
                "ams_mel_rf_job_wait_status",
                "ams_mel_rf_job_cancel",
                "ams_mel_rf_job_close",
                "ams_mel_rf_data_open",
                "ams_mel_rf_data_get_provider_version",
                "ams_mel_rf_data_get_mfa_info",
                "ams_mel_rf_data_quantize_duration",
                "ams_mel_rf_data_get_physical_data",
                "ams_mel_rf_data_get_tx_power_modes",
                "ams_mel_rf_data_get_tx_power_mode",
                "ams_mel_rf_tx_power_mode_snapshot_view",
                "ams_mel_rf_tx_power_mode_snapshot_close",
                "ams_mel_rf_physical_data_view",
                "ams_mel_rf_physical_data_close",
                "ams_mel_rf_mfa_info_view",
                "ams_mel_rf_mfa_info_close",
                "ams_mel_rf_data_close",
                "ams_mel_rf_data_submit_product_rx",
                "ams_mel_rf_product_rx_request_wait",
                "ams_mel_rf_product_rx_request_claim",
                "ams_mel_rf_product_rx_request_close",
                "ams_mel_rf_product_rx_receive",
                "ams_mel_rf_product_rx_get_counters",
                "ams_mel_rf_product_rx_close",
                "ams_mel_rf_product_rx_event_view",
                "ams_mel_rf_product_rx_event_close",
            ),
        )
        self.assertEqual(len(_native.BOUND_FUNCTION_NAMES), 193)
        self.assertEqual(len(set(_native.BOUND_FUNCTION_NAMES)), 193)
        repository = Path(__file__).resolve().parents[2]
        exports = (repository / "native/src/exports.map").read_text(encoding="utf-8")
        exported = sorted(
            line.strip().rstrip(";")
            for line in exports.splitlines()
            if line.strip().startswith("ams_mel_")
        )
        self.assertEqual(len(exported), 193)
        self.assertEqual(sorted(_native.BOUND_FUNCTION_NAMES), exported)
        for name in _native.BOUND_FUNCTION_NAMES:
            function = getattr(_native, name)
            self.assertIsNotNone(function.argtypes)
            self.assertIs(function.restype, ctypes.c_int32)

    def test_private_common_channel_signatures_are_exact(self) -> None:
        """Task 032B1: private raw ctypes shapes of the nine Channel exports."""
        diagnostic = [
            _native.CharPointer, ctypes.c_size_t, _native.SizePointer
        ]
        handle = _native.IrChannelHandle
        expected = {
            "ams_mel_ir_channel_from_c2": [_native.IrC2Handle],
            "ams_mel_ir_channel_from_stream": [_native.IrStreamHandle],
            "ams_mel_ir_channel_from_health": [_native.IrHealthHandle],
            "ams_mel_ir_channel_from_instrumentation": [
                _native.IrInstrumentationHandle
            ],
            "ams_mel_ir_channel_from_track": [_native.IrTrackHandle],
        }
        for name, source in expected.items():
            function = getattr(_native, name)
            self.assertEqual(
                function.argtypes,
                [*source, ctypes.POINTER(handle), *diagnostic],
                name,
            )
            self.assertIs(function.restype, ctypes.c_int32)
        self.assertEqual(
            _native.ams_mel_ir_channel_send_keepalive.argtypes,
            [handle, ctypes.POINTER(_native.IrReturnRequestHandle), *diagnostic],
        )
        self.assertEqual(
            _native.ams_mel_ir_channel_submit_comms_test.argtypes,
            [
                handle,
                ctypes.POINTER(_native.IrChannelCommsTestRequestV1),
                ctypes.POINTER(_native.IrChannelCommsRequestHandle),
                *diagnostic,
            ],
        )
        self.assertEqual(
            _native.ams_mel_ir_channel_get_capabilities.argtypes,
            [
                handle,
                ctypes.POINTER(_native.IrChannelCapabilityHandle),
                *diagnostic,
            ],
        )
        self.assertEqual(
            _native.ams_mel_ir_channel_close.argtypes,
            [ctypes.POINTER(handle), *diagnostic],
        )
        for name in (
            "ams_mel_ir_channel_send_keepalive",
            "ams_mel_ir_channel_submit_comms_test",
            "ams_mel_ir_channel_get_capabilities",
            "ams_mel_ir_channel_close",
        ):
            self.assertIs(getattr(_native, name).restype, ctypes.c_int32)

    def test_private_rf_admin_signatures_are_exact(self) -> None:
        diagnostic = [_native.CharPointer, ctypes.c_size_t, _native.SizePointer]
        handle = _native.RfAdminHandle
        expected = {
            "ams_mel_rf_admin_open": [ctypes.c_char_p, ctypes.c_char_p,
                                        ctypes.POINTER(handle)],
            "ams_mel_rf_admin_command_state": [handle, ctypes.c_uint32,
                                                 ctypes.POINTER(ctypes.c_uint32)],
            "ams_mel_rf_admin_close": [ctypes.POINTER(handle)],
        }
        for name, prefix in expected.items():
            function = getattr(_native, name)
            self.assertEqual(function.argtypes, [*prefix, *diagnostic], name)
            self.assertIs(function.restype, ctypes.c_int32)

    def test_private_rf_c2_signatures_are_exact(self) -> None:
        diagnostic = [_native.CharPointer, ctypes.c_size_t, _native.SizePointer]
        handle = _native.RfC2Handle
        expected = {
            "ams_mel_rf_c2_open": [ctypes.c_char_p, ctypes.c_char_p,
                                    ctypes.POINTER(handle)],
            "ams_mel_rf_c2_close": [ctypes.POINTER(handle)],
        }
        for name, prefix in expected.items():
            function = getattr(_native, name)
            self.assertEqual(function.argtypes, [*prefix, *diagnostic], name)
            self.assertIs(function.restype, ctypes.c_int32)

    def test_private_rf_va_signatures_are_exact(self) -> None:
        diagnostic = [_native.CharPointer, ctypes.c_size_t, _native.SizePointer]
        request = _native.RfVaRequestHandle
        va = _native.RfVaHandle
        expected = {
            "ams_mel_rf_c2_submit_virtual_aperture": [_native.RfC2Handle,
                ctypes.POINTER(_native.RfVaConfigV1), ctypes.POINTER(request)],
            "ams_mel_rf_virtual_aperture_request_wait": [request, ctypes.c_uint32,
                ctypes.POINTER(_native.RfVaResultV1)],
            "ams_mel_rf_virtual_aperture_request_claim": [request, ctypes.POINTER(va)],
            "ams_mel_rf_virtual_aperture_request_close": [ctypes.POINTER(request)],
            "ams_mel_rf_virtual_aperture_view": [va, ctypes.POINTER(ctypes.POINTER(_native.RfVaInfoV1))],
            "ams_mel_rf_virtual_aperture_close": [ctypes.POINTER(va)],
        }
        for name, prefix in expected.items():
            function = getattr(_native, name)
            self.assertEqual(function.argtypes, [*prefix, *diagnostic], name)
            self.assertIs(function.restype, ctypes.c_int32)

    def test_private_rf_job_signatures_are_exact(self) -> None:
        diagnostic = [_native.CharPointer, ctypes.c_size_t, _native.SizePointer]
        request, job = _native.RfJobRequestHandle, _native.RfJobHandle
        expected = {
            "ams_mel_rf_virtual_aperture_submit_job": [_native.RfVaHandle,
                ctypes.POINTER(_native.RfJobRequestConfigV1), ctypes.POINTER(request)],
            "ams_mel_rf_job_request_wait": [request, ctypes.c_uint32,
                ctypes.POINTER(_native.RfJobResultV1)],
            "ams_mel_rf_job_request_claim": [request, ctypes.POINTER(job)],
            "ams_mel_rf_job_request_close": [ctypes.POINTER(request)],
            "ams_mel_rf_job_view": [job, ctypes.POINTER(ctypes.POINTER(_native.RfJobInfoV1))],
            "ams_mel_rf_job_add_rx_intervals": [job, _native.RfJobIntervalConfigSpanV1],
            "ams_mel_rf_job_flush": [job],
            "ams_mel_rf_job_cancel_remaining_intervals": [job],
            "ams_mel_rf_job_finalize": [job],
            "ams_mel_rf_job_wait_status": [job, ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32)],
            "ams_mel_rf_job_cancel": [job, ctypes.POINTER(_native.RfJobCancelResultV1)],
            "ams_mel_rf_job_close": [ctypes.POINTER(job)],
        }
        for name, prefix in expected.items():
            function = getattr(_native, name)
            self.assertEqual(function.argtypes, [*prefix, *diagnostic], name)
            self.assertIs(function.restype, ctypes.c_int32)

    def test_private_rf_data_signatures_are_exact(self) -> None:
        """Task 033B: private raw ctypes shapes of the six RF DataMEL exports."""
        diagnostic = [
            _native.CharPointer, ctypes.c_size_t, _native.SizePointer
        ]
        data = _native.RfDataHandle
        info = _native.RfMfaInfoHandle
        expected = {
            "ams_mel_rf_data_open": [
                ctypes.c_char_p, ctypes.c_char_p, ctypes.POINTER(data)
            ],
            "ams_mel_rf_data_get_provider_version": [
                data, ctypes.POINTER(_native.ProviderVersionV1)
            ],
            "ams_mel_rf_data_get_mfa_info": [data, ctypes.POINTER(info)],
            "ams_mel_rf_data_quantize_duration": [
                data, ctypes.c_int64, ctypes.POINTER(ctypes.c_int64)
            ],
            "ams_mel_rf_mfa_info_view": [
                info, ctypes.POINTER(ctypes.POINTER(_native.RfMfaInfoV1))
            ],
            "ams_mel_rf_mfa_info_close": [ctypes.POINTER(info)],
            "ams_mel_rf_data_close": [ctypes.POINTER(data)],
        }
        for name, prefix in expected.items():
            function = getattr(_native, name)
            self.assertEqual(function.argtypes, [*prefix, *diagnostic], name)
            self.assertIs(function.restype, ctypes.c_int32)
        self.assertEqual(
            [
                getattr(_native, f"AMS_MEL_RF_JOB_DATA_FORMAT_{suffix}")
                for suffix in (
                    "DIRECT_INT8", "DIRECT_INT16", "COMPLEX_INT8", "COMPLEX_INT16",
                    "AMS_VITA_SMALL", "AMS_VITA_MEDIUM", "AMS_VITA_LARGE",
                    "AMS_VITA_EXTRA_LARGE", "PDW_TYPE1", "PDW_TYPE2", "PDW_TYPE3",
                    "LF_TYPE1", "LF_TYPE2", "LF_TYPE3",
                )
            ],
            list(range(14)),
        )

    def test_private_rf_product_rx_signatures_are_exact(self) -> None:
        """Task 033D: private raw ctypes shapes of the nine ProductRx exports."""
        diagnostic = [
            _native.CharPointer, ctypes.c_size_t, _native.SizePointer
        ]
        data = _native.RfDataHandle
        request = _native.RfProductRxRequestHandle
        endpoint = _native.RfProductRxHandle
        event = _native.RfProductRxEventHandle
        expected = {
            "ams_mel_rf_data_submit_product_rx": [
                data, ctypes.POINTER(_native.RfProductRxConfigV1), ctypes.POINTER(request)
            ],
            "ams_mel_rf_product_rx_request_wait": [
                request, ctypes.c_uint32, ctypes.POINTER(_native.RfProductRxRequestResultV1)
            ],
            "ams_mel_rf_product_rx_request_claim": [
                request, ctypes.POINTER(endpoint), ctypes.POINTER(_native.RfProductRxInfoV1)
            ],
            "ams_mel_rf_product_rx_request_close": [ctypes.POINTER(request)],
            "ams_mel_rf_product_rx_receive": [
                endpoint, ctypes.c_uint32, ctypes.POINTER(event)
            ],
            "ams_mel_rf_product_rx_get_counters": [
                endpoint, ctypes.POINTER(_native.RfProductRxCountersV1)
            ],
            "ams_mel_rf_product_rx_close": [ctypes.POINTER(endpoint)],
            "ams_mel_rf_product_rx_event_view": [
                event, ctypes.POINTER(ctypes.POINTER(_native.RfProductRxEventV1))
            ],
            "ams_mel_rf_product_rx_event_close": [ctypes.POINTER(event)],
        }
        for name, prefix in expected.items():
            function = getattr(_native, name)
            self.assertEqual(function.argtypes, [*prefix, *diagnostic], name)
            self.assertIs(function.restype, ctypes.c_int32)

    def test_private_rf_product_rx_null_preconditions(self) -> None:
        request = _native.RfProductRxRequestHandle()
        endpoint = _native.RfProductRxHandle()
        event = _native.RfProductRxEventHandle()
        config = _native.RfProductRxConfigV1(
            _native.AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT16, 4096, 4, 64
        )
        self.assertEqual(
            _native.ams_mel_rf_data_submit_product_rx(
                None, ctypes.byref(config), ctypes.byref(request), None, 0, None
            ),
            _native.AMS_MEL_INVALID_ARGUMENT,
        )
        self.assertIsNone(request.value)
        self.assertEqual(
            _native.ams_mel_rf_product_rx_receive(None, 0, ctypes.byref(event), None, 0, None),
            _native.AMS_MEL_INVALID_ARGUMENT,
        )
        self.assertIsNone(event.value)
        for close in (
            (_native.ams_mel_rf_product_rx_request_close, request),
            (_native.ams_mel_rf_product_rx_close, endpoint),
            (_native.ams_mel_rf_product_rx_event_close, event),
        ):
            self.assertEqual(close[0](ctypes.byref(close[1]), None, 0, None), _native.AMS_MEL_OK)

    def test_private_rf_data_null_preconditions(self) -> None:
        data = _native.RfDataHandle()
        info = _native.RfMfaInfoHandle()
        self.assertEqual(
            _native.ams_mel_rf_data_open(None, b"", ctypes.byref(data), None, 0, None),
            _native.AMS_MEL_INVALID_ARGUMENT,
        )
        self.assertIsNone(data.value)
        self.assertEqual(
            _native.ams_mel_rf_data_get_mfa_info(None, ctypes.byref(info), None, 0, None),
            _native.AMS_MEL_INVALID_ARGUMENT,
        )
        self.assertIsNone(info.value)
        self.assertEqual(
            _native.ams_mel_rf_mfa_info_close(ctypes.byref(info), None, 0, None),
            _native.AMS_MEL_OK,
        )
        self.assertEqual(
            _native.ams_mel_rf_data_close(ctypes.byref(data), None, 0, None),
            _native.AMS_MEL_OK,
        )

    def test_private_common_channel_null_preconditions(self) -> None:
        channel = _native.IrChannelHandle()
        self.assertEqual(
            _native.ams_mel_ir_channel_from_c2(
                None, ctypes.byref(channel), None, 0, None
            ),
            _native.AMS_MEL_INVALID_ARGUMENT,
        )
        self.assertIsNone(channel.value)
        self.assertEqual(
            _native.ams_mel_ir_channel_close(
                ctypes.byref(channel), None, 0, None
            ),
            _native.AMS_MEL_OK,
        )
        self.assertIsNone(channel.value)

    def test_ctypes_declarations_match_authoritative_c_header(self) -> None:
        repository = Path(__file__).resolve().parents[2]
        native_library = Path(os.environ["AMS_MEL_NATIVE_LIB"]).resolve()
        compiler = os.environ.get("CC", "cc")
        with tempfile.TemporaryDirectory(prefix="ams-mel-python-abi-") as directory:
            probe = Path(directory) / "abi_probe"
            compile_result = subprocess.run(
                [
                    compiler,
                    "-std=c11",
                    "-Wall",
                    "-Wextra",
                    "-Wpedantic",
                    "-Werror",
                    f"-I{repository / 'native/include'}",
                    str(Path(__file__).with_name("abi_probe.c")),
                    str(native_library),
                    f"-Wl,-rpath,{native_library.parent}",
                    "-o",
                    str(probe),
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(compile_result.returncode, 0, compile_result.stderr)
            probe_result = subprocess.run(
                [str(probe)], check=False, capture_output=True, text=True
            )
            self.assertEqual(probe_result.returncode, 0, probe_result.stderr)

        actual = [int(value) for value in probe_result.stdout.split()]
        expected = [
            _native.AMS_MEL_OK,
            _native.AMS_MEL_INVALID_ARGUMENT,
            _native.AMS_MEL_LIBRARY_LOAD_FAILED,
            _native.AMS_MEL_SYMBOL_NOT_FOUND,
            _native.AMS_MEL_FACTORY_FAILED,
            _native.AMS_MEL_INITIALIZATION_FAILED,
            _native.AMS_MEL_PROVIDER_EXCEPTION,
            _native.AMS_MEL_BUFFER_TOO_SMALL,
            _native.AMS_MEL_INTERNAL_ERROR,
            _native.AMS_MEL_TIMEOUT,
            _native.AMS_MEL_STREAM_STOPPED,
            _native.AMS_MEL_PROVIDER_FAILED,
            _native.AMS_MEL_COMMAND_REJECTED,
            _native.AMS_MEL_RESOURCE_EXHAUSTED,
            _native.AMS_MEL_IR_CHANNEL_IRST_IMAGE,
            _native.AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL,
            _native.AMS_MEL_IR_MFA_MODE_UNUSED,
            _native.AMS_MEL_IR_MFA_MODE_TASK_SCHED,
            _native.AMS_MEL_IR_MFA_MODE_SCAN_VOLUME_SCHED,
            _native.AMS_MEL_IR_MFA_MODE_SCAN_BAR_SCHED,
            *range(_native.AMS_MEL_IR_MFA_STATE_NOT_SET,
                   _native.AMS_MEL_IR_MFA_STATE_MAX_EXCLUSIVE + 1),
            _native.AMS_MEL_IR_COORD_FRAME_INERTIAL,
            _native.AMS_MEL_IR_COORD_FRAME_AIRCRAFT,
            _native.AMS_MEL_IR_DEGRADATION_CAPACITY,
            _native.AMS_MEL_IR_DEGRADATION_VOLUME,
            _native.AMS_MEL_IR_DEGRADATION_RANGE,
            _native.AMS_MEL_IR_DEGRADATION_REVISIT,
            _native.AMS_MEL_IR_RETURN_SUCCESS,
            _native.AMS_MEL_IR_RETURN_BAD_POINTER,
            _native.AMS_MEL_IR_RETURN_FAIL,
            _native.AMS_MEL_IR_RETURN_NOT_SUPPORTED,
            _native.AMS_MEL_IR_RETURN_NOT_IMPLEMENTED,
            _native.AMS_MEL_ERROR_NONE,
            _native.AMS_MEL_ERROR_INVALID_ID,
            _native.AMS_MEL_ERROR_INVALID_STATE,
            _native.AMS_MEL_ERROR_INVALID_PARAMETERS,
            _native.AMS_MEL_ERROR_INSUFFICIENT_PERMISSIONS,
            _native.AMS_MEL_ERROR_INSUFFICIENT_RESOURCES,
            _native.AMS_MEL_ERROR_INSUFFICIENT_LOCAL_RESOURCES,
            _native.AMS_MEL_ERROR_INSUFFICIENT_REMOTE_RESOURCES,
            _native.AMS_MEL_ERROR_UNSUPPORTED,
            _native.AMS_MEL_IR_PIXEL_MONO,
            _native.AMS_MEL_IR_IMAGE_STARING,
            _native.AMS_MEL_IR_IMAGE_SCANNING,
            _native.AMS_MEL_IR_FLIP_NONE,
            _native.AMS_MEL_IR_FLIP_VERTICAL,
            _native.AMS_MEL_IR_FLIP_HORIZONTAL,
            _native.AMS_MEL_IR_FLIP_BOTH,
        ]
        expected.extend(self._layout(_native.AbiVersionV1, "major", "minor"))
        expected.extend(self._layout(_native.SessionOptionsV1, "max_async_requests"))
        expected.extend(
            self._layout(
                _native.ProviderVersionV1,
                "api_version",
                "library_version",
                "vendor",
                "vendor_capacity",
                "vendor_required",
                "description",
                "description_capacity",
                "description_required",
            )
        )
        expected.extend(
            self._layout(_native.StringViewV1, "data", "size")
        )
        expected.extend(self._layout(_native.U32SpanV1, "data", "size"))
        expected.extend(self._layout(_native.StringViewSpanV1, "data", "size"))
        expected.extend(self._layout(_native.IrScanTypeV1,
            "continuous_scan", "returning", "agile_scan"))
        expected.extend(self._layout(_native.IrScanParamV1,
            "elevation_defined_with_range_and_altitude", "center_az_rad",
            "center_el_rad", "center_frame_ref_el", "center_frame_ref_az",
            "scan_width_rad", "scan_height_rad", "scan_type", "scan_id",
            "scan_rate_rad_per_second", "preferred_revisit_interval_seconds",
            "required_revisit_interval_seconds", "max_range_of_interest_m",
            "min_range_of_interest_m", "elevation_scan_center_altitude_m",
            "elevation_scan_center_range_m", "degradation_method"))
        expected.extend(self._layout(_native.IrModeCommandV1,
            "command_id", "state", "mode", "scan_parameters"))
        expected.extend(self._layout(_native.IrBitCommandV1,
            "command_id", "initiate_bit_ids", "cancel_bit_ids", "clear_fault_codes"))
        expected.extend(self._layout(_native.IrConfigSetCommandV1,
            "command_id", "system_time_ns", "config"))
        expected.extend(self._layout(_native.UciIdV1, "uuid", "descriptive_label"))
        expected.extend(
            self._layout(
                _native.ComponentLocationV1,
                "offset_x_m", "offset_y_m", "offset_z_m", "key", "system_name",
            )
        )
        expected.extend(
            self._layout(
                _native.IrStreamConfigV1,
                "channel_type", "channel_id", "platform_id", "sensor_location",
                "buffer_count", "buffer_size", "queue_capacity",
            )
        )
        expected.extend(
            self._layout(
                _native.IrC2ConfigV1,
                "channel_type", "channel_id", "platform_id", "sensor_location",
            )
        )
        expected.extend(
            self._layout(_native.IrModeResultV1, "mode", "error_code")
        )
        expected.extend(
            self._layout(_native.IrReturnResultV1, "value", "error_code")
        )
        expected.extend(
            self._layout(
                _native.IrFrameV1,
                "system_time_ns", "integration_time_ns", "width", "height",
                "bits_per_pixel", "number_of_bands", "horizontal_fov_rad",
                "vertical_fov_rad", "pixel_format", "frame_id", "subframe_id",
                "subframe_total", "image_type", "image_flip", "image_flags",
                "dither_row", "dither_column", "row_offset", "column_offset",
                "band_index", "reserved", "pixels", "pixel_capacity",
                "pixel_required",
            )
        )
        expected.extend(
            self._layout(
                _native.IrStreamCountersV1,
                "frames_received", "frames_dropped_queue_full",
                "malformed_or_unsupported_frames",
            )
        )
        expected.extend(self._layout(_native.U8SpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.IrContributingSensorV1, 'location', 'sensor_id'))
        expected.extend(self._layout(_native.IrDirectionalV1, 'x', 'y', 'z'))
        expected.extend(self._layout(_native.IrQuaternionV1, 'x', 'y', 'z', 'w'))
        expected.extend(self._layout(_native.IrNavErrorV1, 'x', 'y', 'z', 'w'))
        expected.extend(self._layout(_native.IrUncertaintyV1, 'sensor_uncertainties', 'platform_uncertainties'))
        expected.extend(self._layout(_native.IrOrientationV1, 'kind', 'euler', 'quaternion'))
        expected.extend(self._layout(_native.IrSensorInertialStateV1, 'system_time_ns', 'q_xyzw', 'q_ecef_xyzw', 'sensor_position', 'sensor_velocity', 'uncertainties'))
        expected.extend(self._layout(_native.IrSensorInertialStateSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.IrSensorNavStateV1, 'position', 'position_error', 'velocity', 'velocity_error', 'acceleration', 'acceleration_error', 'orientation', 'orientation_error', 'orientation_velocity', 'orientation_velocity_error', 'orientation_acceleration', 'orientation_acceleration_error', 'coordinate_system'))
        expected.extend(self._layout(_native.IrSensorNavStateSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.IrFrameSnapshotV1, 'system_time_ns', 'integration_time_ns', 'width', 'height', 'bits_per_pixel', 'number_of_bands', 'horizontal_fov_rad', 'vertical_fov_rad', 'contributing_sensor', 'pixel_format', 'frame_id', 'subframe_id', 'subframe_total', 'image_type', 'image_flip', 'image_flags', 'dither_row', 'dither_column', 'row_offset', 'column_offset', 'sensor_inertial_states', 'sensor_nav_states', 'band_index', 'pixels'))
        expected.extend([ctypes.sizeof(_native.IrFrameSnapshotHandle), ctypes.alignment(_native.IrFrameSnapshotHandle)])
        expected.extend([
            _native.AMS_MEL_IR_C2_METADATA_COMMAND_STATUS,
            _native.AMS_MEL_IR_C2_METADATA_BIT_CONFIGURATION,
            _native.AMS_MEL_IR_C2_METADATA_BIT_STATUS,
            _native.AMS_MEL_IR_IMAGE_METADATA_BAD_PIXEL_LIST,
            _native.AMS_MEL_IR_IMAGE_METADATA_LINE_OF_SIGHT_REPORT,
            _native.AMS_MEL_IR_IMAGE_METADATA_LINE_OF_SIGHT_EULER,
            _native.AMS_MEL_IR_IMAGE_METADATA_NAVIGATION_RESPONSE,
            _native.AMS_MEL_IR_BAD_PIXEL_REASON_UNKNOWN,
            _native.AMS_MEL_IR_COMMAND_NOT_SET,
            _native.AMS_MEL_IR_COMMAND_RECEIVED,
            _native.AMS_MEL_IR_COMMAND_ACCEPTED,
            _native.AMS_MEL_IR_COMMAND_REJECTED,
            _native.AMS_MEL_IR_COMMAND_CANCELLED,
            _native.AMS_MEL_IR_CANNOT_COMPLY_NOT_SET,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_ATTEMPTS,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_ENDURANCE,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_CLASSIFICATION,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_FOR_FOV_LIMIT,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_GATING,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_MANEUVER_LIMIT,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_OP,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_OCCLUSION,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_RANGE,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_PERFORMANCE,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_RF,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_ROUTE,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_SAFETY,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_TARGET_ANGLE,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_TIME,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_SYSTEM,
            _native.AMS_MEL_IR_CANNOT_COMPLY_INFEASIBLE_ROUTE,
            _native.AMS_MEL_IR_CANNOT_COMPLY_MISSION_EVENT,
            _native.AMS_MEL_IR_CANNOT_COMPLY_STATE_OR_SETTINGS,
            _native.AMS_MEL_IR_CANNOT_COMPLY_STATE_OR_SETTINGS_CHANGE,
            _native.AMS_MEL_IR_CANNOT_COMPLY_SYSTEM_UNAVAILABLE,
            _native.AMS_MEL_IR_CANNOT_COMPLY_SYSTEM_FAULT,
            _native.AMS_MEL_IR_CANNOT_COMPLY_SYSTEM_CONFLICT,
            _native.AMS_MEL_IR_CANNOT_COMPLY_SUBSYSTEM_UNAVAILABLE,
            _native.AMS_MEL_IR_CANNOT_COMPLY_SUBSYSTEM_FAULT,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_FAULT,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_PRECEDENCE,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_UNAVAILABLE,
            _native.AMS_MEL_IR_CANNOT_COMPLY_INSUFFICIENT_RESOURCES,
            _native.AMS_MEL_IR_CANNOT_COMPLY_RANKING,
            _native.AMS_MEL_IR_CANNOT_COMPLY_WEATHER,
            _native.AMS_MEL_IR_CANNOT_COMPLY_INELIGIBLE_CONTROL_SOURCE,
            _native.AMS_MEL_IR_CANNOT_COMPLY_DEPENDENCY_PREDECESSOR,
            _native.AMS_MEL_IR_CANNOT_COMPLY_DEPENDENCY_ALL_OR_NOTHING,
            _native.AMS_MEL_IR_CANNOT_COMPLY_DEPENDENCY_EITHER_OR,
            _native.AMS_MEL_IR_CANNOT_COMPLY_INIT_CRITERIA_NOT_MET,
            _native.AMS_MEL_IR_CANNOT_COMPLY_UNKNOWN_ID,
            _native.AMS_MEL_IR_CANNOT_COMPLY_INVALID_INPUT_PARAMETER,
            _native.AMS_MEL_IR_CANNOT_COMPLY_INPUT_OTHER,
            _native.AMS_MEL_IR_CANNOT_COMPLY_MDF_ACTIVATION_ERROR,
            _native.AMS_MEL_IR_CANNOT_COMPLY_MULTIPLE,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CANCELLED,
            _native.AMS_MEL_IR_CANNOT_COMPLY_OTHER,
            _native.AMS_MEL_IR_CANNOT_COMPLY_UNKNOWN,
            _native.AMS_MEL_IR_CANNOT_COMPLY_ABORTED,
            _native.AMS_MEL_IR_CANNOT_COMPLY_ALIGNMENT_MANEUVER,
            _native.AMS_MEL_BIT_CONTROL_NOT_SET,
            _native.AMS_MEL_BIT_CONTROL_SUBSYSTEM_BIT_COMMAND,
            _native.AMS_MEL_BIT_CONTROL_SUBSYSTEM_STATE_COMMAND,
            _native.AMS_MEL_BIT_CONTROL_SUBSYSTEM_INITIATED,
            _native.AMS_MEL_BIT_RESULT_NOT_SET,
            _native.AMS_MEL_BIT_RESULT_PASS,
            _native.AMS_MEL_BIT_RESULT_FAIL,
            _native.AMS_MEL_BIT_RESULT_INTERRUPTED,
            _native.AMS_MEL_BIT_RESULT_NOT_TESTED,
            _native.AMS_MEL_FAULT_SEVERITY_NOT_SET,
            _native.AMS_MEL_FAULT_SEVERITY_NOMINAL,
            _native.AMS_MEL_FAULT_SEVERITY_CAUTION,
            _native.AMS_MEL_FAULT_SEVERITY_WARNING,
            _native.AMS_MEL_FAULT_SEVERITY_FAILED,
            _native.AMS_MEL_FAULT_STATE_NOT_SET,
            _native.AMS_MEL_FAULT_STATE_SET,
            _native.AMS_MEL_FAULT_STATE_CLEARED,
            _native.AMS_MEL_FAULT_STATE_UNKNOWN,
        ])
        expected.extend(self._layout(_native.UciIdSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.IrCommandStatusV1, 'command_id', 'state', 'reason_id', 'reason_description'))
        expected.extend(self._layout(_native.BitTypeV1, 'bit_id', 'accepted_interface', 'bit_item_names', 'subsystem_component_ids', 'expected_duration_ns'))
        expected.extend(self._layout(_native.BitTypeSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.BitConfigurationV1, 'bit_types'))
        expected.extend(self._layout(_native.ActiveBitV1, 'bit_id', 'estimated_completion_time_ns', 'estimated_percent_complete'))
        expected.extend(self._layout(_native.ActiveBitSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.CompletedBitItemV1, 'bit_item_name', 'result', 'fail_reason'))
        expected.extend(self._layout(_native.CompletedBitItemSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.CompletedBitV1, 'bit_id', 'time_tag_ns', 'result', 'fail_reason', 'bit_items'))
        expected.extend(self._layout(_native.CompletedBitSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.FaultDataV1, 'key', 'value', 'format', 'units'))
        expected.extend(self._layout(_native.FaultDataSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.FaultAmbiguityGroupV1, 'diagnostic_test_ids', 'component_ids'))
        expected.extend(self._layout(_native.FaultAmbiguityGroupSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.FaultV1, 'fault_id', 'severity', 'state', 'fault_data', 'detection_time_ns', 'fault_code', 'fault_description', 'component_ids', 'ambiguity_groups'))
        expected.extend(self._layout(_native.FaultSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.BitStatusV1, 'active_bits', 'completed_bits', 'faults'))
        expected.extend(self._layout(_native.IrC2MetadataEventV1, 'kind', 'command_status', 'bit_configuration', 'bit_status', 'channel_comms_test'))
        expected.extend(self._layout(_native.IrC2MetadataCountersV1, 'events_received', 'events_dropped_queue_full', 'malformed_or_unsupported'))
        expected.extend(self._layout(_native.IrBadPixelV1, 'row', 'column', 'reason'))
        expected.extend(self._layout(_native.IrBadPixelSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.IrBadPixelListV1, 'reported_size', 'reported_count', 'pixels'))
        expected.extend(self._layout(_native.IrAzElV1, 'azimuth_rad', 'elevation_rad'))
        expected.extend(self._layout(_native.IrLineOfSightReportV1, 'system_time_ns', 'pointing_angle', 'pointing_angle_rates', 'at_speed', 'in_tolerance', 'platform_attitude', 'validity_flag_bitfield', 'image_rotation_rad'))
        expected.extend(self._layout(_native.IrLineOfSightEulerV1, 'system_time_ns', 'attitude', 'attitude_rates'))
        expected.extend(self._layout(_native.IrNavigationResponseV1, 'system_time_ns', 'command_id', 'request_id'))
        expected.extend(self._layout(_native.IrImageMetadataEventV1, 'kind', 'bad_pixel_list', 'line_of_sight_report', 'line_of_sight_euler', 'navigation_response'))
        expected.extend(self._layout(_native.IrChannelCommsTestReportV1, 'command_id', 'request_id'))
        expected.extend(self._layout(_native.IrChannelCommsTestRequestV1, 'command_id', 'channel_id', 'request_id'))
        expected.extend(self._layout(_native.IrChannelCommsTestResultV1, 'command_id', 'request_id', 'error_code'))
        expected.extend(self._layout(_native.IrBandInfoV1, 'kind', 'min_wavelength_m', 'max_wavelength_m'))
        expected.extend(self._layout(_native.IrBandInfoSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.IrImageBandV1, 'band_index', 'bands'))
        expected.extend(self._layout(_native.IrImageBandSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.IrChannelCapabilityV1, 'channel_id', 'height', 'width', 'bit_depth', 'row_pitch', 'buffer_size', 'image_size', 'number_of_bands', 'pixel_format', 'sensor_types', 'platform_id', 'sensor_location', 'channel_types', 'task_schedule_depth', 'odc_available', 'nuc_available', 'metadata_capabilities', 'image_bands', 'nav_frames'))
        expected.extend(self._layout(_native.IrHealthConfigV1, 'channel_id', 'channel_type', 'platform_id', 'sensor_location'))
        expected.extend(self._layout(_native.EulerV1, 'roll', 'pitch', 'yaw'))
        expected.extend(self._layout(_native.ForeignKeyV1, 'key', 'system_name'))
        expected.extend(self._layout(_native.InstallationDetailsV1, 'location', 'orientation', 'boresight'))
        expected.extend(self._layout(_native.TemperatureStatusV1, 'temperature_c', 'state'))
        expected.extend(self._layout(_native.MfaComponentV1, 'component_id', 'state', 'temperature', 'installation_location_id', 'installation_details'))
        expected.extend(self._layout(_native.MfaComponentSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.AboutV1, 'model', 'serial_number', 'software_version', 'bootloader_software_version', 'hardware_version'))
        expected.extend(self._layout(_native.MfaStatusV1, 'state', 'state_description', 'mode_description', 'transition_status', 'about', 'components'))
        expected.extend(self._layout(_native.IrSubsystemDepInfoV1, 'subsystem_id', 'criticality', 'failure'))
        expected.extend(self._layout(_native.IrSubsystemDepInfoSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.IrVersionV1, 'source', 'major_revision', 'minor_revision', 'engineering_revision'))
        expected.extend(self._layout(_native.IrSubsystemCsciInfoV1, 'csci', 'mode', 'version', 'criticality', 'failure', 'bit_report', 'connection_established'))
        expected.extend(self._layout(_native.IrSubsystemCsciInfoSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.IrSubsystemStatusV1, 'subsystem_id', 'criticality', 'status_sequence_number', 'failure', 'subsystem_count', 'subsystems', 'csci_count', 'csci'))
        expected.extend(self._layout(_native.NameValuePairV1, 'name', 'value'))
        expected.extend(self._layout(_native.NameValuePairSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.SecurityArtifactV1, 'component_id', 'associated_id'))
        expected.extend(self._layout(_native.SecurityArtifactSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.SecurityEventV1, 'kind', 'category', 'details', 'subsystem_id', 'service_id', 'mdf_id'))
        expected.extend(self._layout(_native.SecurityAuditRecordV1, 'security_event_id', 'event_timestamp_ns', 'subsystem_id', 'artifacts', 'event', 'outcome', 'severity'))
        expected.extend(self._layout(_native.IrHealthMetadataEventV1, 'kind', 'mfa_status', 'bit_status', 'subsystem_status', 'discrete_status', 'security_audit', 'mfa_status_detailed'))
        expected.extend([ctypes.sizeof(_native.IrC2MetadataHandle), ctypes.alignment(_native.IrC2MetadataHandle)])
        expected.extend([ctypes.sizeof(_native.IrC2MetadataEventHandle), ctypes.alignment(_native.IrC2MetadataEventHandle)])
        expected.extend([ctypes.sizeof(_native.IrImageMetadataHandle), ctypes.alignment(_native.IrImageMetadataHandle)])
        expected.extend([ctypes.sizeof(_native.IrImageMetadataEventHandle), ctypes.alignment(_native.IrImageMetadataEventHandle)])
        expected.extend([ctypes.sizeof(_native.IrChannelCommsRequestHandle), ctypes.alignment(_native.IrChannelCommsRequestHandle)])
        expected.extend([ctypes.sizeof(_native.IrChannelCapabilityHandle), ctypes.alignment(_native.IrChannelCapabilityHandle)])
        expected.extend([ctypes.sizeof(_native.IrHealthHandle), ctypes.alignment(_native.IrHealthHandle)])
        expected.extend([ctypes.sizeof(_native.IrHealthMetadataHandle), ctypes.alignment(_native.IrHealthMetadataHandle)])
        expected.extend([ctypes.sizeof(_native.IrHealthMetadataEventHandle), ctypes.alignment(_native.IrHealthMetadataEventHandle)])
        expected.extend([ctypes.sizeof(_native.IrNavigationRequestHandle), ctypes.alignment(_native.IrNavigationRequestHandle)])
        expected.extend(
            [
                _native.AMS_MEL_POSITION_SOLUTION_NOT_SET,
                _native.AMS_MEL_POSITION_SOLUTION_ALIGNING,
                _native.AMS_MEL_POSITION_SOLUTION_FREE_INERTIAL,
                _native.AMS_MEL_POSITION_SOLUTION_GPS,
                _native.AMS_MEL_POSITION_SOLUTION_BLENDED,
                _native.AMS_MEL_POSITION_SOLUTION_MAX_EXCLUSIVE,
            ]
        )
        expected.extend(self._layout(_native.NorthEastDownV1, 'north', 'east', 'down'))
        expected.extend(self._layout(_native.AttitudeRateV1, 'attitude_rate', 'attitude_rate_time_ns'))
        expected.extend(
            self._layout(
                _native.PositionVelocityCovarianceV1,
                'position_position_pn_pn',
                'position_position_pn_pe',
                'position_position_pn_pd',
                'position_position_pe_pe',
                'position_position_pe_pd',
                'position_position_pd_pd',
                'position_velocity_pn_vn',
                'position_velocity_pn_ve',
                'position_velocity_pn_vd',
                'position_velocity_pe_ve',
                'position_velocity_pe_vd',
                'position_velocity_pd_vd',
                'velocity_velocity_vn_vn',
                'velocity_velocity_vn_ve',
                'velocity_velocity_vn_vd',
                'velocity_velocity_ve_ve',
                'velocity_velocity_ve_vd',
                'velocity_velocity_vd_vd',
            )
        )
        expected.extend(
            self._layout(
                _native.NavigationReportV1,
                'system_time_ns',
                'state',
                'latitude_rad',
                'longitude_rad',
                'altitude_m',
                'attitude',
                'attitude_rate',
                'speed',
                'acceleration',
                'wander_angle_rad',
                'magnetic_heading',
                'altitude_msl',
                'position_velocity_covariance_uncertainty',
            )
        )
        expected.extend(self._layout(_native.IrNavigationResultV1, 'response', 'error_code'))
        expected.extend([ctypes.sizeof(_native.IrInstrumentationHandle), ctypes.alignment(_native.IrInstrumentationHandle)])
        expected.extend([ctypes.sizeof(_native.IrInstrumentationRequestHandle), ctypes.alignment(_native.IrInstrumentationRequestHandle)])
        expected.extend([ctypes.sizeof(_native.IrInstrumentationMetadataHandle), ctypes.alignment(_native.IrInstrumentationMetadataHandle)])
        expected.extend([ctypes.sizeof(_native.IrInstrumentationMetadataEventHandle), ctypes.alignment(_native.IrInstrumentationMetadataEventHandle)])
        expected.extend(
            [
                _native.AMS_MEL_IR_PRIORITY_NORMAL,
                _native.AMS_MEL_IR_PRIORITY_DEBUG,
                _native.AMS_MEL_IR_INSTRUMENTATION_METADATA_REPORT,
                _native.AMS_MEL_IR_CHANNEL_INSTRUMENTATION,
            ]
        )
        expected.extend(
            self._layout(
                _native.IrInstrumentationConfigV1,
                'channel_id',
                'channel_type',
                'platform_id',
                'sensor_location',
            )
        )
        expected.extend(self._layout(_native.IrInstrumentationLevelCommandV1, 'command_id', 'priority'))
        expected.extend(
            self._layout(
                _native.IrInstrumentationReportV1,
                'command_id',
                'size',
                'timestamp_ns',
                'priority',
            )
        )
        expected.extend(self._layout(_native.IrInstrumentationResultV1, 'report', 'error_code'))
        expected.extend(self._layout(_native.IrInstrumentationMetadataEventV1, 'kind', 'report'))
        expected.extend([ctypes.sizeof(_native.IrTrackHandle), ctypes.alignment(_native.IrTrackHandle)])
        expected.extend([_native.AMS_MEL_IR_CHANNEL_IRST_TRACK])
        expected.extend(
            self._layout(
                _native.IrTrackConfigV1,
                'channel_id',
                'channel_type',
                'platform_id',
                'sensor_location',
            )
        )
        expected.extend(
            [
                ctypes.sizeof(_native.IrTrackMetadataHandle),
                ctypes.alignment(_native.IrTrackMetadataHandle),
                ctypes.sizeof(_native.IrTrackMetadataEventHandle),
                ctypes.alignment(_native.IrTrackMetadataEventHandle),
                _native.AMS_MEL_IR_TRACK_STATE_IDLE,
                _native.AMS_MEL_IR_TRACK_STATE_DETECTED,
                _native.AMS_MEL_IR_TRACK_STATE_COAST,
                _native.AMS_MEL_IR_TRACK_STATE_DROPPED,
                _native.AMS_MEL_IR_TRACK_MODE_IDLE,
                _native.AMS_MEL_IR_TRACK_MODE_SCAN,
                _native.AMS_MEL_IR_TRACK_MODE_STARE,
                _native.AMS_MEL_IR_TRACK_METADATA_IRST_TRACK_REPORT,
                _native.AMS_MEL_IR_TRACK_METADATA_REQUEST_SYSTEM_TRACK_DATA,
                _native.AMS_MEL_IR_TRACK_METADATA_CANDIDATE_OBJECT_MESSAGE,
                _native.AMS_MEL_IR_TRACK_METADATA_CANDIDATE_OBJECT_PREPROC_MESSAGE,
                _native.AMS_MEL_IR_MAX_CANDIDATE_OBJECTS,
                _native.AMS_MEL_IR_CANDIDATE_BACKGROUND_SIDE,
                _native.AMS_MEL_IR_CANDIDATE_BACKGROUND_SAMPLES,
                _native.AMS_MEL_IR_HOT_REGION_INVALID,
                _native.AMS_MEL_IR_HOT_REGION_FLARE,
                _native.AMS_MEL_IR_HOT_REGION_SOLAR,
                _native.AMS_MEL_IR_HOT_REGION_MASK,
            ]
        )
        expected.extend(self._layout(_native.IrRowColV1, 'row', 'column'))
        expected.extend(
            self._layout(
                _native.IrHotRegionV1,
                'kind',
                'size',
                'top',
                'left',
                'right',
                'bottom',
            )
        )
        expected.extend(self._layout(_native.IrHotRegionSpanV1, 'data', 'size'))
        expected.extend(
            self._layout(
                _native.IrCandidateObjectHeaderV1,
                'number_of_cos',
                'stack_frame_index',
                'cfar',
                'validity_flag_bitfield',
                'tov_utc_ns',
            )
        )
        expected.extend(
            self._layout(
                _native.IrCandidateObjectV1,
                'system_time_ns',
                'detection_category',
                'sensor_index',
                'subpixel',
                'intensity',
                'sensor_relative_unit',
                'signal_to_interference_ratio',
                'signal_to_noise_ratio',
            )
        )
        expected.extend(
            self._layout(_native.IrCandidateObjectSpanV1, 'data', 'size')
        )
        expected.extend(
            self._layout(
                _native.IrCandidateObjectMessageV1,
                'header',
                'inertial_state',
                'hot_regions',
                'candidate_objects',
            )
        )
        expected.extend(
            self._layout(
                _native.IrTrackReportV1,
                'system_time_ns',
                'activity_id',
                'measured_ned',
                'measured_intensity',
                'measured_snr',
                'filtered_ned',
                'filtered_intensity',
                'filtered_snr',
                'range_m',
                'range_error_m',
                'spatial_extent_rad',
                'track_quality',
                'clutter',
                'age_ns',
                'state',
                'mode',
            )
        )
        expected.extend(
            self._layout(
                _native.IrRequestSystemTrackDataV1,
                'system_time_ns',
                'command_id',
                'request_id',
                'track_id',
            )
        )
        # Frozen v1 has exactly these three members; the authoritative C probe
        # comparison is what would detect a future accidental v1 growth.
        expected.extend(
            self._layout(
                _native.IrTrackMetadataEventV1,
                'kind',
                'track_report',
                'request_system_track_data',
            )
        )
        # Frozen v2 has exactly these two members; the authoritative C probe
        # comparison is what would detect a future accidental v2 growth.
        expected.extend(
            self._layout(
                _native.IrTrackMetadataEventV2,
                'base',
                'candidate_object_message',
            )
        )
        expected.extend(self._layout(_native.IrCandidateBackgroundV1, 'samples'))
        expected.extend(
            self._layout(
                _native.IrCandidateObjectPreProcV1,
                'system_time_ns',
                'detection_category',
                'sensor_index',
                'subpixel',
                'intensity',
                'sensor_relative_unit',
                'signal_to_interference_ratio',
                'signal_to_noise_ratio',
                'candidate_object_with_background',
                'clutter',
                'candidate_object_quality',
                'sir_delta',
                'inertial_state',
                'edge',
                'az_sigma',
                'el_sigma',
                'background_normalizer',
            )
        )
        expected.extend(
            self._layout(_native.IrCandidateObjectPreProcSpanV1, 'data', 'size')
        )
        expected.extend(
            self._layout(
                _native.IrCandidateObjectPreProcMessageV1,
                'header',
                'inertial_state',
                'hot_regions',
                'candidate_object_preprocs',
            )
        )
        expected.extend(
            self._layout(
                _native.IrTrackMetadataEventV3,
                'base',
                'candidate_object_preproc_message',
            )
        )
        expected.extend(
            [
                ctypes.sizeof(_native.IrTrackUpdateRequestHandle),
                ctypes.alignment(_native.IrTrackUpdateRequestHandle),
                _native.AMS_MEL_IR_TRACK_STATUS_CREATE,
                _native.AMS_MEL_IR_TRACK_STATUS_UPDATE,
                _native.AMS_MEL_IR_TRACK_STATUS_PREDICT,
                _native.AMS_MEL_IR_TRACK_STATUS_DELETE,
            ]
        )
        expected.extend(
            self._layout(
                _native.IrTrackCovarianceV1,
                'xx',
                'xy',
                'xz',
                'x_vx',
                'x_vy',
                'x_vz',
                'yy',
                'yz',
                'y_vx',
                'y_vy',
                'y_vz',
                'zz',
                'z_vx',
                'z_vy',
                'z_vz',
                'vx_vx',
                'vx_vy',
                'vx_vz',
                'vy_vy',
                'vy_vz',
                'vz_vz',
            )
        )
        expected.extend(
            self._layout(
                _native.IrTrackDataUpdateV1,
                'platform_id',
                'capability_uuid',
                'activity_uuid',
                'track_id',
                'entity_uuid',
                'track_status',
                'time_of_validity_seconds',
                'time_of_last_update_seconds',
                'track_position_ecef',
                'track_velocity_ecef',
                'covariance',
                'maneuver_probability',
                'track_quality',
            )
        )
        expected.extend(
            self._layout(_native.IrTrackUpdateResultV1, 'status', 'error_code')
        )
        expected.extend(
            [
                ctypes.sizeof(_native.IrTrackSystemResponseRequestHandle),
                ctypes.alignment(_native.IrTrackSystemResponseRequestHandle),
            ]
        )
        expected.extend(
            self._layout(
                _native.IrSystemTrackDataResponseV1,
                'system_time_ns',
                'command_id',
                'request_id',
                'track_id',
                'range_m',
                'range_rate_mps',
                'range_error_m',
                'range_rate_error_mps',
                'az_el_valid',
                'range_valid',
                'inertial_az_el',
                'az_el_error',
            )
        )
        expected.extend(
            self._layout(
                _native.IrTrackSystemResponseResultV1, 'status', 'error_code'
            )
        )
        # Task 034B1A RF Admin alias and owner, then Task 033B RF DataMEL.
        for handle in (_native.RfAdminHandle, _native.RfC2Handle,
                       _native.RfVaRequestHandle, _native.RfVaHandle):
            expected.extend([ctypes.sizeof(handle), ctypes.alignment(handle)])
        expected.extend(self._layout(_native.RfVaConfigV1, 'va_definition_id', 'priority',
                                     'local_function_info', 'va_definition_file_info', 'capability_ids'))
        expected.extend(self._layout(_native.RfVaResultV1, 'error_code'))
        expected.extend(self._layout(_native.RfVaInfoV1, 'va_instance_ids', 'element_group_labels',
                                     'is_single_group'))
        expected.extend([ctypes.sizeof(_native.RfVirtualApertureStatus), ctypes.alignment(_native.RfVirtualApertureStatus)])
        for handle in (_native.RfVaInstanceListHandle, _native.RfVaInstanceStatusReportHandle):
            expected.extend([ctypes.sizeof(handle), ctypes.alignment(handle)])
        expected.extend(self._layout(_native.RfVaLocalFunctionStatusV1, 'local_function_type_id', 'statuses'))
        expected.extend(self._layout(_native.RfVaLocalFunctionStatusSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.RfVaInstanceStatusReportV1, 'va_instance_id', 'status', 'local_functions'))
        for handle in (_native.RfJobRequestHandle, _native.RfJobHandle):
            expected.extend([ctypes.sizeof(handle), ctypes.alignment(handle)])
        expected.extend(self._layout(_native.U64SpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.RfRxElementGroupConfigV1,
                                     'label', 'desired_duty_factor', 'expected_center_frequencies',
                                     'endpoint_ids', 'data_pipe_label'))
        expected.extend(self._layout(_native.RfJobRequestConfigV1,
                                     'request_id', 'priority', 'precedence_within_priority',
                                     'is_interruptable', 'instance_selection', 'rx_group'))
        expected.extend(self._layout(_native.RfUtcTimeV1, 'seconds', 'fractional_femtoseconds'))
        expected.extend(self._layout(_native.RfRxDataPipeEndpointConfigV1, 'data_pipe_label', 'endpoint_ids'))
        expected.extend(self._layout(_native.RfRxDataPipeEndpointConfigSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.RfRxElementGroupConfigV2, 'label', 'desired_duty_factor', 'expected_center_frequencies', 'data_pipe_endpoint_configs'))
        expected.extend(self._layout(_native.RfRxElementGroupConfigSpanV2, 'data', 'size'))
        expected.extend(self._layout(_native.RfJobRequestConfigV2, 'request_id', 'priority', 'precedence_within_priority', 'is_interruptable', 'instance_selection', 'rx_groups', 'min_start_time', 'max_complete_time', 'duration_femtoseconds', 'capability_id', 'activity_id', 'tx_power_mode_ids', 'lookahead_femtoseconds'))
        expected.extend([ctypes.sizeof(_native.RfPointingKind), ctypes.alignment(_native.RfPointingKind),
                         _native.RF_POINTING_ECEF,
                         _native.RF_POINTING_LLA,
                         _native.RF_POINTING_PLATFORM_RELATIVE,
                         _native.RF_POINTING_FACE_RELATIVE,
                         _native.RF_POINTING_BASELINE_RELATIVE,
                         ])
        expected.extend(self._layout(_native.RfVector3V1, 'x', 'y', 'z'))
        expected.extend(self._layout(_native.RfAzElV1, 'azimuth_rad', 'elevation_rad'))
        expected.extend(self._layout(_native.RfEcefPointingV1, 'location_m', 'velocity_mps', 'time_of_validity'))
        expected.extend(self._layout(_native.RfLlaPointingV1, 'latitude_rad', 'longitude_rad', 'altitude_m', 'velocity_north_mps', 'velocity_east_mps', 'velocity_down_mps', 'time_of_validity'))
        expected.extend(self._layout(_native.RfPointingV1, 'kind', 'ecef', 'lla', 'platform_relative', 'face_relative', 'baseline_relative_conic_rad'))
        expected.extend(self._layout(_native.RfPointingSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.RfRxElementGroupConfigV3, 'group', 'expected_pointing_angles'))
        expected.extend(self._layout(_native.RfRxElementGroupConfigSpanV3, 'data', 'size'))
        expected.extend(self._layout(_native.RfJobRequestConfigV3, 'request_id', 'priority', 'precedence_within_priority', 'is_interruptable', 'instance_selection', 'rx_groups', 'min_start_time', 'max_complete_time', 'duration_femtoseconds', 'capability_id', 'activity_id', 'tx_power_mode_ids', 'lookahead_femtoseconds', 'has_estimated_stab_point', 'estimated_stab_point'))
        expected.extend(self._layout(_native.RfTxElementGroupConfigV1, 'label', 'tx_power_level', 'desired_duty_factor', 'expected_center_frequencies'))
        expected.extend(self._layout(_native.RfJobElementGroupConfigV4, 'mode', 'rx', 'tx'))
        expected.extend(self._layout(_native.RfJobElementGroupConfigSpanV4, 'data', 'size'))
        expected.extend(self._layout(_native.RfJobRequestConfigV4, 'request_id', 'priority', 'precedence_within_priority', 'is_interruptable', 'instance_selection', 'element_groups', 'min_start_time', 'max_complete_time', 'duration_femtoseconds', 'capability_id', 'activity_id', 'tx_power_mode_ids', 'lookahead_femtoseconds', 'has_estimated_stab_point', 'estimated_stab_point'))
        expected.extend(self._layout(_native.RfJobResultV1, 'error_code'))
        expected.extend(self._layout(_native.RfJobInfoV1,
                                     'actual_start_seconds', 'actual_start_femtoseconds',
                                     'total_job_duration_femtoseconds', 'va_instance_id',
                                     'va_definition_id', 'job_details_id', 'job_request_id',
                                     'lookahead_femtoseconds', 'rx_stream_ids'))
        expected.extend([ctypes.sizeof(ctypes.c_uint32), ctypes.alignment(ctypes.c_uint32)] * 2)
        expected.extend(range(6))
        expected.append(_native.AMS_MEL_RF_CANCEL_ERROR_NONE)
        expected.append(_native.AMS_MEL_RF_JOB_INTERVAL_CONTINUE_FROM_PREVIOUS_FS)
        expected.extend(self._layout(_native.RfReceiveEventConfigV1, 'event_id', 'element_group_label', 'start_femtoseconds', 'duration_femtoseconds', 'center_frequency_hz', 'sample_frequency_hz', 'agc_processing_iterations', 'ignored_post_agc_iterations', 'max_extension_femtoseconds'))
        expected.extend(self._layout(_native.RfReceiveEventConfigSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.RfJobIntervalConfigV1, 'interval_start_femtoseconds', 'interval_id', 'interval_starting_gap_femtoseconds', 'sequence_duration_femtoseconds', 'sequence_repeat_count', 'calibration_duration_femtoseconds', 'interval_ending_gap_femtoseconds', 'phase_coherence_with_prior', 'iterations_per_signal', 'max_data_rate_bps', 'max_sample_rate_hz', 'job_details_id', 'receive_events'))
        expected.extend(self._layout(_native.RfJobIntervalConfigSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.RfJobCancelResultV1, 'cancelled', 'error_code'))
        for handle in (ctypes.c_uint32, _native.RfDataHandle, _native.RfMfaInfoHandle,
                       ctypes.c_uint32):
            expected.extend([ctypes.sizeof(handle), ctypes.alignment(handle)])
        expected.extend(range(14))
        expected.extend(self._layout(_native.RfFrequencyRangeV1, 'min_hz', 'max_hz'))
        expected.extend(self._layout(_native.RfFrequencyRangeSpanV1, 'data', 'size'))
        expected.extend(
            self._layout(
                _native.RfFaceInfoV1,
                'face_id',
                'supports_receive',
                'supports_transmit',
                'requires_endpoint_association',
                'agc_processing_time_fs',
                'min_job_request_lead_time_fs',
                'max_job_request_lead_time_fs',
                'min_job_detail_lead_time_fs',
                'tx_rx_switching_time_fs',
                'rx_tx_switching_time_fs',
                'tx_tx_switching_time_fs',
                'rx_rx_switching_time_fs',
                'rx_frequency_ranges',
                'tx_frequency_ranges',
                'sample_frequency_ranges',
            )
        )
        expected.extend(self._layout(_native.RfFaceInfoSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.RfEulerV1, 'roll_rad', 'pitch_rad', 'yaw_rad'))
        expected.extend(self._layout(_native.RfComponentLocationV1, 'offset_x_m', 'offset_y_m', 'offset_z_m', 'key', 'system_name'))
        expected.extend(self._layout(_native.RfPhysicalDataV1, 'antenna_height_m', 'antenna_width_m', 'lattice_angle_rad', 'location', 'orientation', 'boresight'))
        expected.extend(self._layout(_native.RfTxPowerModeV1, 'tx_power_mode_id', 'is_linear_operation', 'tx_power_level', 'tx_frequency_ranges', 'max_tx_duty_factor', 'max_tx_pulse_width_ns', 'max_tx_atten', 'tx_atten_step_size'))
        expected.extend(self._layout(_native.RfTxPowerModeSpanV1, 'data', 'size'))
        expected.extend(
            self._layout(
                _native.RfMfaInfoV1,
                'reported_num_faces',
                'contains_open_additions',
                'scheduler_resolution_fs',
                'max_user_defined_context_bytes',
                'supported_data_formats',
                'faces',
            )
        )
        # Task 033D RF ProductRxEndpoint ComplexINT16 receive.
        for handle in (
            _native.RfProductRxRequestHandle,
            _native.RfProductRxHandle,
            _native.RfProductRxEventHandle,
        ):
            expected.extend([ctypes.sizeof(handle), ctypes.alignment(handle)])
        expected.extend(
            self._layout(
                _native.RfProductRxConfigV1,
                'data_format',
                'region_size_bytes',
                'queue_capacity',
                'max_samples_per_event',
            )
        )
        expected.extend(self._layout(_native.RfComplexI16V1, 'real', 'imag'))
        expected.extend(self._layout(_native.RfComplexI16SpanV1, 'data', 'size'))
        expected.extend(
            self._layout(
                _native.RfProductRxMetadataV1,
                'mel_protocol_version_id',
                'va_definition_id',
                'va_instance_id',
                'job_details_id',
                'job_interval_id',
                'lf_type_id',
                'lf_instance_id',
                'phase_coherence_with_prior',
                'first_rx_event_start_s',
                'first_rx_event_start_fs',
                'rx_stream_ids',
            )
        )
        expected.extend(
            self._layout(
                _native.RfProductRxEventV1, 'endpoint_id', 'data_format', 'samples', 'metadata'
            )
        )
        expected.extend(
            self._layout(_native.RfProductRxInfoV1, 'endpoint_id', 'assigned_data_format')
        )
        expected.extend(self._layout(_native.RfProductRxRequestResultV1, 'error_code'))
        expected.extend(
            self._layout(
                _native.RfProductRxCountersV1,
                'callbacks_received',
                'products_queued',
                'products_dropped_queue_full',
                'malformed_or_unsupported',
                'allocation_failures',
                'callbacks_after_close',
            )
        )
        expected.extend(self._layout(_native.RfJobIntervalConfigV2, 'interval', 'status_enable'))
        expected.extend(self._layout(_native.RfJobIntervalConfigSpanV2, 'data', 'size'))
        expected.extend(self._layout(_native.RfReceiveEventConfigV2, 'event', 'stab_point_index', 'applicable_rx_element_groups'))
        expected.extend(self._layout(_native.RfReceiveEventConfigSpanV2, 'data', 'size'))
        expected.extend(self._layout(_native.RfJobIntervalConfigV3, 'interval_start_femtoseconds', 'interval_id', 'interval_starting_gap_femtoseconds', 'sequence_duration_femtoseconds', 'sequence_repeat_count', 'calibration_duration_femtoseconds', 'interval_ending_gap_femtoseconds', 'phase_coherence_with_prior', 'iterations_per_signal', 'max_data_rate_bps', 'max_sample_rate_hz', 'job_details_id', 'status_enable', 'stab_points', 'receive_events'))
        expected.extend(self._layout(_native.RfJobIntervalConfigSpanV3, 'data', 'size'))

        expected.extend(self._layout(_native.RfJobEventLogEntryV1, 'event_id', 'trigger', 'time_seconds', 'time_fractional_femtoseconds'))
        expected.extend(self._layout(_native.RfJobEventLogSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.RfJobIntervalStatusV1, 'interval_id', 'completion_status', 'event_log', 'activity_id'))
        expected.extend(self._layout(_native.RfJobIntervalStatusOptionsV1, 'queue_capacity', 'max_event_log_entries', 'max_activity_id_bytes'))
        expected.extend(self._layout(_native.RfJobIntervalStatusCountersV1, 'callback_entries', 'events_queued', 'events_delivered', 'queue_full_drops', 'malformed_drops', 'oversize_drops', 'allocation_failures', 'callbacks_after_close'))
        expected.append(_native.AMS_MEL_RF_INTERVAL_STATUS_NEVER)
        expected.append(_native.AMS_MEL_RF_INTERVAL_STATUS_ALWAYS)
        expected.append(_native.AMS_MEL_RF_INTERVAL_STATUS_ON_EXCEPTION)
        expected.append(_native.AMS_MEL_RF_INTERVAL_COMPLETION_NONE)
        expected.append(_native.AMS_MEL_RF_INTERVAL_COMPLETION_READY_FOR_NEXT_JOB_INTERVAL)
        expected.append(_native.AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INTERRUPTED)
        expected.append(_native.AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_TX_EVENT_SPATIAL_DATA)
        expected.append(_native.AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_TX_EVENT_SIGNAL_DATA)
        expected.append(_native.AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_TX_EVENT_TEMPORAL_DATA)
        expected.append(_native.AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_TX_EVENT_IDENTIFIER_DATA)
        expected.append(_native.AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_RX_EVENT_SPATIAL_DATA)
        expected.append(_native.AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_RX_EVENT_SIGNAL_DATA)
        expected.append(_native.AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_RX_EVENT_TEMPORAL_DATA)
        expected.append(_native.AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_RX_EVENT_IDENTIFIER_DATA)
        expected.append(_native.AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_SEQUENCE_TEMPORAL_DATA)
        expected.append(_native.AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_INTERVAL_SPATIAL_DATA)
        expected.append(_native.AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_INTERVAL_EVENT_SIGNAL_DATA)
        expected.append(_native.AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_INTERVAL_EVENT_TEMPORAL_DATA)
        expected.append(_native.AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_INTERVAL_EVENT_IDENTIFIER_DATA)
        expected.append(_native.AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_TEMPORAL_DATA)
        expected.append(_native.AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_IDENTIFIER_DATA)
        expected.append(_native.AMS_MEL_RF_INTERVAL_COMPLETION_COMPLETED)
        expected.append(_native.AMS_MEL_RF_INTERVAL_COMPLETION_CANCELLED)
        expected.append(_native.AMS_MEL_RF_INTERVAL_COMPLETION_LATE_CONTROLS)
        expected.append(_native.AMS_MEL_RF_INTERVAL_COMPLETION_INVALID_CONTROLS)
        expected.append(_native.AMS_MEL_RF_INTERVAL_COMPLETION_ANTENNA_FOV_ERROR)
        expected.append(_native.AMS_MEL_RF_INTERVAL_COMPLETION_TRANSMIT_RF_INHIBITED)
        expected.append(_native.AMS_MEL_RF_INTERVAL_COMPLETION_STARTED)
        expected.append(_native.AMS_MEL_RF_LOG_TRIGGER_NONE)
        expected.append(_native.AMS_MEL_RF_LOG_TRIGGER_EVENT_EXTENDED)
        expected.append(_native.AMS_MEL_RF_LOG_TRIGGER_EVENT_TRIGGERED)
        expected.append(_native.AMS_MEL_RF_LOG_TRIGGER_EVENT_RESUMED)
        expected.append(_native.AMS_MEL_RF_LOG_TRIGGER_EVENT_CANCELLED)
        expected.append(_native.AMS_MEL_RF_LOG_TRIGGER_EVENT_INHIBITED)
        expected.append(_native.AMS_MEL_RF_LOG_TRIGGER_EVENT_DELAYED_START)
        expected.append(_native.AMS_MEL_RF_LOG_TRIGGER_EVENT_TYPE_NOT_SUPPORTED)
        expected.extend(
            [
                _native.AMS_MEL_OK,
                ctypes.sizeof(_native.RfVaSubscriptionHandle),
                ctypes.alignment(_native.RfVaSubscriptionHandle),
                *self._layout(_native.RfVaSubscriptionStatisticsV1, 'callback_entries',
                              'callbacks_coalesced', 'notifications_delivered',
                              'callbacks_after_stop', 'pending', 'stopped'),
                ctypes.sizeof(_native.RfElementGroupSnapshotHandle), ctypes.alignment(_native.RfElementGroupSnapshotHandle),
                ctypes.sizeof(_native.RfElementGroupMode), ctypes.alignment(_native.RfElementGroupMode),
                _native.RF_ELEMENT_GROUP_MODE_RX, _native.RF_ELEMENT_GROUP_MODE_TX,
                *self._layout(_native.RfElementGroupSnapshotOptionsV1, 'include_data_pipes'),
                *self._layout(_native.RfDataPipeInfoV1, 'lookup_label', 'label', 'associated_endpoint_ids'),
                *self._layout(_native.RfDataPipeInfoSpanV1, 'data', 'size'),
                *self._layout(_native.RfElementGroupDescriptorV1, 'lookup_label', 'label', 'mode', 'max_rf_bandwidth_hz', 'max_sample_rate_samples_per_second', 'max_data_rate_bits_per_second', 'max_duty_factor', 'data_pipes'),
                *self._layout(_native.RfElementGroupDescriptorSpanV1, 'data', 'size'),
                *self._layout(_native.RfElementGroupSnapshotV1, 'data_pipes_included', 'descriptors'),
                ctypes.sizeof(_native.RfVaLocalFunctionListHandle), ctypes.alignment(_native.RfVaLocalFunctionListHandle),
                ctypes.sizeof(_native.RfVaLocalFunctionStatusHandle), ctypes.alignment(_native.RfVaLocalFunctionStatusHandle),
                *self._layout(_native.RfVaLocalFunctionInfoV1, 'local_function_type_id', 'instance_count'),
                *self._layout(_native.RfVaLocalFunctionInfoSpanV1, 'data', 'size'),
                ctypes.sizeof(_native.RfVaDataPipeConnectionsSnapshotHandle), ctypes.alignment(_native.RfVaDataPipeConnectionsSnapshotHandle),
                *self._layout(_native.RfVaDataPipeGroupV1, 'element_group_lookup_label', 'data_pipes'),
                *self._layout(_native.RfVaDataPipeGroupSpanV1, 'data', 'size'),
                *self._layout(_native.RfVaDataPipeConnectionsSnapshotV1, 'groups'),
                _native.AMS_MEL_ABI_VERSION_MAJOR,
                _native.AMS_MEL_ABI_VERSION_MINOR,
                _native.AMS_MEL_ABI_VERSION_MAJOR,
                _native.AMS_MEL_ABI_VERSION_MINOR,
            ]
        )
        self.assertEqual(actual, expected)

    @staticmethod
    def _layout(structure: type[ctypes.Structure], *fields: str) -> list[int]:
        values = [ctypes.sizeof(structure), ctypes.alignment(structure)]
        values.extend(getattr(structure, field).offset for field in fields)
        return values


if __name__ == "__main__":
    unittest.main()
