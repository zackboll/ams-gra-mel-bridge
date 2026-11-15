//! Opt-in actual provider qualification, exclusively public safe Rust.
#![forbid(unsafe_code)]
use ams_mel::*;

#[test]
#[ignore = "requires AMS_MEL_REFERENCE_PROVIDER at verified revision cf22a80"]
fn reference_stacked_foundation() {
    let library = std::env::var_os("AMS_MEL_REFERENCE_PROVIDER").expect("actual reference DSO");
    let session = Session::open(library, "", "").unwrap();
    let version = session.provider_version().unwrap();
    println!("reference version: {version:?}");
    assert_eq!(version.vendor, "minimal-mel-example");
    assert_eq!((version.api_version, version.library_version), (4, 1));
    let capabilities = session.capabilities().unwrap();
    let choices: Vec<_> = capabilities
        .iter()
        .filter(|c| c.channel_types.contains(&ChannelType::StackedImage))
        .collect();
    assert_eq!(choices.len(), 1);
    let discovered = choices[0];
    let config = StackedConfig::new(
        discovered.channel_id.clone(),
        discovered.platform_id.clone(),
        discovered.sensor_location.clone(),
    );
    let mut stacked = session.open_stacked_channel(&config).unwrap();
    let attached = stacked.capabilities().unwrap();
    assert_eq!(&attached, discovered);
    assert_eq!(
        (
            attached.height,
            attached.width,
            attached.bit_depth,
            attached.row_pitch,
            attached.buffer_size,
            attached.image_size,
            attached.number_of_bands
        ),
        (0, 0, 0, 0, 0, 0, 0)
    );
    println!("Stacked capability: {attached:?}");
    let mut view = stacked.channel_view().unwrap();
    assert_eq!(view.capabilities().unwrap(), attached);
    let mut keepalive = view.send_keepalive().unwrap();
    assert_eq!(
        keepalive.wait(15000).unwrap(),
        ReturnResult::Completed {
            value: CommandReturn::Success
        }
    );
    keepalive.close().unwrap();
    let ids = CommsTestRequest {
        command_id: u32::MAX,
        // The reference stub documents ChannelType as its numeric routing ID.
        channel_id: 6,
        request_id: 0xf000_0002,
    };
    let mut comms = view.submit_comms_test(ids).unwrap();
    assert_eq!(
        comms.wait(15000).unwrap(),
        CommsTestResult::Completed {
            report: CommsTestReport {
                command_id: ids.command_id,
                request_id: ids.request_id
            }
        }
    );
    comms.close().unwrap();
    stacked.enable().unwrap();
    assert_eq!(stacked.capabilities().unwrap(), attached);
    view.close().unwrap();
    stacked.close().unwrap();
    session.close().unwrap();
    println!("PASS: reference Stacked foundation; no acquisition or callback registration");
}
