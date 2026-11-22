//! Scheduling ownership, common Channel services and AbortTask execution.
use super::*;
use std::{fmt, marker::PhantomData};

/// Task list action carried unchanged in [`SchedulingTaskInfo`].
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[repr(u32)]
pub enum TaskListAction {
    /// Normal action (0).
    Normal = 0,
    /// Next action (1).
    Next = 1,
    /// Immediate action (2).
    Immediate = 2,
    /// Abort action (3), not required by [`SchedulingChannel::exec_abort`].
    Abort = 3,
}

/// Owned `TaskInfo` for the upstream `AbortTask` operation.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct SchedulingTaskInfo {
    /// Signed system time in nanoseconds, with no epoch conversion.
    pub system_time_ns: i64,
    /// Full-width request ID; not a command ID.
    pub request_id: u32,
    /// Exact task list action; the operation does not override it.
    pub action: TaskListAction,
    /// Full-width precedence within the selected priority.
    pub precedence_within_priority: u32,
    /// Full-width priority, without normalization.
    pub priority: u32,
}

/// Owned channel identity, platform identity and sensor location for attachment.
#[derive(Clone, Debug, PartialEq)]
pub struct SchedulingConfig {
    channel_id: UciId,
    platform_id: UciId,
    sensor_location: ComponentLocation,
}

impl SchedulingConfig {
    /// Stores the complete values without contacting the provider.
    pub fn new(channel_id: UciId, platform_id: UciId, sensor_location: ComponentLocation) -> Self {
        Self {
            channel_id,
            platform_id,
            sensor_location,
        }
    }
}

impl Session {
    /// Attaches a Scheduling channel without enabling it. The owner does not
    /// borrow the configuration or the public Session.
    ///
    /// # Errors
    /// Missing Scheduling advertisement is [`ErrorKind::InitializationFailed`]
    /// and leaves the Session usable. Attachment, validation and rollback are
    /// delegated to the native facade; native failures keep their diagnostics.
    pub fn open_scheduling_channel(
        &self,
        config: &SchedulingConfig,
    ) -> Result<SchedulingChannel, Error> {
        open_with(
            config,
            |config, out, d, c, r| {
                // SAFETY: the Session and config (including its borrowed text)
                // are live through the call; the writable owner slot starts null.
                unsafe { sys::ams_mel_ir_scheduling_open(self.raw, config, out, d, c, r) }
            },
            best_effort_close,
        )
    }
}

/// Unique owner of an attached Scheduling channel. Not `Clone`, `Send`, or `Sync`.
///
/// The native owner survives public Session closure. Weak [`ChannelView`]s do
/// not keep it or its provider alive. Admitted requests own independent
/// native completion claims; timeout, request Close and parent closure do not
/// cancel provider work.
///
/// ```compile_fail,E0277
/// fn require_send<T: Send>() {}
/// require_send::<ams_mel_ir::SchedulingChannel>();
/// ```
/// ```compile_fail,E0277
/// fn require_sync<T: Sync>() {}
/// require_sync::<ams_mel_ir::SchedulingChannel>();
/// ```
/// ```compile_fail,E0277
/// fn require_clone<T: Clone>() {}
/// require_clone::<ams_mel_ir::SchedulingChannel>();
/// ```
pub struct SchedulingChannel {
    raw: *mut sys::AmsMelIrScheduling,
    _not_send_sync: PhantomData<Rc<()>>,
}

impl fmt::Debug for SchedulingChannel {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("SchedulingChannel")
            .field("is_open", &self.is_open())
            .finish_non_exhaustive()
    }
}

impl SchedulingChannel {
    /// Whether a native owner remains, including one retained only for Close retry.
    /// This does not indicate whether the channel is Enabled or operational.
    pub fn is_open(&self) -> bool {
        !self.raw.is_null()
    }

    /// Explicitly enables the attached channel. Idempotent after success.
    ///
    /// # Errors
    /// A provider refusal or exception fails the native lifecycle and preserves
    /// its status and diagnostic. Subsequent calls cannot retry provider Enable.
    pub fn enable(&mut self) -> Result<(), Error> {
        let raw = self.live()?;
        call_with_diagnostic(|d, c, r| {
            // SAFETY: unique live owner, serialized through &mut self.
            unsafe { sys::ams_mel_ir_scheduling_enable(raw, d, c, r) }
        })
    }

