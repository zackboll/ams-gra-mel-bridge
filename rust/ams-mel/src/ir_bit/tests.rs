use super::*;
use crate::{wait_for_return_with, CommandReturn, MelErrorCode, ReturnResult};

#[test]
fn exact_one_choice_mapping_and_call_lifetimes() {
    for action in [
        BitAction::NoOp,
        BitAction::Initiate(vec![7]),
        BitAction::Initiate(vec![0, u32::MAX, 0x8000_0001, 7, 0, 7]),
        BitAction::Cancel(vec![7]),
        BitAction::Cancel(vec![0, u32::MAX, 0x8000_0001, 7, 0, 7]),
        BitAction::ClearFaultCodes(vec!["fault-€".into()]),
        BitAction::ClearFaultCodes(vec![
            "".into(),
            "é".into(),
            "e\u{301}".into(),
            " 🛰️\r\n ".into(),
            "é".into(),
        ]),
    ] {
        for command_id in [0, 0x8000_0000, u32::MAX] {
            let command = BitCommand {
                command_id,
                action: action.clone(),
            };
            assert_eq!(command, command.clone());
            assert!(format!("{command:?}").contains("BitCommand"));
            let mut calls = 0;
            let error = submit_with(&command, |raw, out| {
                calls += 1;
                assert!(out.is_null());
                assert_eq!(raw.command_id, command_id);
                let (initiate, cancel, faults): (&[u32], &[u32], &[String]) = match &command.action
                {
                    BitAction::NoOp => (&[], &[], &[]),
                    BitAction::Initiate(ids) => (ids, &[], &[]),
                    BitAction::Cancel(ids) => (&[], ids, &[]),
                    BitAction::ClearFaultCodes(codes) => (&[], &[], codes),
                };
                for (span, expected) in [
                    (raw.initiate_bit_ids, initiate),
                    (raw.cancel_bit_ids, cancel),
                ] {
                    assert_eq!(span.size, expected.len());
                    if expected.is_empty() {
                        assert!(span.data.is_null());
                    } else {
                        assert_eq!(span.data, expected.as_ptr());
                        // SAFETY: the original command's vector is borrowed through submit.
                        assert_eq!(
                            unsafe { std::slice::from_raw_parts(span.data, span.size) },
                            expected
                        );
                    }
                }
                let span = raw.clear_fault_codes;
                assert_eq!(span.size, faults.len());
                if faults.is_empty() {
                    assert!(span.data.is_null());
                } else {
                    // SAFETY: submit_with keeps the descriptor vector alive during this call.
                    let views = unsafe { std::slice::from_raw_parts(span.data, span.size) };
                    for (view, expected) in views.iter().zip(faults) {
                        assert_eq!(view.size, expected.len()); // bytes, not characters
                        if expected.is_empty() {
                            assert!(view.data.is_null());
                        } else {
                            assert_eq!(view.data.cast::<u8>(), expected.as_ptr());
                            // SAFETY: each original String remains borrowed through submit.
                            let copied = unsafe {
                                std::slice::from_raw_parts(view.data.cast::<u8>(), view.size)
                            }
                            .to_vec();
                            assert_eq!(copied, expected.as_bytes());
                        }
                    }
                }
                Err(Error::new(ErrorKind::ResourceExhausted, "injected refusal"))
            })
            .unwrap_err();
            assert_eq!(error.kind(), &ErrorKind::ResourceExhausted);
            assert_eq!(calls, 1);
        }
    }
}

