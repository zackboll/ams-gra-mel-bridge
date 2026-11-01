//! Safe RF Admin state control (RUST-MEL-001) over the existing
//! `ams_mel_rf_admin_open`, `ams_mel_rf_admin_command_state`, and
//! `ams_mel_rf_admin_close` C exports. No C ABI is added or changed.

use std::path::Path;
use std::ptr;
use std::rc::Rc;

use ams_mel_sys as sys;

use crate::{c_path, c_string, call_with_diagnostic, checked_bool, protocol, Error, ErrorKind};

/// Value placed in the native acceptance output before each command. A
/// successful native call must overwrite it with exactly 0 or 1; if it does
/// not, the result is reported as a protocol inconsistency rather than being
/// mistaken for a provider rejection or acceptance.
const UNWRITTEN_ACCEPTANCE: u32 = u32::MAX;

/// Published Common MEL `MFA_State` value commanded through
/// [`RfAdmin::command_state`].
///
/// These are MFA *states*, distinct from [`MfaMode`](crate::MfaMode). Each
/// variant's discriminant is exactly the corresponding
/// `AMS_MEL_IR_MFA_STATE_*` value; RF Admin uses the identical Common MEL
/// domain (`ams_mel_rf_mfa_state_t`). Only the fifteen published values 0..=14
/// exist, so an out-of-range state cannot be constructed through safe Rust.
/// Raw values are admitted only through the checked `TryFrom<u32>`
/// conversion, which rejects `MAX_EXCLUSIVE` (15) and every larger value.
#[derive(Clone, Copy, Debug, Eq, PartialEq, Hash)]
#[repr(u32)]
pub enum RfMfaState {
    NotSet = sys::AMS_MEL_IR_MFA_STATE_NOT_SET,
    Unknown = sys::AMS_MEL_IR_MFA_STATE_UNKNOWN,
    NotInstalled = sys::AMS_MEL_IR_MFA_STATE_NOT_INSTALLED,
    Off = sys::AMS_MEL_IR_MFA_STATE_OFF,
    PreInitialization = sys::AMS_MEL_IR_MFA_STATE_PRE_INITIALIZATION,
    Initialization = sys::AMS_MEL_IR_MFA_STATE_INITIALIZATION,
    Standby = sys::AMS_MEL_IR_MFA_STATE_STANDBY,
    Operate = sys::AMS_MEL_IR_MFA_STATE_OPERATE,
    OperateRxOnly = sys::AMS_MEL_IR_MFA_STATE_OPERATE_RX_ONLY,
    OperateTxOnly = sys::AMS_MEL_IR_MFA_STATE_OPERATE_TX_ONLY,
    Maintenance = sys::AMS_MEL_IR_MFA_STATE_MAINTENANCE,
    Calibration = sys::AMS_MEL_IR_MFA_STATE_CALIBRATION,
    InitiatedBit = sys::AMS_MEL_IR_MFA_STATE_INITIATED_BIT,
    Shutdown = sys::AMS_MEL_IR_MFA_STATE_SHUTDOWN,
    Degraded = sys::AMS_MEL_IR_MFA_STATE_DEGRADED,
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
    fn try_from(raw: u32) -> Result<Self, Error> {
        Self::ALL
            .iter()
            .copied()
            .find(|state| state.as_raw() == raw)
            .ok_or_else(|| {
                Error::new(
                    ErrorKind::InvalidArgument,
                    format!(
                        "RF MFA state {raw} is outside the published range 0..{}",
                        sys::AMS_MEL_IR_MFA_STATE_MAX_EXCLUSIVE
                    ),
                )
            })
    }
}

/// Safe owner of one independent RF `createAdminMEL` provider object
/// (`ams_mel_rf_admin *`).
///
/// An `RfAdmin` is not an IR [`Session`](crate::Session), RF DataMEL, or RF
/// C2 owner and neither creates nor shares any of them. It owns the provider
/// AdminMEL and a reference to its provider library. Raw pointers and
/// provider objects are never exposed.
///
/// [`command_state`](Self::command_state) reports whether the provider
/// **accepted** the commanded state. Acceptance does not establish that the
/// hardware actually transitioned to that state.
///
/// Lifecycle:
///
/// - [`close`](Self::close) consumes the native owner exactly once, even when
///   provider shutdown throws. Once closed, further `close` calls return
///   `Ok(())` without calling native code, and
///   [`command_state`](Self::command_state) fails without a provider call.
/// - On successful shutdown the bridge destroys the provider AdminMEL
///   *before* releasing its provider library.
/// - If shutdown throws, the error is returned and the owner is closed. The
///   native bridge intentionally retains the uncertain provider object and
///   library for the rest of the process; this binding never retries shutdown
///   or unloads the provider itself.
/// - [`Drop`] performs the same Close for a still-open owner as best-effort
///   cleanup. It never panics and reports nothing, so a failure is never
///   presented as a successful shutdown; call `close` to observe the outcome.
///
/// The owner is intentionally neither `Send` nor `Sync`, and command and
/// close take `&mut self`, so the native requirement that operations on one
/// owner be externally serialized is enforced statically.
///
/// ```compile_fail
/// fn require_send<T: Send>() {}
/// require_send::<ams_mel::RfAdmin>();
/// ```
///
/// ```compile_fail
/// fn require_sync<T: Sync>() {}
/// require_sync::<ams_mel::RfAdmin>();
/// ```
///
/// ```no_run
/// use ams_mel::{RfAdmin, RfMfaState};
///
/// # fn main() -> Result<(), ams_mel::Error> {
/// let mut admin = RfAdmin::open("/opt/provider/librfmel.so", "configuration")?;
/// if !admin.command_state(RfMfaState::Standby)? {
///     // The provider rejected the command; this is not an error.
/// }
/// admin.close()?;
/// # Ok(())
/// # }
/// ```
#[derive(Debug)]
pub struct RfAdmin {
    raw: *mut sys::AmsMelRfAdmin,
    _not_send_sync: Rc<()>,
}

