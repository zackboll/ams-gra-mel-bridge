//! Complete value-only C2 ModeCmd input. Native code owns asynchronous work.

use crate::{
    call_with_diagnostic, protocol, sys, ControlChannel, Error, ErrorKind, MfaMode, ModeRequest,
};
use std::{ptr, rc::Rc};

macro_rules! input_enum {
    ($(#[$meta:meta])* $name:ident { $($variant:ident = $value:expr),+ $(,)? }) => {
        $(#[$meta])*
        #[derive(Clone, Copy, Debug, Eq, PartialEq, Hash)]
        #[repr(u32)]
        pub enum $name {
            $(
                /// Published alternative; see the enum's value table for its numeric association and meaning.
                $variant = $value
            ),+
        }
        impl $name {
            /// All published values in numeric order.
            pub const ALL: &'static [Self] = &[$(Self::$variant),+];
        }
        impl TryFrom<u32> for $name {
            type Error = Error;
            /// Converts a published numeric input without substituting a fallback.
            ///
            /// # Errors
            /// Returns [`ErrorKind::InvalidArgument`] for unpublished values.
            fn try_from(value: u32) -> Result<Self, Error> {
                match value {
                    $($value => Ok(Self::$variant),)+
                    _ => Err(Error::new(ErrorKind::InvalidArgument,
                        format!("unsupported {} value: {value}", stringify!($name)))),
                }
            }
        }
    };
}

input_enum! {
    /// Published Common MEL MFA states, exactly 0 through 14.
    /// `Unknown` is the published state 1, not a fallback for unknown integers.
    ///
    /// Published associations are categories, not a quality ordering or a bitmask.
    /// Names retain the pinned interface meaning; no provider policy is inferred.
    ///
    /// | Alternative | Value | Meaning |
    /// |---|---:|---|
    /// | `NotSet` | 0 | Value not set. |
    /// | `Unknown` | 1 | Published unknown category (not an unknown-number fallback). |
    /// | `NotInstalled` | 2 | Published not installed category. |
    /// | `Off` | 3 | Published off category. |
    /// | `PreInitialization` | 4 | Published pre initialization category. |
    /// | `Initialization` | 5 | Published initialization category. |
    /// | `Standby` | 6 | Published standby category. |
    /// | `Operate` | 7 | Published operate category. |
    /// | `OperateRxOnly` | 8 | Published operate rx only category. |
    /// | `OperateTxOnly` | 9 | Published operate tx only category. |
    /// | `Maintenance` | 10 | Published maintenance category. |
    /// | `Calibration` | 11 | Published calibration category. |
    /// | `InitiatedBit` | 12 | Published initiated bit category. |
    /// | `Shutdown` | 13 | Published shutdown category. |
    /// | `Degraded` | 14 | Published degraded category. |
    MfaState {
        NotSet = 0, Unknown = 1, NotInstalled = 2, Off = 3,
        PreInitialization = 4, Initialization = 5, Standby = 6, Operate = 7,
        OperateRxOnly = 8, OperateTxOnly = 9, Maintenance = 10,
        Calibration = 11, InitiatedBit = 12, Shutdown = 13, Degraded = 14,
    }
}
input_enum! {
    /// Independent reference for each scan-center angle. No transformation is performed.
    ///
    /// | Alternative | Value | Meaning |
    /// |---|---:|---|
    /// | `Inertial` | 0 | Published inertial reference. |
    /// | `Aircraft` | 1 | Published aircraft reference. |
    ///
    /// The binding does not define an additional origin, axis convention or
    /// relationship between these frames beyond the pinned declaration.
    CoordinateFrameReference { Inertial = 0, Aircraft = 1 }
}
input_enum! {
    /// Requested overload handling; this binding performs no scheduling algorithm.
    ///
    /// | Alternative | Value | Meaning |
    /// |---|---:|---|
    /// | `Capacity` | 0 | Published capacity degradation category. |
    /// | `Volume` | 1 | Published volume degradation category. |
    /// | `Range` | 2 | Published range degradation category. |
    /// | `Revisit` | 3 | Published revisit degradation category. |
    ///
    /// These select the provider's policy; Rust does not infer how much any
    /// scan property changes or whether the provider can honor the request.
    DegradationMethod { Capacity = 0, Volume = 1, Range = 2, Revisit = 3 }
}

impl TryFrom<u32> for MfaMode {
    type Error = Error;
    /// Admits only the four published input modes. Result decoding continues to
    /// retain the existing `Unknown` representation where applicable.
    ///
    /// # Errors
    /// Returns [`ErrorKind::InvalidArgument`] for any number outside 0..=3.
    fn try_from(value: u32) -> Result<Self, Error> {
        match value {
            0 => Ok(Self::Unused),
            1 => Ok(Self::TaskSched),
            2 => Ok(Self::ScanVolumeSched),
            3 => Ok(Self::ScanBarSched),
            _ => Err(Error::new(
                ErrorKind::InvalidArgument,
                format!("unsupported MFA mode: {value}"),
            )),
        }
    }
}

/// Published ScanType controls are unsigned integers, **not** Booleans.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct ScanType {
    /// Published unsigned continuous-scan control; not restricted to Boolean values.
    pub continuous_scan: u32,
    /// Published unsigned returning control; not restricted to Boolean values.
    pub returning: u32,
    /// Published unsigned agile-scan control; not restricted to Boolean values.
    pub agile_scan: u32,
}

/// Complete ScanParam payload. Angles are radians, scan rate radians/second,
/// intervals seconds, and range/altitude scalars use the native `_m` fields.
/// No altitude datum, feasibility check, normalization or state/mode rule is inferred.
/// Zero range/altitude scalars retain upstream's unset representation.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ScanParameters {
    /// Published selector for defining elevation with range and altitude; no geometry is calculated here.
    pub elevation_defined_with_range_and_altitude: bool,
    /// Scan-center azimuth in radians, using `center_frame_ref_az`.
    pub center_az_rad: f64,
    /// Scan-center elevation in radians, using `center_frame_ref_el`.
    pub center_el_rad: f64,
    /// Independent elevation reference frame; defaults to Inertial.
    pub center_frame_ref_el: CoordinateFrameReference,
    /// Independent azimuth reference frame; defaults to Aircraft.
    pub center_frame_ref_az: CoordinateFrameReference,
    /// Published scan width in radians, without feasibility validation.
    pub scan_width_rad: f64,
    /// Published scan height in radians, without feasibility validation.
    pub scan_height_rad: f64,
    /// Published unsigned scan controls, copied unchanged.
    pub scan_type: ScanType,
    /// Provider scan correlation ID, preserving all 32 bits.
    pub scan_id: u32,
    /// Published scan rate in radians per second.
    pub scan_rate_rad_per_second: f64,
    /// Published preferred revisit interval in seconds.
    pub preferred_revisit_interval_seconds: f64,
    /// Published required revisit interval in seconds.
    pub required_revisit_interval_seconds: f64,
    /// Maximum range of interest in metres; zero retains the upstream unset representation.
    pub max_range_of_interest_m: u32,
    /// Minimum range of interest in metres; zero retains the upstream unset representation.
    pub min_range_of_interest_m: u32,
    /// Elevation scan-center altitude in metres; zero is unset and no datum is inferred.
    pub elevation_scan_center_altitude_m: u32,
    /// Elevation scan-center range in metres; zero retains the upstream unset representation.
    pub elevation_scan_center_range_m: u32,
    /// Requested overload-handling category; no scheduling algorithm is implemented here.
    pub degradation_method: DegradationMethod,
}

