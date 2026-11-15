//! Complete owned NavigationReport submission. The native bridge alone owns
//! completion workers, cached outcomes, admission and deferred Image teardown.

use std::{ffi::c_char, ptr, rc::Rc};

use ams_mel_sys as sys;

use crate::{
    call_with_diagnostic, decode_diagnostic, error_from_status, mel_error_code, protocol, Error,
    ErrorKind, Euler, ImageStream, MelErrorCode, NavigationResponse, WAIT_DIAGNOSTIC_CAPACITY,
};

/// Complete published solution state domain. The MaxExclusive sentinel is not
/// a solution. No quality ordering or interpretation is imposed.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum PositionSolutionState {
    /// Position solution has not been set (published value 0).
    NotSet,
    /// Published aligning solution (1); upstream defines no accuracy threshold or quality rank.
    Aligning,
    /// Published free-inertial solution (2); upstream defines no accuracy threshold or quality rank.
    FreeInertial,
    /// Published GPS solution (3); upstream specifies no fix class, accuracy threshold or quality rank.
    Gps,
    /// Published blended solution (4); upstream does not specify the blend algorithm or a quality rank.
    Blended,
}

impl TryFrom<u32> for PositionSolutionState {
    type Error = Error;

    /// Converts exactly the five published solution values, without ranking them.
    ///
    /// # Errors
    /// Returns [`ErrorKind::InvalidArgument`] for the MaxExclusive sentinel (5)
    /// or any other unpublished numeric value.
    fn try_from(value: u32) -> Result<Self, Error> {
        match value {
            sys::AMS_MEL_POSITION_SOLUTION_NOT_SET => Ok(Self::NotSet),
            sys::AMS_MEL_POSITION_SOLUTION_ALIGNING => Ok(Self::Aligning),
            sys::AMS_MEL_POSITION_SOLUTION_FREE_INERTIAL => Ok(Self::FreeInertial),
            sys::AMS_MEL_POSITION_SOLUTION_GPS => Ok(Self::Gps),
            sys::AMS_MEL_POSITION_SOLUTION_BLENDED => Ok(Self::Blended),
            _ => Err(Error::new(
                ErrorKind::InvalidArgument,
                "invalid position solution state",
            )),
        }
    }
}

impl PositionSolutionState {
    fn raw(self) -> u32 {
        match self {
            Self::NotSet => sys::AMS_MEL_POSITION_SOLUTION_NOT_SET,
            Self::Aligning => sys::AMS_MEL_POSITION_SOLUTION_ALIGNING,
            Self::FreeInertial => sys::AMS_MEL_POSITION_SOLUTION_FREE_INERTIAL,
            Self::Gps => sys::AMS_MEL_POSITION_SOLUTION_GPS,
            Self::Blended => sys::AMS_MEL_POSITION_SOLUTION_BLENDED,
        }
    }
}

/// North/East/Down components. Units belong to the containing upstream field;
/// none are inferred by this representation and no frame transform is applied.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct NorthEastDown {
    /// North component; units follow the containing navigation field.
    pub north: f64,
    /// East component; units follow the containing navigation field.
    pub east: f64,
    /// Down component; units are established only by the containing field.
    pub down: f64,
}

/// Euler rate components and their independent signed nanosecond timestamp.
/// The pinned declaration does not specify rate units or a timestamp epoch.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct AttitudeRate {
    /// Euler rate components; the pinned report does not explicitly establish rate units.
    pub attitude_rate: Euler,
    /// Independent signed attitude-rate timestamp in nanoseconds, with no inferred epoch.
    pub attitude_rate_time_ns: i64,
}

