//! Opaque callback ownership for control-only Image. Queueing, lifetime pins,
//! and release reconciliation belong entirely to the native bridge.

use std::{ffi::c_char, fmt, ptr, ptr::NonNull};

use ams_mel_sys as sys;

use crate::{call_with_diagnostic, protocol, Error, ErrorKind, ImageStream};

/// Cumulative, saturating diagnostics from one native snapshot.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ImageCheckoutCounters {
    /// All callbacks, including null deliveries.
    pub callbacks_received: u64,
    /// Callback Buffers accepted into the FIFO.
    pub checkouts_accepted: u64,
    /// Rejections at FIFO or outstanding checkout capacity.
    pub queue_overflow: u64,
    /// All callbacks not accepted as checkouts, including overflow and nulls.
    pub callbacks_rejected: u64,
    /// Successful releases followed by safe callback-wrapper destruction.
    pub releases_succeeded: u64,
    /// Failed releases or conservative retention without release.
    pub releases_uncertain: u64,
}

/// Failure to release an opaque checkout.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CheckoutReleaseError {
    /// Existing error category and diagnostic, including protocol failures.
    pub error: Error,
    /// Exact bridge status when a native call was made.
    pub native_status: Option<i32>,
    /// Exact numeric provider `Return`, independent of bridge status. The ABI's
    /// `UINT32_MAX` sentinel maps to `None`, including on provider exception.
    pub provider_return: Option<u32>,
}

impl fmt::Display for CheckoutReleaseError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{} (native status {:?}, release Return {:?})",
            self.error, self.native_status, self.provider_return
        )
    }
}

impl std::error::Error for CheckoutReleaseError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        Some(&self.error)
    }
}

/// One-shot bounded FIFO of opaque Image callbacks.
///
/// The native owner retains the Image and Session graph without borrowing their
/// Rust owners. A logical stop may end reception before this wrapper is closed.
/// Operations on this owner are serialized; it is neither `Send` nor `Sync`.
pub struct ImageCheckoutReceiver {
    handle: Option<NonNull<sys::AmsMelIrImageCheckout>>,
}

/// Unique release obligation for the exact Buffer delivered by an Image callback.
///
/// This is not the registered application [`crate::HostBuffer`] and exposes no
/// address, bytes, dimensions, header, or layout. Neither the receiver nor the
/// public Image or Session is borrowed. An outstanding lease blocks unregister.
/// Prefer explicit [`Self::close`] to inspect the release result. Drop is only
/// a best-effort attempt; a failed release can retain the native graph permanently.
///
/// ```compile_fail
/// fn clone_owner<T: Clone>() {}
/// clone_owner::<ams_mel_ir::ImageCheckoutLease>();
/// ```
/// ```compile_fail
/// fn send<T: Send>() {}
/// send::<ams_mel_ir::ImageCheckoutLease>();
/// ```
/// ```compile_fail
/// fn share<T: Sync>() {}
/// share::<ams_mel_ir::ImageCheckoutLease>();
/// ```
pub struct ImageCheckoutLease {
    handle: Option<NonNull<sys::AmsMelIrImageCheckoutLease>>,
}

impl fmt::Debug for ImageCheckoutReceiver {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("ImageCheckoutReceiver")
            .field("is_open", &self.is_open())
            .finish()
    }
}

impl fmt::Debug for ImageCheckoutLease {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("ImageCheckoutLease")
            .field("is_open", &self.is_open())
            .finish()
    }
}

