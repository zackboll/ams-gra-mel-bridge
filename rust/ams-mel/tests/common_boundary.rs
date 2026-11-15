//! Compile-time type identity and validation contracts through the compatibility façade.
use ams_mel as legacy;
use ams_mel_common as common;

#[test]
fn shared_types_have_one_identity() {
    let id: legacy::UciId = common::UciId::new([0x42; 16], "sensor").unwrap();
    let common_id: common::UciId = id;
    assert_eq!(common_id.uuid(), &[0x42; 16]);

    let location: common::ComponentLocation =
        legacy::ComponentLocation::new(1.0, -2.0, 3.0, "sensor", "platform").unwrap();
    let legacy_location: legacy::ComponentLocation = location;
    assert_eq!(legacy_location.offset_y_m(), -2.0);

    let abi: legacy::AbiVersion = common::AbiVersion { major: 1, minor: 0 };
    let common_abi: common::AbiVersion = abi;
    assert_eq!(common_abi.major, 1);

    let version: common::ProviderVersion = legacy::ProviderVersion {
        api_version: 1,
        library_version: 2,
        vendor: "vendor".into(),
        description: String::new(),
    };
    let _: legacy::ProviderVersion = version;

    let kind: legacy::ErrorKind = common::ErrorKind::Timeout;
    let common_kind: common::ErrorKind = kind;
    assert_eq!(common_kind, common::ErrorKind::Timeout);
    let error: legacy::Error = common::Error::new(common_kind, "timeout");
    let same_error: common::Error = error;
    assert_eq!(same_error.kind(), &common::ErrorKind::Timeout);
}

#[test]
fn nul_and_native_error_contracts_remain_stable() {
    let bad: legacy::Error = legacy::UciId::new([0; 16], "bad\0label").unwrap_err();
    assert_eq!(bad.kind(), &common::ErrorKind::InvalidArgument);
    assert!(bad.to_string().contains("descriptive_label contains an embedded NUL"));
    let native: common::Error = common::Error::from_native_parts(
        common::ErrorKind::ProviderFailed,
        Some("native failure".into()),
        Some(28),
    );
    let public: legacy::Error = native;
    assert_eq!(public.diagnostic(), Some("native failure"));
    assert_eq!(public.diagnostic_required(), Some(28));
}
