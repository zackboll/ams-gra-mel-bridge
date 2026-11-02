//! Opt-in Image/Health NUC_TempData reception over the seven existing C exports.
//!
//! The native facade owns registration, callbacks, bounded drop-incoming queues,
//! channel teardown and callback quiescence. Rust only polls and copies immutable
//! events. No returned value borrows native storage. The upstream private optional
//! `tempError` has no public accessor: it is deliberately not represented here;
//! its absence from this API does not imply absence in the provider.

use std::{ffi::c_char, fmt, ptr, rc::Rc};

use ams_mel_sys as sys;

use crate::{
    band_type, call_with_diagnostic, checked_span, command_return, copy_mapped, protocol, BandInfo,
    BandType, CommandReturn, Error, ErrorKind,
};

/// Original upstream PixelValue alternative; width is never inferred from magnitude.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum NucPixelValue {
    U8(u8),
    U16(u16),
    U32(u32),
}

/// Exactly the three published TempPointType values (0, 1, 2).
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum NucPointType {
    NucPoint,
    MfaEstimate,
    Other,
}

/// Every publicly readable upstream TempPoint field, owned without normalization.
/// The inaccessible private optional `tempError` is not read or inferred.
#[derive(Clone, Debug, PartialEq)]
pub struct NucTempPoint {
    /// Exact signed nanoseconds; upstream clock and epoch are unspecified.
    pub timestamp_ns: i64,
    /// Band kind and vacuum wavelength limits in meters.
    pub frame_band: BandInfo,
    pub pixel: NucPixelValue,
    /// Exact Kelvin double, including nonfinite values and signed zero.
    pub temperature_k: f64,
    pub point_type: NucPointType,
}

/// Complete ordered point vector, preserving duplicates and empty messages.
#[derive(Clone, Debug, PartialEq)]
pub struct NucTempData {
    pub points: Vec<NucTempPoint>,
}

/// Native saturating callback counters (not Rust Receive call counts).
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct NucCounters {
    pub events_received: u64,
    pub events_dropped_queue_full: u64,
    /// Includes callback allocation failures; these do not fail the stream.
    pub malformed_or_unsupported: u64,
}

/// Failed optional registration, retaining both native status and, only when
/// written by the native facade, the mapped provider Return. No text parsing.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct NucOpenError {
    error: Error,
    registration: Option<CommandReturn>,
}

impl NucOpenError {
    pub(crate) fn preflight(error: Error) -> Self {
        Self {
            error,
            registration: None,
        }
    }

    pub fn error(&self) -> &Error {
        &self.error
    }

    pub fn registration(&self) -> Option<CommandReturn> {
        self.registration
    }
}

impl fmt::Display for NucOpenError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{} (NUC registration: {:?})",
            self.error, self.registration
        )
    }
}

impl std::error::Error for NucOpenError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        Some(&self.error)
    }
}

type Close<T> = unsafe extern "C" fn(*mut *mut T, *mut c_char, usize, *mut usize) -> i32;

/// Unique native NUC owner, shared by Image and Health registration.
///
/// Closing deactivates consumption, but does not unregister callbacks or promise
/// quiescence. Native channel teardown retains callback state and provider code
/// until safe release. Image consumption stops with its stream; Health stops on
/// channel destruction (uncertain detach can report ProviderFailed). Queued
/// events remain drainable before terminal status. The owner may outlive its
/// channel and Session. Drop attempts Close without panicking.
///
/// ```compile_fail
/// fn require_send<T: Send>() {}
/// require_send::<ams_mel::NucStream>();
/// ```
/// ```compile_fail
/// fn require_sync<T: Sync>() {}
/// require_sync::<ams_mel::NucStream>();
/// ```
#[derive(Debug)]
pub struct NucStream {
    raw: *mut sys::AmsMelIrNucMetadata,
    close_op: Close<sys::AmsMelIrNucMetadata>,
    _not_send_sync: Rc<()>,
}

