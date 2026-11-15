use super::*;
use std::cell::Cell;

// Every hostile owner is a live Box token, consumed only by this fake Close.
// It is never sent to an actual native function.
struct Token {
    closes: Rc<Cell<usize>>,
    mode: u8,
}

unsafe extern "C" fn close(
    out: *mut *mut sys::AmsMelIrChannelCommsMetadata,
    d: *mut c_char,
    _: usize,
    r: *mut usize,
) -> i32 {
    // SAFETY: all tests publish a unique live Token with this exact Close seam.
    let token = unsafe { Box::from_raw((*out).cast::<Token>()) };
    token.closes.set(token.closes.get() + 1);
    // Mode 1 deliberately leaves a consumed pointer unchanged; never dereference it again.
    unsafe {
        if token.mode != 1 {
            *out = ptr::null_mut();
        }
        if !d.is_null() {
            *d = 0;
        }
        if !r.is_null() {
            *r = if token.mode == 2 { 0 } else { 1 };
        }
    }
    if token.mode == 3 {
        sys::AMS_MEL_PROVIDER_FAILED
    } else {
        sys::AMS_MEL_OK
    }
}

unsafe fn diagnostic(d: *mut c_char, r: *mut usize) {
    // SAFETY: writable slots supplied by call_with_diagnostic.
    unsafe {
        *d = 0;
        *r = 1;
    }
}

fn token(closes: &Rc<Cell<usize>>, mode: u8) -> *mut sys::AmsMelIrChannelCommsMetadata {
    Box::into_raw(Box::new(Token {
        closes: closes.clone(),
        mode,
    }))
    .cast()
}

fn stream(mode: u8) -> (CommsMetadataStream, Rc<Cell<usize>>) {
    let closes = Rc::new(Cell::new(0));
    let raw = token(&closes, mode);
    (
        CommsMetadataStream {
            owner: Receiver { raw, close },
            _not_send_sync: Rc::new(()),
        },
        closes,
    )
}

#[test]
fn zero_capacity_never_enters_native() {
    let error = CommsMetadataStream::open_with(0, close, |_, _, _, _, _| panic!("native entry"))
        .unwrap_err();
    assert_eq!(error.error().kind(), &ErrorKind::InvalidArgument);
    assert_eq!(error.registration(), None);
}

#[test]
fn open_output_matrix_and_primary_error_cleanup() {
    for status in [
        sys::AMS_MEL_OK,
        sys::AMS_MEL_PROVIDER_FAILED,
        sys::AMS_MEL_PROVIDER_EXCEPTION,
        sys::AMS_MEL_INTERNAL_ERROR,
        -77,
    ] {
        for registration in [0, 1, 2, 3, 4, 99, u32::MAX] {
            for publish in [false, true] {
                for bad_diagnostic in [false, true] {
                    let calls = Cell::new(0);
                    let closes = Rc::new(Cell::new(0));
                    let result = CommsMetadataStream::open_with(2, close, |out, reg, d, _, r| {
                        calls.set(calls.get() + 1);
                        // SAFETY: exact initialized writable output slots.
                        unsafe {
                            assert!((*out).is_null());
                            assert_eq!(*reg, u32::MAX);
                            *reg = registration;
                            if publish {
                                *out = token(&closes, 3);
                            }
                            diagnostic(d, r);
                            if bad_diagnostic {
                                *r = 0;
                            }
                        }
                        status
                    });
                    assert_eq!(calls.get(), 1);
                    if status == sys::AMS_MEL_OK && registration == 0 && publish && !bad_diagnostic
                    {
                        drop(result.unwrap());
                    } else {
                        let error = result.unwrap_err();
                        assert_eq!(error.registration(), command_return(registration).ok());
                        let expected = if bad_diagnostic || status == sys::AMS_MEL_OK {
                            ErrorKind::ProtocolInconsistency
                        } else {
                            crate::error_from_status(status, None, None).kind().clone()
                        };
                        assert_eq!(error.error().kind(), &expected);
                        assert!(std::error::Error::source(&error).is_some());
                        assert!(!error.to_string().is_empty());
                    }
                    assert_eq!(closes.get(), usize::from(publish));
                }
            }
        }
    }
}

