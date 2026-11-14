//! Public safe API qualification using the existing separate mock provider.
use ams_mel::{
    AttitudeRate, ComponentLocation, ErrorKind, Euler, ImageConfig, ImageMetadataEvent,
    MelErrorCode, NavigationReport, NavigationResponse, NavigationResult, NorthEastDown,
    PositionSolutionState, PositionVelocityCovariance, Session, UciId,
};
use std::{
    env, fs,
    path::{Path, PathBuf},
    sync::Mutex,
    thread,
    time::{Duration, Instant},
};

static TEST: Mutex<()> = Mutex::new(());

fn report() -> NavigationReport {
    NavigationReport {
        system_time_ns: -123456789012,
        state: PositionSolutionState::Blended,
        // Exact mock fixture 0.523598, intentionally NOT pi/6. Preserve the
        // provider's input rather than replacing it with a mathematical angle.
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

fn expected() -> NavigationResult {
    NavigationResult::Success {
        response: NavigationResponse {
            system_time_ns: -8765432109,
            command_id: 0xf1234567,
            request_id: 0x89abcdef,
        },
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

fn open(scenario: &str) -> Session {
    Session::open(provider(), scenario, "").unwrap()
}

fn config() -> ImageConfig {
    ImageConfig::with_limits(
        UciId::new([0; 16], "IR image channel").unwrap(),
        UciId::new([0; 16], "test platform").unwrap(),
        ComponentLocation::new(0.0, 0.0, 0.0, "station-1", "mock-aircraft").unwrap(),
        2,
        64,
        2,
    )
    .unwrap()
}

#[test]
fn complete_input_fidelity_before_start_and_cached_response() {
    let _guard = TEST.lock().unwrap();
    // Mock checks every getter, including all 18 distinct covariance terms.
    let session = open("navigation-fidelity");
    let mut image = session.open_image_stream(&config()).unwrap();
    let mut request = image.submit_navigation_report(&report()).unwrap();
    assert_eq!(request.wait(5_000).unwrap(), expected());
    assert_eq!(request.wait(0).unwrap(), expected());
    request.close().unwrap();
    image.close().unwrap();
    session.close().unwrap();
}

#[test]
fn immediate_completion_multiple_requests_and_running_submission() {
    let _guard = TEST.lock().unwrap();
    let session = open("navigation-sync-two");
    let mut image = session.open_image_stream(&config()).unwrap();
    let mut first = image.submit_navigation_report(&report()).unwrap();
    image.start().unwrap();
    let mut second = image.submit_navigation_report(&report()).unwrap();
    assert_eq!(second.wait(5_000).unwrap(), expected());
    assert_eq!(first.wait(5_000).unwrap(), expected());
    assert_eq!(second.wait(0).unwrap(), expected());
    image.stop().unwrap();
    assert_eq!(
        image
            .submit_navigation_report(&report())
            .unwrap_err()
            .kind(),
        &ErrorKind::ProviderFailed
    );
    image.close().unwrap();
}

#[test]
fn all_solution_states_and_invalid_domain() {
    let _guard = TEST.lock().unwrap();
    let session = open("navigation-sync");
    let mut image = session.open_image_stream(&config()).unwrap();
    for state in 0..5 {
        let mut report = report();
        report.state = PositionSolutionState::try_from(state).unwrap();
        assert_eq!(
            image
                .submit_navigation_report(&report)
                .unwrap()
                .wait(5_000)
                .unwrap(),
            expected()
        );
    }
    for invalid in [5, 6, u32::MAX] {
        assert_eq!(
            PositionSolutionState::try_from(invalid).unwrap_err().kind(),
            &ErrorKind::InvalidArgument
        );
    }
}

#[test]
fn safe_submission_accepts_untransformed_numeric_boundaries() {
    let _guard = TEST.lock().unwrap();
    let session = open("navigation-sync");
    let mut image = session.open_image_stream(&config()).unwrap();
    for (time, rate_time) in [(i64::MIN, i64::MAX), (i64::MAX, i64::MIN)] {
        for value in [
            0.0,
            -0.0,
            f64::from_bits(1),
            f64::MAX,
            f64::MIN,
            f64::INFINITY,
            f64::NEG_INFINITY,
            f64::NAN,
        ] {
            let mut input = report();
            input.system_time_ns = time;
            input.attitude_rate.attitude_rate_time_ns = rate_time;
            input.latitude_rad = value;
            input.longitude_rad = value;
            input.altitude_m = value;
            input.magnetic_heading = value;
            input
                .position_velocity_covariance_uncertainty
                .position_position_pn_pn = value;
            assert_eq!(
                image
                    .submit_navigation_report(&input)
                    .unwrap()
                    .wait(5_000)
                    .unwrap(),
                expected()
            );
        }
    }
}

#[test]
fn exact_provider_rejections_including_long_utf8() {
    let _guard = TEST.lock().unwrap();
    for (scenario, description) in [
        ("navigation-reject", "invalid navigation report".to_owned()),
        (
            "navigation-reject-long",
            format!("{}€{}", "x".repeat(510), "y".repeat(100)),
        ),
    ] {
        let session = open(scenario);
        let mut image = session.open_image_stream(&config()).unwrap();
        let mut request = image.submit_navigation_report(&report()).unwrap();
        let expected = NavigationResult::Rejected {
            code: MelErrorCode::InvalidParameters,
            description,
        };
        assert_eq!(request.wait(5_000).unwrap(), expected);
        assert_eq!(request.wait(0).unwrap(), expected);
    }
}

#[test]
fn provider_exception_failed_future_and_null_response() {
    let _guard = TEST.lock().unwrap();
    for (scenario, kind, description) in [
        (
            "navigation-send-throw",
            ErrorKind::ProviderException,
            "mock navigation send exception",
        ),
        (
            "navigation-future-throw",
            ErrorKind::ProviderException,
            "mock navigation future exception",
        ),
        (
            "navigation-null-result",
            ErrorKind::ProviderFailed,
            "provider returned null successful NavigationReportResp",
        ),
    ] {
        let session = open(scenario);
        let mut image = session.open_image_stream(&config()).unwrap();
        let error = if scenario == "navigation-send-throw" {
            image.submit_navigation_report(&report()).unwrap_err()
        } else {
            let mut request = image.submit_navigation_report(&report()).unwrap();
            let error = request.wait(5_000).unwrap_err();
            assert_eq!(request.wait(0).unwrap_err(), error);
            error
        };
        assert_eq!(error.kind(), &kind);
        assert_eq!(error.diagnostic(), Some(description));
        assert_eq!(error.diagnostic_required(), Some(description.len() + 1));
    }
}

// Exact completion-owner observation, not future arrival or elapsed time.
extern "C" {
    fn ams_mel_test_completion_boundary(
        family: u32,
        operation: u32,
        target: u64,
        counts: *mut u64,
    ) -> i32;
    fn ams_mel_test_completion_owner(family: u32, target: u64, observed: *mut u64) -> i32;
}

fn reclaimed() {
    let mut counts = [0; 4];
    let mut observed = 0;
    // SAFETY: existing test-only probe signatures, Navigation family = 3.
    unsafe {
        assert_eq!(
            ams_mel_test_completion_boundary(3, 3, 0, counts.as_mut_ptr()),
            1
        );
        assert_eq!(
            ams_mel_test_completion_owner(3, counts[2], &mut observed),
            1
        );
    }
}

struct Hold {
    directory: PathBuf,
    base: PathBuf,
    log: PathBuf,
}
impl Hold {
    fn new() -> Self {
        // Earlier tests can return from Wait before native worker reclamation.
        // Drain those owners before installing a process-global lifetime log.
        reclaimed();
        let directory = env::temp_dir().join(format!("rust-ir-008-{}", std::process::id()));
        fs::create_dir(&directory).unwrap();
        let base = directory.join("hold");
        let log = directory.join("lifetime.log");
        env::set_var("AMS_MEL_TEST_IMAGE_NAVIGATION_HOLD_BARRIER", &base);
        env::set_var("AMS_MEL_TEST_LIFETIME_LOG", &log);
        Self {
            directory,
            base,
            log,
        }
    }
    fn marker(&self, suffix: &str) -> PathBuf {
        self.base.with_extension(suffix)
    }
    fn release(&self) {
        fs::write(self.marker("release"), "").unwrap();
    }
    fn complete(&self) {
        fs::write(self.marker("complete"), "").unwrap();
    }
    fn callback(&self) {
        let deadline = Instant::now() + Duration::from_secs(5);
        while !self.marker("callback-done").exists() {
            assert!(Instant::now() < deadline, "callback barrier deadline");
            thread::sleep(Duration::from_millis(1));
        }
    }
    fn events(&self) -> String {
        fs::read_to_string(&self.log).unwrap_or_default()
    }
    fn assert_order(&self) {
        let text = self.events();
        let events: Vec<_> = text.lines().collect();
        let index = |name| {
            events
                .iter()
                .position(|event| *event == name)
                .unwrap_or_else(|| panic!("missing {name}: {text}"))
        };
        assert!(
            index("navigation_completed") < index("channel_destroyed"),
            "{text}"
        );
        assert!(
            index("channel_destroyed") < index("library_unloaded"),
            "{text}"
        );
        assert!(
            index("callbacks_quiesced_by_channel_destruction") < index("library_unloaded"),
            "{text}"
        );
        assert_eq!(
            events.iter().filter(|e| **e == "channel_destroyed").count(),
            1
        );
        assert_eq!(
            events.iter().filter(|e| **e == "library_unloaded").count(),
            1
        );
    }
}
impl Drop for Hold {
    fn drop(&mut self) {
        // Unblock a provider even if an assertion panics.
        let _ = fs::write(self.marker("release"), "");
        let _ = fs::write(self.marker("complete"), "");
        env::remove_var("AMS_MEL_TEST_IMAGE_NAVIGATION_HOLD_BARRIER");
        env::remove_var("AMS_MEL_TEST_LIFETIME_LOG");
        if !thread::panicking() {
            fs::remove_dir_all(&self.directory).unwrap();
        }
    }
}

#[test]
fn callback_arrival_is_not_completion_and_timeout_does_not_cancel() {
    let _guard = TEST.lock().unwrap();
    let hold = Hold::new();
    let session = open("navigation-hold");
    let mut image = session.open_image_stream(&config()).unwrap();
    let mut metadata = image.open_metadata(8).unwrap();
    while metadata.receive(0).is_ok() {}
    let mut request = image.submit_navigation_report(&report()).unwrap();
    assert_eq!(request.wait(0).unwrap_err().kind(), &ErrorKind::Timeout);
    hold.release();
    hold.callback();
    loop {
        if let ImageMetadataEvent::NavigationResponse(response) = metadata.receive(0).unwrap() {
            assert_eq!(NavigationResult::Success { response }, expected());
            break;
        }
    }
    assert_eq!(request.wait(0).unwrap_err().kind(), &ErrorKind::Timeout);
    hold.complete();
    assert_eq!(request.wait(5_000).unwrap(), expected());
    assert_eq!(request.wait(0).unwrap(), expected());
    reclaimed();
    drop(request);
    metadata.close().unwrap();
    image.close().unwrap();
    session.close().unwrap();
    hold.assert_order();
}

#[test]
fn requests_survive_each_parent_close_order_and_defer_unload() {
    let _guard = TEST.lock().unwrap();
    for session_first in [false, true] {
        let hold = Hold::new();
        let session = open("navigation-hold");
        let mut image = session.open_image_stream(&config()).unwrap();
        let mut request = image.submit_navigation_report(&report()).unwrap();
        assert_eq!(request.wait(0).unwrap_err().kind(), &ErrorKind::Timeout);
        // The hold scenario keeps the first future pending and completes later
        // sends immediately. Both public request owners coexist independently.
        let mut second = image.submit_navigation_report(&report()).unwrap();
        assert_eq!(second.wait(5_000).unwrap(), expected());
        assert_eq!(request.wait(0).unwrap_err().kind(), &ErrorKind::Timeout);
        if session_first {
            session.close().unwrap();
            image.close().unwrap();
        } else {
            image.close().unwrap();
            session.close().unwrap();
        }
        assert!(!hold.events().contains("channel_destroyed"));
        assert!(!hold.events().contains("library_unloaded"));
        hold.release();
        hold.callback();
        assert_eq!(request.wait(0).unwrap_err().kind(), &ErrorKind::Timeout);
        hold.complete();
        assert_eq!(request.wait(5_000).unwrap(), expected());
        reclaimed();
        // Cached value is usable even after the provider library is gone.
        hold.assert_order();
        assert_eq!(request.wait(0).unwrap(), expected());
        assert_eq!(second.wait(0).unwrap(), expected());
        request.close().unwrap();
        second.close().unwrap();
    }
}

#[test]
fn pending_request_drop_and_explicit_close_are_not_cancellation() {
    let _guard = TEST.lock().unwrap();
    for explicit in [false, true] {
        let hold = Hold::new();
        let session = open("navigation-hold");
        let mut image = session.open_image_stream(&config()).unwrap();
        let request = image.submit_navigation_report(&report()).unwrap();
        if explicit {
            request.close().unwrap();
        } else {
            drop(request);
        }
        image.close().unwrap();
        session.close().unwrap();
        assert!(!hold.events().contains("channel_destroyed"));
        assert!(!hold.events().contains("library_unloaded"));
        hold.release();
        hold.callback();
        hold.complete();
        reclaimed();
        hold.assert_order();
    }
}

#[test]
fn deferred_cleanup_failure_is_a_cached_provider_failure_not_success() {
    let _guard = TEST.lock().unwrap();
    let hold = Hold::new();
    let session = open("navigation-hold-detach-fail");
    let mut image = session.open_image_stream(&config()).unwrap();
    let mut request = image.submit_navigation_report(&report()).unwrap();
    image.stop().unwrap();
    assert_eq!(
        image
            .submit_navigation_report(&report())
            .unwrap_err()
            .kind(),
        &ErrorKind::ProviderFailed
    );
    hold.release();
    hold.callback();
    hold.complete();
    let error = request.wait(5_000).unwrap_err();
    assert_eq!(error.kind(), &ErrorKind::ProviderFailed);
    assert_eq!(error.diagnostic(), Some("deferred Image cleanup failed"));
    assert_eq!(request.wait(0).unwrap_err(), error);
    reclaimed();
    assert!(!hold.events().contains("channel_destroyed"));
    // Native Close retries detach successfully but preserves the earlier
    // poisoned lifecycle status; it must not fabricate OK after cleanup failed.
    assert_eq!(
        image.close().unwrap_err().kind(),
        &ErrorKind::ProviderFailed
    );
    session.close().unwrap();
    hold.assert_order();
}
