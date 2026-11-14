use super::*;

fn capability(kind: ChannelType, seed: u8) -> ChannelCapability {
    ChannelCapability {
        channel_id: UciId::new([seed; 16], format!("channel-{seed}")).unwrap(),
        platform_id: UciId::new([99; 16], "platform").unwrap(),
        sensor_location: ComponentLocation::new(0.0, 0.0, 0.0, "key", "system").unwrap(),
        height: 0,
        width: 0,
        bit_depth: 0,
        row_pitch: 0,
        buffer_size: 0,
        image_size: 0,
        number_of_bands: 0,
        pixel_format: PixelFormat::Mono,
        sensor_types: vec![],
        channel_types: vec![kind],
        task_schedule_depth: 0,
        odc_available: false,
        nuc_available: false,
        metadata_capabilities: vec![],
        image_bands: vec![],
        nav_frames: vec![],
    }
}

fn caps() -> Vec<ChannelCapability> {
    REQUIRED
        .into_iter()
        .enumerate()
        .map(|(i, k)| capability(k, i as u8))
        .collect()
}

#[test]
fn argument_policy() {
    // Keep the real entry point type-checked in the policy-only inclusion too;
    // subprocess tests below exercise its exit statuses without consuming the
    // test runner's own command line here.
    let _: fn() -> ExitCode = main;
    assert_eq!(arguments(["--help".into()]), Ok(Arguments::Help));
    assert_eq!(
        arguments(["provider.so".into()]),
        Ok(Arguments::Provider("provider.so".into()))
    );
    assert!(arguments([]).is_err());
    assert!(arguments(["--help".into(), "extra".into()]).is_err());
    assert!(arguments(["provider.so".into(), "extra".into()]).is_err());
}

#[test]
fn strict_provider_identity() {
    let good = ProviderVersion {
        api_version: 4,
        library_version: 1,
        vendor: "minimal-mel-example".into(),
        description: "stub".into(),
    };
    assert!(profile(&good).is_ok());
    for bad in [
        ProviderVersion {
            vendor: "mock".into(),
            ..good.clone()
        },
        ProviderVersion {
            api_version: 5,
            ..good.clone()
        },
        ProviderVersion {
            library_version: 2,
            ..good
        },
    ] {
        assert!(profile(&bad).unwrap_err().contains("identity mismatch"));
    }
}

#[test]
fn discovery_is_order_preserving_and_drives_identity() {
    let mut caps = caps();
    caps.reverse();
    assert!(discovery(&caps).is_ok());
    let selected = select(&caps, ChannelType::CommandAndControl).unwrap();
    assert_eq!(selected.channel_id.uuid(), &[0; 16]);
    assert_eq!(caps[0].channel_types, [ChannelType::StackedImage]);
    assert!(attached(selected, selected).is_ok());
    let mut wrong = selected.clone();
    wrong.platform_id = UciId::new([55; 16], "wrong").unwrap();
    assert!(attached(&wrong, selected).is_err());
    wrong = selected.clone();
    wrong.sensor_location = ComponentLocation::new(1.0, 0.0, 0.0, "key", "system").unwrap();
    assert!(attached(&wrong, selected).is_err());
}

#[test]
fn missing_ambiguous_and_reused_identity_rejected() {
    let mut missing = caps();
    missing.remove(0);
    assert!(discovery(&missing).unwrap_err().contains("missing"));
    let mut ambiguous = caps();
    ambiguous.push(ambiguous[0].clone());
    assert!(discovery(&ambiguous).unwrap_err().contains("ambiguous"));
    let mut duplicate = caps();
    duplicate[1].channel_id = duplicate[0].channel_id.clone();
    assert!(discovery(&duplicate).unwrap_err().contains("reuse"));
}