/// All 18 published terms in declaration order, not an anonymous matrix.
/// Covariance units are not stated upstream. No missing terms are synthesized.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct PositionVelocityCovariance {
    /// Published `position position pn pn` covariance term; units are unspecified and no symmetry completion is performed.
    pub position_position_pn_pn: f64,
    /// Published `position position pn pe` covariance term; units are unspecified and no symmetry completion is performed.
    pub position_position_pn_pe: f64,
    /// Published `position position pn pd` covariance term; units are unspecified and no symmetry completion is performed.
    pub position_position_pn_pd: f64,
    /// Published `position position pe pe` covariance term; units are unspecified and no symmetry completion is performed.
    pub position_position_pe_pe: f64,
    /// Published `position position pe pd` covariance term; units are unspecified and no symmetry completion is performed.
    pub position_position_pe_pd: f64,
    /// Published `position position pd pd` covariance term; units are unspecified and no symmetry completion is performed.
    pub position_position_pd_pd: f64,
    /// Published `position velocity pn vn` covariance term; units are unspecified and no symmetry completion is performed.
    pub position_velocity_pn_vn: f64,
    /// Published `position velocity pn ve` covariance term; units are unspecified and no symmetry completion is performed.
    pub position_velocity_pn_ve: f64,
    /// Published `position velocity pn vd` covariance term; units are unspecified and no symmetry completion is performed.
    pub position_velocity_pn_vd: f64,
    /// Published `position velocity pe ve` covariance term; units are unspecified and no symmetry completion is performed.
    pub position_velocity_pe_ve: f64,
    /// Published `position velocity pe vd` covariance term; units are unspecified and no symmetry completion is performed.
    pub position_velocity_pe_vd: f64,
    /// Published `position velocity pd vd` covariance term; units are unspecified and no symmetry completion is performed.
    pub position_velocity_pd_vd: f64,
    /// Published `velocity velocity vn vn` covariance term; units are unspecified and no symmetry completion is performed.
    pub velocity_velocity_vn_vn: f64,
    /// Published `velocity velocity vn ve` covariance term; units are unspecified and no symmetry completion is performed.
    pub velocity_velocity_vn_ve: f64,
    /// Published `velocity velocity vn vd` covariance term; units are unspecified and no symmetry completion is performed.
    pub velocity_velocity_vn_vd: f64,
    /// Published `velocity velocity ve ve` covariance term; units are unspecified and no symmetry completion is performed.
    pub velocity_velocity_ve_ve: f64,
    /// Published `velocity velocity ve vd` covariance term; units are unspecified and no symmetry completion is performed.
    pub velocity_velocity_ve_vd: f64,
    /// Published `velocity velocity vd vd` covariance term; units are unspecified and no symmetry completion is performed.
    pub velocity_velocity_vd_vd: f64,
}

/// Complete published NavigationReport. Every numeric value is copied without
/// normalization, range restriction, rounding, or coordinate/unit conversion.
///
/// This input becomes a complete native `NavigationReport` during
/// [`ImageStream::submit_navigation_report`]; the native call copies it before
/// returning, so the Rust value need not outlive the request. The bridge does
/// not estimate navigation quality, validate physical consistency, fill missing
/// covariance terms or turn the report into a metadata subscription. Timestamps
/// have no established epoch; attitude-rate and covariance units and additional
/// geodetic/reference conventions remain unspecified where upstream is silent.
///
/// ```
/// use ams_mel_ir::{AttitudeRate, Euler, NavigationReport, NorthEastDown,
///               PositionSolutionState, PositionVelocityCovariance};
/// let report = NavigationReport {
///     system_time_ns: -10,
///     state: PositionSolutionState::FreeInertial,
///     latitude_rad: 0.5,
///     longitude_rad: -1.0,
///     altitude_m: 1200.0,
///     attitude: Euler { roll: 0.0, pitch: 0.0, yaw: 0.0 },
///     attitude_rate: AttitudeRate {
///         attitude_rate: Euler { roll: 0.0, pitch: 0.0, yaw: 0.0 },
///         attitude_rate_time_ns: -11,
///     },
///     speed: NorthEastDown { north: 20.0, east: 0.0, down: 0.0 },
///     acceleration: NorthEastDown { north: 0.0, east: 0.0, down: 0.0 },
///     wander_angle_rad: 0.0,
///     magnetic_heading: 0.0,
///     altitude_msl: 0.0,
///     position_velocity_covariance_uncertainty: PositionVelocityCovariance {
///         position_position_pn_pn: 1.0, position_position_pn_pe: 0.0,
///         position_position_pn_pd: 0.0, position_position_pe_pe: 1.0,
///         position_position_pe_pd: 0.0, position_position_pd_pd: 1.0,
///         position_velocity_pn_vn: 0.0, position_velocity_pn_ve: 0.0,
///         position_velocity_pn_vd: 0.0, position_velocity_pe_ve: 0.0,
///         position_velocity_pe_vd: 0.0, position_velocity_pd_vd: 0.0,
///         velocity_velocity_vn_vn: 1.0, velocity_velocity_vn_ve: 0.0,
///         velocity_velocity_vn_vd: 0.0, velocity_velocity_ve_ve: 1.0,
///         velocity_velocity_ve_vd: 0.0, velocity_velocity_vd_vd: 1.0,
///     },
/// };
/// assert_eq!(report.attitude_rate.attitude_rate_time_ns, -11);
/// // The numeric example is not a statement of sensor quality or valid flight data.
/// ```
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct NavigationReport {
    /// Signed nanoseconds; no clock or epoch is inferred.
    pub system_time_ns: i64,
    /// Reported state in the containing record's published domain.
    pub state: PositionSolutionState,
    /// Published latitude in radians; no geodetic datum or range validation is added.
    pub latitude_rad: f64,
    /// Published longitude in radians; no wrapping or datum conversion is performed.
    pub longitude_rad: f64,
    /// Published altitude in metres; no altitude datum is inferred.
    pub altitude_m: f64,
    /// Published Euler attitude; see [`Euler`] for sequence and reference conventions.
    pub attitude: Euler,
    /// Euler rate components; the pinned report does not explicitly establish rate units.
    pub attitude_rate: AttitudeRate,
    /// North/East/Down speed, metres per second.
    pub speed: NorthEastDown,
    /// North/East/Down acceleration, metres per second squared.
    pub acceleration: NorthEastDown,
    /// Published wander angle in radians, copied without normalization.
    pub wander_angle_rad: f64,
    /// Upstream does not explicitly document units.
    pub magnetic_heading: f64,
    /// Barometric altitude; upstream does not explicitly document units.
    pub altitude_msl: f64,
    /// All 18 published covariance terms, without inferred units or synthesized terms.
    pub position_velocity_covariance_uncertainty: PositionVelocityCovariance,
}

