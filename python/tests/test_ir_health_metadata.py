"""PY-MEL-005: six-family parity, native ownership, and malformed ABI outputs.

Positive scenarios use the unmodified compiled provider. Boundary injection is
only for C responses the provider cannot publish. Retention/barrier cases run
in subprocesses; the test-only public-owner observer is not a production API.
"""

from __future__ import annotations

import ctypes
import dataclasses
import gc
import math
import os
from pathlib import Path
import pickle
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import weakref

import ams_mel as m
from ams_mel import _native as n
from ams_mel import _health_convert, _health_metadata
import test_ir_health as health
import test_common_channel as common


PREFIX = "ams_mel_ir_health_metadata_"
PROTOCOL = m.ErrorKind.PROTOCOL_INCONSISTENCY


def uid(seed, label):
    return m.UciId(bytes((seed + 7 * i) % 256 for i in range(16)), label)


def expected_mfa():
    forward, back = m.Euler(.1, .2, .3), m.Euler(-.4, .5, -.6)
    location = m.ComponentLocation(1.5, -2.5, 3.5, "bay", "platform-€")
    foreign = m.ForeignKey("rack-β", "aircraft")
    return m.MfaStatus(m.MFAState.MAINTENANCE, "healthy-α", "mode-€",
        m.StateTransitionStatus.TRANSITIONING,
        m.About("model", "serial", "software", "boot", "hardware"), (
            m.MfaComponent(uid(0x91, "component-γ"), m.ComponentState.OPERATIONAL,
                m.TemperatureStatus(42.25, m.TemperatureState.NORMAL), foreign,
                m.InstallationDetails(location, forward, back)),
            m.MfaComponent(uid(0xA1, "component two"), m.ComponentState.DEGRADED,
                m.TemperatureStatus(-12.5, m.TemperatureState.UNDER_TEMP), foreign,
                m.InstallationDetails(location, back, forward))))


def expected_bit():
    return m.BitStatus((
        m.ActiveBit(uid(0x81, "active negative"), -5, 1.25),
        m.ActiveBit(uid(0x82, "active zero"), 0, 0.),
        m.ActiveBit(uid(0x83, "active positive"), 987654321, .5)), (
        m.CompletedBit(uid(0x91, "completed pass"), 42, m.BitResult.PASS, "", (
            m.CompletedBitItem("item pass", m.BitResult.PASS, ""),
            m.CompletedBitItem("item-γ", m.BitResult.FAIL, "item reason"))),
        m.CompletedBit(uid(0x92, "completed fail"), -99, m.BitResult.FAIL, "failure-€", ())), (
        m.Fault(uid(0xD0, "fault-ζ"), m.FaultSeverity.WARNING, m.FaultState.SET, (
            m.FaultData("temperature", "101", "integer", "°C"),
            m.FaultData("phase-δ", "bad", "text", "")), -1234567, "F-42", "overheat-€", (
            uid(0xE1, "fault component one"), uid(0xE2, "fault component two")), (
            m.FaultAmbiguityGroup((uid(0xB1, "diagnostic one"), uid(0xB2, "diagnostic two")),
                (uid(0xC1, "ambiguous one"), uid(0xC2, "ambiguous two"))),
            m.FaultAmbiguityGroup((uid(0xB3, "diagnostic three"),),
                (uid(0xC3, "ambiguous three"),)))),))


def expected_subsystem():
    return m.SubsystemStatus(0x80000001, 7, 0xF0000002, m.FailureLevel.MAJOR, 99, (
        m.SubsystemDependency(0x80000003, 8, m.FailureLevel.CRITICAL),
        m.SubsystemDependency(4, 9, m.FailureLevel.AVAILABLE)), 88, (
        m.CsciInfo("flight-δ", m.CsciMode.OPERATIONAL, m.CsciVersion(1, 2, 3, 4),
            10, m.FailureLevel.PARAMETRIC, 0x80000005, True),
        m.CsciInfo("maintenance", m.CsciMode.MAINTENANCE, m.CsciVersion(5, 6, 7, 8),
            11, m.FailureLevel.INFORMATIONAL, 6, False)))


