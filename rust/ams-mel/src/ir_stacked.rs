//! Required StackedImage channel details, not stacked-image acquisition.
use super::*;

const CLOSED: &str = "IR Stacked channel is closed";

/// Owned identities and sensor location for StackedImage attachment.
///
/// Construction performs no provider operation. [`UciId::new`] and
/// [`ComponentLocation::new`] reject embedded NUL; empty labels are valid.
/// No dimensions, pixel format, stack geometry or acquisition storage is inferred.
///
/// ```no_run
/// use ams_mel::{ChannelType, Error, Session, StackedConfig};
/// fn attach(session: &Session) -> Result<(), Error> {
///     let capabilities = session.capabilities()?;
///     if let Some(c) = capabilities.iter()
///         .find(|c| c.channel_types.contains(&ChannelType::StackedImage)) {
///         let config = StackedConfig::new(c.channel_id.clone(),
///             c.platform_id.clone(), c.sensor_location.clone());
///         let mut stacked = session.open_stacked_channel(&config)?;
///         let snapshot = stacked.capabilities()?;
///         let mut view = stacked.channel_view()?;
///         stacked.enable()?;
///         view.close()?;
///         stacked.close()?;
///         assert!(snapshot.channel_types.contains(&ChannelType::StackedImage));
///     }
///     Ok(())
/// }
/// ```
#[derive(Clone, Debug, PartialEq)]
pub struct StackedConfig {
    channel_id: UciId,
    platform_id: UciId,
    sensor_location: ComponentLocation,
}

impl StackedConfig {
    /// Owns the complete configuration without discovery, attachment or enablement.
    pub fn new(channel_id: UciId, platform_id: UciId, sensor_location: ComponentLocation) -> Self {
        Self {
            channel_id,
            platform_id,
            sensor_location,
        }
    }
}

impl Session {
    /// Discovers and attaches StackedImage exactly once through the native facade.
    /// Returns Attached, not Enabled. The configuration is borrowed only during
    /// this call; the resulting owner does not borrow this Session.
    ///
    /// # Errors
    /// Missing advertisement is [`ErrorKind::InitializationFailed`] and leaves
    /// the Session usable. Null attachment is [`ErrorKind::FactoryFailed`].
    /// Concrete-type validation, rollback and provider exceptions retain native
    /// status and diagnostics. Malformed output is rejected without retrying Open;
    /// an unexpected published owner receives one best-effort cleanup attempt.
    pub fn open_stacked_channel(&self, config: &StackedConfig) -> Result<StackedChannel, Error> {
        open_with(
            config,
            |config, out, d, c, r| {
                // SAFETY: exact C record, borrowed strings live throughout the call,
                // Session belongs to self, and out is an initially null writable slot.
                unsafe { sys::ams_mel_ir_stacked_open(self.raw, config, out, d, c, r) }
            },
            best_effort_close,
        )
    }
}

/// Unique StackedImage owner; intentionally neither Send nor Sync.
///
/// Open returns Attached. Explicit Enable transitions to Enabled and is
/// idempotent after success. Refusal/exception makes the native lifecycle Failed;
/// Enable cannot then be retried and capability operations fail.
///
/// Weak [`ChannelView`]s do not retain this owner or the provider library.
/// Admitted KeepAlive/CommsTest requests have independent native completion
/// claims and share Session admission limits, with family-local accounting.
/// Close/Drop never cancel admitted work. The native graph survives parent
/// closure until those claims are safe; uncertain deferred detach can retain it
/// permanently. A cleared public handle proves neither physical teardown nor
/// callback quiescence. No callbacks or acquisition buffers are registered here.
///
/// ```compile_fail
/// fn send<T: Send>() {}
/// send::<ams_mel::StackedChannel>();
/// ```
/// ```compile_fail
/// fn sync<T: Sync>() {}
/// sync::<ams_mel::StackedChannel>();
/// ```
#[derive(Debug)]
pub struct StackedChannel {
    raw: *mut sys::AmsMelIrStacked,
    _not_send_sync: Rc<()>,
}

impl StackedChannel {
    /// Whether a native owner remains, not whether it is Enabled or operational.
    /// A logically closed owner retained for detach retry still returns true.
    pub fn is_open(&self) -> bool {
        !self.raw.is_null()
    }

    /// Explicitly enables the attached channel, without starting acquisition.
    ///
    /// # Errors
    /// Native refusal/exception marks the owner Failed and preserves its error
    /// category and diagnostic. Failed/closed lifecycle operations are refused.
    pub fn enable(&mut self) -> Result<(), Error> {
        let raw = self.live()?;
        call_with_diagnostic(|d, c, r| {
            // SAFETY: uniquely owned handle, serialized through &mut self.
            unsafe { sys::ams_mel_ir_stacked_enable(raw, d, c, r) }
        })
    }

    /// Copies the complete owned capability while Attached or Enabled.
    /// Zero layout/depth and existing unknown/reserved representations remain
    /// verbatim. The snapshot survives channel/Session closure and provider unload.
    ///
    /// # Errors
    /// Native lifecycle/query errors and shared snapshot conversion/cleanup
    /// errors retain their established categories. No partial snapshot is returned.
    pub fn capabilities(&mut self) -> Result<ChannelCapability, Error> {
        let raw = self.live()?;
        capability_snapshot(|out, d, c, r| {
            // SAFETY: live unique owner; helper owns the output and its cleanup.
            unsafe { sys::ams_mel_ir_stacked_get_capabilities(raw, out, d, c, r) }
        })
    }

