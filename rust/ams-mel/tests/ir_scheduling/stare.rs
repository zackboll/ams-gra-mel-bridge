use super::*;
use CoordinateFrameReference::{Aircraft, Inertial};
use TaskListAction::{Abort, Immediate, Next, Normal};

const TASK: SchedulingStareTask = SchedulingStareTask {
    command_id: u32::MAX,
    parameters: SchedulingStareParameters {
        center_azimuth_rad: -0.0,
        center_elevation_rad: 0.0,
        elevation_frame: Inertial,
        azimuth_frame: Aircraft,
        duration_ns: i64::MIN,
    },
    task_info: SchedulingTaskInfo {
        system_time_ns: i64::MAX,
        request_id: 0xf123_4567,
        action: Normal,
        precedence_within_priority: u32::MAX,
        priority: 0x8000_0001,
    },
};
const SUCCESS: ReturnResult = ReturnResult::Completed {
    value: CommandReturn::Success,
};
const WAIT_MS: u32 = 5_000;
const DISPATCH: &str = "scheduling_stare_task_exec";

// The compatibility crate exports these concrete types, not a second wrapper.
fn submit(
    channel: &mut SchedulingChannel,
    task: &ams_mel_ir::SchedulingStareTask,
) -> Result<ams_mel_ir::ReturnRequest, Error> {
    let _: ams_mel_ir::SchedulingStareParameters = task.parameters;
    channel.exec_stare(task)
}

fn assert_dispatch(log: &Log, pin: &Pin, count: u32) {
    assert_eq!(pin.count(c"mock_scheduling_stare_calls"), count);
    for symbol in [
        c"mock_scheduling_specific_calls",
        c"mock_scheduling_ack_registrations",
        c"mock_scheduling_rep_registrations",
        c"mock_scheduling_abort_calls",
        c"mock_scheduling_scan_calls",
        c"mock_scheduling_track_calls",
    ] {
        assert_eq!(pin.count(symbol), 0, "{symbol:?}");
    }
    log.check(DISPATCH, count as usize);
    log.check("keepalive_sent", 0);
    log.check("comms_sent", 0);
    log.no_unexpected_scheduling_operations(Some(DISPATCH));
}

fn verify_values(pin: &Pin, task: &SchedulingStareTask, frames: (u64, u64), action: u64) {
    let mut actual = [0_u64; 11];
    // SAFETY: exact test-only signature; the fixture writes eleven words and
    // no other submission can race the inspection in this child process.
    unsafe {
        let symbol = dlsym(pin.0, c"mock_scheduling_stare_values".as_ptr());
        assert!(!symbol.is_null());
        let values: unsafe extern "C" fn(*mut u64) = std::mem::transmute(symbol);
        values(actual.as_mut_ptr());
    }
    let p = task.parameters;
    let info = task.task_info;
    assert_eq!(
        actual,
        [
            u64::from(task.command_id),
            p.center_azimuth_rad.to_bits(),
            p.center_elevation_rad.to_bits(),
            frames.0,
            frames.1,
            p.duration_ns as u64,
            info.system_time_ns as u64,
            u64::from(info.request_id),
            action,
            u64::from(info.precedence_within_priority),
            u64::from(info.priority),
        ]
    );
}

fn exact_values(channel: &mut SchedulingChannel, log: &Log, pin: &Pin) {
    for (index, (el, az, duration, action, azimuth, elevation)) in [
        (Inertial, Aircraft, i64::MIN, Normal, -0.0, 0.0),
        (Aircraft, Inertial, i64::MAX, Next, 0.0, -0.0),
        (Inertial, Inertial, -1, Immediate, -1.23456789012345, 2.75),
        (Aircraft, Aircraft, 0, Abort, 1.23456789012345, -2.75),
    ]
    .into_iter()
    .enumerate()
    {
        let task = SchedulingStareTask {
            command_id: [u32::MAX, 0x8000_0000, 0, 17][index],
            parameters: SchedulingStareParameters {
                center_azimuth_rad: azimuth,
                center_elevation_rad: elevation,
                elevation_frame: el,
                azimuth_frame: az,
                duration_ns: duration,
            },
            task_info: SchedulingTaskInfo {
                system_time_ns: [i64::MAX, i64::MIN, -42, 0][index],
                request_id: [0xf123_4567, u32::MAX, 0x8000_0000, 17][index],
                action,
                precedence_within_priority: [u32::MAX, 0, 37, 0x8000_0002][index],
                priority: [0x8000_0001, u32::MAX, 0, 19][index],
            },
        };
        let mut request = submit(channel, &task).unwrap();
        assert_dispatch(log, pin, index as u32 + 1);
        verify_values(
            pin,
            &task,
            [(0, 1), (1, 0), (0, 0), (1, 1)][index],
            index as u64,
        );
        assert_eq!(request.wait(WAIT_MS).unwrap(), SUCCESS);
        assert_eq!(request.wait(0).unwrap(), SUCCESS);
        request.close().unwrap();
        // Cached terminal Wait may precede retirement of the Session permit.
        wait_for_return_cleanup();
    }
}

