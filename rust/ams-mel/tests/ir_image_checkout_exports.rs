use ams_mel::{
    CheckoutReleaseError, Error, ImageCheckoutCounters, ImageCheckoutLease, ImageCheckoutReceiver,
    ImageStream,
};

#[test]
fn compatibility_exports_have_the_same_concrete_types() {
    let _: fn(&mut ImageStream, usize, usize) -> Result<ImageCheckoutReceiver, Error> =
        ams_mel_ir::ImageStream::open_checkout_receiver;
    let _: fn(&mut ImageCheckoutReceiver, u32) -> Result<ImageCheckoutLease, Error> =
        ams_mel_ir::ImageCheckoutReceiver::receive;
    let _: fn(&ImageCheckoutReceiver) -> Result<ImageCheckoutCounters, Error> =
        ams_mel_ir::ImageCheckoutReceiver::counters;
    let _: fn(&mut ImageCheckoutLease) -> Result<(), CheckoutReleaseError> =
        ams_mel_ir::ImageCheckoutLease::close;
}
