//! Complete owned reception of the conditional Image LineOfSightQuaternion callback.
//! Native code owns registration, queueing and callback lifetime. The C facade
//! is the trusted memory-provenance boundary; structural pointer checks cannot
//! prove arbitrary addresses are mapped. Rust adds no callbacks or workers.

use std::{ffi::c_char, fmt, ptr, rc::Rc};

use ams_mel_sys as sys;

use crate::{
    call_with_diagnostic, checked_span, command_return, protocol, CommandReturn, Error, ErrorKind,
    ForwardRightDown, ImageMetadataCounters,
};

/// Quaternion components in published XYZW getter order, without normalization.
/// Opposite signs remain distinct; no frame conversion or attitude is inferred.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Quaternion {
    pub x: f64,
    pub y: f64,
    pub z: f64,
    pub w: f64,
}

/// Complete owned callback value. Signed nanoseconds have no inferred epoch.
/// Components are copied unchanged, including non-unit, zero, signed-zero,
/// subnormal, extreme and nonfinite values. NaN payload bits are not promised
/// beyond the native contract. Reception is not attitude estimation or evidence
/// of physical accuracy. Values outlive every native owner and provider code.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct LineOfSightQuaternion {
    pub system_time_ns: i64,
    pub q_xyzw: Quaternion,
    pub pqr_rps: ForwardRightDown,
}

/// Conditional registration failure, preserving native status and diagnostic plus
/// the provider Return only when native code actually published a known value.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct LineOfSightQuaternionOpenError {
    error: Error,
    registration: Option<CommandReturn>,
}

impl LineOfSightQuaternionOpenError {
    pub(crate) fn preflight(error: Error) -> Self {
        Self {
            error,
            registration: None,
        }
    }

    pub fn error(&self) -> &Error {
        &self.error
    }

    /// None for local failures, unknown Returns and provider exceptions.
    /// Diagnostic text is never parsed to manufacture a provider Return.
    pub fn registration(&self) -> Option<CommandReturn> {
        self.registration
    }
}

impl fmt::Display for LineOfSightQuaternionOpenError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{} (LineOfSightQuaternion registration: {:?})",
            self.error, self.registration
        )
    }
}

impl std::error::Error for LineOfSightQuaternionOpenError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        Some(&self.error)
    }
}

type Close<T> = unsafe extern "C" fn(*mut *mut T, *mut c_char, usize, *mut usize) -> i32;

/// Unique, independent native metadata consumer, not a borrow of Image/Session.
///
/// Queued values drain before StreamStopped or ProviderFailed. Close discards the
/// remaining queue but neither unregisters callbacks nor closes a parent nor
/// waits for callback quiescence. Native physical teardown must ensure quiescence
/// before provider unload. Drop is best-effort and does not panic. An uncertain
/// release is never retried (fail-safe loss of ownership can leak native state).
///
/// ```compile_fail
/// fn require_send<T: Send>() {}
/// require_send::<ams_mel::LineOfSightQuaternionStream>();
/// ```
/// ```compile_fail
/// fn require_sync<T: Sync>() {}
/// require_sync::<ams_mel::LineOfSightQuaternionStream>();
/// ```
/// ```compile_fail
/// fn require_clone<T: Clone>() {}
/// require_clone::<ams_mel::LineOfSightQuaternionStream>();
/// ```
#[derive(Debug)]
pub struct LineOfSightQuaternionStream {
    raw: *mut sys::AmsMelIrImageLosQuaternionMetadata,
    close_op: Close<sys::AmsMelIrImageLosQuaternionMetadata>,
    _not_send_sync: Rc<()>,
}

