//! Private call-injection tests; tokens are never passed to native code.
use super::*;
use std::cell::Cell;

fn config() -> ImageControlConfig {
    ImageControlConfig::new(
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
fn image_control_owned_mapping_and_call_local_storage() {
    let original = config();
    let config = original.clone();
    assert_eq!(original, config);
    drop(original);
    let calls = Cell::new(0);
    let error = open_image_control_with(
        &config,
        |raw, out, d, _, r| {
            calls.set(calls.get() + 1);
            assert_eq!(raw.channel_type, sys::AMS_MEL_IR_CHANNEL_IRST_IMAGE);
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
        Some("Image control open succeeded without a stream owner")
    );
}

#[test]
fn image_control_failed_open_closes_unexpected_owner_once_without_retry() {
    for retain in [false, true] {
        for malformed_diagnostic in [false, true] {
            let closes = Cell::new(0);
            let opens = Cell::new(0);
            let token = std::ptr::dangling_mut::<sys::AmsMelIrStream>();
            let error = open_image_control_with(
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
fn image_control_identity_validation_and_empty_text() {
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
    let config = ImageControlConfig::new(
        UciId::new([0; 16], "").unwrap(),
        UciId::new([0; 16], "").unwrap(),
        ComponentLocation::new(0.0, 0.0, 0.0, "", "").unwrap(),
    );
    let error = open_image_control_with(
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
fn image_control_malformed_success_with_owner_is_closed_once() {
    let closes = Cell::new(0);
    let token = ptr::dangling_mut::<sys::AmsMelIrStream>();
    let error = open_image_control_with(
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