impl NucStream {
    /// The caller borrows a typed channel exclusively for this one native call.
    pub(crate) fn open_with(
        queue_capacity: usize,
        open: impl FnOnce(
            *mut *mut sys::AmsMelIrNucMetadata,
            *mut u32,
            *mut c_char,
            usize,
            *mut usize,
        ) -> i32,
    ) -> Result<Self, NucOpenError> {
        let mut owner = Self {
            raw: ptr::null_mut(),
            close_op: sys::ams_mel_ir_nuc_close,
            _not_send_sync: Rc::new(()),
        };
        if queue_capacity == 0 {
            return Err(NucOpenError {
                error: Error::new(
                    ErrorKind::InvalidArgument,
                    "NUC queue capacity must be nonzero",
                ),
                registration: None,
            });
        }
        // Unpublished sentinel: native leaves this unchanged if no mapped
        // provider Return exists, notably when registration throws.
        let mut registration = u32::MAX;
        let result =
            call_with_diagnostic(|d, c, r| open(&mut owner.raw, &mut registration, d, c, r));
        owner.finish_open(result, registration)
    }

    fn finish_open(
        self,
        result: Result<(), Error>,
        raw_registration: u32,
    ) -> Result<Self, NucOpenError> {
        let registration = command_return(raw_registration).ok();
        // On any failure, this unique owner's Drop closes even an unexpectedly
        // published handle. Preserve the original native status category.
        result.map_err(|error| NucOpenError {
            error,
            registration,
        })?;
        if self.raw.is_null() || registration != Some(CommandReturn::Success) {
            return Err(NucOpenError {
                error: protocol("NUC open succeeded without an owner and successful registration"),
                registration,
            });
        }
        Ok(self)
    }

    /// Local handle ownership only, not callback/provider liveness.
    pub fn is_open(&self) -> bool {
        !self.raw.is_null()
    }

    /// Receives exactly once; zero polls. Preserves native Timeout,
    /// StreamStopped, ProviderFailed and other categories. Validates and copies
    /// every point while an event guard is alive, then closes the event before
    /// returning. Conversion failure or unwind also releases the event.
    pub fn receive(&mut self, timeout_ms: u32) -> Result<NucTempData, Error> {
        let raw = self.live()?;
        self.receive_event_with(&EVENT_OPS, |out, d, c, r| {
            // SAFETY: live uniquely borrowed owner; event output is guarded.
            unsafe { sys::ams_mel_ir_nuc_receive(raw, timeout_ms, out, d, c, r) }
        })
    }

    fn receive_event_with(
        &mut self,
        ops: &'static EventOps,
        receive: impl FnOnce(*mut *mut sys::AmsMelIrNucEvent, *mut c_char, usize, *mut usize) -> i32,
    ) -> Result<NucTempData, Error> {
        let mut event = NativeEvent {
            raw: ptr::null_mut(),
            ops,
        };
        // FnOnce and the diagnostic helper guarantee a single call: never
        // retry a dequeue, including when diagnostics are malformed.
        call_with_diagnostic(|d, c, r| receive(&mut event.raw, d, c, r))?;
        event.into_owned()
    }

    pub fn counters(&mut self) -> Result<NucCounters, Error> {
        let raw = self.live()?;
        let mut counters = sys::AmsMelIrC2MetadataCountersV1::default();
        call_with_diagnostic(|d, c, r| {
            // SAFETY: live owner and exact writable counter record.
            unsafe { sys::ams_mel_ir_nuc_get_counters(raw, &mut counters, d, c, r) }
        })?;
        Ok(NucCounters {
            events_received: counters.events_received,
            events_dropped_queue_full: counters.events_dropped_queue_full,
            malformed_or_unsupported: counters.malformed_or_unsupported,
        })
    }

    /// Idempotent public Close; no callback unregistration or parent closure.
    pub fn close(&mut self) -> Result<(), Error> {
        close_owned(&mut self.raw, self.close_op)
    }