impl RfAdmin {
    /// Loads `provider_library`, resolves its `createAdminMEL` factory and
    /// calls it once with `configuration`.
    ///
    /// The path must be valid UTF-8 and neither argument may contain an
    /// embedded NUL; both are rejected as [`ErrorKind::InvalidArgument`]
    /// before any native call. Native failures keep their categories, for
    /// example [`ErrorKind::LibraryLoadFailed`],
    /// [`ErrorKind::SymbolNotFound`], [`ErrorKind::FactoryFailed`] for a null
    /// factory result and [`ErrorKind::ProviderException`] for a throwing
    /// factory. A failed open leaves no owner.
    pub fn open(
        provider_library: impl AsRef<Path>,
        configuration: impl AsRef<str>,
    ) -> Result<Self, Error> {
        let library = c_path(provider_library.as_ref())?;
        let configuration = c_string(configuration.as_ref(), "configuration")?;
        let mut raw = ptr::null_mut();
        let status = call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: both C strings live through the call; `raw` is an
            // initially null writable owner and the diagnostic storage matches
            // its advertised capacity.
            unsafe {
                sys::ams_mel_rf_admin_open(
                    library.as_ptr(),
                    configuration.as_ptr(),
                    &mut raw,
                    diagnostic,
                    capacity,
                    required,
                )
            }
        });
        match status {
            Ok(()) if raw.is_null() => Err(protocol(
                "RF Admin open succeeded without returning an owner",
            )),
            Ok(()) => Ok(Self {
                raw,
                _not_send_sync: Rc::new(()),
            }),
            Err(error) => {
                if !raw.is_null() {
                    // A malformed native implementation must still be given
                    // the opportunity to release an owner it published.
                    best_effort_close_rf_admin(&mut raw);
                }
                Err(error)
            }
        }
    }

    /// Returns `true` until [`close`](Self::close) or Drop has consumed the
    /// native owner.
    pub fn is_open(&self) -> bool {
        !self.raw.is_null()
    }

    /// Commands the provider's StatusControl to `state`.
    ///
    /// Returns `Ok(true)` if the provider accepted the command and `Ok(false)`
    /// if it rejected it normally. Neither value proves that the hardware
    /// reached `state`. Provider failures (for example a null UCI_Control or
    /// StatusControl), provider exceptions, invalid arguments and malformed
    /// native output return `Err`, and the owner stays open. A closed owner
    /// returns [`ErrorKind::ProviderFailed`] without calling native code.
    pub fn command_state(&mut self, state: RfMfaState) -> Result<bool, Error> {
        if self.raw.is_null() {
            return Err(Error::new(ErrorKind::ProviderFailed, "RF Admin is closed"));
        }
        let admin = self.raw;
        let mut accepted = UNWRITTEN_ACCEPTANCE;
        call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: `admin` is this wrapper's live unique owner; `&mut self`
            // serializes this call with Close. `accepted` is writable storage.
            unsafe {
                sys::ams_mel_rf_admin_command_state(
                    admin,
                    state.as_raw(),
                    &mut accepted,
                    diagnostic,
                    capacity,
                    required,
                )
            }
        })?;
        acceptance(accepted)
    }

    /// Shuts down and releases the provider AdminMEL exactly once.
    ///
    /// The owner is consumed whether or not provider shutdown succeeds; on
    /// return [`is_open`](Self::is_open) is always `false`. A throwing
    /// shutdown returns its error (normally [`ErrorKind::ProviderException`]
    /// with the provider diagnostic). The native bridge then intentionally
    /// retains the uncertain provider object and library, and shutdown is
    /// never retried. Repeated calls return `Ok(())` without native calls.
    pub fn close(&mut self) -> Result<(), Error> {
        if self.raw.is_null() {
            return Ok(());
        }
        let result = call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: `raw` is this wrapper's unique live owner. Native Close
            // consumes and nulls it before shutdown, including on failure.
            unsafe { sys::ams_mel_rf_admin_close(&mut self.raw, diagnostic, capacity, required) }
        });
        // The C contract always consumes the owner. Should an implementation
        // ever fail to clear it, the handle is deliberately abandoned rather
        // than offered to a second shutdown through Close or Drop.
        let retained = !self.raw.is_null();
        self.raw = ptr::null_mut();
        match result {
            Err(error) => Err(error),
            Ok(()) if retained => Err(protocol(
                "RF Admin close succeeded without consuming its owner",
            )),
            Ok(()) => Ok(()),
        }
    }
}

