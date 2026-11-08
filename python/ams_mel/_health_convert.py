"""Checked deep copies; callers keep the native event alive throughout."""

from __future__ import annotations

from enum import IntEnum
from typing import Callable, TypeVar

from . import _native, _health_types as t
from ._errors import ErrorKind, _local_error
from ._ffi import _checked_span, _copy_bool, _copy_string_view, _copy_uci_id
from ._types import ComponentLocation, MFAState


_E = TypeVar("_E", bound=IntEnum)


def _enum(domain: type[_E], raw: int) -> _E:
    try:
        return domain(int(raw))
    except ValueError as error:
        raise _local_error(
            ErrorKind.PROTOCOL_INCONSISTENCY,
            f"native {domain.__name__} has unpublished value {raw}",
        ) from error


def _span(raw, convert: Callable, field: str) -> tuple:
    # The element type comes from the declared ctypes pointer, not provider data.
    return _checked_span(raw.data, raw.size, raw.data._type_, convert, field)


def _text(raw) -> str:
    return _copy_string_view(raw, "Health metadata text")


def _id(raw):
    return _copy_uci_id(raw, "Health metadata identifier")


def _ids(raw) -> tuple:
    return _span(raw, _id, "Health metadata identifiers")


def _euler(raw) -> t.Euler:
    return t.Euler(float(raw.roll), float(raw.pitch), float(raw.yaw))


def _component(raw) -> t.MfaComponent:
    details = raw.installation_details
    location = details.location
    return t.MfaComponent(
        _id(raw.component_id), _enum(t.ComponentState, raw.state),
        t.TemperatureStatus(float(raw.temperature.temperature_c),
                            _enum(t.TemperatureState, raw.temperature.state)),
        t.ForeignKey(_text(raw.installation_location_id.key),
                     _text(raw.installation_location_id.system_name)),
        t.InstallationDetails(
            ComponentLocation(float(location.offset_x_m), float(location.offset_y_m),
                              float(location.offset_z_m), _text(location.key),
                              _text(location.system_name)),
            _euler(details.orientation), _euler(details.boresight)),
    )


def _mfa(raw) -> t.MfaStatus:
    about = raw.about
    return t.MfaStatus(
        _enum(MFAState, raw.state), _text(raw.state_description),
        _text(raw.mode_description), _enum(t.StateTransitionStatus, raw.transition_status),
        t.About(_text(about.model), _text(about.serial_number),
                _text(about.software_version), _text(about.bootloader_software_version),
                _text(about.hardware_version)),
        _span(raw.components, _component, "MFA components"),
    )


def _active(raw) -> t.ActiveBit:
    return t.ActiveBit(_id(raw.bit_id), int(raw.estimated_completion_time_ns),
                       float(raw.estimated_percent_complete))


def _bit_item(raw) -> t.CompletedBitItem:
    return t.CompletedBitItem(_text(raw.bit_item_name), _enum(t.BitResult, raw.result),
                              _text(raw.fail_reason))


def _completed(raw) -> t.CompletedBit:
    return t.CompletedBit(
        _id(raw.bit_id), int(raw.time_tag_ns), _enum(t.BitResult, raw.result),
        _text(raw.fail_reason), _span(raw.bit_items, _bit_item, "BIT items"),
    )


def _fault_data(raw) -> t.FaultData:
    return t.FaultData(_text(raw.key), _text(raw.value), _text(raw.format), _text(raw.units))


def _ambiguity(raw) -> t.FaultAmbiguityGroup:
    return t.FaultAmbiguityGroup(_ids(raw.diagnostic_test_ids), _ids(raw.component_ids))


def _fault(raw) -> t.Fault:
    return t.Fault(
        _id(raw.fault_id), _enum(t.FaultSeverity, raw.severity), _enum(t.FaultState, raw.state),
        _span(raw.fault_data, _fault_data, "fault data"), int(raw.detection_time_ns),
        _text(raw.fault_code), _text(raw.fault_description), _ids(raw.component_ids),
        _span(raw.ambiguity_groups, _ambiguity, "fault ambiguity groups"),
    )


def _bit(raw) -> t.BitStatus:
    return t.BitStatus(_span(raw.active_bits, _active, "active BITs"),
                       _span(raw.completed_bits, _completed, "completed BITs"),
                       _span(raw.faults, _fault, "faults"))


def _dependency(raw) -> t.SubsystemDependency:
    return t.SubsystemDependency(int(raw.subsystem_id), int(raw.criticality),
                                  _enum(t.FailureLevel, raw.failure))