fn assert_error(error: Error, kind: ErrorKind, diagnostic: &str) {
    assert_eq!(error.kind(), &kind);
    assert_eq!(error.diagnostic(), Some(diagnostic));
}

fn terminal(case: &str, channel: &mut SchedulingChannel) {
    let submission = submit(channel, &TASK);
    match case {
        "exec-throw" => assert_error(
            submission.unwrap_err(),
            ErrorKind::ProviderException,
            "mock StareTask exec exception",
        ),
        "invalid-future" => assert_error(
            submission.unwrap_err(),
            ErrorKind::ProviderFailed,
            "provider returned an invalid common request future",
        ),
        _ => {
            let mut request = submission.unwrap();
            let outcome = request.wait(WAIT_MS);
            assert_eq!(request.wait(0), outcome);
            match case {
                "unsupported" | "invalid-state" => assert_eq!(
                    outcome.unwrap(),
                    ReturnResult::Rejected {
                        code: if case == "unsupported" {
                            MelErrorCode::Unsupported
                        } else {
                            MelErrorCode::InvalidState
                        },
                        description: format!("StareTask rejection: {}", "x".repeat(1024)),
                    }
                ),
                "future-throw" => assert_error(
                    outcome.unwrap_err(),
                    ErrorKind::ProviderException,
                    "mock StareTask future exception",
                ),
                "null" | "malformed" => assert_error(
                    outcome.unwrap_err(),
                    ErrorKind::ProviderFailed,
                    "provider returned null or unowned successful Return result",
                ),
                "invalid-return" => assert_error(
                    outcome.unwrap_err(),
                    ErrorKind::ProviderFailed,
                    "provider returned unknown IR Return value",
                ),
                _ => {
                    let value = match case {
                        "reentry" => CommandReturn::Success,
                        "fail" => CommandReturn::Fail,
                        "not-supported" => CommandReturn::NotSupported,
                        "not-implemented" => CommandReturn::NotImplemented,
                        _ => unreachable!("unexpected case: {case}"),
                    };
                    assert_eq!(outcome.unwrap(), ReturnResult::Completed { value });
                }
            }
            request.close().unwrap();
        }
    }
}

fn assert_exhausted(error: Error) {
    assert_error(
        error,
        ErrorKind::ResourceExhausted,
        "async request limit reached",
    );
}

