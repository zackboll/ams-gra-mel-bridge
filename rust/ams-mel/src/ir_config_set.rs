//! Complete value-only ConfigSetCommand input; native code owns asynchronous work.

use crate::{
    call_with_diagnostic, protocol, string_view, sys, ControlChannel, Error, ErrorKind,
    ReturnRequest,
};
use std::{ptr, rc::Rc};

/// Owned configuration command. Configuration is opaque UTF-8, not a schema.
/// Embedded NUL bytes are rejected at submission; empty text is valid.
///
/// ```no_run
/// use ams_mel::{ConfigSetCommand, ControlChannel, Error, ReturnResult};
///
/// fn configure(channel: &mut ControlChannel) -> Result<ReturnResult, Error> {
///     let mut command = ConfigSetCommand::new(301);
///     command.system_time_ns = -123;
///     command.config = "opaque configuration-€".into();
///     let mut request = channel.submit_config_set(&command)?;
///     let result = request.wait(1_000)?;
///     request.close()?;
///     Ok(result) // completion is not evidence of physically applied configuration
/// }
/// ```
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ConfigSetCommand {
    /// All 32 command-ID bits are passed unchanged.
    pub command_id: u32,
    /// Signed nanoseconds, passed unchanged without an inferred clock or epoch.
    pub system_time_ns: i64,
    /// Original UTF-8 bytes, without parsing, normalization or format restrictions.
    pub config: String,
}

impl ConfigSetCommand {
    /// Uses the published defaults: zero system time and empty configuration.
    pub fn new(command_id: u32) -> Self {
        Self {
            command_id,
            system_time_ns: 0,
            config: String::new(),
        }
    }

    fn to_raw(&self) -> Result<sys::AmsMelIrConfigSetCommandV1, Error> {
        if self.config.as_bytes().contains(&0) {
            return Err(Error::new(
                ErrorKind::InvalidArgument,
                "configuration contains an embedded NUL byte",
            ));
        }
        Ok(sys::AmsMelIrConfigSetCommandV1 {
            command_id: self.command_id,
            system_time_ns: self.system_time_ns,
            config: string_view(&self.config),
        })
    }
}

impl ControlChannel {
    /// Submits one complete command on an enabled C2 channel. Native code copies
    /// every input synchronously; no borrowed text escapes this call. Empty text
    /// is valid; embedded NUL returns [`ErrorKind::InvalidArgument`] before send.
    ///
    /// The existing [`ReturnRequest`] owns cached completion independently of
    /// this channel and its Session. Timeout and Close/Drop do not cancel work.
    /// [`crate::CommandReturn::Fail`] is a completed payload, not an exception;
    /// [`crate::ReturnResult::Rejected`] retains the MEL error and description.
    /// Neither submission nor Success proves physically applied configuration.
    /// CommandStatus metadata is independent and is not implicitly registered.
    pub fn submit_config_set(
        &mut self,
        command: &ConfigSetCommand,
    ) -> Result<ReturnRequest, Error> {
        submit_with(command, |command, out| {
            call_with_diagnostic(|diagnostic, capacity, required| {
                // SAFETY: this channel is uniquely borrowed; the original
                // command String and all output storage live through native's
                // synchronous copy. Empty views use the ABI's null/zero pair.
                unsafe {
                    sys::ams_mel_ir_c2_submit_config_set(
                        self.raw, command, out, diagnostic, capacity, required,
                    )
                }
            })
        })
    }
}

fn submit_with(
    command: &ConfigSetCommand,
    submit: impl FnOnce(
        &sys::AmsMelIrConfigSetCommandV1,
        &mut *mut sys::AmsMelIrReturnRequest,
    ) -> Result<(), Error>,
) -> Result<ReturnRequest, Error> {
    let raw = command.to_raw()?;
    // Establish the existing RAII owner before native submission. Unexpected
    // non-null error output is dropped exactly once; uncertain release is not
    // retried. No new request lifecycle or completion consumer is introduced.
    let mut owner = ReturnRequest {
        raw: ptr::null_mut(),
        _not_send_sync: Rc::new(()),
    };
    submit(&raw, &mut owner.raw)?;
    if owner.raw.is_null() {
        return Err(protocol(
            "ConfigSet submit succeeded without a request owner",
        ));
    }
    Ok(owner)
}

#[cfg(test)]
mod tests;
