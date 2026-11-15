//! Copied common ChannelCommsTestRep reception; native code owns the callback and FIFO.

use std::{ffi::c_char, fmt, ptr, rc::Rc};

use ams_mel_sys as sys;

use crate::{
    call_with_diagnostic, command_return, protocol, ChannelView, CommandReturn, CommsTestReport,
    Error, ErrorKind,
};

/// Snapshot of saturating native callback counters, never reset by reading.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct CommsMetadataCounters {
    /// Callback invocations, including null reports, queue-full drops and inactive delivery;
    /// not a count of successful Rust Receive operations.
    pub events_received: u64,
    /// Incoming valid reports dropped at capacity. Older FIFO entries are never evicted.
    pub events_dropped_queue_full: u64,
    /// Malformed or unsupported reports, including null callback payloads.
    pub malformed_or_unsupported: u64,
}

/// Registration failure: the operation error and actual provider Return are independent.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CommsMetadataOpenError {
    error: Error,
    registration: Option<CommandReturn>,
}

impl CommsMetadataOpenError {
    fn preflight(error: Error) -> Self {
        Self {
            error,
            registration: None,
        }
    }

    /// Native/safe-layer error and diagnostic, not a provider Return inferred from text.
    pub fn error(&self) -> &Error {
        &self.error
    }

    /// Actual published provider Return, if obtained and recognized.
    /// Local failures, exceptions before Return, and unknown values produce `None`.
    pub fn registration(&self) -> Option<CommandReturn> {
        self.registration
    }
}

impl fmt::Display for CommsMetadataOpenError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{} (CommsTest registration: {:?})",
            self.error, self.registration
        )
    }
}

impl std::error::Error for CommsMetadataOpenError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        Some(&self.error)
    }
}

type Close = unsafe extern "C" fn(
    *mut *mut sys::AmsMelIrChannelCommsMetadata,
    *mut c_char,
    usize,
    *mut usize,
) -> i32;

/// Private unique receiver guard. Taking the handle *before* Close ensures that
/// malformed diagnostics or uncertain consumption never cause another release.
struct Receiver {
    raw: *mut sys::AmsMelIrChannelCommsMetadata,
    close: Close,
}

impl fmt::Debug for Receiver {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Receiver")
            .field("is_open", &!self.raw.is_null())
            .finish_non_exhaustive()
    }
}

impl Receiver {
    fn close(&mut self) -> Result<(), Error> {
        let mut raw = std::mem::replace(&mut self.raw, ptr::null_mut());
        if raw.is_null() {
            return Ok(());
        }
        call_with_diagnostic(|d, c, r| {
            // SAFETY: this guard uniquely owns raw, with no further release attempt.
            unsafe { (self.close)(&mut raw, d, c, r) }
        })?;
        if !raw.is_null() {
            return Err(protocol(
                "Comms metadata Close succeeded without clearing its owner",
            ));
        }
        Ok(())
    }
}

impl Drop for Receiver {
    fn drop(&mut self) {
        let mut raw = std::mem::replace(&mut self.raw, ptr::null_mut());
        if !raw.is_null() {
            // SAFETY: unique owner; null diagnostics are supported. No allocation or panic.
            let _ = unsafe { (self.close)(&mut raw, ptr::null_mut(), 0, ptr::null_mut()) };
        }
    }
}

/// Independent, single-consumer native bounded FIFO of owned two-ID reports.
///
/// No Rust parent reference, callback, queue, worker or executor is retained.
/// Closing a weak View does not close this stream; closing this stream neither
/// disables/detaches its typed channel nor cancels pending requests. Native Close
/// deactivates consumption, **not** upstream callback registration or quiescence.
/// Native callback retention controls provider lifetime; uncertain provider
/// teardown can retain the graph indefinitely. Prefer explicit Close; Drop makes
/// at most one non-panicking best-effort release attempt.
///
/// ```compile_fail
/// fn send<T: Send>() {}
/// send::<ams_mel::CommsMetadataStream>();
/// ```
/// ```compile_fail
/// fn sync<T: Sync>() {}
/// sync::<ams_mel::CommsMetadataStream>();
/// ```
#[derive(Debug)]
pub struct CommsMetadataStream {
    owner: Receiver,
    _not_send_sync: Rc<()>,
}

