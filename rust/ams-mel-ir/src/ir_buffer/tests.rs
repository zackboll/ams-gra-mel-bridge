//! Narrow malicious-output seams. Tokens never enter native production calls.
use super::*;
use std::cell::Cell;

fn diagnostic(d: *mut c_char, r: *mut usize) {
    // SAFETY: only called with writable slots from call_with_diagnostic.
    unsafe {
        *d = 0;
        *r = 1;
    }
}

#[test]
fn open_requires_owner_and_real_success_return() {
    for (owner, result) in [(false, 0), (true, 3), (true, u32::MAX)] {
        let cleanups = Cell::new(0);
        let error = open_with(
            1,
            |out, ret, d, _, r| {
                diagnostic(d, r);
                if owner {
                    *out = ptr::dangling_mut();
                }
                *ret = result;
                sys::AMS_MEL_OK
            },
            |_| cleanups.set(cleanups.get() + 1),
        )
        .unwrap_err();
        assert_eq!(error.error.kind(), &ErrorKind::ProtocolInconsistency);
        assert_eq!(error.native_status, Some(sys::AMS_MEL_OK));
        assert_eq!(error.provider_return, returned(result));
        assert_eq!(cleanups.get(), usize::from(owner));
    }
}

#[test]
fn failure_output_cleanup_preserves_original_status_return_and_diagnostic() {
    let calls = Cell::new(0);
    let error = open_with(
        8,
        |out, ret, d, _, r| {
            *out = ptr::dangling_mut();
            *ret = 79;
            // SAFETY: the diagnostic array has at least 8 bytes.
            unsafe {
                ptr::copy_nonoverlapping(c"refused".as_ptr(), d, 8);
                *r = 8;
            }
            sys::AMS_MEL_INITIALIZATION_FAILED
        },
        |raw| {
            assert!(!raw.is_null());
            calls.set(calls.get() + 1);
        },
    )
    .unwrap_err();
    assert_eq!(calls.get(), 1);
    assert_eq!(
        error.native_status,
        Some(sys::AMS_MEL_INITIALIZATION_FAILED)
    );
    assert_eq!(error.error.diagnostic(), Some("refused"));
    assert_eq!(error.provider_return, Some(79));
    assert!(std::error::Error::source(&error).is_some());
}

#[test]
fn malformed_diagnostic_still_preserves_owner_cleanup_and_native_status() {
    let calls = Cell::new(0);
    let error = open_with(
        1,
        |out, ret, _, _, r| {
            *out = ptr::dangling_mut();
            *ret = 0;
            // SAFETY: valid diagnostic-required output slot.
            unsafe {
                *r = 0;
            }
            sys::AMS_MEL_PROVIDER_EXCEPTION
        },
        |_| calls.set(calls.get() + 1),
    )
    .unwrap_err();
    assert_eq!(calls.get(), 1);
    assert_eq!(error.error.kind(), &ErrorKind::ProtocolInconsistency);
    assert_eq!(error.native_status, Some(sys::AMS_MEL_PROVIDER_EXCEPTION));
    assert_eq!(error.provider_return, Some(0));
}

#[test]
fn close_retains_failed_and_malformed_success_handles_then_retries() {
    let token = ptr::dangling_mut();
    let mut raw = token;
    for status in [
        sys::AMS_MEL_PROVIDER_FAILED,
        sys::AMS_MEL_PROVIDER_EXCEPTION,
        sys::AMS_MEL_OK,
    ] {
        let error = close_with(&mut raw, |_, d, _, r| {
            diagnostic(d, r);
            status
        })
        .unwrap_err();
        assert_eq!(raw, token);
        assert_eq!(
            error.kind(),
            &match status {
                sys::AMS_MEL_PROVIDER_FAILED => ErrorKind::ProviderFailed,
                sys::AMS_MEL_PROVIDER_EXCEPTION => ErrorKind::ProviderException,
                _ => ErrorKind::ProtocolInconsistency,
            }
        );
    }
    close_with(&mut raw, |out, d, _, r| {
        *out = ptr::null_mut();
        diagnostic(d, r);
        sys::AMS_MEL_OK
    })
    .unwrap();
    assert!(raw.is_null());
    close_with(&mut raw, |out, d, _, r| {
        assert!(out.is_null());
        diagnostic(d, r);
        sys::AMS_MEL_OK
    })
    .unwrap();
}

#[test]
fn info_rejects_unknown_location_and_noncanonical_boolean() {
    for location in [3, u32::MAX] {
        assert_eq!(
            info_from_raw(sys::AmsMelIrBufferInfoV1 {
                image_location: location,
                ..Default::default()
            })
            .unwrap_err()
            .kind(),
            &ErrorKind::ProtocolInconsistency
        );
    }
    assert!(info_from_raw(sys::AmsMelIrBufferInfoV1 {
        buffer_address_matches: 2,
        ..Default::default()
    })
    .is_err());
}

