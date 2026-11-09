//! Complete owned reception of the optional Image OpticalDistortionMap callback.
//! Native code owns registration, queueing and callback lifetime. The C facade
//! is the trusted memory-provenance boundary; structural pointer checks cannot
//! prove arbitrary addresses are mapped. Rust adds no callbacks or workers.

use std::{ffi::c_char, fmt, ptr, rc::Rc};

use ams_mel_sys as sys;

use crate::{
    call_with_diagnostic, checked_span, command_return, copy_mapped, protocol, CommandReturn,
    Error, ErrorKind, ImageMetadataCounters,
};

/// Untransformed components in published getter order. No units are inferred.
/// Reusable for other native ForwardRightDown values, without normalization.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ForwardRightDown {
    pub forward: f64,
    pub right: f64,
    pub down: f64,
}

/// Provider coordinates; no origin, rounding or axis conversion is inferred.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct RowCol {
    pub row: f64,
    pub column: f64,
}

/// Every published getter, copied without interpreting dimension relationships.
/// Collections preserve provider order, duplicates and independent lengths.
/// Doubles retain signed zero, subnormals and nonfinite values; NaN payload bits
/// are not promised by native code. Values outlive all native owners and code.
#[derive(Clone, Debug, PartialEq)]
pub struct OpticalDistortionMap {
    pub width: u32,
    pub height: u32,
    pub right_size: u32,
    pub down_size: u32,
    pub pixels_per_odm: u32,
    pub right_min: f64,
    pub right_max: f64,
    pub down_min: f64,
    pub down_max: f64,
    pub frd: Vec<ForwardRightDown>,
    pub row_col: Vec<RowCol>,
}

/// Optional registration failure, preserving native status and diagnostic plus
/// the provider Return only when native code actually published a known value.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct OpticalDistortionMapOpenError {
    error: Error,
    registration: Option<CommandReturn>,
}

impl OpticalDistortionMapOpenError {
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

impl fmt::Display for OpticalDistortionMapOpenError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{} (OpticalDistortionMap registration: {:?})",
            self.error, self.registration
        )
    }
}

impl std::error::Error for OpticalDistortionMapOpenError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        Some(&self.error)
    }
}

type Close<T> = unsafe extern "C" fn(*mut *mut T, *mut c_char, usize, *mut usize) -> i32;

/// Unique, independent native metadata consumer, not a borrow of Image/Session.
///
/// Queued maps drain before StreamStopped or ProviderFailed. Close discards the
/// remaining queue but neither unregisters callbacks nor closes a parent nor
/// waits for callback quiescence. Native physical teardown must ensure quiescence
/// before provider unload. Drop is best-effort and does not panic. An uncertain
/// release is never retried (fail-safe loss of ownership can leak native state).
///
/// ```compile_fail
/// fn require_send<T: Send>() {}
/// require_send::<ams_mel::OpticalDistortionMapStream>();
/// ```
/// ```compile_fail
/// fn require_sync<T: Sync>() {}
/// require_sync::<ams_mel::OpticalDistortionMapStream>();
/// ```
#[derive(Debug)]
pub struct OpticalDistortionMapStream {
    raw: *mut sys::AmsMelIrImageOdmMetadata,
    close_op: Close<sys::AmsMelIrImageOdmMetadata>,
    _not_send_sync: Rc<()>,
}