    fn live(&self) -> Result<*mut sys::AmsMelIrNucMetadata, Error> {
        if self.raw.is_null() {
            Err(Error::new(
                ErrorKind::ProviderFailed,
                "IR NUC stream is closed",
            ))
        } else {
            Ok(self.raw)
        }
    }
}

impl Drop for NucStream {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            // SAFETY: unique owner; null diagnostics are supported. No panic
            // or diagnostic allocation on this best-effort cleanup path.
            let _ = unsafe { (self.close_op)(&mut self.raw, ptr::null_mut(), 0, ptr::null_mut()) };
        }
    }
}

fn close_owned<T>(raw: &mut *mut T, close: Close<T>) -> Result<(), Error> {
    if raw.is_null() {
        return Ok(());
    }
    let mut status = sys::AMS_MEL_INTERNAL_ERROR;
    let result = call_with_diagnostic(|d, c, r| {
        // SAFETY: caller owns the unique handle and no view is used afterwards.
        status = unsafe { close(raw, d, c, r) };
        status
    });
    if status == sys::AMS_MEL_OK && !raw.is_null() {
        // Native claims release: forget the stale handle even if diagnostics
        // were malformed, rather than risking a second free from Drop.
        *raw = ptr::null_mut();
        return Err(protocol("NUC close succeeded without clearing the handle"));
    }
    result
}

struct EventOps {
    view: unsafe extern "C" fn(
        *const sys::AmsMelIrNucEvent,
        *mut *const sys::AmsMelIrNucTempDataEventV1,
        *mut c_char,
        usize,
        *mut usize,
    ) -> i32,
    close: Close<sys::AmsMelIrNucEvent>,
}

static EVENT_OPS: EventOps = EventOps {
    view: sys::ams_mel_ir_nuc_event_view,
    close: sys::ams_mel_ir_nuc_event_close,
};

struct NativeEvent {
    raw: *mut sys::AmsMelIrNucEvent,
    ops: &'static EventOps,
}

impl NativeEvent {
    fn into_owned(self) -> Result<NucTempData, Error> {
        self.copy_with(data_from_raw)
    }

    fn copy_with(
        mut self,
        copy: impl FnOnce(&sys::AmsMelIrNucTempDataEventV1) -> Result<NucTempData, Error>,
    ) -> Result<NucTempData, Error> {
        if self.raw.is_null() {
            return Err(protocol("NUC receive succeeded without an event"));
        }
        let mut view = ptr::null();
        call_with_diagnostic(|d, c, r| {
            // SAFETY: guard keeps event alive; view output slot is writable.
            unsafe { (self.ops.view)(self.raw, &mut view, d, c, r) }
        })?;
        // SAFETY: native immutable event storage stays alive through conversion.
        // Check the outer record before dereference, then all of its point span.
        let record = unsafe { checked_span(view, 1, "NUC event record")? };
        let value = copy(&record[0])?;
        close_owned(&mut self.raw, self.ops.close)?;
        Ok(value)
    }
}

impl Drop for NativeEvent {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            // SAFETY: unique event handle; no borrowed values survive cleanup.
            let _ = unsafe { (self.ops.close)(&mut self.raw, ptr::null_mut(), 0, ptr::null_mut()) };
        }
    }
}

// Called only with immutable records from a live event, or synthetically backed
// unit-test records. Native C is the trusted allocation/provenance boundary.
fn data_from_raw(raw: &sys::AmsMelIrNucTempDataEventV1) -> Result<NucTempData, Error> {
    // SAFETY: caller keeps native storage alive. Validation precedes slicing.
    let points =
        unsafe { checked_span(raw.temp_points.data, raw.temp_points.size, "NUC TempPoints")? };
    Ok(NucTempData {
        points: copy_mapped(points, "NUC TempPoints", point_from_raw)?,
    })
}

