//! Private call-injection tests; tokens are never passed to native code.
use super::open_with as open_stacked_with;
use super::*;
use std::cell::Cell;

#[test]
fn close_retained_consumed_and_malformed_outputs_are_authoritative() {
    for retained in [true, false] {
        for malformed in [true, false] {
            let token = ptr::dangling_mut::<sys::AmsMelIrStacked>();
            let mut raw = token;
            let calls = Cell::new(0);
            let error = close_with(&mut raw, |out, d, _, r| {
                calls.set(calls.get() + 1);
                // SAFETY: injected writable slots; token never reaches native Close.
                unsafe {
                    if !retained {
                        *out = ptr::null_mut();
                    }
                    *d = 0;
                    *r = usize::from(!malformed);
                }
                sys::AMS_MEL_PROVIDER_FAILED
            })
            .unwrap_err();
            assert_eq!(calls.get(), 1);
            assert_eq!(raw.is_null(), !retained || malformed);
            assert_eq!(
                error.kind(),
                &if malformed {
                    ErrorKind::ProtocolInconsistency
                } else {
                    ErrorKind::ProviderFailed
                }
            );
        }
    }
    let mut raw = ptr::dangling_mut::<sys::AmsMelIrStacked>();
    assert_eq!(
        close_with(&mut raw, |_, d, _, r| {
            // SAFETY: injected diagnostic slots, no owner access.
            unsafe {
                *d = 0;
                *r = 1;
            }
            sys::AMS_MEL_OK
        })
        .unwrap_err()
        .kind(),
        &ErrorKind::ProtocolInconsistency
    );
    assert!(raw.is_null(), "malformed success cannot be released twice");
    for status in [sys::AMS_MEL_INTERNAL_ERROR, sys::AMS_MEL_PROVIDER_EXCEPTION] {
        let mut raw = ptr::dangling_mut::<sys::AmsMelIrStacked>();
        close_with(&mut raw, |_, d, _, r| {
            // SAFETY: injected output storage; uncertain token never reaches FFI.
            unsafe {
                *d = 0;
                *r = 1;
            }
            status
        })
        .unwrap_err();
        assert!(
            raw.is_null(),
            "uncertain status cannot authorize another release"
        );
    }
}

struct SnapshotCalls {
    raw: sys::AmsMelIrChannelCapabilityV1,
    null_view: bool,
    closes: Cell<usize>,
}
impl CapabilityCalls for SnapshotCalls {
    fn view(
        &self,
        _: *mut sys::AmsMelIrChannelCapability,
        out: *mut *const sys::AmsMelIrChannelCapabilityV1,
        d: *mut c_char,
        _: usize,
        r: *mut usize,
    ) -> i32 {
        // SAFETY: injected output slots, self.raw lives throughout conversion.
        unsafe {
            *out = if self.null_view {
                ptr::null()
            } else {
                &self.raw
            };
            *d = 0;
            *r = 1;
        }
        sys::AMS_MEL_OK
    }
    fn close(
        &self,
        out: *mut *mut sys::AmsMelIrChannelCapability,
        _: *mut c_char,
        _: usize,
        r: *mut usize,
    ) -> i32 {
        self.closes.set(self.closes.get() + 1);
        // SAFETY: injected token is compared/cleared, never native-closed.
        unsafe {
            *out = ptr::null_mut();
            if !r.is_null() {
                *r = 1;
            }
        }
        sys::AMS_MEL_OK
    }
}

#[test]
fn shared_snapshot_rejects_malformed_owners_and_conversions_with_one_cleanup() {
    for fault in 0..6 {
        // SAFETY: all-zero C record contains valid scalar representations and empty spans.
        let mut raw: sys::AmsMelIrChannelCapabilityV1 = unsafe { mem::zeroed() };
        match fault {
            3 => {
                raw.channel_types = sys::AmsMelU32SpanV1 {
                    data: ptr::null(),
                    size: 1,
                }
            }
            4 => raw.odc_available = 2,
            5 => {
                raw.channel_id.descriptive_label = sys::AmsMelStringViewV1 {
                    data: b"\xff".as_ptr().cast(),
                    size: 1,
                }
            }
            _ => {}
        }
        let calls = SnapshotCalls {
            raw,
            null_view: fault == 1,
            closes: Cell::new(0),
        };
        let result = capability_snapshot_with(
            |out, d, _, r| {
                // SAFETY: injected slots; opaque token is never dereferenced.
                unsafe {
                    if fault != 0 {
                        *out = ptr::dangling_mut();
                    }
                    *d = 0;
                    *r = 1;
                }
                sys::AMS_MEL_OK
            },
            &calls,
        );
        if fault == 2 {
            result.unwrap();
        } else {
            assert_eq!(
                result.unwrap_err().kind(),
                &if fault == 5 {
                    ErrorKind::InvalidUtf8
                } else {
                    ErrorKind::ProtocolInconsistency
                }
            );
        }
        assert_eq!(calls.closes.get(), usize::from(fault != 0));
    }
}

