//! Compiled-provider tests run in isolated children for DSO unload and deliberate
//! retention scenarios. No process-global fixture state leaks across tests.
use ams_mel_ir::{BufferFlag, ErrorKind, ImageLocation, Session};
use std::{env, fs, path::PathBuf, process::Command};

fn provider() -> PathBuf {
    env::var_os("AMS_MEL_TEST_PROVIDER_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| {
            PathBuf::from(env!("CARGO_MANIFEST_DIR"))
                .join("../../native/build-tests/test-providers")
        })
        .join("libmock_ir_provider.so")
}

fn session(scenario: &str) -> Session {
    Session::open(provider(), scenario, "").unwrap()
}

#[test]
fn initialization_properties_and_signed_contexts() {
    for context in [i64::MIN, -1, 0, 23, i64::MAX] {
        for size in [1, 32, 4096] {
            let parent = session("foundation-normal");
            let mut buffer = parent.open_host_buffer(size, context).unwrap();
            let info = buffer.info().unwrap();
            assert_eq!(info.storage_size, size);
            assert_eq!(info.initialized_context, context);
            assert_eq!(info.provider_size, size as i64);
            assert_eq!(info.provider_context, context);
            assert!(info.buffer_address_matches);
            assert_eq!(info.image_location, ImageLocation::InsideStorage);
            assert_eq!(
                buffer.read_storage(0, size as usize).unwrap(),
                vec![0; size as usize]
            );
            buffer.close().unwrap();
            buffer.close().unwrap();
            assert!(!buffer.is_open());
            assert!(buffer.info().is_err());
            assert!(buffer.read_storage(0, 0).is_err());
            assert!(buffer.write_storage(0, &[]).is_err());
            assert!(buffer.flags().is_err());
            assert!(buffer.set_flags(&[]).is_err());
            assert!(buffer.add_flag(BufferFlag::Host).is_err());
            parent.close().unwrap();
        }
    }
}

#[test]
fn allocation_boundaries_and_native_failures_preserve_returns() {
    let parent = session("foundation-normal");
    for size in [0, i64::MAX as u64 + 1, u64::MAX] {
        let error = parent.open_host_buffer(size, -7).unwrap_err();
        assert_eq!(error.error.kind(), &ErrorKind::InvalidArgument);
        assert_eq!(error.provider_return, None);
    }
    parent.close().unwrap();
    for (scenario, kind, result) in [
        ("foundation-factory-null", ErrorKind::FactoryFailed, None),
        (
            "foundation-factory-throw",
            ErrorKind::ProviderException,
            None,
        ),
        (
            "foundation-factory-allocation",
            ErrorKind::InternalError,
            None,
        ),
        (
            "foundation-init-refuse",
            ErrorKind::InitializationFailed,
            Some(3),
        ),
    ] {
        let parent = session(scenario);
        let error = parent.open_host_buffer(8, -7).unwrap_err();
        assert_eq!(error.error.kind(), &kind);
        assert!(error.native_status.is_some());
        assert!(error.error.diagnostic().is_some());
        assert_eq!(error.provider_return, result);
        parent.close().unwrap();
    }
}

#[test]
fn discrepant_getters_and_image_locations_are_not_normalized() {
    for (scenario, location, matches) in [
        ("foundation-properties", ImageLocation::InsideStorage, true),
        ("foundation-image-null", ImageLocation::Null, true),
        (
            "foundation-image-outside",
            ImageLocation::OutsideStorage,
            true,
        ),
        (
            "foundation-address-mismatch",
            ImageLocation::InsideStorage,
            false,
        ),
    ] {
        let parent = session(scenario);
        let mut buffer = parent.open_host_buffer(32, i64::MAX).unwrap();
        let info = buffer.info().unwrap();
        assert_eq!(info.image_location, location);
        assert_eq!(info.buffer_address_matches, matches);
        assert_eq!(info.storage_size, 32);
        assert_eq!(info.initialized_context, i64::MAX);
        if scenario == "foundation-properties" {
            assert_eq!(info.provider_size, -42);
            assert_eq!(info.provider_context, i64::MIN);
        }
        buffer.close().unwrap();
        parent.close().unwrap();
    }
}