impl OpticalDistortionMapStream {
    pub(crate) fn open_with(
        queue_capacity: usize,
        open: impl FnOnce(
            *mut *mut sys::AmsMelIrImageOdmMetadata,
            *mut u32,
            *mut c_char,
            usize,
            *mut usize,
        ) -> i32,
    ) -> Result<Self, OpticalDistortionMapOpenError> {
        if queue_capacity == 0 {
            return Err(OpticalDistortionMapOpenError::preflight(Error::new(
                ErrorKind::InvalidArgument,
                "OpticalDistortionMap queue capacity must be nonzero",
            )));
        }
        let mut owner = Self {
            raw: ptr::null_mut(),
            close_op: sys::ams_mel_ir_image_odm_close,
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
    ) -> Result<Self, OpticalDistortionMapOpenError> {
        let registration = command_return(raw_registration).ok();
        // Drop guards even an unexpectedly published owner on failure.
        result.map_err(|error| OpticalDistortionMapOpenError {
            error,
            registration,
        })?;
        if self.raw.is_null() || registration != Some(CommandReturn::Success) {
            return Err(OpticalDistortionMapOpenError {
                error: protocol("OpticalDistortionMap open succeeded without an owner and successful registration"),
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
    pub fn receive(&mut self, timeout_ms: u32) -> Result<OpticalDistortionMap, Error> {
        let raw = self.live()?;
        self.receive_event_with(&EVENT_OPS, |out, d, c, r| {
            // SAFETY: unique live owner, writable guarded output slots.
            unsafe { sys::ams_mel_ir_image_odm_receive(raw, timeout_ms, out, d, c, r) }
        })
    }

    fn receive_event_with(
        &mut self,
        ops: &'static EventOps,
        receive: impl FnOnce(
            *mut *mut sys::AmsMelIrImageOdmEvent,
            *mut c_char,
            usize,
            *mut usize,
        ) -> i32,
    ) -> Result<OpticalDistortionMap, Error> {
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
            unsafe { sys::ams_mel_ir_image_odm_get_counters(raw, out, d, c, r) }
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

    fn live(&self) -> Result<*mut sys::AmsMelIrImageOdmMetadata, Error> {
        if self.is_open() {
            Ok(self.raw)
        } else {
            Err(Error::new(
                ErrorKind::ProviderFailed,
                "IR OpticalDistortionMap stream is closed",
            ))
        }
    }
}

impl Drop for OpticalDistortionMapStream {
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
            "OpticalDistortionMap close succeeded without clearing the handle",
        ));
    }
    Ok(())
}

struct EventOps {
    view: unsafe extern "C" fn(
        *const sys::AmsMelIrImageOdmEvent,
        *mut *const sys::AmsMelIrOpticalDistortionMapV1,
        *mut c_char,
        usize,
        *mut usize,
    ) -> i32,
    close: Close<sys::AmsMelIrImageOdmEvent>,
}

static EVENT_OPS: EventOps = EventOps {
    view: sys::ams_mel_ir_image_odm_event_view,
    close: sys::ams_mel_ir_image_odm_event_close,
};

struct NativeEvent {
    raw: *mut sys::AmsMelIrImageOdmEvent,
    ops: &'static EventOps,
}

impl NativeEvent {
    fn copy_with(
        mut self,
        copy: impl FnOnce(&sys::AmsMelIrOpticalDistortionMapV1) -> Result<OpticalDistortionMap, Error>,
    ) -> Result<OpticalDistortionMap, Error> {
        if self.raw.is_null() {
            return Err(protocol(
                "OpticalDistortionMap receive succeeded without an event",
            ));
        }
        let mut view = ptr::null();
        call_with_diagnostic(|d, c, r| {
            // SAFETY: event guard owns storage, view output is writable.
            unsafe { (self.ops.view)(self.raw, &mut view, d, c, r) }
        })?;
        // SAFETY: facade provenance; validate the outer record before reading it.
        let record = unsafe { checked_span(view, 1, "OpticalDistortionMap event view")? };
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
fn map_from_raw(raw: &sys::AmsMelIrOpticalDistortionMapV1) -> Result<OpticalDistortionMap, Error> {
    // SAFETY: native event stays alive; both spans checked before copying either.
    let frd = unsafe { checked_span(raw.frd.data, raw.frd.size, "OpticalDistortionMap FRD")? };
    // SAFETY: same immutable event lifetime and trusted native provenance.
    let row_col = unsafe {
        checked_span(
            raw.row_col.data,
            raw.row_col.size,
            "OpticalDistortionMap RowCol",
        )?
    };
    Ok(OpticalDistortionMap {
        width: raw.width,
        height: raw.height,
        right_size: raw.right_size,
        down_size: raw.down_size,
        pixels_per_odm: raw.pixels_per_odm,
        right_min: raw.right_min,
        right_max: raw.right_max,
        down_min: raw.down_min,
        down_max: raw.down_max,
        frd: copy_mapped(frd, "OpticalDistortionMap FRD", |v| {
            Ok(ForwardRightDown {
                forward: v.forward,
                right: v.right,
                down: v.down,
            })
        })?,
        row_col: copy_mapped(row_col, "OpticalDistortionMap RowCol", |v| {
            Ok(RowCol {
                row: v.row,
                column: v.column,
            })
        })?,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::{
        alloc::{GlobalAlloc, Layout, System},
        cell::Cell,
        panic::{catch_unwind, AssertUnwindSafe},
    };

    thread_local! { static FAIL_SIZE: Cell<usize> = const { Cell::new(0) }; }
    struct Allocator;
    // SAFETY: all successful allocations and every deallocation use System.
    // The thread-local one-shot rejects only the deliberately selected layout.
    unsafe impl GlobalAlloc for Allocator {
        unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
            let fail = FAIL_SIZE
                .try_with(|v| {
                    if v.get() == layout.size() {
                        v.set(0);
                        true
                    } else {
                        false
                    }
                })
                .unwrap_or(false);
            if fail {
                ptr::null_mut()
            } else {
                // SAFETY: forward the unchanged allocator contract.
                unsafe { System.alloc(layout) }
            }
        }
        unsafe fn dealloc(&self, p: *mut u8, layout: Layout) {
            // SAFETY: every non-null allocation came from System.
            unsafe { System.dealloc(p, layout) }
        }
    }
    #[global_allocator]
    static ALLOCATOR: Allocator = Allocator;

    fn empty_record() -> sys::AmsMelIrOpticalDistortionMapV1 {
        // SAFETY: this C record contains only scalars and nullable raw-pointer spans.
        unsafe { std::mem::zeroed() }
    }

    fn assert_protocol<T: fmt::Debug>(result: Result<T, Error>) {
        assert_eq!(
            result.unwrap_err().kind(),
            &ErrorKind::ProtocolInconsistency
        );
    }

    #[test]
    fn every_scalar_preserves_full_width_and_double_bits() {
        for value in [0, 1, 0x80000001, u32::MAX] {
            let mut raw = empty_record();
            raw.width = value;
            raw.height = value;
            raw.right_size = value;
            raw.down_size = value;
            raw.pixels_per_odm = value;
            let map = map_from_raw(&raw).unwrap();
            assert_eq!(
                [
                    map.width,
                    map.height,
                    map.right_size,
                    map.down_size,
                    map.pixels_per_odm
                ],
                [value; 5]
            );
        }
        for bits in [
            0,
            1,
            1 << 63,
            f64::MAX.to_bits(),
            f64::INFINITY.to_bits(),
            f64::NEG_INFINITY.to_bits(),
            f64::NAN.to_bits(),
        ] {
            let value = f64::from_bits(bits);
            let frd = [sys::AmsMelIrForwardRightDownV1 {
                forward: value,
                right: value,
                down: value,
            }];
            let rc = [sys::AmsMelIrRowColV1 {
                row: value,
                column: value,
            }];
            let mut raw = empty_record();
            raw.right_min = value;
            raw.right_max = value;
            raw.down_min = value;
            raw.down_max = value;
            raw.frd = sys::AmsMelIrForwardRightDownSpanV1 {
                data: frd.as_ptr(),
                size: 1,
            };
            raw.row_col = sys::AmsMelIrRowColSpanV1 {
                data: rc.as_ptr(),
                size: 1,
            };
            let map = map_from_raw(&raw).unwrap();
            for actual in [
                map.right_min,
                map.right_max,
                map.down_min,
                map.down_max,
                map.frd[0].forward,
                map.frd[0].right,
                map.frd[0].down,
                map.row_col[0].row,
                map.row_col[0].column,
            ] {
                if value.is_nan() {
                    assert!(actual.is_nan());
                } else {
                    assert_eq!(actual.to_bits(), bits);
                }
            }
        }
    }

    #[test]
    fn spans_reject_null_misalignment_and_impossible_lengths_before_dereference() {
        let frd = [sys::AmsMelIrForwardRightDownV1::default(); 2];
        let rc = [sys::AmsMelIrRowColV1::default(); 3];
        for (data, size) in [
            (ptr::null(), 1),
            (ptr::without_provenance(1), 1),
            (frd.as_ptr(), usize::MAX),
            (
                frd.as_ptr(),
                isize::MAX as usize / std::mem::size_of_val(&frd[0]) + 1,
            ),
        ] {
            let mut raw = empty_record();
            raw.frd = sys::AmsMelIrForwardRightDownSpanV1 { data, size };
            assert_protocol(map_from_raw(&raw));
        }
        for (data, size) in [
            (ptr::null(), 1),
            (ptr::without_provenance(1), 1),
            (rc.as_ptr(), usize::MAX),
            (
                rc.as_ptr(),
                isize::MAX as usize / std::mem::size_of_val(&rc[0]) + 1,
            ),
        ] {
            let mut raw = empty_record();
            raw.row_col = sys::AmsMelIrRowColSpanV1 { data, size };
            assert_protocol(map_from_raw(&raw));
        }
        let mut raw = empty_record();
        // Empty spans need no valid pointer, including misaligned sentinels.
        raw.frd.data = ptr::without_provenance(1);
        raw.row_col.data = ptr::without_provenance(1);
        assert!(map_from_raw(&raw).unwrap().frd.is_empty());
        raw.frd = sys::AmsMelIrForwardRightDownSpanV1 {
            data: frd.as_ptr(),
            size: 2,
        };
        raw.row_col = sys::AmsMelIrRowColSpanV1 {
            data: rc.as_ptr(),
            size: 3,
        };
        let value = map_from_raw(&raw).unwrap();
        assert_eq!((value.frd.len(), value.row_col.len()), (2, 3));
        assert_eq!(value.row_col[0], value.row_col[2]);
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
        view: sys::AmsMelIrOpticalDistortionMapV1,
        closes: Rc<Cell<usize>>,
        mode: u8,
    }

    unsafe extern "C" fn view(
        raw: *const sys::AmsMelIrImageOdmEvent,
        out: *mut *const sys::AmsMelIrOpticalDistortionMapV1,
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
            if fake.mode == 5 {
                sys::AMS_MEL_PROVIDER_EXCEPTION
            } else {
                sys::AMS_MEL_OK
            }
        }
    }

    unsafe extern "C" fn event_close(
        raw: *mut *mut sys::AmsMelIrImageOdmEvent,
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
        let mut view = empty_record();
        if mode == 8 {
            view.row_col.size = 1;
        }
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
            (8, Some(ErrorKind::ProtocolInconsistency)),
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
    }

    #[test]
    fn allocation_failure_in_either_collection_releases_event_and_recovers() {
        let frd = [sys::AmsMelIrForwardRightDownV1::default(); 4];
        let rc = [sys::AmsMelIrRowColV1::default(); 3];
        for size in [std::mem::size_of_val(&frd), std::mem::size_of_val(&rc)] {
            let (event, closes) = event(0);
            // SAFETY: private boxed fake, not currently borrowed or shared.
            unsafe {
                let raw = &mut *event.raw.cast::<FakeEvent>();
                raw.view.frd = sys::AmsMelIrForwardRightDownSpanV1 {
                    data: frd.as_ptr(),
                    size: 4,
                };
                raw.view.row_col = sys::AmsMelIrRowColSpanV1 {
                    data: rc.as_ptr(),
                    size: 3,
                };
            }
            FAIL_SIZE.with(|v| v.set(size));
            let result = event.copy_with(map_from_raw);
            let remaining = FAIL_SIZE.with(|v| v.replace(0));
            assert_eq!(remaining, 0, "selected allocation must actually fail");
            assert_eq!(result.unwrap_err().kind(), &ErrorKind::InternalError);
            assert_eq!(closes.get(), 1);
            let (event, closes) = self::event(0);
            assert!(event.copy_with(map_from_raw).is_ok());
            assert_eq!(closes.get(), 1);
        }
    }

    unsafe extern "C" fn stream_close(
        raw: *mut *mut sys::AmsMelIrImageOdmMetadata,
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

    fn fake_stream() -> (OpticalDistortionMapStream, Rc<Cell<usize>>) {
        let closes = Rc::new(Cell::new(0));
        (
            OpticalDistortionMapStream {
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
        raw: *mut *mut sys::AmsMelIrImageOdmMetadata,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32 {
        // SAFETY: same fake ownership as stream_close, deliberately stale output.
        unsafe {
            diagnostic(d, c, r);
            let counter = Box::from_raw((*raw).cast::<Rc<Cell<usize>>>());
            counter.set(counter.get() + 1);
        }
        sys::AMS_MEL_INTERNAL_ERROR
    }

    #[test]
    fn uncertain_stream_close_is_attempted_exactly_once() {
        let (mut stream, closes) = fake_stream();
        stream.close_op = uncertain_stream_close;
        assert_eq!(
            stream.close().unwrap_err().kind(),
            &ErrorKind::InternalError
        );
        stream.close().unwrap();
        assert!(!stream.is_open());
        drop(stream);
        assert_eq!(closes.get(), 1);
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
        for registration in [0, 1, 2, 3, u32::MAX] {
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
        let error = OpticalDistortionMapStream::open_with(1, |_, reg, d, c, r| {
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
            OpticalDistortionMapStream::open_with(0, |_, _, _, _, _| panic!("must not register"))
                .unwrap_err()
                .registration(),
            None
        );
        let mut image = crate::ImageStream {
            raw: ptr::null_mut(),
            _not_send_sync: Rc::new(()),
        };
        for capacity in [0, 1] {
            let error = image.open_optical_distortion_map(capacity).unwrap_err();
            assert_eq!(error.error().kind(), &ErrorKind::ProviderFailed);
            assert_eq!(error.registration(), None);
        }
    }
}