impl Default for ScanParameters {
    /// Matches the published C++ ScanParam defaults, notably **different**
    /// elevation and azimuth frame references (not a zeroed native record).
    fn default() -> Self {
        Self {
            elevation_defined_with_range_and_altitude: false,
            center_az_rad: 0.0,
            center_el_rad: 0.0,
            center_frame_ref_el: CoordinateFrameReference::Inertial,
            center_frame_ref_az: CoordinateFrameReference::Aircraft,
            scan_width_rad: 0.0,
            scan_height_rad: 0.0,
            scan_type: ScanType::default(),
            scan_id: 0,
            scan_rate_rad_per_second: 0.0,
            preferred_revisit_interval_seconds: 0.0,
            required_revisit_interval_seconds: 0.0,
            max_range_of_interest_m: 0,
            min_range_of_interest_m: 0,
            elevation_scan_center_altitude_m: 0,
            elevation_scan_center_range_m: 0,
            degradation_method: DegradationMethod::Capacity,
        }
    }
}

/// Complete owned ModeCmd. Provider acceptance does not prove physical scanning
/// or a completed state transition. `MfaMode::Unknown(_)` is rejected on input,
/// even if its payload happens to equal a published value.
///
/// ```
/// use ams_mel::{MfaState, MfaMode, ModeCommand, ScanParameters};
/// let command = ModeCommand {
///     command_id: u32::MAX,
///     state: MfaState::Standby,
///     mode: MfaMode::Unused,
///     scan_parameters: ScanParameters::default(),
/// };
/// assert_eq!(command.state as u32, 6);
/// ```
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ModeCommand {
    /// Provider command correlation ID, preserving all 32 bits; not a completion indicator.
    pub command_id: u32,
    /// Reported state in the containing record's published domain.
    pub state: MfaState,
    /// Published mode value; no state/mode compatibility rule is imposed by this binding.
    pub mode: MfaMode,
    /// Complete scan payload, copied even when the requested state or mode does not use it.
    pub scan_parameters: ScanParameters,
}

