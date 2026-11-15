//! Common MEL MFA state shared by safe IR and RF without an RF crate dependency.

use crate::{Error, ErrorKind};

/// Published Common MEL `MFA_State` value commanded through
/// `RfAdmin::command_state`.
///
/// These are MFA *states*, distinct from IR `MfaMode`. Each
/// variant's discriminant is exactly the corresponding
/// `AMS_MEL_IR_MFA_STATE_*` value; RF Admin uses the identical Common MEL
/// domain (`ams_mel_rf_mfa_state_t`). Only the fifteen published values 0..=14
/// exist, so an out-of-range state cannot be constructed through safe Rust.
/// Raw values are admitted only through the checked `TryFrom<u32>`
/// conversion, which rejects `MAX_EXCLUSIVE` (15) and every larger value.
///
/// ```
/// use ams_mel_common::{ErrorKind, RfMfaState};
/// assert_eq!(RfMfaState::OperateRxOnly.as_raw(), 8);
/// assert_eq!(RfMfaState::try_from(6)?, RfMfaState::Standby);
/// assert_eq!(RfMfaState::try_from(15).unwrap_err().kind(), &ErrorKind::InvalidArgument);
/// # Ok::<(), ams_mel_common::Error>(())
/// ```
#[derive(Clone, Copy, Debug, Eq, PartialEq, Hash)]
#[repr(u32)]
pub enum RfMfaState {
    /// State has not been set (`0`).
    NotSet = 0,
    /// Subsystem state is unknown (`1`).
    Unknown = 1,
    /// Subsystem is not installed (`2`).
    NotInstalled = 2,
    /// Subsystem is not powered up (`3`).
    Off = 3,
    /// Bootstrap and initial operational flight program (OFP) load (`4`).
    PreInitialization = 4,
    /// Initialization and mission-data load (`5`).
    Initialization = 5,
    /// Initialization complete; ready to transition to operation (`6`).
    Standby = 6,
    /// Subsystem may perform its enabled capabilities (`7`).
    Operate = 7,
    /// Subsystem may perform its enabled receive-only capabilities (`8`).
    OperateRxOnly = 8,
    /// Transmit-only operation state (`9`).
    OperateTxOnly = 9,
    /// Performing or having performed the maintenance procedure (`10`).
    Maintenance = 10,
    /// State-commanded calibration that interrupts other functions (`11`),
    /// distinct from calibration initiated through a BIT command without a state change.
    Calibration = 11,
    /// State-commanded initiated built-in test (BIT) that interrupts other
    /// functions (`12`), distinct from BIT commands that need no state change.
    InitiatedBit = 12,
    /// Performing or having performed shutdown (`13`); the published state
    /// definition specifies no subsystem messages once shutdown completes.
    Shutdown = 13,
    /// Subsystem cannot perform all normal activities (`14`).
    Degraded = 14,
}

impl RfMfaState {
    /// Every published state, in ascending raw-value order (0..=14).
    pub const ALL: [RfMfaState; 15] = [
        Self::NotSet,
        Self::Unknown,
        Self::NotInstalled,
        Self::Off,
        Self::PreInitialization,
        Self::Initialization,
        Self::Standby,
        Self::Operate,
        Self::OperateRxOnly,
        Self::OperateTxOnly,
        Self::Maintenance,
        Self::Calibration,
        Self::InitiatedBit,
        Self::Shutdown,
        Self::Degraded,
    ];

    /// Returns the exact Common MEL `MFA_State` value passed to the C ABI.
    pub const fn as_raw(self) -> u32 {
        self as u32
    }
}

impl From<RfMfaState> for u32 {
    fn from(state: RfMfaState) -> Self {
        state.as_raw()
    }
}

impl TryFrom<u32> for RfMfaState {
    type Error = Error;

    /// Checked raw conversion. Values 15 (`MAX_EXCLUSIVE`) and above return
    /// [`ErrorKind::InvalidArgument`]; they are never mapped to a state.
    ///
    /// # Errors
    /// Returns [`ErrorKind::InvalidArgument`] for any value outside `0..=14`.
    fn try_from(raw: u32) -> Result<Self, Error> {
        Self::ALL
            .iter()
            .copied()
            .find(|state| state.as_raw() == raw)
            .ok_or_else(|| {
                Error::new(
                    ErrorKind::InvalidArgument,
                    format!("RF MFA state {raw} is outside the published range 0..15"),
                )
            })
    }
}

