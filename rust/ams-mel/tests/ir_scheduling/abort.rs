use super::*;
use std::cell::RefCell;

const TASK: SchedulingTaskInfo = SchedulingTaskInfo {
    system_time_ns: i64::MIN,
    request_id: 0xf123_4567,
    action: TaskListAction::Normal,
    precedence_within_priority: u32::MAX,
    priority: 0x8000_0001,
};
const SUCCESS: ReturnResult = ReturnResult::Completed {
    value: CommandReturn::Success,
};
const WAIT_MS: u32 = 5_000;

// Type identity is intentional: no compatibility-layer wrappers or copies.
fn submit(
    channel: &mut SchedulingChannel,
    info: &ams_mel_ir::SchedulingTaskInfo,
) -> Result<ams_mel_ir::ReturnRequest, Error> {
    channel.exec_abort(info)
}

fn wait_for_return_cleanup() {
    unsafe extern "C" {
        fn ams_mel_test_completion_owner_notify(
            family: u32,
            op: u32,
            target: u64,
            out: *mut u64,
        ) -> i32;
    }
    let mut observed = 0;
    // SAFETY: test-only handshake, Return family, drain all owner notifications.
    assert_eq!(
        unsafe { ams_mel_test_completion_owner_notify(1, 0, 0, &mut observed) },
        1
    );
}

fn terminal(scenario: &str, channel: &mut SchedulingChannel) -> u32 {
    if scenario == "exec-throw" {
        let err = submit(channel, &TASK).unwrap_err();
        assert_eq!(err.kind(), &ErrorKind::ProviderException);
        assert_eq!(err.diagnostic(), Some("mock AbortTask exec exception"));
        return 1;
    }
    let mut request = submit(channel, &TASK).unwrap();
    let outcome = request.wait(WAIT_MS);
    assert_eq!(request.wait(0), outcome);
    match scenario {
        "unsupported" | "invalid-state" => {
            assert_eq!(
                outcome.unwrap(),
                ReturnResult::Rejected {
                    code: if scenario == "unsupported" {
                        MelErrorCode::Unsupported
                    } else {
                        MelErrorCode::InvalidState
                    },
                    description: format!("AbortTask rejection: {}", "x".repeat(1024)),
                }
            );
        }
        "future-throw" | "null" | "invalid-return" => {
            let (kind, diagnostic) = match scenario {
                "future-throw" => (
                    ErrorKind::ProviderException,
                    "mock AbortTask future exception",
                ),
                "null" => (
                    ErrorKind::ProviderFailed,
                    "provider returned null or unowned successful Return result",
                ),
                _ => (
                    ErrorKind::ProviderFailed,
                    "provider returned unknown IR Return value",
                ),
            };
            let err = outcome.unwrap_err();
            assert_eq!(err.kind(), &kind);
            assert_eq!(err.diagnostic(), Some(diagnostic));
        }
        _ => {
            let value = match scenario {
                "success" | "reentry" => CommandReturn::Success,
                "fail" => CommandReturn::Fail,
                "not-supported" => CommandReturn::NotSupported,
                "not-implemented" => CommandReturn::NotImplemented,
                _ => unreachable!("unexpected case: {scenario}"),
            };
            assert_eq!(outcome.unwrap(), ReturnResult::Completed { value });
        }
    }
    request.close().unwrap();
    if scenario != "success" {
        return 1;
    }
    // Terminal Wait may precede retirement of the single Session permit.
    wait_for_return_cleanup();
    for (action, system_time_ns) in [
        (TaskListAction::Next, i64::MAX),
        (TaskListAction::Immediate, -1),
        (TaskListAction::Abort, 0),
    ] {
        let info = SchedulingTaskInfo {
            action,
            system_time_ns,
            request_id: u32::MAX,
            ..TASK
        };
        let mut request = submit(channel, &info).unwrap();
        assert_eq!(request.wait(WAIT_MS).unwrap(), SUCCESS);
        request.close().unwrap();
        wait_for_return_cleanup();
    }
    4
}

thread_local! {
    static REENTRY: RefCell<Option<(ChannelView, bool)>> = const { RefCell::new(None) };
}

extern "C" fn reenter_capabilities() {
    // The mock calls back on the submitting thread. Use an independent weak
    // view of the same channel, not an alias of the mutably borrowed owner.
    REENTRY.with_borrow_mut(|slot| {
        if let Some((view, succeeded)) = slot {
            *succeeded = view.capabilities().is_ok();
        }
    });
}

fn arm_reentry(pin: &Pin, view: ChannelView) {
    REENTRY.set(Some((view, false)));
    // SAFETY: exact mock signature, held DSO pin. The mock consumes this hook
    // once, synchronously inside exec, before returning the future.
    unsafe {
        let symbol = dlsym(pin.0, c"mock_scheduling_abort_reentry".as_ptr());
        assert!(!symbol.is_null());
        let set: unsafe extern "C" fn(extern "C" fn()) = std::mem::transmute(symbol);
        set(reenter_capabilities);
    }
}

fn view_closed(view: &mut ChannelView) {
    assert_eq!(
        view.capabilities().unwrap_err().kind(),
        &ErrorKind::ProviderFailed
    );
    view.close().unwrap();
}