    /// Submits the upstream `SchedulingChannel::exec(AbortTask)` once, on an
    /// Enabled channel. Native code copies all five fields during submission.
    /// It does not infer a command ID or require [`TaskListAction::Abort`].
    ///
    /// The independent request survives closure of the channel, Session and
    /// weak views. Polling, timeouts and request Close/Drop do not cancel work.
    /// Inspect the cached [`ReturnResult`] for a provider return or explicit
    /// MEL rejection; neither submission nor completion proves a physical abort.
    /// This operation does not register or consume Scheduling metadata.
    ///
    /// # Errors
    /// Refuses Attached, Failed and Closed channels. Native admission and
    /// provider failures retain their error categories and available diagnostics.
    /// Malformed diagnostics or a missing successful request owner are errors.
    /// A submission error can occur after dispatch and is not proof that the
    /// provider did not execute. This method never retries submission.
    pub fn exec_abort(&mut self, info: &SchedulingTaskInfo) -> Result<ReturnRequest, Error> {
        let raw = self.live()?;
        submit_abort_with(info, |info, out, d, c, r| {
            // SAFETY: the unique borrow serializes submission against Close. Native
            // checks Enabled and copies info synchronously. All outputs are writable.
            unsafe { sys::ams_mel_ir_scheduling_exec_abort(raw, info, out, d, c, r) }
        })
    }

    /// Copies a complete capability snapshot while Attached or Enabled.
    /// It outlives the channel and provider. `task_schedule_depth` is an unmodified
    /// `u32`: zero is a valid application-managed priority mode, not lack of support.
    ///
    /// # Errors
    /// Native errors and shared snapshot conversion errors are reported without
    /// returning a partial snapshot.
    pub fn capabilities(&mut self) -> Result<ChannelCapability, Error> {
        let raw = self.live()?;
        capability_snapshot(|out, d, c, r| {
            // SAFETY: live unique owner; helper manages the snapshot and cleanup.
            unsafe { sys::ams_mel_ir_scheduling_get_capabilities(raw, out, d, c, r) }
        })
    }

    /// Creates a weak common Channel view. Closing it does not close this owner.
    ///
    /// # Errors
    /// A consumed owner is [`ErrorKind::ProviderFailed`]. Other errors follow
    /// the existing common-view output contract.
    pub fn channel_view(&self) -> Result<ChannelView, Error> {
        let raw = self.live()?;
        channel_view_from(
            |out, d, c, r| {
                // SAFETY: the source is live during view creation; output starts null.
                unsafe { sys::ams_mel_ir_channel_from_scheduling(raw, out, d, c, r) }
            },
            "Scheduling",
        )
    }

    /// Stops admission, then delegates disable/detach to native code. Disable
    /// runs at most once and only if Enable was attempted. Pending requests can
    /// defer physical teardown. Closing an already consumed owner is a no-op.
    ///
    /// # Errors
    /// Disable failure is reported even when detach consumes the owner. Only a
    /// documented detach failure retaining the same handle permits a retry.
    /// Contradictory release outputs disarm the Rust owner. Drop makes one
    /// diagnostic-free, best-effort attempt on a remaining owner.
    pub fn close(&mut self) -> Result<(), Error> {
        close_with(&mut self.raw, |out, d, c, r| {
            // SAFETY: unique writable slot. Native can consume the owner or
            // retain the same one on detach failure.
            unsafe { sys::ams_mel_ir_scheduling_close(out, d, c, r) }
        })
    }

    fn live(&self) -> Result<*mut sys::AmsMelIrScheduling, Error> {
        if self.is_open() {
            Ok(self.raw)
        } else {
            Err(Error::new(
                ErrorKind::ProviderFailed,
                "IR Scheduling channel is closed",
            ))
        }
    }
}

impl Drop for SchedulingChannel {
    fn drop(&mut self) {
        best_effort_close(mem::replace(&mut self.raw, ptr::null_mut()));
    }
}

