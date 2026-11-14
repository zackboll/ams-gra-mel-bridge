//! One-choice BIT commands; native code copies inputs and owns asynchronous work.

use crate::{
    call_with_diagnostic, protocol, reserve_owned, string_view, sys, ControlChannel, Error,
    ErrorKind, ReturnRequest,
};
use std::{ptr, rc::Rc};

/// Exactly one BIT operation. Non-NoOp collections must be nonempty at submission.
/// Ordering and duplicates are preserved; no catalog or device restrictions are added.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum BitAction {
    /// All three native collections are empty; does not initiate an unspecified BIT.
    NoOp,
    /// Numeric BIT IDs to initiate, not UCI UUID identities. All 32 bits are retained.
    Initiate(Vec<u32>),
    /// Numeric BIT IDs to cancel, not UCI UUID identities. All 32 bits are retained.
    Cancel(Vec<u32>),
    /// Opaque UTF-8 fault codes. Empty strings are valid; embedded NUL is rejected.
    /// Bytes are not normalized, parsed, trimmed, sorted or deduplicated.
    ClearFaultCodes(Vec<String>),
}

/// Owned one-choice BIT command. Mixed native choices (whose upstream precedence
/// is initiate, cancel, then clear faults) cannot be expressed through this type.
///
/// ```no_run
/// use ams_mel::{BitAction, BitCommand, ControlChannel, Error};
///
/// fn command_bit(channel: &mut ControlChannel) -> Result<(), Error> {
///     for action in [
///         BitAction::NoOp,
///         BitAction::Initiate(vec![0, u32::MAX]),
///         BitAction::Cancel(vec![7, 7]),
///         BitAction::ClearFaultCodes(vec!["fault-€".into(), "".into()]),
///     ] {
///         let command = BitCommand { command_id: 301, action };
///         let mut request = channel.submit_bit(&command)?;
///         let result = request.wait(1_000)?;
///         request.close()?;
///         // Inspect result: acceptance/completion does not prove physical effects.
///         println!("{result:?}");
///     }
///     Ok(())
/// }
/// ```
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct BitCommand {
    /// Command correlation ID, passed unchanged with all 32 bits preserved.
    pub command_id: u32,
    /// The single operation and its owned payload.
    pub action: BitAction,
}

impl ControlChannel {
    /// Submits exactly one BIT command on an enabled C2 channel, using Session-wide
    /// asynchronous admission and the existing [`ReturnRequest`] completion owner.
    /// Native code copies all inputs synchronously; the command may be dropped or
    /// changed immediately afterward. No Rust worker, callback or queue is created.
    ///
    /// Empty action collections and embedded NUL yield [`ErrorKind::InvalidArgument`]
    /// before native submission. A nonempty fault collection may contain empty strings.
    /// Timeout (including `wait(0)` polling) and request Close/Drop do not cancel work.
    /// Requests may outlive both channel and Session; physical teardown is deferred.
    /// [`crate::CommandReturn::Fail`] is an ordinary completed payload, whereas
    /// [`crate::ReturnResult::Rejected`] retains an explicit MEL code and description.
    /// CommandStatus/BIT metadata is independent and not implicitly registered.
    /// Success is not proof of physical BIT execution, cancellation or fault clearance.
    pub fn submit_bit(&mut self, command: &BitCommand) -> Result<ReturnRequest, Error> {
        submit_with(command, |command, out| {
            call_with_diagnostic(|diagnostic, capacity, required| {
                // SAFETY: channel is uniquely borrowed. The command's numeric
                // vectors/Strings, local string-view vector and initialized output
                // storage all remain live during native's synchronous copy.
                unsafe {
                    sys::ams_mel_ir_c2_submit_bit(
                        self.raw, command, out, diagnostic, capacity, required,
                    )
                }
            })
        })
    }
}

fn submit_with(
    command: &BitCommand,
    submit: impl FnOnce(
        &sys::AmsMelIrBitCommandV1,
        &mut *mut sys::AmsMelIrReturnRequest,
    ) -> Result<(), Error>,
) -> Result<ReturnRequest, Error> {
    let mut raw = sys::AmsMelIrBitCommandV1 {
        command_id: command.command_id,
        initiate_bit_ids: sys::AmsMelU32SpanV1 {
            data: ptr::null(),
            size: 0,
        },
        cancel_bit_ids: sys::AmsMelU32SpanV1 {
            data: ptr::null(),
            size: 0,
        },
        clear_fault_codes: sys::AmsMelStringViewSpanV1 {
            data: ptr::null(),
            size: 0,
        },
    };
    let mut views = Vec::new();
    match &command.action {
        BitAction::NoOp => {}
        BitAction::Initiate(ids) | BitAction::Cancel(ids) => {
            require_nonempty(ids.len())?;
            let span = sys::AmsMelU32SpanV1 {
                data: ids.as_ptr(),
                size: ids.len(),
            };
            if matches!(&command.action, BitAction::Initiate(_)) {
                raw.initiate_bit_ids = span;
            } else {
                raw.cancel_bit_ids = span;
            }
        }
        BitAction::ClearFaultCodes(codes) => {
            require_nonempty(codes.len())?;
            if codes.iter().any(|code| code.as_bytes().contains(&0)) {
                return Err(Error::new(
                    ErrorKind::InvalidArgument,
                    "fault code contains an embedded NUL byte",
                ));
            }
            // Rust Strings already guarantee UTF-8. Reserve fallibly before push;
            // the helper also checks allocation-size overflow. Byte lengths come
            // directly from String, without multiplication or character counting.
            views = reserve_owned(codes.len(), "BIT fault-code views")?;
            for code in codes {
                views.push(string_view(code));
            }
            raw.clear_fault_codes = sys::AmsMelStringViewSpanV1 {
                data: views.as_ptr(),
                size: views.len(),
            };
        }
    }
    // Install the existing RAII owner before submit, including for unexpected
    // error output. Non-panicking Drop releases once, without retrying uncertain
    // closure. No borrowed input pointer is stored in the returned owner.
    let mut owner = ReturnRequest {
        raw: ptr::null_mut(),
        _not_send_sync: Rc::new(()),
    };
    submit(&raw, &mut owner.raw)?;
    // Keep the descriptor allocation live through the single call above.
    drop(views);
    if owner.raw.is_null() {
        return Err(protocol("BIT submit succeeded without a request owner"));
    }
    Ok(owner)
}

fn require_nonempty(size: usize) -> Result<(), Error> {
    if size == 0 {
        Err(Error::new(
            ErrorKind::InvalidArgument,
            "BIT action collection is empty; use NoOp",
        ))
    } else {
        Ok(())
    }
}

#[cfg(test)]
mod tests;