fn pending_stare(
    case: &str,
    log: &Log,
    pin: Pin,
    session: Session,
    mut channel: SchedulingChannel,
) {
    let mut view = channel.channel_view().unwrap();
    let mut request = if let Some(failpoint) = case.strip_prefix("post-") {
        // The historical fault names now model failed public publication.
        env::set_var("AMS_MEL_TEST_COMMON_POST_SEND_FAILURE", failpoint);
        assert_error(
            submit(&mut channel, &TASK).unwrap_err(),
            ErrorKind::InternalError,
            "injected publication failure; completion worker owns future",
        );
        None
    } else {
        let mut request = submit(&mut channel, &TASK).unwrap();
        for timeout in [0, 5, 0] {
            assert_eq!(
                request.wait(timeout).unwrap_err().kind(),
                &ErrorKind::Timeout
            );
        }
        assert_exhausted(submit(&mut channel, &TASK).unwrap_err());
        Some(request)
    };
    match case {
        "observer-close" => request.take().unwrap().close().unwrap(),
        "observer-drop" => drop(request.take().unwrap()),
        _ => {}
    }
    // The StareTask still holds the Session-wide permit after dropping its
    // observer (or failing publication). KeepAlive must not reach the provider.
    assert_exhausted(view.send_keepalive().unwrap_err());
    assert_dispatch(log, &pin, 1);
    if case == "session-drop" {
        drop(session);
    } else {
        session.close().unwrap();
    }
    channel.close().unwrap();
    assert_error(
        submit(&mut channel, &TASK).unwrap_err(),
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
    // RTLD_NOLOAD proves the pending graph retained the DSO without us. Pin
    // it across the test release function itself (which executes in the DSO).
    let pin = Pin::loaded();
    let detach_failed = case == "detach-fail";
    pin.release(16); // StareTask mock future family
    if let Some(mut request) = request {
        let result = request.wait(WAIT_MS);
        assert_eq!(request.wait(0), result);
        if detach_failed {
            assert_error(
                result.unwrap_err(),
                ErrorKind::ProviderFailed,
                "deferred Scheduling cleanup failed",
            );
        } else {
            assert_eq!(result.unwrap(), SUCCESS);
        }
        request.close().unwrap();
    }
    // Terminal publication isn't the permit-retirement boundary. Wait for
    // native final-owner notification before dropping the last test pin.
    wait_for_return_cleanup();
    assert_dispatch(log, &pin, 1);
    log.check("scheduling_enabled", 1);
    log.check("library_unloaded", 0);
    drop(pin);
    if detach_failed {
        log.check("channel_detach_failed", 1);
        log.check("scheduling_disabled", 1);
        log.check("channel_detached", 0);
        log.check("scheduling_channel_destroyed", 0);
        log.check("library_unloaded", 0);
        drop(Pin::loaded()); // The native graph remains resident after the failure.
    } else {
        log.check_cleanup_order(true);
    }
}

pub(super) fn run(scenario: &str, log: &Log) {
    let case = scenario.strip_prefix("stare-").unwrap();
    let pending = matches!(
        case,
        "closure" | "session-drop" | "observer-close" | "observer-drop" | "detach-fail"
    ) || case.starts_with("post-");
    let native = if case == "admission-states" {
        "sched-enable-fail".into()
    } else if pending && case != "detach-fail" {
        "sched-stare-delayed".into()
    } else {
        format!("sched-{scenario}")
    };
    let session = Session::open_with_options(
        provider(),
        &native,
        "",
        SessionOptions {
            max_async_requests: 1,
        },
    )
    .unwrap();
    let mut channel = session.open_scheduling_channel(&config()).unwrap();
    let pin = Pin::loaded();
    assert_error(
        submit(&mut channel, &TASK).unwrap_err(),
        ErrorKind::ProviderFailed,
        "common channel is not available",
    );
    assert_dispatch(log, &pin, 0);
    log.check("scheduling_enabled", 0);

    let calls = if case == "admission-states" {
        assert_eq!(
            channel.enable().unwrap_err().kind(),
            &ErrorKind::ProviderFailed
        );
        assert_error(
            submit(&mut channel, &TASK).unwrap_err(),
            ErrorKind::ProviderFailed,
            "common channel is not available",
        );
        0
    } else {
        channel.enable().unwrap();
        if pending {
            pending_stare(case, log, pin, session, channel);
            return;
        }
        if case == "success" {
            exact_values(&mut channel, log, &pin);
            4
        } else {
            if case == "reentry" {
                arm_reentry(
                    &pin,
                    c"mock_scheduling_stare_reentry",
                    channel.channel_view().unwrap(),
                );
            }
            terminal(case, &mut channel);
            if case == "reentry" {
                finish_reentry();
            }
            verify_values(&pin, &TASK, (0, 1), 0);
            1
        }
    };
    channel.close().unwrap();
    assert_error(
        submit(&mut channel, &TASK).unwrap_err(),
        ErrorKind::ProviderFailed,
        "IR Scheduling channel is closed",
    );
    session.close().unwrap();
    wait_for_return_cleanup();
    assert_dispatch(log, &pin, calls);
    drop(pin);
    log.check("scheduling_enabled", 1);
    log.check_cleanup_order(true);
}

scenarios! {
    stare_refuses_attached_failed_and_closed => "stare-admission-states",
    stare_provider_receives_exact_values => "stare-success",
    stare_completed_fail => "stare-fail",
    stare_completed_not_supported => "stare-not-supported",
    stare_completed_not_implemented => "stare-not-implemented",
    stare_rejected_unsupported => "stare-unsupported",
    stare_rejected_invalid_state => "stare-invalid-state",
    stare_exec_exception => "stare-exec-throw",
    stare_future_exception => "stare-future-throw",
    stare_invalid_future_before_publication => "stare-invalid-future",
    stare_null_result => "stare-null",
    stare_malformed_result => "stare-malformed",
    stare_unknown_return => "stare-invalid-return",
    stare_reentrant_same_channel_capabilities => "stare-reentry",
    stare_survives_session_channel_and_view_close => "stare-closure",
    stare_survives_session_drop => "stare-session-drop",
    stare_observer_close_does_not_cancel => "stare-observer-close",
    stare_observer_drop_does_not_cancel => "stare-observer-drop",
    stare_post_send_allocation_failure_is_not_retried => "stare-post-allocation",
    stare_post_send_worker_launch_failure_is_not_retried => "stare-post-worker-launch",
    stare_deferred_detach_failure_retains_native_graph => "stare-detach-fail",
}