impl Drop for RfAdmin {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            best_effort_close_rf_admin(&mut self.raw);
            self.raw = ptr::null_mut();
        }
    }
}

/// Isolated conversion of the native acceptance output of a successful
/// command. Only exactly 0 or 1 is valid.
fn acceptance(accepted: u32) -> Result<bool, Error> {
    checked_bool(accepted, "RF Admin commandState acceptance")
}

fn best_effort_close_rf_admin(raw: &mut *mut sys::AmsMelRfAdmin) {
    // SAFETY: called only for a unique RF Admin owner. Null diagnostics are
    // supported. Native Close consumes the owner even when shutdown throws, so
    // it is never retried; the result is ignored because Drop cannot report it.
    let _ = unsafe { sys::ams_mel_rf_admin_close(raw, ptr::null_mut(), 0, ptr::null_mut()) };
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn every_state_has_its_exact_common_mel_value() {
        let expected = [
            sys::AMS_MEL_IR_MFA_STATE_NOT_SET,
            sys::AMS_MEL_IR_MFA_STATE_UNKNOWN,
            sys::AMS_MEL_IR_MFA_STATE_NOT_INSTALLED,
            sys::AMS_MEL_IR_MFA_STATE_OFF,
            sys::AMS_MEL_IR_MFA_STATE_PRE_INITIALIZATION,
            sys::AMS_MEL_IR_MFA_STATE_INITIALIZATION,
            sys::AMS_MEL_IR_MFA_STATE_STANDBY,
            sys::AMS_MEL_IR_MFA_STATE_OPERATE,
            sys::AMS_MEL_IR_MFA_STATE_OPERATE_RX_ONLY,
            sys::AMS_MEL_IR_MFA_STATE_OPERATE_TX_ONLY,
            sys::AMS_MEL_IR_MFA_STATE_MAINTENANCE,
            sys::AMS_MEL_IR_MFA_STATE_CALIBRATION,
            sys::AMS_MEL_IR_MFA_STATE_INITIATED_BIT,
            sys::AMS_MEL_IR_MFA_STATE_SHUTDOWN,
            sys::AMS_MEL_IR_MFA_STATE_DEGRADED,
        ];
        assert_eq!(
            RfMfaState::ALL.len(),
            sys::AMS_MEL_IR_MFA_STATE_MAX_EXCLUSIVE as usize
        );
        for (index, (state, raw)) in RfMfaState::ALL.iter().zip(expected).enumerate() {
            assert_eq!(raw as usize, index);
            assert_eq!(state.as_raw(), raw);
            assert_eq!(u32::from(*state), raw);
            assert_eq!(RfMfaState::try_from(raw).expect("published state"), *state);
        }
    }

    #[test]
    fn checked_conversion_rejects_max_exclusive_and_above() {
        for raw in [
            sys::AMS_MEL_IR_MFA_STATE_MAX_EXCLUSIVE,
            sys::AMS_MEL_IR_MFA_STATE_MAX_EXCLUSIVE + 1,
            u32::MAX,
        ] {
            let error = RfMfaState::try_from(raw).expect_err("unpublished state");
            assert_eq!(error.kind(), &ErrorKind::InvalidArgument);
            assert!(error
                .diagnostic()
                .is_some_and(|text| text.contains(&raw.to_string())));
        }
    }

    #[test]
    fn acceptance_admits_only_native_zero_or_one() {
        assert!(!acceptance(0).expect("rejection"));
        assert!(acceptance(1).expect("acceptance"));
        for malformed in [2, 77, UNWRITTEN_ACCEPTANCE] {
            let error = acceptance(malformed).expect_err("malformed acceptance");
            assert_eq!(error.kind(), &ErrorKind::ProtocolInconsistency);
            assert!(error
                .diagnostic()
                .is_some_and(|text| text.contains(&malformed.to_string())));
        }
    }

    #[test]
    fn oversized_native_diagnostic_preserves_status_and_required_length() {
        let error = call_with_diagnostic(|_, _, required| {
            // SAFETY: `required` is the helper's valid writable output.
            unsafe { *required = 5000 };
            sys::AMS_MEL_PROVIDER_EXCEPTION
        })
        .expect_err("oversized failure");
        assert_eq!(error.kind(), &ErrorKind::ProviderException);
        assert_eq!(error.diagnostic(), None);
        assert_eq!(error.diagnostic_required(), Some(5000));
    }
}
