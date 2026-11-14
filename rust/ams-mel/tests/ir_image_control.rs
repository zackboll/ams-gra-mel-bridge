//! Existing native mock scenarios, isolated per process for DSO globals and
//! deliberately retained unsafe-to-release state. No acquisition storage FFI.
use ams_mel::{
    AttitudeRate, ChannelType, CommandReturn, CommsTestReport, CommsTestRequest, CommsTestResult,
    ComponentLocation, ErrorKind, Euler, ImageConfig, ImageControlConfig, ImageMetadataEvent,
    MelErrorCode, NavigationReport, NavigationResponse, NavigationResult, NorthEastDown,
    PixelFormat, PositionSolutionState, PositionVelocityCovariance, ReturnResult, Session, UciId,
};
use std::{
    env, fs,
    path::{Path, PathBuf},
    process::Command,
    thread,
    time::{Duration, Instant},
};

const CHILD: &str = "AMS_MEL_RUST_IMAGE_CONTROL_CHILD";

unsafe extern "C" {
    fn ams_mel_test_completion_boundary(
        family: u32,
        operation: u32,
        target: u64,
        counts: *mut u64,
    ) -> i32;
    fn ams_mel_test_completion_owner(family: u32, target: u64, observed: *mut u64) -> i32;
}

fn reclaimed() {
    for family in [1, 2, 3] {
        let mut counts = [0; 4];
        let mut observed = 0;
        // SAFETY: established test-only boundary/FinalOwner probe signatures.
        // Wait for actual worker reclamation, not merely future readiness.
        unsafe {
            assert_eq!(
                ams_mel_test_completion_boundary(family, 3, 0, counts.as_mut_ptr()),
                1
            );
            assert_eq!(
                ams_mel_test_completion_owner(family, counts[2], &mut observed),
                1
            );
        }
    }
}

fn provider() -> PathBuf {
    env::var_os("AMS_MEL_TEST_PROVIDER_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| {
            Path::new(env!("CARGO_MANIFEST_DIR")).join("../../native/build-tests/test-providers")
        })
        .join("libmock_ir_provider.so")
}

fn config() -> ImageControlConfig {
    ImageControlConfig::new(
        UciId::new(
            std::array::from_fn(|i| i as u8),
            String::from("IR image channel"),
        )
        .unwrap(),
        UciId::new(
            std::array::from_fn(|i| 0xf0 + i as u8),
            String::from("test platform"),
        )
        .unwrap(),
        ComponentLocation::new(
            1.25,
            -2.5,
            3.75,
            String::from("station-1"),
            String::from("mock-aircraft"),
        )
        .unwrap(),
    )
}

fn acquisition() -> ImageConfig {
    ImageConfig::with_limits(
        UciId::new(std::array::from_fn(|i| i as u8), "IR image channel").unwrap(),
        UciId::new(std::array::from_fn(|i| 0xf0 + i as u8), "test platform").unwrap(),
        ComponentLocation::new(1.25, -2.5, 3.75, "station-1", "mock-aircraft").unwrap(),
        2,
        64,
        2,
    )
    .unwrap()
}

fn report() -> NavigationReport {
    // A complete value, not evidence of an INS solution. No field is omitted.
    NavigationReport {
        system_time_ns: -123456789012,
        state: PositionSolutionState::NotSet,
        // Preserve the existing fixture, not a replacement mathematical angle.
        latitude_rad: f64::from_bits(0x3fe0_c150_97c8_0842),
        longitude_rad: -1.308997,
        altitude_m: 987.5,
        attitude: Euler {
            roll: 0.1,
            pitch: 0.2,
            yaw: 0.3,
        },
        attitude_rate: AttitudeRate {
            attitude_rate: Euler {
                roll: 0.11,
                pitch: -0.22,
                yaw: 0.33,
            },
            attitude_rate_time_ns: -424242,
        },
        speed: NorthEastDown {
            north: 10.0,
            east: -20.0,
            down: 30.0,
        },
        acceleration: NorthEastDown {
            north: -1.0,
            east: 2.0,
            down: -3.0,
        },
        wander_angle_rad: 0.05,
        magnetic_heading: 12.5,
        altitude_msl: 1000.25,
        position_velocity_covariance_uncertainty: PositionVelocityCovariance {
            position_position_pn_pn: 1.5,
            position_position_pn_pe: 2.5,
            position_position_pn_pd: 3.5,
            position_position_pe_pe: 4.5,
            position_position_pe_pd: 5.5,
            position_position_pd_pd: 6.5,
            position_velocity_pn_vn: 7.5,
            position_velocity_pn_ve: 8.5,
            position_velocity_pn_vd: 9.5,
            position_velocity_pe_ve: 10.5,
            position_velocity_pe_vd: 11.5,
            position_velocity_pd_vd: 12.5,
            velocity_velocity_vn_vn: 13.5,
            velocity_velocity_vn_ve: 14.5,
            velocity_velocity_vn_vd: 15.5,
            velocity_velocity_ve_ve: 16.5,
            velocity_velocity_ve_vd: 17.5,
            velocity_velocity_vd_vd: 18.5,
        },
    }
}

