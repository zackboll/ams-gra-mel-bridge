//! Public discovery against the unchanged separate native provider. Each case
//! runs in a fresh process so actual unload and lifecycle logs are deterministic.
use ams_mel::{
    BandInfo, BandType, ChannelCapability, ChannelType, ComponentLocation, ControlConfig,
    CoordinateSystem, ErrorKind, HealthConfig, ImageBand, ImageControlConfig, MetadataCapability,
    PixelFormat, SensorType, Session, UciId,
};
use std::{
    env, fs,
    path::{Path, PathBuf},
    process::Command,
};

const CHILD: &str = "AMS_MEL_RUST_DISCOVERY_CHILD";
const TYPES: [ChannelType; 4] = [
    ChannelType::CommandAndControl,
    ChannelType::HealthAndStatus,
    ChannelType::IrstImage,
    ChannelType::StackedImage,
];

fn provider() -> PathBuf {
    env::var_os("AMS_MEL_TEST_PROVIDER_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| {
            Path::new(env!("CARGO_MANIFEST_DIR")).join("../../native/build-tests/test-providers")
        })
        .join("libmock_ir_provider.so")
}

fn id(seed: u8, label: &str) -> UciId {
    UciId::new(
        std::array::from_fn(|i| seed.wrapping_add(7 * i as u8)),
        label,
    )
    .unwrap()
}

fn rich(index: usize) -> ChannelCapability {
    ChannelCapability {
        channel_id: id(0x10 + index as u8, &format!("discovery-{index}-α")),
        height: 1080,
        width: 1920,
        bit_depth: 12,
        row_pitch: 4096,
        buffer_size: 8388608,
        image_size: 4147200,
        number_of_bands: 3,
        pixel_format: PixelFormat::Rgb,
        sensor_types: vec![SensorType::GimbalHorizontal, SensorType::StepStare],
        platform_id: id(0x31, "platform-€"),
        sensor_location: ComponentLocation::new(1.25, -2.5, 3.75, "sensor-key", "system-β")
            .unwrap(),
        channel_types: vec![
            TYPES[index],
            ChannelType::Instrumentation,
            ChannelType::Reserved2,
        ],
        task_schedule_depth: 17,
        odc_available: true,
        nuc_available: true,
        metadata_capabilities: vec![
            MetadataCapability::BadPixelList,
            MetadataCapability::CommandStatus,
            MetadataCapability::ChannelCommsTestRep,
            MetadataCapability::Reserved10,
        ],
        image_bands: vec![
            ImageBand {
                band_index: 2,
                bands: vec![
                    BandInfo {
                        kind: BandType::IrLongwave,
                        min_wavelength_m: 8e-6,
                        max_wavelength_m: 12e-6,
                    },
                    BandInfo {
                        kind: BandType::IrMidwave,
                        min_wavelength_m: 3e-6,
                        max_wavelength_m: 5e-6,
                    },
                ],
            },
            ImageBand {
                band_index: 9,
                bands: vec![BandInfo {
                    kind: BandType::VisibleRed,
                    min_wavelength_m: 620e-9,
                    max_wavelength_m: 750e-9,
                }],
            },
        ],
        nav_frames: vec![CoordinateSystem::NedSensor, CoordinateSystem::Ecef],
    }
}

fn selected_configs(capabilities: &[ChannelCapability]) {
    let select = |kind| {
        capabilities
            .iter()
            .find(|cap| cap.channel_types.contains(&kind))
            .unwrap()
    };
    let c2 = select(ChannelType::CommandAndControl);
    let _control = ControlConfig::new(
        c2.channel_id.clone(),
        c2.platform_id.clone(),
        c2.sensor_location.clone(),
    );
    let health = select(ChannelType::HealthAndStatus);
    let _health = HealthConfig::new(
        health.channel_id.clone(),
        health.platform_id.clone(),
        health.sensor_location.clone(),
    );
    let image = select(ChannelType::IrstImage);
    let _image = ImageControlConfig::new(
        image.channel_id.clone(),
        image.platform_id.clone(),
        image.sensor_location.clone(),
    );
    // Construction only. Advertisement does not promise attachment support.
}