#[test]
fn validation_precedes_submission_and_null_success_fails_closed() {
    for action in [
        BitAction::Initiate(vec![]),
        BitAction::Cancel(vec![]),
        BitAction::ClearFaultCodes(vec![]),
        BitAction::ClearFaultCodes(vec!["\0".into()]),
        BitAction::ClearFaultCodes(vec!["valid".into(), "€\0after".into()]),
    ] {
        let command = BitCommand {
            command_id: 1,
            action,
        };
        assert_eq!(
            submit_with(&command, |_, _| panic!("invalid command submitted"))
                .unwrap_err()
                .kind(),
            &ErrorKind::InvalidArgument
        );
    }
    let command = BitCommand {
        command_id: 0,
        action: BitAction::NoOp,
    };
    let mut calls = 0;
    assert_eq!(
        submit_with(&command, |_, out| {
            calls += 1;
            assert!(out.is_null());
            Ok(())
        })
        .unwrap_err()
        .kind(),
        &ErrorKind::ProtocolInconsistency
    );
    assert_eq!(calls, 1);
}
#[test]
fn all_published_returns_are_completed_payloads() {
    for (raw, value) in [
        CommandReturn::Success,
        CommandReturn::BadPointer,
        CommandReturn::Fail,
        CommandReturn::NotSupported,
        CommandReturn::NotImplemented,
    ]
    .into_iter()
    .enumerate()
    {
        let result = wait_for_return_with(0, |_, result, out, _, required| {
            result.value = raw as u32;
            result.error_code = sys::AMS_MEL_ERROR_NONE;
            *required = 1;
            // SAFETY: wait supplies writable diagnostic storage.
            unsafe { *out = 0 };
            sys::AMS_MEL_OK
        })
        .unwrap();
        assert_eq!(result, ReturnResult::Completed { value });
    }
}

#[test]
fn long_utf8_rejection_retrieves_cached_diagnostic_without_resubmit() {
    let text = format!("{}€{}", "x".repeat(510), "y".repeat(100));
    let bytes = format!("{text}\0").into_bytes();
    let mut calls = 0;
    let result = wait_for_return_with(123, |timeout, result, out, capacity, required| {
        calls += 1;
        assert_eq!(timeout, if calls == 1 { 123 } else { 0 });
        assert_eq!(capacity, if calls == 1 { 512 } else { bytes.len() });
        result.error_code = sys::AMS_MEL_ERROR_INVALID_PARAMETERS;
        *required = bytes.len();
        // SAFETY: copy is bounded by the output capacity and source length.
        unsafe {
            ptr::copy_nonoverlapping(bytes.as_ptr(), out.cast::<u8>(), capacity.min(bytes.len()));
        }
        sys::AMS_MEL_COMMAND_REJECTED
    })
    .unwrap();
    assert_eq!(calls, 2);
    assert_eq!(
        result,
        ReturnResult::Rejected {
            code: MelErrorCode::InvalidParameters,
            description: text,
        }
    );
}

#[test]
fn malformed_return_outputs_fail_closed() {
    for (status, value, code, bytes, size) in [
        (sys::AMS_MEL_OK, u32::MAX, 0, &b"\0"[..], 1),
        (sys::AMS_MEL_OK, 0, 3, &b"\0"[..], 1),
        (sys::AMS_MEL_OK, 0, 0, &b"x\0"[..], 2),
        (sys::AMS_MEL_OK, 0, 0, &b"\0"[..], 0),
        (sys::AMS_MEL_COMMAND_REJECTED, 0, 3, &b"\xff\0"[..], 2),
        (sys::AMS_MEL_COMMAND_REJECTED, 0, 3, &b"x"[..], 1),
        (sys::AMS_MEL_COMMAND_REJECTED, 0, 3, &b"a\0b\0"[..], 4),
    ] {
        let result = wait_for_return_with(0, |_, result, out, capacity, required| {
            result.value = value;
            result.error_code = code;
            *required = size;
            // SAFETY: wait provides sufficient writable capacity for these small fixtures.
            unsafe {
                ptr::copy_nonoverlapping(
                    bytes.as_ptr(),
                    out.cast::<u8>(),
                    capacity.min(bytes.len()),
                );
            }
            status
        });
        assert!(result.is_err(), "{status} {value} {code} {bytes:?}");
    }
}