fn assert_dispatch(log: &Log, pin: &Pin, count: u32) {
    assert_eq!(pin.count(c"mock_scheduling_abort_calls"), count);
    for symbol in [
        c"mock_scheduling_specific_calls",
        c"mock_scheduling_ack_registrations",
        c"mock_scheduling_rep_registrations",
        c"mock_scheduling_stare_calls",
        c"mock_scheduling_scan_calls",
        c"mock_scheduling_track_calls",
    ] {
        assert_eq!(pin.count(symbol), 0, "{symbol:?}");
    }
    log.check("scheduling_abort_task_exec", count as usize);
    log.no_unexpected_scheduling_operations(true);
}

pub(super) fn run(scenario: &str, log: &Log) {
    let case = scenario.strip_prefix("abort-").unwrap();
    let post_send = case.strip_prefix("post-");
    let delayed =
        matches!(case, "closure" | "observer-close" | "observer-drop") || post_send.is_some();
    let native = if delayed {
        "sched-abort-delayed".into()
    } else if case == "admission-states" {
        "sched-enable-fail".into()
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
    let mut view = channel.channel_view().unwrap();
    let pin = Pin::loaded();
    assert_eq!(
        submit(&mut channel, &TASK).unwrap_err().diagnostic(),
        Some("common channel is not available")
    );
    assert_dispatch(log, &pin, 0);

    if case == "admission-states" {
        assert_eq!(
            channel.enable().unwrap_err().kind(),
            &ErrorKind::ProviderFailed
        );
        assert_eq!(
            submit(&mut channel, &TASK).unwrap_err().kind(),
            &ErrorKind::ProviderFailed
        );
    } else {
        channel.enable().unwrap();
    }

    if delayed {
        let mut request = if let Some(failpoint) = post_send {
            // Each mode runs in its own child, before dispatch/worker startup.
            env::set_var("AMS_MEL_TEST_COMMON_POST_SEND_FAILURE", failpoint);
            let err = submit(&mut channel, &TASK).unwrap_err();
            assert_eq!(err.kind(), &ErrorKind::InternalError);
            assert_eq!(
                err.diagnostic(),
                Some("injected publication failure; completion worker owns future")
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
            Some(request)
        };
        // The admitted AbortTask holds the Session-wide permit, not just a
        // Scheduling slot. Neither another AbortTask nor KeepAlive can enter.
        let assert_limit = |error: Error| {
            assert_eq!(error.kind(), &ErrorKind::ResourceExhausted);
            assert_eq!(error.diagnostic(), Some("async request limit reached"));
        };
        if post_send.is_none() {
            assert_limit(submit(&mut channel, &TASK).unwrap_err());
        }
        assert_limit(view.send_keepalive().unwrap_err());
        assert_dispatch(log, &pin, 1);
        session.close().unwrap();
        channel.close().unwrap();
        assert_eq!(
            submit(&mut channel, &TASK).unwrap_err().kind(),
            &ErrorKind::ProviderFailed
        );
        view_closed(&mut view);
        if case == "observer-close" {
            request.take().unwrap().close().unwrap();
        } else if case == "observer-drop" {
            drop(request.take().unwrap());
        }
        drop(pin);
        for event in [
            "scheduling_disabled",
            "channel_detached",
            "library_unloaded",
        ] {
            log.check(event, 0);
        }
        // Repin only to execute the fixture's release function safely. The
        // still-pending native graph, not the test harness, retained the DSO.
        let pin = Pin::loaded();
        pin.release(15); // AbortTask mock future family
        if let Some(mut request) = request {
            assert_eq!(request.wait(WAIT_MS).unwrap(), SUCCESS);
            assert_eq!(request.wait(0).unwrap(), SUCCESS);
            request.close().unwrap();
        }
        wait_for_return_cleanup();
        assert_dispatch(log, &pin, 1);
        drop(pin);
    } else {
        if case == "reentry" {
            arm_reentry(&pin, channel.channel_view().unwrap());
        }
        let calls = if case == "admission-states" {
            0
        } else {
            terminal(case, &mut channel)
        };
        if case == "reentry" {
            let (mut reentrant_view, succeeded) = REENTRY.take().unwrap();
            assert!(succeeded);
            reentrant_view.close().unwrap();
        }
        channel.close().unwrap();
        assert_eq!(
            submit(&mut channel, &TASK).unwrap_err().diagnostic(),
            Some("IR Scheduling channel is closed")
        );
        view_closed(&mut view);
        session.close().unwrap();
        wait_for_return_cleanup();
        assert_dispatch(log, &pin, calls);
        drop(pin);
    }
    log.check_cleanup_order(true);
    log.no_unexpected_scheduling_operations(true);
}

scenarios! {
    abort_refuses_attached_failed_and_closed => "abort-admission-states",
    abort_dispatches_all_actions_and_completes => "abort-success",
    abort_completed_fail => "abort-fail",
    abort_completed_not_supported => "abort-not-supported",
    abort_completed_not_implemented => "abort-not-implemented",
    abort_rejected_unsupported => "abort-unsupported",
    abort_rejected_invalid_state => "abort-invalid-state",
    abort_exec_exception => "abort-exec-throw",
    abort_future_exception => "abort-future-throw",
    abort_null_payload => "abort-null",
    abort_unknown_return => "abort-invalid-return",
    abort_reentrant_same_channel_capabilities => "abort-reentry",
    abort_survives_session_channel_and_view_close => "abort-closure",
    abort_observer_close_does_not_cancel => "abort-observer-close",
    abort_observer_drop_does_not_cancel => "abort-observer-drop",
    abort_post_send_allocation_failure_is_not_retried => "abort-post-allocation",
    abort_post_send_worker_launch_failure_is_not_retried => "abort-post-worker-launch",
}
