"""PY-MEL-008: compiled C2 callbacks plus hostile published-C-ABI outputs.

Mock expectations follow native/tests/test_ir_c2.c. Process-global failpoints,
retained callbacks and actual DSO unload checks run in isolated subprocesses.
"""

from __future__ import annotations

from contextlib import closing, contextmanager
import ctypes
import dataclasses
import gc
import math
import os
from pathlib import Path
import pickle
import re
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import weakref

import ams_mel as m
from ams_mel import _native as n, _c2_convert, _c2_metadata
import test_control_channel as control
import test_ir_health as health
import test_ir_health_metadata as health_metadata


PREFIX = "ams_mel_ir_c2_metadata_"
PROTOCOL = m.ErrorKind.PROTOCOL_INCONSISTENCY
LONG_DESCRIPTION = "x" * 510 + "€" + "y" * 100


def expected_configuration():
    uid = health_metadata.uid
    return m.C2BitConfiguration((
        m.C2BitType(uid(0x80, "BIT-α"), m.C2BitControlInterface.SUBSYSTEM_BIT_COMMAND,
                    ("sensor", "optical-€"),
                    (uid(0x90, "component one"), uid(0xA0, "component-β")), 123456789),
        m.C2BitType(uid(0xF0, "startup"), m.C2BitControlInterface.SUBSYSTEM_INITIATED,
                    (), (), 0)))


def diagnostic(args, status=n.AMS_MEL_OK, text=b""):
    """Write a complete diagnostic for injected outputs, as the real ABI does."""
    buffer, capacity, required = args[-3:]
    if required is not None:
        ctypes.cast(required, ctypes.POINTER(ctypes.c_size_t))[0] = len(text) + 1
    if buffer is not None and capacity:
        ctypes.memmove(buffer, text + b"\0", min(capacity, len(text) + 1))
    return status


class _C2TestCase(health._HealthTestCase):
    def open(self, scenario="metadata-rich", configuration=None):
        session = m.Session.open(health.provider(), scenario, "")
        self.addCleanup(session.close)
        channel = session.open_control_channel(control.ControlChannelTests.config())
        self.addCleanup(channel.close)
        return session, channel

    def metadata(self, scenario="metadata-rich", capacity=16, enabled=False):
        session, channel = self.open(scenario)
        if enabled:
            channel.enable()
        stream = channel.open_metadata(capacity)
        self.addCleanup(stream.close)
        return session, channel, stream

    @contextmanager
    def output(self, raw=None, address=None):
        """Only replace the view: the event allocation/release is still native."""
        _, _, stream = self.metadata()
        pointer = ctypes.pointer(raw) if raw is not None else ctypes.cast(
            address, ctypes.POINTER(n.IrC2MetadataEventV1))

        def view(owner, output, *args):
            ctypes.cast(output, ctypes.POINTER(ctypes.POINTER(n.IrC2MetadataEventV1)))[0] = pointer
            return diagnostic(args)

        with mock.patch.object(n, PREFIX + "event_view", side_effect=view), \
                mock.patch.object(n, PREFIX + "event_close",
                                  wraps=getattr(n, PREFIX + "event_close")) as close:
            yield stream, close

    def malformed(self, raw, kind=PROTOCOL):
        with self.output(raw) as (stream, close):
            self.error(kind, stream.receive)
            self.assertEqual(close.call_count, 1)