#[test]
fn storage_copy_bounds_binary_and_empty_end_spans() {
    let parent = session("foundation-normal");
    let mut buffer = parent.open_host_buffer(256, -7).unwrap();
    let bytes: Vec<_> = (0..=255).collect();
    buffer.write_storage(0, &bytes).unwrap();
    let snapshot = buffer.read_storage(0, 256).unwrap();
    assert_eq!(snapshot, bytes);
    buffer.write_storage(0, &[255, 0, 128]).unwrap();
    assert_eq!(&snapshot[..3], &[0, 1, 2]);
    assert_eq!(buffer.read_storage(0, 3).unwrap(), [255, 0, 128]);
    assert!(buffer.read_storage(256, 0).unwrap().is_empty());
    buffer.write_storage(256, &[]).unwrap();
    for (offset, length) in [(257, 0), (256, 1), (255, 2), (u64::MAX, 1), (1, usize::MAX)] {
        assert_eq!(
            buffer.read_storage(offset, length).unwrap_err().kind(),
            &ErrorKind::InvalidArgument
        );
    }
    for (offset, bytes) in [
        (257, &[][..]),
        (256, &[1][..]),
        (255, &[1, 2][..]),
        (u64::MAX, &[1][..]),
    ] {
        assert_eq!(
            buffer.write_storage(offset, bytes).unwrap_err().kind(),
            &ErrorKind::InvalidArgument
        );
    }
    assert_eq!(buffer.read_storage(3, 253).unwrap(), snapshot[3..]);
    buffer.close().unwrap();
    parent.close().unwrap();
}

#[test]
fn ordered_duplicate_empty_and_all_flag_alternatives() {
    let parent = session("foundation-normal");
    let mut buffer = parent.open_host_buffer(8, 1).unwrap();
    assert!(buffer.flags().unwrap().is_empty());
    let flags = [
        BufferFlag::Rdma,
        BufferFlag::Host,
        BufferFlag::Gpu,
        BufferFlag::Host,
    ];
    buffer.set_flags(&flags).unwrap();
    assert_eq!(buffer.flags().unwrap(), flags);
    buffer.add_flag(BufferFlag::Gpu).unwrap();
    assert_eq!(
        buffer.flags().unwrap(),
        [
            BufferFlag::Rdma,
            BufferFlag::Host,
            BufferFlag::Gpu,
            BufferFlag::Host,
            BufferFlag::Gpu
        ]
    );
    buffer.set_flags(&[]).unwrap();
    assert!(buffer.flags().unwrap().is_empty());
    buffer.close().unwrap();
    parent.close().unwrap();
}

#[test]
fn flag_refusal_unknown_and_exceptions() {
    for (scenario, kind, ret) in [
        ("foundation-get-refuse", ErrorKind::ProviderFailed, Some(4)),
        ("foundation-get-throw", ErrorKind::ProviderException, None),
        (
            "foundation-unknown-flag",
            ErrorKind::ProviderFailed,
            Some(0),
        ),
    ] {
        let parent = session(scenario);
        let mut buffer = parent.open_host_buffer(8, 1).unwrap();
        let error = buffer.flags().unwrap_err();
        assert_eq!(error.error.kind(), &kind);
        assert_eq!(error.provider_return, ret);
        assert!(error.native_status.is_some());
        buffer.close().unwrap();
        parent.close().unwrap();
    }
    for scenario in [
        "foundation-add-throw",
        "foundation-set-throw",
        "foundation-info-throw",
    ] {
        let parent = session(scenario);
        let mut buffer = parent.open_host_buffer(8, 1).unwrap();
        let error = match scenario {
            "foundation-add-throw" => buffer.add_flag(BufferFlag::Host).unwrap_err(),
            "foundation-set-throw" => buffer.set_flags(&[]).unwrap_err(),
            _ => buffer.info().unwrap_err(),
        };
        assert_eq!(error.kind(), &ErrorKind::ProviderException);
        buffer.close().unwrap();
        parent.close().unwrap();
    }
}