impl ImageStream {
    /// Installs the native preallocated checkout FIFO on an attached control-only
    /// Image, before its first Start. This does not install another provider
    /// listener or allocate image buffers. Close does not permit reopening.
    ///
    /// `queue_capacity` and `max_checkouts` must be positive, and the FIFO bound
    /// must not exceed `max_checkouts` (the maximum queued plus leased owners).
    /// Exhaustion rejects the incoming checkout; the native release engine handles
    /// it. A misbehaving provider can exhaust the native emergency reserve, which
    /// is fail-stop rather than destroy unreturned Buffers.
    ///
    /// # Errors
    /// Invalid capacities, a closed or ineligible Image (including Mono8
    /// acquisition), previous installation, or a native failure are reported.
    pub fn open_checkout_receiver(
        &mut self,
        queue_capacity: usize,
        max_checkouts: usize,
    ) -> Result<ImageCheckoutReceiver, Error> {
        if self.raw.is_null() {
            return Err(Error::new(
                ErrorKind::ProviderFailed,
                "image stream is closed",
            ));
        }
        if queue_capacity == 0 || queue_capacity > max_checkouts || max_checkouts > usize::MAX - 2 {
            return Err(Error::new(
                ErrorKind::InvalidArgument,
                "invalid checkout capacities",
            ));
        }
        let handle = acquire(|out, d, c, r| {
            // SAFETY: exclusive access to the live Image wrapper and writable
            // call-scoped output slots. Native retains its own Image graph.
            unsafe {
                sys::ams_mel_ir_image_checkout_open(
                    self.raw,
                    queue_capacity,
                    max_checkouts,
                    out,
                    d,
                    c,
                    r,
                )
            }
        })?;
        Ok(ImageCheckoutReceiver {
            handle: Some(handle),
        })
    }
}

impl ImageCheckoutReceiver {
    /// Whether this wrapper still owns a usable receiver handle, not whether
    /// the channel is running.
    pub fn is_open(&self) -> bool {
        self.handle.is_some()
    }

    /// Receives the oldest accepted checkout. Zero milliseconds polls without
    /// blocking on delivery. Timeout does not cancel reception, consume a
    /// checkout, or affect registration. Only one Receive can borrow this owner.
    ///
    /// # Errors
    /// Reports [`ErrorKind::Timeout`] if nothing arrives by the deadline,
    /// [`ErrorKind::StreamStopped`] if stopped and drained or locally closed,
    /// and native or binding protocol failures otherwise.
    pub fn receive(&mut self, timeout_ms: u32) -> Result<ImageCheckoutLease, Error> {
        let receiver = self.live()?;
        let handle = acquire(|out, d, c, r| {
            // SAFETY: the exclusive borrow prevents competing Receive or Close;
            // native transfers an independent lease into the null output slot.
            unsafe {
                sys::ams_mel_ir_image_checkout_receive(receiver.as_ptr(), timeout_ms, out, d, c, r)
            }
        })?;
        Ok(ImageCheckoutLease {
            handle: Some(handle),
        })
    }

    /// Copies the six native counters, including work for independently held
    /// leases. Take the snapshot before closing this receiver.
    ///
    /// # Errors
    /// Reports a closed receiver or native/binding failure.
    pub fn counters(&self) -> Result<ImageCheckoutCounters, Error> {
        let receiver = self.live()?;
        let mut raw = sys::AmsMelIrImageCheckoutCountersV1::default();
        call_with_diagnostic(|d, c, r| {
            // SAFETY: receiver stays live throughout this read, and all output
            // storage is writable and scoped to the call.
            unsafe {
                sys::ams_mel_ir_image_checkout_get_counters(receiver.as_ptr(), &mut raw, d, c, r)
            }
        })?;
        Ok(ImageCheckoutCounters {
            callbacks_received: raw.callbacks_received,
            checkouts_accepted: raw.checkouts_accepted,
            queue_overflow: raw.queue_overflow,
            callbacks_rejected: raw.callbacks_rejected,
            releases_succeeded: raw.releases_succeeded,
            releases_uncertain: raw.releases_uncertain,
        })
    }

    /// Stops admission and drains queued callbacks through native release. An
    /// acquired lease survives. This does not disable, unregister, or detach.
    /// Already closed receivers succeed without calling native code.
    ///
    /// # Errors
    /// Failed draining is reported even when the receiver was consumed. Use
    /// [`Self::is_open`] to distinguish this from a pre-admission refusal.
    pub fn close(&mut self) -> Result<(), Error> {
        self.close_with(|slot, d, c, r| {
            // SAFETY: the slot holds the unique receiver being closed. No
            // competing operation or borrowed native storage survives this call.
            unsafe { sys::ams_mel_ir_image_checkout_close(slot, d, c, r) }
        })
    }