class C2MetadataTests(_C2TestCase):
    def test_rich_synchronous_base_families_and_canonical_health_bit_graph(self):
        _, channel, stream = self.metadata()
        self.assertNotIn("c2_enabled", self.events())
        self.assertEqual(stream.counters(), m.C2MetadataCounters(2, 0, 0))
        config, bit = stream.receive(), stream.receive()
        self.assertEqual(config, m.C2MetadataEvent(m.C2MetadataKind.BIT_CONFIGURATION,
                                                expected_configuration()))
        self.assertEqual(bit, m.C2MetadataEvent(m.C2MetadataKind.BIT_STATUS,
                                             health_metadata.expected_bit()))
        self.assertIs(type(bit.payload), m.BitStatus)
        self.assertIs(dict(n.IrC2MetadataEventV1._fields_)["bit_status"], n.BitStatusV1)
        self.assertIs(dict(n.IrHealthMetadataEventV1._fields_)["bit_status"], n.BitStatusV1)
        health_metadata.assert_owned(self, (config, bit))
        self.error(m.ErrorKind.TIMEOUT, stream.receive, n.AMS_MEL_TIMEOUT)
        self.assertEqual(stream.counters(), m.C2MetadataCounters(2, 0, 0))
        channel.enable()
        with closing(channel.submit_operate(0xFFFFFFFF)) as request:
            self.assertIsInstance(request.wait(1000), m.ModeSuccess)
        event = stream.receive()
        self.assertEqual(event, m.C2MetadataEvent(m.C2MetadataKind.COMMAND_STATUS,
            m.C2CommandStatus(0xFFFFFFFF, m.C2CommandState.RECEIVED,
                              m.C2CannotComply.NOT_SET, "")))
        self.assertEqual(stream.counters(), m.C2MetadataCounters(3, 0, 0))

    def test_exact_native_base_registration_order_and_seven_existing_exports(self):
        root = Path(__file__).resolve().parents[2]
        source = (root / "native/src/ir_c2.cpp").read_text()
        opening = source.split('extern "C" ams_mel_status_t ams_mel_ir_c2_metadata_open(', 1)[1]
        opening = opening.split('extern "C"', 1)[0]
        self.assertLess(opening.index("const mel::BIT_Configuration"),
                        opening.index("const irmel::CommandStatus"))
        self.assertLess(opening.index("const irmel::CommandStatus"),
                        opening.index("const mel::BIT_Status"))
        self.assertNotIn("ChannelCommsTestRep", opening)
        for suffix in ("open", "register_comms_test", "receive", "get_counters", "close",
                       "event_view", "event_close"):
            self.assertIsNotNone(getattr(n, PREFIX + suffix).argtypes)

    def test_comms_opt_in_ids_independent_request_and_metadata_observations(self):
        _, channel, stream = self.metadata("comms-high")
        with channel.channel_view() as view:
            # No capability gate: the mock advertises no metadata capabilities.
            self.assertEqual(view.capabilities().metadata_capabilities, ())
            input = m.CommsTestRequest(0x80000001, 0xF0000002, 0xE0000003)
            with closing(view.submit_comms_test(input)) as request:
                self.assertEqual(request.wait(1000),
                    m.CommsCompleted(m.CommsTestReport(input.command_id, input.request_id)))
            self.error(m.ErrorKind.TIMEOUT, stream.receive)
            stream.enable_comms_test_events()
            self.error(m.ErrorKind.INVALID_ARGUMENT, stream.enable_comms_test_events)
            with closing(view.submit_comms_test(input)) as request:
                # The callback is delivered synchronously, before request Wait.
                event = stream.receive()
                stream.close()
                self.assertEqual(request.wait(1000), m.CommsCompleted(event.payload))
                self.assertEqual(event, m.C2MetadataEvent(m.C2MetadataKind.CHANNEL_COMMS_TEST,
                    m.CommsTestReport(0x80000001, 0xE0000003)))
                self.assertFalse(hasattr(event.payload, "channel_id"))
        self.assertNotIn("c2_enabled", self.events())

    def test_all_four_families_share_one_fifo_and_counters(self):
        _, channel, stream = self.metadata()
        stream.enable_comms_test_events()
        channel.enable()
        with closing(channel.submit_operate(17)) as mode, channel.channel_view() as view:
            with closing(view.submit_comms_test(m.CommsTestRequest(18, 19, 20))) as comms:
                self.assertEqual([stream.receive().kind for _ in range(4)],
                    [m.C2MetadataKind.BIT_CONFIGURATION, m.C2MetadataKind.BIT_STATUS,
                     m.C2MetadataKind.COMMAND_STATUS, m.C2MetadataKind.CHANNEL_COMMS_TEST])
                self.assertEqual(stream.counters(), m.C2MetadataCounters(4, 0, 0))
                mode.wait(1000)
                comms.wait(1000)

    def test_registration_attached_and_enabled_no_implicit_enable(self):
        for enabled in (False, True):
            with self.subTest(enabled=enabled):
                _, _, stream = self.metadata(enabled=enabled)
                stream.enable_comms_test_events()
                self.assertEqual(stream.receive().payload, expected_configuration())
                self.assertEqual(stream.receive().payload, health_metadata.expected_bit())

    def test_invalid_capacity_does_not_consume_registration_or_enter_native(self):
        _, channel = self.open()
        with mock.patch.object(n, PREFIX + "open", wraps=getattr(n, PREFIX + "open")) as op:
            for value in (0, -1, True, 1.5, "16", 1 << (ctypes.sizeof(ctypes.c_size_t) * 8)):
                self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: channel.open_metadata(value))
            self.assertEqual(op.call_count, 0)
        stream = channel.open_metadata()
        self.addCleanup(stream.close)
        self.error(m.ErrorKind.INVALID_ARGUMENT, channel.open_metadata)
        stream.close()
        self.error(m.ErrorKind.INVALID_ARGUMENT, channel.open_metadata)
        channel.close()
        with mock.patch.object(n, PREFIX + "open") as op:
            self.error(m.ErrorKind.INVALID_ARGUMENT, channel.open_metadata)
            op.assert_not_called()

    def test_drop_incoming_native_fifo_and_persistent_counters(self):
        _, _, stream = self.metadata("metadata-overflow", 2)
        self.assertEqual(stream.counters(), m.C2MetadataCounters(3, 1, 0))
        self.assertEqual(stream.receive().payload, expected_configuration())
        self.assertEqual(stream.receive().payload, m.C2CommandStatus(
            1, m.C2CommandState.ACCEPTED, m.C2CannotComply.NOT_SET, ""))
        self.error(m.ErrorKind.TIMEOUT, stream.receive)
        self.assertEqual(stream.counters(), m.C2MetadataCounters(3, 1, 0))

    def test_comms_opt_in_uses_same_bounded_drop_incoming_queue(self):
        _, channel, stream = self.metadata(capacity=2)
        stream.enable_comms_test_events()
        with channel.channel_view() as view:
            with closing(view.submit_comms_test(m.CommsTestRequest(1, 2, 3))) as request:
                self.assertEqual(request.wait(1000), m.CommsCompleted(m.CommsTestReport(1, 3)))
        self.assertEqual(stream.counters(), m.C2MetadataCounters(3, 1, 0))
        self.assertEqual(stream.receive().kind, m.C2MetadataKind.BIT_CONFIGURATION)
        self.assertEqual(stream.receive().kind, m.C2MetadataKind.BIT_STATUS)
        self.error(m.ErrorKind.TIMEOUT, stream.receive)

    def test_malformed_native_callbacks_do_not_discard_later_valid_events(self):
        _, _, stream = self.metadata("metadata-malformed", 4)
        self.assertEqual(stream.counters(), m.C2MetadataCounters(6, 0, 4))
        self.assertEqual(stream.receive().payload, m.C2CommandStatus(
            5, m.C2CommandState.CANCELLED, m.C2CannotComply.CANCELLED, "valid"))
        self.assertEqual(stream.receive().payload, health_metadata.expected_bit())
        self.error(m.ErrorKind.TIMEOUT, stream.receive)
        self.assertEqual(stream.counters(), m.C2MetadataCounters(6, 0, 4))

    def test_command_states_high_ids_and_complete_long_reason(self):
        _, _, stream = self.metadata("metadata-command-states", 4)
        states = (m.C2CommandState.ACCEPTED, m.C2CommandState.REJECTED,
                  m.C2CommandState.CANCELLED, m.C2CommandState.RECEIVED)
        for index, state in enumerate(states):
            event = stream.receive()
            self.assertEqual(event.kind, m.C2MetadataKind.COMMAND_STATUS)
            self.assertEqual(event.payload.command_id, 0x80000001 + index)
            self.assertIs(event.payload.state, state)
            if index == 1:
                self.assertEqual(event.payload.reason, m.C2CannotComply.INVALID_INPUT_PARAMETER)
                self.assertEqual(event.payload.reason_description, LONG_DESCRIPTION)
                self.assertEqual(len(event.payload.reason_description.encode()), 613)

    def test_command_status_does_not_replace_mode_wait_result(self):
        _, channel, stream = self.metadata("metadata-command-status", enabled=True)
        with closing(channel.submit_operate(0x80000001)) as request:
            event = stream.receive()
            self.assertEqual(event.payload, m.C2CommandStatus(0x80000001,
                m.C2CommandState.REJECTED, m.C2CannotComply.INVALID_INPUT_PARAMETER,
                LONG_DESCRIPTION))
            # Mock intentionally rejects the metadata command while fulfilling Wait.
            self.assertIsInstance(request.wait(1000), m.ModeSuccess)

    def test_metadata_close_leaves_c2_requests_and_weak_services_operational(self):
        _, channel, stream = self.metadata("metadata-command-status")
        stream.close()
        stream.close()
        channel.enable()
        with closing(channel.submit_operate(0x80000001)) as request, channel.channel_view() as view:
            self.assertIsInstance(request.wait(1000), m.ModeSuccess)
            with closing(view.send_keepalive()) as keepalive:
                self.assertIsInstance(keepalive.wait(1000), m.ReturnCompleted)
            view.capabilities()
        for operation in (stream.receive, stream.counters, stream.enable_comms_test_events,
                          stream.__enter__):
            self.error(m.ErrorKind.INVALID_ARGUMENT, operation)

    def test_parent_first_teardown_drain_and_no_python_parent_retention(self):
        for session_first in (True, False):
            session = m.Session.open(health.provider(), "metadata-rich", "")
            channel = session.open_control_channel(control.ControlChannelTests.config())
            stream = channel.open_metadata()
            self.addCleanup(stream.close)
            sr, cr = weakref.ref(session), weakref.ref(channel)
            with channel.channel_view() as view:
                if session_first:
                    session.close()
                    view.capabilities()
                    channel.close()
                else:
                    channel.close()
                    session.close()
                self.error(m.ErrorKind.PROVIDER_FAILED, view.capabilities)
            self.assertEqual(stream.receive().payload, expected_configuration())
            self.assertEqual(stream.receive().payload, health_metadata.expected_bit())
            self.error(m.ErrorKind.STREAM_STOPPED, stream.receive, n.AMS_MEL_STREAM_STOPPED)
            del session, channel
            gc.collect()
            self.assertIsNone(sr())
            self.assertIsNone(cr())

    def test_pending_request_survives_metadata_session_and_c2_close(self):
        session, channel, stream = self.metadata("comms-delayed")
        with channel.channel_view() as view:
            request = view.submit_comms_test(m.CommsTestRequest(17, 7, 19))
        with closing(request):
            stream.close()
            session.close()
            channel.close()
            self.assertEqual(request.wait(1000), m.CommsCompleted(m.CommsTestReport(17, 19)))

    def test_exact_enum_domains_and_all_47_reasons_match_current_c_header(self):
        header = (Path(__file__).resolve().parents[2] / "native/include/ams_mel/abi.h").read_text()
        published = [(name, int(value)) for name, value in re.findall(
            r"#define AMS_MEL_IR_CANNOT_COMPLY_(\w+) UINT32_C\((\d+)\)", header)]
        self.assertEqual([(v.name, v.value) for v in m.C2CannotComply], published)
        self.assertEqual([v.value for v in m.C2CannotComply], list(range(47)))
        self.assertEqual([v.value for v in m.C2CommandState], list(range(5)))
        self.assertEqual([v.value for v in m.C2BitControlInterface], list(range(4)))
        for reason in m.C2CannotComply:
            raw = n.IrC2MetadataEventV1(kind=1)
            raw.command_status.reason_id = reason.value
            self.assertIs(_c2_convert._event_from_raw(raw).payload.reason, reason)
        self.assertEqual(m.C2CannotComply.UNKNOWN.value, 44)
        with self.assertRaises(ValueError):
            m.C2CannotComply(47)

    def test_public_discriminator_pickle_identity_and_frozen_owned_graphs(self):
        values = (m.C2CommandStatus(1, m.C2CommandState.NOT_SET, m.C2CannotComply.UNKNOWN, "€"),
                  expected_configuration(), health_metadata.expected_bit(), m.CommsTestReport(2, 3))
        for kind in m.C2MetadataKind:
            for index, payload in enumerate(values, 1):
                if kind.value != index:
                    with self.assertRaises(TypeError):
                        m.C2MetadataEvent(kind, payload)
                else:
                    event = m.C2MetadataEvent(kind, payload)
                    health_metadata.assert_owned(self, event)
                    self.assertEqual(pickle.loads(pickle.dumps(event)), event)
                    self.assertEqual(type(event).__module__, "ams_mel")
                    with self.assertRaises(dataclasses.FrozenInstanceError):
                        event.kind = kind
        for value in (*m.C2MetadataKind, *m.C2CannotComply, *m.C2CommandState,
                      *m.C2BitControlInterface, m.C2MetadataCounters(2**64-1, 2**63, 7)):
            self.assertIs(type(pickle.loads(pickle.dumps(value))), type(value))
            self.assertEqual(type(value).__module__, "ams_mel")
        with self.assertRaises(TypeError):
            m.C2MetadataEvent(1, values[0])
        with self.assertRaises(TypeError):
            m.C2MetadataStream()


