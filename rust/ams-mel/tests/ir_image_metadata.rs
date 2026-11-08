//! Each case runs in a child: mock failpoints, barriers and DSO logs are global.
//! Production-compatible cases use only the six published metadata exports.
use std::{
    env, fs,
    path::{Path, PathBuf},
    process::Command,
    time::{Duration, Instant},
};

use ams_mel::{
    AzimuthElevation, BadPixel, BadPixelList, BadPixelReason, ComponentLocation, ErrorKind, Euler,
    ImageConfig, ImageMetadataCounters, ImageMetadataEvent, LineOfSightEuler, LineOfSightReport,
    NavigationResponse, Session, UciId,
};

const CHILD: &str = "AMS_MEL_RUST_IMAGE_METADATA_CHILD";
const BARRIER: &str = "AMS_MEL_TEST_IMAGE_METADATA_CALLBACK_BARRIER";

fn provider() -> PathBuf {
    env::var_os("AMS_MEL_TEST_PROVIDER_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| {
            Path::new(env!("CARGO_MANIFEST_DIR")).join("../../native/build-tests/test-providers")
        })
        .join("libmock_ir_provider.so")
}

fn config() -> ImageConfig {
    let id = || UciId::new([0; 16], "image").unwrap();
    ImageConfig::with_limits(
        id(),
        id(),
        ComponentLocation::new(0.0, 0.0, 0.0, "key", "system").unwrap(),
        3,
        64,
        2,
    )
    .unwrap()
}

fn bad(base: u32) -> ImageMetadataEvent {
    ImageMetadataEvent::BadPixelList(BadPixelList {
        reported_size: 0xa5a50001,
        reported_count: 0x5a5a0003,
        pixels: [
            (1 + base, 0x80000001),
            (0xf0000002, 17 + base),
            (123 + base, 456),
        ]
        .map(|(row, column)| BadPixel {
            row,
            column,
            reason: BadPixelReason::Unknown,
        })
        .to_vec(),
    })
}

fn report() -> ImageMetadataEvent {
    ImageMetadataEvent::LineOfSightReport(LineOfSightReport {
        system_time_ns: -123456789,
        pointing_angle: AzimuthElevation {
            azimuth_rad: 1.25,
            elevation_rad: -2.5,
        },
        pointing_angle_rates: AzimuthElevation {
            azimuth_rad: 0.125,
            elevation_rad: -0.25,
        },
        at_speed: true,
        in_tolerance: false,
        platform_attitude: Euler {
            roll: 0.5,
            pitch: -0.75,
            yaw: 1.0,
        },
        validity_flag_bitfield: 0xa5a50003,
        image_rotation_rad: -1.5,
    })
}

fn euler() -> ImageMetadataEvent {
    ImageMetadataEvent::LineOfSightEuler(LineOfSightEuler {
        system_time_ns: 987654321,
        attitude: Euler {
            roll: -0.5,
            pitch: 1.25,
            yaw: -2.0,
        },
        attitude_rates: Euler {
            roll: 0.25,
            pitch: -0.125,
            yaw: 0.0625,
        },
    })
}

fn navigation() -> ImageMetadataEvent {
    ImageMetadataEvent::NavigationResponse(NavigationResponse {
        system_time_ns: -8765432109,
        command_id: 0xf1234567,
        request_id: 0x89abcdef,
    })
}

struct Scratch(PathBuf);
impl Scratch {
    fn new() -> Self {
        let dir = env::temp_dir().join(format!("ams-image-metadata-{}", std::process::id()));
        fs::create_dir(&dir).unwrap();
        env::set_var("AMS_MEL_TEST_LIFETIME_LOG", dir.join("lifetime"));
        Self(dir)
    }
    fn events(&self) -> String {
        fs::read_to_string(self.0.join("lifetime")).unwrap_or_default()
    }
    fn record(&self, line: &str) {
        use std::io::Write;
        writeln!(
            fs::OpenOptions::new()
                .append(true)
                .create(true)
                .open(self.0.join("lifetime"))
                .unwrap(),
            "{line}"
        )
        .unwrap();
    }
    fn unloaded(&self) {
        assert!(
            self.events().ends_with("library_unloaded\n"),
            "{}",
            self.events()
        );
    }
}
impl Drop for Scratch {
    fn drop(&mut self) {
        env::remove_var("AMS_MEL_TEST_LIFETIME_LOG");
        env::remove_var("AMS_MEL_TEST_IMAGE_CALLBACK_FAILURE");
        env::remove_var(BARRIER);
        let _ = fs::remove_dir_all(&self.0);
    }
}

