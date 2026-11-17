//! Run with the compiled mock provider path as the only argument.
use ams_mel_ir::{BufferFlag, Session};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let path = std::env::args_os()
        .nth(1)
        .ok_or("expected compiled mock provider path")?;
    let session = Session::open(path, "foundation-normal", "")?;
    let mut buffer = session.open_host_buffer(16, -7)?;
    buffer.write_storage(0, &[0, 127, 128, 255])?;
    println!("bytes: {:?}", buffer.read_storage(0, 4)?);
    buffer.set_flags(&[BufferFlag::Host])?;
    buffer.add_flag(BufferFlag::Host)?;
    println!("flags: {:?}", buffer.flags()?);
    println!("properties: {:?}", buffer.info()?);
    buffer.close()?;
    session.close()?;
    Ok(())
}
