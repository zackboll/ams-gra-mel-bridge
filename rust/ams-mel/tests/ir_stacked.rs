//! Safe Stacked foundation. Every mock scenario has its own subprocess.
use ams_mel::*;
use std::{
    env,
    ffi::{c_char, c_void, CString},
    fs,
    path::{Path, PathBuf},
    process::Command,
};

const CHILD: &str = "AMS_MEL_RUST_STACKED_CHILD";
const HIGH: CommsTestRequest = CommsTestRequest {
    command_id: 0x8000_0001,
    channel_id: 0xf000_0002,
    request_id: 0xe000_0003,
};
const REPORT: CommsTestResult = CommsTestResult::Completed {
    report: CommsTestReport {
        command_id: HIGH.command_id,
        request_id: HIGH.request_id,
    },
};

unsafe extern "C" {
    fn dlopen(path: *const c_char, flags: i32) -> *mut c_void;
    fn dlsym(handle: *mut c_void, name: *const c_char) -> *mut c_void;
    fn dlclose(handle: *mut c_void) -> i32;
    fn ams_mel_test_completion_boundary(family: u32, op: u32, target: u64, out: *mut u64) -> i32;
    fn ams_mel_test_completion_owner(family: u32, target: u64, out: *mut u64) -> i32;
}