impl ModeCommand {
    fn to_raw(self) -> Result<sys::AmsMelIrModeCommandV1, Error> {
        let mode = match self.mode {
            MfaMode::Unused => sys::AMS_MEL_IR_MFA_MODE_UNUSED,
            MfaMode::TaskSched => sys::AMS_MEL_IR_MFA_MODE_TASK_SCHED,
            MfaMode::ScanVolumeSched => sys::AMS_MEL_IR_MFA_MODE_SCAN_VOLUME_SCHED,
            MfaMode::ScanBarSched => sys::AMS_MEL_IR_MFA_MODE_SCAN_BAR_SCHED,
            MfaMode::Unknown(value) => {
                return Err(Error::new(
                    ErrorKind::InvalidArgument,
                    format!("unsupported MFA mode input: {value}"),
                ))
            }
        };
        let s = self.scan_parameters;
        Ok(sys::AmsMelIrModeCommandV1 {
            command_id: self.command_id,
            state: self.state as u32,
            mode,
            scan_parameters: sys::AmsMelIrScanParamV1 {
                elevation_defined_with_range_and_altitude: u32::from(
                    s.elevation_defined_with_range_and_altitude,
                ),
                center_az_rad: s.center_az_rad,
                center_el_rad: s.center_el_rad,
                center_frame_ref_el: s.center_frame_ref_el as u32,
                center_frame_ref_az: s.center_frame_ref_az as u32,
                scan_width_rad: s.scan_width_rad,
                scan_height_rad: s.scan_height_rad,
                scan_type: sys::AmsMelIrScanTypeV1 {
                    continuous_scan: s.scan_type.continuous_scan,
                    returning: s.scan_type.returning,
                    agile_scan: s.scan_type.agile_scan,
                },
                scan_id: s.scan_id,
                scan_rate_rad_per_second: s.scan_rate_rad_per_second,
                preferred_revisit_interval_seconds: s.preferred_revisit_interval_seconds,
                required_revisit_interval_seconds: s.required_revisit_interval_seconds,
                max_range_of_interest_m: s.max_range_of_interest_m,
                min_range_of_interest_m: s.min_range_of_interest_m,
                elevation_scan_center_altitude_m: s.elevation_scan_center_altitude_m,
                elevation_scan_center_range_m: s.elevation_scan_center_range_m,
                degradation_method: s.degradation_method as u32,
            },
        })
    }
}

impl ControlChannel {
    /// Submits a complete ModeCmd on an enabled C2 channel, using native Session
    /// admission and the existing cached completion engine. Inputs are copied
    /// during this call. Invalid input enums return [`ErrorKind::InvalidArgument`].
    /// No feasibility or state/mode compatibility rules are added.
    ///
    /// Timeout is not cancellation; the returned [`ModeRequest`] can outlive
    /// this channel and its Session. Explicit MEL rejection is a normal
    /// [`crate::ModeResult::Rejected`], retaining provider code and description.
    ///
    /// # Errors
    /// Returns [`ErrorKind::InvalidArgument`] for an unsupported input mode, including any
    /// `MfaMode::Unknown(_)`. Native lifecycle, admission and provider failures retain their categories
    /// and diagnostics; a missing request owner is a protocol error. Successful submission is not completion.
    pub fn submit_mode(&mut self, command: &ModeCommand) -> Result<ModeRequest, Error> {
        let command = command.to_raw()?;
        submit_with(|out| {
            call_with_diagnostic(|diagnostic, capacity, required| {
                // SAFETY: channel is uniquely borrowed; command and initialized
                // output storage remain valid for the synchronous native copy.
                unsafe {
                    sys::ams_mel_ir_c2_submit_mode(
                        self.raw, &command, out, diagnostic, capacity, required,
                    )
                }
            })
        })
    }
}

fn submit_with(
    submit: impl FnOnce(&mut *mut sys::AmsMelIrModeRequest) -> Result<(), Error>,
) -> Result<ModeRequest, Error> {
    // Acquire the existing owner before calling native code, so an unexpected
    // non-null error output is also released once by non-panicking Drop.
    let mut owner = ModeRequest {
        raw: ptr::null_mut(),
        _not_send_sync: Rc::new(()),
    };
    submit(&mut owner.raw)?;
    if owner.raw.is_null() {
        return Err(protocol("C2 submit succeeded without a request owner"));
    }
    Ok(owner)
}

#[cfg(test)]
mod tests;