fn point_from_raw(raw: &sys::AmsMelIrTempPointV1) -> Result<NucTempPoint, Error> {
    let kind = band_type(raw.frame_band.kind);
    if matches!(kind, BandType::Unknown(_)) {
        return Err(protocol("NUC band type is unpublished"));
    }
    let pixel = match (raw.pixel_value.kind, raw.pixel_value.value) {
        // abi.h: AMS_MEL_IR_PIXEL_VALUE_UINT8/UINT16/UINT32 = 1/2/3.
        (1, value) if value <= u8::MAX.into() => NucPixelValue::U8(value as u8),
        (2, value) if value <= u16::MAX.into() => NucPixelValue::U16(value as u16),
        (3, value) => NucPixelValue::U32(value),
        _ => {
            return Err(protocol(
                "NUC pixel variant or width/value combination is invalid",
            ))
        }
    };
    let point_type = match raw.point_type {
        // abi.h: AMS_MEL_IR_TEMP_POINT_NUC_POINT/MFA_ESTIMATE/OTHER = 0/1/2.
        0 => NucPointType::NucPoint,
        1 => NucPointType::MfaEstimate,
        2 => NucPointType::Other,
        _ => return Err(protocol("NUC point type is unpublished")),
    };
    Ok(NucTempPoint {
        timestamp_ns: raw.timestamp_ns,
        frame_band: BandInfo {
            kind,
            min_wavelength_m: raw.frame_band.min_wavelength_m,
            max_wavelength_m: raw.frame_band.max_wavelength_m,
        },
        pixel,
        temperature_k: raw.temperature_k,
        point_type,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::{
        cell::Cell,
        panic::{catch_unwind, AssertUnwindSafe},
    };

    fn point() -> sys::AmsMelIrTempPointV1 {
        sys::AmsMelIrTempPointV1 {
            timestamp_ns: -123,
            frame_band: sys::AmsMelIrBandInfoV1 {
                kind: 5,
                min_wavelength_m: 3.0e-6,
                max_wavelength_m: 5.0e-6,
            },
            pixel_value: sys::AmsMelIrPixelValueV1 {
                kind: 1,
                value: 255,
            },
            temperature_k: 300.25,
            point_type: 0,
        }
    }

    fn assert_protocol<T: fmt::Debug>(result: Result<T, Error>) {
        assert_eq!(
            result.unwrap_err().kind(),
            &ErrorKind::ProtocolInconsistency
        );
    }

    #[test]
    fn pixel_widths_bounds_and_unknowns() {
        let mut raw = point();
        for (kind, value, expected) in [
            (1, 0, NucPixelValue::U8(0)),
            (1, 255, NucPixelValue::U8(255)),
            (2, 0, NucPixelValue::U16(0)),
            (2, 255, NucPixelValue::U16(255)),
            (2, 65535, NucPixelValue::U16(65535)),
            (3, 0, NucPixelValue::U32(0)),
            (3, 255, NucPixelValue::U32(255)),
            (3, u32::MAX, NucPixelValue::U32(u32::MAX)),
        ] {
            raw.pixel_value = sys::AmsMelIrPixelValueV1 { kind, value };
            assert_eq!(point_from_raw(&raw).unwrap().pixel, expected);
        }
        for (kind, value) in [
            (0, 0),
            (4, 0),
            (u32::MAX, 0),
            (1, 256),
            (1, u32::MAX),
            (2, 65536),
            (2, u32::MAX),
        ] {
            raw.pixel_value = sys::AmsMelIrPixelValueV1 { kind, value };
            assert_protocol(point_from_raw(&raw));
        }
    }

    #[test]
    fn every_band_and_point_type_is_strictly_mapped() {
        let mut raw = point();
        let bands = [
            BandType::Invalid,
            BandType::Multiband,
            BandType::IrFar,
            BandType::IrNear,
            BandType::IrLongwave,
            BandType::IrMidwave,
            BandType::IrShortwave,
            BandType::VisibleWhite,
            BandType::VisibleRed,
            BandType::VisibleGreen,
            BandType::VisibleBlue,
            BandType::Uva,
            BandType::Uvb,
            BandType::Uvc,
            BandType::UvVacuum,
        ];
        for (kind, expected) in bands.into_iter().enumerate() {
            raw.frame_band.kind = kind as u32;
            assert_eq!(point_from_raw(&raw).unwrap().frame_band.kind, expected);
        }
        for kind in [15, u32::MAX] {
            raw.frame_band.kind = kind;
            assert_protocol(point_from_raw(&raw));
        }
        raw = point();
        for (kind, expected) in [
            NucPointType::NucPoint,
            NucPointType::MfaEstimate,
            NucPointType::Other,
        ]
        .into_iter()
        .enumerate()
        {
            raw.point_type = kind as u32;
            assert_eq!(point_from_raw(&raw).unwrap().point_type, expected);
        }
        for kind in [3, u32::MAX] {
            raw.point_type = kind;
            assert_protocol(point_from_raw(&raw));
        }
    }

    #[test]
    fn exact_numeric_mapping_extremes_bits_order_and_duplicates() {
        let mut raw = point();
        for timestamp in [i64::MIN, -1, 0, 42, i64::MAX] {
            raw.timestamp_ns = timestamp;
            assert_eq!(point_from_raw(&raw).unwrap().timestamp_ns, timestamp);
        }
        for bits in [
            0,
            1_u64 << 63,
            f64::INFINITY.to_bits(),
            f64::NEG_INFINITY.to_bits(),
            0x7ff8_1234_5678_9abc,
            0xfff8_1234_5678_9abc,
            0x7ff0_0000_0000_0001,
        ] {
            raw.temperature_k = f64::from_bits(bits);
            raw.frame_band.min_wavelength_m = f64::from_bits(bits);
            raw.frame_band.max_wavelength_m = f64::from_bits(bits ^ (1 << 63));
            let value = point_from_raw(&raw).unwrap();
            assert_eq!(value.temperature_k.to_bits(), bits);
            assert_eq!(value.frame_band.min_wavelength_m.to_bits(), bits);
            assert_eq!(
                value.frame_band.max_wavelength_m.to_bits(),
                bits ^ (1 << 63)
            );
        }
        raw = point();
        let expected = NucTempPoint {
            timestamp_ns: -123,
            frame_band: BandInfo {
                kind: BandType::IrMidwave,
                min_wavelength_m: 3.0e-6,
                max_wavelength_m: 5.0e-6,
            },
            pixel: NucPixelValue::U8(255),
            temperature_k: 300.25,
            point_type: NucPointType::NucPoint,
        };
        assert_eq!(point_from_raw(&raw).unwrap(), expected);
        let mut swapped = raw;
        std::mem::swap(
            &mut swapped.frame_band.min_wavelength_m,
            &mut swapped.frame_band.max_wavelength_m,
        );
        assert_ne!(point_from_raw(&swapped).unwrap(), expected);
        std::mem::swap(
            &mut swapped.temperature_k,
            &mut swapped.frame_band.min_wavelength_m,
        );
        assert_ne!(point_from_raw(&swapped).unwrap(), expected);
        let mut second = raw;
        second.timestamp_ns = 9;
        let points = [raw, second, raw];
        let record = sys::AmsMelIrNucTempDataEventV1 {
            temp_points: sys::AmsMelIrTempPointSpanV1 {
                data: points.as_ptr(),
                size: points.len(),
            },
        };
        let value = data_from_raw(&record).unwrap();
        assert_eq!(
            value.points,
            vec![expected.clone(), point_from_raw(&second).unwrap(), expected]
        );
    }

    #[test]
    fn spans_are_validated_before_slicing() {
        let points = [point(); 2];
        for (data, size, valid) in [
            (ptr::null(), 0, true),
            (points.as_ptr(), 0, true),
            (points.as_ptr(), 2, true),
            (ptr::null(), 1, false),
            (
                points.as_ptr().cast::<u8>().wrapping_add(1).cast(),
                1,
                false,
            ),
            (points.as_ptr(), usize::MAX, false),
            (
                points.as_ptr(),
                isize::MAX as usize / std::mem::size_of_val(&points[0]) + 1,
                false,
            ),
        ] {
            let raw = sys::AmsMelIrNucTempDataEventV1 {
                temp_points: sys::AmsMelIrTempPointSpanV1 { data, size },
            };
            let result = data_from_raw(&raw);
            if valid {
                assert_eq!(result.unwrap().points.len(), size);
            } else {
                assert_protocol(result);
            }
        }
    }

    struct FakeEvent {
        record: sys::AmsMelIrNucTempDataEventV1,
        _points: Vec<sys::AmsMelIrTempPointV1>,
        closes: Rc<Cell<usize>>,
        mode: u8,
    }

    unsafe fn diagnostic(d: *mut c_char, c: usize, r: *mut usize) {
        if !r.is_null() {
            unsafe { *r = 1 };
        }
        if !d.is_null() && c > 0 {
            unsafe { *d = 0 };
        }
    }

    unsafe extern "C" fn fake_view(
        raw: *const sys::AmsMelIrNucEvent,
        out: *mut *const sys::AmsMelIrNucTempDataEventV1,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32 {
        // SAFETY: tests supply a live boxed FakeEvent and writable output.
        unsafe {
            diagnostic(d, c, r);
            let fake = &*raw.cast::<FakeEvent>();
            *out = match fake.mode {
                3 => ptr::null(),
                4 => (&fake.record as *const sys::AmsMelIrNucTempDataEventV1)
                    .cast::<u8>()
                    .add(1)
                    .cast(),
                _ => &fake.record,
            };
            if fake.mode == 5 {
                sys::AMS_MEL_PROVIDER_FAILED
            } else {
                sys::AMS_MEL_OK
            }
        }
    }

    unsafe extern "C" fn fake_event_close(
        raw: *mut *mut sys::AmsMelIrNucEvent,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32 {
        // SAFETY: test owns this box exclusively; modes model native responses.
        unsafe {
            diagnostic(d, c, r);
            let fake = &mut *(*raw).cast::<FakeEvent>();
            fake.closes.set(fake.closes.get() + 1);
            if fake.mode == 2 {
                fake.mode = 0;
                return sys::AMS_MEL_INTERNAL_ERROR;
            }
            let stale = fake.mode == 1 || fake.mode == 6;
            if fake.mode == 6 && !r.is_null() {
                *r = 0;
            }
            drop(Box::from_raw((*raw).cast::<FakeEvent>()));
            if !stale {
                *raw = ptr::null_mut();
            }
            sys::AMS_MEL_OK
        }
    }

    static FAKE_OPS: EventOps = EventOps {
        view: fake_view,
        close: fake_event_close,
    };

    fn fake_event(
        points: Vec<sys::AmsMelIrTempPointV1>,
        mode: u8,
    ) -> (NativeEvent, Rc<Cell<usize>>) {
        let closes = Rc::new(Cell::new(0));
        let fake = Box::new(FakeEvent {
            record: sys::AmsMelIrNucTempDataEventV1 {
                temp_points: sys::AmsMelIrTempPointSpanV1 {
                    data: points.as_ptr(),
                    size: points.len(),
                },
            },
            _points: points,
            closes: closes.clone(),
            mode,
        });
        (
            NativeEvent {
                raw: Box::into_raw(fake).cast(),
                ops: &FAKE_OPS,
            },
            closes,
        )
    }

    #[test]
    fn event_lifetime_success_conversion_error_and_unwind() {
        let (event, closes) = fake_event(vec![point()], 0);
        let value = event
            .copy_with(|raw| {
                assert_eq!(closes.get(), 0, "event must be alive during conversion");
                data_from_raw(raw)
            })
            .unwrap();
        assert_eq!(closes.get(), 1, "event must close before return");
        assert_eq!(value.points[0].timestamp_ns, -123);
        let mut invalid = point();
        invalid.point_type = 99;
        let (event, closes) = fake_event(vec![point(), invalid], 0);
        assert_protocol(event.into_owned());
        assert_eq!(closes.get(), 1);
        let (event, closes) = fake_event(vec![point()], 0);
        assert!(catch_unwind(AssertUnwindSafe(
            || event.copy_with(|_| panic!("conversion unwind"))
        ))
        .is_err());
        assert_eq!(closes.get(), 1);
    }

    #[test]
    fn event_missing_outer_view_and_malformed_close_results() {
        assert_protocol(
            NativeEvent {
                raw: ptr::null_mut(),
                ops: &FAKE_OPS,
            }
            .into_owned(),
        );
        for mode in [1, 2, 3, 4, 5, 6] {
            let (event, closes) = fake_event(vec![], mode);
            let error = event.into_owned().unwrap_err();
            assert_eq!(
                error.kind(),
                &match mode {
                    2 => ErrorKind::InternalError,
                    5 => ErrorKind::ProviderFailed,
                    _ => ErrorKind::ProtocolInconsistency,
                }
            );
            assert_eq!(
                closes.get(),
                if mode == 2 { 2 } else { 1 },
                "mode {mode}: no double free after success"
            );
        }
    }

    unsafe extern "C" fn fake_stream_close(
        raw: *mut *mut sys::AmsMelIrNucMetadata,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32 {
        // SAFETY: fake_stream allocates exactly this box, uniquely consumed here.
        unsafe {
            diagnostic(d, c, r);
            let observed = Box::from_raw((*raw).cast::<Rc<Cell<usize>>>());
            observed.set(observed.get() + 1);
            *raw = ptr::null_mut();
        }
        sys::AMS_MEL_OK
    }

    fn fake_stream() -> (NucStream, Rc<Cell<usize>>) {
        let closes = Rc::new(Cell::new(0));
        (
            NucStream {
                raw: Box::into_raw(Box::new(closes.clone())).cast(),
                close_op: fake_stream_close,
                _not_send_sync: Rc::new(()),
            },
            closes,
        )
    }

    #[test]
    fn stream_drop_invokes_close() {
        let (stream, closes) = fake_stream();
        assert!(stream.is_open());
        drop(stream);
        assert_eq!(closes.get(), 1, "Drop must call native Close");
    }

    /// Image ordinary metadata has no public safe Rust owner yet. Use its
    /// existing raw exports here on the same privately accessible ImageStream
    /// to verify compatibility without adding any public API or test hook.
    #[test]
    fn ordinary_image_metadata_is_independent_of_safe_nuc_registration() {
        use crate::{ComponentLocation, ImageConfig, Session, UciId};
        use std::path::PathBuf;
        let provider = std::env::var_os("AMS_MEL_TEST_PROVIDER_DIR")
            .map(PathBuf::from)
            .unwrap_or_else(|| {
                PathBuf::from(env!("CARGO_MANIFEST_DIR"))
                    .join("../../native/build-tests/test-providers")
            })
            .join("libmock_ir_provider.so");
        for scenario in ["image-metadata-four-mixed", "nuc-rich"] {
            let session = Session::open(&provider, scenario, "").unwrap();
            let id = || UciId::new([0; 16], "image").unwrap();
            let config = ImageConfig::with_limits(
                id(),
                id(),
                ComponentLocation::new(0.0, 0.0, 0.0, "key", "system").unwrap(),
                3,
                64,
                2,
            )
            .unwrap();
            let mut image = session.open_image_stream(&config).unwrap();
            let mut metadata = ptr::null_mut();
            // SAFETY: live Image owner, initially null ordinary metadata owner.
            call_with_diagnostic(|d, c, r| unsafe {
                sys::ams_mel_ir_image_metadata_open(image.raw, 4, &mut metadata, d, c, r)
            })
            .unwrap();
            assert!(!metadata.is_null());
            if scenario == "nuc-rich" {
                let mut nuc = image.open_nuc(4).unwrap();
                assert_eq!(nuc.counters().unwrap().events_received, 3);
                assert!(nuc.receive(0).unwrap().points.is_empty());
                nuc.close().unwrap();
            } else {
                let error = image.open_nuc(4).unwrap_err();
                assert_eq!(error.registration(), Some(CommandReturn::NotSupported));
                assert_eq!(
                    image.open_nuc(4).unwrap_err().error().kind(),
                    &ErrorKind::InvalidArgument
                );
            }
            for kind in 1..=4 {
                let mut event = ptr::null_mut();
                // SAFETY: live unique ordinary metadata owner, null event output.
                let result = call_with_diagnostic(|d, c, r| unsafe {
                    sys::ams_mel_ir_image_metadata_receive(metadata, 0, &mut event, d, c, r)
                });
                if scenario == "nuc-rich" {
                    assert_eq!(result.unwrap_err().kind(), &ErrorKind::Timeout);
                    break;
                }
                result.unwrap();
                let mut view = ptr::null();
                // SAFETY: received event stays live until Close below.
                unsafe {
                    call_with_diagnostic(|d, c, r| {
                        sys::ams_mel_ir_image_metadata_event_view(event, &mut view, d, c, r)
                    })
                    .unwrap();
                    assert_eq!((*view).kind, kind);
                    call_with_diagnostic(|d, c, r| {
                        sys::ams_mel_ir_image_metadata_event_close(&mut event, d, c, r)
                    })
                    .unwrap();
                }
            }
            // SAFETY: unique ordinary metadata owner, no outstanding views.
            call_with_diagnostic(|d, c, r| unsafe {
                sys::ams_mel_ir_image_metadata_close(&mut metadata, d, c, r)
            })
            .unwrap();
            image.close().unwrap();
            session.close().unwrap();
        }
    }

    #[test]
    fn stream_explicit_close_and_closed_operations() {
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

    #[test]
    fn receive_once_preserves_status_and_guards_unexpected_event_on_failure() {
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
            let (mut event, closes) = fake_event(vec![], 0);
            let calls = Cell::new(0);
            let result = stream.receive_event_with(&FAKE_OPS, |out, d, c, r| {
                calls.set(calls.get() + 1);
                // SAFETY: exact writable output slots supplied by the guard.
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
        assert_protocol(stream.receive_event_with(&FAKE_OPS, |_, d, c, r| {
            // SAFETY: diagnostic slots are supplied by call_with_diagnostic.
            unsafe {
                diagnostic(d, c, r);
            }
            sys::AMS_MEL_OK
        }));
    }

    #[test]
    fn open_validates_both_outputs_and_cleans_unexpected_owners() {
        for (status, registration, expected) in [
            (Ok(()), 0, None),
            (Ok(()), 2, Some(ErrorKind::ProtocolInconsistency)),
            (Ok(()), u32::MAX, Some(ErrorKind::ProtocolInconsistency)),
            (
                Err(Error::new(ErrorKind::ProviderException, "throw")),
                u32::MAX,
                Some(ErrorKind::ProviderException),
            ),
            (
                Err(Error::new(ErrorKind::ProviderFailed, "no")),
                3,
                Some(ErrorKind::ProviderFailed),
            ),
        ] {
            let (stream, closes) = fake_stream();
            let result = stream.finish_open(status, registration);
            if let Some(kind) = expected {
                let error = result.unwrap_err();
                assert_eq!(error.error().kind(), &kind);
                assert_eq!(error.registration(), command_return(registration).ok());
                assert!(std::error::Error::source(&error).is_some());
                assert!(!error.to_string().is_empty());
            } else {
                drop(result.unwrap());
            }
            assert_eq!(closes.get(), 1);
        }
        let stream = NucStream {
            raw: ptr::null_mut(),
            close_op: fake_stream_close,
            _not_send_sync: Rc::new(()),
        };
        assert_eq!(
            stream.finish_open(Ok(()), 0).unwrap_err().error().kind(),
            &ErrorKind::ProtocolInconsistency
        );
        let error =
            NucStream::open_with(0, |_, _, _, _, _| panic!("must not register")).unwrap_err();
        assert_eq!(error.error().kind(), &ErrorKind::InvalidArgument);
        assert_eq!(error.registration(), None);
    }
}