#[test]
fn timeout_never_retries_and_provider_failure_remains_an_error() {
    for (status, kind) in [
        (sys::AMS_MEL_TIMEOUT, ErrorKind::Timeout),
        (sys::AMS_MEL_PROVIDER_FAILED, ErrorKind::ProviderFailed),
        (
            sys::AMS_MEL_PROVIDER_EXCEPTION,
            ErrorKind::ProviderException,
        ),
    ] {
        let mut calls = 0;
        let error = wait_for_return_with(0, |timeout, _, out, _, required| {
            calls += 1;
            assert_eq!(timeout, 0);
            *required = 1;
            // SAFETY: wait provides writable diagnostic storage.
            unsafe { *out = 0 };
            status
        })
        .unwrap_err();
        assert_eq!(error.kind(), &kind);
        assert_eq!(calls, 1);
    }
}

#[test]
fn changing_cached_terminal_output_is_rejected() {
    for field in 0..4 {
        let mut calls = 0;
        let result = wait_for_return_with(10, |timeout, result, out, _, required| {
            calls += 1;
            assert_eq!(timeout, if calls == 1 { 10 } else { 0 });
            *required = 600;
            result.error_code = sys::AMS_MEL_ERROR_INVALID_PARAMETERS;
            // SAFETY: wait provides at least one writable byte.
            unsafe { *out = 0 };
            if calls == 2 {
                match field {
                    0 => *required = 599,
                    1 => result.value = 1,
                    2 => result.error_code = 4,
                    _ => return sys::AMS_MEL_PROVIDER_FAILED,
                }
            }
            sys::AMS_MEL_COMMAND_REJECTED
        });
        assert_eq!(
            result.unwrap_err().kind(),
            &ErrorKind::ProtocolInconsistency
        );
        assert_eq!(calls, 2);
    }
}

#[test]
fn unexpected_error_owner_uses_existing_raii_once() {
    use crate::{ComponentLocation, ControlConfig, Session, UciId};
    let path = std::env::var_os("AMS_MEL_TEST_PROVIDER_DIR")
        .map(std::path::PathBuf::from)
        .unwrap_or_else(|| {
            std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
                .join("../../native/build-tests/test-providers")
        })
        .join("libmock_ir_provider.so");
    let session = Session::open(path, "bit-empty", "").unwrap();
    let config = ControlConfig::new(
        UciId::new([0; 16], "channel").unwrap(),
        UciId::new([0; 16], "platform").unwrap(),
        ComponentLocation::new(1.25, -2.5, 3.75, "station-1", "mock-aircraft").unwrap(),
    );
    let mut channel = session.open_control_channel(&config).unwrap();
    channel.enable().unwrap();
    let command = BitCommand {
        command_id: 0,
        action: BitAction::NoOp,
    };
    for (status, size, byte, expected) in [
        (
            sys::AMS_MEL_PROVIDER_EXCEPTION,
            1,
            0,
            ErrorKind::ProviderException,
        ),
        (sys::AMS_MEL_OK, 0, 0, ErrorKind::ProtocolInconsistency),
        (sys::AMS_MEL_OK, 1, 0xff, ErrorKind::ProtocolInconsistency),
        (sys::AMS_MEL_OK, 5000, 0, ErrorKind::ProtocolInconsistency),
    ] {
        let mut request = channel.submit_bit(&command).unwrap();
        request.wait(1000).unwrap();
        let raw = std::mem::replace(&mut request.raw, ptr::null_mut());
        let mut calls = 0;
        let error = submit_with(&command, |_, out| {
            *out = raw; // real unique native owner, never a fabricated address
            call_with_diagnostic(|diagnostic, _, required| {
                calls += 1;
                // SAFETY: the diagnostic helper provides writable output storage.
                unsafe {
                    *required = size;
                    *diagnostic = byte as std::ffi::c_char;
                }
                status
            })
        })
        .unwrap_err();
        assert_eq!(calls, 1);
        assert_eq!(error.kind(), &expected);
        drop(request); // disarmed owner must not attempt another release
    }
    channel.close().unwrap();
    session.close().unwrap();
}
