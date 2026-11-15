//! Provider-free IR crate consumer surface tests; no compatibility-façade dependency.
use ams_mel_ir::{
    AbiVersion, ComponentLocation, Error, ErrorKind, ImageStream, NavigationReport, Session,
    RfMfaState, UciId,
};

#[test]
fn common_values_are_owned_and_provider_free() {
    let id = UciId::new([7; 16], "IR image").unwrap();
    assert_eq!(id.uuid(), &[7; 16]);
    let location = ComponentLocation::new(1.0, -2.0, 3.5, "sensor", "platform").unwrap();
    assert_eq!(location.offset_z_m(), 3.5);
    let state: RfMfaState = ams_mel_common::RfMfaState::Standby;
    assert_eq!(state.as_raw(), 6);
    let version = AbiVersion { major: 1, minor: 0 };
    assert_eq!(version.major, 1);
    let error: Error = UciId::new([0; 16], "embedded\0NUL").unwrap_err();
    assert_eq!(error.kind(), &ErrorKind::InvalidArgument);
}

#[test]
fn owner_and_navigation_types_are_available_without_rf_crate() {
    let _session_identity: fn(Session) -> Session = |value| value;
    let _image_identity: fn(ImageStream) -> ImageStream = |value| value;
    let _navigation_identity: fn(NavigationReport) -> NavigationReport = |value| value;
}
