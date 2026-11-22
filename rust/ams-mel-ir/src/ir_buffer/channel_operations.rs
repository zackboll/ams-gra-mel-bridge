use std::{ffi::c_char, fmt};

use ams_mel_sys as sys;

use super::{returned, HostBuffer};
use crate::{call_with_diagnostic, protocol, ChannelView, Error};

/// Failure from common Channel Disable or host Buffer registration/unregistration.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ChannelOperationError {
    /// Existing error category and diagnostic, including binding protocol errors.
    pub error: Error,
    /// Exact bridge status, or `None` for a closed wrapper rejected before FFI.
    pub native_status: Option<i32>,
    /// Exact provider `Return`, including unknown values, independent of bridge
    /// status. The ABI's `UINT32_MAX` sentinel maps to `None` (no Return obtained).
    pub provider_return: Option<u32>,
}

impl fmt::Display for ChannelOperationError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{} (native status {:?}, provider Return {:?})",
            self.error, self.native_status, self.provider_return
        )
    }
}

impl std::error::Error for ChannelOperationError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        Some(&self.error)
    }
}

impl ChannelOperationError {
    fn local(error: Error) -> Self {
        Self {
            error,
            native_status: None,
            provider_return: None,
        }
    }
}

type BufferOperation = unsafe extern "C" fn(
    *const sys::AmsMelIrChannel,
    *mut sys::AmsMelIrBuffer,
    *mut u32,
    *mut c_char,
    usize,
    *mut usize,
) -> i32;

impl ChannelView {
    /// Registers the existing host Buffer with this attached, disabled channel.
    ///
    /// Supported for C2, HealthAndStatus, StackedImage and **control-only** Image,
    /// before Enable/Start or after successful [`Self::disable`]. The channel
    /// and Buffer must belong to the exact same native Session. The View stays
    /// weak and the caller retains the unique public Buffer owner. Native code
    /// retains the association and guards the storage; this does not enable the
    /// channel or perform acquisition, checkout or release.
    ///
    /// # Errors
    /// Closed/expired handles, unsupported families, invalid lifecycle, duplicate
    /// or conflicting association, and provider failures are reported. Refusal
    /// or exception after provider entry leaves an ownership obligation on the
    /// original channel; reconcile with explicit [`Self::unregister_buffer`]
    /// through a live view of that channel, not another registration or Drop.
    pub fn register_buffer(
        &mut self,
        buffer: &mut HostBuffer,
    ) -> Result<(), ChannelOperationError> {
        self.buffer_operation(
            buffer,
            "register_buffer",
            sys::ams_mel_ir_channel_register_buffer,
        )
    }

    /// Unregisters from the same attached, disabled channel, possibly through
    /// a different view of that channel. Disable explicitly if enabled/running;
    /// terminal [`crate::ImageStream::stop`] is not a substitute.
    ///
    /// # Errors
    /// Wrong channel, invalid lifecycle, closed/expired handles, unsupported
    /// families and provider failures are reported. Failed provider calls leave
    /// the association uncertain. Even after successful unregister, extra
    /// provider ownership can still block Buffer access and Close. This operation
    /// neither closes either owner nor promises callback quiescence.
    pub fn unregister_buffer(
        &mut self,
        buffer: &mut HostBuffer,
    ) -> Result<(), ChannelOperationError> {
        self.buffer_operation(
            buffer,
            "unregister_buffer",
            sys::ams_mel_ir_channel_unregister_buffer,
        )
    }

    /// Disables C2, HealthAndStatus, StackedImage or control-only Image without
    /// detaching. An already-disabled or never-enabled eligible channel succeeds
    /// without a provider call. Does not unregister Buffers, stop Image, remove
    /// callbacks, cancel pending work or guarantee callback quiescence.
    ///
    /// # Errors
    /// Unsupported, closed/expired or conflicting lifecycle states fail. A
    /// provider refusal or exception consumes the cycle's Disable attempt and
    /// leaves native lifecycle failure sticky; neither this method nor native
    /// Close retries that provider attempt.
    pub fn disable(&mut self) -> Result<(), ChannelOperationError> {
        let channel = self.live("disable").map_err(ChannelOperationError::local)?;
        channel_operation(true, |result, d, c, r| {
            // SAFETY: this exclusive View borrow keeps the native wrapper live;
            // native resolves the weak family and guards it across provider entry.
            // The output and diagnostic storage are valid through this call.
            unsafe { sys::ams_mel_ir_channel_disable(channel, result, d, c, r) }
        })
    }