fn exercise(scenario: &str) {
    let dir = env::temp_dir().join(format!("ams-rust-discovery-{}", std::process::id()));
    fs::create_dir(&dir).unwrap();
    let log = dir.join("lifetime");
    env::set_var("AMS_MEL_TEST_LIFETIME_LOG", &log);
    let session = Session::open(provider(), scenario, "").unwrap();
    let failure = match scenario {
        "discovery-malformed" | "discovery-bad-label" => Some((
            ErrorKind::ProviderFailed,
            "provider returned malformed ChannelCapability",
        )),
        "discovery-throw" => Some((ErrorKind::ProviderException, "mock discovery exception")),
        "discovery-unknown-throw" => Some((
            ErrorKind::ProviderException,
            "unknown provider capability exception",
        )),
        "discovery-bad-alloc" => Some((
            ErrorKind::InternalError,
            "capability list allocation failed",
        )),
        _ => None,
    };
    if let Some((kind, text)) = failure {
        for _ in 0..2 {
            let error = session.capabilities().unwrap_err();
            assert_eq!(error.kind(), &kind);
            assert_eq!(error.diagnostic(), Some(text));
            assert_eq!(error.diagnostic_required(), Some(text.len() + 1));
        }
        session.close().unwrap();
    } else {
        let mut first = session.capabilities().unwrap();
        let second = session.capabilities().unwrap();
        assert_eq!(first, second);
        session.close().unwrap();
        // Check the unload event before accessing ANY returned nested value.
        assert!(fs::read_to_string(&log)
            .unwrap()
            .lines()
            .any(|e| e == "library_unloaded"));
        if scenario == "discovery-empty" {
            assert!(first.is_empty());
        } else {
            assert_eq!(first.len(), 4);
            for (i, cap) in first.iter().enumerate() {
                assert_eq!(
                    cap.channel_id,
                    id(0x10 + i as u8, &format!("discovery-{i}-α"))
                );
                assert_eq!(cap.channel_types[0], TYPES[i]);
                if scenario == "discovery-rich" {
                    assert_eq!(*cap, rich(i)); // all scalar and nested fields
                } else {
                    assert_eq!(cap.channel_types, [TYPES[i]]);
                    assert_eq!(
                        (cap.width, cap.height, cap.bit_depth, cap.row_pitch),
                        (0, 0, 0, 0)
                    );
                    assert_eq!(
                        (cap.buffer_size, cap.image_size, cap.number_of_bands),
                        (0, 0, 0)
                    );
                    assert_eq!(cap.pixel_format, PixelFormat::Mono); // copied enum, not inferred layout
                }
            }
            selected_configs(&first);
            assert_ne!(first.as_ptr(), second.as_ptr());
            assert_ne!(
                first[0].channel_id.descriptive_label().as_ptr(),
                second[0].channel_id.descriptive_label().as_ptr()
            );
            first[0].channel_types.clear();
            assert!(!second[0].channel_types.is_empty());
            if scenario == "discovery-rich" {
                assert_ne!(
                    first[0].image_bands[0].bands.as_ptr(),
                    second[0].image_bands[0].bands.as_ptr()
                );
                first[0].image_bands[0].bands.clear();
                assert_eq!(second[0], rich(0));
            }
        }
    }
    let events = fs::read_to_string(&log).unwrap();
    assert_eq!(
        events.lines().filter(|e| *e == "library_unloaded").count(),
        1,
        "{events}"
    );
    for forbidden in [
        "channel_attached",
        "channel_enabled",
        "c2_enabled",
        "health_enabled",
        "stacked_enabled",
        "channel_detached",
        "channel_destroyed",
        "buffer_factory_called",
        "buffer_initialized",
        "buffer_registered",
        "buffer_unregistered",
    ] {
        assert!(
            !events.lines().any(|e| e == forbidden),
            "{forbidden}: {events}"
        );
    }
    env::remove_var("AMS_MEL_TEST_LIFETIME_LOG");
    fs::remove_dir_all(dir).unwrap();
}

#[test]
fn discovery_child() {
    if let Ok(scenario) = env::var(CHILD) {
        exercise(&scenario);
    }
}

macro_rules! scenario {
    ($name:ident, $scenario:literal) => {
        #[test]
        fn $name() {
            let output = Command::new(env::current_exe().unwrap())
                .args(["--exact", "discovery_child", "--nocapture"])
                .env(CHILD, $scenario)
                .output()
                .unwrap();
            assert!(
                output.status.success(),
                "{}\n{}",
                String::from_utf8_lossy(&output.stdout),
                String::from_utf8_lossy(&output.stderr)
            );
        }
    };
}
scenario!(discovery_empty, "discovery-empty");
scenario!(discovery_zero, "discovery-zero");
scenario!(discovery_rich, "discovery-rich");
scenario!(discovery_malformed, "discovery-malformed");
scenario!(discovery_bad_label, "discovery-bad-label");
scenario!(discovery_throw, "discovery-throw");
scenario!(discovery_unknown_throw, "discovery-unknown-throw");
scenario!(discovery_bad_alloc, "discovery-bad-alloc");