def expected_security():
    variants = (
        m.SecurityNone(),
        m.SecurityAuthentication(m.SecurityAuthenticationCategory.MDF_AUTHENTICATION,
            "auth-ζ", uid(1, "auth subsystem"), uid(2, "auth service"), uid(3, "auth mdf")),
        m.SecurityIntegrity(m.SecurityIntegrityCategory.OTHER_INTEGRITY_CHECK,
            "integrity-η", uid(4, "integrity mdf")),
        m.SecurityFileManagement(m.SecurityFileManagementCategory.OTHER_FILE_UPDATE,
            "file-θ", uid(5, "file mdf")),
        m.SecurityKeyManagement(m.SecurityKeyManagementCategory.CERTIFICATE_DELETION, "key-ι"),
        m.SecuritySystem(m.SecuritySystemCategory.FAULT, "system-κ"),
        m.SecuritySanitization(m.SecuritySanitizationCategory.CLASSIFIED_DATA_ERASE, "sanitize-λ"))
    return tuple(m.SecurityAuditRecord(uid(0xD1, "event-ε"), -987654321,
        uid(0xE1, "security subsystem"), (
            m.SecurityArtifact(uid(0xF1, "artifact one"), uid(0x81, "associated one")),
            m.SecurityArtifact(uid(0x82, "artifact two"), uid(0x83, "associated two"))),
        v, m.SecurityOutcome.FAILURE, m.SecuritySeverity.WARNING) for v in variants)


def expected_events():
    payloads = (expected_mfa(), expected_bit(), expected_subsystem(),
        m.DiscreteStatus((m.NameValuePair("duplicate", "one"), m.NameValuePair("duplicate", ""),
                          m.NameValuePair("utf8", "€"))),
        *expected_security(),
        m.MfaStatusDetailed((m.NameValuePair("load", "23"), m.NameValuePair("load", ""),
                             m.NameValuePair("label", "μ"))))
    return tuple(m.HealthMetadataEvent(m.HealthMetadataKind(k), p)
                 for k, p in zip((1, 2, 3, 4, *([5] * 7), 6), payloads))


def public_owners():
    observer = n._LIBRARY.ams_mel_test_health_metadata_public_owners
    observer.argtypes = [ctypes.POINTER(ctypes.c_size_t)]
    observer.restype = ctypes.c_int
    count = ctypes.c_size_t()
    if observer(ctypes.byref(count)) != 1:
        raise AssertionError("public owner observation failed")
    return count.value


def assert_owned(test, value):
    if dataclasses.is_dataclass(value):
        test.assertTrue(value.__dataclass_params__.frozen)
        for field in dataclasses.fields(value):
            assert_owned(test, getattr(value, field.name))
    elif isinstance(value, tuple):
        for item in value:
            assert_owned(test, item)
    else:
        test.assertIsInstance(value, (int, float, bool, str, bytes))
    test.assertFalse(isinstance(value, (ctypes.Structure, ctypes.Array, list, dict)))


class _MetadataTestCase(health._HealthTestCase):
    def metadata(self, scenario="health-rich", capacity=16):
        session, channel = self.open(scenario)
        stream = channel.open_metadata(capacity)
        self.addCleanup(stream.close)
        return session, channel, stream