    /// Creates an independent weak common Channel view without retaining this
    /// owner or its provider. Closing the view does not close this channel.
    ///
    /// # Errors
    /// A locally closed owner is ProviderFailed. Native allocation errors and
    /// malformed view output use the existing common-view error machinery.
    pub fn channel_view(&self) -> Result<ChannelView, Error> {
        let raw = self.live()?;
        channel_view_from(
            |out, d, c, r| {
                // SAFETY: source is borrowed only during creation, output starts null.
                unsafe { sys::ams_mel_ir_channel_from_stacked(raw, out, d, c, r) }
            },
            "Stacked",
        )
    }

    /// Prevents new operations, then disables (only if Enable was attempted)
    /// and detaches, or defers cleanup until admitted requests release their claims.
    /// Disable is attempted at most once. Repeated completed Close is a no-op.
    ///
    /// # Errors
    /// Disable failure is reported even when detach succeeds and consumes the
    /// handle. Failed detach can retain the handle for an explicit Close retry;
    /// only that retained handle may be offered again. Native output, including
    /// consumption on error, is authoritative. Malformed release results disarm
    /// the Rust handle rather than retrying an owner whose release is uncertain.
    /// Drop makes one non-panicking
    /// best-effort attempt on any remaining handle, never on a consumed one.
    pub fn close(&mut self) -> Result<(), Error> {
        if self.raw.is_null() {
            return Ok(());
        }
        close_with(&mut self.raw, |out, d, c, r| {
            // SAFETY: unique writable owner slot. Native either consumes it or
            // retains the same owner to allow explicit detach retry.
            unsafe { sys::ams_mel_ir_stacked_close(out, d, c, r) }
        })
    }

    fn live(&self) -> Result<*mut sys::AmsMelIrStacked, Error> {
        if self.raw.is_null() {
            Err(Error::new(ErrorKind::ProviderFailed, CLOSED))
        } else {
            Ok(self.raw)
        }
    }
}

impl Drop for StackedChannel {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            best_effort_close(&mut self.raw);
        }
    }
}

fn close_with(
    raw: &mut *mut sys::AmsMelIrStacked,
    close: impl FnOnce(*mut *mut sys::AmsMelIrStacked, *mut c_char, usize, *mut usize) -> i32,
) -> Result<(), Error> {
    let original = mem::replace(raw, ptr::null_mut());
    let mut output = original;
    let mut status = sys::AMS_MEL_INTERNAL_ERROR;
    let result = call_with_diagnostic(|d, c, r| {
        status = close(&mut output, d, c, r);
        status
    });
    // Only the documented native detach-failure result proves that the exact
    // same owner is retained for retry. Every other uncertain result leaves the
    // Rust slot disarmed; it must not reach Drop/native Close a second time.
    if status == sys::AMS_MEL_PROVIDER_FAILED
        && output == original
        && result
            .as_ref()
            .is_err_and(|e| e.kind() == &ErrorKind::ProviderFailed)
    {
        *raw = output;
    }
    result?;
    if !output.is_null() {
        return Err(protocol(
            "Stacked close succeeded without releasing the owner",
        ));
    }
    Ok(())
}

fn best_effort_close(raw: &mut *mut sys::AmsMelIrStacked) {
    // SAFETY: only an owned native handle is passed. Null diagnostics are valid.
    // A retained owner is deliberately left intact, not freed or retried here.
    let _ = unsafe { sys::ams_mel_ir_stacked_close(raw, ptr::null_mut(), 0, ptr::null_mut()) };
}

fn open_with(
    config: &StackedConfig,
    open: impl FnOnce(
        &sys::AmsMelIrStackedConfigV1,
        *mut *mut sys::AmsMelIrStacked,
        *mut c_char,
        usize,
        *mut usize,
    ) -> i32,
    close: impl FnOnce(&mut *mut sys::AmsMelIrStacked),
) -> Result<StackedChannel, Error> {
    let config = sys::AmsMelIrStackedConfigV1 {
        channel_id: raw_uci_id(&config.channel_id),
        channel_type: sys::AMS_MEL_IR_CHANNEL_STACKED_IMAGE,
        platform_id: raw_uci_id(&config.platform_id),
        sensor_location: sys::AmsMelComponentLocationV1 {
            offset_x_m: config.sensor_location.offset_x_m(),
            offset_y_m: config.sensor_location.offset_y_m(),
            offset_z_m: config.sensor_location.offset_z_m(),
            key: string_view(config.sensor_location.key()),
            system_name: string_view(config.sensor_location.system_name()),
        },
    };
    let mut raw = ptr::null_mut();
    let result = call_with_diagnostic(|d, c, r| open(&config, &mut raw, d, c, r));
    if let Err(error) = result {
        if !raw.is_null() {
            close(&mut raw);
        }
        return Err(error);
    }
    if raw.is_null() {
        return Err(protocol("Stacked open succeeded without a channel owner"));
    }
    Ok(StackedChannel {
        raw,
        _not_send_sync: Rc::new(()),
    })
}

#[cfg(test)]
mod tests;
