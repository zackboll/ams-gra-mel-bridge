//! Compiled-mock contracts. Each scenario isolates logging and retained graphs.
use ams_mel::*;
use std::{
    cell::RefCell,
    env,
    ffi::{c_char, c_void, CString},
    fs,
    path::{Path, PathBuf},
    process::Command,
};

const SCENARIO: &str = "AMS_MEL_RUST_SCHEDULING_CHILD";
const IDS: CommsTestRequest = CommsTestRequest {
    command_id: 0x8000_0001,
    channel_id: 0xf000_0002,
    request_id: 0xe000_0003,
};

unsafe extern "C" {
    fn dlopen(path: *const c_char, flags: i32) -> *mut c_void;
    fn dlsym(handle: *mut c_void, name: *const c_char) -> *mut c_void;
    fn dlclose(handle: *mut c_void) -> i32;
    fn ams_mel_test_completion_boundary(family: u32, op: u32, target: u64, out: *mut u64) -> i32;
    fn ams_mel_test_completion_owner(family: u32, target: u64, out: *mut u64) -> i32;
    fn ams_mel_test_completion_owner_notify(
        family: u32,
        op: u32,
        target: u64,
        out: *mut u64,
    ) -> i32;
}

fn provider() -> PathBuf {
    env::var_os("AMS_MEL_TEST_PROVIDER_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| {
            Path::new(env!("CARGO_MANIFEST_DIR")).join("../../native/build-tests/test-providers")
        })
        .join("libmock_ir_provider.so")
}

struct Pin(*mut c_void);
impl Pin {
    fn loaded() -> Self {
        let path = CString::new(provider().as_os_str().as_encoded_bytes()).unwrap();
        // SAFETY: NUL-terminated path; Linux RTLD_NOW | RTLD_NOLOAD never loads a new DSO.
        let raw = unsafe { dlopen(path.as_ptr(), 2 | 4) };
        assert!(!raw.is_null(), "native graph must keep the mock loaded");
        Self(raw)
    }

    fn release(&self, family: u32) {
        // SAFETY: exact mock-only symbol/signature and a live dlopen reference.
        unsafe {
            let symbol = dlsym(self.0, c"mock_completion_release_family".as_ptr());
            assert!(!symbol.is_null());
            let release: unsafe extern "C" fn(u32, u32) -> i32 = std::mem::transmute(symbol);
            assert_eq!(release(family, 1), 1);
        }
    }

    fn no_task_calls(&self) {
        for name in [
            c"mock_scheduling_specific_calls",
            c"mock_scheduling_ack_registrations",
            c"mock_scheduling_rep_registrations",
            c"mock_scheduling_abort_calls",
            c"mock_scheduling_scan_calls",
            c"mock_scheduling_stare_calls",
            c"mock_scheduling_track_calls",
        ] {
            assert_eq!(self.count(name), 0, "{name:?}");
        }
    }

    fn count(&self, name: &std::ffi::CStr) -> u32 {
        // SAFETY: callers use the exact names of mock-only unsigned counters.
        unsafe {
            let symbol = dlsym(self.0, name.as_ptr());
            assert!(!symbol.is_null(), "{name:?}");
            let count: unsafe extern "C" fn() -> u32 = std::mem::transmute(symbol);
            count()
        }
    }
}
impl Drop for Pin {
    fn drop(&mut self) {
        // SAFETY: one test-only loader reference; never a facade owner.
        let _ = unsafe { dlclose(self.0) };
    }
}

fn wait_for_return_cleanup() {
    let mut observed = 0;
    // SAFETY: test-only handshake, Return family, drain all owner notifications.
    assert_eq!(
        unsafe { ams_mel_test_completion_owner_notify(1, 0, 0, &mut observed) },
        1
    );
}

thread_local! {
    static REENTRY: RefCell<Option<(ChannelView, bool)>> = const { RefCell::new(None) };
}

extern "C" fn reenter_capabilities() {
    // Synchronous on the submitting thread, through an independent safe view.
    REENTRY.with_borrow_mut(|slot| {
        if let Some((view, succeeded)) = slot {
            *succeeded = view.capabilities().is_ok();
        }
    });
}

fn arm_reentry(pin: &Pin, name: &std::ffi::CStr, view: ChannelView) {
    REENTRY.set(Some((view, false)));
    // SAFETY: callers name the mock's one-shot reentry setters; the DSO is pinned.
    unsafe {
        let symbol = dlsym(pin.0, name.as_ptr());
        assert!(!symbol.is_null());
        let set: unsafe extern "C" fn(extern "C" fn()) = std::mem::transmute(symbol);
        set(reenter_capabilities);
    }
}