fn raw_euler(value: Euler) -> sys::AmsMelEulerV1 {
    sys::AmsMelEulerV1 {
        roll: value.roll,
        pitch: value.pitch,
        yaw: value.yaw,
    }
}

fn raw_ned(value: NorthEastDown) -> sys::AmsMelNorthEastDownV1 {
    sys::AmsMelNorthEastDownV1 {
        north: value.north,
        east: value.east,
        down: value.down,
    }
}

impl NavigationReport {
    fn to_raw(self) -> Result<sys::AmsMelNavigationReportV1, Error> {
        let state = self.state.raw();
        // Explicit checked domain conversion, with no numeric narrowing.
        PositionSolutionState::try_from(state)?;
        let c = self.position_velocity_covariance_uncertainty;
        Ok(sys::AmsMelNavigationReportV1 {
            system_time_ns: self.system_time_ns,
            state,
            latitude_rad: self.latitude_rad,
            longitude_rad: self.longitude_rad,
            altitude_m: self.altitude_m,
            attitude: raw_euler(self.attitude),
            attitude_rate: sys::AmsMelAttitudeRateV1 {
                attitude_rate: raw_euler(self.attitude_rate.attitude_rate),
                attitude_rate_time_ns: self.attitude_rate.attitude_rate_time_ns,
            },
            speed: raw_ned(self.speed),
            acceleration: raw_ned(self.acceleration),
            wander_angle_rad: self.wander_angle_rad,
            magnetic_heading: self.magnetic_heading,
            altitude_msl: self.altitude_msl,
            position_velocity_covariance_uncertainty: sys::AmsMelPositionVelocityCovarianceV1 {
                position_position_pn_pn: c.position_position_pn_pn,
                position_position_pn_pe: c.position_position_pn_pe,
                position_position_pn_pd: c.position_position_pn_pd,
                position_position_pe_pe: c.position_position_pe_pe,
                position_position_pe_pd: c.position_position_pe_pd,
                position_position_pd_pd: c.position_position_pd_pd,
                position_velocity_pn_vn: c.position_velocity_pn_vn,
                position_velocity_pn_ve: c.position_velocity_pn_ve,
                position_velocity_pn_vd: c.position_velocity_pn_vd,
                position_velocity_pe_ve: c.position_velocity_pe_ve,
                position_velocity_pe_vd: c.position_velocity_pe_vd,
                position_velocity_pd_vd: c.position_velocity_pd_vd,
                velocity_velocity_vn_vn: c.velocity_velocity_vn_vn,
                velocity_velocity_vn_ve: c.velocity_velocity_vn_ve,
                velocity_velocity_vn_vd: c.velocity_velocity_vn_vd,
                velocity_velocity_ve_ve: c.velocity_velocity_ve_ve,
                velocity_velocity_ve_vd: c.velocity_velocity_ve_vd,
                velocity_velocity_vd_vd: c.velocity_velocity_vd_vd,
            },
        })
    }
}

