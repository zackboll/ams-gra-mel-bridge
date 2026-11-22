//! Demonstrates a successful lifecycle against the compiled mock provider.
use ams_mel_ir::{BufferFlag, ComponentLocation, ImageControlConfig, Session, UciId};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let path = std::env::args_os()
        .nth(1)
        .ok_or("expected mock provider path")?;
    let session = Session::open(path, "registration020-normal", "")?;
    let config = ImageControlConfig::new(
        UciId::new([1; 16], "image")?,
        UciId::new([2; 16], "platform")?,
        ComponentLocation::new(0.0, 0.0, 0.0, "sensor", "platform")?,
    );
    let mut image = session.open_image_control(&config)?;
    let mut channel = image.channel_view()?;
    let mut buffer = session.open_host_buffer(16, -20)?;
    buffer.write_storage(0, &[0, 127, 128, 255])?;
    buffer.set_flags(&[BufferFlag::Host])?;

    channel.register_buffer(&mut buffer)?;
    image.start()?;
    channel.disable()?;
    channel.unregister_buffer(&mut buffer)?;

    println!("bytes after unregister: {:?}", buffer.read_storage(0, 4)?);
    buffer.close()?;
    image.try_close()?;
    channel.close()?;
    session.close()?;
    Ok(())
}
