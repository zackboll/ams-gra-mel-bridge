use super::*;
use CoordinateFrameReference::{Aircraft, Inertial};

const WAIT_MS: u32 = 5_000;
const SUCCESS: ReturnResult = ReturnResult::Completed {
    value: CommandReturn::Success,
};
const DISPATCH: &str = "scheduling_scan_task_exec";

fn fixture(index: usize) -> SchedulingScanTask {
    let (el, az) = [
        (Inertial, Aircraft),
        (Aircraft, Inertial),
        (Inertial, Inertial),
        (Aircraft, Aircraft),
    ][index];
    SchedulingScanTask {
        command_id: [u32::MAX, 0x8000_0000, 0, 17][index],
        parameters: ScanParameters {
            elevation_defined_with_range_and_altitude: [true, false, true, false][index],
            center_az_rad: -0.0,
            center_el_rad: 1.23456789012345,
            center_frame_ref_el: el,
            center_frame_ref_az: az,
            scan_width_rad: -2.34567890123456,
            scan_height_rad: 0.0,
            scan_type: ScanType {
                continuous_scan: u32::MAX,
                returning: 0x8000_0000,
                agile_scan: 2,
            },
            scan_id: 0xfedc_ba98,
            scan_rate_rad_per_second: -3.45678901234567,
            preferred_revisit_interval_seconds: if index == 2 {
                f64::INFINITY
            } else {
                4.56789012345678
            },
            required_revisit_interval_seconds: if index == 3 {
                f64::from_bits(0x7ff8_0000_0000_0123)
            } else {
                -5.67890123456789
            },
            max_range_of_interest_m: u32::MAX,
            min_range_of_interest_m: 0x8000_0001,
            elevation_scan_center_altitude_m: 0x8765_4321,
            elevation_scan_center_range_m: 0xf123_4567,
            degradation_method: [
                DegradationMethod::Capacity,
                DegradationMethod::Volume,
                DegradationMethod::Range,
                DegradationMethod::Revisit,
            ][index],
        },
        task_info: SchedulingTaskInfo {
            system_time_ns: [i64::MIN, i64::MAX, -42, 0][index],
            request_id: [0xf123_4567, u32::MAX, 0, 19][index],
            action: [
                TaskListAction::Normal,
                TaskListAction::Next,
                TaskListAction::Immediate,
                TaskListAction::Abort,
            ][index],
            precedence_within_priority: [u32::MAX, 0, 37, 0x8000_0002][index],
            priority: [0x8000_0001, u32::MAX, 0, 21][index],
        },
    }
}

// Both crate paths expose the same task, scan parameters and request types.
fn submit(
    channel: &mut SchedulingChannel,
    task: &ams_mel_ir::SchedulingScanTask,
) -> Result<ReturnRequest, Error> {
    let _: ams_mel_ir::ScanParameters = task.parameters;
    let request: ams_mel_ir::ReturnRequest = channel.exec_scan(task)?;
    Ok(request)
}

fn verify_values(pin: &Pin, task: &SchedulingScanTask, index: usize) {
    let mut actual = [0_u64; 26];
    // SAFETY: the mock writes 26 words; the pinned symbol has this signature.
    // The subprocess performs all submissions and reads serially.
    unsafe {
        let symbol = dlsym(pin.0, c"mock_scheduling_scan_values".as_ptr());
        assert!(!symbol.is_null());
        let read: unsafe extern "C" fn(*mut u64) = std::mem::transmute(symbol);
        read(actual.as_mut_ptr());
    }
    let p = task.parameters;
    let t = task.task_info;
    let (el, az) = [(0, 1), (1, 0), (0, 0), (1, 1)][index];
    assert_eq!(
        actual,
        [
            u64::from(task.command_id),
            u64::from(p.elevation_defined_with_range_and_altitude),
            p.center_az_rad.to_bits(),
            p.center_el_rad.to_bits(),
            el,
            az,
            p.scan_width_rad.to_bits(),
            p.scan_height_rad.to_bits(),
            u64::from(p.scan_type.continuous_scan),
            u64::from(p.scan_type.returning),
            u64::from(p.scan_type.agile_scan),
            u64::from(p.scan_id),
            p.scan_rate_rad_per_second.to_bits(),
            p.preferred_revisit_interval_seconds.to_bits(),
            p.required_revisit_interval_seconds.to_bits(),
            u64::from(p.max_range_of_interest_m),
            u64::from(p.min_range_of_interest_m),
            u64::from(p.elevation_scan_center_altitude_m),
            u64::from(p.elevation_scan_center_range_m),
            index as u64,
            t.system_time_ns as u64,
            u64::from(t.request_id),
            index as u64,
            u64::from(t.precedence_within_priority),
            u64::from(t.priority),
            0,
        ]
    );
}