impl LineOfSightQuaternionStream {
    pub(crate) fn open_with(
        queue_capacity: usize,
        open: impl FnOnce(
            *mut *mut sys::AmsMelIrImageLosQuaternionMetadata,
            *mut u32,
            *mut c_char,
            usize,
            *mut usize,
        ) -> i32,
    ) -> Result<Self, LineOfSightQuaternionOpenError> {
        if queue_capacity == 0 {
            return Err(LineOfSightQuaternionOpenError::preflight(Error::new(
                ErrorKind::InvalidArgument,
                "LineOfSightQuaternion queue capacity must be nonzero",
            )));
        }
        let mut owner = Self {
            raw: ptr::null_mut(),
            close_op: sys::ams_mel_ir_image_los_quaternion_close,
            _not_send_sync: Rc::new(()),
        };
        // Unpublished sentinel, unchanged when native has no known Return.
        let mut registration = u32::MAX;
        let result =
            call_with_diagnostic(|d, c, r| open(&mut owner.raw, &mut registration, d, c, r));
        owner.finish_open(result, registration)
    }

    fn finish_open(
        self,
        result: Result<(), Error>,
        raw_registration: u32,
    ) -> Result<Self, LineOfSightQuaternionOpenError> {
        let registration = command_return(raw_registration).ok();
        // Drop guards even an unexpectedly published owner on failure.
        result.map_err(|error| LineOfSightQuaternionOpenError {
            error,
            registration,
        })?;
        if self.raw.is_null() || registration != Some(CommandReturn::Success) {
            return Err(LineOfSightQuaternionOpenError {
                error: protocol("LineOfSightQuaternion open succeeded without an owner and successful registration"),
                registration,
            });
        }
        Ok(self)
    }

    /// Local ownership, not callback liveness or parent state.
    pub fn is_open(&self) -> bool {
        !self.raw.is_null()
    }

    /// Exactly one native Receive. Zero polls; other timeouts pass unchanged.
    /// The native event remains guarded through validation/copy and is closed
    /// before returning any owned value, including on conversion error/unwind.
    pub fn receive(&mut self, timeout_ms: u32) -> Result<LineOfSightQuaternion, Error> {
        let raw = self.live()?;
        self.receive_event_with(&EVENT_OPS, |out, d, c, r| {
            // SAFETY: unique live owner, writable guarded output slots.
            unsafe { sys::ams_mel_ir_image_los_quaternion_receive(raw, timeout_ms, out, d, c, r) }
        })
    }

    fn receive_event_with(
        &mut self,
        ops: &'static EventOps,
        receive: impl FnOnce(
            *mut *mut sys::AmsMelIrImageLosQuaternionEvent,
            *mut c_char,
            usize,
            *mut usize,
        ) -> i32,
    ) -> Result<LineOfSightQuaternion, Error> {
        let mut event = NativeEvent {
            raw: ptr::null_mut(),
            ops,
        };
        call_with_diagnostic(|d, c, r| receive(&mut event.raw, d, c, r))?;
        event.copy_with(map_from_raw)
    }

    /// Native saturating callback counts, identical to ordinary Image metadata.
    pub fn counters(&mut self) -> Result<ImageMetadataCounters, Error> {
        let raw = self.live()?;
        self.counters_with(|out, d, c, r| {
            // SAFETY: unique live owner and exact writable counter record.
            unsafe { sys::ams_mel_ir_image_los_quaternion_get_counters(raw, out, d, c, r) }
        })
    }

    fn counters_with(
        &mut self,
        get: impl FnOnce(*mut sys::AmsMelIrC2MetadataCountersV1, *mut c_char, usize, *mut usize) -> i32,
    ) -> Result<ImageMetadataCounters, Error> {
        let mut counters = sys::AmsMelIrC2MetadataCountersV1::default();
        call_with_diagnostic(|d, c, r| get(&mut counters, d, c, r))?;
        Ok(ImageMetadataCounters {
            events_received: counters.events_received,
            events_dropped_queue_full: counters.events_dropped_queue_full,
            malformed_or_unsupported: counters.malformed_or_unsupported,
        })
    }

    /// Idempotent after consumption. One release attempt, even on uncertainty.
    pub fn close(&mut self) -> Result<(), Error> {
        close_owned(&mut self.raw, self.close_op)
    }