#[test]
fn malformed_diagnostics_are_never_retried_and_registration_survives() {
    for mode in 0..5 {
        let calls = Cell::new(0);
        let closes = Rc::new(Cell::new(0));
        let error = CommsMetadataStream::open_with(1, close, |out, reg, d, c, r| {
            calls.set(calls.get() + 1);
            // SAFETY: writable output buffer and slots, in bounds.
            unsafe {
                *out = token(&closes, 0);
                *reg = 3;
                diagnostic(d, r);
                match mode {
                    0 => *r = 0,
                    1 => {
                        *r = 2;
                        *d = 1;
                        *d.add(1) = 1;
                    }
                    2 => {
                        *r = 2;
                        *d = -1;
                        *d.add(1) = 0;
                    }
                    3 => {
                        *r = 3;
                        *d = 0;
                        *d.add(1) = 1;
                        *d.add(2) = 0;
                    }
                    _ => *r = c + 1,
                }
            }
            sys::AMS_MEL_PROVIDER_FAILED
        })
        .unwrap_err();
        assert_eq!(error.registration(), Some(CommandReturn::NotSupported));
        assert_eq!(calls.get(), 1);
        assert_eq!(closes.get(), 1);
    }
}

#[test]
fn close_and_drop_release_at_most_once_even_with_consumed_or_uncertain_outputs() {
    for mode in 0..4 {
        let (mut stream, closes) = stream(mode);
        assert!(stream.is_open());
        let result = stream.close();
        assert_eq!(result.is_ok(), mode == 0);
        assert!(!stream.is_open());
        stream.close().unwrap();
        assert_eq!(
            stream.receive(0).unwrap_err().kind(),
            &ErrorKind::ProviderFailed
        );
        assert_eq!(
            stream.counters().unwrap_err().kind(),
            &ErrorKind::ProviderFailed
        );
        drop(stream);
        assert_eq!(closes.get(), 1);
        let (stream, closes) = self::stream(mode);
        drop(stream);
        assert_eq!(closes.get(), 1);
    }
}

#[test]
fn debug_does_not_publish_native_pointer() {
    let (stream, _) = stream(0);
    let debug = format!("{stream:?}");
    assert!(debug.contains("is_open: true"));
    assert!(!debug.contains("0x"));
    assert!(!debug.contains("raw:"));
}

#[test]
fn receive_and_counters_copy_full_width_once_and_discard_failed_partial_outputs() {
    let (mut stream, _) = stream(0);
    for status in [
        sys::AMS_MEL_OK,
        sys::AMS_MEL_TIMEOUT,
        sys::AMS_MEL_STREAM_STOPPED,
        sys::AMS_MEL_PROVIDER_FAILED,
        sys::AMS_MEL_PROVIDER_EXCEPTION,
        -55,
    ] {
        for malformed in [false, true] {
            let calls = Cell::new(0);
            let report = stream.receive_with(|out, d, _, r| {
                calls.set(calls.get() + 1);
                // SAFETY: exact writable records supplied by receive seam.
                unsafe {
                    *out = sys::AmsMelIrChannelCommsTestReportV1 {
                        command_id: u32::MAX,
                        request_id: 0x80000000,
                    };
                    diagnostic(d, r);
                    if malformed {
                        *r = 0;
                    }
                }
                status
            });
            assert_eq!(calls.get(), 1);
            if status == 0 && !malformed {
                let CommsTestReport {
                    command_id,
                    request_id,
                } = report.unwrap();
                assert_eq!((command_id, request_id), (u32::MAX, 0x80000000));
            } else {
                assert!(report.is_err());
            }
            calls.set(0);
            let counters = stream.counters_with(|out, d, _, r| {
                calls.set(calls.get() + 1);
                // SAFETY: exact writable record supplied by counters seam.
                unsafe {
                    *out = sys::AmsMelIrC2MetadataCountersV1 {
                        events_received: u64::MAX,
                        events_dropped_queue_full: 1 << 63,
                        malformed_or_unsupported: u64::MAX - 1,
                    };
                    diagnostic(d, r);
                    if malformed {
                        *r = 0;
                    }
                }
                status
            });
            assert_eq!(calls.get(), 1);
            if status == 0 && !malformed {
                assert_eq!(
                    counters.unwrap(),
                    CommsMetadataCounters {
                        events_received: u64::MAX,
                        events_dropped_queue_full: 1 << 63,
                        malformed_or_unsupported: u64::MAX - 1
                    }
                );
            } else {
                assert!(counters.is_err());
            }
        }
    }
}

