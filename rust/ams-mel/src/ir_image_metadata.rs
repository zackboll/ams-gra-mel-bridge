//! Owned polling of the four C-bridged IR Image metadata families. Native code
//! owns callback registration, the bounded FIFO, counters and callback lifetime;
//! Rust adds no callbacks, threads, locks or queue. The C facade remains the
//! trusted storage/provenance boundary for checked immutable event views.

use std::{ffi::c_char, ptr, rc::Rc};

use ams_mel_sys as sys;

use crate::{
    call_with_diagnostic, checked_bool, checked_span, copy_mapped, protocol, Error, ErrorKind,
    Euler,
};

/// The complete pinned BadPixelReason domain. Unpublished values are rejected.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[repr(u32)]
pub enum BadPixelReason {
    Unknown = sys::AMS_MEL_IR_BAD_PIXEL_REASON_UNKNOWN,
}

/// Provider coordinates, without inferring their origin or renumbering them.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct BadPixel {
    pub row: u32,
    pub column: u32,
    pub reason: BadPixelReason,
}

/// Complete ordered collection, preserving duplicates. Reported fields are
/// independent of `pixels.len()` and never used to truncate the copied vector.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct BadPixelList {
    pub reported_size: u32,
    pub reported_count: u32,
    pub pixels: Vec<BadPixel>,
}

/// Radian angles. When used for pointing rates, the pinned upstream interface
/// does not explicitly document a time denominator; none is inferred here.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct AzimuthElevation {
    pub azimuth_rad: f64,
    pub elevation_rad: f64,
}

/// Complete pointing report, without normalization or validity-bit interpretation.
#[derive(Clone, Debug, PartialEq)]
pub struct LineOfSightReport {
    /// Exact signed system-time nanoseconds; no epoch is inferred.
    pub system_time_ns: i64,
    pub pointing_angle: AzimuthElevation,
    pub pointing_angle_rates: AzimuthElevation,
    pub at_speed: bool,
    pub in_tolerance: bool,
    pub platform_attitude: Euler,
    pub validity_flag_bitfield: u32,
    pub image_rotation_rad: f64,
}

/// Attitude and attitude rates are distinct, untransformed Euler quantities.
#[derive(Clone, Debug, PartialEq)]
pub struct LineOfSightEuler {
    /// Exact signed system-time nanoseconds; no epoch is inferred.
    pub system_time_ns: i64,
    pub attitude: Euler,
    pub attitude_rates: Euler,
}

/// Complete NavigationReportResp value, shared by metadata and request results.
/// Metadata arrival is NOT request-handle completion. No provider error code or
/// completion state is inferred from its identifiers.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct NavigationResponse {
    pub system_time_ns: i64,
    pub command_id: u32,
    pub request_id: u32,
}

/// Exactly the four native families, with their published discriminants 1..=4.
/// Every payload is fully owned and can outlive all native owners and the DSO.
#[derive(Clone, Debug, PartialEq)]
#[repr(u32)]
pub enum ImageMetadataEvent {
    BadPixelList(BadPixelList) = sys::AMS_MEL_IR_IMAGE_METADATA_BAD_PIXEL_LIST,
    LineOfSightReport(LineOfSightReport) = sys::AMS_MEL_IR_IMAGE_METADATA_LINE_OF_SIGHT_REPORT,
    LineOfSightEuler(LineOfSightEuler) = sys::AMS_MEL_IR_IMAGE_METADATA_LINE_OF_SIGHT_EULER,
    NavigationResponse(NavigationResponse) = sys::AMS_MEL_IR_IMAGE_METADATA_NAVIGATION_RESPONSE,
}

/// Saturating native callback activity snapshots, not Rust Receive-call counts.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ImageMetadataCounters {
    pub events_received: u64,
    pub events_dropped_queue_full: u64,
    /// Includes malformed payloads and callback allocation failures. These do
    /// not automatically fail the stream.
    pub malformed_or_unsupported: u64,
}