fn finish_reentry() {
    let (mut view, succeeded) = REENTRY.take().unwrap();
    assert!(succeeded);
    view.close().unwrap();
}

fn wait_for_completion_owners() {
    for family in [1, 2] {
        let mut counts = [0; 4];
        let mut observed = 0;
        // SAFETY: exact test-only ABI; slots have the documented sizes.
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

struct Log(PathBuf);
impl Log {
    fn new() -> Self {
        let directory = env::temp_dir().join(format!("ams-rust-scheduling-{}", std::process::id()));
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
    fn check(&self, event: &str, expected: usize) {
        assert_eq!(self.count(event), expected, "{event}:\n{}", self.text());
    }
    fn finish(&self, retained: bool) {
        self.check("library_unloaded", usize::from(!retained));
        self.no_unexpected_scheduling_operations(None);
    }

    fn no_unexpected_scheduling_operations(&self, allowed_dispatch: Option<&str>) {
        for line in self
            .text()
            .lines()
            .filter(|line| line.starts_with("scheduling_"))
        {
            if Some(line) == allowed_dispatch {
                continue;
            }
            assert!(
                !["_exec", "_sent", "_registered", "_registration_attempted"]
                    .iter()
                    .any(|suffix| line.ends_with(suffix)),
                "unexpected task/metadata operation: {line}"
            );
        }
    }
    fn cleaned_up(&self, enabled: bool) {
        self.check_cleanup_order(enabled);
        self.finish(false);
    }

    fn check_cleanup_order(&self, enabled: bool) {
        self.check("scheduling_disabled", usize::from(enabled));
        let text = self.text();
        let mut lines = text.lines();
        if enabled {
            assert!(lines.any(|line| line == "scheduling_disabled"), "{text}");
        }
        for event in [
            "channel_detached",
            "scheduling_channel_destroyed",
            "control_destroyed",
            "manager_destroyed",
            "library_unloaded",
        ] {
            self.check(event, 1);
            assert!(
                lines.any(|line| line == event),
                "missing or out of order: {event}\n{text}"
            );
        }
    }
}
impl Drop for Log {
    fn drop(&mut self) {
        let _ = fs::remove_dir_all(&self.0);
    }
}

fn config() -> ams_mel_ir::SchedulingConfig {
    SchedulingConfig::new(
        UciId::new(
            std::array::from_fn(|i| 0xa0 + i as u8),
            "IR scheduling μ channel",
        )
        .unwrap(),
        UciId::new(std::array::from_fn(|i| 0xf0 + i as u8), "test platform").unwrap(),
        ComponentLocation::new(1.25, -2.5, 3.75, "station-1", "mock-aircraft").unwrap(),
    )
}

fn assert_profile(value: ChannelCapability, depth: u32) {
    use MetadataCapability as M;
    assert_eq!(
        value,
        ChannelCapability {
            channel_id: UciId::new(std::array::from_fn(|i| 0x11 + (i * 7) as u8), "channel-α")
                .unwrap(),
            platform_id: UciId::new(std::array::from_fn(|i| 0x31 + (i * 7) as u8), "platform-€")
                .unwrap(),
            height: 1080,
            width: 1920,
            bit_depth: 12,
            row_pitch: 4096,
            buffer_size: 8388608,
            image_size: 4147200,
            number_of_bands: 3,
            pixel_format: PixelFormat::Rgb,
            sensor_types: vec![SensorType::GimbalHorizontal, SensorType::StepStare],
            sensor_location: ComponentLocation::new(1.25, -2.5, 3.75, "sensor-key", "system-β")
                .unwrap(),
            channel_types: vec![ChannelType::Scheduling, ChannelType::Reserved1],
            task_schedule_depth: depth,
            odc_available: true,
            nuc_available: true,
            metadata_capabilities: vec![
                M::TaskExecutingRep,
                M::ExecuteTaskAck,
                M::SchedCreatedRep,
                M::ChannelCommsTestRep,
                M::UpdateTrackListResponse,
                M::TaskEvents,
                M::ScanPerformanceReport
            ],
            image_bands: vec![
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
                    },]
                }
            ],
            nav_frames: vec![CoordinateSystem::NedSensor, CoordinateSystem::Ecef],
        }
    );
}