    fn close_with(
        &mut self,
        call: impl FnOnce(&mut *mut sys::AmsMelIrImageCheckout, *mut c_char, usize, *mut usize) -> i32,
    ) -> Result<(), Error> {
        let Some(original) = self.handle.take() else {
            return Ok(());
        };
        let mut output = original.as_ptr();
        let mut status = sys::AMS_MEL_OK;
        let diagnostic = call_with_diagnostic(|d, c, r| {
            status = call(&mut output, d, c, r);
            status
        });
        reconcile_close(&mut self.handle, original, output, status, true, diagnostic)
    }

    fn live(&self) -> Result<NonNull<sys::AmsMelIrImageCheckout>, Error> {
        self.handle
            .ok_or_else(|| Error::new(ErrorKind::StreamStopped, "checkout receiver is closed"))
    }
}

impl Drop for ImageCheckoutReceiver {
    fn drop(&mut self) {
        if let Some(handle) = self.handle.take() {
            // SAFETY: unique validated owner. No diagnostic allocation, no retry,
            // and no use of the handle slot after the best-effort call.
            unsafe {
                sys::ams_mel_ir_image_checkout_close(
                    &mut handle.as_ptr(),
                    ptr::null_mut(),
                    0,
                    ptr::null_mut(),
                )
            };
        }
    }
}

impl ImageCheckoutLease {
    /// Whether this wrapper still holds a usable lease handle.
    pub fn is_open(&self) -> bool {
        self.handle.is_some()
    }

    /// Releases the exact callback Buffer, at most once. Already closed leases
    /// succeed without a native call. Provider refusal or exception consumes the
    /// public lease and leaves the uncertain native ownership retained, not retried.
    ///
    /// # Errors
    /// Reports bridge failures and the independent provider Return. Check
    /// [`Self::is_open`] after an error: a pre-admission refusal does not consume
    /// a valid handle, whereas a release failure does.
    pub fn close(&mut self) -> Result<(), CheckoutReleaseError> {
        self.close_with(|slot, result, d, c, r| {
            // SAFETY: the slot holds the unique live lease; all writable output
            // storage is disjoint and valid for the synchronous call.
            unsafe { sys::ams_mel_ir_image_checkout_lease_close(slot, result, d, c, r) }
        })
    }

    fn close_with(
        &mut self,
        call: impl FnOnce(
            &mut *mut sys::AmsMelIrImageCheckoutLease,
            &mut u32,
            *mut c_char,
            usize,
            *mut usize,
        ) -> i32,
    ) -> Result<(), CheckoutReleaseError> {
        let Some(original) = self.handle.take() else {
            return Ok(());
        };
        let mut output = original.as_ptr();
        let mut result = u32::MAX;
        let mut status = sys::AMS_MEL_OK;
        let diagnostic = call_with_diagnostic(|d, c, r| {
            status = call(&mut output, &mut result, d, c, r);
            status
        });
        let provider_return = (result != u32::MAX).then_some(result);
        reconcile_close(
            &mut self.handle,
            original,
            output,
            status,
            provider_return.is_none(),
            diagnostic,
        )
        .and_then(|()| {
            if result == sys::AMS_MEL_IR_RETURN_SUCCESS {
                Ok(())
            } else {
                Err(protocol(
                    "successful checkout release did not report provider Success",
                ))
            }
        })
        .map_err(|error| CheckoutReleaseError {
            error,
            native_status: Some(status),
            provider_return,
        })
    }
}

impl Drop for ImageCheckoutLease {
    fn drop(&mut self) {
        if let Some(handle) = self.handle.take() {
            let mut ignored_return = u32::MAX;
            // SAFETY: unique validated lease and writable call-scoped Return.
            // Drop never decodes diagnostics or revisits a consumed handle.
            unsafe {
                sys::ams_mel_ir_image_checkout_lease_close(
                    &mut handle.as_ptr(),
                    &mut ignored_return,
                    ptr::null_mut(),
                    0,
                    ptr::null_mut(),
                )
            };
        }
    }
}