/// Native terminal outcome. A rejection is distinct from timeout or a failed
/// future. Only Success contains a valid NavigationReportResp.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum NavigationResult {
    /// Cached successful future completion carrying the provider response; not proof of sensor effects.
    Success {
        /// Complete provider NavigationReportResp, copied into Rust-owned storage.
        response: NavigationResponse,
    },
    /// Explicit provider MEL rejection, distinct from timeout or a failed future.
    Rejected {
        /// Provider MEL error code; unknown codes retain their numeric value.
        code: MelErrorCode,
        /// Complete provider rejection description, copied as UTF-8.
        description: String,
    },
}

type Close = unsafe extern "C" fn(
    *mut *mut sys::AmsMelIrNavigationRequest,
    *mut c_char,
    usize,
    *mut usize,
) -> i32;
type Wait = unsafe extern "C" fn(
    *const sys::AmsMelIrNavigationRequest,
    u32,
    *mut sys::AmsMelIrNavigationResultV1,
    *mut c_char,
    usize,
    *mut usize,
) -> i32;

/// Unique request owner, independent of ImageStream and Session lifetimes.
/// Native completion retains the provider graph until work is finished, even
/// after Close/Drop. Neither Close nor timeout cancels provider work.
/// No Rust callback or worker thread is created. Drop closes once, best-effort.
///
/// ```compile_fail
/// fn send<T: Send>() {} send::<ams_mel_ir::NavigationRequest>();
/// ```
/// ```compile_fail
/// fn sync<T: Sync>() {} sync::<ams_mel_ir::NavigationRequest>();
/// ```
/// ```compile_fail
/// fn clone<T: Clone>() {} clone::<ams_mel_ir::NavigationRequest>();
/// ```
#[derive(Debug)]
pub struct NavigationRequest {
    raw: *mut sys::AmsMelIrNavigationRequest,
    close_op: Close,
    _not_send_sync: Rc<()>,
}

impl ImageStream {
    /// Copies the complete report synchronously and submits one native future.
    /// Valid while Attached (before Start) or Running, not after Stop/Close.
    /// Metadata callback delivery is separate from this request's completion.
    /// Image Close consumes its Rust owner, preventing any later submission.
    /// The returned unique request is neither `Send` nor `Sync` and may outlive
    /// Image and Session. A successful call is admission/submission, not future
    /// completion or physical acceptance of the navigation state by a sensor.
    ///
    /// ```no_run
    /// use ams_mel_ir::{Error, ErrorKind, ImageStream, NavigationReport, NavigationResult};
    /// fn submit(image: &mut ImageStream, report: &NavigationReport) -> Result<(), Error> {
    ///     let mut request = image.submit_navigation_report(report)?;
    ///     match request.wait(0) {
    ///         Ok(NavigationResult::Success { response }) => println!("{response:?}"),
    ///         Ok(NavigationResult::Rejected { code, description }) => {
    ///             eprintln!("rejected: {code:?}: {description}");
    ///         }
    ///         Err(error) if error.kind() == &ErrorKind::Timeout => {
    ///             // This timeout did not cancel work. Wait again or relinquish the owner.
    ///         }
    ///         Err(error) => return Err(error),
    ///     }
    ///     request.close() // nonblocking owner release, not cancellation
    /// }
    /// ```
    ///
    /// ```compile_fail
    /// fn after_close(mut image: ams_mel_ir::ImageStream, report: &ams_mel_ir::NavigationReport) {
    ///     image.close().unwrap();
    ///     image.submit_navigation_report(report).unwrap();
    /// }
    /// ```
    ///
    /// # Errors
    /// Native lifecycle, Session admission, provider exceptions and failed submission retain
    /// their native error categories and diagnostics. Invalid solution states are rejected during
    /// checked conversion; malformed successful output is a protocol error. Future MEL rejection
    /// is returned by [`NavigationRequest::wait`] as [`NavigationResult::Rejected`], not inferred here.
    pub fn submit_navigation_report(
        &mut self,
        report: &NavigationReport,
    ) -> Result<NavigationRequest, Error> {
        let report = report.to_raw()?;
        NavigationRequest::submit_with(
            |out, d, c, r| {
                // SAFETY: exclusive stream access, exact live input/output storage;
                // the native call copies report before returning.
                unsafe {
                    sys::ams_mel_ir_stream_submit_navigation_report(self.raw, &report, out, d, c, r)
                }
            },
            sys::ams_mel_ir_navigation_request_close,
        )
    }
}