fn check_dispatch(log: &Log, pin: &Pin, count: u32) {
    assert_eq!(pin.count(c"mock_scheduling_scan_calls"), count);
    log.check(DISPATCH, count as usize);
    log.check("keepalive_sent", 0);
    log.check("comms_sent", 0);
    log.no_unexpected_scheduling_operations(Some(DISPATCH));
}

fn check_error(error: Error, kind: ErrorKind, diagnostic: &str) {
    assert_eq!(error.kind(), &kind);
    assert_eq!(error.diagnostic(), Some(diagnostic));
}

fn check_exhausted(error: Error) {
    check_error(
        error,
        ErrorKind::ResourceExhausted,
        "async request limit reached",
    );
}

fn terminal(case: &str, submission: Result<ReturnRequest, Error>) {
    match case {
        "exec-throw" => check_error(
            submission.unwrap_err(),
            ErrorKind::ProviderException,
            "mock ScanTask exec exception",
        ),
        "invalid-future" => check_error(
            submission.unwrap_err(),
            ErrorKind::ProviderFailed,
            "provider returned an invalid common request future",
        ),
        _ => {
            let mut request = submission.unwrap();
            let result = request.wait(WAIT_MS);
            assert_eq!(request.wait(0), result);
            match case {
                "unsupported" | "invalid-state" => assert_eq!(
                    result.unwrap(),
                    ReturnResult::Rejected {
                        code: if case == "unsupported" {
                            MelErrorCode::Unsupported
                        } else {
                            MelErrorCode::InvalidState
                        },
                        description: format!("ScanTask rejection: {}", "x".repeat(1024)),
                    }
                ),
                "future-throw" => check_error(
                    result.unwrap_err(),
                    ErrorKind::ProviderException,
                    "mock ScanTask future exception",
                ),
                "null" | "malformed" => check_error(
                    result.unwrap_err(),
                    ErrorKind::ProviderFailed,
                    "provider returned null or unowned successful Return result",
                ),
                "invalid-return" => check_error(
                    result.unwrap_err(),
                    ErrorKind::ProviderFailed,
                    "provider returned unknown IR Return value",
                ),
                _ => assert_eq!(
                    result.unwrap(),
                    ReturnResult::Completed {
                        value: match case {
                            "success" | "reentry" => CommandReturn::Success,
                            "fail" => CommandReturn::Fail,
                            "not-supported" => CommandReturn::NotSupported,
                            "not-implemented" => CommandReturn::NotImplemented,
                            _ => unreachable!("{case}"),
                        },
                    }
                ),
            }
            request.close().unwrap();
        }
    }
}

fn pending(case: &str, log: &Log, pin: Pin, session: Session, mut channel: SchedulingChannel) {
    let task = fixture(0);
    let mut view = channel.channel_view().unwrap();
    let request = if let Some(failpoint) = case.strip_prefix("post-") {
        env::set_var("AMS_MEL_TEST_COMMON_POST_SEND_FAILURE", failpoint);
        check_error(
            submit(&mut channel, &task).unwrap_err(),
            ErrorKind::InternalError,
            "injected publication failure; completion worker owns future",
        );
        None
    } else {
        let mut request = submit(&mut channel, &task).unwrap();
        for timeout in [0, 5, 0] {
            assert_eq!(
                request.wait(timeout).unwrap_err().kind(),
                &ErrorKind::Timeout
            );
        }
        check_exhausted(submit(&mut channel, &task).unwrap_err());
        match case {
            "observer-close" => {
                request.close().unwrap();
                None
            }
            "observer-drop" => {
                drop(request);
                None
            }
            _ => Some(request),
        }
    };
    // Closing the observer (or failing to publish it) did not retire admission.
    check_exhausted(view.send_keepalive().unwrap_err());
    verify_values(&pin, &task, 0);
    check_dispatch(log, &pin, 1);
    if case == "channel-first" {
        channel.close().unwrap();
        session.close().unwrap();
    } else {
        session.close().unwrap();
        channel.close().unwrap();
    }
    check_error(
        submit(&mut channel, &task).unwrap_err(),
        ErrorKind::ProviderFailed,
        "IR Scheduling channel is closed",
    );
    assert_eq!(
        view.capabilities().unwrap_err().kind(),
        &ErrorKind::ProviderFailed
    );
    view.close().unwrap();
    drop(pin);
    for event in [
        "scheduling_disabled",
        "channel_detached",
        "library_unloaded",
    ] {
        log.check(event, 0);
    }
    let pin = Pin::loaded(); // RTLD_NOLOAD: only the admitted graph kept the DSO resident.
    pin.release(17); // ScanTask mock future family.
    let detach_failed = case == "detach-fail";
    if let Some(mut request) = request {
        let result = request.wait(WAIT_MS);
        assert_eq!(request.wait(0), result);
        if detach_failed {
            check_error(
                result.unwrap_err(),
                ErrorKind::ProviderFailed,
                "deferred Scheduling cleanup failed",
            );
        } else {
            assert_eq!(result.unwrap(), SUCCESS);
        }
        request.close().unwrap();
    }
    wait_for_return_cleanup();
    check_dispatch(log, &pin, 1);
    log.check("scheduling_enabled", 1);
    drop(pin);
    if detach_failed {
        log.check("channel_detach_failed", 1);
        log.check("scheduling_disabled", 1);
        log.check("channel_detached", 0);
        log.check("scheduling_channel_destroyed", 0);
        log.check("library_unloaded", 0);
        drop(Pin::loaded());
    } else {
        log.check_cleanup_order(true);
    }
}