fn order(events: &str, expected: &[&str]) {
    let positions: Vec<_> = expected
        .iter()
        .map(|line| {
            events
                .lines()
                .position(|v| v == *line)
                .unwrap_or_else(|| panic!("missing {line}: {events}"))
        })
        .collect();
    assert!(positions.windows(2).all(|v| v[0] < v[1]), "{events}");
}

fn run(case: &str) {
    let output = Command::new(env::current_exe().unwrap())
        .args([
            "--exact",
            "child_entry",
            "--ignored",
            "--nocapture",
            "--test-threads=1",
        ])
        .env(CHILD, case)
        .env_remove("AMS_MEL_TEST_LIFETIME_LOG")
        .env_remove("AMS_MEL_TEST_IMAGE_CALLBACK_FAILURE")
        .env_remove(BARRIER)
        .output()
        .unwrap();
    assert!(
        output.status.success(),
        "{case}:\n{}\n{}",
        String::from_utf8_lossy(&output.stdout),
        String::from_utf8_lossy(&output.stderr)
    );
    assert!(String::from_utf8_lossy(&output.stdout).contains("IMAGE_METADATA_CHILD_COMPLETE"));
}

#[test]
fn rich_payloads_fifo_and_counters() {
    for case in [
        "sync",
        "los-rich",
        "four-mixed",
        "overflow",
        "los-mixed",
        "malformed",
        "los-report-null",
        "los-euler-null",
        "navigation-null",
    ] {
        run(case);
    }
}

/// Explicit native test failpoints; omit only this test for production linkage.
#[test]
fn native_test_allocation_failpoints() {
    for case in [
        "allocation",
        "los-report-allocation",
        "navigation-allocation",
    ] {
        run(case);
    }
}

#[test]
fn registration_failures_consume_one_shot() {
    for case in [
        "register-fail",
        "register-not-supported",
        "register-throw",
        "report-register-fail",
        "report-register-throw",
        "euler-register-fail",
        "euler-register-throw",
        "navigation-register-fail",
        "navigation-register-throw",
    ] {
        run(case);
    }
}

#[test]
fn ownership_stop_drop_and_frame_independence() {
    for case in [
        "metadata-first",
        "image-first",
        "session-first",
        "stop-drain",
        "drop",
        "running",
        "nuc-independent",
        "nuc-first",
    ] {
        run(case);
    }
}

/// Mock barrier is provider-side, not a private facade symbol or test failpoint.
#[test]
fn callback_quiescence_close_and_drop() {
    run("barrier-close");
    run("barrier-drop");
}