#[test]
fn shared_snapshot_preserves_unknown_collections_and_maximum_depth() {
    // SAFETY: valid zero scalar representations and empty spans, filled below.
    let mut raw: sys::AmsMelIrChannelCapabilityV1 = unsafe { mem::zeroed() };
    let types = [6, 7, u32::MAX];
    let metadata = [u32::MAX];
    raw.channel_types = sys::AmsMelU32SpanV1 {
        data: types.as_ptr(),
        size: types.len(),
    };
    raw.metadata_capabilities = sys::AmsMelU32SpanV1 {
        data: metadata.as_ptr(),
        size: metadata.len(),
    };
    raw.task_schedule_depth = u32::MAX;
    raw.pixel_format = u32::MAX;
    let calls = SnapshotCalls {
        raw,
        null_view: false,
        closes: Cell::new(0),
    };
    let value = capability_snapshot_with(
        |out, d, _, r| {
            // SAFETY: injected slots; borrowed collections live throughout snapshot.
            unsafe {
                *out = ptr::dangling_mut();
                *d = 0;
                *r = 1;
            }
            sys::AMS_MEL_OK
        },
        &calls,
    )
    .unwrap();
    assert_eq!(
        value.channel_types,
        [
            ChannelType::StackedImage,
            ChannelType::Reserved1,
            ChannelType::Unknown(u32::MAX)
        ]
    );
    assert_eq!(
        value.metadata_capabilities,
        [MetadataCapability::Unknown(u32::MAX)]
    );
    assert_eq!(value.pixel_format, PixelFormat::Unknown(u32::MAX));
    assert_eq!(value.task_schedule_depth, u32::MAX);
    assert_eq!(calls.closes.get(), 1);
}

fn config() -> StackedConfig {
    StackedConfig::new(
        UciId::new(std::array::from_fn(|i| i as u8), String::from("通道 🛰")).unwrap(),
        UciId::new(
            std::array::from_fn(|i| 0xf0 + i as u8),
            String::from("平台 é"),
        )
        .unwrap(),
        ComponentLocation::new(1.25, -2.5, -0.0, String::from("位置"), String::from("航機"))
            .unwrap(),
    )
}

#[test]
fn stacked_owned_mapping_and_call_local_storage() {
    let original = config();
    let config = original.clone();
    assert_eq!(original, config);
    drop(original);
    let calls = Cell::new(0);
    let error = open_stacked_with(
        &config,
        |raw, out, d, _, r| {
            calls.set(calls.get() + 1);
            assert_eq!(raw.channel_type, sys::AMS_MEL_IR_CHANNEL_STACKED_IMAGE);
            assert_eq!(raw.channel_id.uuid, std::array::from_fn(|i| i as u8));
            assert_eq!(
                raw.platform_id.uuid,
                std::array::from_fn(|i| 0xf0 + i as u8)
            );
            assert_eq!(raw.sensor_location.offset_x_m, 1.25);
            assert_eq!(raw.sensor_location.offset_y_m, -2.5);
            assert_eq!(
                raw.sensor_location.offset_z_m.to_bits(),
                (-0.0f64).to_bits()
            );
            for (view, expected) in [
                (raw.channel_id.descriptive_label, "通道 🛰"),
                (raw.platform_id.descriptive_label, "平台 é"),
                (raw.sensor_location.key, "位置"),
                (raw.sensor_location.system_name, "航機"),
            ] {
                // SAFETY: helper must keep all config-owned bytes alive during Open.
                let bytes =
                    unsafe { std::slice::from_raw_parts(view.data.cast::<u8>(), view.size) };
                assert_eq!(bytes, expected.as_bytes());
            }
            // SAFETY: exact live output and diagnostic storage from the helper.
            unsafe {
                assert!((*out).is_null());
                *d = 0;
                *r = 1;
            }
            sys::AMS_MEL_OK
        },
        |_| panic!("null success must not close an owner"),
    )
    .unwrap_err();
    assert_eq!(calls.get(), 1);
    assert_eq!(error.kind(), &ErrorKind::ProtocolInconsistency);
    assert_eq!(
        error.diagnostic(),
        Some("Stacked open succeeded without a channel owner")
    );
}