#[test]
fn native_lifecycle_isolated_processes() {
    for scenario in ["explicit", "drop", "session-drop", "retained", "init-throw"] {
        let log = env::temp_dir().join(format!("rust019-{}-{scenario}.log", std::process::id()));
        let _ = fs::remove_file(&log);
        let output = Command::new(env::current_exe().unwrap())
            .args(["--exact", "native_lifecycle_child", "--nocapture"])
            .env("RUST019_CHILD", scenario)
            .env("AMS_MEL_TEST_LIFETIME_LOG", &log)
            .output()
            .unwrap();
        assert!(
            output.status.success(),
            "{scenario}: {} {}",
            String::from_utf8_lossy(&output.stdout),
            String::from_utf8_lossy(&output.stderr)
        );
        let events = fs::read_to_string(&log).unwrap();
        assert!(events.contains("foundation_instance_manager_verified"));
        for forbidden in [
            "channel_attached",
            "buffer_registered",
            "buffer_released",
            "image_delivered",
        ] {
            assert!(!events.contains(forbidden), "{events}");
        }
        if scenario == "retained" || scenario == "init-throw" {
            for absent in [
                "foundation_buffer_destroyed_with_storage",
                "control_destroyed",
                "manager_destroyed",
            ] {
                assert!(!events.contains(absent), "{events}");
            }
        } else {
            let mut previous = 0;
            for event in [
                "foundation_buffer_destroyed_with_storage",
                "control_destroyed",
                "manager_destroyed",
                "library_unloaded",
            ] {
                let index = events
                    .find(event)
                    .unwrap_or_else(|| panic!("missing {event}: {events}"));
                assert!(index > previous, "{events}");
                previous = index;
                assert_eq!(events.matches(event).count(), 1);
            }
        }
        fs::remove_file(log).unwrap();
    }
}

#[test]
fn native_lifecycle_child() {
    let Ok(scenario) = env::var("RUST019_CHILD") else {
        return;
    };
    let parent = session(match scenario.as_str() {
        "retained" => "foundation-retained",
        "init-throw" => "foundation-init-throw",
        _ => "foundation-normal",
    });
    if scenario == "init-throw" {
        let error = parent.open_host_buffer(16, i64::MIN).unwrap_err();
        assert_eq!(error.error.kind(), &ErrorKind::ProviderException);
        assert_eq!(error.provider_return, None);
        parent.close().unwrap();
        return;
    }
    let mut buffer = parent.open_host_buffer(16, i64::MIN).unwrap();
    if scenario == "session-drop" {
        drop(parent);
    } else {
        parent.close().unwrap();
    }
    let log = || fs::read_to_string(env::var_os("AMS_MEL_TEST_LIFETIME_LOG").unwrap()).unwrap();
    assert!(!log().contains("control_destroyed"));
    buffer.write_storage(15, &[237]).unwrap();
    assert_eq!(buffer.read_storage(15, 1).unwrap(), [237]);
    assert_eq!(buffer.info().unwrap().initialized_context, i64::MIN);
    if scenario == "retained" {
        for _ in 0..2 {
            assert_eq!(
                buffer.close().unwrap_err().kind(),
                &ErrorKind::ProviderFailed
            );
            assert!(buffer.is_open());
        }
        drop(buffer); // best-effort refuses; complete graph must remain alive
        assert!(!log().contains("foundation_buffer_destroyed_with_storage"));
    } else if scenario == "drop" {
        drop(buffer);
    } else {
        buffer.close().unwrap();
        buffer.close().unwrap();
    }
}