impl NavigationRequest {
    fn submit_with(
        submit: impl FnOnce(
            *mut *mut sys::AmsMelIrNavigationRequest,
            *mut c_char,
            usize,
            *mut usize,
        ) -> i32,
        close_op: Close,
    ) -> Result<Self, Error> {
        let mut owner = Self {
            raw: ptr::null_mut(),
            close_op,
            _not_send_sync: Rc::new(()),
        };
        // Drop guards unexpected non-null output even when status or diagnostic
        // decoding fails. Submission is never retried.
        call_with_diagnostic(|d, c, r| submit(&mut owner.raw, d, c, r))?;
        if owner.raw.is_null() {
            return Err(protocol(
                "navigation submission succeeded without a request owner",
            ));
        }
        Ok(owner)
    }

    /// Finite native wait; zero polls. Terminal outcomes are cached natively
    /// and repeatable. Timeout returns ErrorKind::Timeout, without cancellation.
    ///
    /// # Errors
    /// Returns [`ErrorKind::Timeout`] if the future is not ready within `timeout_ms` milliseconds.
    /// This does not cancel work: polling with zero or waiting again is allowed. Provider/future
    /// failures retain native categories; malformed responses, changing cached outcomes or invalid
    /// UTF-8 are rejected rather than partially returned. Explicit MEL rejection is an `Ok` result.
    pub fn wait(&mut self, timeout_ms: u32) -> Result<NavigationResult, Error> {
        self.wait_with(timeout_ms, sys::ams_mel_ir_navigation_request_wait)
    }

    fn wait_with(&mut self, timeout_ms: u32, wait: Wait) -> Result<NavigationResult, Error> {
        let mut result = sys::AmsMelIrNavigationResultV1::default();
        let mut diagnostic = [0; WAIT_DIAGNOSTIC_CAPACITY];
        let mut required = 0;
        // SAFETY: unique live request; correctly sized writable C storage.
        let status = unsafe {
            wait(
                self.raw,
                timeout_ms,
                &mut result,
                diagnostic.as_mut_ptr().cast(),
                diagnostic.len(),
                &mut required,
            )
        };
        if required == 0 {
            return Err(protocol("native wait returned an invalid diagnostic size"));
        }
        if status == sys::AMS_MEL_TIMEOUT {
            let message = if required <= diagnostic.len() {
                Some(decode_diagnostic(&diagnostic[..required])?)
            } else {
                None
            };
            return Err(error_from_status(status, message, Some(required)));
        }
        let message = if required <= diagnostic.len() {
            decode_diagnostic(&diagnostic[..required])?
        } else {
            let mut complete = crate::zeroed_buffer(required, "navigation wait diagnostic")?;
            let mut retry = sys::AmsMelIrNavigationResultV1::default();
            let mut retry_required = 0;
            // SAFETY: terminal outcomes are cached; poll the same live owner
            // with exact diagnostic storage. Never repeat submission/release.
            let retry_status = unsafe {
                wait(
                    self.raw,
                    0,
                    &mut retry,
                    complete.as_mut_ptr().cast(),
                    complete.len(),
                    &mut retry_required,
                )
            };
            // Rejection/failure response storage is unspecified: compare only
            // fields valid for the actual status, not inactive response bytes.
            if retry_status != status
                || retry_required != required
                || (status == sys::AMS_MEL_COMMAND_REJECTED
                    && retry.error_code != result.error_code)
                || (status == sys::AMS_MEL_OK
                    && (retry.error_code != result.error_code
                        || retry.response.system_time_ns != result.response.system_time_ns
                        || retry.response.command_id != result.response.command_id
                        || retry.response.request_id != result.response.request_id))
            {
                return Err(protocol(
                    "native terminal wait changed during diagnostic retry",
                ));
            }
            decode_diagnostic(&complete)?
        };
        match status {
            sys::AMS_MEL_OK => {
                if required != 1
                    || !message.is_empty()
                    || result.error_code != sys::AMS_MEL_ERROR_NONE
                {
                    return Err(protocol(
                        "successful navigation wait returned an error or diagnostic",
                    ));
                }
                Ok(NavigationResult::Success {
                    response: NavigationResponse {
                        system_time_ns: result.response.system_time_ns,
                        command_id: result.response.command_id,
                        request_id: result.response.request_id,
                    },
                })
            }
            sys::AMS_MEL_COMMAND_REJECTED => Ok(NavigationResult::Rejected {
                code: mel_error_code(result.error_code),
                description: message,
            }),
            _ => Err(error_from_status(status, Some(message), Some(required))),
        }
    }