struct Scratch(PathBuf);
impl Scratch {
    fn new() -> Self {
        let dir = env::temp_dir().join(format!("ams-rust-image-control-{}", std::process::id()));
        fs::create_dir(&dir).unwrap();
        env::set_var("AMS_MEL_TEST_LIFETIME_LOG", dir.join("lifetime"));
        Self(dir)
    }
    fn events(&self) -> String {
        fs::read_to_string(self.0.join("lifetime")).unwrap_or_default()
    }
    fn count(&self, event: &str) -> usize {
        self.events().lines().filter(|line| *line == event).count()
    }
    fn no_buffers(&self) {
        for event in [
            "buffer_factory_called",
            "buffer_initialized",
            "buffer_registered",
            "buffer_unregistered",
        ] {
            assert_eq!(self.count(event), 0, "{event}: {}", self.events());
        }
    }
    fn unloaded(&self) {
        reclaimed();
        wait_until(|| self.count("library_unloaded") == 1);
        let events = self.events();
        let mut previous = 0;
        for event in [
            "channel_detached",
            "channel_destroyed",
            "control_destroyed",
            "manager_destroyed",
            "library_unloaded",
        ] {
            let offset = events
                .find(&format!("{event}\n"))
                .unwrap_or_else(|| panic!("missing {event}: {events}"));
            assert!(offset >= previous, "out of order {event}: {events}");
            previous = offset;
        }
        assert_eq!(self.count("channel_detached"), 1);
        assert_eq!(self.count("channel_destroyed"), 1);
    }
}
impl Drop for Scratch {
    fn drop(&mut self) {
        env::remove_var("AMS_MEL_TEST_LIFETIME_LOG");
        fs::remove_dir_all(&self.0).unwrap();
    }
}

fn wait_until(mut predicate: impl FnMut() -> bool) {
    let deadline = Instant::now() + Duration::from_secs(10);
    while !predicate() {
        assert!(Instant::now() < deadline, "lifecycle probe timed out");
        thread::sleep(Duration::from_millis(1));
    }
}

