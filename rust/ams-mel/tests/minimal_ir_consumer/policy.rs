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
    for index in 0..REQUIRED.len() {
        let mut missing = caps();
        let removed = missing.remove(index);
        assert!(select(&missing, removed.channel_types[0]).is_err());
        assert!(discovery(&missing).unwrap_err().contains("missing"));
        let mut ambiguous = caps();
        ambiguous.push(ambiguous[index].clone());
        assert!(select(&ambiguous, removed.channel_types[0]).is_err());
        assert!(discovery(&ambiguous).unwrap_err().contains("ambiguous"));
        let mut duplicate = caps();
        duplicate[index].channel_id = duplicate[(index + 1) % REQUIRED.len()].channel_id.clone();
        assert!(discovery(&duplicate).unwrap_err().contains("reuse"));
    }
}

#[test]
fn stacked_configuration_uses_discovered_identity_not_reference_constants() {
    let mut caps = caps();
    caps.reverse();
    let selected = select(&caps, ChannelType::StackedImage).unwrap();
    let mut selected = selected.clone();
    selected.channel_id = UciId::new([71; 16], "not the reference label").unwrap();
    selected.platform_id = UciId::new([72; 16], "another platform").unwrap();
    selected.sensor_location = ComponentLocation::new(1.5, -2.5, 3.5, "other", "location").unwrap();
    caps[0] = selected.clone();
    assert!(discovery(&caps).is_ok());
    let discovered = select(&caps, ChannelType::StackedImage).unwrap();
    assert_eq!(
        stacked_config(discovered),
        StackedConfig::new(
            selected.channel_id.clone(),
            selected.platform_id.clone(),
            selected.sensor_location.clone()
        )
    );
    assert!(attached(discovered, &selected).is_ok());
    for field in 0..4 {
        let mut wrong = selected.clone();
        match field {
            0 => wrong.channel_id = UciId::new([73; 16], "wrong channel").unwrap(),
            1 => wrong.platform_id = UciId::new([74; 16], "wrong platform").unwrap(),
            2 => wrong.sensor_location = ComponentLocation::new(0.0, 0.0, 0.0, "", "").unwrap(),
            _ => wrong.channel_types.push(ChannelType::IrstImage),
        }
        assert!(attached(&wrong, discovered).is_err());
    }
}

#[test]
fn unconfigured_layout_is_not_acquisition() {
    for kind in REQUIRED {
        let cap = capability(kind, 7);
        assert!(zero_layout(&cap).is_ok());
        assert!(attached(&cap, &cap).is_ok());
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
            assert!(attached(&bad, &cap).is_err());
            let mut profile = caps();
            *profile
                .iter_mut()
                .find(|c| c.channel_types == [kind])
                .unwrap() = bad;
            assert!(discovery(&profile).is_err());
        }
    }
    assert_eq!(stub_address(ChannelType::IrstImage), Ok(1));
    assert_eq!(stub_address(ChannelType::CommandAndControl), Ok(2));
    assert_eq!(stub_address(ChannelType::HealthAndStatus), Ok(4));
    assert_eq!(stub_address(ChannelType::StackedImage), Ok(6));
    for kind in [
        ChannelType::Unknown(6),
        ChannelType::Unknown(u32::MAX),
        ChannelType::IrstTrack,
    ] {
        assert!(stub_address(kind).is_err());
    }
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
    for counts in [(1, 0, 0), (2, 1, 0), (2, 0, 1), (3, 0, 0), (8, 0, 0)] {
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
fn four_destinations_require_eight_correlated_futures_not_eight_callbacks() {
    assert_eq!(REQUIRED.len(), 4);
    let mut operations = 0;
    let mut observable_c2_events = 0;
    for (index, kind) in REQUIRED.into_iter().enumerate() {
        let request = CommsTestRequest {
            command_id: 100 + index as u32,
            channel_id: stub_address(kind).unwrap(),
            request_id: 200 + index as u32,
        };
        for operation in ["direct CommsTest", "C2-routed CommsTest"] {
            let result = CommsTestResult::Completed {
                report: CommsTestReport {
                    command_id: request.command_id,
                    request_id: request.request_id,
                },
            };
            assert!(comms_result(&result, request, operation).is_ok());
            operations += 1;
            if kind == ChannelType::CommandAndControl {
                observable_c2_events += 1;
            }
        }
    }
    assert_eq!(operations, 8);
    assert_eq!(observable_c2_events, 2);
}

#[test]
fn unexpected_stacked_results_fail_the_application_checks() {
    let request = CommsTestRequest {
        command_id: 103,
        channel_id: stub_address(ChannelType::StackedImage).unwrap(),
        request_id: 203,
    };
    for result in [
        CommsTestResult::Rejected {
            code: MelErrorCode::InvalidId,
            description: "wrong route".into(),
        },
        CommsTestResult::Completed {
            report: CommsTestReport {
                command_id: 102,
                request_id: 203,
            },
        },
        CommsTestResult::Completed {
            report: CommsTestReport {
                command_id: 103,
                request_id: 202,
            },
        },
    ] {
        assert!(comms_result(&result, request, "StackedImage CommsTest").is_err());
    }
    for value in [
        CommandReturn::Fail,
        CommandReturn::NotImplemented,
        CommandReturn::NotSupported,
        CommandReturn::BadPointer,
    ] {
        assert!(returned(
            &ReturnResult::Completed { value },
            CommandReturn::Success,
            "StackedImage KeepAlive"
        )
        .is_err());
    }
    assert!(returned(
        &ReturnResult::Rejected {
            code: MelErrorCode::InvalidState,
            description: "disabled".into()
        },
        CommandReturn::Success,
        "StackedImage KeepAlive"
    )
    .is_err());
}

#[test]
fn application_forbids_unsafe_code() {
    assert!(
        include_str!("../../examples/minimal_ir_consumer.rs").contains("#![forbid(unsafe_code)]")
    );
}

#[test]
fn unrelated_native_failures_are_not_stub_results() {
    let error = || Session::open("/definitely/absent/provider.so", "policy", "").unwrap_err();
    assert!(control_only(Err(error())).is_err());
    assert!(no_telemetry::<C2MetadataEvent>(Err(error()), "C2").is_err());
}