    /// Nonblocking release of the public owner only. A release is attempted
    /// once, even if the native status or output pointer is inconsistent.
    ///
    /// # Errors
    /// Native cleanup failures retain their categories and diagnostics; inconsistent successful
    /// owner output is a protocol error. See the owner's lifecycle contract for whether a failed
    /// release retains a retryable handle or consumes the one release attempt. No outcome implies
    /// cancellation of pending work or immediate callback quiescence.
    pub fn close(mut self) -> Result<(), Error> {
        self.close_inner()
    }

    fn close_inner(&mut self) -> Result<(), Error> {
        let mut raw = std::mem::replace(&mut self.raw, ptr::null_mut());
        if raw.is_null() {
            return Ok(());
        }
        let result = call_with_diagnostic(|d, c, r| {
            // SAFETY: unique owner is relinquished before this call; no wait
            // or Drop can use/retry a potentially freed pointer afterward.
            unsafe { (self.close_op)(&mut raw, d, c, r) }
        });
        result?;
        if !raw.is_null() {
            return Err(protocol(
                "navigation close succeeded without clearing the owner",
            ));
        }
        Ok(())
    }
}

impl Drop for NavigationRequest {
    fn drop(&mut self) {
        let _ = self.close_inner();
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicUsize, Ordering};

    static RELEASES: AtomicUsize = AtomicUsize::new(0);
    static LOCK: std::sync::Mutex<()> = std::sync::Mutex::new(());

    // A fake opaque pointer is only an address token, never dereferenced. This
    // safely models both cleared and uncertain potentially-freed outputs.
    fn token() -> *mut sys::AmsMelIrNavigationRequest {
        ptr::NonNull::dangling().as_ptr()
    }

    unsafe extern "C" fn close(
        raw: *mut *mut sys::AmsMelIrNavigationRequest,
        _: *mut c_char,
        _: usize,
        required: *mut usize,
    ) -> i32 {
        RELEASES.fetch_add(1, Ordering::SeqCst);
        // SAFETY: the owner passes writable pointer/diagnostic-size storage.
        unsafe {
            *raw = ptr::null_mut();
            *required = 1;
        }
        sys::AMS_MEL_OK
    }

    unsafe extern "C" fn uncertain(
        _: *mut *mut sys::AmsMelIrNavigationRequest,
        _: *mut c_char,
        _: usize,
        required: *mut usize,
    ) -> i32 {
        RELEASES.fetch_add(1, Ordering::SeqCst);
        // SAFETY: valid size storage. Deliberately leave the pointer unchanged.
        unsafe {
            *required = 1;
        }
        sys::AMS_MEL_INTERNAL_ERROR
    }

    unsafe extern "C" fn uncleared(
        _: *mut *mut sys::AmsMelIrNavigationRequest,
        _: *mut c_char,
        _: usize,
        required: *mut usize,
    ) -> i32 {
        RELEASES.fetch_add(1, Ordering::SeqCst);
        // SAFETY: valid size storage; simulate a malformed successful close.
        unsafe {
            *required = 1;
        }
        sys::AMS_MEL_OK
    }

    fn owner(close_op: Close) -> NavigationRequest {
        NavigationRequest {
            raw: token(),
            close_op,
            _not_send_sync: Rc::new(()),
        }
    }

    #[test]
    fn submission_defends_null_and_unexpected_owners_and_diagnostics() {
        let _guard = LOCK.lock().unwrap();
        RELEASES.store(0, Ordering::SeqCst);
        let error = NavigationRequest::submit_with(
            |_, _, _, r| {
                // SAFETY: submit_with supplies writable required-size storage.
                unsafe {
                    *r = 1;
                }
                sys::AMS_MEL_OK
            },
            close,
        )
        .unwrap_err();
        assert_eq!(error.kind(), &ErrorKind::ProtocolInconsistency);
        assert_eq!(RELEASES.load(Ordering::SeqCst), 0);
        for (status, size, expected) in [
            (
                sys::AMS_MEL_PROVIDER_EXCEPTION,
                1,
                ErrorKind::ProviderException,
            ),
            (sys::AMS_MEL_OK, 0, ErrorKind::ProtocolInconsistency),
            (
                sys::AMS_MEL_OK,
                usize::MAX,
                ErrorKind::ProtocolInconsistency,
            ),
        ] {
            let before = RELEASES.load(Ordering::SeqCst);
            let error = NavigationRequest::submit_with(
                |out, _, _, r| {
                    // SAFETY: exact writable output slots supplied by submit_with.
                    unsafe {
                        *out = token();
                        *r = size;
                    }
                    status
                },
                close,
            )
            .unwrap_err();
            assert_eq!(error.kind(), &expected);
            assert_eq!(RELEASES.load(Ordering::SeqCst), before + 1);
        }
    }

