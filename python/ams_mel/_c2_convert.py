"""Checked C2 deep copies; only the selected event payload is inspected."""

from __future__ import annotations

from . import _native, _c2_types as t
from ._ffi import _checked_span, _copy_string_view, _copy_uci_id
from ._health_convert import _bit, _enum
from ._types import CommsTestReport


def _span(raw, convert, field: str) -> tuple:
    return _checked_span(raw.data, raw.size, raw.data._type_, convert, field)


def _text(raw) -> str:
    return _copy_string_view(raw, "C2 metadata text")


def _id(raw):
    return _copy_uci_id(raw, "C2 metadata identifier")


def _bit_type(raw) -> t.C2BitType:
    return t.C2BitType(
        _id(raw.bit_id), _enum(t.C2BitControlInterface, raw.accepted_interface),
        _span(raw.bit_item_names, _text, "C2 BIT item names"),
        _span(raw.subsystem_component_ids, _id, "C2 BIT component identifiers"),
        int(raw.expected_duration_ns))


def _event_from_raw(raw: _native.IrC2MetadataEventV1) -> t.C2MetadataEvent:
    kind = _enum(t.C2MetadataKind, raw.kind)
    if kind == t.C2MetadataKind.COMMAND_STATUS:
        status = raw.command_status
        payload = t.C2CommandStatus(
            int(status.command_id), _enum(t.C2CommandState, status.state),
            _enum(t.C2CannotComply, status.reason_id), _text(status.reason_description))
    elif kind == t.C2MetadataKind.BIT_CONFIGURATION:
        payload = t.C2BitConfiguration(
            _span(raw.bit_configuration.bit_types, _bit_type, "C2 BIT types"))
    elif kind == t.C2MetadataKind.BIT_STATUS:
        payload = _bit(raw.bit_status)
    else:
        report = raw.channel_comms_test
        payload = CommsTestReport(int(report.command_id), int(report.request_id))
    return t.C2MetadataEvent(kind, payload)