fn submit_abort_with(
    info: &SchedulingTaskInfo,
    submit: impl FnOnce(
        &sys::AmsMelIrTaskInfoV1,
        &mut *mut sys::AmsMelIrReturnRequest,
        *mut c_char,
        usize,
        *mut usize,
    ) -> i32,
) -> Result<ReturnRequest, Error> {
    let info = sys::AmsMelIrTaskInfoV1 {
        system_time_ns: info.system_time_ns,
        request_id: info.request_id,
        action: info.action as u32,
        precedence_within_priority: info.precedence_within_priority,
        priority: info.priority,
    };
    // Guard even an unexpected handle on failure or malformed success. Drop
    // attempts cleanup once, without adopting the output as a usable request.
    let mut owner = ReturnRequest {
        raw: ptr::null_mut(),
        _not_send_sync: Rc::new(()),
    };
    call_with_diagnostic(|d, c, r| submit(&info, &mut owner.raw, d, c, r))?;
    if owner.raw.is_null() {
        return Err(protocol(
            "Scheduling AbortTask succeeded without a request owner",
        ));
    }
    Ok(owner)
}

fn best_effort_close(mut raw: *mut sys::AmsMelIrScheduling) {
    if !raw.is_null() {
        // SAFETY: unique owner and valid null diagnostics. Never reuse the
        // output slot, including when detach fails and leaves the graph intact.
        let _ = unsafe {
            sys::ams_mel_ir_scheduling_close(&mut raw, ptr::null_mut(), 0, ptr::null_mut())
        };
    }
}

fn close_with(
    raw: &mut *mut sys::AmsMelIrScheduling,
    close: impl FnOnce(*mut *mut sys::AmsMelIrScheduling, *mut c_char, usize, *mut usize) -> i32,
) -> Result<(), Error> {
    let original = mem::replace(raw, ptr::null_mut());
    if original.is_null() {
        return Ok(());
    }
    let mut output = original;
    let mut status = sys::AMS_MEL_INTERNAL_ERROR;
    let result = call_with_diagnostic(|d, c, r| {
        status = close(&mut output, d, c, r);
        status
    });
    // As for Stacked, only a well-formed ProviderFailed with the exact same
    // pointer authorizes another Close. A consumed or contradictory output
    // cannot reach Drop, even when the diagnostic itself is malformed.
    if status == sys::AMS_MEL_PROVIDER_FAILED
        && output == original
        && result.as_ref().is_err_and(|error| {
            error.kind() == &ErrorKind::ProviderFailed
                && error
                    .diagnostic_required()
                    .is_some_and(|required| (1..=DIAGNOSTIC_CAPACITY).contains(&required))
        })
    {
        *raw = original;
    }
    result?;
    if !output.is_null() {
        return Err(protocol(
            "Scheduling close succeeded without releasing the owner",
        ));
    }
    Ok(())
}

fn open_with(
    config: &SchedulingConfig,
    open: impl FnOnce(
        &sys::AmsMelIrSchedulingConfigV1,
        *mut *mut sys::AmsMelIrScheduling,
        *mut c_char,
        usize,
        *mut usize,
    ) -> i32,
    close: impl FnOnce(*mut sys::AmsMelIrScheduling),
) -> Result<SchedulingChannel, Error> {
    let config = sys::AmsMelIrSchedulingConfigV1 {
        channel_id: raw_uci_id(&config.channel_id),
        platform_id: raw_uci_id(&config.platform_id),
        channel_type: sys::AMS_MEL_IR_CHANNEL_SCHEDULING,
        sensor_location: sys::AmsMelComponentLocationV1 {
            offset_x_m: config.sensor_location.offset_x_m(),
            offset_y_m: config.sensor_location.offset_y_m(),
            offset_z_m: config.sensor_location.offset_z_m(),
            key: string_view(config.sensor_location.key()),
            system_name: string_view(config.sensor_location.system_name()),
        },
    };
    let mut raw = ptr::null_mut();
    if let Err(error) = call_with_diagnostic(|d, c, r| open(&config, &mut raw, d, c, r)) {
        if !raw.is_null() {
            close(raw);
        }
        return Err(error);
    }
    if raw.is_null() {
        return Err(protocol(
            "Scheduling open succeeded without a channel owner",
        ));
    }
    Ok(SchedulingChannel {
        raw,
        _not_send_sync: PhantomData,
    })
}

#[cfg(test)]
mod tests;