type Close<T> = unsafe extern "C" fn(*mut *mut T, *mut c_char, usize, *mut usize) -> i32;

/// Unique native metadata consumption owner, independent of Image/Session Rust
/// lifetimes. Close does not close the Image stream or unregister callbacks.
/// Image Stop/Close prevents enqueueing; queued events drain before StreamStopped.
/// Native channel destruction is the callback-quiescence/provider-unload boundary.
/// Drop makes a non-panicking best-effort Close.
///
/// ```compile_fail
/// fn require_send<T: Send>() {}
/// require_send::<ams_mel::ImageMetadataStream>();
/// ```
/// ```compile_fail
/// fn require_sync<T: Sync>() {}
/// require_sync::<ams_mel::ImageMetadataStream>();
/// ```
#[derive(Debug)]
pub struct ImageMetadataStream {
    raw: *mut sys::AmsMelIrImageMetadata,
    close_op: Close<sys::AmsMelIrImageMetadata>,
    _not_send_sync: Rc<()>,
}

impl ImageMetadataStream {
    pub(crate) fn open_with(
        queue_capacity: usize,
        open: impl FnOnce(*mut *mut sys::AmsMelIrImageMetadata, *mut c_char, usize, *mut usize) -> i32,
    ) -> Result<Self, Error> {
        if queue_capacity == 0 {
            return Err(Error::new(
                ErrorKind::InvalidArgument,
                "Image metadata capacity must be nonzero",
            ));
        }
        let mut owner = Self {
            raw: ptr::null_mut(),
            close_op: sys::ams_mel_ir_image_metadata_close,
            _not_send_sync: Rc::new(()),
        };
        // Guard an unexpected output on failure as well. Registration is never
        // retried, and this API has no separate provider Return output.
        let result = call_with_diagnostic(|d, c, r| open(&mut owner.raw, d, c, r));
        owner.finish_open(result)
    }

    fn finish_open(self, result: Result<(), Error>) -> Result<Self, Error> {
        result?;
        if !self.is_open() {
            return Err(protocol("Image metadata open succeeded without an owner"));
        }
        Ok(self)
    }

    /// Local ownership, not callback/provider liveness.
    pub fn is_open(&self) -> bool {
        !self.raw.is_null()
    }

    /// One native Receive; zero polls. Deeply copies the selected payload and
    /// closes its event before returning. Errors and unwind also release it.
    pub fn receive(&mut self, timeout_ms: u32) -> Result<ImageMetadataEvent, Error> {
        let raw = self.live()?;
        self.receive_event_with(&EVENT_OPS, |out, d, c, r| {
            // SAFETY: live exclusive owner; event output is guarded.
            unsafe { sys::ams_mel_ir_image_metadata_receive(raw, timeout_ms, out, d, c, r) }
        })
    }

    fn receive_event_with(
        &mut self,
        ops: &'static EventOps,
        receive: impl FnOnce(
            *mut *mut sys::AmsMelIrImageMetadataEvent,
            *mut c_char,
            usize,
            *mut usize,
        ) -> i32,
    ) -> Result<ImageMetadataEvent, Error> {
        let mut event = NativeEvent {
            raw: ptr::null_mut(),
            ops,
        };
        call_with_diagnostic(|d, c, r| receive(&mut event.raw, d, c, r))?;
        event.copy_with(event_from_raw)
    }