def _csci(raw) -> t.CsciInfo:
    version = raw.version
    return t.CsciInfo(
        _text(raw.csci), _enum(t.CsciMode, raw.mode),
        t.CsciVersion(int(version.source), int(version.major_revision),
                      int(version.minor_revision), int(version.engineering_revision)),
        int(raw.criticality), _enum(t.FailureLevel, raw.failure), int(raw.bit_report),
        _copy_bool(raw.connection_established, "CSCI connection_established"),
    )


def _subsystem(raw) -> t.SubsystemStatus:
    return t.SubsystemStatus(
        int(raw.subsystem_id), int(raw.criticality), int(raw.status_sequence_number),
        _enum(t.FailureLevel, raw.failure), int(raw.subsystem_count),
        _span(raw.subsystems, _dependency, "subsystem dependencies"), int(raw.csci_count),
        _span(raw.csci, _csci, "CSCI items"),
    )


def _pairs(raw) -> tuple[t.NameValuePair, ...]:
    return _span(raw, lambda pair: t.NameValuePair(_text(pair.name), _text(pair.value)),
                 "Health name/value pairs")


def _unset(raw, field: str) -> None:
    if any(raw.uuid) or raw.descriptive_label.size:
        raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                           f"native inactive security {field} is set")


def _security_event(raw) -> t._SecurityEvent:
    kind = _enum(t.SecurityEventKind, raw.kind)
    if kind != t.SecurityEventKind.AUTHENTICATION:
        _unset(raw.subsystem_id, "subsystem_id")
        _unset(raw.service_id, "service_id")
    if kind not in (t.SecurityEventKind.AUTHENTICATION, t.SecurityEventKind.INTEGRITY,
                    t.SecurityEventKind.FILE_MANAGEMENT):
        _unset(raw.mdf_id, "mdf_id")
    if kind == t.SecurityEventKind.NONE:
        if raw.category or raw.details.size:
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "native None security event carries category or details")
        return t.SecurityNone()
    details = _text(raw.details)
    if kind == t.SecurityEventKind.AUTHENTICATION:
        return t.SecurityAuthentication(
            _enum(t.SecurityAuthenticationCategory, raw.category), details,
            _id(raw.subsystem_id), _id(raw.service_id), _id(raw.mdf_id))
    if kind == t.SecurityEventKind.INTEGRITY:
        return t.SecurityIntegrity(_enum(t.SecurityIntegrityCategory, raw.category),
                                   details, _id(raw.mdf_id))
    if kind == t.SecurityEventKind.FILE_MANAGEMENT:
        return t.SecurityFileManagement(_enum(t.SecurityFileManagementCategory, raw.category),
                                        details, _id(raw.mdf_id))
    if kind == t.SecurityEventKind.KEY_MANAGEMENT:
        return t.SecurityKeyManagement(_enum(t.SecurityKeyManagementCategory, raw.category), details)
    if kind == t.SecurityEventKind.SYSTEM:
        return t.SecuritySystem(_enum(t.SecuritySystemCategory, raw.category), details)
    return t.SecuritySanitization(_enum(t.SecuritySanitizationCategory, raw.category), details)


def _security(raw) -> t.SecurityAuditRecord:
    return t.SecurityAuditRecord(
        _id(raw.security_event_id), int(raw.event_timestamp_ns), _id(raw.subsystem_id),
        _span(raw.artifacts, lambda a: t.SecurityArtifact(_id(a.component_id), _id(a.associated_id)),
              "security artifacts"),
        _security_event(raw.event), _enum(t.SecurityOutcome, raw.outcome),
        _enum(t.SecuritySeverity, raw.severity),
    )


def _event_from_raw(raw: _native.IrHealthMetadataEventV1) -> t.HealthMetadataEvent:
    kind = _enum(t.HealthMetadataKind, raw.kind)
    # Do not inspect inactive payloads: their fields need not be valid records.
    if kind == t.HealthMetadataKind.MFA_STATUS:
        payload = _mfa(raw.mfa_status)
    elif kind == t.HealthMetadataKind.BIT_STATUS:
        payload = _bit(raw.bit_status)
    elif kind == t.HealthMetadataKind.SUBSYSTEM_STATUS:
        payload = _subsystem(raw.subsystem_status)
    elif kind == t.HealthMetadataKind.DISCRETE_STATUS:
        payload = t.DiscreteStatus(_pairs(raw.discrete_status))
    elif kind == t.HealthMetadataKind.SECURITY_AUDIT:
        payload = _security(raw.security_audit)
    else:
        payload = t.MfaStatusDetailed(_pairs(raw.mfa_status_detailed))
    return t.HealthMetadataEvent(kind, payload)