pub(super) fn run(scenario: &str, log: &Log) {
    let case = scenario.strip_prefix("scan-").unwrap();
    let held = matches!(
        case,
        "channel-first" | "session-first" | "observer-close" | "observer-drop" | "detach-fail"
    ) || case.starts_with("post-");
    let instance = match case {
        "admission-states" => "sched-enable-fail".into(),
        "reentry" => "sched-scan-success".into(),
        _ if held && case != "detach-fail" => "sched-scan-delayed".into(),
        _ => format!("sched-{scenario}"),
    };
    let session = Session::open_with_options(
        provider(),
        &instance,
        "",
        SessionOptions {
            max_async_requests: 1,
        },
    )
    .unwrap();
    let pin = Pin::loaded();
    let mut channel = session.open_scheduling_channel(&config()).unwrap();
    let task = fixture(0);
    check_error(
        submit(&mut channel, &task).unwrap_err(),
        ErrorKind::ProviderFailed,
        "common channel is not available",
    );
    check_dispatch(log, &pin, 0);
    log.check("scheduling_enabled", 0);

    let calls = if case == "admission-states" {
        assert_eq!(
            channel.enable().unwrap_err().kind(),
            &ErrorKind::ProviderFailed
        );
        check_error(
            submit(&mut channel, &task).unwrap_err(),
            ErrorKind::ProviderFailed,
            "common channel is not available",
        );
        0
    } else {
        channel.enable().unwrap();
        if held {
            pending(case, log, pin, session, channel);
            return;
        }
        if case == "reentry" {
            arm_reentry(
                &pin,
                c"mock_scheduling_scan_reentry",
                channel.channel_view().unwrap(),
            );
        }
        let count = if case == "success" { 4 } else { 1 };
        for index in 0..count {
            let task = fixture(index);
            terminal(case, submit(&mut channel, &task));
            verify_values(&pin, &task, index);
            check_dispatch(log, &pin, index as u32 + 1);
            wait_for_return_cleanup();
        }
        if case == "reentry" {
            finish_reentry();
        }
        count as u32
    };
    channel.close().unwrap();
    check_error(
        submit(&mut channel, &task).unwrap_err(),
        ErrorKind::ProviderFailed,
        "IR Scheduling channel is closed",
    );
    session.close().unwrap();
    check_dispatch(log, &pin, calls);
    drop(pin);
    log.check("scheduling_enabled", 1);
    log.check_cleanup_order(true);
}

scenarios! {
    scan_refuses_attached_failed_and_closed => "scan-admission-states",
    scan_provider_receives_exact_nested_values => "scan-success",
    scan_completed_fail => "scan-fail",
    scan_completed_not_supported => "scan-not-supported",
    scan_completed_not_implemented => "scan-not-implemented",
    scan_rejected_unsupported => "scan-unsupported",
    scan_rejected_invalid_state => "scan-invalid-state",
    scan_exec_exception => "scan-exec-throw",
    scan_future_exception => "scan-future-throw",
    scan_invalid_future_before_publication => "scan-invalid-future",
    scan_null_result => "scan-null",
    scan_malformed_result => "scan-malformed",
    scan_unknown_return => "scan-invalid-return",
    scan_reentrant_same_channel_capabilities => "scan-reentry",
    scan_survives_channel_first_close => "scan-channel-first",
    scan_survives_session_first_close => "scan-session-first",
    scan_observer_close_does_not_cancel => "scan-observer-close",
    scan_observer_drop_does_not_cancel => "scan-observer-drop",
    scan_post_send_allocation_failure_is_not_retried => "scan-post-allocation",
    scan_post_send_worker_launch_failure_is_not_retried => "scan-post-worker-launch",
    scan_deferred_detach_failure_retains_native_graph => "scan-detach-fail",
}