    fn live(&self) -> Result<*mut sys::AmsMelIrImageLosQuaternionMetadata, Error> {
        if self.is_open() {
            Ok(self.raw)
        } else {
            Err(Error::new(
                ErrorKind::ProviderFailed,
                "IR LineOfSightQuaternion stream is closed",
            ))
        }
    }
}

impl Drop for LineOfSightQuaternionStream {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            // SAFETY: unique owner; supported null diagnostics, no allocation.
            let _ = unsafe { (self.close_op)(&mut self.raw, ptr::null_mut(), 0, ptr::null_mut()) };
        }
    }
}

fn close_owned<T>(raw: &mut *mut T, close: Close<T>) -> Result<(), Error> {
    if raw.is_null() {
        return Ok(());
    }
    // Forget BEFORE the call: an uncertain release must not be retried by Drop.
    let mut released = std::mem::replace(raw, ptr::null_mut());
    call_with_diagnostic(|d, c, r| {
        // SAFETY: caller uniquely owned this handle; all views have expired.
        unsafe { close(&mut released, d, c, r) }
    })?;
    if !released.is_null() {
        return Err(protocol(
            "LineOfSightQuaternion close succeeded without clearing the handle",
        ));
    }
    Ok(())
}

struct EventOps {
    view: unsafe extern "C" fn(
        *const sys::AmsMelIrImageLosQuaternionEvent,
        *mut *const sys::AmsMelIrLineOfSightQuaternionV1,
        *mut c_char,
        usize,
        *mut usize,
    ) -> i32,
    close: Close<sys::AmsMelIrImageLosQuaternionEvent>,
}

static EVENT_OPS: EventOps = EventOps {
    view: sys::ams_mel_ir_image_los_quaternion_event_view,
    close: sys::ams_mel_ir_image_los_quaternion_event_close,
};

struct NativeEvent {
    raw: *mut sys::AmsMelIrImageLosQuaternionEvent,
    ops: &'static EventOps,
}

impl NativeEvent {
    fn copy_with(
        mut self,
        copy: impl FnOnce(&sys::AmsMelIrLineOfSightQuaternionV1) -> Result<LineOfSightQuaternion, Error>,
    ) -> Result<LineOfSightQuaternion, Error> {
        if self.raw.is_null() {
            return Err(protocol(
                "LineOfSightQuaternion receive succeeded without an event",
            ));
        }
        let mut view = ptr::null();
        call_with_diagnostic(|d, c, r| {
            // SAFETY: event guard owns storage, view output is writable.
            unsafe { (self.ops.view)(self.raw, &mut view, d, c, r) }
        })?;
        // SAFETY: facade provenance; validate the outer record before reading it.
        let record = unsafe { checked_span(view, 1, "LineOfSightQuaternion event view")? };
        let value = copy(&record[0])?;
        close_owned(&mut self.raw, self.ops.close)?;
        Ok(value)
    }
}

impl Drop for NativeEvent {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            // SAFETY: unique event; no public borrowed storage survives cleanup.
            let _ = unsafe { (self.ops.close)(&mut self.raw, ptr::null_mut(), 0, ptr::null_mut()) };
        }
    }
}

