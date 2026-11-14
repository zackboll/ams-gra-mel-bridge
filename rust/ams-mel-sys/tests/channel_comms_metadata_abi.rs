use ams_mel_sys::*;
use std::ffi::c_char;

#[test]
fn channel_comms_metadata_signatures() {
    let _: unsafe extern "C" fn(
        *const AmsMelIrChannel,
        usize,
        *mut *mut AmsMelIrChannelCommsMetadata,
        *mut c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_channel_comms_metadata_open;
    let _: unsafe extern "C" fn(
        *const AmsMelIrChannelCommsMetadata,
        u32,
        *mut AmsMelIrChannelCommsTestReportV1,
        *mut c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_channel_comms_metadata_receive;
    let _: unsafe extern "C" fn(
        *const AmsMelIrChannelCommsMetadata,
        *mut AmsMelIrC2MetadataCountersV1,
        *mut c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_channel_comms_metadata_get_counters;
    let _: unsafe extern "C" fn(
        *mut *mut AmsMelIrChannelCommsMetadata,
        *mut c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_channel_comms_metadata_close;
    assert_eq!(std::mem::size_of::<AmsMelIrChannelCommsTestReportV1>(), 8);
    assert_eq!(
        std::mem::offset_of!(AmsMelIrChannelCommsTestReportV1, request_id),
        4
    );
    assert_eq!(std::mem::size_of::<AmsMelIrC2MetadataCountersV1>(), 24);
}