class HealthMetadataTests(_MetadataTestCase):
    def test_all_six_families_full_graph_after_all_native_owners_and_dso_close(self):
        session, channel, stream = self.metadata()
        self.assertNotIn("channel_enabled", self.events())
        self.assertEqual(stream.counters(), m.HealthMetadataCounters(12, 0, 0))
        events = tuple(stream.receive() for _ in range(12))
        self.error(m.ErrorKind.TIMEOUT, stream.receive, n.AMS_MEL_TIMEOUT)
        stream.close()
        channel.close()
        session.close()
        self.assertIn("library_unloaded", self.events())
        self.assertEqual(events, expected_events())
        self.assertEqual(set(e.kind for e in events), set(m.HealthMetadataKind))
        self.assertEqual([e.payload.event.kind for e in events[4:11]], list(m.SecurityEventKind))
        assert_owned(self, events)
        for event in events:
            self.assertEqual(pickle.loads(pickle.dumps(event)), event)
            with self.assertRaises(dataclasses.FrozenInstanceError):
                event.kind = m.HealthMetadataKind.BIT_STATUS

    def test_enabled_registration_and_context_management(self):
        session, channel = self.open("health-rich")
        channel.enable()
        with channel.open_metadata() as stream:
            self.assertTrue(stream.is_open)
            self.assertEqual(stream.receive().payload, expected_mfa())
        self.assertFalse(stream.is_open)
        stream.close()
        channel.capabilities()
        channel.enable()
        session.close()

    def test_fifo_overflow_and_nonresetting_counters(self):
        _, _, stream = self.metadata("health-overflow", 2)
        counts = m.HealthMetadataCounters(12, 10, 0)
        self.assertEqual(stream.counters(), counts)
        self.assertEqual((stream.receive(), stream.receive(1000)), expected_events()[:2])
        for timeout in (0, 1):
            self.error(m.ErrorKind.TIMEOUT, lambda: stream.receive(timeout), n.AMS_MEL_TIMEOUT)
        self.assertEqual(stream.counters(), counts)

    def test_malformed_callback_is_counted_and_valid_record_recovers(self):
        _, _, stream = self.metadata("health-malformed")
        self.assertEqual(stream.counters(), m.HealthMetadataCounters(2, 0, 1))
        self.assertEqual(stream.receive().payload, expected_mfa())
        self.error(m.ErrorKind.TIMEOUT, stream.receive)

    def test_allocation_failure_drains_then_reports_failed_even_after_health_close(self):
        _, channel, stream = self.metadata("health-allocation")
        self.assertEqual(stream.counters(), m.HealthMetadataCounters(2, 0, 0))
        self.assertEqual(stream.receive().payload, expected_mfa())
        self.error(m.ErrorKind.PROVIDER_FAILED, stream.receive, n.AMS_MEL_PROVIDER_FAILED)
        channel.close()
        self.error(m.ErrorKind.PROVIDER_FAILED, stream.receive)
        self.assertEqual(stream.counters(), m.HealthMetadataCounters(2, 0, 0))

    def test_invalid_capacity_before_native_and_registration_one_shot(self):
        _, channel = self.open("health-rich")
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
        self.error(m.ErrorKind.INVALID_ARGUMENT, channel.open_metadata)

    def test_metadata_first_close_leaves_health_and_weak_view_operational(self):
        _, channel, stream = self.metadata()
        stream.close()
        stream.close()
        channel.enable()
        channel.capabilities()
        with channel.channel_view() as view:
            view.capabilities()
        self.error(m.ErrorKind.INVALID_ARGUMENT, stream.receive)
        self.error(m.ErrorKind.INVALID_ARGUMENT, stream.counters)

    def test_parent_first_lifetimes_without_python_retention(self):
        session = m.Session.open(health.provider(), "health-rich", "")
        channel = session.open_health_channel(health.config())
        stream = channel.open_metadata()
        self.addCleanup(stream.close)
        sr, cr = weakref.ref(session), weakref.ref(channel)
        session.close()
        channel.capabilities()
        channel.close()
        del session, channel
        gc.collect()
        self.assertIsNone(sr())
        self.assertIsNone(cr())
        self.assertEqual(tuple(stream.receive() for _ in range(12)), expected_events())
        self.error(m.ErrorKind.STREAM_STOPPED, stream.receive, n.AMS_MEL_STREAM_STOPPED)
        self.assertEqual(stream.counters(), m.HealthMetadataCounters(12, 0, 0))

    def test_independent_health_owners(self):
        session, first, a = self.metadata()
        second = session.open_health_channel(health.config())
        self.addCleanup(second.close)
        b = second.open_metadata()
        self.addCleanup(b.close)
        a.close()
        first.close()
        second.enable()
        self.assertEqual(tuple(b.receive() for _ in range(12)), expected_events())
        self.assertEqual(b.counters(), m.HealthMetadataCounters(12, 0, 0))

    def test_metadata_registration_does_not_register_nuc(self):
        _, _, stream = self.metadata("nuc-rich")
        self.assertFalse(any("nuc" in line for line in self.events()))
        self.error(m.ErrorKind.TIMEOUT, stream.receive)
        self.assertEqual(stream.counters(), m.HealthMetadataCounters(0, 0, 0))

    def test_implicit_finalization_releases_exactly_one_public_wrapper(self):
        baseline = public_owners()
        _, channel = self.open("health-rich")
        stream = channel.open_metadata()
        self.assertEqual(public_owners(), baseline + 1)
        ref = weakref.ref(stream)
        del stream
        gc.collect()
        self.assertIsNone(ref())
        self.assertEqual(public_owners(), baseline)
        channel.capabilities()
        channel.close()
        self.assertEqual(public_owners(), baseline)

    def test_finalizer_is_nonraising_and_never_retries_uncertain_cleanup(self):
        _, _, stream = self.metadata()
        native = stream._close_function
        def failed(*args):
            native(*args)
            raise RuntimeError("cleanup")
        with mock.patch.object(stream, "_close_function", side_effect=failed) as close:
            stream.__del__()
            stream.__del__()
            self.assertEqual(close.call_count, 1)
        self.assertFalse(stream.is_open)

    def test_exceptional_context_preserves_original_and_releases_owner(self):
        _, channel = self.open("health-rich")
        baseline = public_owners()
        with self.assertRaisesRegex(RuntimeError, "application"):
            with channel.open_metadata() as stream:
                raise RuntimeError("application")
        self.assertFalse(stream.is_open)
        self.assertEqual(public_owners(), baseline)

    def test_parent_first_implicit_finalization_releases_public_owner(self):
        baseline = public_owners()
        session, channel = self.open("health-rich")
        stream = channel.open_metadata()
        session.close()
        channel.close()
        self.assertEqual(public_owners(), baseline + 1)
        del stream
        gc.collect()
        self.assertEqual(public_owners(), baseline)

    def test_finalizer_retained_handle_is_not_retried(self):
        _, channel = self.open("health-rich")
        stream = channel.open_metadata()
        native = stream._close_function
        # Simulate an uncertain failure without consuming. Release explicitly
        # through the real ABI afterwards so this adversarial test leaks nothing.
        with mock.patch.object(stream, "_close_function", side_effect=MemoryError("cleanup")) as close:
            stream.__del__()
            stream.__del__()
            self.assertEqual(close.call_count, 1)
        self.assertTrue(stream.is_open)
        self.assertEqual(native(stream._owner_pointer, None, 0, None), n.AMS_MEL_OK)

    def test_discriminated_models_and_distinct_security_category_domains(self):
        for k in m.HealthMetadataKind:
            for event in expected_events():
                if event.kind == k:
                    self.assertEqual(m.HealthMetadataEvent(k, event.payload), event)
                else:
                    with self.assertRaises(TypeError):
                        m.HealthMetadataEvent(k, event.payload)
        with self.assertRaises(TypeError):
            m.HealthMetadataEvent(1, expected_mfa())
        with self.assertRaises(TypeError):
            m.SecuritySystem(m.SecuritySanitizationCategory.SANITIZATION, "")
        with self.assertRaises(TypeError):
            m.HealthMetadataStream()
        with self.assertRaises(TypeError):
            dataclasses.replace(expected_security()[0], event={})

    def test_exact_enum_domains_and_all_published_values_decode(self):
        domains = (
            (m.HealthMetadataKind, range(1, 7)), (m.MFAState, range(15)),
            (m.StateTransitionStatus, range(4)), (m.ComponentState, range(9)),
            (m.TemperatureState, range(6)), (m.BitResult, range(5)),
            (m.FaultSeverity, range(5)), (m.FaultState, range(4)),
            (m.FailureLevel, range(7)), (m.CsciMode, range(11)),
            (m.SecurityOutcome, range(3)), (m.SecuritySeverity, range(5)),
            (m.SecurityEventKind, range(7)), (m.SecurityAuthenticationCategory, range(8)),
            (m.SecurityIntegrityCategory, range(7)), (m.SecurityFileManagementCategory, range(5)),
            (m.SecurityKeyManagementCategory, range(7)), (m.SecuritySystemCategory, range(5)),
            (m.SecuritySanitizationCategory, range(4)))
        for domain, values in domains:
            with self.subTest(domain=domain.__name__):
                self.assertEqual([v.value for v in domain], list(values))
                for value in values:
                    self.assertIs(_health_convert._enum(domain, value), domain(value))
                with self.assertRaises(ValueError):
                    domain(0xFFFFFFFF)
                self.error(PROTOCOL, lambda: _health_convert._enum(domain, 0xFFFFFFFF))

    def test_isolated_partial_registration_and_callback_quiescence(self):
        for case in ("partial", "barrier"):
            code = "import test_ir_health_metadata as t; t.isolated_case(%r)" % case
            environment = dict(os.environ)
            environment["PYTHONPATH"] = os.pathsep.join(
                (str(Path(__file__).resolve().parent), environment.get("PYTHONPATH", "")))
            result = subprocess.run([sys.executable, "-W", "error", "-c", code],
                env=environment, capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


def isolated_case(case):
    test = unittest.TestCase()
    with tempfile.TemporaryDirectory(prefix="py-health-metadata-child-") as directory:
        log = Path(directory) / "log"
        os.environ["AMS_MEL_TEST_LIFETIME_LOG"] = str(log)
        scenario = "health-register-partial" if case == "partial" else "health-metadata-nonquiescing-disable"
        session = m.Session.open(health.provider(), scenario, "")
        channel = session.open_health_channel(health.config())
        if case == "partial":
            baseline = public_owners()
            for kind in (m.ErrorKind.PROVIDER_FAILED, m.ErrorKind.INVALID_ARGUMENT):
                with test.assertRaises(m.MelError) as error:
                    channel.open_metadata()
                test.assertIs(error.exception.kind, kind)
            test.assertEqual(public_owners(), baseline)
            channel.enable()
            channel.close()
            session.close()
            lines = log.read_text().splitlines()
            test.assertLess(lines.index("retained_health_callback_invoked"),
                            lines.index("health_channel_destroyed"))
        else:
            barrier = Path(directory) / "barrier"
            os.environ["AMS_MEL_TEST_HEALTH_CALLBACK_BARRIER"] = str(barrier)
            stream = channel.open_metadata()
            channel.enable()
            barrier.with_suffix(".start").touch()
            channel.close()  # Native/provider barriers join the in-flight callback.
            test.assertEqual(stream.counters(), m.HealthMetadataCounters(1, 0, 0))
            with test.assertRaises(m.MelError) as error:
                stream.receive()
            test.assertIs(error.exception.kind, m.ErrorKind.STREAM_STOPPED)
            stream.close()
            session.close()
            lines = log.read_text().splitlines()
            ordered = ("health_callback_entered", "disable_returned_with_health_callback_active",
                       "health_callback_returned", "health_channel_destroyed", "library_unloaded")
            test.assertEqual([lines.index(x) for x in ordered], sorted(lines.index(x) for x in ordered))
        test.assertIn("library_unloaded", lines)


class HealthMetadataBoundaryTests(_MetadataTestCase):
    def injected(self, raw, kind=PROTOCOL):
        _, _, stream = self.metadata()
        native = getattr(n, PREFIX + "event_view")
        def view(owner, output, *args):
            status = native(owner, output, *args)
            ctypes.cast(output, ctypes.POINTER(ctypes.POINTER(n.IrHealthMetadataEventV1)))[0] = ctypes.pointer(raw)
            return status
        with mock.patch.object(n, PREFIX + "event_view", side_effect=view), \
                mock.patch.object(n, PREFIX + "event_close", wraps=getattr(n, PREFIX + "event_close")) as close:
            self.error(kind, stream.receive)
            self.assertEqual(close.call_count, 1)

    def test_invalid_kind_and_each_native_enum_in_active_nested_records(self):
        paths = (
            (0, "kind"), (1, "mfa_status.state"), (1, "mfa_status.transition_status"),
            (1, "mfa_status.components.data.0.state"),
            (1, "mfa_status.components.data.0.temperature.state"),
            (2, "bit_status.completed_bits.data.0.result"),
            (2, "bit_status.completed_bits.data.0.bit_items.data.0.result"),
            (2, "bit_status.faults.data.0.severity"), (2, "bit_status.faults.data.0.state"),
            (3, "subsystem_status.failure"), (3, "subsystem_status.subsystems.data.0.failure"),
            (3, "subsystem_status.csci.data.0.mode"), (3, "subsystem_status.csci.data.0.failure"),
            (5, "security_audit.event.kind"), (5, "security_audit.outcome"),
            (5, "security_audit.severity"))
        for family, path in paths:
            raw, keep = raw_graph(family or 1)
            with self.subTest(path=path):
                set_path(raw, path, 0xFFFFFFFF)
                self.injected(raw)

    def test_bad_nested_spans_at_every_collection_level(self):
        for family, path in SPANS:
            for mutation in ("null", "alignment", "huge"):
                raw, keep = raw_graph(family)
                span = get_path(raw, path)
                element = span.data._type_
                if mutation == "alignment" and ctypes.alignment(element) == 1:
                    continue
                span.size = 1
                if mutation == "null":
                    span.data = ctypes.POINTER(element)()
                elif mutation == "alignment":
                    span.data = ctypes.cast(ctypes.addressof(keep[0]) + 1, ctypes.POINTER(element))
                else:
                    span.size = sys.maxsize
                with self.subTest(path=path, mutation=mutation):
                    self.injected(raw)

    def test_nested_string_views_utf8_nul_null_and_huge(self):
        paths = ((1, "mfa_status.state_description"), (1, "mfa_status.about.model"),
            (1, "mfa_status.components.data.0.installation_details.location.system_name"),
            (2, "bit_status.completed_bits.data.0.bit_items.data.0.fail_reason"),
            (2, "bit_status.faults.data.0.fault_data.data.0.units"),
            (3, "subsystem_status.csci.data.0.csci"), (4, "discrete_status.data.0.name"),
            (5, "security_audit.artifacts.data.0.component_id.descriptive_label"),
            (6, "mfa_status_detailed.data.0.value"))
        for family, path in paths:
            for value in (b"\xff", b"a\0b", None, "huge"):
                raw, keep = raw_graph(family)
                text = get_path(raw, path)
                if isinstance(value, bytes):
                    buffer = ctypes.create_string_buffer(value)
                    keep.append(buffer)
                    text.data = ctypes.cast(buffer, n.CharPointer)
                    text.size = len(value)
                else:
                    text.data = n.CharPointer() if value is None else ctypes.cast(ctypes.byref(keep[0]), n.CharPointer)
                    text.size = 1 if value is None else (1 << 64) - 1
                with self.subTest(path=path, value=value):
                    self.injected(raw, m.ErrorKind.INVALID_UTF8 if value == b"\xff" else PROTOCOL)

    def test_security_kind_specific_categories_and_inactive_identifiers(self):
        for kind in range(7):
            raw, keep = raw_graph(5)
            event = raw.security_audit.event
            event.kind = kind
            event.category = (1, 8, 7, 5, 7, 5, 4)[kind]
            with self.subTest(kind=kind, case="category"):
                self.injected(raw)
            if kind == 0:
                event.category = 0
                event.details.size = 1
                with self.subTest(kind=kind, case="details"):
                    self.injected(raw)
            for field in ("subsystem_id", "service_id", "mdf_id"):
                if kind == 1 or (kind in (2, 3) and field == "mdf_id"):
                    continue
                for label in (False, True):
                    raw, keep = raw_graph(5)
                    raw.security_audit.event.kind = kind
                    ident = getattr(raw.security_audit.event, field)
                    if label:
                        ident.descriptive_label.size = 1
                    else:
                        ident.uuid[15] = 1
                    with self.subTest(kind=kind, field=field, label=label):
                        self.injected(raw)

    def test_strict_native_bool_and_unmodified_numeric_values(self):
        for value in (2, 0xFFFFFFFF):
            raw, keep = raw_graph(3)
            raw.subsystem_status.csci.data[0].connection_established = value
            self.injected(raw)
        raw, keep = raw_graph(1)
        raw.mfa_status.components.data[0].temperature.temperature_c = float("nan")
        self.assertTrue(math.isnan(_health_convert._event_from_raw(raw).payload.components[0].temperature.temperature_c))
        raw, keep = raw_graph(2)
        raw.bit_status.active_bits.data[0].estimated_percent_complete = float("inf")
        self.assertEqual(_health_convert._event_from_raw(raw).payload.active_bits[0].estimated_percent_complete,
                         float("inf"))

    def test_only_active_payload_is_read_and_empty_names_survive(self):
        raw = n.IrHealthMetadataEventV1()
        ctypes.memset(ctypes.byref(raw), 0xFF, ctypes.sizeof(raw))
        raw.kind = 4
        text = ctypes.create_string_buffer("µ".encode())
        pairs = (n.NameValuePairV1 * 2)()
        pairs[1].value = n.StringViewV1(ctypes.cast(text, n.CharPointer), 2)
        raw.discrete_status = n.NameValuePairSpanV1(pairs, 2)
        value = _health_convert._event_from_raw(raw)
        self.assertEqual(value.payload.pairs, (m.NameValuePair("", ""), m.NameValuePair("", "µ")))

    def test_null_event_and_null_misaligned_or_failed_view(self):
        for mutation in ("null-event", "null-view", "misaligned-view", "failed-view"):
            _, _, stream = self.metadata()
            receive, view = getattr(n, PREFIX + "receive"), getattr(n, PREFIX + "event_view")
            def rec(*args):
                status = receive(*args)
                if mutation == "null-event":
                    getattr(n, PREFIX + "event_close")(args[2], None, 0, None)
                return status
            def vw(owner, output, *args):
                status = view(owner, output, *args)
                ptr = ctypes.cast(output, ctypes.POINTER(ctypes.POINTER(n.IrHealthMetadataEventV1)))
                if mutation == "null-view":
                    ptr[0] = ctypes.POINTER(n.IrHealthMetadataEventV1)()
                if mutation == "misaligned-view":
                    ptr[0] = ctypes.cast(ctypes.cast(ptr[0], ctypes.c_void_p).value + 1,
                                         ctypes.POINTER(n.IrHealthMetadataEventV1))
                return n.AMS_MEL_PROVIDER_FAILED if mutation == "failed-view" else status
            with self.subTest(mutation=mutation), \
                    mock.patch.object(n, PREFIX + "receive", side_effect=rec), \
                    mock.patch.object(n, PREFIX + "event_view", side_effect=vw), \
                    mock.patch.object(n, PREFIX + "event_close", wraps=getattr(n, PREFIX + "event_close")) as close:
                self.error(m.ErrorKind.PROVIDER_FAILED if mutation == "failed-view" else PROTOCOL, stream.receive)
                self.assertEqual(close.call_count, 1)

    def test_failed_receive_with_published_event_still_closes_once(self):
        _, _, stream = self.metadata()
        receive = getattr(n, PREFIX + "receive")
        def failed(*args):
            receive(*args)
            return n.AMS_MEL_PROVIDER_FAILED
        with mock.patch.object(n, PREFIX + "receive", side_effect=failed), \
                mock.patch.object(n, PREFIX + "event_close", wraps=getattr(n, PREFIX + "event_close")) as close:
            self.error(m.ErrorKind.PROVIDER_FAILED, stream.receive)
            self.assertEqual(close.call_count, 1)

    def test_event_close_failure_and_consumed_handle_not_retried(self):
        _, _, stream = self.metadata()
        native = getattr(n, PREFIX + "event_close")
        def failed(*args):
            native(*args)
            return n.AMS_MEL_INTERNAL_ERROR
        with mock.patch.object(n, PREFIX + "event_close", side_effect=failed) as close:
            self.error(m.ErrorKind.INTERNAL_ERROR, stream.receive)
            self.assertEqual(close.call_count, 1)
        self.assertEqual(stream.receive().payload, expected_bit())

    def test_event_close_retained_success_or_error_is_not_retried(self):
        for status, kind in ((n.AMS_MEL_OK, PROTOCOL),
                             (n.AMS_MEL_PROVIDER_FAILED, m.ErrorKind.PROVIDER_FAILED)):
            _, _, stream = self.metadata()
            native = getattr(n, PREFIX + "event_close")
            saved = n.IrHealthMetadataEventHandle()
            def retained(owner, diagnostic, capacity, required):
                saved.value = ctypes.cast(owner, ctypes.POINTER(ctypes.c_void_p))[0]
                common.write_diagnostic(diagnostic, capacity, b"")
                common.set_required(required, 1)
                return status
            try:
                with mock.patch.object(n, PREFIX + "event_close", side_effect=retained) as close:
                    self.error(kind, stream.receive)
                    self.assertEqual(close.call_count, 1)
            finally:
                if saved.value:
                    native(ctypes.byref(saved), None, 0, None)

    def test_metadata_close_retained_and_consumed_handle_semantics(self):
        _, _, stream = self.metadata()
        native = stream._close_function
        for status, kind in ((n.AMS_MEL_OK, PROTOCOL),
                             (n.AMS_MEL_PROVIDER_FAILED, m.ErrorKind.PROVIDER_FAILED)):
            def retained(owner, diagnostic, capacity, required):
                common.write_diagnostic(diagnostic, capacity, b"")
                common.set_required(required, 1)
                return status
            with mock.patch.object(stream, "_close_function", side_effect=retained):
                self.error(kind, stream.close)
                self.assertTrue(stream.is_open)
        def consumed(*args):
            native(*args)
            return n.AMS_MEL_PROVIDER_FAILED
        with mock.patch.object(stream, "_close_function", side_effect=consumed) as close:
            self.error(m.ErrorKind.PROVIDER_FAILED, stream.close)
            self.assertFalse(stream.is_open)
            stream.close()
            self.assertEqual(close.call_count, 1)

    def test_counter_errors_retain_native_status(self):
        _, _, stream = self.metadata()
        native = getattr(n, PREFIX + "get_counters")
        def failed(*args):
            native(*args)
            return n.AMS_MEL_INTERNAL_ERROR
        with mock.patch.object(n, PREFIX + "get_counters", side_effect=failed):
            self.error(m.ErrorKind.INTERNAL_ERROR, stream.counters, n.AMS_MEL_INTERNAL_ERROR)

    def test_conversion_exception_preserved_despite_cleanup_exception(self):
        _, _, stream = self.metadata()
        native = getattr(n, PREFIX + "event_close")
        def failed(*args):
            native(*args)
            raise RuntimeError("cleanup")
        with mock.patch.object(_health_metadata, "_event_from_raw", side_effect=MemoryError("copy")), \
                mock.patch.object(n, PREFIX + "event_close", side_effect=failed) as close:
            with self.assertRaisesRegex(MemoryError, "copy"):
                stream.receive()
            self.assertEqual(close.call_count, 1)

    def test_allocation_failure_before_close_call_still_releases_event(self):
        _, _, stream = self.metadata()
        call = _health_metadata._call_with_diagnostic
        calls = 0
        def allocate(operation):
            nonlocal calls
            calls += 1
            if calls == 3:
                raise MemoryError("close diagnostic")
            return call(operation)
        with mock.patch.object(_health_metadata, "_call_with_diagnostic", side_effect=allocate), \
                mock.patch.object(n, PREFIX + "event_close", wraps=getattr(n, PREFIX + "event_close")) as close:
            with self.assertRaisesRegex(MemoryError, "close diagnostic"):
                stream.receive()
            self.assertEqual(close.call_count, 1)

    def test_failed_open_after_owner_publication_preserves_original_and_finalizes_once(self):
        _, channel = self.open("health-rich")
        op = getattr(n, PREFIX + "open")
        native_close = m.HealthMetadataStream._close_function
        baseline = public_owners()
        def failed_open(*args):
            op(*args)
            return n.AMS_MEL_PROVIDER_FAILED
        def failed_close(*args):
            native_close(*args)
            raise MemoryError("cleanup")
        with mock.patch.object(n, PREFIX + "open", side_effect=failed_open), \
                mock.patch.object(m.HealthMetadataStream, "_close_function", staticmethod(mock.Mock(side_effect=failed_close))) as close:
            self.error(m.ErrorKind.PROVIDER_FAILED, channel.open_metadata)
            self.assertEqual(close.__func__.call_count, 1)
        gc.collect()
        self.assertEqual(public_owners(), baseline)
        self.error(m.ErrorKind.INVALID_ARGUMENT, channel.open_metadata)

    def test_open_null_success_counters_uint64_and_local_timeout_validation(self):
        _, channel = self.open("health-rich")
        op = getattr(n, PREFIX + "open")
        def empty(*args):
            status = op(*args)
            m.HealthMetadataStream._close_function(args[2], None, 0, None)
            return status
        with mock.patch.object(n, PREFIX + "open", side_effect=empty):
            self.error(PROTOCOL, channel.open_metadata)
        _, _, stream = self.metadata()
        counters = getattr(n, PREFIX + "get_counters")
        def high(owner, output, *args):
            status = counters(owner, output, *args)
            raw = ctypes.cast(output, ctypes.POINTER(n.IrC2MetadataCountersV1)).contents
            raw.events_received = (1 << 64) - 1
            raw.events_dropped_queue_full = 1 << 63
            raw.malformed_or_unsupported = (1 << 64) - 2
            return status
        with mock.patch.object(n, PREFIX + "get_counters", side_effect=high):
            self.assertEqual(stream.counters(), m.HealthMetadataCounters((1 << 64) - 1, 1 << 63, (1 << 64) - 2))
        with mock.patch.object(n, PREFIX + "receive") as receive:
            for timeout in (-1, 1 << 32, True, "0", .5):
                self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: stream.receive(timeout))
            receive.assert_not_called()