fn reclaimed() {
    for family in [1, 2] {
        let mut counts = [0; 4];
        let mut observed = 0;
        // SAFETY: exact established test-only signatures; writable output slots.
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

// Test-only pin for deterministic gates, released before asserting unload.
struct Gate(*mut c_void);
impl Gate {
    fn new() -> Self {
        let path = CString::new(provider().as_os_str().as_encoded_bytes()).unwrap();
        // SAFETY: Linux RTLD_NOW | RTLD_NOLOAD, already loaded by Session.
        let pin = unsafe { dlopen(path.as_ptr(), 2 | 4) };
        assert!(!pin.is_null());
        Self(pin)
    }
    fn release(&self, family: u32, count: u32) {
        // SAFETY: known mock-only export, exact signature, pin live during call.
        unsafe {
            let symbol = dlsym(self.0, c"mock_completion_release_family".as_ptr());
            assert!(!symbol.is_null());
            let release: unsafe extern "C" fn(u32, u32) -> i32 = std::mem::transmute(symbol);
            assert_eq!(release(family, count), 1);
        }
    }
}
impl Drop for Gate {
    fn drop(&mut self) {
        // SAFETY: unique test dlopen reference, never a facade owner.
        assert_eq!(unsafe { dlclose(self.0) }, 0);
    }
}

struct Log(PathBuf);
impl Log {
    fn new() -> Self {
        let directory = env::temp_dir().join(format!("ams-rust-stacked-{}", std::process::id()));
        fs::create_dir(&directory).unwrap();
        env::set_var("AMS_MEL_TEST_LIFETIME_LOG", directory.join("events"));
        Self(directory)
    }
    fn text(&self) -> String {
        fs::read_to_string(self.0.join("events")).unwrap_or_default()
    }
    fn count(&self, event: &str) -> usize {
        self.text().lines().filter(|line| *line == event).count()
    }
    fn order(&self, events: &[&str]) {
        let text = self.text();
        let mut offset = 0;
        for event in events {
            let index = text
                .find(&format!("{event}\n"))
                .unwrap_or_else(|| panic!("missing {event}: {text}"));
            assert!(index >= offset, "out of order {event}: {text}");
            offset = index;
        }
    }
    fn finish(&self, retained: bool) {
        reclaimed();
        assert_eq!(
            self.count("library_unloaded"),
            usize::from(!retained),
            "{}",
            self.text()
        );
        for event in [
            "stacked_forbidden_registration",
            "buffer_factory_called",
            "buffer_initialized",
            "buffer_registered",
        ] {
            assert_eq!(self.count(event), 0, "{event}");
        }
    }
}
impl Drop for Log {
    fn drop(&mut self) {
        env::remove_var("AMS_MEL_TEST_LIFETIME_LOG");
        fs::remove_dir_all(&self.0).unwrap();
    }
}

fn config() -> StackedConfig {
    StackedConfig::new(
        UciId::new(
            std::array::from_fn(|i| 0xa0 + i as u8),
            "IR stacked μ channel",
        )
        .unwrap(),
        UciId::new(std::array::from_fn(|i| 0xf0 + i as u8), "test platform").unwrap(),
        ComponentLocation::new(1.25, -2.5, 3.75, "station-1", "mock-aircraft").unwrap(),
    )
}

fn expired(view: &mut ChannelView) {
    assert!(view.is_open());
    assert_eq!(
        view.capabilities().unwrap_err().kind(),
        &ErrorKind::ProviderFailed
    );
    assert_eq!(
        view.send_keepalive().unwrap_err().kind(),
        &ErrorKind::ProviderFailed
    );
    assert_eq!(
        view.submit_comms_test(HIGH).unwrap_err().kind(),
        &ErrorKind::ProviderFailed
    );
}

fn profile(value: &ChannelCapability, depth: u32) {
    assert_eq!(
        value.channel_types,
        [ChannelType::StackedImage, ChannelType::Reserved1]
    );
    assert_eq!(
        value.metadata_capabilities,
        [MetadataCapability::ChannelCommsTestRep]
    );
    assert_eq!(value.task_schedule_depth, depth);
    assert_eq!(
        value.channel_id.uuid(),
        &std::array::from_fn(|i| 0x11 + (i * 7) as u8)
    );
    assert_eq!(
        value.platform_id.uuid(),
        &std::array::from_fn(|i| 0x31u8.wrapping_add((i * 7) as u8))
    );
    assert_eq!(value.channel_id.descriptive_label(), "channel-α");
    assert_eq!(value.platform_id.descriptive_label(), "platform-€");
    assert_eq!(
        (value.height, value.width, value.bit_depth, value.row_pitch),
        (1080, 1920, 12, 4096)
    );
    assert_eq!(
        (value.buffer_size, value.image_size, value.number_of_bands),
        (8388608, 4147200, 3)
    );
    assert_eq!(value.pixel_format, PixelFormat::Rgb);
    assert_eq!(
        value.sensor_types,
        [SensorType::GimbalHorizontal, SensorType::StepStare]
    );
    assert_eq!(
        value.sensor_location,
        ComponentLocation::new(1.25, -2.5, 3.75, "sensor-key", "system-β").unwrap()
    );
    assert!(value.odc_available && value.nuc_available);
    assert_eq!(
        value.image_bands,
        [
            ImageBand {
                band_index: 2,
                bands: vec![
                    BandInfo {
                        kind: BandType::IrLongwave,
                        min_wavelength_m: 8e-6,
                        max_wavelength_m: 12e-6
                    },
                    BandInfo {
                        kind: BandType::IrMidwave,
                        min_wavelength_m: 3e-6,
                        max_wavelength_m: 5e-6
                    },
                ]
            },
            ImageBand {
                band_index: 9,
                bands: vec![BandInfo {
                    kind: BandType::VisibleRed,
                    min_wavelength_m: 620e-9,
                    max_wavelength_m: 750e-9
                }]
            },
        ]
    );
    assert_eq!(
        value.nav_frames,
        [CoordinateSystem::NedSensor, CoordinateSystem::Ecef]
    );
}

fn lifecycle(scenario: &str, log: &Log) {
    let session = Session::open(provider(), scenario, "").unwrap();
    let discovered = session.capabilities().unwrap();
    let owned = config();
    let mut stacked = session.open_stacked_channel(&owned).unwrap();
    drop(owned);
    assert_eq!(log.count("stacked_attach_requested"), 1);
    assert_eq!(log.count("stacked_enabled"), 0);
    let before = stacked.capabilities().unwrap();
    assert_eq!(before, discovered[0]);
    let mut a = stacked.channel_view().unwrap();
    let mut b = stacked.channel_view().unwrap();
    assert_eq!(a.capabilities().unwrap(), before);
    a.close().unwrap();
    assert!(stacked.is_open());
    stacked.enable().unwrap();
    stacked.enable().unwrap();
    assert_eq!(log.count("stacked_enabled"), 1);
    assert_eq!(stacked.capabilities().unwrap(), before);
    session.close().unwrap();
    assert_eq!(log.count("control_destroyed"), 0);
    assert_eq!(b.capabilities().unwrap(), before);
    stacked.close().unwrap();
    stacked.close().unwrap();
    assert!(!stacked.is_open());
    expired(&mut b);
    assert_eq!(
        stacked.enable().unwrap_err().diagnostic(),
        Some("IR Stacked channel is closed")
    );
    assert!(stacked.capabilities().is_err());
    assert!(stacked.channel_view().is_err());
    log.finish(false); // live weak view cannot delay unload
    if scenario == "stacked-zero" {
        assert_eq!(
            (
                before.height,
                before.width,
                before.bit_depth,
                before.row_pitch,
                before.buffer_size,
                before.image_size,
                before.number_of_bands
            ),
            (0, 0, 0, 0, 0, 0, 0)
        );
        assert!(before.image_bands.is_empty() && before.nav_frames.is_empty());
    } else {
        profile(
            &before,
            match scenario {
                "stacked-depth-zero" => 0,
                "stacked-depth-max" => u32::MAX,
                _ => 23,
            },
        );
    }
    if scenario == "stacked-config" {
        assert_eq!(log.count("stacked_config_verified"), 1);
    }
    log.order(&[
        "stacked_enabled",
        "stacked_disabled",
        "channel_detached",
        "stacked_channel_destroyed",
        "control_destroyed",
        "manager_destroyed",
        "library_unloaded",
    ]);
}

fn open_failure(scenario: &str, log: &Log) {
    let session = Session::open(provider(), scenario, "").unwrap();
    let error = session.open_stacked_channel(&config()).unwrap_err();
    let (kind, attaches, retained) = match scenario {
        "stacked-not-advertised" | "stacked-empty" => (ErrorKind::InitializationFailed, 0, false),
        "stacked-discovery-throw" => (ErrorKind::ProviderException, 0, false),
        "stacked-attach-null" => (ErrorKind::FactoryFailed, 1, false),
        "stacked-attach-throw" => (ErrorKind::ProviderException, 1, false),
        "stacked-open-detach-fail" | "stacked-capability-throw-detach-fail" => {
            (ErrorKind::ProviderFailed, 1, true)
        }
        _ => (ErrorKind::InitializationFailed, 1, false),
    };
    assert_eq!(error.kind(), &kind, "{error:?}");
    assert!(!error.diagnostic().unwrap().is_empty());
    assert_eq!(log.count("stacked_attach_requested"), attaches);
    assert!(!session.provider_version().unwrap().vendor.is_empty());
    // Empty/throwing discovery also prevents C2 discovery by fixture design.
    // Version queries above still prove the Session was not poisoned.
    let c2 = session.open_control_channel(&ControlConfig::new(
        UciId::new([0; 16], "").unwrap(),
        UciId::new([0; 16], "").unwrap(),
        ComponentLocation::new(0., 0., 0., "", "").unwrap(),
    ));
    if scenario == "stacked-empty" {
        assert_eq!(c2.unwrap_err().kind(), &ErrorKind::InitializationFailed);
    } else if scenario == "stacked-discovery-throw" {
        assert_eq!(c2.unwrap_err().kind(), &ErrorKind::ProviderException);
    } else {
        c2.unwrap().close().unwrap();
    }
    session.close().unwrap();
    log.finish(retained);
}

fn failure(scenario: &str, log: &Log) {
    let native_scenario = if scenario == "stacked-drop-detach-fail" {
        "stacked-detach-fail"
    } else {
        scenario
    };
    let session = Session::open(provider(), native_scenario, "").unwrap();
    let mut stacked = session.open_stacked_channel(&config()).unwrap();
    if scenario.contains("enable-") {
        let error = stacked.enable().unwrap_err();
        assert_eq!(
            error.kind(),
            &if scenario.ends_with("throw") {
                ErrorKind::ProviderException
            } else {
                ErrorKind::ProviderFailed
            }
        );
        assert!(!error.diagnostic().unwrap().is_empty());
        assert_eq!(
            stacked.enable().unwrap_err().kind(),
            &ErrorKind::ProviderFailed
        );
        assert_eq!(
            stacked.capabilities().unwrap_err().kind(),
            &ErrorKind::ProviderFailed
        );
        assert_eq!(log.count("stacked_enabled"), 1);
    } else if scenario != "stacked-close-before-enable" {
        stacked.enable().unwrap();
    }
    if scenario == "stacked-drop-detach-fail" {
        session.close().unwrap();
        drop(stacked);
        assert_eq!(log.count("channel_detach_failed"), 1);
        log.finish(true);
        return;
    }
    let mut session = Some(session);
    if scenario.contains("detach-") || scenario.contains("disable-") {
        assert_eq!(
            stacked.close().unwrap_err().kind(),
            &ErrorKind::ProviderFailed
        );
        if scenario.contains("detach-") {
            assert!(stacked.is_open());
            assert!(stacked.capabilities().is_err());
            assert!(stacked.enable().is_err());
            session.take().unwrap().close().unwrap();
            assert_eq!(log.count("library_unloaded"), 0);
            stacked.close().unwrap();
        } else {
            assert!(!stacked.is_open());
        }
    } else {
        stacked.close().unwrap();
    }
    stacked.close().unwrap();
    drop(stacked);
    if let Some(session) = session {
        session.close().unwrap();
    }
    assert_eq!(
        log.count("stacked_disabled"),
        usize::from(scenario != "stacked-close-before-enable")
    );
    assert_eq!(log.count("channel_detached"), 1);
    log.finish(false);
}

fn pending(scenario: &str, log: &Log) {
    let drop_only = scenario == "stacked-drop-pending";
    let abandon = scenario == "stacked-abandon-pending";
    let fail = scenario == "stacked-deferred-detach-fail";
    let session = Session::open_with_options(
        provider(),
        scenario,
        "",
        SessionOptions {
            max_async_requests: 2,
        },
    )
    .unwrap();
    let gate = Gate::new();
    let mut stacked = session.open_stacked_channel(&config()).unwrap();
    let mut a = stacked.channel_view().unwrap();
    let mut b = stacked.channel_view().unwrap();
    stacked.enable().unwrap();
    let mut ret = a.send_keepalive().unwrap();
    let mut comms = b.submit_comms_test(HIGH).unwrap();
    assert_eq!(ret.wait(0).unwrap_err().kind(), &ErrorKind::Timeout);
    assert_eq!(comms.wait(0).unwrap_err().kind(), &ErrorKind::Timeout);
    assert_eq!(
        a.send_keepalive().unwrap_err().kind(),
        &ErrorKind::ResourceExhausted
    );
    if drop_only {
        drop(a);
        drop(b);
        drop(stacked);
        drop(session);
    } else {
        a.close().unwrap();
        stacked.close().unwrap();
        session.close().unwrap();
        assert_eq!(
            b.capabilities().unwrap_err().kind(),
            &ErrorKind::ProviderFailed
        );
        // Admission happens before family liveness checks in the shared engine.
        assert_eq!(
            b.send_keepalive().unwrap_err().kind(),
            &ErrorKind::ResourceExhausted
        );
        assert_eq!(
            b.submit_comms_test(HIGH).unwrap_err().kind(),
            &ErrorKind::ResourceExhausted
        );
        b.close().unwrap();
    }
    assert_eq!(log.count("channel_detached"), 0);
    assert_eq!(log.count("stacked_disabled"), 0);
    if abandon {
        ret.close().unwrap();
        drop(comms); // both explicit request Close and fallback Drop are non-cancellation
        gate.release(14, 1);
        gate.release(13, 1);
    } else {
        gate.release(14, 1);
        assert_eq!(comms.wait(15000).unwrap(), REPORT);
        assert_eq!(comms.wait(0).unwrap(), REPORT);
        assert_eq!(log.count("channel_detached"), 0);
        gate.release(13, 1);
        if fail {
            assert_eq!(
                ret.wait(15000).unwrap_err().kind(),
                &ErrorKind::ProviderFailed
            );
        } else {
            let expected = ReturnResult::Completed {
                value: if scenario == "stacked-return-fail" {
                    CommandReturn::Fail
                } else {
                    CommandReturn::Success
                },
            };
            assert_eq!(ret.wait(15000).unwrap(), expected);
            assert_eq!(ret.wait(0).unwrap(), expected);
        }
        ret.close().unwrap();
        comms.close().unwrap();
    }
    reclaimed();
    drop(gate);
    assert_eq!(log.count("stacked_disabled"), 1);
    assert_eq!(log.count("stacked_channel_destroyed"), usize::from(!fail));
    log.finish(fail);
}

fn accounting(log: &Log) {
    let session = Session::open_with_options(
        provider(),
        "stacked-basic",
        "",
        SessionOptions {
            max_async_requests: 2,
        },
    )
    .unwrap();
    let gate = Gate::new();
    let mut stacked = session.open_stacked_channel(&config()).unwrap();
    let mut health = session
        .open_health_channel(&HealthConfig::new(
            UciId::new([0; 16], "").unwrap(),
            UciId::new([0; 16], "").unwrap(),
            ComponentLocation::new(0., 0., 0., "", "").unwrap(),
        ))
        .unwrap();
    let mut sv = stacked.channel_view().unwrap();
    let mut hv = health.channel_view().unwrap();
    let mut sr = sv.send_keepalive().unwrap();
    let mut hr = hv.send_keepalive().unwrap();
    assert_eq!(
        sv.send_keepalive().unwrap_err().kind(),
        &ErrorKind::ResourceExhausted
    );
    gate.release(13, 1);
    assert_eq!(
        sr.wait(15000).unwrap(),
        ReturnResult::Completed {
            value: CommandReturn::Success
        }
    );
    stacked.close().unwrap();
    assert_eq!(log.count("stacked_channel_destroyed"), 1);
    assert_eq!(hr.wait(0).unwrap_err().kind(), &ErrorKind::Timeout);
    health.close().unwrap();
    session.close().unwrap();
    gate.release(1, 1);
    assert_eq!(
        hr.wait(15000).unwrap(),
        ReturnResult::Completed {
            value: CommandReturn::Success
        }
    );
    sr.close().unwrap();
    hr.close().unwrap();
    reclaimed();
    drop(gate);
    log.finish(false);
}

fn common_attached_and_enabled(log: &Log) {
    let session = Session::open(provider(), "stacked-basic", "").unwrap();
    let gate = Gate::new();
    let mut stacked = session.open_stacked_channel(&config()).unwrap();
    let mut view = stacked.channel_view().unwrap();
    let snapshot = stacked.capabilities().unwrap();
    for enabled in [false, true] {
        if enabled {
            stacked.enable().unwrap();
        }
        let mut keepalive = view.send_keepalive().unwrap();
        gate.release(13, 1);
        assert_eq!(
            keepalive.wait(15000).unwrap(),
            ReturnResult::Completed {
                value: CommandReturn::Success
            }
        );
        keepalive.close().unwrap();
        let mut comms = view.submit_comms_test(HIGH).unwrap();
        gate.release(14, 1);
        assert_eq!(comms.wait(15000).unwrap(), REPORT);
        comms.close().unwrap();
        assert_eq!(view.capabilities().unwrap(), snapshot);
    }
    reclaimed();
    drop(gate);
    view.close().unwrap();
    stacked.close().unwrap();
    session.close().unwrap();
    log.finish(false);
}

macro_rules! scenarios {
    ($($name:ident => $value:literal),+ $(,)?) => { $(#[test] fn $name() {
        let output = Command::new(env::current_exe().unwrap())
            .args(["--exact", "scenario_child", "--ignored", "--nocapture"])
            .env(CHILD, $value).output().unwrap();
        assert!(output.status.success(), "{}\n{}", String::from_utf8_lossy(&output.stdout), String::from_utf8_lossy(&output.stderr));
        assert!(String::from_utf8_lossy(&output.stdout).contains("STACKED_CHILD_COMPLETE"));
    })+ };
}
scenarios! {
    owned_attachment => "stacked-config", zero_layout => "stacked-zero",
    depth_zero => "stacked-depth-zero", depth_max => "stacked-depth-max",
    missing => "stacked-not-advertised", empty => "stacked-empty",
    discovery_exception => "stacked-discovery-throw", null_attachment => "stacked-attach-null",
    open_exception => "stacked-attach-throw", wrong_concrete => "stacked-wrong-concrete",
    missing_attached_type => "stacked-capability-wrong", capability_exception => "stacked-capability-throw",
    rollback_retained => "stacked-open-detach-fail", exception_rollback_retained => "stacked-capability-throw-detach-fail",
    enable_refused => "stacked-enable-fail", enable_exception => "stacked-enable-throw",
    disable_refused => "stacked-disable-fail", disable_exception => "stacked-disable-throw",
    detach_retry => "stacked-detach-fail", detach_exception_retry => "stacked-detach-throw",
    close_before_enable => "stacked-close-before-enable", drop_retained => "stacked-drop-detach-fail",
    pending_both => "stacked-basic", pending_drop => "stacked-drop-pending",
    abandoned_requests => "stacked-abandon-pending", deferred_retention => "stacked-deferred-detach-fail",
    return_fail_payload => "stacked-return-fail", independent_accounting => "accounting",
    send_exception => "stacked-keepalive-throw",
    common_before_and_after_enable => "common-attached-enabled",
}

#[test]
#[ignore = "subprocess entry; exercised by all parent scenarios"]
fn scenario_child() {
    let scenario = env::var(CHILD).unwrap();
    let log = Log::new();
    match scenario.as_str() {
        "stacked-config" | "stacked-zero" | "stacked-depth-zero" | "stacked-depth-max" => {
            lifecycle(&scenario, &log)
        }
        "stacked-not-advertised"
        | "stacked-empty"
        | "stacked-discovery-throw"
        | "stacked-attach-null"
        | "stacked-attach-throw"
        | "stacked-wrong-concrete"
        | "stacked-capability-wrong"
        | "stacked-capability-throw"
        | "stacked-open-detach-fail"
        | "stacked-capability-throw-detach-fail" => open_failure(&scenario, &log),
        "stacked-basic"
        | "stacked-drop-pending"
        | "stacked-abandon-pending"
        | "stacked-deferred-detach-fail"
        | "stacked-return-fail" => pending(&scenario, &log),
        "accounting" => accounting(&log),
        "common-attached-enabled" => common_attached_and_enabled(&log),
        "stacked-keepalive-throw" => {
            let session = Session::open(provider(), &scenario, "").unwrap();
            let mut stacked = session.open_stacked_channel(&config()).unwrap();
            let mut view = stacked.channel_view().unwrap();
            let error = view.send_keepalive().unwrap_err();
            assert_eq!(error.kind(), &ErrorKind::ProviderException);
            assert!(!error.diagnostic().unwrap().is_empty());
            stacked.close().unwrap();
            session.close().unwrap();
            log.finish(false);
        }
        _ => failure(&scenario, &log),
    }
    println!("STACKED_CHILD_COMPLETE {scenario}");
}