fn lifecycle(scenario: &str, log: &Log) {
    let session = Session::open(provider(), scenario, "").unwrap();
    let pin = Pin::loaded();
    let config: SchedulingConfig = config();
    let mut channel: ams_mel_ir::SchedulingChannel =
        session.open_scheduling_channel(&config).unwrap();
    drop(config);
    log.check("scheduling_attach_requested", 1);
    log.check(
        "scheduling_config_verified",
        usize::from(scenario == "sched-config"),
    );
    log.check("scheduling_enabled", 0);
    let attached = channel.capabilities().unwrap();
    let mut view = channel.channel_view().unwrap();
    channel.channel_view().unwrap().close().unwrap();
    pin.no_task_calls();
    drop(pin);
    if scenario == "sched-depth-max" {
        drop(session);
    } else {
        session.close().unwrap();
    }
    log.check("library_unloaded", 0);
    assert!(channel.is_open());
    assert_eq!(view.capabilities().unwrap(), attached);
    let enable = scenario == "sched-config";
    if enable {
        channel.enable().unwrap();
        channel.enable().unwrap();
        log.check("scheduling_enabled", 1);
        assert_eq!(channel.capabilities().unwrap(), attached);
    }
    channel.close().unwrap();
    channel.close().unwrap();
    assert!(!channel.is_open());
    assert_eq!(
        channel.enable().unwrap_err().kind(),
        &ErrorKind::ProviderFailed
    );
    assert!(channel.capabilities().is_err());
    assert!(channel.channel_view().is_err());
    drop(channel);
    log.cleaned_up(enable); // The view is still alive but does not retain the DSO.
    assert_eq!(
        view.capabilities().unwrap_err().kind(),
        &ErrorKind::ProviderFailed
    );
    assert_eq!(
        view.send_keepalive().unwrap_err().kind(),
        &ErrorKind::ProviderFailed
    );
    assert_eq!(
        view.submit_comms_test(IDS).unwrap_err().kind(),
        &ErrorKind::ProviderFailed
    );
    view.close().unwrap();
    assert_profile(
        attached,
        match scenario {
            "sched-depth-zero" => 0,
            "sched-depth-max" => u32::MAX,
            _ => 23,
        },
    );
}

fn open_failure(scenario: &str, log: &Log) {
    let session = Session::open(provider(), scenario, "").unwrap();
    let pin = Pin::loaded();
    let (kind, diagnostic, attaches, detached, retained) = match scenario {
        "sched-not-advertised" => (
            ErrorKind::InitializationFailed,
            "provider does not advertise Scheduling",
            0,
            0,
            false,
        ),
        "sched-wrong-concrete" => (
            ErrorKind::InitializationFailed,
            "attached channel is not a SchedulingChannel",
            1,
            1,
            false,
        ),
        "sched-open-detach-fail" => (
            ErrorKind::ProviderFailed,
            "incompatible Scheduling channel and detach failed",
            1,
            0,
            true,
        ),
        _ => unreachable!(),
    };
    let err = session.open_scheduling_channel(&config()).unwrap_err();
    assert_eq!(err.kind(), &kind);
    assert_eq!(err.diagnostic(), Some(diagnostic));
    log.check("scheduling_attach_requested", attaches);
    log.check("channel_detached", detached);
    log.check("scheduling_enabled", 0);
    assert!(!session.provider_version().unwrap().vendor.is_empty());
    if scenario == "sched-not-advertised" {
        session
            .open_control_channel(&ControlConfig::new(
                UciId::new([0; 16], "").unwrap(),
                UciId::new([0; 16], "").unwrap(),
                ComponentLocation::new(0., 0., 0., "", "").unwrap(),
            ))
            .unwrap()
            .close()
            .unwrap();
    }
    pin.no_task_calls();
    drop(pin);
    session.close().unwrap();
    log.finish(retained);
}