SPANS = (
    (1, "mfa_status.components"), (2, "bit_status.active_bits"), (2, "bit_status.completed_bits"),
    (2, "bit_status.completed_bits.data.0.bit_items"), (2, "bit_status.faults"),
    (2, "bit_status.faults.data.0.fault_data"), (2, "bit_status.faults.data.0.component_ids"),
    (2, "bit_status.faults.data.0.ambiguity_groups"),
    (2, "bit_status.faults.data.0.ambiguity_groups.data.0.diagnostic_test_ids"),
    (2, "bit_status.faults.data.0.ambiguity_groups.data.0.component_ids"),
    (3, "subsystem_status.subsystems"), (3, "subsystem_status.csci"),
    (4, "discrete_status"), (5, "security_audit.artifacts"), (6, "mfa_status_detailed"))


def get_path(raw, path):
    for part in path.split("."):
        raw = raw[int(part)] if part.isdigit() else getattr(raw, part)
    return raw


def set_path(raw, path, value):
    parent, _, field = path.rpartition(".")
    setattr(get_path(raw, parent) if parent else raw, field, value)


def raw_graph(family):
    """Owned, valid zero-domain graph; each nested collection has one item."""
    raw = n.IrHealthMetadataEventV1()
    raw.kind = family
    keep = [raw]
    # Build all arrays with ctypes-owned backing so injection never dereferences
    # fabricated mapped addresses. Set only metadata to malformed values later.
    for _, path in SPANS:
        span = get_path(raw, path)
        array = (span.data._type_ * 1)()
        keep.append(array)
        span.data, span.size = array, 1
    return raw, keep


if __name__ == "__main__":
    unittest.main()