    #[test]
    fn release_is_never_retried_even_when_uncertain_or_malformed() {
        let _guard = LOCK.lock().unwrap();
        for (op, expected) in [
            (uncertain as Close, ErrorKind::InternalError),
            (uncleared as Close, ErrorKind::ProtocolInconsistency),
        ] {
            RELEASES.store(0, Ordering::SeqCst);
            assert_eq!(owner(op).close().unwrap_err().kind(), &expected);
            assert_eq!(RELEASES.load(Ordering::SeqCst), 1);
            drop(owner(op));
            assert_eq!(RELEASES.load(Ordering::SeqCst), 2);
            let error = NavigationRequest::submit_with(
                |out, _, _, r| {
                    // SAFETY: writable output slots supplied by submit_with.
                    unsafe {
                        *out = token();
                        *r = 1;
                    }
                    sys::AMS_MEL_PROVIDER_FAILED
                },
                op,
            )
            .unwrap_err();
            assert_eq!(error.kind(), &ErrorKind::ProviderFailed);
            assert_eq!(RELEASES.load(Ordering::SeqCst), 3);
        }
    }

    unsafe extern "C" fn malformed_wait(
        _: *const sys::AmsMelIrNavigationRequest,
        mode: u32,
        result: *mut sys::AmsMelIrNavigationResultV1,
        diagnostic: *mut c_char,
        _: usize,
        required: *mut usize,
    ) -> i32 {
        // SAFETY: wait_with supplies all output storage. Modes deliberately
        // violate one native contract at a time without invalid memory access.
        unsafe {
            *required = 1;
            match mode {
                0 => *required = 0,
                1 => *diagnostic = b'x' as c_char,
                2 => {
                    *required = 2;
                    *diagnostic = -1_i8 as c_char;
                    *diagnostic.add(1) = 0;
                }
                3 => (*result).error_code = sys::AMS_MEL_ERROR_INVALID_PARAMETERS,
                4 => {
                    *required = 2;
                    *diagnostic = b'x' as c_char;
                    *diagnostic.add(1) = 0;
                }
                _ => unreachable!(),
            }
        }
        sys::AMS_MEL_OK
    }

    #[test]
    fn malformed_wait_outputs_fail_closed() {
        let _guard = LOCK.lock().unwrap();
        let mut request = owner(close);
        for mode in 0..5 {
            let expected = if mode == 2 {
                ErrorKind::InvalidUtf8
            } else {
                ErrorKind::ProtocolInconsistency
            };
            assert_eq!(
                request.wait_with(mode, malformed_wait).unwrap_err().kind(),
                &expected
            );
        }
    }

    unsafe extern "C" fn changing_wait(
        _: *const sys::AmsMelIrNavigationRequest,
        timeout: u32,
        result: *mut sys::AmsMelIrNavigationResultV1,
        _: *mut c_char,
        _: usize,
        required: *mut usize,
    ) -> i32 {
        // SAFETY: exact writable outputs. Initial oversized rejection requests
        // a cached retry; the retry changes its code and must be refused.
        unsafe {
            *required = WAIT_DIAGNOSTIC_CAPACITY + 1;
            (*result).error_code = if timeout == 0 {
                sys::AMS_MEL_ERROR_UNSUPPORTED
            } else {
                sys::AMS_MEL_ERROR_INVALID_PARAMETERS
            };
        }
        sys::AMS_MEL_COMMAND_REJECTED
    }

    #[test]
    fn diagnostic_retry_checks_cached_outcome() {
        let _guard = LOCK.lock().unwrap();
        let mut request = owner(close);
        assert_eq!(
            request.wait_with(1, changing_wait).unwrap_err().kind(),
            &ErrorKind::ProtocolInconsistency
        );
    }