fn exercise(scenario: &str, scratch: &Scratch, session_first: bool, drop_only: bool) {
    let session = Session::open(provider(), scenario, "").unwrap();
    let owned_config = config();
    assert_eq!(owned_config.clone(), owned_config);
    let mut image = session.open_image_control(&owned_config).unwrap();
    drop(owned_config); // no borrowed input storage survives the native call
    let mut view = image.channel_view().unwrap();
    let capability = view.capabilities().unwrap();
    assert_eq!(
        (
            capability.width,
            capability.height,
            capability.bit_depth,
            capability.row_pitch
        ),
        (0, 0, 0, 0)
    );
    assert_eq!(
        (
            capability.buffer_size,
            capability.image_size,
            capability.number_of_bands
        ),
        (0, 0, 0)
    );
    // Published enum value zero is Mono. This is a copied capability value,
    // NOT selection of Mono8 acquisition or inference from the zero layout.
    assert_eq!(capability.pixel_format, PixelFormat::Mono);
    assert_eq!(capability.channel_types, vec![ChannelType::IrstImage]);
    // These are the provider snapshot's identities, not the submitted config.
    assert_eq!(capability.channel_id.uuid()[0], 0x11);
    assert_eq!(capability.platform_id.uuid()[0], 0x31);
    assert_eq!(capability.sensor_location.offset_x_m(), 1.25);
    assert_eq!(capability.sensor_location.offset_y_m(), -2.5);
    assert_eq!(capability.sensor_location.offset_z_m(), 3.75);
    assert_eq!(capability.metadata_capabilities.len(), 4);
    assert_eq!(capability.image_bands.len(), 2);

    let mut metadata = image.open_metadata(8).unwrap();
    let received: Vec<_> = (0..4).map(|_| metadata.receive(0).unwrap()).collect();
    assert!(
        matches!(&received[0], ImageMetadataEvent::BadPixelList(v) if v.pixels.len() == 3 && v.pixels[0].row == 1)
    );
    assert!(
        matches!(&received[1], ImageMetadataEvent::LineOfSightReport(v) if v.system_time_ns == -123456789)
    );
    assert!(
        matches!(&received[2], ImageMetadataEvent::LineOfSightEuler(v) if v.system_time_ns == 987654321)
    );
    let navigation = ImageMetadataEvent::NavigationResponse(NavigationResponse {
        system_time_ns: -8765432109,
        command_id: 0xf1234567,
        request_id: 0x89abcdef,
    });
    assert_eq!(received[3], navigation);
    assert_eq!(metadata.receive(0).unwrap_err().kind(), &ErrorKind::Timeout);
    assert_eq!(metadata.counters().unwrap().events_received, 4);

    let mut request = image.submit_navigation_report(&report()).unwrap(); // before Start
    let rejection = NavigationResult::Rejected {
        code: MelErrorCode::InvalidState,
        description: "no navigation hardware".into(),
    };
    assert_eq!(request.wait(5000).unwrap(), rejection);
    assert_eq!(request.wait(0).unwrap(), rejection);
    request.close().unwrap();
    assert_eq!(metadata.receive(0).unwrap(), navigation);

    let mut keep = view.send_keepalive().unwrap();
    assert_eq!(
        keep.wait(5000).unwrap(),
        ReturnResult::Completed {
            value: CommandReturn::Success
        }
    );
    keep.close().unwrap();
    let mut comms = view
        .submit_comms_test(CommsTestRequest {
            command_id: 0x80000001,
            channel_id: 0xf0000002,
            request_id: 0xe0000003,
        })
        .unwrap();
    let correlated = CommsTestResult::Completed {
        report: CommsTestReport {
            command_id: 0x80000001,
            request_id: 0xe0000003,
        },
    };
    assert_eq!(comms.wait(5000).unwrap(), correlated);
    assert_eq!(comms.wait(0).unwrap(), correlated);
    comms.close().unwrap();

    let uncertain = scenario.contains("release-");
    if uncertain {
        reclaimed();
        assert_eq!(
            image.start().unwrap_err().kind(),
            &ErrorKind::ProviderFailed
        );
    } else {
        image.start().unwrap();
        image.start().unwrap();
        assert_eq!(scratch.count("channel_enabled"), 1);
    }
    for timeout in [0, 100] {
        let error = image.receive(timeout).unwrap_err();
        assert_eq!(error.kind(), &ErrorKind::InitializationFailed);
        assert!(error.diagnostic().unwrap().contains("control-only"));
    }
    // Native counters count unsolicited callbacks too, not delivered samples.
    let unsolicited = if uncertain || scenario == "image-control-unexpected" {
        2
    } else if scenario == "image-control-attach-reentry" {
        1
    } else {
        0
    };
    let counters = image.counters().unwrap();
    assert_eq!(counters.frames_received, unsolicited);
    assert_eq!(counters.malformed_or_unsupported, unsolicited);
    scratch.no_buffers();
    metadata.close().unwrap();
    view.close().unwrap();
    view.close().unwrap();
    if drop_only {
        drop(image);
        drop(session);
    } else if session_first {
        session.close().unwrap();
        image.stop().unwrap();
        image.close().unwrap();
    } else if uncertain || scenario == "image-control-detach-fail" {
        assert_eq!(
            image.close().unwrap_err().kind(),
            &ErrorKind::ProviderFailed
        );
        // Existing consuming Rust Close invokes one Drop fallback if native
        // retained the owner. Detach-fail retries, but uncertain release stays.
        session.close().unwrap();
    } else {
        image.stop().unwrap();
        assert_eq!(image.start().unwrap_err().kind(), &ErrorKind::StreamStopped);
        image.close().unwrap();
        session.close().unwrap();
    }
    if scenario == "image-control-unexpected" || uncertain {
        assert_eq!(scratch.count("control_buffer_released"), 2);
        assert_eq!(scratch.count("control_reentry_returned"), 1);
    }
    if scenario == "image-control-attach-reentry" {
        assert_eq!(scratch.count("control_buffer_released"), 1);
        assert_eq!(scratch.count("control_attach_callback_returned"), 1);
    }
    for event in [
        "bad_pixel_registration_attempted",
        "line_of_sight_report_registration_attempted",
        "line_of_sight_euler_registration_attempted",
        "navigation_response_registration_attempted",
    ] {
        assert_eq!(scratch.count(event), 1, "{event}");
    }
    for event in [
        "nuc_registration_attempted",
        "odm_registration_attempted",
        "losq_registration_attempted",
        "camera_registration_attempted",
    ] {
        assert_eq!(scratch.count(event), 0, "unexpected optional {event}");
    }
    if uncertain {
        assert_eq!(scratch.count("channel_destroyed"), 0);
        assert_eq!(scratch.count("library_unloaded"), 0);
        assert_eq!(scratch.count("control_buffer_destroyed"), 1);
    } else {
        scratch.unloaded();
        assert_eq!(scratch.count("control_shutdown_callbacks_returned"), 1);
    }
    assert_eq!(received[3], navigation); // owned values survive teardown/unload
    assert_eq!(capability.image_bands.len(), 2);
    scratch.no_buffers();
}

