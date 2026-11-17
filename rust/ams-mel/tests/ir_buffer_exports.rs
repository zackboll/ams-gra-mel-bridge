//! Compatibility exports must be the same concrete types, not wrapper owners.
use ams_mel::{
    BufferFlag, BufferFlagsError, BufferInfo, BufferOpenError, HostBuffer, ImageLocation, Session,
};

#[test]
fn buffer_exports_share_identity() {
    let _: fn(&Session, u64, i64) -> Result<HostBuffer, BufferOpenError> =
        ams_mel_ir::Session::open_host_buffer;
    let _: fn(&HostBuffer) -> Result<BufferInfo, ams_mel::Error> = ams_mel_ir::HostBuffer::info;
    let _: fn(&HostBuffer) -> Result<Vec<BufferFlag>, BufferFlagsError> =
        ams_mel_ir::HostBuffer::flags;
    let location: ImageLocation = ams_mel_ir::ImageLocation::OutsideStorage;
    assert_eq!(location, ImageLocation::OutsideStorage);
    assert_eq!(BufferFlag::Host as u32, 0);
    assert_eq!(BufferFlag::Gpu as u32, 1);
    assert_eq!(BufferFlag::Rdma as u32, 2);
}