class C2DefensiveTests(_C2TestCase):
    def test_unknown_event_kind_and_command_enums(self):
        self.malformed(n.IrC2MetadataEventV1(kind=99))
        for field, value in (("state", 5), ("reason_id", 47), ("reason_id", 0xFFFFFFFF)):
            raw = n.IrC2MetadataEventV1(kind=1)
            setattr(raw.command_status, field, value)
            self.malformed(raw)

    def test_null_and_misaligned_outer_event_views(self):
        raw = n.IrC2MetadataEventV1(kind=1)
        pointer_max = (1 << (8 * ctypes.sizeof(ctypes.c_void_p))) - 1
        aligned_max = pointer_max - pointer_max % ctypes.alignment(type(raw))
        for address in (None, ctypes.addressof(raw) + 1, aligned_max):
            with self.output(address=address) as (stream, close):
                self.error(PROTOCOL, stream.receive)
                self.assertEqual(close.call_count, 1)

    def test_bit_configuration_nested_spans_and_control_enum(self):
        raw = n.IrC2MetadataEventV1(kind=2)
        item = n.BitTypeV1()
        raw.bit_configuration.bit_types = n.BitTypeSpanV1(ctypes.pointer(item), 1)
        for value in range(4):
            item.accepted_interface = value
            self.assertEqual(_c2_convert._event_from_raw(raw).payload.bit_types[0].accepted_interface,
                             m.C2BitControlInterface(value))
        item.accepted_interface = 4
        self.malformed(raw)
        item.accepted_interface = 0
        for span in (raw.bit_configuration.bit_types, item.bit_item_names,
                     item.subsystem_component_ids):
            pointer, size = span.data, span.size
            pointer_max = (1 << (ctypes.sizeof(ctypes.c_void_p) * 8)) - 1
            aligned_max = pointer_max - pointer_max % ctypes.alignment(span.data._type_)
            for address, length in ((None, 1), (1, 1), (ctypes.addressof(item), sys.maxsize),
                                    (aligned_max, 2)):
                span.data = ctypes.cast(address, type(span.data))
                span.size = length
                self.malformed(raw)
            span.data, span.size = pointer, size

    def test_bit_configuration_signed_duration_empty_duplicate_and_ordered_values(self):
        raw = n.IrC2MetadataEventV1(kind=2)
        item = n.BitTypeV1(expected_duration_ns=-(1 << 63))
        names = (n.StringViewV1 * 3)()
        buffers = [ctypes.create_string_buffer(s) for s in (b"z", "µ".encode(), b"z")]
        for i, buffer in enumerate(buffers):
            names[i] = n.StringViewV1(ctypes.cast(buffer, n.CharPointer), len(buffer.value))
        item.bit_item_names = n.StringViewSpanV1(names, 3)
        ids = (n.UciIdV1 * 2)()
        ids[0].uuid[0] = ids[1].uuid[0] = 0xFF
        item.subsystem_component_ids = n.UciIdSpanV1(ids, 2)
        items = (n.BitTypeV1 * 2)(item, item)
        raw.bit_configuration.bit_types = n.BitTypeSpanV1(items, 2)
        with self.output(raw) as (stream, close):
            value = stream.receive().payload
            self.assertEqual(value.bit_types[0], value.bit_types[1])
            self.assertEqual(value.bit_types[0].bit_item_names, ("z", "µ", "z"))
            self.assertEqual(value.bit_types[0].expected_duration_ns, -(1 << 63))
            self.assertEqual(value.bit_types[0].subsystem_component_ids[0],
                             value.bit_types[0].subsystem_component_ids[1])
            self.assertEqual(close.call_count, 1)
        raw.bit_configuration.bit_types.size = 0
        self.assertEqual(_c2_convert._event_from_raw(raw).payload, m.C2BitConfiguration(()))

    def test_corrupt_strings_and_nested_identifier_labels(self):
        raw = n.IrC2MetadataEventV1(kind=2)
        item = n.BitTypeV1()
        components = (n.UciIdV1 * 1)()
        names = (n.StringViewV1 * 1)()
        item.subsystem_component_ids = n.UciIdSpanV1(components, 1)
        item.bit_item_names = n.StringViewSpanV1(names, 1)
        raw.bit_configuration.bit_types = n.BitTypeSpanV1(ctypes.pointer(item), 1)
        command = n.IrC2MetadataEventV1(kind=1)
        for target, event in ((item.bit_id.descriptive_label, raw),
                              (components[0].descriptive_label, raw), (names[0], raw),
                              (command.command_status.reason_description, command)):
            for text, kind in ((b"a\0b", PROTOCOL), (b"bad\xc3(", m.ErrorKind.INVALID_UTF8)):
                buffer = ctypes.create_string_buffer(text)
                target.data, target.size = ctypes.cast(buffer, n.CharPointer), len(text)
                self.malformed(event, kind)
            target.data, target.size = n.CharPointer(), 1
            self.malformed(event)
            target.size = 0

    def test_shared_bit_converter_rejects_nested_unknown_enums_and_bad_spans(self):
        # Mutate only a live event-owned view, never provider data or its ABI.
        mutations = (
            lambda r: setattr(r.completed_bits.data[0], "result", 99),
            lambda r: setattr(r.completed_bits.data[0].bit_items.data[0], "result", 99),
            lambda r: setattr(r.faults.data[0], "severity", 99),
            lambda r: setattr(r.faults.data[0], "state", 99),
            lambda r: setattr(r.active_bits, "size", sys.maxsize),
            lambda r: setattr(r.completed_bits.data[0].bit_items, "data", None),
            lambda r: setattr(r.faults.data[0].fault_data, "data", None),
            lambda r: setattr(r.faults.data[0].component_ids, "data", None),
            lambda r: setattr(r.faults.data[0].ambiguity_groups, "data", None),
            lambda r: setattr(r.faults.data[0].ambiguity_groups.data[0].diagnostic_test_ids,
                              "data", None),
        )
        native_view = getattr(n, PREFIX + "event_view")
        for mutate in mutations:
            with self.subTest(mutate=mutate):
                _, _, stream = self.metadata()
                stream.receive()  # configuration
                def view(owner, output, *args):
                    status = native_view(owner, output, *args)
                    raw = ctypes.cast(output, ctypes.POINTER(
                        ctypes.POINTER(n.IrC2MetadataEventV1)))[0].contents
                    mutate(raw.bit_status)
                    return status
                with mock.patch.object(n, PREFIX + "event_view", side_effect=view), \
                        mock.patch.object(n, PREFIX + "event_close",
                                          wraps=getattr(n, PREFIX + "event_close")) as close:
                    self.error(PROTOCOL, stream.receive)
                    self.assertEqual(close.call_count, 1)

    def test_nonfinite_bit_percentages_and_duplicates_are_not_normalized(self):
        raw = n.IrC2MetadataEventV1(kind=3)
        values = (n.ActiveBitV1 * 5)()
        for item, percent in zip(values, (float("nan"), float("inf"), -float("inf"), -1., -1.)):
            item.estimated_percent_complete = percent
            item.estimated_completion_time_ns = -(1 << 63)
        raw.bit_status.active_bits = n.ActiveBitSpanV1(values, len(values))
        result = _c2_convert._event_from_raw(raw).payload
        self.assertTrue(math.isnan(result.active_bits[0].estimated_percent_complete))
        self.assertEqual(tuple(v.estimated_percent_complete for v in result.active_bits[1:]),
                         (float("inf"), -float("inf"), -1., -1.))
        self.assertEqual(result.active_bits[-1], result.active_bits[-2])
        self.assertEqual(result.active_bits[0].estimated_completion_time_ns, -(1 << 63))

    def test_only_active_payload_is_inspected(self):
        for kind in m.C2MetadataKind:
            raw = n.IrC2MetadataEventV1()
            ctypes.memset(ctypes.byref(raw), 0xFF, ctypes.sizeof(raw))
            raw.kind = kind.value
            field, record = {
                1: ("command_status", n.IrCommandStatusV1),
                2: ("bit_configuration", n.BitConfigurationV1),
                3: ("bit_status", n.BitStatusV1),
                4: ("channel_comms_test", n.IrChannelCommsTestReportV1),
            }[kind.value]
            setattr(raw, field, record())
            with self.output(raw) as (stream, close):
                self.assertEqual(stream.receive().kind, kind)
                self.assertEqual(close.call_count, 1)

    def test_receive_errors_with_published_owner_preserve_status_and_cleanup_once(self):
        receive = getattr(n, PREFIX + "receive")
        for status, kind in ((n.AMS_MEL_TIMEOUT, m.ErrorKind.TIMEOUT),
                (n.AMS_MEL_STREAM_STOPPED, m.ErrorKind.STREAM_STOPPED),
                (n.AMS_MEL_PROVIDER_FAILED, m.ErrorKind.PROVIDER_FAILED),
                (n.AMS_MEL_PROVIDER_EXCEPTION, m.ErrorKind.PROVIDER_EXCEPTION),
                (n.AMS_MEL_INTERNAL_ERROR, m.ErrorKind.INTERNAL_ERROR),
                (n.AMS_MEL_RESOURCE_EXHAUSTED, m.ErrorKind.RESOURCE_EXHAUSTED),
                (n.AMS_MEL_INVALID_ARGUMENT, m.ErrorKind.INVALID_ARGUMENT)):
            _, _, stream = self.metadata()
            def failed(*args):
                self.assertEqual(receive(*args), n.AMS_MEL_OK)
                return diagnostic(args, status, b"original native diagnostic")
            with mock.patch.object(n, PREFIX + "receive", side_effect=failed) as call, \
                    mock.patch.object(n, PREFIX + "event_close",
                                      wraps=getattr(n, PREFIX + "event_close")) as close:
                self.error(kind, stream.receive, status, "original native diagnostic")
                self.assertEqual(call.call_count, 1)
                self.assertEqual(close.call_count, 1)

    def test_successful_receive_without_event_does_not_close_null(self):
        _, _, stream = self.metadata()
        with mock.patch.object(n, PREFIX + "receive", side_effect=lambda *a: diagnostic(a)), \
                mock.patch.object(n, PREFIX + "event_close") as close:
            self.error(PROTOCOL, stream.receive)
            close.assert_not_called()

    def test_event_view_failure_cleans_event(self):
        _, _, stream = self.metadata()
        with mock.patch.object(n, PREFIX + "event_view", side_effect=lambda *a:
                               diagnostic(a, n.AMS_MEL_INTERNAL_ERROR, b"view failure")), \
                mock.patch.object(n, PREFIX + "event_close",
                                  wraps=getattr(n, PREFIX + "event_close")) as close:
            self.error(m.ErrorKind.INTERNAL_ERROR, stream.receive, n.AMS_MEL_INTERNAL_ERROR,
                       "view failure")
            self.assertEqual(close.call_count, 1)

    def test_event_close_consumed_retained_errors_and_retained_success_never_retry(self):
        native_close = getattr(n, PREFIX + "event_close")
        for consume, status in ((True, n.AMS_MEL_INTERNAL_ERROR),
                                (False, n.AMS_MEL_INTERNAL_ERROR), (False, n.AMS_MEL_OK)):
            _, _, stream = self.metadata()
            retained = n.IrC2MetadataEventHandle()
            def close(owner, *args):
                if consume:
                    native_close(owner, None, 0, None)
                else:
                    retained.value = ctypes.cast(owner, ctypes.POINTER(ctypes.c_void_p))[0]
                return diagnostic(args, status, b"event close")
            try:
                with mock.patch.object(n, PREFIX + "event_close", side_effect=close) as call:
                    self.error(PROTOCOL if status == n.AMS_MEL_OK else m.ErrorKind.INTERNAL_ERROR,
                               stream.receive)
                    self.assertEqual(call.call_count, 1)
            finally:
                if retained.value:
                    native_close(ctypes.byref(retained), None, 0, None)

    def test_conversion_and_base_exceptions_preserved_when_cleanup_also_raises(self):
        for error in (MemoryError("copy"), KeyboardInterrupt("interrupt"), RuntimeError("body")):
            _, _, stream = self.metadata()
            native_close = getattr(n, PREFIX + "event_close")
            def failed(*args):
                native_close(*args)
                raise RuntimeError("cleanup")
            with mock.patch.object(_c2_metadata, "_event_from_raw", side_effect=error), \
                    mock.patch.object(n, PREFIX + "event_close", side_effect=failed) as close:
                with self.assertRaises(type(error)) as caught:
                    stream.receive()
                self.assertIs(caught.exception, error)
                self.assertEqual(close.call_count, 1)

    def test_diagnostic_allocation_failure_before_event_close_still_cleans_once(self):
        _, _, stream = self.metadata()
        call = _c2_metadata._call_with_diagnostic
        count = 0
        def allocate(operation):
            nonlocal count
            count += 1
            if count == 3:
                raise MemoryError("close diagnostic")
            return call(operation)
        with mock.patch.object(_c2_metadata, "_call_with_diagnostic", side_effect=allocate), \
                mock.patch.object(n, PREFIX + "event_close",
                                  wraps=getattr(n, PREFIX + "event_close")) as close:
            with self.assertRaisesRegex(MemoryError, "close diagnostic"):
                stream.receive()
            self.assertEqual(close.call_count, 1)

    def test_owner_close_consumed_retained_errors_and_retained_success(self):
        for consume, status in ((True, n.AMS_MEL_INTERNAL_ERROR),
                                (False, n.AMS_MEL_INTERNAL_ERROR), (False, n.AMS_MEL_OK)):
            _, _, stream = self.metadata()
            native_close = stream._close_function
            def close(owner, *args):
                if consume:
                    native_close(owner, None, 0, None)
                return diagnostic(args, status, b"owner close")
            with mock.patch.object(stream, "_close_function", side_effect=close) as call:
                self.error(PROTOCOL if status == n.AMS_MEL_OK else m.ErrorKind.INTERNAL_ERROR,
                           stream.close)
                self.assertEqual(stream.is_open, not consume)
                if not consume:
                    self.error(PROTOCOL, stream.close)
                    for operation in (stream.receive, stream.counters, stream.enable_comms_test_events):
                        self.error(m.ErrorKind.INVALID_ARGUMENT, operation)
                else:
                    stream.close()
                stream.__del__()
                self.assertEqual(call.call_count, 1)
            if stream.is_open:
                native_close(stream._owner_pointer, None, 0, None)

    def test_finalizer_is_nonraising_one_attempt_and_context_preserves_body_exception(self):
        for consume in (True, False):
            _, _, stream = self.metadata()
            native_close = stream._close_function
            def close(*args):
                if consume:
                    native_close(*args)
                raise MemoryError("cleanup")
            with mock.patch.object(stream, "_close_function", side_effect=close) as call:
                with self.assertRaisesRegex(RuntimeError, "application"):
                    with stream:
                        raise RuntimeError("application")
                stream.__del__()
                self.assertEqual(call.call_count, 1)
            if stream.is_open:
                native_close(stream._owner_pointer, None, 0, None)

    def test_normal_context_exit_propagates_close_error(self):
        _, _, stream = self.metadata()
        native_close = stream._close_function
        def close(*args):
            native_close(*args)
            return n.AMS_MEL_PROVIDER_FAILED
        with mock.patch.object(stream, "_close_function", side_effect=close):
            self.error(m.ErrorKind.PROVIDER_FAILED, lambda: stream.__exit__(None, None, None))

    def test_implicit_finalizer_calls_native_once(self):
        _, channel = self.open()
        stream = channel.open_metadata()
        ref = weakref.ref(stream)
        close = m.C2MetadataStream._close_function
        with mock.patch.object(m.C2MetadataStream, "_close_function",
                               staticmethod(mock.Mock(wraps=close))) as wrapped:
            del stream
            gc.collect()
            self.assertIsNone(ref())
            self.assertEqual(wrapped.__func__.call_count, 1)

    def test_open_failure_publishing_owner_preserves_original_and_cleans_once(self):
        _, channel = self.open()
        opening = getattr(n, PREFIX + "open")
        close = m.C2MetadataStream._close_function
        def failed(*args):
            self.assertEqual(opening(*args), n.AMS_MEL_OK)
            return diagnostic(args, n.AMS_MEL_PROVIDER_FAILED, b"open failed")
        def cleanup(*args):
            close(*args)
            raise RuntimeError("cleanup")
        with mock.patch.object(n, PREFIX + "open", side_effect=failed), \
                mock.patch.object(m.C2MetadataStream, "_close_function",
                                  staticmethod(mock.Mock(side_effect=cleanup))) as wrapped:
            self.error(m.ErrorKind.PROVIDER_FAILED, channel.open_metadata,
                       n.AMS_MEL_PROVIDER_FAILED, "open failed")
            self.assertEqual(wrapped.__func__.call_count, 1)
        gc.collect()
        self.error(m.ErrorKind.INVALID_ARGUMENT, channel.open_metadata)

    def test_open_success_without_owner_is_protocol_error(self):
        _, channel = self.open()
        with mock.patch.object(n, PREFIX + "open", side_effect=lambda *a: diagnostic(a)):
            self.error(PROTOCOL, channel.open_metadata)

    def test_full_uint64_counters_and_local_timeout_validation(self):
        _, _, stream = self.metadata()
        def high(owner, output, *args):
            raw = ctypes.cast(output, ctypes.POINTER(n.IrC2MetadataCountersV1)).contents
            raw.events_received = (1 << 64) - 1
            raw.events_dropped_queue_full = 1 << 63
            raw.malformed_or_unsupported = (1 << 64) - 2
            return diagnostic(args)
        with mock.patch.object(n, PREFIX + "get_counters", side_effect=high):
            self.assertEqual(stream.counters(), m.C2MetadataCounters(2**64-1, 2**63, 2**64-2))
        with mock.patch.object(n, PREFIX + "receive") as receive:
            for value in (-1, True, 1.5, "0", 1 << 32):
                self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: stream.receive(value))
            receive.assert_not_called()
        self.assertEqual(stream.receive((1 << 32) - 1).kind, m.C2MetadataKind.BIT_CONFIGURATION)

    def test_malformed_receive_diagnostics_release_published_events(self):
        receive = getattr(n, PREFIX + "receive")
        for corruption, kind in (("zero", PROTOCOL), ("nul", PROTOCOL),
                                  ("utf8", m.ErrorKind.INVALID_UTF8), ("unterminated", PROTOCOL)):
            _, _, stream = self.metadata()
            def bad(*args):
                status = receive(*args)
                buffer, _, required = args[-3:]
                if corruption == "zero":
                    ctypes.cast(required, ctypes.POINTER(ctypes.c_size_t))[0] = 0
                elif corruption == "nul":
                    diagnostic(args, text=b"a\0b")
                elif corruption == "utf8":
                    diagnostic(args, text=b"\xff")
                else:
                    ctypes.memset(buffer, 0xFF, 1)
                return status
            with mock.patch.object(n, PREFIX + "receive", side_effect=bad), \
                    mock.patch.object(n, PREFIX + "event_close",
                                      wraps=getattr(n, PREFIX + "event_close")) as close:
                self.error(kind, stream.receive)
                self.assertEqual(close.call_count, 1)

    def test_successful_context_exit_and_idempotent_close(self):
        _, channel = self.open()
        with channel.open_metadata() as stream:
            self.assertTrue(stream.is_open)
        self.assertFalse(stream.is_open)
        stream.close()

    def test_all_shared_bit_nested_spans_and_strings_are_validated(self):
        # Derive every populated nested span/text path from the actual compiled
        # fixture, then corrupt each separately on a fresh native event.
        native_view = getattr(n, PREFIX + "event_view")
        paths = []

        def discover(raw, path=()):
            if isinstance(raw, ctypes.Structure):
                if hasattr(raw, "data") and hasattr(raw, "size"):
                    paths.append((path, isinstance(raw, n.StringViewV1)))
                    if not isinstance(raw, n.StringViewV1) and raw.size:
                        discover(raw.data[0], path + ("data", 0))
                else:
                    for name, _ in raw._fields_:
                        discover(getattr(raw, name), path + (name,))

        _, _, initial = self.metadata()
        initial.receive()
        def inspect(owner, output, *args):
            status = native_view(owner, output, *args)
            raw = ctypes.cast(output, ctypes.POINTER(
                ctypes.POINTER(n.IrC2MetadataEventV1)))[0].contents
            discover(raw.bit_status)
            return status
        with mock.patch.object(n, PREFIX + "event_view", side_effect=inspect):
            initial.receive()
        self.assertEqual(len(paths), 24)  # All populated span/text paths in rich_bit_status.
        for path, text in paths:
            for corruption in (("null", "oversized") if text else ("null", "misaligned", "oversized")):
                with self.subTest(path=path, corruption=corruption):
                    _, _, stream = self.metadata()
                    stream.receive()
                    def corrupt(owner, output, *args):
                        status = native_view(owner, output, *args)
                        raw = ctypes.cast(output, ctypes.POINTER(
                            ctypes.POINTER(n.IrC2MetadataEventV1)))[0].contents.bit_status
                        for step in path:
                            raw = raw[step] if isinstance(step, int) else getattr(raw, step)
                        if corruption == "null":
                            raw.data, raw.size = None, 1
                        elif corruption == "misaligned":
                            raw.data = ctypes.cast(1, type(raw.data))
                            raw.size = 1
                        else:
                            raw.size = (1 << (ctypes.sizeof(ctypes.c_size_t) * 8)) - 1
                        return status
                    with mock.patch.object(n, PREFIX + "event_view", side_effect=corrupt), \
                            mock.patch.object(n, PREFIX + "event_close",
                                              wraps=getattr(n, PREFIX + "event_close")) as close:
                        self.error(PROTOCOL, stream.receive)
                        self.assertEqual(close.call_count, 1)

    def test_corrupt_shared_bit_nested_text_utf8_and_embedded_nul(self):
        native_view = getattr(n, PREFIX + "event_view")
        targets = (
            lambda r: r.active_bits.data[0].bit_id.descriptive_label,
            lambda r: r.completed_bits.data[0].bit_id.descriptive_label,
            lambda r: r.completed_bits.data[0].fail_reason,
            lambda r: r.completed_bits.data[0].bit_items.data[0].bit_item_name,
            lambda r: r.completed_bits.data[0].bit_items.data[0].fail_reason,
            lambda r: r.faults.data[0].fault_id.descriptive_label,
            lambda r: r.faults.data[0].fault_data.data[0].key,
            lambda r: r.faults.data[0].fault_data.data[0].value,
            lambda r: r.faults.data[0].fault_data.data[0].format,
            lambda r: r.faults.data[0].fault_data.data[0].units,
            lambda r: r.faults.data[0].fault_description,
            lambda r: r.faults.data[0].fault_code,
            lambda r: r.faults.data[0].component_ids.data[0].descriptive_label,
            lambda r: r.faults.data[0].ambiguity_groups.data[0].diagnostic_test_ids.data[0].descriptive_label,
            lambda r: r.faults.data[0].ambiguity_groups.data[0].component_ids.data[0].descriptive_label,
        )
        for target in targets:
            for text, kind in ((b"\xff", m.ErrorKind.INVALID_UTF8), (b"a\0b", PROTOCOL)):
                _, _, stream = self.metadata()
                stream.receive()
                buffer = ctypes.create_string_buffer(text)
                def corrupt(owner, output, *args):
                    status = native_view(owner, output, *args)
                    raw = ctypes.cast(output, ctypes.POINTER(
                        ctypes.POINTER(n.IrC2MetadataEventV1)))[0].contents
                    value = target(raw.bit_status)
                    value.data, value.size = ctypes.cast(buffer, n.CharPointer), len(text)
                    return status
                with mock.patch.object(n, PREFIX + "event_view", side_effect=corrupt), \
                        mock.patch.object(n, PREFIX + "event_close",
                                          wraps=getattr(n, PREFIX + "event_close")) as close:
                    self.error(kind, stream.receive)
                    self.assertEqual(close.call_count, 1)

    def test_failed_receive_with_owner_and_failed_cleanup_preserves_original(self):
        _, _, stream = self.metadata()
        receive, native_close = getattr(n, PREFIX + "receive"), getattr(n, PREFIX + "event_close")
        def failed_receive(*args):
            receive(*args)
            return diagnostic(args, n.AMS_MEL_PROVIDER_EXCEPTION, b"receive original")
        def failed_close(*args):
            native_close(*args)
            raise KeyboardInterrupt("cleanup")
        with mock.patch.object(n, PREFIX + "receive", side_effect=failed_receive), \
                mock.patch.object(n, PREFIX + "event_close", side_effect=failed_close) as close:
            self.error(m.ErrorKind.PROVIDER_EXCEPTION, stream.receive,
                       n.AMS_MEL_PROVIDER_EXCEPTION, "receive original")
            self.assertEqual(close.call_count, 1)

    def test_oversized_native_error_diagnostic_is_not_retried_or_truncated(self):
        _, _, stream = self.metadata()
        def failed(*args):
            buffer, _, required = args[-3:]
            ctypes.memmove(buffer, b"partial\0", 8)
            ctypes.cast(required, ctypes.POINTER(ctypes.c_size_t))[0] = 9000
            return n.AMS_MEL_PROVIDER_EXCEPTION
        with mock.patch.object(n, PREFIX + "receive", side_effect=failed) as receive:
            error = self.error(m.ErrorKind.PROVIDER_EXCEPTION, stream.receive,
                               n.AMS_MEL_PROVIDER_EXCEPTION)
            self.assertIsNone(error.diagnostic)
            self.assertEqual(error.diagnostic_required, 9000)
            self.assertEqual(receive.call_count, 1)