// Only live facade storage (or backed unit fixtures) enters this private helper.
fn map_from_raw(
    raw: &sys::AmsMelIrLineOfSightQuaternionV1,
) -> Result<LineOfSightQuaternion, Error> {
    Ok(LineOfSightQuaternion {
        system_time_ns: raw.system_time_ns,
        q_xyzw: Quaternion {
            x: raw.q_xyzw.x,
            y: raw.q_xyzw.y,
            z: raw.q_xyzw.z,
            w: raw.q_xyzw.w,
        },
        pqr_rps: ForwardRightDown {
            forward: raw.pqr_rps.forward,
            right: raw.pqr_rps.right,
            down: raw.pqr_rps.down,
        },
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::{
        cell::Cell,
        panic::{catch_unwind, AssertUnwindSafe},
    };

    fn empty_record() -> sys::AmsMelIrLineOfSightQuaternionV1 {
        sys::AmsMelIrLineOfSightQuaternionV1::default()
    }

    fn assert_protocol<T: fmt::Debug>(result: Result<T, Error>) {
        assert_eq!(
            result.unwrap_err().kind(),
            &ErrorKind::ProtocolInconsistency
        );
    }

    #[test]
    fn full_signed_time_range_and_all_float_fields_are_untransformed() {
        for time in [i64::MIN, i64::MIN + 1, -1, 0, 1, i64::MAX - 1, i64::MAX] {
            for bits in [
                0,
                1 << 63,
                1,
                (1 << 63) | 1,
                f64::MAX.to_bits(),
                (-f64::MAX).to_bits(),
                f64::INFINITY.to_bits(),
                f64::NEG_INFINITY.to_bits(),
                f64::NAN.to_bits(),
            ] {
                let f = f64::from_bits(bits);
                let raw = sys::AmsMelIrLineOfSightQuaternionV1 {
                    system_time_ns: time,
                    q_xyzw: sys::AmsMelIrQuaternionV1 {
                        x: f,
                        y: f,
                        z: f,
                        w: f,
                    },
                    pqr_rps: sys::AmsMelIrForwardRightDownV1 {
                        forward: f,
                        right: f,
                        down: f,
                    },
                };
                let value = map_from_raw(&raw).unwrap();
                assert_eq!(value.system_time_ns, time);
                for actual in [
                    value.q_xyzw.x,
                    value.q_xyzw.y,
                    value.q_xyzw.z,
                    value.q_xyzw.w,
                    value.pqr_rps.forward,
                    value.pqr_rps.right,
                    value.pqr_rps.down,
                ] {
                    if f.is_nan() {
                        assert!(actual.is_nan());
                    } else {
                        assert_eq!(actual.to_bits(), bits);
                    }
                }
            }
        }
    }

    unsafe fn diagnostic(d: *mut c_char, c: usize, r: *mut usize) {
        // SAFETY: test operations receive writable slots from the diagnostic helper.
        unsafe {
            if c > 0 {
                *d = 0;
            }
            if !r.is_null() {
                *r = 1;
            }
        }
    }

    struct FakeEvent {
        view: sys::AmsMelIrLineOfSightQuaternionV1,
        closes: Rc<Cell<usize>>,
        mode: u8,
    }

    unsafe extern "C" fn view(
        raw: *const sys::AmsMelIrImageLosQuaternionEvent,
        out: *mut *const sys::AmsMelIrLineOfSightQuaternionV1,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32 {
        // SAFETY: a live boxed FakeEvent and writable outputs are supplied.
        unsafe {
            diagnostic(d, c, r);
            let fake = &*raw.cast::<FakeEvent>();
            *out = match fake.mode {
                3 => ptr::null(),
                4 => ptr::without_provenance(1),
                _ => &fake.view,
            };
            if fake.mode == 7 {
                *r = 0;
            }
            if fake.mode == 8 {
                *r = 2;
                *d = 0xff_u8 as c_char;
                *d.add(1) = 0;
            }
            if fake.mode == 5 {
                sys::AMS_MEL_PROVIDER_EXCEPTION
            } else {
                sys::AMS_MEL_OK
            }
        }
    }

    unsafe extern "C" fn event_close(
        raw: *mut *mut sys::AmsMelIrImageLosQuaternionEvent,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32 {
        // SAFETY: tests transfer a unique boxed FakeEvent into this operation.
        unsafe {
            diagnostic(d, c, r);
            let fake = Box::from_raw((*raw).cast::<FakeEvent>());
            fake.closes.set(fake.closes.get() + 1);
            let mode = fake.mode;
            // Even failed/uncertain releases free this fake to detect retries.
            if mode != 1 && mode != 2 && mode != 6 {
                *raw = ptr::null_mut();
            }
            if mode == 6 && !r.is_null() {
                *r = 0;
            }
            if mode == 2 {
                sys::AMS_MEL_INTERNAL_ERROR
            } else {
                sys::AMS_MEL_OK
            }
        }
    }

    static OPS: EventOps = EventOps {
        view,
        close: event_close,
    };

    fn event(mode: u8) -> (NativeEvent, Rc<Cell<usize>>) {
        let closes = Rc::new(Cell::new(0));
        let view = empty_record();
        let raw = Box::into_raw(Box::new(FakeEvent {
            view,
            closes: closes.clone(),
            mode,
        }))
        .cast();
        (NativeEvent { raw, ops: &OPS }, closes)
    }

    #[test]
    fn event_guard_cleanup_on_view_conversion_close_failure_and_unwind() {
        for (mode, expected) in [
            (0, None),
            (1, Some(ErrorKind::ProtocolInconsistency)),
            (2, Some(ErrorKind::InternalError)),
            (3, Some(ErrorKind::ProtocolInconsistency)),
            (4, Some(ErrorKind::ProtocolInconsistency)),
            (5, Some(ErrorKind::ProviderException)),
            (6, Some(ErrorKind::ProtocolInconsistency)),
            (7, Some(ErrorKind::ProtocolInconsistency)),
            (8, Some(ErrorKind::InvalidUtf8)),
        ] {
            let (event, closes) = event(mode);
            let result = event.copy_with(map_from_raw);
            if let Some(kind) = expected {
                assert_eq!(result.unwrap_err().kind(), &kind);
            } else {
                assert!(result.is_ok());
            }
            assert_eq!(closes.get(), 1, "mode {mode}: exactly one release");
        }
        let (event, closes) = event(0);
        assert!(catch_unwind(AssertUnwindSafe(
            || event.copy_with(|_| panic!("conversion unwind"))
        ))
        .is_err());
        assert_eq!(closes.get(), 1);
        let (event, closes) = self::event(0);
        assert_protocol(event.copy_with(|_| Err(protocol("conversion rejected"))));
        assert_eq!(closes.get(), 1);
    }

    #[test]
    fn receive_unwind_after_publishing_an_event_releases_once() {
        let (mut stream, _) = fake_stream();
        let (mut event, closes) = event(0);
        assert!(catch_unwind(AssertUnwindSafe(|| {
            let _ = stream.receive_event_with(&OPS, |out, _, _, _| {
                // SAFETY: writable guarded slot, transferring unique fake ownership.
                unsafe {
                    *out = event.raw;
                }
                event.raw = ptr::null_mut();
                panic!("receive unwind after publishing");
            });
        }))
        .is_err());
        assert_eq!(closes.get(), 1);
    }

    unsafe extern "C" fn stream_close(
        raw: *mut *mut sys::AmsMelIrImageLosQuaternionMetadata,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32 {
        // SAFETY: exact boxed counter transferred by fake_stream.
        unsafe {
            diagnostic(d, c, r);
            let counter = Box::from_raw((*raw).cast::<Rc<Cell<usize>>>());
            counter.set(counter.get() + 1);
            *raw = ptr::null_mut();
        }
        sys::AMS_MEL_OK
    }

    fn fake_stream() -> (LineOfSightQuaternionStream, Rc<Cell<usize>>) {
        let closes = Rc::new(Cell::new(0));
        (
            LineOfSightQuaternionStream {
                raw: Box::into_raw(Box::new(closes.clone())).cast(),
                close_op: stream_close,
                _not_send_sync: Rc::new(()),
            },
            closes,
        )
    }

    #[test]
    fn stream_drop_and_idempotent_explicit_close() {
        let (stream, closes) = fake_stream();
        drop(stream);
        assert_eq!(closes.get(), 1);
        let (mut stream, closes) = fake_stream();
        stream.close().unwrap();
        stream.close().unwrap();
        assert!(!stream.is_open());
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
    }

    unsafe extern "C" fn uncertain_stream_close(
        raw: *mut *mut sys::AmsMelIrImageLosQuaternionMetadata,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32 {
        // SAFETY: backed counter retained by the test until all release attempts
        // finish. Deliberately stale output models an uncertain native release.
        unsafe {
            diagnostic(d, c, r);
            let counter = &*(*raw).cast::<Rc<Cell<usize>>>();
            counter.set(counter.get() + 1);
        }
        sys::AMS_MEL_INTERNAL_ERROR
    }

    #[test]
    fn uncertain_stream_close_is_attempted_exactly_once() {
        let (mut stream, closes) = fake_stream();
        let backing = stream.raw;
        stream.close_op = uncertain_stream_close;
        assert_eq!(
            stream.close().unwrap_err().kind(),
            &ErrorKind::InternalError
        );
        let second_close = stream.close();
        let still_open = stream.is_open();
        drop(stream);
        // SAFETY: fake retained its backing to detect retries without dangling
        // pointer access. No operation can use it after the stream is dropped.
        unsafe {
            drop(Box::from_raw(backing.cast::<Rc<Cell<usize>>>()));
        }
        assert_eq!(closes.get(), 1);
        assert_eq!(second_close, Ok(()));
        assert!(!still_open);
    }

    #[test]
    fn receive_once_preserves_status_and_guards_even_error_outputs_and_bad_diagnostics() {
        let (mut stream, _) = fake_stream();
        for (status, kind) in [
            (sys::AMS_MEL_TIMEOUT, ErrorKind::Timeout),
            (sys::AMS_MEL_STREAM_STOPPED, ErrorKind::StreamStopped),
            (sys::AMS_MEL_PROVIDER_FAILED, ErrorKind::ProviderFailed),
            (
                sys::AMS_MEL_PROVIDER_EXCEPTION,
                ErrorKind::ProviderException,
            ),
            (sys::AMS_MEL_INTERNAL_ERROR, ErrorKind::InternalError),
        ] {
            let (mut event, closes) = event(0);
            let calls = Cell::new(0);
            let result = stream.receive_event_with(&OPS, |out, d, c, r| {
                calls.set(calls.get() + 1);
                // SAFETY: writable outputs; ownership passes to Receive's guard.
                unsafe {
                    diagnostic(d, c, r);
                    *out = event.raw;
                }
                event.raw = ptr::null_mut();
                status
            });
            assert_eq!(result.unwrap_err().kind(), &kind);
            assert_eq!(calls.get(), 1);
            assert_eq!(closes.get(), 1);
        }
        for mode in 0..4 {
            let (mut event, closes) = event(0);
            assert_protocol(stream.receive_event_with(&OPS, |out, d, c, r| {
                // SAFETY: writable diagnostics, intentionally malformed values.
                unsafe {
                    diagnostic(d, c, r);
                    *out = event.raw;
                    match mode {
                        0 => *r = 0,
                        1 => {
                            *r = 2;
                            *d = b'x' as c_char;
                            *d.add(1) = b'y' as c_char;
                        }
                        2 => {
                            *r = 2;
                            *d = b'x' as c_char;
                            *d.add(1) = 0;
                        }
                        _ => *r = c + 1,
                    }
                }
                event.raw = ptr::null_mut();
                sys::AMS_MEL_OK
            }));
            assert_eq!(closes.get(), 1);
        }
        assert_protocol(stream.receive_event_with(&OPS, |_, d, c, r| {
            // SAFETY: writable diagnostic slots; deliberately null successful event.
            unsafe {
                diagnostic(d, c, r);
            }
            sys::AMS_MEL_OK
        }));
    }

    #[test]
    fn full_width_counters_do_not_narrow() {
        let (mut stream, _) = fake_stream();
        let value = stream
            .counters_with(|out, d, c, r| {
                // SAFETY: exact writable counter and diagnostic slots.
                unsafe {
                    diagnostic(d, c, r);
                    *out = sys::AmsMelIrC2MetadataCountersV1 {
                        events_received: u64::MAX,
                        events_dropped_queue_full: u64::MAX - 1,
                        malformed_or_unsupported: 1 << 63,
                    };
                }
                sys::AMS_MEL_OK
            })
            .unwrap();
        assert_eq!(value.events_received, u64::MAX);
        assert_eq!(value.events_dropped_queue_full, u64::MAX - 1);
        assert_eq!(value.malformed_or_unsupported, 1 << 63);
    }

    #[test]
    fn invalid_utf8_receive_diagnostic_still_releases_event() {
        let (mut stream, _) = fake_stream();
        let (mut event, closes) = event(0);
        let error = stream
            .receive_event_with(&OPS, |out, d, c, r| {
                // SAFETY: writable outputs; deliberately invalid UTF-8 diagnostic.
                unsafe {
                    diagnostic(d, c, r);
                    *r = 2;
                    *d = 0xff_u8 as c_char;
                    *d.add(1) = 0;
                    *out = event.raw;
                }
                event.raw = ptr::null_mut();
                sys::AMS_MEL_PROVIDER_FAILED
            })
            .unwrap_err();
        assert_eq!(error.kind(), &ErrorKind::InvalidUtf8);
        assert_eq!(closes.get(), 1);
    }

    #[test]
    fn failed_open_preserves_status_and_registration_and_drops_unexpected_owner() {
        for registration in [
            sys::AMS_MEL_IR_RETURN_BAD_POINTER,
            sys::AMS_MEL_IR_RETURN_FAIL,
            sys::AMS_MEL_IR_RETURN_NOT_SUPPORTED,
            sys::AMS_MEL_IR_RETURN_NOT_IMPLEMENTED,
            u32::MAX,
        ] {
            let (stream, closes) = fake_stream();
            let source = Error::new(ErrorKind::ProviderFailed, "original native diagnostic");
            let error = stream
                .finish_open(Err(source.clone()), registration)
                .unwrap_err();
            assert_eq!(error.error(), &source);
            assert_eq!(error.registration(), command_return(registration).ok());
            assert_eq!(closes.get(), 1);
        }
    }

    #[test]
    fn open_validates_outputs_and_local_preflight() {
        for registration in [0, 1, 2, 3, 4, u32::MAX] {
            let (stream, closes) = fake_stream();
            let result = stream.finish_open(Ok(()), registration);
            if command_return(registration).ok() == Some(CommandReturn::Success) {
                drop(result.unwrap());
            } else {
                let error = result.unwrap_err();
                assert_eq!(error.error().kind(), &ErrorKind::ProtocolInconsistency);
                assert_eq!(error.registration(), command_return(registration).ok());
                assert!(std::error::Error::source(&error).is_some());
                assert!(!error.to_string().is_empty());
            }
            assert_eq!(closes.get(), 1);
        }
        let error = LineOfSightQuaternionStream::open_with(1, |_, reg, d, c, r| {
            // SAFETY: writable slots, deliberately no owner on success.
            unsafe {
                diagnostic(d, c, r);
                *reg = sys::AMS_MEL_IR_RETURN_SUCCESS;
            }
            sys::AMS_MEL_OK
        })
        .unwrap_err();
        assert_eq!(error.error().kind(), &ErrorKind::ProtocolInconsistency);
        assert_eq!(
            LineOfSightQuaternionStream::open_with(0, |_, _, _, _, _| panic!("must not register"))
                .unwrap_err()
                .registration(),
            None
        );
        let mut image = crate::ImageStream {
            raw: ptr::null_mut(),
            _not_send_sync: Rc::new(()),
        };
        for capacity in [0, 1] {
            let error = image.open_line_of_sight_quaternion(capacity).unwrap_err();
            assert_eq!(error.error().kind(), &ErrorKind::ProviderFailed);
            assert_eq!(error.registration(), None);
        }
    }
}
