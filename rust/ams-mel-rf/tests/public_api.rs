//! RF-only consumer coverage, with no dependency on the ams-mel facade.
use ams_mel_rf::{Error, ErrorKind, RfAdmin, RfData, RfJobDataFormat, RfMfaState};

#[test]
fn rf_enumerations_preserve_raw_contracts() {
    assert_eq!(RfMfaState::try_from(6).unwrap(), RfMfaState::Standby);
    assert_eq!(RfMfaState::OperateRxOnly.as_raw(), 8);
    assert_eq!(RfMfaState::try_from(15).unwrap_err().kind(), &ErrorKind::InvalidArgument);
    let future = RfJobDataFormat::from_raw(0xF000_0001);
    assert_eq!(future, RfJobDataFormat::Unknown(0xF000_0001));
    assert_eq!(future.as_raw(), 0xF000_0001);
}

#[test]
fn rf_public_method_signatures_use_common_error_type() {
    fn admin_open() {
        let _method: fn(&mut RfAdmin, RfMfaState) -> Result<bool, Error> =
            RfAdmin::command_state;
    }
    fn data_quantization() {
        let _method: fn(&mut RfData, i64) -> Result<i64, Error> =
            RfData::quantize_duration;
    }
    admin_open();
    data_quantization();
}