class C2IsolatedTests(unittest.TestCase):
    def run_child(self, code, environment=None):
        # unittest discovery adds this directory to sys.path, not PYTHONPATH;
        # reproduce it explicitly for child interpreters under make test-python.
        path = os.pathsep.join((str(Path(__file__).resolve().parent),
                                os.environ.get("PYTHONPATH", "")))
        result = subprocess.run([sys.executable, "-W", "error", "-c", code],
            env={**os.environ, "PYTHONPATH": path, **(environment or {})},
            text=True, capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_callback_allocation_failure_drains_then_provider_failed(self):
        self.run_child('''
import os
import ams_mel as m
from test_ir_c2_metadata import expected_configuration
from test_ir_health import provider
from test_control_channel import ControlChannelTests
with m.Session.open(provider(), "metadata-overflow", "") as session:
    with session.open_control_channel(ControlChannelTests.config()) as channel:
        os.environ["AMS_MEL_TEST_METADATA_CALLBACK_FAILURE"] = "command-allocation"
        with channel.open_metadata(4) as stream:
            del os.environ["AMS_MEL_TEST_METADATA_CALLBACK_FAILURE"]
            assert stream.receive().payload == expected_configuration()
            try: stream.receive()
            except m.MelError as e: assert e.kind is m.ErrorKind.PROVIDER_FAILED
            else: raise AssertionError("missing provider failure")
            assert stream.counters() == m.C2MetadataCounters(3, 0, 0)
''')

    def test_base_registration_failure_consumes_one_shot_and_retains_callback(self):
        with tempfile.TemporaryDirectory() as temp:
            log = Path(temp) / "lifetime"
            self.run_child('''
import ams_mel as m
from test_ir_health import provider
from test_control_channel import ControlChannelTests
with m.Session.open(provider(), "metadata-register-fail", "") as session:
    with session.open_control_channel(ControlChannelTests.config()) as channel:
        for expected in (m.ErrorKind.PROVIDER_FAILED, m.ErrorKind.INVALID_ARGUMENT):
            try: channel.open_metadata()
            except m.MelError as e: assert e.kind is expected
            else: raise AssertionError("registration unexpectedly succeeded")
''', {"AMS_MEL_TEST_LIFETIME_LOG": str(log)})
            events = log.read_text().splitlines()
            self.assertLess(events.index("retained_metadata_callback_invoked"),
                            events.index("c2_channel_destroyed"))
            self.assertLess(events.index("c2_channel_destroyed"), events.index("library_unloaded"))

    def test_comms_registration_failure_keeps_base_callback_and_queue(self):
        with tempfile.TemporaryDirectory() as temp:
            log = Path(temp) / "lifetime"
            self.run_child('''
from contextlib import closing
import ams_mel as m
from test_ir_health import provider
from test_control_channel import ControlChannelTests
with m.Session.open(provider(), "comms-register-retain-fail", "") as session:
    with session.open_control_channel(ControlChannelTests.config()) as channel:
        with channel.open_metadata() as stream:
            for expected in (m.ErrorKind.PROVIDER_FAILED, m.ErrorKind.INVALID_ARGUMENT):
                try: stream.enable_comms_test_events()
                except m.MelError as e: assert e.kind is expected
                else: raise AssertionError("registration unexpectedly succeeded")
            channel.enable()
            with closing(channel.submit_operate(23)) as request:
                assert stream.receive().payload.command_id == 23
                assert isinstance(request.wait(1000), m.ModeSuccess)
            with channel.channel_view() as view:
                with closing(view.submit_comms_test(m.CommsTestRequest(31, 32, 33))) as request:
                    assert stream.receive().payload == m.CommsTestReport(31, 33)
                    request.wait(1000)
            channel.close()
            try: stream.receive()
            except m.MelError as e: assert e.kind is m.ErrorKind.STREAM_STOPPED
            else: raise AssertionError("stopped queue unexpectedly delivered")
            assert stream.counters() == m.C2MetadataCounters(3, 0, 0)
''', {"AMS_MEL_TEST_LIFETIME_LOG": str(log)})
            events = log.read_text().splitlines()
            self.assertLess(events.index("retained_comms_callback_invoked"),
                            events.index("c2_channel_destroyed"))
            self.assertLess(events.index("c2_channel_destroyed"), events.index("library_unloaded"))

    def test_nonquiescing_disable_uses_native_callback_barrier(self):
        with tempfile.TemporaryDirectory() as temp:
            log, barrier = Path(temp) / "lifetime", Path(temp) / "barrier"
            self.run_child('''
import ams_mel as m
from test_ir_health import provider
from test_control_channel import ControlChannelTests
with m.Session.open(provider(), "metadata-nonquiescing-disable", "") as session:
    with session.open_control_channel(ControlChannelTests.config()) as channel:
        with channel.open_metadata() as stream:
            channel.enable()
            channel.close()
            try: stream.receive()
            except m.MelError as e: assert e.kind is m.ErrorKind.STREAM_STOPPED
            else: raise AssertionError("in-flight callback must not enqueue after inactive")
''', {"AMS_MEL_TEST_LIFETIME_LOG": str(log),
      "AMS_MEL_TEST_METADATA_CALLBACK_BARRIER": str(barrier)})
            events = log.read_text().splitlines()
            ordered = ("metadata_callback_entered", "disable_returned_with_metadata_callback_active",
                       "metadata_callback_returned", "c2_channel_destroyed", "library_unloaded")
            self.assertEqual(sorted(events.index(e) for e in ordered),
                             [events.index(e) for e in ordered])
            self.assertTrue(barrier.with_suffix(".entered").exists())
            self.assertTrue(barrier.with_suffix(".release").exists())

    def test_owned_values_survive_actual_provider_unload_and_parent_collection(self):
        with tempfile.TemporaryDirectory() as temp:
            log = Path(temp) / "lifetime"
            self.run_child('''
import gc, os, weakref
from pathlib import Path
import ams_mel as m
from test_ir_c2_metadata import expected_configuration
from test_ir_health_metadata import expected_bit
from test_ir_health import provider
from test_control_channel import ControlChannelTests
session = m.Session.open(provider(), "metadata-rich", "")
channel = session.open_control_channel(ControlChannelTests.config())
stream = channel.open_metadata()
sr, cr = weakref.ref(session), weakref.ref(channel)
session.close()
channel.close()
del session, channel
gc.collect()
assert sr() is None and cr() is None
# The metadata queue does not pin the provider. Its event data are adapter-owned.
assert "library_unloaded" in Path(os.environ["AMS_MEL_TEST_LIFETIME_LOG"]).read_text()
values = (stream.receive(), stream.receive())
stream.close()
assert values[0].payload == expected_configuration()
assert values[1].payload == expected_bit()
''', {"AMS_MEL_TEST_LIFETIME_LOG": str(log)})


if __name__ == "__main__":
    unittest.main()