    pub fn counters(&mut self) -> Result<ImageMetadataCounters, Error> {
        let raw = self.live()?;
        self.counters_with(|out, d, c, r| {
            // SAFETY: live owner and exact writable counter storage.
            unsafe { sys::ams_mel_ir_image_metadata_get_counters(raw, out, d, c, r) }
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

    /// Idempotent consumption Close; callbacks can remain retained natively.
    pub fn close(&mut self) -> Result<(), Error> {
        close_owned(&mut self.raw, self.close_op)
    }

    fn live(&self) -> Result<*mut sys::AmsMelIrImageMetadata, Error> {
        if self.is_open() {
            Ok(self.raw)
        } else {
            Err(Error::new(
                ErrorKind::ProviderFailed,
                "IR Image metadata stream is closed",
            ))
        }
    }
}

impl Drop for ImageMetadataStream {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            // SAFETY: unique owner, supported null diagnostics; no allocation.
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
        // SAFETY: caller owns this unique handle; views are no longer in use.
        status = unsafe { close(raw, d, c, r) };
        status
    });
    if status == sys::AMS_MEL_OK && !raw.is_null() {
        // A claimed release may already have freed storage. Forget even if the
        // diagnostic was malformed, rather than risking a second free in Drop.
        *raw = ptr::null_mut();
        return Err(protocol(
            "Image metadata close succeeded without clearing the handle",
        ));
    }
    result
}

struct EventOps {
    view: unsafe extern "C" fn(
        *const sys::AmsMelIrImageMetadataEvent,
        *mut *const sys::AmsMelIrImageMetadataEventV1,
        *mut c_char,
        usize,
        *mut usize,
    ) -> i32,
    close: Close<sys::AmsMelIrImageMetadataEvent>,
}

static EVENT_OPS: EventOps = EventOps {
    view: sys::ams_mel_ir_image_metadata_event_view,
    close: sys::ams_mel_ir_image_metadata_event_close,
};

struct NativeEvent {
    raw: *mut sys::AmsMelIrImageMetadataEvent,
    ops: &'static EventOps,
}

impl NativeEvent {
    fn copy_with(
        mut self,
        copy: impl FnOnce(&sys::AmsMelIrImageMetadataEventV1) -> Result<ImageMetadataEvent, Error>,
    ) -> Result<ImageMetadataEvent, Error> {
        if self.raw.is_null() {
            return Err(protocol(
                "Image metadata receive succeeded without an event",
            ));
        }
        let mut view = ptr::null();
        call_with_diagnostic(|d, c, r| {
            // SAFETY: live event guard and writable output pointer.
            unsafe { (self.ops.view)(self.raw, &mut view, d, c, r) }
        })?;
        // SAFETY: facade owns immutable storage through conversion. Validate
        // outer pointer before dereference, then only the active payload span.
        let record = unsafe { checked_span(view, 1, "Image metadata event record")? };
        let value = copy(&record[0])?;
        close_owned(&mut self.raw, self.ops.close)?;
        Ok(value)
    }
}

impl Drop for NativeEvent {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            // SAFETY: unique event; no borrowed values survive cleanup.
            let _ = unsafe { (self.ops.close)(&mut self.raw, ptr::null_mut(), 0, ptr::null_mut()) };
        }
    }
}

fn euler(raw: &sys::AmsMelEulerV1) -> Euler {
    Euler {
        roll: raw.roll,
        pitch: raw.pitch,
        yaw: raw.yaw,
    }
}

fn az_el(raw: &sys::AmsMelIrAzElV1) -> AzimuthElevation {
    AzimuthElevation {
        azimuth_rad: raw.azimuth_rad,
        elevation_rad: raw.elevation_rad,
    }
}