impl ChannelView {
    /// Registers common communications-test metadata exactly once on the typed channel.
    ///
    /// Eligible: Health/Stacked Attached or Enabled; Image Attached or Running,
    /// including control-only zero-layout Image. No acquisition is configured.
    /// A refused/throwing registration may consume the one-shot, even through
    /// another View. Never automatically retries a side-effecting Open.
    /// C2 preserves native rejection: use [`crate::ControlChannel::open_metadata`]
    /// and [`crate::C2MetadataStream::enable_comms_test_events`] instead.
    ///
    /// # Errors
    /// Closed Views and zero capacity fail locally. Native bridge errors and
    /// provider registration refusal remain separate in [`CommsMetadataOpenError`].
    /// Contradictory successful outputs are protocol errors; an unexpected owner
    /// on any failed Open receives one best-effort Close, preserving the primary error.
    ///
    /// ```no_run
    /// use ams_mel::{Session, HealthConfig, CommsTestReport};
    /// fn receive_health(session: &Session, config: &HealthConfig)
    ///     -> Result<CommsTestReport, Box<dyn std::error::Error>> {
    ///     let mut health = session.open_health_channel(config)?;
    ///     let mut view = health.channel_view()?;
    ///     let mut metadata = view.open_comms_metadata(16)?;
    ///     health.enable()?;
    ///     let report = metadata.receive(1000)?;
    ///     let counters = metadata.counters()?;
    ///     assert!(counters.events_received >= 1);
    ///     metadata.close()?;
    ///     view.close()?;
    ///     health.close()?;
    ///     Ok(report)
    /// }
    /// ```
    pub fn open_comms_metadata(
        &mut self,
        queue_capacity: usize,
    ) -> Result<CommsMetadataStream, CommsMetadataOpenError> {
        let channel = self
            .live("open_comms_metadata")
            .map_err(CommsMetadataOpenError::preflight)?;
        CommsMetadataStream::open_with(
            queue_capacity,
            sys::ams_mel_ir_channel_comms_metadata_close,
            |out, registration, d, c, r| {
                // SAFETY: live weak View, initialized writable outputs and exact seven-argument ABI.
                unsafe {
                    sys::ams_mel_ir_channel_comms_metadata_open(
                        channel,
                        queue_capacity,
                        out,
                        registration,
                        d,
                        c,
                        r,
                    )
                }
            },
        )
    }
}

impl CommsMetadataStream {
    fn open_with(
        capacity: usize,
        close: Close,
        open: impl FnOnce(
            *mut *mut sys::AmsMelIrChannelCommsMetadata,
            *mut u32,
            *mut c_char,
            usize,
            *mut usize,
        ) -> i32,
    ) -> Result<Self, CommsMetadataOpenError> {
        if capacity == 0 {
            return Err(CommsMetadataOpenError::preflight(Error::new(
                ErrorKind::InvalidArgument,
                "Comms metadata capacity must be positive",
            )));
        }
        let mut owner = Receiver {
            raw: ptr::null_mut(),
            close,
        };
        let mut registration = u32::MAX;
        let result =
            call_with_diagnostic(|d, c, r| open(&mut owner.raw, &mut registration, d, c, r));
        let registration = if registration == u32::MAX {
            None
        } else {
            command_return(registration).ok()
        };
        result.map_err(|error| CommsMetadataOpenError {
            error,
            registration,
        })?;
        if registration != Some(CommandReturn::Success) || owner.raw.is_null() {
            return Err(CommsMetadataOpenError {
                error: protocol(
                    "Comms metadata Open succeeded with inconsistent owner/registration",
                ),
                registration,
            });
        }
        Ok(Self {
            owner,
            _not_send_sync: Rc::new(()),
        })
    }

