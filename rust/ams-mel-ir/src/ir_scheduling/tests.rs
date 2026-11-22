//! The injected opaque values are never dereferenced or passed to native Close.
use super::*;
use std::cell::Cell;

fn config() -> SchedulingConfig {
    SchedulingConfig::new(
        UciId::new(std::array::from_fn(|i| i as u8), "通道 🛰").unwrap(),
        UciId::new(std::array::from_fn(|i| 0xf0 + i as u8), "plateforme é").unwrap(),
        ComponentLocation::new(1.25, -2.5, -0.0, "位置", "航機").unwrap(),
    )
}

#[test]
fn exact_input_and_null_success() {
    let calls = Cell::new(0);
    let error = open_with(
        &config(),
        |raw, out, d, _, r| {
            calls.set(calls.get() + 1);
            assert_eq!(raw.channel_type, 3);
            assert_eq!(raw.channel_id.uuid, std::array::from_fn(|i| i as u8));
            assert_eq!(
                raw.platform_id.uuid,
                std::array::from_fn(|i| 0xf0 + i as u8)
            );
            assert_eq!(raw.sensor_location.offset_x_m, 1.25);
            assert_eq!(raw.sensor_location.offset_y_m, -2.5);
            assert_eq!(
                raw.sensor_location.offset_z_m.to_bits(),
                (-0.0_f64).to_bits()
            );
            for (view, expected) in [
                (raw.channel_id.descriptive_label, "通道 🛰"),
                (raw.platform_id.descriptive_label, "plateforme é"),
                (raw.sensor_location.key, "位置"),
                (raw.sensor_location.system_name, "航機"),
            ] {
                // SAFETY: the raw strings borrow the live config for this call.
                let bytes = unsafe { slice::from_raw_parts(view.data.cast::<u8>(), view.size) };
                assert_eq!(bytes, expected.as_bytes());
            }
            // SAFETY: helper-provided writable slots, no handle dereference.
            unsafe {
                assert!((*out).is_null());
                *d = 0;
                *r = 1;
            }
            sys::AMS_MEL_OK
        },
        |_| panic!("nothing to release after null-success Open"),
    )
    .unwrap_err();
    assert_eq!(calls.get(), 1);
    assert_eq!(error.kind(), &ErrorKind::ProtocolInconsistency);
    assert_eq!(
        error.diagnostic(),
        Some("Scheduling open succeeded without a channel owner")
    );
}

#[test]
fn unexpected_owner_gets_one_cleanup_without_retrying_open() {
    for (status, diagnostic, kind) in [
        (
            sys::AMS_MEL_PROVIDER_EXCEPTION,
            b"attach threw\0".as_slice(),
            ErrorKind::ProviderException,
        ),
        (
            sys::AMS_MEL_PROVIDER_FAILED,
            b"".as_slice(),
            ErrorKind::ProtocolInconsistency,
        ),
        (
            sys::AMS_MEL_OK,
            b"not empty\0".as_slice(),
            ErrorKind::ProtocolInconsistency,
        ),
        (
            sys::AMS_MEL_PROVIDER_FAILED,
            b"\xff\0".as_slice(),
            ErrorKind::InvalidUtf8,
        ),
    ] {
        let token = ptr::dangling_mut();
        let cleanups = Cell::new(0);
        let error = open_with(
            &config(),
            |_, out, d, _, r| {
                // SAFETY: exact writable slots; diagnostic fits and token is opaque.
                unsafe {
                    *out = token;
                    ptr::copy_nonoverlapping(diagnostic.as_ptr(), d.cast(), diagnostic.len());
                    *r = diagnostic.len();
                }
                status
            },
            |raw| {
                assert_eq!(raw, token);
                cleanups.set(cleanups.get() + 1);
                // Model cleanup retaining the owner; Open must never offer it again.
            },
        )
        .unwrap_err();
        assert_eq!(cleanups.get(), 1);
        assert_eq!(error.kind(), &kind);
        if kind == ErrorKind::ProviderException {
            assert_eq!(error.diagnostic(), Some("attach threw"));
        }
    }
}

#[test]
fn close_only_retries_well_formed_retained_owner() {
    let original = ptr::dangling_mut();
    let replacement = std::ptr::without_provenance_mut(4096);
    let null = ptr::null_mut();
    let failed = sys::AMS_MEL_PROVIDER_FAILED;
    let ok = sys::AMS_MEL_OK;
    for (status, output, required, kind, retained) in [
        (ok, null, 1, None, false),
        (
            ok,
            original,
            1,
            Some(ErrorKind::ProtocolInconsistency),
            false,
        ),
        (
            ok,
            replacement,
            1,
            Some(ErrorKind::ProtocolInconsistency),
            false,
        ),
        (failed, original, 1, Some(ErrorKind::ProviderFailed), true),
        (failed, null, 1, Some(ErrorKind::ProviderFailed), false),
        (
            failed,
            replacement,
            1,
            Some(ErrorKind::ProviderFailed),
            false,
        ),
        (
            failed,
            original,
            0,
            Some(ErrorKind::ProtocolInconsistency),
            false,
        ),
        (
            failed,
            null,
            0,
            Some(ErrorKind::ProtocolInconsistency),
            false,
        ),
        (
            failed,
            original,
            usize::MAX,
            Some(ErrorKind::ProviderFailed),
            false,
        ),
        (
            sys::AMS_MEL_INTERNAL_ERROR,
            original,
            1,
            Some(ErrorKind::InternalError),
            false,
        ),
    ] {
        let mut owner = original;
        let result = close_with(&mut owner, |out, d, _, r| {
            // SAFETY: exact writable slots; injected tokens never reach the ABI.
            unsafe {
                assert_eq!(*out, original);
                *out = output;
                *d = 0;
                *r = required;
            }
            status
        });
        assert_eq!(result.err().map(|error| error.kind().clone()), kind);
        assert_eq!(owner, if retained { original } else { null });
        if !retained {
            close_with(&mut owner, |_, _, _, _| panic!("owner was disarmed")).unwrap();
        }
    }
}

#[test]
fn shared_conversion_keeps_reserved_and_unknown_tags() {
    // SAFETY: all-zero C snapshot contains valid scalar values and empty spans.
    let mut raw: sys::AmsMelIrChannelCapabilityV1 = unsafe { mem::zeroed() };
    let types = [
        sys::AMS_MEL_IR_CHANNEL_SCHEDULING,
        sys::AMS_MEL_IR_CHANNEL_RESERVED_1,
        u32::MAX,
    ];
    let metadata = [sys::AMS_MEL_IR_METADATA_RESERVED_3, u32::MAX];
    raw.channel_types = sys::AmsMelU32SpanV1 {
        data: types.as_ptr(),
        size: types.len(),
    };
    raw.metadata_capabilities = sys::AmsMelU32SpanV1 {
        data: metadata.as_ptr(),
        size: metadata.len(),
    };
    raw.pixel_format = u32::MAX;
    raw.task_schedule_depth = u32::MAX;
    let capabilities = capability_from_raw(&raw).unwrap();
    assert_eq!(
        capabilities.channel_types,
        [
            ChannelType::Scheduling,
            ChannelType::Reserved1,
            ChannelType::Unknown(u32::MAX)
        ]
    );
    assert_eq!(
        capabilities.metadata_capabilities,
        [
            MetadataCapability::Reserved3,
            MetadataCapability::Unknown(u32::MAX)
        ]
    );
    assert_eq!(capabilities.pixel_format, PixelFormat::Unknown(u32::MAX));
    assert_eq!(capabilities.task_schedule_depth, u32::MAX);
}