// Process-isolated compiled-provider controls need private receiver access only
// for established native test probes. No pointer is added to the public API.
mod native {
    use super::*;
    use crate::{CommsTestRequest, ComponentLocation, HealthConfig, Session, UciId};
    use std::{
        env,
        ffi::c_void,
        fs,
        path::Path,
        process::Command,
        sync::{mpsc, Condvar, Mutex, OnceLock},
        thread,
    };

    unsafe extern "C" {
        fn dlopen(path: *const c_char, flags: i32) -> *mut c_void;
        fn dlsym(handle: *mut c_void, name: *const c_char) -> *mut c_void;
        fn dlclose(handle: *mut c_void) -> i32;
        fn ams_mel_test_comms_metadata_probe(
            owner: *mut sys::AmsMelIrChannelCommsMetadata,
            operation: u32,
            hook: Option<extern "C" fn()>,
        ) -> i32;
        fn ams_mel_test_completion_snapshot(family: u32, values: *mut u64) -> i32;
        fn ams_mel_test_completion_owner_notify(
            family: u32,
            operation: u32,
            target: u64,
            observed: *mut u64,
        ) -> i32;
    }
    static BARRIER: OnceLock<(Mutex<(bool, bool)>, Condvar)> = OnceLock::new();
    extern "C" fn hold_callback() {
        let (mutex, ready) = BARRIER.get().unwrap();
        let mut state = mutex.lock().unwrap();
        state.0 = true;
        ready.notify_all();
        while !state.1 {
            state = ready.wait(state).unwrap();
        }
    }
    fn release_callback() {
        let (mutex, ready) = BARRIER.get().unwrap();
        let mut state = mutex.lock().unwrap();
        state.1 = true;
        ready.notify_all();
    }
    fn wait_callback() {
        let (mutex, ready) = BARRIER.get().unwrap();
        let mut state = mutex.lock().unwrap();
        while !state.0 {
            state = ready.wait(state).unwrap();
        }
    }

    thread_local! {
        static REENTER: std::cell::RefCell<Option<crate::HealthStatusChannel>> = const { std::cell::RefCell::new(None) };
    }
    extern "C" fn close_reentry() {
        REENTER.with(|owner| owner.borrow_mut().as_mut().unwrap().close().unwrap());
    }

    #[test]
    fn isolated_native_controls() {
        for case in [
            "saturation",
            "allocation",
            "register-reentry",
            "blocked",
            "async",
            "async-typed",
            "c2",
        ] {
            let output = Command::new("timeout")
                .arg("30s")
                .arg(env::current_exe().unwrap())
                .args([
                    "--exact",
                    "ir_comms_metadata::tests::native::child",
                    "--ignored",
                    "--nocapture",
                ])
                .env("RUST_COMMON_METADATA_PRIVATE_CASE", case)
                .output()
                .unwrap();
            assert!(
                output.status.success(),
                "{case}\n{}\n{}",
                String::from_utf8_lossy(&output.stdout),
                String::from_utf8_lossy(&output.stderr)
            );
            assert!(String::from_utf8_lossy(&output.stdout).contains("PRIVATE_CONTROL_COMPLETE"));
        }
    }