// Do not arm a Drop owner until both the native output contract and the
// diagnostic have been validated. An untrusted output is never closed, even
// if that means retaining its native graph permanently.
fn acquire<T>(
    call: impl FnOnce(&mut *mut T, *mut c_char, usize, *mut usize) -> i32,
) -> Result<NonNull<T>, Error> {
    let mut output = ptr::null_mut();
    let mut status = sys::AMS_MEL_OK;
    let diagnostic = call_with_diagnostic(|d, c, r| {
        status = call(&mut output, d, c, r);
        status
    });
    if status != sys::AMS_MEL_OK && !output.is_null() {
        return Err(protocol(
            "failed checkout operation wrote a non-null handle",
        ));
    }
    diagnostic?;
    NonNull::new(output).ok_or_else(|| protocol("successful checkout operation returned no handle"))
}

fn reconcile_close<T>(
    handle: &mut Option<NonNull<T>>,
    original: NonNull<T>,
    output: *mut T,
    status: i32,
    no_provider_return: bool,
    diagnostic: Result<(), Error>,
) -> Result<(), Error> {
    if output.is_null() {
        // Native consumed the handle, irrespective of status/diagnostic.
        return diagnostic;
    }
    if output == original.as_ptr() && status == sys::AMS_MEL_INVALID_ARGUMENT && no_provider_return
    {
        // In this ABI InvalidArgument can only refuse before the ownership
        // exchange. The diagnostic may itself be malformed; that does not
        // change the native ownership outcome.
        *handle = Some(original);
        return diagnostic;
    }
    // Replacement, success without consumption, or any other inconsistent
    // result gives no safe handle to retry, including from Drop.
    Err(protocol("checkout Close returned an inconsistent handle"))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn write_diagnostic(d: *mut c_char, c: usize, r: *mut usize, message: &[u8]) {
        assert!(message.len() <= c);
        // SAFETY: in these seams d and r are the independent writable slots
        // supplied by call_with_diagnostic, valid for this call only.
        unsafe {
            d.cast::<u8>()
                .copy_from_nonoverlapping(message.as_ptr(), message.len());
            *r = message.len();
        }
    }

    #[test]
    fn acquisition_never_adopts_a_contradictory_output() {
        let fake = NonNull::<u8>::dangling().as_ptr();
        for (status, written, message, kind) in [
            (
                sys::AMS_MEL_OK,
                ptr::null_mut(),
                &b"\0"[..],
                ErrorKind::ProtocolInconsistency,
            ),
            (
                sys::AMS_MEL_TIMEOUT,
                fake,
                &b"timed out\0"[..],
                ErrorKind::ProtocolInconsistency,
            ),
            (
                sys::AMS_MEL_OK,
                fake,
                &b"\xff\0"[..],
                ErrorKind::InvalidUtf8,
            ),
            (
                sys::AMS_MEL_OK,
                fake,
                &b"x"[..],
                ErrorKind::ProtocolInconsistency,
            ),
            (
                sys::AMS_MEL_OK,
                fake,
                &b"unexpected\0"[..],
                ErrorKind::ProtocolInconsistency,
            ),
        ] {
            let error = acquire::<u8>(|out, d, c, r| {
                assert!(out.is_null());
                *out = written;
                write_diagnostic(d, c, r, message);
                status
            })
            .unwrap_err();
            assert_eq!(error.kind(), &kind);
        }
    }

    #[test]
    fn lease_close_reconciles_handle_before_interpreting_status_and_return() {
        const OK: i32 = sys::AMS_MEL_OK;
        const FAILED: i32 = sys::AMS_MEL_PROVIDER_FAILED;
        const INVALID: i32 = sys::AMS_MEL_INVALID_ARGUMENT;
        const NONE: u32 = u32::MAX;
        const UNSUPPORTED: u32 = sys::AMS_MEL_IR_RETURN_NOT_SUPPORTED;
        // Non-owning fabricated addresses are used solely in the private output
        // seam, never passed to a native call (including Drop).
        let original = NonNull::dangling();
        let replacement = ptr::without_provenance_mut(8);
        for (output, status, returned, message, remains_open, kind) in [
            (
                ptr::null_mut(),
                FAILED,
                UNSUPPORTED,
                &b"refused\0"[..],
                false,
                ErrorKind::ProviderFailed,
            ),
            (
                ptr::null_mut(),
                FAILED,
                0,
                &b"failure\0"[..],
                false,
                ErrorKind::ProviderFailed,
            ),
            (
                ptr::null_mut(),
                -123,
                0xfedc_ba98,
                &b"\xff\0"[..],
                false,
                ErrorKind::InvalidUtf8,
            ),
            (
                ptr::null_mut(),
                OK,
                0,
                &b"x"[..],
                false,
                ErrorKind::ProtocolInconsistency,
            ),
            (
                ptr::null_mut(),
                OK,
                NONE,
                &b"\0"[..],
                false,
                ErrorKind::ProtocolInconsistency,
            ),
            (
                ptr::null_mut(),
                OK,
                UNSUPPORTED,
                &b"\0"[..],
                false,
                ErrorKind::ProtocolInconsistency,
            ),
            (
                ptr::null_mut(),
                INVALID,
                NONE,
                &b"invalid\0"[..],
                false,
                ErrorKind::InvalidArgument,
            ),
            (
                original.as_ptr(),
                INVALID,
                NONE,
                &b"\xff\0"[..],
                true,
                ErrorKind::InvalidUtf8,
            ),
            (
                original.as_ptr(),
                FAILED,
                NONE,
                &b"failure\0"[..],
                false,
                ErrorKind::ProtocolInconsistency,
            ),
            (
                original.as_ptr(),
                INVALID,
                UNSUPPORTED,
                &b"invalid\0"[..],
                false,
                ErrorKind::ProtocolInconsistency,
            ),
            (
                original.as_ptr(),
                OK,
                0,
                &b"\0"[..],
                false,
                ErrorKind::ProtocolInconsistency,
            ),
            (
                replacement,
                INVALID,
                NONE,
                &b"invalid\0"[..],
                false,
                ErrorKind::ProtocolInconsistency,
            ),
        ] {
            let mut lease = std::mem::ManuallyDrop::new(ImageCheckoutLease {
                handle: Some(original),
            });
            let error = lease
                .close_with(|slot, result, d, c, r| {
                    assert_eq!(*slot, original.as_ptr());
                    assert_eq!(*result, NONE);
                    *slot = output;
                    *result = returned;
                    write_diagnostic(d, c, r, message);
                    status
                })
                .unwrap_err();
            assert_eq!(lease.is_open(), remains_open);
            assert_eq!(error.error.kind(), &kind);
            assert_eq!(error.native_status, Some(status));
            assert_eq!(
                error.provider_return,
                (returned != NONE).then_some(returned)
            );
            if !remains_open {
                lease
                    .close_with(|_, _, _, _, _| panic!("attempted another release"))
                    .unwrap();
            }
        }
    }

    #[test]
    fn receiver_close_distinguishes_drain_failure_from_pre_admission_refusal() {
        let original = NonNull::dangling();
        let mut receiver = std::mem::ManuallyDrop::new(ImageCheckoutReceiver {
            handle: Some(original),
        });
        let error = receiver
            .close_with(|slot, d, c, r| {
                assert_eq!(*slot, original.as_ptr());
                write_diagnostic(d, c, r, b"invalid\0");
                sys::AMS_MEL_INVALID_ARGUMENT
            })
            .unwrap_err();
        assert_eq!(error.kind(), &ErrorKind::InvalidArgument);
        assert!(receiver.is_open());
        let error = receiver
            .close_with(|slot, d, c, r| {
                assert_eq!(*slot, original.as_ptr());
                *slot = ptr::null_mut();
                write_diagnostic(d, c, r, b"drain failed\0");
                sys::AMS_MEL_PROVIDER_FAILED
            })
            .unwrap_err();
        assert_eq!(error.kind(), &ErrorKind::ProviderFailed);
        assert!(!receiver.is_open());
        receiver
            .close_with(|_, _, _, _| panic!("attempted another drain"))
            .unwrap();
    }
}