fn enable_or_close_failure(scenario: &str, log: &Log) {
    let native = if scenario == "drop-detach-fail" {
        "sched-enabled-detach-fail"
    } else {
        scenario
    };
    let session = Session::open(provider(), native, "").unwrap();
    let pin = Pin::loaded();
    let mut channel = session.open_scheduling_channel(&config()).unwrap();
    if scenario.starts_with("sched-enable-") {
        let exception = scenario.ends_with("throw");
        let err = channel.enable().unwrap_err();
        assert_eq!(
            err.kind(),
            &if exception {
                ErrorKind::ProviderException
            } else {
                ErrorKind::ProviderFailed
            }
        );
        assert_eq!(
            err.diagnostic(),
            Some(if exception {
                "provider exception during Scheduling enable"
            } else {
                "Scheduling enable failed"
            })
        );
        assert_eq!(
            channel.enable().unwrap_err().diagnostic(),
            Some("Scheduling channel is not attached")
        );
        assert_eq!(
            channel.capabilities().unwrap_err().kind(),
            &ErrorKind::ProviderFailed
        );
    } else {
        channel.enable().unwrap();
    }
    log.check("scheduling_enabled", 1);
    pin.no_task_calls();
    drop(pin);
    session.close().unwrap();
    if scenario == "drop-detach-fail" {
        drop(channel);
        log.check("channel_detach_failed", 1);
        log.check("scheduling_disabled", 1);
        log.check("scheduling_channel_destroyed", 0);
        log.finish(true);
        return;
    }
    if scenario == "sched-enabled-detach-fail" || scenario == "sched-disable-fail" {
        let retained = scenario == "sched-enabled-detach-fail";
        let err = channel.close().unwrap_err();
        assert_eq!(err.kind(), &ErrorKind::ProviderFailed);
        assert_eq!(
            err.diagnostic(),
            Some(if retained {
                "Scheduling detach failed; provider state retained"
            } else {
                "Scheduling disable failed"
            })
        );
        assert_eq!(channel.is_open(), retained);
        log.check("library_unloaded", usize::from(!retained));
        if retained {
            assert!(channel.enable().is_err());
            assert!(channel.capabilities().is_err());
            log.check("channel_detach_failed", 1);
        }
    }
    channel.close().unwrap();
    drop(channel);
    log.check("scheduling_enabled", 1);
    log.cleaned_up(true);
}

fn pending(scenario: &str, log: &Log) {
    let failed_detach = scenario == "deferred-detach-fail";
    let session = Session::open_with_options(
        provider(),
        if failed_detach {
            "sched-enabled-detach-fail"
        } else {
            "sched-basic"
        },
        "",
        SessionOptions {
            max_async_requests: 2,
        },
    )
    .unwrap();
    let mut channel = session.open_scheduling_channel(&config()).unwrap();
    let mut view = channel.channel_view().unwrap();
    // KeepAlive works while Attached, CommsTest while Enabled.
    let mut keepalive = view.send_keepalive().unwrap();
    channel.enable().unwrap();
    let mut comms = view.submit_comms_test(IDS).unwrap();
    assert_eq!(keepalive.wait(0).unwrap_err().kind(), &ErrorKind::Timeout);
    assert_eq!(comms.wait(0).unwrap_err().kind(), &ErrorKind::Timeout);
    assert_eq!(
        view.send_keepalive().unwrap_err().kind(),
        &ErrorKind::ResourceExhausted
    );
    assert_eq!(
        view.submit_comms_test(IDS).unwrap_err().kind(),
        &ErrorKind::ResourceExhausted
    );
    if scenario == "drop-pending" {
        drop(session);
        assert!(channel.capabilities().is_ok());
        drop(channel);
    } else {
        session.close().unwrap();
        assert!(channel.capabilities().is_ok());
        channel.close().unwrap();
        assert!(!channel.is_open());
    }
    assert_eq!(
        view.capabilities().unwrap_err().kind(),
        &ErrorKind::ProviderFailed
    );
    view.close().unwrap();
    log.check("scheduling_disabled", 0);
    log.check("channel_detached", 0);
    let pin = Pin::loaded(); // Requests alone were sufficient before taking this pin.
    if scenario == "abandon-pending" {
        keepalive.close().unwrap();
        drop(comms);
        log.check("scheduling_disabled", 0);
        pin.release(2);
        pin.release(1);
    } else {
        pin.release(2);
        let report = CommsTestResult::Completed {
            report: CommsTestReport {
                command_id: IDS.command_id,
                request_id: IDS.request_id,
            },
        };
        assert_eq!(comms.wait(15_000).unwrap(), report);
        assert_eq!(comms.wait(0).unwrap(), report);
        assert_eq!(keepalive.wait(0).unwrap_err().kind(), &ErrorKind::Timeout);
        log.check("channel_detached", 0);
        pin.release(1);
        if failed_detach {
            assert_eq!(
                keepalive.wait(15_000).unwrap_err().kind(),
                &ErrorKind::ProviderFailed
            );
        } else {
            let success = ReturnResult::Completed {
                value: CommandReturn::Success,
            };
            assert_eq!(keepalive.wait(15_000).unwrap(), success);
            assert_eq!(keepalive.wait(0).unwrap(), success);
        }
        keepalive.close().unwrap();
        comms.close().unwrap();
    }
    wait_for_completion_owners();
    pin.no_task_calls();
    drop(pin);
    log.check("keepalive_sent", 1);
    log.check("comms_sent", 1);
    if failed_detach {
        log.check("channel_detach_failed", 1);
        log.check("scheduling_disabled", 1);
        log.check("channel_detached", 0);
        log.check("scheduling_channel_destroyed", 0);
        log.finish(true);
    } else {
        log.cleaned_up(true);
    }
}

