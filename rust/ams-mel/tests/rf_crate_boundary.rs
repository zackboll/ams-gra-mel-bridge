//! Compile-time RF type identity through the backwards compatible ams-mel facade.
use ams_mel as legacy;
use ams_mel_rf as rf;

#[test]
fn reexports_are_identical_types_not_wrappers() {
    let state: legacy::RfMfaState = rf::RfMfaState::Standby;
    let _: rf::RfMfaState = state;
    let format: legacy::RfJobDataFormat = rf::RfJobDataFormat::from_raw(0xFFFF_0000);
    let _: rf::RfJobDataFormat = format;

    let _admin: fn(rf::RfAdmin) -> legacy::RfAdmin = |value| value;
    let _data: fn(rf::RfData) -> legacy::RfData = |value| value;
    let _info: fn(rf::RfMfaInfo) -> legacy::RfMfaInfo = |value| value;
    let _face: fn(rf::RfFaceInfo) -> legacy::RfFaceInfo = |value| value;
    let _range: fn(rf::RfFrequencyRange) -> legacy::RfFrequencyRange = |value| value;
    let _common: fn(rf::Error) -> legacy::Error = |value| value;
    let _version: fn(rf::ProviderVersion) -> legacy::ProviderVersion = |value| value;
}
