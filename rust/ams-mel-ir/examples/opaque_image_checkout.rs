//! Receives one genuine opaque callback from the compiled mock provider.
use ams_mel_ir::{
    CommandReturn, ComponentLocation, ErrorKind, ImageControlConfig, ReturnResult, Session, UciId,
};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let path = std::env::args_os()
        .nth(1)
        .ok_or("expected mock provider path")?;
    let session = Session::open(path, "checkout021-normal", "")?;
    let config = ImageControlConfig::new(
        UciId::new([1; 16], "image")?,
        UciId::new([2; 16], "platform")?,
        ComponentLocation::new(0.0, 0.0, 0.0, "sensor", "platform")?,
    );
    let mut image = session.open_image_control(&config)?;
    let mut receiver = image.open_checkout_receiver(2, 3)?;
    let mut channel = image.channel_view()?;
    let mut buffer = session.open_host_buffer(16, -20)?;
    buffer.write_storage(0, &[0, 127, 128, 255])?;
    channel.register_buffer(&mut buffer)?;
    image.start()?;

    // This compiled mock deliberately delivers onImage during KeepAlive. Real
    // cameras need not deliver any image in response to KeepAlive.
    let mut trigger = channel.send_keepalive()?;
    if !matches!(
        trigger.wait(1_000)?,
        ReturnResult::Completed {
            value: CommandReturn::Success
        }
    ) {
        return Err("mock KeepAlive failed".into());
    }
    trigger.close()?;
    let mut lease = receiver.receive(1_000)?;
    let counters = receiver.counters()?;
    assert_eq!(counters.checkouts_accepted, 1);
    println!("Received an opaque checkout: {counters:?}");

    channel.disable()?;
    let blocked = channel
        .unregister_buffer(&mut buffer)
        .expect_err("held lease must block unregister");
    assert_eq!(blocked.error.kind(), &ErrorKind::ProviderFailed);
    assert_eq!(blocked.provider_return, None);
    lease.close()?;
    assert_eq!(receiver.counters()?.releases_succeeded, 1);
    receiver.close()?;
    channel.unregister_buffer(&mut buffer)?;
    buffer.close()?;
    image.try_close()?;
    channel.close()?;
    session.close()?;
    println!("Released the checkout and unregistered the application Buffer.");
    Ok(())
}