fn failed_open(scenario: &str, scratch: &Scratch) {
    let session = Session::open(provider(), scenario, "").unwrap();
    let error = session.open_image_control(&config()).unwrap_err();
    let expected = match scenario {
        "attach-null" => ErrorKind::FactoryFailed,
        "attach-throw" | "image-control-attach-reentry-throw" => ErrorKind::ProviderException,
        "image-control-wrong-type" => ErrorKind::InitializationFailed,
        "image-control-open-detach-fail" => ErrorKind::ProviderFailed,
        _ => panic!("unknown failure"),
    };
    assert_eq!(error.kind(), &expected);
    let diagnostic = match scenario {
        "attach-null" => "attachChannel returned null",
        "attach-throw" | "image-control-attach-reentry-throw" => {
            "provider exception during stream open"
        }
        "image-control-wrong-type" => "provider does not advertise Mono8 single-band capability",
        "image-control-open-detach-fail" => "incompatible channel and detach failed",
        _ => unreachable!(),
    };
    assert_eq!(error.diagnostic(), Some(diagnostic));
    session.close().unwrap();
    scratch.no_buffers();
    if scenario != "image-control-wrong-type" {
        assert_eq!(scratch.count("control_destroyed"), 0);
        assert_eq!(scratch.count("library_unloaded"), 0);
    } else {
        wait_until(|| scratch.count("library_unloaded") == 1);
        assert_eq!(scratch.count("health_channel_destroyed"), 1);
    }
    if scenario == "image-control-attach-reentry-throw" {
        assert_eq!(scratch.count("control_attach_callback_returned"), 1);
        assert_eq!(scratch.count("control_buffer_released"), 1);
    }
}

// Test-only provider completion barrier. Keep the extra dlopen through release's
// return: a completion worker may otherwise unload its code while it executes.
// Close it before the final unload assertion; channel retention is independently
// checked before release and completion-owner reclamation is probed afterward.
fn pending(scratch: &Scratch, session_first: bool) {
    unsafe extern "C" {
        fn dlopen(path: *const std::ffi::c_char, flags: i32) -> *mut std::ffi::c_void;
        fn dlsym(
            handle: *mut std::ffi::c_void,
            name: *const std::ffi::c_char,
        ) -> *mut std::ffi::c_void;
        fn dlclose(handle: *mut std::ffi::c_void) -> i32;
    }
    let path = std::ffi::CString::new(provider().as_os_str().as_encoded_bytes()).unwrap();
    // SAFETY: known local mock DSO, RTLD_NOW; exact exported test-only signature.
    let (library, release) = unsafe {
        let library = dlopen(path.as_ptr(), 2);
        assert!(!library.is_null());
        let symbol = dlsym(library, c"mock_completion_release_family".as_ptr());
        assert!(!symbol.is_null());
        (
            library,
            std::mem::transmute::<*mut std::ffi::c_void, unsafe extern "C" fn(u32, u32) -> i32>(
                symbol,
            ),
        )
    };
    let session = Session::open(provider(), "image-control-pending", "").unwrap();
    let mut image = session.open_image_control(&config()).unwrap();
    let mut metadata = image.open_metadata(8).unwrap();
    let mut request = image.submit_navigation_report(&report()).unwrap();
    assert_eq!(request.wait(0).unwrap_err().kind(), &ErrorKind::Timeout);
    if session_first {
        session.close().unwrap();
        assert_eq!(request.wait(0).unwrap_err().kind(), &ErrorKind::Timeout);
        image.close().unwrap();
    } else {
        image.close().unwrap();
        assert_eq!(request.wait(0).unwrap_err().kind(), &ErrorKind::Timeout);
        session.close().unwrap();
    }
    metadata.close().unwrap();
    assert_eq!(scratch.count("channel_detached"), 0);
    assert_eq!(scratch.count("channel_destroyed"), 0);
    assert_eq!(scratch.count("library_unloaded"), 0);
    // SAFETY: explicit dlopen guard retains release's code until it returns.
    assert_eq!(unsafe { release(3, 1) }, 1);
    // SAFETY: release has returned and this is our unique dlopen reference.
    assert_eq!(unsafe { dlclose(library) }, 0);
    let expected = NavigationResult::Success {
        response: NavigationResponse {
            system_time_ns: -8765432109,
            command_id: 0xf1234567,
            request_id: 0x89abcdef,
        },
    };
    assert_eq!(request.wait(5000).unwrap(), expected);
    scratch.unloaded();
    assert_eq!(request.wait(0).unwrap(), expected);
    request.close().unwrap();
    scratch.no_buffers();
}