// Called only on live facade storage or backed unit fixtures. Never inspect an
// inactive payload: poisoned inactive spans/Booleans must not affect decoding.
fn event_from_raw(raw: &sys::AmsMelIrImageMetadataEventV1) -> Result<ImageMetadataEvent, Error> {
    match raw.kind {
        sys::AMS_MEL_IR_IMAGE_METADATA_BAD_PIXEL_LIST => {
            let list = &raw.bad_pixel_list;
            // SAFETY: event storage remains alive; validate before slicing.
            let pixels =
                unsafe { checked_span(list.pixels.data, list.pixels.size, "BadPixelList.pixels")? };
            Ok(ImageMetadataEvent::BadPixelList(BadPixelList {
                reported_size: list.reported_size,
                reported_count: list.reported_count,
                pixels: copy_mapped(pixels, "BadPixelList.pixels", |pixel| {
                    let reason = match pixel.reason {
                        sys::AMS_MEL_IR_BAD_PIXEL_REASON_UNKNOWN => BadPixelReason::Unknown,
                        _ => return Err(protocol("unpublished BadPixelReason")),
                    };
                    Ok(BadPixel {
                        row: pixel.row,
                        column: pixel.column,
                        reason,
                    })
                })?,
            }))
        }
        sys::AMS_MEL_IR_IMAGE_METADATA_LINE_OF_SIGHT_REPORT => {
            let report = &raw.line_of_sight_report;
            Ok(ImageMetadataEvent::LineOfSightReport(LineOfSightReport {
                system_time_ns: report.system_time_ns,
                pointing_angle: az_el(&report.pointing_angle),
                pointing_angle_rates: az_el(&report.pointing_angle_rates),
                at_speed: checked_bool(report.at_speed.into(), "LineOfSightReport.at_speed")?,
                in_tolerance: checked_bool(
                    report.in_tolerance.into(),
                    "LineOfSightReport.in_tolerance",
                )?,
                platform_attitude: euler(&report.platform_attitude),
                validity_flag_bitfield: report.validity_flag_bitfield,
                image_rotation_rad: report.image_rotation_rad,
            }))
        }
        sys::AMS_MEL_IR_IMAGE_METADATA_LINE_OF_SIGHT_EULER => {
            let los = &raw.line_of_sight_euler;
            Ok(ImageMetadataEvent::LineOfSightEuler(LineOfSightEuler {
                system_time_ns: los.system_time_ns,
                attitude: euler(&los.attitude),
                attitude_rates: euler(&los.attitude_rates),
            }))
        }
        sys::AMS_MEL_IR_IMAGE_METADATA_NAVIGATION_RESPONSE => {
            let nav = &raw.navigation_response;
            Ok(ImageMetadataEvent::NavigationResponse(NavigationResponse {
                system_time_ns: nav.system_time_ns,
                command_id: nav.command_id,
                request_id: nav.request_id,
            }))
        }
        _ => Err(protocol("unpublished Image metadata event kind")),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::{
        cell::Cell,
        panic::{catch_unwind, AssertUnwindSafe},
    };

    fn record(kind: u32) -> sys::AmsMelIrImageMetadataEventV1 {
        // SAFETY: raw records contain only numeric fields and nullable pointers.
        let mut raw: sys::AmsMelIrImageMetadataEventV1 = unsafe { std::mem::zeroed() };
        raw.kind = kind;
        raw
    }

    fn assert_protocol<T: std::fmt::Debug>(result: Result<T, Error>) {
        assert_eq!(
            result.unwrap_err().kind(),
            &ErrorKind::ProtocolInconsistency
        );
    }

    #[test]
    fn dispatch_only_reads_active_payload_and_rejects_unknowns() {
        for kind in 1..=4 {
            let mut raw = record(kind);
            // Invalid inactive fields, never dereferenced. Active fields repaired.
            raw.bad_pixel_list.pixels.data = ptr::without_provenance(1);
            raw.bad_pixel_list.pixels.size = usize::MAX;
            raw.line_of_sight_report.at_speed = 255;
            raw.line_of_sight_report.in_tolerance = 2;
            if kind == 1 {
                raw.bad_pixel_list.pixels.size = 0;
            }
            if kind == 2 {
                raw.line_of_sight_report.at_speed = 1;
                raw.line_of_sight_report.in_tolerance = 0;
            }
            let event = event_from_raw(&raw).unwrap();
            assert!(matches!(
                (kind, event),
                (1, ImageMetadataEvent::BadPixelList(_))
                    | (2, ImageMetadataEvent::LineOfSightReport(_))
                    | (3, ImageMetadataEvent::LineOfSightEuler(_))
                    | (4, ImageMetadataEvent::NavigationResponse(_))
            ));
        }
        for kind in [0, 5, u32::MAX] {
            assert_protocol(event_from_raw(&record(kind)));
        }
    }

    #[test]
    fn bad_pixels_complete_order_duplicates_counts_and_reason_domain() {
        let mut pixels = [sys::AmsMelIrBadPixelV1 {
            row: u32::MAX,
            column: 0x80000001,
            reason: 0,
        }; 3];
        pixels[1].row = 7;
        let mut raw = record(1);
        raw.bad_pixel_list.reported_size = 0xa5a50001;
        raw.bad_pixel_list.reported_count = 1;
        raw.bad_pixel_list.pixels = sys::AmsMelIrBadPixelSpanV1 {
            data: pixels.as_ptr(),
            size: pixels.len(),
        };
        let ImageMetadataEvent::BadPixelList(value) = event_from_raw(&raw).unwrap() else {
            panic!()
        };
        assert_eq!(value.reported_size, 0xa5a50001);
        assert_eq!(value.reported_count, 1);
        assert_eq!(
            value.pixels,
            vec![
                BadPixel {
                    row: u32::MAX,
                    column: 0x80000001,
                    reason: BadPixelReason::Unknown
                },
                BadPixel {
                    row: 7,
                    column: 0x80000001,
                    reason: BadPixelReason::Unknown
                },
                BadPixel {
                    row: u32::MAX,
                    column: 0x80000001,
                    reason: BadPixelReason::Unknown
                },
            ]
        );
        assert_eq!(BadPixelReason::Unknown as u32, 0);
        for reason in [1, 99, u32::MAX] {
            pixels[2].reason = reason;
            assert_protocol(event_from_raw(&raw));
        }
    }

    #[test]
    fn spans_reject_null_misalignment_and_impossible_sizes_accept_empty() {
        let mut raw = record(1);
        for data in [
            ptr::null(),
            ptr::without_provenance(1),
            ptr::NonNull::dangling().as_ptr(),
        ] {
            raw.bad_pixel_list.pixels = sys::AmsMelIrBadPixelSpanV1 { data, size: 0 };
            let ImageMetadataEvent::BadPixelList(value) = event_from_raw(&raw).unwrap() else {
                panic!()
            };
            assert!(value.pixels.is_empty());
        }
        for (data, size) in [
            (ptr::null(), 1),
            (ptr::without_provenance(1), 1),
            (ptr::NonNull::dangling().as_ptr(), usize::MAX),
            (
                ptr::NonNull::dangling().as_ptr(),
                isize::MAX as usize / std::mem::size_of::<sys::AmsMelIrBadPixelV1>() + 1,
            ),
        ] {
            raw.bad_pixel_list.pixels = sys::AmsMelIrBadPixelSpanV1 { data, size };
            assert_protocol(event_from_raw(&raw));
        }
    }

    #[test]
    fn booleans_are_strict_and_independent() {
        for at_speed in [0, 1, 2, 255] {
            for in_tolerance in [0, 1, 2, 255] {
                let mut raw = record(2);
                raw.line_of_sight_report.at_speed = at_speed;
                raw.line_of_sight_report.in_tolerance = in_tolerance;
                let result = event_from_raw(&raw);
                if at_speed > 1 || in_tolerance > 1 {
                    assert_protocol(result);
                } else {
                    let ImageMetadataEvent::LineOfSightReport(value) = result.unwrap() else {
                        panic!()
                    };
                    assert_eq!(value.at_speed, at_speed == 1);
                    assert_eq!(value.in_tolerance, in_tolerance == 1);
                }
            }
        }
    }

    fn raw_euler(values: [f64; 3]) -> sys::AmsMelEulerV1 {
        sys::AmsMelEulerV1 {
            roll: values[0],
            pitch: values[1],
            yaw: values[2],
        }
    }

    fn euler_bits(value: Euler) -> [u64; 3] {
        [
            value.roll.to_bits(),
            value.pitch.to_bits(),
            value.yaw.to_bits(),
        ]
    }

    #[test]
    fn all_float_fields_preserve_bits_and_field_mapping() {
        // Rotate all values through every floating field, including NaN payloads,
        // negative zero, infinities and the smallest positive subnormal.
        let bits = [
            0x7ff8000000001234,
            0x8000000000000000,
            0,
            1,
            f64::INFINITY.to_bits(),
            f64::NEG_INFINITY.to_bits(),
            1.25f64.to_bits(),
            (-2.5f64).to_bits(),
        ];
        for shift in 0..bits.len() {
            let b: Vec<_> = (0..8).map(|i| bits[(i + shift) % 8]).collect();
            let f: Vec<_> = b.iter().map(|v| f64::from_bits(*v)).collect();
            let mut raw = record(2);
            let report = &mut raw.line_of_sight_report;
            report.pointing_angle = sys::AmsMelIrAzElV1 {
                azimuth_rad: f[0],
                elevation_rad: f[1],
            };
            report.pointing_angle_rates = sys::AmsMelIrAzElV1 {
                azimuth_rad: f[2],
                elevation_rad: f[3],
            };
            report.platform_attitude = raw_euler([f[4], f[5], f[6]]);
            report.validity_flag_bitfield = u32::MAX;
            report.image_rotation_rad = f[7];
            let ImageMetadataEvent::LineOfSightReport(value) = event_from_raw(&raw).unwrap() else {
                panic!()
            };
            assert_eq!(
                [
                    value.pointing_angle.azimuth_rad.to_bits(),
                    value.pointing_angle.elevation_rad.to_bits(),
                    value.pointing_angle_rates.azimuth_rad.to_bits(),
                    value.pointing_angle_rates.elevation_rad.to_bits()
                ],
                b[..4]
            );
            assert_eq!(euler_bits(value.platform_attitude), b[4..7]);
            assert_eq!(value.image_rotation_rad.to_bits(), b[7]);
            assert_eq!(value.validity_flag_bitfield, u32::MAX);
            raw.kind = 3;
            raw.line_of_sight_euler.attitude = raw_euler([f[0], f[1], f[2]]);
            raw.line_of_sight_euler.attitude_rates = raw_euler([f[3], f[4], f[5]]);
            let ImageMetadataEvent::LineOfSightEuler(value) = event_from_raw(&raw).unwrap() else {
                panic!()
            };
            assert_eq!(euler_bits(value.attitude), b[..3]);
            assert_eq!(euler_bits(value.attitude_rates), b[3..6]);
        }
    }

    #[test]
    fn signed_timestamp_extremes_and_navigation_ids() {
        for time in [i64::MIN, -1, 0, i64::MAX] {
            let mut raw = record(2);
            raw.line_of_sight_report.system_time_ns = time;
            let ImageMetadataEvent::LineOfSightReport(value) = event_from_raw(&raw).unwrap() else {
                panic!()
            };
            assert_eq!(value.system_time_ns, time);
            raw.kind = 3;
            raw.line_of_sight_euler.system_time_ns = time;
            let ImageMetadataEvent::LineOfSightEuler(value) = event_from_raw(&raw).unwrap() else {
                panic!()
            };
            assert_eq!(value.system_time_ns, time);
            raw.kind = 4;
            raw.navigation_response = sys::AmsMelIrNavigationResponseV1 {
                system_time_ns: time,
                command_id: 0xf1234567,
                request_id: 0x89abcdef,
            };
            assert_eq!(
                event_from_raw(&raw).unwrap(),
                ImageMetadataEvent::NavigationResponse(NavigationResponse {
                    system_time_ns: time,
                    command_id: 0xf1234567,
                    request_id: 0x89abcdef
                })
            );
        }
    }

    unsafe fn diagnostic(d: *mut c_char, c: usize, r: *mut usize) {
        // SAFETY: fake operations receive the helper's writable diagnostic slots.
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
        view: sys::AmsMelIrImageMetadataEventV1,
        closes: Rc<Cell<usize>>,
        mode: u8,
    }

    unsafe extern "C" fn fake_view(
        raw: *const sys::AmsMelIrImageMetadataEvent,
        out: *mut *const sys::AmsMelIrImageMetadataEventV1,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32 {
        // SAFETY: handle points at the uniquely guarded fake Box.
        unsafe {
            diagnostic(d, c, r);
            let fake = &*raw.cast::<FakeEvent>();
            *out = match fake.mode {
                3 => ptr::null(),
                4 => ptr::without_provenance(1),
                _ => &fake.view,
            };
            if fake.mode == 5 {
                sys::AMS_MEL_PROVIDER_EXCEPTION
            } else {
                sys::AMS_MEL_OK
            }
        }
    }

    unsafe extern "C" fn fake_event_close(
        raw: *mut *mut sys::AmsMelIrImageMetadataEvent,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32 {
        // SAFETY: exact boxed fake, freed only once; mode 2 fails once retaining
        // ownership, allowing the guard's fallback retry to release it.
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

    fn fake_event(kind: u32, mode: u8) -> (NativeEvent, Rc<Cell<usize>>) {
        let closes = Rc::new(Cell::new(0));
        let raw = Box::into_raw(Box::new(FakeEvent {
            view: record(kind),
            closes: closes.clone(),
            mode,
        }))
        .cast();
        (
            NativeEvent {
                raw,
                ops: &FAKE_OPS,
            },
            closes,
        )
    }

    #[test]
    fn event_view_conversion_close_errors_and_unwind_cleanup() {
        for (kind, mode, expected, count) in [
            (1, 0, None, 1),
            (99, 0, Some(ErrorKind::ProtocolInconsistency), 1),
            (1, 1, Some(ErrorKind::ProtocolInconsistency), 1),
            (1, 2, Some(ErrorKind::InternalError), 2),
            (1, 3, Some(ErrorKind::ProtocolInconsistency), 1),
            (1, 4, Some(ErrorKind::ProtocolInconsistency), 1),
            (1, 5, Some(ErrorKind::ProviderException), 1),
            (1, 6, Some(ErrorKind::ProtocolInconsistency), 1),
        ] {
            let (event, closes) = fake_event(kind, mode);
            let result = event.copy_with(event_from_raw);
            if let Some(expected) = expected {
                assert_eq!(result.unwrap_err().kind(), &expected);
            } else {
                assert!(result.is_ok());
            }
            assert_eq!(closes.get(), count);
        }
        let (event, closes) = fake_event(1, 0);
        assert!(catch_unwind(AssertUnwindSafe(
            || event.copy_with(|_| panic!("conversion unwind"))
        ))
        .is_err());
        assert_eq!(closes.get(), 1);
    }

    unsafe extern "C" fn fake_stream_close(
        raw: *mut *mut sys::AmsMelIrImageMetadata,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32 {
        // SAFETY: stream fixture owns exactly this boxed shared counter.
        unsafe {
            diagnostic(d, c, r);
            let closes = Box::from_raw((*raw).cast::<Rc<Cell<usize>>>());
            closes.set(closes.get() + 1);
            *raw = ptr::null_mut();
            sys::AMS_MEL_OK
        }
    }

    fn fake_stream() -> (ImageMetadataStream, Rc<Cell<usize>>) {
        let closes = Rc::new(Cell::new(0));
        (
            ImageMetadataStream {
                raw: Box::into_raw(Box::new(closes.clone())).cast(),
                close_op: fake_stream_close,
                _not_send_sync: Rc::new(()),
            },
            closes,
        )
    }

    #[test]
    fn stream_drop_calls_native_close_exactly_once() {
        let (stream, closes) = fake_stream();
        drop(stream);
        assert_eq!(closes.get(), 1);
    }

    #[test]
    fn open_failures_preserve_categories_and_close_unexpected_owner() {
        for kind in [
            ErrorKind::ProviderFailed,
            ErrorKind::ProviderException,
            ErrorKind::InternalError,
        ] {
            let (stream, closes) = fake_stream();
            assert_eq!(
                stream
                    .finish_open(Err(Error::new(kind.clone(), "registration failure")))
                    .unwrap_err()
                    .kind(),
                &kind
            );
            assert_eq!(closes.get(), 1);
        }
        let (stream, closes) = fake_stream();
        drop(stream.finish_open(Ok(())).unwrap());
        assert_eq!(closes.get(), 1);
    }

    #[test]
    fn counters_preserve_full_width_saturation_and_distinct_fields() {
        let (mut stream, _) = fake_stream();
        for (received, dropped, malformed) in
            [(u64::MAX, u64::MAX - 1, 0x8000000000000001), (9, 3, 2)]
        {
            let calls = Cell::new(0);
            let actual = stream
                .counters_with(|out, d, c, r| {
                    calls.set(calls.get() + 1);
                    // SAFETY: exact writable counter and diagnostic storage.
                    unsafe {
                        diagnostic(d, c, r);
                        *out = sys::AmsMelIrC2MetadataCountersV1 {
                            events_received: received,
                            events_dropped_queue_full: dropped,
                            malformed_or_unsupported: malformed,
                        };
                    }
                    sys::AMS_MEL_OK
                })
                .unwrap();
            assert_eq!(
                actual,
                ImageMetadataCounters {
                    events_received: received,
                    events_dropped_queue_full: dropped,
                    malformed_or_unsupported: malformed
                }
            );
            assert_eq!(calls.get(), 1);
        }
        assert_eq!(
            stream
                .counters_with(|_, d, c, r| {
                    // SAFETY: valid diagnostic slots; failure publishes no counters.
                    unsafe {
                        diagnostic(d, c, r);
                    }
                    sys::AMS_MEL_PROVIDER_FAILED
                })
                .unwrap_err()
                .kind(),
            &ErrorKind::ProviderFailed
        );
    }

    #[test]
    fn explicit_close_is_idempotent_and_closed_operations_reject() {
        let (mut stream, closes) = fake_stream();
        assert!(stream.is_open());
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
    fn receive_once_preserves_status_and_cleans_unexpected_error_outputs() {
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
            (sys::AMS_MEL_INVALID_ARGUMENT, ErrorKind::InvalidArgument),
        ] {
            let (mut event, closes) = fake_event(1, 0);
            let calls = Cell::new(0);
            let result = stream.receive_event_with(&FAKE_OPS, |out, d, c, r| {
                calls.set(calls.get() + 1);
                // SAFETY: writable slots; ownership moves into receive guard.
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
            // SAFETY: valid diagnostic slots, null event output deliberately unchanged.
            unsafe {
                diagnostic(d, c, r);
            }
            sys::AMS_MEL_OK
        }));
    }

    #[test]
    fn open_null_success_zero_capacity_and_null_parent_preflight() {
        assert_protocol(ImageMetadataStream::open_with(1, |_, d, c, r| {
            // SAFETY: valid diagnostic slots, deliberately missing owner.
            unsafe {
                diagnostic(d, c, r);
            }
            sys::AMS_MEL_OK
        }));
        assert_eq!(
            ImageMetadataStream::open_with(0, |_, _, _, _| panic!("must not register"))
                .unwrap_err()
                .kind(),
            &ErrorKind::InvalidArgument
        );
        let mut image = crate::ImageStream {
            raw: ptr::null_mut(),
            _not_send_sync: Rc::new(()),
        };
        for capacity in [0, 1] {
            assert_eq!(
                image.open_metadata(capacity).unwrap_err().kind(),
                &ErrorKind::ProviderFailed
            );
        }
    }
}