fn invalid_config(log: &Log) {
    let session = Session::open(provider(), "sched-config", "").unwrap();
    let pin = Pin::loaded();
    for label in ["channel\0invalid", "platform\0invalid"] {
        assert_eq!(
            UciId::new([0; 16], label).unwrap_err().kind(),
            &ErrorKind::InvalidArgument
        );
    }
    for (key, system) in [("key\0invalid", "system"), ("key", "system\0invalid")] {
        assert_eq!(
            ComponentLocation::new(0., 0., 0., key, system)
                .unwrap_err()
                .kind(),
            &ErrorKind::InvalidArgument
        );
    }
    log.check("scheduling_attach_requested", 0);
    let channel = session.open_scheduling_channel(&config()).unwrap();
    log.check("scheduling_attach_requested", 1);
    pin.no_task_calls();
    drop(pin);
    drop(channel);
    session.close().unwrap();
    log.cleaned_up(false);
}

macro_rules! scenarios {
    ($($name:ident => $scenario:literal),+ $(,)?) => {$(
        #[test]
        fn $name() {
            // Bound regressions that would otherwise deadlock in provider reentry.
            let child = Command::new("timeout")
                .args(["--kill-after=2s", "45s"])
                .arg(env::current_exe().unwrap())
                .args(["--exact", "scenario_child", "--ignored", "--nocapture"])
                .env(SCENARIO, $scenario)
                .output().unwrap();
            let stdout = String::from_utf8_lossy(&child.stdout);
            let stderr = String::from_utf8_lossy(&child.stderr);
            assert!(child.status.success() && stdout.contains("SCHEDULING_CHILD_COMPLETE"), "{}\n{stdout}\n{stderr}", child.status);
        }
    )+};
}

scenarios! {
    invalid_identity_or_location => "invalid-config",
    attachment_enable_and_capabilities => "sched-config",
    depth_zero_close_before_enable => "sched-depth-zero",
    depth_max_after_session_drop => "sched-depth-max",
    missing_advertisement_nonfatal => "sched-not-advertised",
    wrong_concrete_attachment => "sched-wrong-concrete",
    retained_open_rollback => "sched-open-detach-fail",
    enable_refusal => "sched-enable-fail",
    enable_exception => "sched-enable-throw",
    detach_failure_then_retry => "sched-enabled-detach-fail",
    disable_failure_consumes_owner => "sched-disable-fail",
    drop_attempts_failed_detach_once => "drop-detach-fail",
    close_with_admitted_requests => "close-pending",
    drop_with_admitted_requests => "drop-pending",
    close_request_does_not_cancel => "abandon-pending",
    deferred_detach_failure_retains_native_graph => "deferred-detach-fail",
}

#[path = "ir_scheduling/abort.rs"]
mod abort;
#[path = "ir_scheduling/stare.rs"]
mod stare;

#[test]
#[ignore = "subprocess entry; exercised by the compiled-mock scenarios"]
fn scenario_child() {
    let scenario = env::var(SCENARIO).unwrap();
    let log = Log::new();
    match scenario.as_str() {
        "invalid-config" => invalid_config(&log),
        "sched-config" | "sched-depth-zero" | "sched-depth-max" => lifecycle(&scenario, &log),
        "sched-not-advertised" | "sched-wrong-concrete" | "sched-open-detach-fail" => {
            open_failure(&scenario, &log)
        }
        "close-pending" | "drop-pending" | "abandon-pending" | "deferred-detach-fail" => {
            pending(&scenario, &log)
        }
        scenario if scenario.starts_with("abort-") => abort::run(scenario, &log),
        scenario if scenario.starts_with("stare-") => stare::run(scenario, &log),
        _ => enable_or_close_failure(&scenario, &log),
    }
    println!("SCHEDULING_CHILD_COMPLETE {scenario}");
}
