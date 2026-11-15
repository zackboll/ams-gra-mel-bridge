//! Opt-in actual C++ provider interoperability, exclusively public safe Rust.
#![forbid(unsafe_code)]
use ams_mel::*;

fn select(caps: &[ChannelCapability], kind: ChannelType) -> &ChannelCapability {
    let mut matches = caps.iter().filter(|c| c.channel_types.contains(&kind));
    let cap = matches.next().unwrap();
    assert!(matches.next().is_none());
    assert_eq!(cap.channel_types, [kind]);
    cap
}

#[test]
#[ignore = "requires actual AMS_MEL_REFERENCE_PROVIDER pinned at cf22a80ec98398eb361df758ada7bfb4024710a3"]
fn reference_eight_futures_and_eight_callbacks() {
    let path = std::env::var_os("AMS_MEL_REFERENCE_PROVIDER").expect("actual reference DSO path");
    println!("provider path: {}", std::path::Path::new(&path).display());
    let session = Session::open(path, "rust-ir-017", "").unwrap();
    let version = session.provider_version().unwrap();
    assert_eq!(version.vendor, "minimal-mel-example");
    assert_eq!((version.api_version, version.library_version), (4, 1));
    println!("actual provider version: {version:?}");
    let caps = session.capabilities().unwrap();
    assert_eq!(caps.len(), 4);
    let c = select(&caps, ChannelType::CommandAndControl);
    let h = select(&caps, ChannelType::HealthAndStatus);
    let i = select(&caps, ChannelType::IrstImage);
    let s = select(&caps, ChannelType::StackedImage);
    let mut c2 = session
        .open_control_channel(&ControlConfig::new(
            c.channel_id.clone(),
            c.platform_id.clone(),
            c.sensor_location.clone(),
        ))
        .unwrap();
    let mut health = session
        .open_health_channel(&HealthConfig::new(
            h.channel_id.clone(),
            h.platform_id.clone(),
            h.sensor_location.clone(),
        ))
        .unwrap();
    let mut image = session
        .open_image_control(&ImageControlConfig::new(
            i.channel_id.clone(),
            i.platform_id.clone(),
            i.sensor_location.clone(),
        ))
        .unwrap();
    let mut stacked = session
        .open_stacked_channel(&StackedConfig::new(
            s.channel_id.clone(),
            s.platform_id.clone(),
            s.sensor_location.clone(),
        ))
        .unwrap();
    let mut views = [
        c2.channel_view().unwrap(),
        health.channel_view().unwrap(),
        image.channel_view().unwrap(),
        stacked.channel_view().unwrap(),
    ];
    for (view, expected) in views.iter_mut().zip([c, h, i, s]) {
        let attached = view.capabilities().unwrap();
        assert_eq!(attached.channel_id, expected.channel_id);
        assert_eq!(attached.platform_id, expected.platform_id);
        assert_eq!(attached.sensor_location, expected.sensor_location);
        assert_eq!(attached.channel_types, expected.channel_types);
        assert_eq!(
            (attached.width, attached.height, attached.buffer_size),
            (0, 0, 0)
        );
        println!("discovered and attached: {attached:?}");
    }
    // Native common C2 rejection must leave dedicated metadata registration untouched.
    let error = views[0].open_comms_metadata(8).unwrap_err();
    assert_eq!(error.error().kind(), &ErrorKind::ProviderFailed);
    assert_eq!(error.registration(), None);
    let mut c2_metadata = c2.open_metadata(8).unwrap();
    c2_metadata.enable_comms_test_events().unwrap();
    let mut common = [
        views[1].open_comms_metadata(8).unwrap(),
        views[2].open_comms_metadata(8).unwrap(),
        views[3].open_comms_metadata(8).unwrap(),
    ];
    c2.enable().unwrap();
    health.enable().unwrap();
    image.start().unwrap();
    stacked.enable().unwrap();
    let mut expected = [[CommsTestReport {
        command_id: 0,
        request_id: 0,
    }; 2]; 4];
    let mut futures = 0;
    // ONLY this reference stub uses ChannelType numeric values as routing IDs.
    for (index, route) in [2, 4, 1, 6].into_iter().enumerate() {
        for (routed, expected_report) in expected[index].iter_mut().enumerate() {
            let request = CommsTestRequest {
                command_id: u32::MAX - (index * 2 + routed) as u32,
                channel_id: route,
                request_id: 0x80000000 + (index * 2 + routed) as u32,
            };
            let report = CommsTestReport {
                command_id: request.command_id,
                request_id: request.request_id,
            };
            *expected_report = report;
            let mut pending = if routed == 0 {
                views[index].submit_comms_test(request)
            } else {
                views[0].submit_comms_test(request)
            }
            .unwrap();
            let result = pending.wait(15000).unwrap();
            println!("future channel={route}, routed={routed}: {result:?}");
            assert_eq!(result, CommsTestResult::Completed { report });
            futures += 1;
            pending.close().unwrap();
        }
    }
    let mut callbacks = 0;
    for report in expected[0] {
        let event = c2_metadata.receive(15000).unwrap();
        println!("C2 actual metadata: {event:?}");
        assert_eq!(event, C2MetadataEvent::CommsTest(report));
        callbacks += 1;
    }
    for (index, metadata) in common.iter_mut().enumerate() {
        for report in expected[index + 1] {
            let actual = metadata.receive(15000).unwrap();
            println!("common channel={} actual metadata: {actual:?}", index + 1);
            assert_eq!(actual, report);
            callbacks += 1;
        }
        let counters = metadata.counters().unwrap();
        println!("common counters: {counters:?}");
        assert_eq!(
            counters,
            CommsMetadataCounters {
                events_received: 2,
                events_dropped_queue_full: 0,
                malformed_or_unsupported: 0
            }
        );
        assert_eq!(metadata.receive(0).unwrap_err().kind(), &ErrorKind::Timeout);
    }
    let counters = c2_metadata.counters().unwrap();
    println!("C2 counters: {counters:?}");
    assert_eq!(
        (
            counters.events_received,
            counters.events_dropped_queue_full,
            counters.malformed_or_unsupported
        ),
        (2, 0, 0)
    );
    assert_eq!((futures, callbacks), (8, 8));
    // Keep the distinction explicit: callbacks above are dequeued, never synthesized from futures.
    for view in &mut views {
        view.close().unwrap();
    }
    c2.close().unwrap();
    health.close().unwrap();
    image.close().unwrap();
    stacked.close().unwrap();
    c2_metadata.close().unwrap();
    for metadata in &mut common {
        metadata.close().unwrap();
    }
    session.close().unwrap();
    println!("PASS: 8 correlated futures, independently 8 provider metadata callbacks; no acquisition/hardware qualification");
}