#[test]
fn flags_changes_are_bounded_and_never_publish_partial_or_unknown_values() {
    for (second_count, value, status, ret, succeeds) in [
        (1, 2, sys::AMS_MEL_OK, 0, true), // shrink is safe
        (3, 0, sys::AMS_MEL_OK, 0, false),
        (3, 0, sys::AMS_MEL_BUFFER_TOO_SMALL, 0, false),
        (2, 3, sys::AMS_MEL_OK, 0, false),
        (2, u32::MAX, sys::AMS_MEL_OK, 0, false),
        (2, 0, sys::AMS_MEL_OK, 4, false),
        (2, 0, sys::AMS_MEL_OK, u32::MAX, false),
        (2, 0, sys::AMS_MEL_PROVIDER_FAILED, 71, false),
    ] {
        let mut calls = 0;
        let output = flags_with(|values, count, result, d, _, r| {
            calls += 1;
            diagnostic(d, r);
            if let Some(values) = values {
                assert_eq!(values.len(), 2);
                values.fill(value);
                *count = second_count;
                *result = ret;
                status
            } else {
                *count = 2;
                *result = 0;
                sys::AMS_MEL_BUFFER_TOO_SMALL
            }
        });
        assert_eq!(calls, 2);
        assert_eq!(output.is_ok(), succeeds);
        if succeeds {
            assert_eq!(output.unwrap(), [BufferFlag::Rdma]);
        } else {
            let error = output.unwrap_err();
            assert_eq!(error.provider_return, returned(ret));
            assert_eq!(error.native_status, Some(status));
        }
    }
}

#[test]
fn flags_count_overflow_missing_return_and_allocation_failures() {
    for (count_value, ret) in [(usize::MAX, 0), (1, 19), (1, u32::MAX)] {
        let mut calls = 0;
        let error = flags_with(|_, count, result, d, _, r| {
            calls += 1;
            diagnostic(d, r);
            *count = count_value;
            *result = ret;
            sys::AMS_MEL_BUFFER_TOO_SMALL
        })
        .unwrap_err();
        assert_eq!(calls, 1);
        assert_eq!(error.provider_return, returned(ret));
        assert!(std::error::Error::source(&error).is_some());
    }
    for allocation in [0, 1] {
        // Fail either raw numeric allocation or converted flag-vector allocation.
        crate::test_allocator::ALLOCATIONS_UNTIL_FAILURE.with(|v| v.set(allocation));
        let error = flags_with(|values, count, result, d, _, r| {
            diagnostic(d, r);
            *count = 7;
            *result = 0;
            if let Some(values) = values {
                values.fill(1);
                sys::AMS_MEL_OK
            } else {
                sys::AMS_MEL_BUFFER_TOO_SMALL
            }
        })
        .unwrap_err();
        assert_eq!(error.error.kind(), &ErrorKind::ResourceExhausted);
    }
}

#[test]
fn copying_uses_fallible_allocation_and_checks_spans_before_allocating() {
    crate::test_allocator::FAIL_SIZE.with(|v| v.set(257));
    assert_eq!(
        allocated(257, 0_u8).unwrap_err().kind(),
        &ErrorKind::ResourceExhausted
    );
    assert!(allocated(usize::MAX, 0_u32).is_err());
    let owner = HostBuffer {
        raw: ptr::dangling_mut(),
        storage_size: 8,
        _not_send_sync: PhantomData,
    };
    assert!(owner.span(8, 0).is_ok());
    assert!(owner.span(9, 0).is_err());
    assert!(owner.span(u64::MAX, usize::MAX).is_err());
    assert!(owner.span(1, usize::MAX).is_err());
    // Token is not a native owner and must never enter Drop/native Close.
    std::mem::forget(owner);
}

#[test]
fn partial_read_failure_cannot_publish_bytes_and_allocation_precedes_call() {
    let error = read_with(8, |bytes, d, _, r| {
        bytes[0] = 255;
        diagnostic(d, r);
        sys::AMS_MEL_PROVIDER_FAILED
    })
    .unwrap_err();
    assert_eq!(error.kind(), &ErrorKind::ProviderFailed);
    crate::test_allocator::FAIL_SIZE.with(|v| v.set(257));
    let error = read_with(257, |_, _, _, _| {
        panic!("native must not be called after allocation failure")
    })
    .unwrap_err();
    assert_eq!(error.kind(), &ErrorKind::ResourceExhausted);
}

#[test]
fn inconsistent_count_query_status_is_rejected_without_copy() {
    for (status, size) in [(sys::AMS_MEL_OK, 1), (sys::AMS_MEL_BUFFER_TOO_SMALL, 0)] {
        let mut calls = 0;
        let error = flags_with(|_, count, result, d, _, r| {
            calls += 1;
            diagnostic(d, r);
            *count = size;
            *result = 0;
            status
        })
        .unwrap_err();
        assert_eq!(calls, 1);
        assert_eq!(error.error.kind(), &ErrorKind::ProtocolInconsistency);
    }
}