fn run(case: &str) {
    let output = Command::new(env::current_exe().unwrap())
        .args(["--exact", "child_case", "--nocapture"])
        .env(CHILD, case)
        .output()
        .unwrap();
    assert!(
        output.status.success(),
        "{case}: {}\n{}",
        String::from_utf8_lossy(&output.stdout),
        String::from_utf8_lossy(&output.stderr)
    );
    assert!(String::from_utf8_lossy(&output.stdout).contains("IMAGE_CONTROL_CHILD_COMPLETE"));
}

macro_rules! cases {
    ($($name:ident => $case:literal),* $(,)?) => { $(#[test] fn $name() { run($case); })* };
}
cases! {
    zero_layout => "image-control-zero",
    unsolicited_buffer_and_release_reentry => "image-control-unexpected",
    attachment_callback_reentry => "image-control-attach-reentry",
    detach_failure => "image-control-detach-fail",
    uncertain_release_failure => "image-control-release-fail",
    uncertain_release_exception => "image-control-release-throw",
    null_attachment => "attach-null",
    attachment_exception => "attach-throw",
    mismatched_channel_type => "image-control-wrong-type",
    failed_open_retains_on_detach_failure => "image-control-open-detach-fail",
    exception_after_listener_exposure => "image-control-attach-reentry-throw",
    pending_image_first => "pending-image-first",
    pending_session_first => "pending-session-first",
    session_first => "session-first",
    drop_cleanup => "drop",
    exact_config_provider_mapping => "mapping",
    mono8_acquisition_unchanged => "acquisition",
}

#[test]
fn child_case() {
    let Ok(case) = env::var(CHILD) else { return };
    let scratch = Scratch::new();
    match case.as_str() {
        "pending-image-first" | "pending-session-first" => {
            pending(&scratch, case == "pending-session-first")
        }
        "session-first" | "drop" => exercise(
            "image-control-zero",
            &scratch,
            case == "session-first",
            case == "drop",
        ),
        "mapping" => {
            // Existing success fixture checks all UUID bytes, labels and location
            // fields in Config during attach, and throws on any mismatch.
            let session = Session::open(provider(), "success", "").unwrap();
            session
                .open_image_control(&config())
                .unwrap()
                .close()
                .unwrap();
            session.close().unwrap();
            scratch.no_buffers();
            scratch.unloaded();
        }
        "acquisition" => {
            let zero = Session::open(provider(), "image-control-zero", "").unwrap();
            assert_eq!(
                zero.open_image_stream(&acquisition()).unwrap_err().kind(),
                &ErrorKind::InitializationFailed
            );
            zero.close().unwrap();
            let session = Session::open(provider(), "success", "").unwrap();
            let mut image = session.open_image_stream(&acquisition()).unwrap();
            image.start().unwrap();
            let frame = image.receive(1000).unwrap();
            assert_eq!(
                (
                    frame.width,
                    frame.height,
                    frame.bits_per_pixel,
                    frame.number_of_bands
                ),
                (4, 3, 8, 1)
            );
            assert_eq!(frame.pixels, (16u8..28).collect::<Vec<_>>());
            image.stop().unwrap();
            image.close().unwrap();
            session.close().unwrap();
            assert!(scratch.count("buffer_factory_called") > 0);
        }
        "attach-null"
        | "attach-throw"
        | "image-control-wrong-type"
        | "image-control-open-detach-fail"
        | "image-control-attach-reentry-throw" => failed_open(&case, &scratch),
        _ => exercise(&case, &scratch, false, false),
    }
    println!("IMAGE_CONTROL_CHILD_COMPLETE");
}