    /// Local consumption-owner presence only; not channel health or callback admission.
    pub fn is_open(&self) -> bool {
        !self.owner.raw.is_null()
    }

    fn live(&self) -> Result<*mut sys::AmsMelIrChannelCommsMetadata, Error> {
        if self.is_open() {
            Ok(self.owner.raw)
        } else {
            Err(Error::new(
                ErrorKind::ProviderFailed,
                "Comms metadata stream is closed",
            ))
        }
    }

    /// Copies exactly one provider callback report; no channel ID is invented.
    /// Zero polls; positive milliseconds bound the wait. Timeout cancels nothing.
    /// Queued copies drain after typed-channel closure before terminal status.
    ///
    /// # Errors
    /// Empty active queues report [`ErrorKind::Timeout`]; inactive drained queues
    /// report [`ErrorKind::StreamStopped`], or [`ErrorKind::ProviderFailed`] after
    /// failed cleanup. Native and diagnostic errors retain their categories.
    /// A locally closed receiver reports `ProviderFailed`. There is one native
    /// Receive call per invocation, including malformed-output failures.
    pub fn receive(&mut self, timeout_ms: u32) -> Result<CommsTestReport, Error> {
        let raw = self.live()?;
        self.receive_with(|out, d, c, r| {
            // SAFETY: exclusive live receiver and exact writable copied-value output.
            unsafe { sys::ams_mel_ir_channel_comms_metadata_receive(raw, timeout_ms, out, d, c, r) }
        })
    }

    fn receive_with(
        &mut self,
        receive: impl FnOnce(
            *mut sys::AmsMelIrChannelCommsTestReportV1,
            *mut c_char,
            usize,
            *mut usize,
        ) -> i32,
    ) -> Result<CommsTestReport, Error> {
        let mut report = sys::AmsMelIrChannelCommsTestReportV1::default();
        call_with_diagnostic(|d, c, r| receive(&mut report, d, c, r))?;
        Ok(CommsTestReport {
            command_id: report.command_id,
            request_id: report.request_id,
        })
    }

    /// Copies all three saturating `u64` callback counters without resetting them.
    ///
    /// # Errors
    /// Closed receivers report `ProviderFailed`; native/diagnostic failures are preserved.
    pub fn counters(&mut self) -> Result<CommsMetadataCounters, Error> {
        let raw = self.live()?;
        self.counters_with(|out, d, c, r| {
            // SAFETY: exclusive live receiver and exact writable counter record.
            unsafe { sys::ams_mel_ir_channel_comms_metadata_get_counters(raw, out, d, c, r) }
        })
    }

    fn counters_with(
        &mut self,
        counters: impl FnOnce(
            *mut sys::AmsMelIrC2MetadataCountersV1,
            *mut c_char,
            usize,
            *mut usize,
        ) -> i32,
    ) -> Result<CommsMetadataCounters, Error> {
        let mut out = sys::AmsMelIrC2MetadataCountersV1::default();
        call_with_diagnostic(|d, c, r| counters(&mut out, d, c, r))?;
        Ok(CommsMetadataCounters {
            events_received: out.events_received,
            events_dropped_queue_full: out.events_dropped_queue_full,
            malformed_or_unsupported: out.malformed_or_unsupported,
        })
    }

    /// Deactivates/releases consumption, not upstream registration or pending work.
    /// Successful Close is idempotent; no result promises callback quiescence.
    ///
    /// # Errors
    /// Native cleanup/diagnostic errors are preserved. An uncertain or consumed
    /// owner is never released twice, including by Drop; no retry is attempted.
    pub fn close(&mut self) -> Result<(), Error> {
        self.owner.close()
    }
}

#[cfg(test)]
mod tests;