#[test]
fn unconfigured_layout_is_not_acquisition() {
    let cap = capability(ChannelType::IrstImage, 7);
    assert!(zero_layout(&cap).is_ok());
    for field in 0..7 {
        let mut bad = cap.clone();
        match field {
            0 => bad.width = 1,
            1 => bad.height = 1,
            2 => bad.bit_depth = 8,
            3 => bad.row_pitch = 1,
            4 => bad.buffer_size = 1,
            5 => bad.image_size = 1,
            _ => bad.number_of_bands = 1,
        }
        assert!(zero_layout(&bad).is_err());
    }
    assert_eq!(stub_address(ChannelType::IrstImage), Ok(1));
    assert_eq!(stub_address(ChannelType::CommandAndControl), Ok(2));
    assert_eq!(stub_address(ChannelType::HealthAndStatus), Ok(4));
    assert!(stub_address(ChannelType::StackedImage).is_err());
    let report = neutral_navigation();
    assert_eq!(report.state, PositionSolutionState::NotSet);
    assert_eq!(report.system_time_ns, 0);
    assert_eq!(report.latitude_rad, 0.0);
}

#[test]
fn bit_and_config_expect_completed_not_implemented_not_hardware_success() {
    for operation in ["BIT_Command", "ConfigSetCommand"] {
        for value in [
            CommandReturn::Success,
            CommandReturn::Fail,
            CommandReturn::NotSupported,
            CommandReturn::BadPointer,
            CommandReturn::NotImplemented,
        ] {
            assert_eq!(
                returned(
                    &ReturnResult::Completed { value },
                    CommandReturn::NotImplemented,
                    operation
                )
                .is_ok(),
                value == CommandReturn::NotImplemented
            );
        }
        let error = returned(
            &ReturnResult::Rejected {
                code: MelErrorCode::InvalidState,
                description: "provider reason".into(),
            },
            CommandReturn::NotImplemented,
            operation,
        )
        .unwrap_err();
        assert!(error.contains("Rejected") && error.contains("provider reason"));
    }
}

#[test]
fn mode_and_navigation_preserve_explicit_rejections() {
    for (code, description, expected) in [
        (MelErrorCode::InvalidState, "provider reason", true),
        (MelErrorCode::InvalidState, "", false),
        (MelErrorCode::InvalidId, "bad id", false),
    ] {
        assert_eq!(
            mode_result(&ModeResult::Rejected {
                code,
                description: description.into()
            })
            .is_ok(),
            expected
        );
        assert_eq!(
            navigation_result(&NavigationResult::Rejected {
                code,
                description: description.into()
            })
            .is_ok(),
            expected
        );
    }
    assert!(mode_result(&ModeResult::Success {
        mode: MfaMode::Unused
    })
    .is_err());
    assert!(navigation_result(&NavigationResult::Success {
        response: NavigationResponse {
            system_time_ns: 0,
            command_id: 0,
            request_id: 0
        }
    })
    .is_err());
}

#[test]
fn correlation_requires_both_exact_identifiers() {
    let request = CommsTestRequest {
        command_id: u32::MAX,
        channel_id: 2,
        request_id: 0x8000_0001,
    };
    assert!(correlated(
        CommsTestReport {
            command_id: request.command_id,
            request_id: request.request_id
        },
        request
    )
    .is_ok());
    assert!(correlated(
        CommsTestReport {
            command_id: 0,
            request_id: request.request_id
        },
        request
    )
    .is_err());
    assert!(correlated(
        CommsTestReport {
            command_id: request.command_id,
            request_id: 1
        },
        request
    )
    .is_err());
}

#[test]
fn fabricated_metadata_is_not_accepted() {
    assert!(metadata_counts(2, 0, 0, 2).is_ok());
    assert!(metadata_counts(0, 0, 0, 0).is_ok());
    for counts in [(1, 0, 0), (2, 1, 0), (2, 0, 1), (3, 0, 0)] {
        assert!(metadata_counts(counts.0, counts.1, counts.2, 2).is_err());
    }
    assert!(no_telemetry(
        Ok(C2MetadataEvent::CommsTest(CommsTestReport {
            command_id: 0,
            request_id: 0
        })),
        "C2"
    )
    .is_err());
}

#[test]
fn unrelated_native_failures_are_not_stub_results() {
    let error = || Session::open("/definitely/absent/provider.so", "policy", "").unwrap_err();
    assert!(control_only(Err(error())).is_err());
    assert!(no_telemetry::<C2MetadataEvent>(Err(error()), "C2").is_err());
}