#[test]
fn stacked_failed_open_closes_unexpected_owner_once_without_retry() {
    for retain in [false, true] {
        for malformed_diagnostic in [false, true] {
            let closes = Cell::new(0);
            let opens = Cell::new(0);
            let token = std::ptr::dangling_mut::<sys::AmsMelIrStacked>();
            let error = open_stacked_with(
                &config(),
                |_, out, d, _, r| {
                    opens.set(opens.get() + 1);
                    // SAFETY: writable helper slots; opaque token never dereferenced.
                    unsafe {
                        *out = token;
                        if malformed_diagnostic {
                            *r = 0;
                        } else {
                            std::ptr::copy_nonoverlapping(c"attach rejected".as_ptr(), d, 16);
                            *r = 16;
                        }
                    }
                    sys::AMS_MEL_PROVIDER_EXCEPTION
                },
                |raw| {
                    closes.set(closes.get() + 1);
                    assert_eq!(*raw, token);
                    if !retain {
                        *raw = ptr::null_mut();
                    }
                },
            )
            .unwrap_err();
            assert_eq!(opens.get(), 1);
            assert_eq!(closes.get(), 1);
            if malformed_diagnostic {
                assert_eq!(error.kind(), &ErrorKind::ProtocolInconsistency);
            } else {
                assert_eq!(error.kind(), &ErrorKind::ProviderException);
                assert_eq!(error.diagnostic(), Some("attach rejected"));
            }
        }
    }
}

#[test]
fn stacked_identity_validation_and_empty_text() {
    for label in ["channel\0suffix", "platform\0suffix"] {
        assert_eq!(
            UciId::new([0; 16], label).unwrap_err().kind(),
            &ErrorKind::InvalidArgument
        );
    }
    for (key, system) in [("key\0suffix", "system"), ("key", "system\0suffix")] {
        assert_eq!(
            ComponentLocation::new(0.0, 0.0, 0.0, key, system)
                .unwrap_err()
                .kind(),
            &ErrorKind::InvalidArgument
        );
    }
    let config = StackedConfig::new(
        UciId::new([0; 16], "").unwrap(),
        UciId::new([0; 16], "").unwrap(),
        ComponentLocation::new(0.0, 0.0, 0.0, "", "").unwrap(),
    );
    let error = open_stacked_with(
        &config,
        |raw, _, d, _, r| {
            for view in [
                raw.channel_id.descriptive_label,
                raw.platform_id.descriptive_label,
                raw.sensor_location.key,
                raw.sensor_location.system_name,
            ] {
                assert!(view.data.is_null());
                assert_eq!(view.size, 0);
            }
            // SAFETY: exact helper-provided diagnostic storage.
            unsafe {
                *d = 0;
                *r = 1;
            }
            sys::AMS_MEL_FACTORY_FAILED
        },
        |_| panic!("no output owner"),
    )
    .unwrap_err();
    assert_eq!(error.kind(), &ErrorKind::FactoryFailed);
}

#[test]
fn stacked_malformed_success_with_owner_is_closed_once() {
    let closes = Cell::new(0);
    let token = ptr::dangling_mut::<sys::AmsMelIrStacked>();
    let error = open_stacked_with(
        &config(),
        |_, out, d, _, r| {
            // SAFETY: exact helper slots; token never dereferenced/native-closed.
            unsafe {
                *out = token;
                *d = b'x' as c_char;
                *d.add(1) = 0;
                *r = 2;
            }
            sys::AMS_MEL_OK
        },
        |out| {
            assert_eq!(*out, token);
            closes.set(closes.get() + 1);
            *out = ptr::null_mut();
        },
    )
    .unwrap_err();
    assert_eq!(error.kind(), &ErrorKind::ProtocolInconsistency);
    assert_eq!(closes.get(), 1);
}