    fn buffer_operation(
        &mut self,
        buffer: &mut HostBuffer,
        name: &str,
        operation: BufferOperation,
    ) -> Result<(), ChannelOperationError> {
        let channel = self.live(name).map_err(ChannelOperationError::local)?;
        let raw_buffer = buffer.live().map_err(ChannelOperationError::local)?;
        channel_operation(false, |result, d, c, r| {
            // SAFETY: the exclusive borrows keep both wrappers live throughout
            // this synchronous call. Native enforces identity and lifecycle and
            // retains the necessary graph; no Rust memory is registered. The
            // output and diagnostic slots are valid only for the call.
            unsafe { operation(channel, raw_buffer, result, d, c, r) }
        })
    }
}

fn channel_operation(
    allow_noop: bool,
    call: impl FnOnce(&mut u32, *mut c_char, usize, *mut usize) -> i32,
) -> Result<(), ChannelOperationError> {
    let mut result = u32::MAX;
    let mut status = sys::AMS_MEL_OK;
    call_with_diagnostic(|d, c, r| {
        status = call(&mut result, d, c, r);
        status
    })
    .and_then(|()| match returned(result) {
        Some(sys::AMS_MEL_IR_RETURN_SUCCESS) => Ok(()),
        None if allow_noop => Ok(()),
        _ => Err(protocol(
            "successful Channel operation returned an inconsistent provider Return",
        )),
    })
    .map_err(|error| ChannelOperationError {
        error,
        native_status: Some(status),
        provider_return: returned(result),
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::ErrorKind;

    #[test]
    fn status_and_return_survive_malformed_diagnostics() {
        let unknown = 0xfedc_ba98;
        let status = -123;
        for (diagnostic, kind) in [
            (&b"unterminated"[..], ErrorKind::ProtocolInconsistency),
            (&b"\xff\0"[..], ErrorKind::InvalidUtf8),
        ] {
            let error = channel_operation(false, |value, d, capacity, required| {
                assert_eq!(*value, u32::MAX);
                assert!(diagnostic.len() <= capacity);
                *value = unknown;
                // SAFETY: initialized, disjoint buffers; length fits capacity.
                unsafe {
                    d.cast::<u8>()
                        .copy_from_nonoverlapping(diagnostic.as_ptr(), diagnostic.len());
                    *required = diagnostic.len();
                }
                status
            })
            .unwrap_err();
            assert_eq!(error.error.kind(), &kind);
            assert_eq!(error.native_status, Some(status));
            assert_eq!(error.provider_return, Some(unknown));
        }
    }

    #[test]
    fn bridge_failure_does_not_become_provider_success_and_only_disable_allows_noop() {
        let call = |status, value| {
            move |result: &mut u32, _: *mut c_char, _: usize, r: *mut usize| {
                *result = value;
                // SAFETY: helper provides writable length and zeroed diagnostic.
                unsafe { *r = 1 };
                status
            }
        };
        let error = channel_operation(true, call(sys::AMS_MEL_PROVIDER_FAILED, 0)).unwrap_err();
        assert_eq!(error.error.kind(), &ErrorKind::ProviderFailed);
        assert_eq!(error.native_status, Some(sys::AMS_MEL_PROVIDER_FAILED));
        assert_eq!(error.provider_return, Some(0));
        let error = channel_operation(false, call(sys::AMS_MEL_OK, u32::MAX)).unwrap_err();
        assert_eq!(error.error.kind(), &ErrorKind::ProtocolInconsistency);
        assert_eq!(error.native_status, Some(sys::AMS_MEL_OK));
        assert_eq!(error.provider_return, None);
        assert!(channel_operation(true, call(sys::AMS_MEL_OK, u32::MAX)).is_ok());
        let error = channel_operation(true, call(sys::AMS_MEL_OK, 123)).unwrap_err();
        assert_eq!(error.error.kind(), &ErrorKind::ProtocolInconsistency);
        assert_eq!(error.provider_return, Some(123));
    }
}