    #[test]
    #[ignore = "private process-isolated native probe entry"]
    fn child() {
        let case = env::var("RUST_COMMON_METADATA_PRIVATE_CASE").unwrap();
        let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("../..");
        let provider = env::var_os("AMS_MEL_TEST_PROVIDER_DIR")
            .map(std::path::PathBuf::from)
            .unwrap_or_else(|| root.join("native/build-tests/test-providers"))
            .join("libmock_ir_provider.so");
        let directory = env::temp_dir().join(format!("rust-common-private-{}", std::process::id()));
        fs::create_dir(&directory).unwrap();
        env::set_var("AMS_MEL_TEST_LIFETIME_LOG", directory.join("events"));
        env::set_var("AMS_MEL_TEST_COMMS_BARRIER", directory.join("gate"));
        let path = std::ffi::CString::new(provider.as_os_str().as_encoded_bytes()).unwrap();
        // SAFETY: test-owned DSO pin, release before unload assertion.
        let pin = unsafe { dlopen(path.as_ptr(), 2) };
        assert!(!pin.is_null());
        let scenario = match case.as_str() {
            "blocked" | "allocation" | "c2" => "parity",
            _ => &case,
        };
        let session = Session::open(&provider, format!("common-meta-{scenario}"), "").unwrap();
        let config = HealthConfig::new(
            UciId::new([0; 16], "").unwrap(),
            UciId::new([0; 16], "").unwrap(),
            ComponentLocation::new(0., 0., 0., "", "").unwrap(),
        );
        let mut health = session.open_health_channel(&config).unwrap();
        let mut view = health.channel_view().unwrap();
        if case == "allocation" {
            let error = view.open_comms_metadata(usize::MAX).unwrap_err();
            assert_eq!(error.error().kind(), &ErrorKind::InternalError);
            assert_eq!(error.registration(), None);
        }
        if case == "register-reentry" {
            REENTER.with(|owner| *owner.borrow_mut() = Some(health));
            // SAFETY: exact mock-only synchronous reentry hook, pin live.
            unsafe {
                let symbol = dlsym(pin, c"mock_comms_reentry".as_ptr());
                assert!(!symbol.is_null());
                let install: unsafe extern "C" fn(extern "C" fn()) = std::mem::transmute(symbol);
                install(close_reentry);
            }
            let mut stream = view.open_comms_metadata(2).unwrap();
            // Closure during registration deactivates admission before provider callback.
            assert_eq!(
                stream.receive(0).unwrap_err().kind(),
                &ErrorKind::StreamStopped
            );
            assert_eq!(stream.counters().unwrap().events_received, 1);
            stream.close().unwrap();
            REENTER.with(|owner| {
                owner.borrow_mut().take();
            });
        } else if case == "c2" {
            let mut c2 = session
                .open_control_channel(&crate::ControlConfig::new(
                    UciId::new([0; 16], "").unwrap(),
                    UciId::new([0; 16], "").unwrap(),
                    ComponentLocation::new(0., 0., 0., "", "").unwrap(),
                ))
                .unwrap();
            let mut cv = c2.channel_view().unwrap();
            assert_eq!(
                cv.open_comms_metadata(2).unwrap_err().error().kind(),
                &ErrorKind::ProviderFailed
            );
            let mut dedicated = c2.open_metadata(2).unwrap();
            dedicated.enable_comms_test_events().unwrap();
            let mut request = cv
                .submit_comms_test(CommsTestRequest {
                    command_id: 17,
                    channel_id: 2,
                    request_id: 19,
                })
                .unwrap();
            request.wait(1000).unwrap();
            request.close().unwrap();
            assert_eq!(
                dedicated.receive(1000).unwrap(),
                crate::C2MetadataEvent::CommsTest(CommsTestReport {
                    command_id: 17,
                    request_id: 19
                })
            );
            dedicated.close().unwrap();
            cv.close().unwrap();
            c2.close().unwrap();
            health.close().unwrap();
        } else {
            let mut stream = view.open_comms_metadata(2).unwrap();
            if case == "saturation" {
                // SAFETY: established test probe, live uniquely owned receiver.
                assert_eq!(
                    unsafe { ams_mel_test_comms_metadata_probe(stream.owner.raw, 0, None) },
                    1
                );
                for index in 0..4 {
                    let mut request = view
                        .submit_comms_test(CommsTestRequest {
                            command_id: u32::MAX - index,
                            channel_id: 4,
                            request_id: 0x80000000 + index,
                        })
                        .unwrap();
                    request.wait(1000).unwrap();
                    request.close().unwrap();
                }
                let expected = CommsMetadataCounters {
                    events_received: u64::MAX,
                    events_dropped_queue_full: u64::MAX,
                    malformed_or_unsupported: u64::MAX,
                };
                assert_eq!(stream.counters().unwrap(), expected);
                for index in 0..2 {
                    assert_eq!(
                        stream.receive(0).unwrap(),
                        CommsTestReport {
                            command_id: u32::MAX - index,
                            request_id: 0x80000000 + index
                        }
                    );
                }
                assert_eq!(stream.counters().unwrap(), expected);
            } else if case == "blocked" {
                // Native-only receive-entry observation, as in the existing
                // Python wakeup control. No public !Send owner crosses threads.
                let raw = stream.owner.raw as usize;
                let (tx, rx) = mpsc::channel();
                let waiter = thread::spawn(move || {
                    let mut report = sys::AmsMelIrChannelCommsTestReportV1::default();
                    tx.send(()).unwrap();
                    // SAFETY: borrowed actual native queue remains owned until join;
                    // only native Receive runs here, while typed Close deactivates it.
                    let status = unsafe {
                        sys::ams_mel_ir_channel_comms_metadata_receive(
                            raw as *const _,
                            10000,
                            &mut report,
                            ptr::null_mut(),
                            0,
                            ptr::null_mut(),
                        )
                    };
                    assert_eq!(status, sys::AMS_MEL_STREAM_STOPPED);
                });
                rx.recv().unwrap();
                health.close().unwrap();
                waiter.join().unwrap();
                assert_eq!(
                    stream.receive(10000).unwrap_err().kind(),
                    &ErrorKind::StreamStopped
                );
            } else if case == "async" || case == "async-typed" {
                BARRIER
                    .set((Mutex::new((false, false)), Condvar::new()))
                    .unwrap();
                // SAFETY: established hook with process lifetime and live receiver.
                assert_eq!(
                    unsafe {
                        ams_mel_test_comms_metadata_probe(stream.owner.raw, 1, Some(hold_callback))
                    },
                    1
                );
                fs::write(directory.join("gate.start"), "").unwrap();
                wait_callback();
                assert!(!fs::read_to_string(directory.join("events"))
                    .unwrap()
                    .contains("library_unloaded"));
                if case == "async" {
                    stream.close().unwrap();
                    release_callback();
                    health.close().unwrap();
                } else {
                    // Release only once typed Close has deactivated reception.
                    // The helper owns no public Rust owner; a native-only monitor
                    // observes the same queue using a borrowed pointer while live.
                    let raw = stream.owner.raw as usize;
                    let (tx, rx) = mpsc::channel();
                    let monitor = thread::spawn(move || {
                        let mut report = sys::AmsMelIrChannelCommsTestReportV1::default();
                        tx.send(()).unwrap();
                        // SAFETY: borrowed real native queue, retained until join;
                        // native queue operations serialize internally. No safe owner crosses threads.
                        let status = unsafe {
                            sys::ams_mel_ir_channel_comms_metadata_receive(
                                raw as *const _,
                                10000,
                                &mut report,
                                ptr::null_mut(),
                                0,
                                ptr::null_mut(),
                            )
                        };
                        assert_eq!(status, sys::AMS_MEL_STREAM_STOPPED);
                        release_callback();
                    });
                    rx.recv().unwrap();
                    health.close().unwrap();
                    monitor.join().unwrap();
                }
                assert!(fs::read_to_string(directory.join("events"))
                    .unwrap()
                    .contains("common_comms_callback_returned"));
            }
            stream.close().unwrap();
            health.close().unwrap();
        }
        view.close().unwrap();
        session.close().unwrap();
        // Explicit observer Close is not worker reclamation. Keep the event
        // directory and DSO pin until every admitted Comms completion has
        // finished its final-owner notification; otherwise a late worker can
        // log after remove_dir_all and abort during process exit.
        let mut counts = [0_u64; 6];
        let mut observed = 0;
        // SAFETY: established test-only probes, process-isolated native facade;
        // family 2 is Comms and no further submissions occur in this child.
        unsafe {
            assert_eq!(ams_mel_test_completion_snapshot(2, counts.as_mut_ptr()), 1);
            assert_eq!(
                ams_mel_test_completion_owner_notify(2, 0, counts[0], &mut observed),
                1
            );
        }
        assert_eq!(observed, counts[0]);
        // SAFETY: unique test DSO pin. Callback/control operations finished above.
        assert_eq!(unsafe { dlclose(pin) }, 0);
        // Completion reclamation is independently qualified in integration tests;
        // these private controls don't infer immediate unload from public Close.
        fs::remove_dir_all(directory).unwrap();
        println!("PRIVATE_CONTROL_COMPLETE {case}");
    }
}