    fn boundary_report(value: f64) -> NavigationReport {
        let euler = Euler {
            roll: value,
            pitch: value,
            yaw: value,
        };
        let ned = NorthEastDown {
            north: value,
            east: value,
            down: value,
        };
        NavigationReport {
            system_time_ns: i64::MIN,
            state: PositionSolutionState::NotSet,
            latitude_rad: value,
            longitude_rad: value,
            altitude_m: value,
            attitude: euler,
            attitude_rate: AttitudeRate {
                attitude_rate: euler,
                attitude_rate_time_ns: i64::MAX,
            },
            speed: ned,
            acceleration: ned,
            wander_angle_rad: value,
            magnetic_heading: value,
            altitude_msl: value,
            position_velocity_covariance_uncertainty: PositionVelocityCovariance {
                position_position_pn_pn: value,
                position_position_pn_pe: value,
                position_position_pn_pd: value,
                position_position_pe_pe: value,
                position_position_pe_pd: value,
                position_position_pd_pd: value,
                position_velocity_pn_vn: value,
                position_velocity_pn_ve: value,
                position_velocity_pn_vd: value,
                position_velocity_pe_ve: value,
                position_velocity_pe_vd: value,
                position_velocity_pd_vd: value,
                velocity_velocity_vn_vn: value,
                velocity_velocity_vn_ve: value,
                velocity_velocity_vn_vd: value,
                velocity_velocity_ve_ve: value,
                velocity_velocity_ve_vd: value,
                velocity_velocity_vd_vd: value,
            },
        }
    }

    #[test]
    fn checked_state_encoding_and_full_range_timestamps() {
        for (index, state) in [
            PositionSolutionState::NotSet,
            PositionSolutionState::Aligning,
            PositionSolutionState::FreeInertial,
            PositionSolutionState::Gps,
            PositionSolutionState::Blended,
        ]
        .into_iter()
        .enumerate()
        {
            let mut report = boundary_report(0.0);
            report.state = state;
            let raw = report.to_raw().unwrap();
            assert_eq!(raw.state, index as u32);
            assert_eq!(
                PositionSolutionState::try_from(index as u32).unwrap(),
                state
            );
            assert_eq!(raw.system_time_ns, i64::MIN);
            assert_eq!(raw.attitude_rate.attitude_rate_time_ns, i64::MAX);
            report.system_time_ns = i64::MAX;
            report.attitude_rate.attitude_rate_time_ns = i64::MIN;
            let raw = report.to_raw().unwrap();
            assert_eq!(raw.system_time_ns, i64::MAX);
            assert_eq!(raw.attitude_rate.attitude_rate_time_ns, i64::MIN);
        }
    }

    #[test]
    fn floating_point_boundary_fidelity_every_field() {
        for value in [
            0.0,
            -0.0,
            f64::from_bits(1),
            -f64::from_bits(1),
            f64::MIN_POSITIVE,
            f64::MIN,
            f64::MAX,
            f64::INFINITY,
            f64::NEG_INFINITY,
            f64::NAN,
        ] {
            let raw = boundary_report(value).to_raw().unwrap();
            let c = raw.position_velocity_covariance_uncertainty;
            for actual in [
                raw.latitude_rad,
                raw.longitude_rad,
                raw.altitude_m,
                raw.attitude.roll,
                raw.attitude.pitch,
                raw.attitude.yaw,
                raw.attitude_rate.attitude_rate.roll,
                raw.attitude_rate.attitude_rate.pitch,
                raw.attitude_rate.attitude_rate.yaw,
                raw.speed.north,
                raw.speed.east,
                raw.speed.down,
                raw.acceleration.north,
                raw.acceleration.east,
                raw.acceleration.down,
                raw.wander_angle_rad,
                raw.magnetic_heading,
                raw.altitude_msl,
                c.position_position_pn_pn,
                c.position_position_pn_pe,
                c.position_position_pn_pd,
                c.position_position_pe_pe,
                c.position_position_pe_pd,
                c.position_position_pd_pd,
                c.position_velocity_pn_vn,
                c.position_velocity_pn_ve,
                c.position_velocity_pn_vd,
                c.position_velocity_pe_ve,
                c.position_velocity_pe_vd,
                c.position_velocity_pd_vd,
                c.velocity_velocity_vn_vn,
                c.velocity_velocity_vn_ve,
                c.velocity_velocity_vn_vd,
                c.velocity_velocity_ve_ve,
                c.velocity_velocity_ve_vd,
                c.velocity_velocity_vd_vd,
            ] {
                if value.is_nan() {
                    assert!(actual.is_nan());
                } else {
                    assert_eq!(actual.to_bits(), value.to_bits());
                }
            }
        }
    }
}