#[test]
#[ignore]
fn child_entry() {
    let case = env::var(CHILD).expect("child case");
    let scratch = Scratch::new();
    if case.starts_with("barrier-") {
        env::set_var(BARRIER, scratch.0.join("callback"));
        let session = Session::open(provider(), "image-metadata-nonquiescing", "").unwrap();
        let mut image = session.open_image_stream(&config()).unwrap();
        let mut metadata = image.open_metadata(2).unwrap();
        fs::write(scratch.0.join("callback.start"), "start").unwrap();
        let deadline = Instant::now() + Duration::from_secs(10);
        while !scratch.0.join("callback.entered").exists() {
            assert!(Instant::now() < deadline, "callback failed to enter");
            std::thread::yield_now();
        }
        assert!(!scratch
            .events()
            .contains("image_metadata_callback_returned"));
        if case == "barrier-close" {
            metadata.close().unwrap();
        }
        drop(metadata);
        scratch.record("public_metadata_closed");
        assert!(!scratch.events().contains("library_unloaded"));
        image.close().unwrap(); // Destructor releases barrier and joins callback.
        session.close().unwrap();
        order(
            &scratch.events(),
            &[
                "image_metadata_callback_entered",
                "public_metadata_closed",
                "image_metadata_callback_returned",
                "channel_destroyed",
                "library_unloaded",
            ],
        );
    } else if case.contains("register-") {
        let scenario = format!("image-metadata-{case}");
        let session = Session::open(provider(), &scenario, "").unwrap();
        let mut image = session.open_image_stream(&config()).unwrap();
        let expected = if case.ends_with("throw") {
            ErrorKind::ProviderException
        } else {
            ErrorKind::ProviderFailed
        };
        assert_eq!(image.open_metadata(4).unwrap_err().kind(), &expected);
        scratch.record("open_failed");
        assert_eq!(
            image.open_metadata(4).unwrap_err().kind(),
            &ErrorKind::InvalidArgument
        );
        image.close().unwrap();
        session.close().unwrap();
        if case.starts_with("navigation-") {
            order(
                &scratch.events(),
                &[
                    "navigation_registration_failed",
                    "open_failed",
                    "retained_callback_invoked",
                    "retained_callback_returned",
                    "channel_destroyed",
                    "library_unloaded",
                ],
            );
        }
    } else if [
        "metadata-first",
        "image-first",
        "session-first",
        "stop-drain",
        "drop",
        "running",
        "nuc-independent",
        "nuc-first",
    ]
    .contains(&case.as_str())
    {
        let scenario = if case == "nuc-independent" || case == "nuc-first" {
            "nuc-rich"
        } else {
            "image-metadata-four-mixed"
        };
        let mut session = Some(Session::open(provider(), scenario, "").unwrap());
        let mut image = session
            .as_ref()
            .unwrap()
            .open_image_stream(&config())
            .unwrap();
        let mut early_nuc = if case == "nuc-first" {
            Some(image.open_nuc(4).unwrap())
        } else {
            None
        };
        if case == "running" {
            image.start().unwrap();
        }
        assert_eq!(
            image.open_metadata(0).unwrap_err().kind(),
            &ErrorKind::InvalidArgument
        );
        let mut metadata = image.open_metadata(4).unwrap();
        assert_eq!(
            image.open_metadata(4).unwrap_err().kind(),
            &ErrorKind::InvalidArgument
        );
        if case == "nuc-independent" || case == "nuc-first" {
            let mut nuc = early_nuc
                .take()
                .unwrap_or_else(|| image.open_nuc(4).unwrap());
            assert!(nuc.receive(0).unwrap().points.is_empty());
            assert_eq!(nuc.receive(0).unwrap().points.len(), 1);
            assert_eq!(nuc.receive(0).unwrap().points.len(), 3);
            assert_eq!(metadata.receive(0).unwrap_err().kind(), &ErrorKind::Timeout);
            nuc.close().unwrap();
            metadata.close().unwrap();
            image.close().unwrap();
        } else {
            if case != "running" {
                image.start().unwrap();
            }
            assert_eq!(
                image.receive(1000).unwrap().pixels,
                (16_u8..28).collect::<Vec<_>>()
            );
            if case == "session-first" {
                session.take().unwrap().close().unwrap();
            }
            if case == "stop-drain" {
                image.stop().unwrap();
                let owned: Vec<_> = (0..4).map(|_| metadata.receive(0).unwrap()).collect();
                assert_eq!(
                    metadata.receive(0).unwrap_err().kind(),
                    &ErrorKind::StreamStopped
                );
                metadata.close().unwrap();
                image.close().unwrap();
                session.take().unwrap().close().unwrap();
                scratch.unloaded();
                assert_eq!(owned, vec![bad(0), report(), euler(), navigation()]);
                println!("IMAGE_METADATA_CHILD_COMPLETE");
                return;
            }
            if case == "image-first" {
                image.close().unwrap();
            } else {
                if case == "metadata-first" {
                    metadata.close().unwrap();
                    assert_eq!(
                        image.open_metadata(4).unwrap_err().kind(),
                        &ErrorKind::InvalidArgument
                    );
                    // Closing metadata does not close or stop ordinary frames.
                    assert_eq!(
                        image.receive(1000).unwrap().pixels,
                        (32_u8..44).collect::<Vec<_>>()
                    );
                }
                if case == "drop" {
                    drop(metadata);
                    image.close().unwrap();
                    session.take().unwrap().close().unwrap();
                    scratch.unloaded();
                    println!("IMAGE_METADATA_CHILD_COMPLETE");
                    return;
                }
                image.close().unwrap();
            }
            if case != "metadata-first" {
                let owned: Vec<_> = (0..4).map(|_| metadata.receive(0).unwrap()).collect();
                assert_eq!(
                    metadata.receive(0).unwrap_err().kind(),
                    &ErrorKind::StreamStopped
                );
                metadata.close().unwrap();
                if let Some(session) = session.take() {
                    session.close().unwrap();
                }
                scratch.unloaded();
                assert_eq!(owned, vec![bad(0), report(), euler(), navigation()]);
            }
            metadata.close().unwrap();
            assert!(!metadata.is_open());
            assert_eq!(
                metadata.receive(0).unwrap_err().kind(),
                &ErrorKind::ProviderFailed
            );
            assert_eq!(
                metadata.counters().unwrap_err().kind(),
                &ErrorKind::ProviderFailed
            );
        }
        if let Some(session) = session.take() {
            session.close().unwrap();
        }
    } else {
        let scenario = format!("image-metadata-{case}");
        let allocation = case.ends_with("allocation");
        if allocation {
            env::set_var("AMS_MEL_TEST_IMAGE_CALLBACK_FAILURE", "allocation");
        }
        let session = Session::open(provider(), &scenario, "").unwrap();
        let mut image = session.open_image_stream(&config()).unwrap();
        let capacity = if case == "four-mixed" || case == "los-rich" {
            4
        } else {
            2
        };
        let mut metadata = image.open_metadata(capacity).unwrap();
        env::remove_var("AMS_MEL_TEST_IMAGE_CALLBACK_FAILURE");
        let (received, dropped, malformed, expected) = match case.as_str() {
            "sync" => (1, 0, 0, vec![bad(0)]),
            "los-rich" => (2, 0, 0, vec![report(), euler()]),
            "four-mixed" => (6, 2, 0, vec![bad(0), report(), euler(), navigation()]),
            "overflow" => (5, 3, 0, vec![bad(0), bad(1)]),
            "los-mixed" => (5, 3, 0, vec![bad(0), report()]),
            "malformed" => (2, 0, 1, vec![bad(0)]),
            "allocation" => (2, 0, 1, vec![bad(9)]),
            "los-report-null" | "los-report-allocation" => (2, 0, 1, vec![report()]),
            "los-euler-null" => (2, 0, 1, vec![euler()]),
            "navigation-null" | "navigation-allocation" => (2, 0, 1, vec![navigation()]),
            _ => panic!("unknown case {case}"),
        };
        let counters = ImageMetadataCounters {
            events_received: received,
            events_dropped_queue_full: dropped,
            malformed_or_unsupported: malformed,
        };
        assert_eq!(metadata.counters().unwrap(), counters);
        let actual: Vec<_> = (0..expected.len())
            .map(|_| metadata.receive(0).unwrap())
            .collect();
        assert_eq!(actual, expected);
        assert_eq!(metadata.counters().unwrap(), counters); // Receive never decrements.
        assert_eq!(metadata.receive(0).unwrap_err().kind(), &ErrorKind::Timeout);
        metadata.close().unwrap();
        image.close().unwrap();
        session.close().unwrap();
        scratch.unloaded();
        assert_eq!(actual, expected); // Independent owned copies after actual unload.
        order(
            &scratch.events(),
            &[
                "bad_pixel_registration_attempted",
                "line_of_sight_report_registration_attempted",
                "line_of_sight_euler_registration_attempted",
                "navigation_response_registration_attempted",
            ],
        );
    }
    scratch.unloaded();
    println!("IMAGE_METADATA_CHILD_COMPLETE");
}
