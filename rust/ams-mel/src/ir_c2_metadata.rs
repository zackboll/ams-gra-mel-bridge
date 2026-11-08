//! Owned reception of the four existing IR C2 metadata families.
//! Native code owns registration, retention and the bounded DROP-INCOMING FIFO.
//! Rust adds no callback, thread, lock or queue. The facade is the trusted
//! memory-provenance boundary for checked event views and nested spans.

use crate::{
    call_with_diagnostic, checked_span, copy_mapped, owned_text, owned_uci_id, protocol, BitStatus,
    CommsTestReport, ControlChannel, Error, ErrorKind, UciId,
};
use ams_mel_sys as sys;
use std::{ffi::c_char, ptr, rc::Rc};

macro_rules! native_enum {
    ($(#[$meta:meta])* $name:ident { $($variant:ident = $value:expr),+ $(,)? }) => {
        $(#[$meta])*
        #[derive(Clone, Copy, Debug, Eq, PartialEq, Hash)]
        #[repr(u32)]
        pub enum $name { $($variant = $value),+ }
        impl $name {
            /// Every published value in ascending native order.
            pub const ALL: &'static [Self] = &[$(Self::$variant),+];
            /// Exact native numeric association.
            pub const fn as_raw(self) -> u32 { self as u32 }
            fn decode(raw: u32) -> Result<Self, Error> {
                match raw {
                    $($value => Ok(Self::$variant),)+
                    _ => Err(protocol(concat!("unpublished ", stringify!($name)))),
                }
            }
        }
        impl From<$name> for u32 {
            fn from(value: $name) -> Self { value.as_raw() }
        }
    };
}

native_enum! {
    /// Reported messaging state; Accepted is not physical completion.
    CommandState {
        NotSet = 0, Received = 1, Accepted = 2, Rejected = 3, Cancelled = 4,
    }
}

native_enum! {
    /// Complete published CannotComply domain. Unknown raw values are errors,
    /// not the published Other or Unknown categories.
    CannotComply {
        NotSet = 0,
        ConstraintAttempts = 1,
        ConstraintEndurance = 2,
        ConstraintClassification = 3,
        ConstraintForFovLimit = 4,
        ConstraintGating = 5,
        ConstraintManeuverLimit = 6,
        ConstraintOp = 7,
        ConstraintOcclusion = 8,
        CapabilityRange = 9,
        CapabilityPerformance = 10,
        ConstraintRf = 11,
        ConstraintRoute = 12,
        ConstraintSafety = 13,
        ConstraintTargetAngle = 14,
        ConstraintTime = 15,
        ConstraintSystem = 16,
        InfeasibleRoute = 17,
        MissionEvent = 18,
        StateOrSettings = 19,
        StateOrSettingsChange = 20,
        SystemUnavailable = 21,
        SystemFault = 22,
        SystemConflict = 23,
        SubsystemUnavailable = 24,
        SubsystemFault = 25,
        CapabilityFault = 26,
        CapabilityPrecedence = 27,
        CapabilityUnavailable = 28,
        InsufficientResources = 29,
        Ranking = 30,
        Weather = 31,
        IneligibleControlSource = 32,
        DependencyPredecessor = 33,
        DependencyAllOrNothing = 34,
        DependencyEitherOr = 35,
        InitCriteriaNotMet = 36,
        UnknownId = 37,
        InvalidInputParameter = 38,
        InputOther = 39,
        MdfActivationError = 40,
        Multiple = 41,
        Cancelled = 42,
        Other = 43,
        Unknown = 44,
        Aborted = 45,
        AlignmentManeuver = 46,
    }
}

native_enum! {
    /// Published BIT control interface. Values 2 and 3 retain upstream names
    /// and numeric associations despite inconsistent adjacent descriptions.
    BitControlInterface {
        NotSet = 0, SubsystemBitCommand = 1,
        SubsystemStateCommand = 2, SubsystemInitiated = 3,
    }
}

/// Independent command metadata, not request completion or inferred correlation.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CommandStatus {
    pub command_id: u32,
    pub state: CommandState,
    pub reason_id: CannotComply,
    pub reason_description: String,
}

/// Complete ordered BIT definitions, including duplicates and empty collections.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct BitConfiguration {
    pub bit_types: Vec<BitTypeDefinition>,
}

/// UCI BIT identity is not a numeric BIT command selector.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct BitTypeDefinition {
    pub bit_id: UciId,
    pub accepted_interface: BitControlInterface,
    pub bit_item_names: Vec<String>,
    pub subsystem_component_ids: Vec<UciId>,
    /// Signed nanoseconds, without normalization or physical validation.
    pub expected_duration_ns: i64,
}

/// Fully owned active event payload. Communications metadata is distinct from
/// a CommsRequest::wait result; BIT status uses the canonical Health types.
#[derive(Clone, Debug, PartialEq)]
#[repr(u32)]
pub enum C2MetadataEvent {
    CommandStatus(CommandStatus) = 1,
    BitConfiguration(BitConfiguration) = 2,
    BitStatus(BitStatus) = 3,
    CommsTest(CommsTestReport) = 4,
}

impl ControlChannel {
    /// One-shot registration of BIT_Configuration, CommandStatus, BIT_Status
    /// in that order while Attached or Enabled. Does not enable C2. Zero
    /// capacity fails locally without consuming registration. Partial native
    /// failure publishes no Rust owner; native callback retention still applies.
    pub fn open_metadata(&mut self, queue_capacity: usize) -> Result<C2MetadataStream, Error> {
        if self.raw.is_null() {
            return Err(Error::new(
                ErrorKind::ProviderFailed,
                "C2 channel is closed",
            ));
        }
        let raw = self.raw;
        C2MetadataStream::open_with(queue_capacity, |out, d, c, r| {
            // SAFETY: live exclusive C2 owner and guarded initially null output.
            unsafe { sys::ams_mel_ir_c2_metadata_open(raw, queue_capacity, out, d, c, r) }
        })
    }
}

/// Saturating native callback activity snapshots, not Rust Receive-call counts.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct C2MetadataCounters {
    pub events_received: u64,
    pub events_dropped_queue_full: u64,
    /// Malformed/unsupported callbacks. Callback-copy allocation exceptions
    /// instead fail the native stream, after its queued events drain.
    pub malformed_or_unsupported: u64,
}

type Close<T> = unsafe extern "C" fn(*mut *mut T, *mut c_char, usize, *mut usize) -> i32;

/// Unique native metadata consumption owner, independent of C2/Session Rust
/// lifetimes. Close does not close C2, unregister callbacks or cancel requests.
/// C2 Close prevents enqueueing; queued events drain before stopped/failed status.
/// Native channel destruction is the callback-quiescence/provider-unload boundary.
/// Drop makes a non-panicking best-effort Close.
///
/// ```compile_fail
/// fn require_send<T: Send>() {}
/// require_send::<ams_mel::C2MetadataStream>();
/// ```
/// ```compile_fail
/// fn require_sync<T: Sync>() {}
/// require_sync::<ams_mel::C2MetadataStream>();
/// ```
#[derive(Debug)]
pub struct C2MetadataStream {
    raw: *mut sys::AmsMelIrC2Metadata,
    close_op: Close<sys::AmsMelIrC2Metadata>,
    _not_send_sync: Rc<()>,
}

impl C2MetadataStream {
    pub(crate) fn open_with(
        queue_capacity: usize,
        open: impl FnOnce(*mut *mut sys::AmsMelIrC2Metadata, *mut c_char, usize, *mut usize) -> i32,
    ) -> Result<Self, Error> {
        if queue_capacity == 0 {
            return Err(Error::new(
                ErrorKind::InvalidArgument,
                "C2 metadata capacity must be nonzero",
            ));
        }
        let mut owner = Self {
            raw: ptr::null_mut(),
            close_op: sys::ams_mel_ir_c2_metadata_close,
            _not_send_sync: Rc::new(()),
        };
        // Guard an unexpected output on failure as well. Registration is never
        // retried, and this API has no separate provider Return output.
        let result = call_with_diagnostic(|d, c, r| open(&mut owner.raw, d, c, r));
        owner.finish_open(result)
    }

    fn finish_open(self, result: Result<(), Error>) -> Result<Self, Error> {
        result?;
        if !self.is_open() {
            return Err(protocol("C2 metadata open succeeded without an owner"));
        }
        Ok(self)
    }

    /// Local ownership, not callback/provider liveness.
    pub fn is_open(&self) -> bool {
        !self.raw.is_null()
    }

    /// Separately attempts optional ChannelCommsTestRep registration once.
    /// Failure consumes an actual registration attempt too. Does not send a
    /// test or re-register the three base callbacks; no automatic retry.
    pub fn enable_comms_test_events(&mut self) -> Result<(), Error> {
        let raw = self.live()?;
        call_with_diagnostic(|d, c, r| {
            // SAFETY: live exclusively accessed metadata owner; native one-shot.
            unsafe { sys::ams_mel_ir_c2_metadata_register_comms_test(raw, d, c, r) }
        })
    }

    /// One native Receive; zero polls. Deeply copies the selected payload and
    /// closes its event before returning. Errors and unwind also release it.
    pub fn receive(&mut self, timeout_ms: u32) -> Result<C2MetadataEvent, Error> {
        let raw = self.live()?;
        self.receive_event_with(&EVENT_OPS, |out, d, c, r| {
            // SAFETY: live exclusive owner; event output is guarded.
            unsafe { sys::ams_mel_ir_c2_metadata_receive(raw, timeout_ms, out, d, c, r) }
        })
    }

    fn receive_event_with(
        &mut self,
        ops: &'static EventOps,
        receive: impl FnOnce(
            *mut *mut sys::AmsMelIrC2MetadataEvent,
            *mut c_char,
            usize,
            *mut usize,
        ) -> i32,
    ) -> Result<C2MetadataEvent, Error> {
        let mut event = NativeEvent {
            raw: ptr::null_mut(),
            ops,
        };
        call_with_diagnostic(|d, c, r| receive(&mut event.raw, d, c, r))?;
        event.copy_with(event_from_raw)
    }

    pub fn counters(&mut self) -> Result<C2MetadataCounters, Error> {
        let raw = self.live()?;
        self.counters_with(|out, d, c, r| {
            // SAFETY: live owner and exact writable counter storage.
            unsafe { sys::ams_mel_ir_c2_metadata_get_counters(raw, out, d, c, r) }
        })
    }

    fn counters_with(
        &mut self,
        get: impl FnOnce(*mut sys::AmsMelIrC2MetadataCountersV1, *mut c_char, usize, *mut usize) -> i32,
    ) -> Result<C2MetadataCounters, Error> {
        let mut counters = sys::AmsMelIrC2MetadataCountersV1::default();
        call_with_diagnostic(|d, c, r| get(&mut counters, d, c, r))?;
        Ok(C2MetadataCounters {
            events_received: counters.events_received,
            events_dropped_queue_full: counters.events_dropped_queue_full,
            malformed_or_unsupported: counters.malformed_or_unsupported,
        })
    }

    /// Idempotent consumption Close; callbacks can remain retained natively.
    /// Consumes the local release attempt even on failure or uncertain output:
    /// Drop must not retry a possibly released handle.
    pub fn close(&mut self) -> Result<(), Error> {
        close_owned(&mut self.raw, self.close_op)
    }

    fn live(&self) -> Result<*mut sys::AmsMelIrC2Metadata, Error> {
        if self.is_open() {
            Ok(self.raw)
        } else {
            Err(Error::new(
                ErrorKind::ProviderFailed,
                "IR C2 metadata stream is closed",
            ))
        }
    }
}

impl Drop for C2MetadataStream {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            // SAFETY: unique owner, supported null diagnostics; no allocation.
            let _ = unsafe { (self.close_op)(&mut self.raw, ptr::null_mut(), 0, ptr::null_mut()) };
        }
    }
}

fn close_owned<T>(raw: &mut *mut T, close: Close<T>) -> Result<(), Error> {
    if raw.is_null() {
        return Ok(());
    }
    let mut owner = std::mem::replace(raw, ptr::null_mut());
    let mut status = sys::AMS_MEL_INTERNAL_ERROR;
    let result = call_with_diagnostic(|d, c, r| {
        // SAFETY: unique detached handle; this is its only release attempt.
        status = unsafe { close(&mut owner, d, c, r) };
        status
    });
    if status == sys::AMS_MEL_OK && !owner.is_null() {
        return Err(protocol(
            "C2 metadata close succeeded without clearing the handle",
        ));
    }
    result
}

struct EventOps {
    view: unsafe extern "C" fn(
        *const sys::AmsMelIrC2MetadataEvent,
        *mut *const sys::AmsMelIrC2MetadataEventV1,
        *mut c_char,
        usize,
        *mut usize,
    ) -> i32,
    close: Close<sys::AmsMelIrC2MetadataEvent>,
}

static EVENT_OPS: EventOps = EventOps {
    view: sys::ams_mel_ir_c2_metadata_event_view,
    close: sys::ams_mel_ir_c2_metadata_event_close,
};

struct NativeEvent {
    raw: *mut sys::AmsMelIrC2MetadataEvent,
    ops: &'static EventOps,
}

impl NativeEvent {
    fn copy_with(
        mut self,
        copy: impl FnOnce(&sys::AmsMelIrC2MetadataEventV1) -> Result<C2MetadataEvent, Error>,
    ) -> Result<C2MetadataEvent, Error> {
        if self.raw.is_null() {
            return Err(protocol("C2 metadata receive succeeded without an event"));
        }
        let mut view = ptr::null();
        call_with_diagnostic(|d, c, r| {
            // SAFETY: live event guard and writable output pointer.
            unsafe { (self.ops.view)(self.raw, &mut view, d, c, r) }
        })?;
        // SAFETY: facade owns immutable storage through conversion. Validate
        // outer pointer before dereference, then only the active payload span.
        let record = unsafe { checked_span(view, 1, "C2 metadata event record")? };
        let value = copy(&record[0])?;
        close_owned(&mut self.raw, self.ops.close)?;
        Ok(value)
    }
}

impl Drop for NativeEvent {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            // SAFETY: unique event; no borrowed values survive cleanup.
            let _ = unsafe { (self.ops.close)(&mut self.raw, ptr::null_mut(), 0, ptr::null_mut()) };
        }
    }
}

// Only active payloads are read. All storage belongs to a live event (or
// backed unit fixture) and must remain valid through the complete conversion.
fn event_from_raw(raw: &sys::AmsMelIrC2MetadataEventV1) -> Result<C2MetadataEvent, Error> {
    match raw.kind {
        sys::AMS_MEL_IR_C2_METADATA_COMMAND_STATUS => {
            let value = &raw.command_status;
            Ok(C2MetadataEvent::CommandStatus(CommandStatus {
                command_id: value.command_id,
                state: CommandState::decode(value.state)?,
                reason_id: CannotComply::decode(value.reason_id)?,
                reason_description: owned_text(
                    value.reason_description,
                    "CommandStatus.reason_description",
                )?,
            }))
        }
        sys::AMS_MEL_IR_C2_METADATA_BIT_CONFIGURATION => {
            let span = &raw.bit_configuration.bit_types;
            // SAFETY: immutable event backing; span validated before slicing.
            let types =
                unsafe { checked_span(span.data, span.size, "BIT_Configuration.bit_types")? };
            Ok(C2MetadataEvent::BitConfiguration(BitConfiguration {
                bit_types: copy_mapped(types, "BIT_Configuration.bit_types", |value| {
                    // SAFETY: nested event backing remains alive; validate each span.
                    let names = unsafe {
                        checked_span(
                            value.bit_item_names.data,
                            value.bit_item_names.size,
                            "BIT_Type.bit_item_names",
                        )?
                    };
                    // SAFETY: same lifetime and provenance as above.
                    let ids = unsafe {
                        checked_span(
                            value.subsystem_component_ids.data,
                            value.subsystem_component_ids.size,
                            "BIT_Type.subsystem_component_ids",
                        )?
                    };
                    Ok(BitTypeDefinition {
                        bit_id: owned_uci_id(&value.bit_id, "BIT_Type.bit_id")?,
                        accepted_interface: BitControlInterface::decode(value.accepted_interface)?,
                        bit_item_names: copy_mapped(names, "BIT_Type.bit_item_names", |name| {
                            owned_text(*name, "BIT item name")
                        })?,
                        subsystem_component_ids: copy_mapped(
                            ids,
                            "BIT_Type.subsystem_component_ids",
                            |id| owned_uci_id(id, "BIT component ID"),
                        )?,
                        expected_duration_ns: value.expected_duration_ns,
                    })
                })?,
            }))
        }
        sys::AMS_MEL_IR_C2_METADATA_BIT_STATUS => {
            crate::ir_health_metadata::bit_status(&raw.bit_status).map(C2MetadataEvent::BitStatus)
        }
        sys::AMS_MEL_IR_C2_METADATA_CHANNEL_COMMS_TEST => {
            Ok(C2MetadataEvent::CommsTest(CommsTestReport {
                command_id: raw.channel_comms_test.command_id,
                request_id: raw.channel_comms_test.request_id,
            }))
        }
        _ => Err(protocol("unpublished C2 metadata event kind")),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::{
        alloc::{GlobalAlloc, Layout, System},
        cell::Cell,
        panic::{catch_unwind, AssertUnwindSafe},
    };

    thread_local! {
        // -1 disables injection. Failure disarms before error construction.
        static ALLOCATIONS_UNTIL_FAILURE: Cell<isize> = const { Cell::new(-1) };
    }
    struct TestAllocator;
    #[global_allocator]
    static TEST_ALLOCATOR: TestAllocator = TestAllocator;
    fn fail_allocation() -> bool {
        ALLOCATIONS_UNTIL_FAILURE.with(|remaining| {
            let count = remaining.get();
            if count < 0 {
                return false;
            }
            remaining.set(count - 1);
            count == 0
        })
    }
    // SAFETY: forwards every allocation unchanged to System except an explicit
    // thread-local null failure, which is permitted by GlobalAlloc's contract.
    unsafe impl GlobalAlloc for TestAllocator {
        unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
            if fail_allocation() {
                return ptr::null_mut();
            }
            // SAFETY: exact caller-provided layout.
            unsafe { System.alloc(layout) }
        }
        unsafe fn dealloc(&self, pointer: *mut u8, layout: Layout) {
            // SAFETY: all successful allocations originate from System.
            unsafe { System.dealloc(pointer, layout) }
        }
        unsafe fn realloc(&self, pointer: *mut u8, layout: Layout, size: usize) -> *mut u8 {
            if fail_allocation() {
                return ptr::null_mut();
            }
            // SAFETY: exact System allocation and caller-provided layout/size.
            unsafe { System.realloc(pointer, layout, size) }
        }
    }

    #[test]
    fn nested_allocation_failures_return_no_partial_event_and_release_once() {
        let names = [text("item")];
        let ids = [id(1, "component")];
        let definition = sys::AmsMelBitTypeV1 {
            bit_id: id(2, "BIT"),
            accepted_interface: 1,
            bit_item_names: sys::AmsMelStringViewSpanV1 {
                data: names.as_ptr(),
                size: 1,
            },
            subsystem_component_ids: sys::AmsMelUciIdSpanV1 {
                data: ids.as_ptr(),
                size: 1,
            },
            expected_duration_ns: -1,
        };
        // Six distinct fallible reservations: definitions, BIT label, names,
        // item text, component IDs, component label. Diagnostic allocations are
        // deliberately outside the injected conversion window.
        for allocation in 0..6 {
            let (event, closes) = fake_event(2, 0);
            // SAFETY: exclusively owned fixture; all nested backing stays live.
            unsafe {
                (*event.raw.cast::<FakeEvent>())
                    .view
                    .bit_configuration
                    .bit_types = sys::AmsMelBitTypeSpanV1 {
                    data: &definition,
                    size: 1,
                };
            }
            let result = event.copy_with(|raw| {
                ALLOCATIONS_UNTIL_FAILURE.with(|count| count.set(allocation));
                let result = event_from_raw(raw);
                ALLOCATIONS_UNTIL_FAILURE.with(|count| count.set(-1));
                result
            });
            assert_eq!(result.unwrap_err().kind(), &ErrorKind::InternalError);
            assert_eq!(closes.get(), 1);
        }
    }

    fn record(kind: u32) -> sys::AmsMelIrC2MetadataEventV1 {
        // SAFETY: raw records contain only scalars and nullable raw spans.
        let mut raw: sys::AmsMelIrC2MetadataEventV1 = unsafe { std::mem::zeroed() };
        raw.kind = kind;
        raw
    }
    fn text(value: &str) -> sys::AmsMelStringViewV1 {
        sys::AmsMelStringViewV1 {
            data: value.as_ptr().cast(),
            size: value.len(),
        }
    }
    fn id(seed: u8, label: &str) -> sys::AmsMelUciIdV1 {
        sys::AmsMelUciIdV1 {
            uuid: [seed; 16],
            descriptive_label: text(label),
        }
    }
    fn assert_protocol<T: std::fmt::Debug>(value: Result<T, Error>) {
        assert_eq!(value.unwrap_err().kind(), &ErrorKind::ProtocolInconsistency);
    }

    #[test]
    fn active_dispatch_ignores_poisoned_inactive_payloads() {
        for kind in 1..=4 {
            let mut raw = record(kind);
            if kind != 1 {
                raw.command_status.state = u32::MAX;
            }
            if kind != 2 {
                raw.bit_configuration.bit_types.size = usize::MAX;
            }
            if kind != 3 {
                raw.bit_status.faults.size = usize::MAX;
            }
            let value = event_from_raw(&raw).unwrap();
            assert!(matches!(
                (kind, value),
                (1, C2MetadataEvent::CommandStatus(_))
                    | (2, C2MetadataEvent::BitConfiguration(_))
                    | (3, C2MetadataEvent::BitStatus(_))
                    | (4, C2MetadataEvent::CommsTest(_))
            ));
        }
        for kind in [0, 5, u32::MAX] {
            assert_protocol(event_from_raw(&record(kind)));
        }
    }

    #[test]
    fn command_domains_exact_mapping_and_owned_text() {
        let states = [
            CommandState::NotSet,
            CommandState::Received,
            CommandState::Accepted,
            CommandState::Rejected,
            CommandState::Cancelled,
        ];
        let reasons = [
            CannotComply::NotSet,
            CannotComply::ConstraintAttempts,
            CannotComply::ConstraintEndurance,
            CannotComply::ConstraintClassification,
            CannotComply::ConstraintForFovLimit,
            CannotComply::ConstraintGating,
            CannotComply::ConstraintManeuverLimit,
            CannotComply::ConstraintOp,
            CannotComply::ConstraintOcclusion,
            CannotComply::CapabilityRange,
            CannotComply::CapabilityPerformance,
            CannotComply::ConstraintRf,
            CannotComply::ConstraintRoute,
            CannotComply::ConstraintSafety,
            CannotComply::ConstraintTargetAngle,
            CannotComply::ConstraintTime,
            CannotComply::ConstraintSystem,
            CannotComply::InfeasibleRoute,
            CannotComply::MissionEvent,
            CannotComply::StateOrSettings,
            CannotComply::StateOrSettingsChange,
            CannotComply::SystemUnavailable,
            CannotComply::SystemFault,
            CannotComply::SystemConflict,
            CannotComply::SubsystemUnavailable,
            CannotComply::SubsystemFault,
            CannotComply::CapabilityFault,
            CannotComply::CapabilityPrecedence,
            CannotComply::CapabilityUnavailable,
            CannotComply::InsufficientResources,
            CannotComply::Ranking,
            CannotComply::Weather,
            CannotComply::IneligibleControlSource,
            CannotComply::DependencyPredecessor,
            CannotComply::DependencyAllOrNothing,
            CannotComply::DependencyEitherOr,
            CannotComply::InitCriteriaNotMet,
            CannotComply::UnknownId,
            CannotComply::InvalidInputParameter,
            CannotComply::InputOther,
            CannotComply::MdfActivationError,
            CannotComply::Multiple,
            CannotComply::Cancelled,
            CannotComply::Other,
            CannotComply::Unknown,
            CannotComply::Aborted,
            CannotComply::AlignmentManeuver,
        ];
        for (state, expected_state) in states.iter().enumerate() {
            for (reason, expected_reason) in reasons.iter().enumerate() {
                let mut raw = record(1);
                let description = String::from("原因-α");
                raw.command_status = sys::AmsMelIrCommandStatusV1 {
                    command_id: u32::MAX,
                    state: state as u32,
                    reason_id: reason as u32,
                    reason_description: text(&description),
                };
                let value = event_from_raw(&raw).unwrap();
                drop(description);
                assert_eq!(
                    value,
                    C2MetadataEvent::CommandStatus(CommandStatus {
                        command_id: u32::MAX,
                        state: *expected_state,
                        reason_id: *expected_reason,
                        reason_description: "原因-α".into(),
                    })
                );
                assert_eq!(expected_reason.as_raw(), reason as u32);
                assert_eq!(expected_state.as_raw(), state as u32);
            }
        }
        assert_eq!(CommandState::ALL, states);
        assert_eq!(CannotComply::ALL, reasons);
        for invalid in [47, 99, u32::MAX] {
            let mut raw = record(1);
            raw.command_status.reason_id = invalid;
            assert_protocol(event_from_raw(&raw));
            raw.command_status.reason_id = 0;
            raw.command_status.state = invalid;
            assert_protocol(event_from_raw(&raw));
        }
        let mut raw = record(1);
        raw.command_status.state = 5;
        assert_protocol(event_from_raw(&raw));
    }

    #[test]
    fn nested_configuration_preserves_every_value_and_duplicate() {
        let names = [text(""), text("item-€"), text("item-€")];
        let ids = [id(0xff, "α"), id(0xff, "α"), id(0, "")];
        for (category, expected) in [
            BitControlInterface::NotSet,
            BitControlInterface::SubsystemBitCommand,
            BitControlInterface::SubsystemStateCommand,
            BitControlInterface::SubsystemInitiated,
        ]
        .into_iter()
        .enumerate()
        {
            for duration in [i64::MIN, -99, 0, i64::MAX] {
                let value = sys::AmsMelBitTypeV1 {
                    bit_id: id(0x80, "BIT-β"),
                    accepted_interface: category as u32,
                    bit_item_names: sys::AmsMelStringViewSpanV1 {
                        data: names.as_ptr(),
                        size: names.len(),
                    },
                    subsystem_component_ids: sys::AmsMelUciIdSpanV1 {
                        data: ids.as_ptr(),
                        size: ids.len(),
                    },
                    expected_duration_ns: duration,
                };
                let values = [value, value];
                let mut raw = record(2);
                raw.bit_configuration.bit_types = sys::AmsMelBitTypeSpanV1 {
                    data: values.as_ptr(),
                    size: 2,
                };
                let C2MetadataEvent::BitConfiguration(result) = event_from_raw(&raw).unwrap()
                else {
                    panic!()
                };
                let expected_type = BitTypeDefinition {
                    bit_id: UciId::new([0x80; 16], "BIT-β").unwrap(),
                    accepted_interface: expected,
                    bit_item_names: vec!["".into(), "item-€".into(), "item-€".into()],
                    subsystem_component_ids: vec![
                        UciId::new([255; 16], "α").unwrap(),
                        UciId::new([255; 16], "α").unwrap(),
                        UciId::new([0; 16], "").unwrap(),
                    ],
                    expected_duration_ns: duration,
                };
                assert_eq!(result.bit_types, vec![expected_type.clone(), expected_type]);
            }
        }
    }

    #[test]
    fn hostile_nested_spans_ids_text_and_interface_domains() {
        let mut raw = record(2);
        for (data, size) in [
            (ptr::null(), 1),
            (ptr::without_provenance(1), 1),
            (ptr::NonNull::dangling().as_ptr(), usize::MAX),
        ] {
            raw.bit_configuration.bit_types = sys::AmsMelBitTypeSpanV1 { data, size };
            assert_protocol(event_from_raw(&raw));
        }
        let mut value: sys::AmsMelBitTypeV1 = unsafe { std::mem::zeroed() };
        let convert = |value: &sys::AmsMelBitTypeV1| {
            let mut event = record(2);
            event.bit_configuration.bit_types = sys::AmsMelBitTypeSpanV1 {
                data: value,
                size: 1,
            };
            event_from_raw(&event)
        };
        for invalid in [4, 99, u32::MAX] {
            value.accepted_interface = invalid;
            assert_protocol(convert(&value));
        }
        value.accepted_interface = 0;
        value.bit_item_names.size = 1;
        assert_protocol(convert(&value));
        value.bit_item_names.size = 0;
        value.subsystem_component_ids.size = 1;
        assert_protocol(convert(&value));
        value.subsystem_component_ids.size = 0;
        value.bit_id.descriptive_label = text("bad\0label");
        assert_protocol(convert(&value));
        value.bit_id.descriptive_label = sys::AmsMelStringViewV1 {
            data: c"\xff".as_ptr(),
            size: 1,
        };
        assert_eq!(convert(&value).unwrap_err().kind(), &ErrorKind::InvalidUtf8);
        value.bit_id = id(0, "");
        let mut nested = [id(0, "")];
        value.subsystem_component_ids = sys::AmsMelUciIdSpanV1 {
            data: nested.as_ptr(),
            size: 1,
        };
        nested[0].descriptive_label.size = 1;
        nested[0].descriptive_label.data = ptr::null();
        value.subsystem_component_ids.data = nested.as_ptr();
        assert_protocol(convert(&value));
        let mut command = record(1);
        command.command_status.reason_description = sys::AmsMelStringViewV1 {
            data: ptr::null(),
            size: 1,
        };
        assert_protocol(event_from_raw(&command));
        command.command_status.reason_description = sys::AmsMelStringViewV1 {
            data: c"\xff".as_ptr(),
            size: 1,
        };
        assert_eq!(
            event_from_raw(&command).unwrap_err().kind(),
            &ErrorKind::InvalidUtf8
        );
    }

    #[test]
    fn bit_status_shared_converter_rejects_nested_invalid_graphs_and_keeps_ieee_bits() {
        let mut raw = record(3);
        let mut active = sys::AmsMelActiveBitV1 {
            bit_id: id(1, ""),
            estimated_completion_time_ns: i64::MIN,
            estimated_percent_complete: 0.0,
        };
        raw.bit_status.active_bits = sys::AmsMelActiveBitSpanV1 {
            data: &active,
            size: 1,
        };
        for value in [
            -1.25,
            1.25,
            -0.0,
            f64::INFINITY,
            f64::NEG_INFINITY,
            f64::NAN,
        ] {
            active.estimated_percent_complete = value;
            raw.bit_status.active_bits.data = &active;
            let C2MetadataEvent::BitStatus(result) = event_from_raw(&raw).unwrap() else {
                panic!()
            };
            assert_eq!(
                result.active_bits[0].estimated_percent_complete.to_bits(),
                value.to_bits()
            );
            assert_eq!(result.active_bits[0].estimated_completion_time_ns, i64::MIN);
        }
        let mut fault: sys::AmsMelFaultV1 = unsafe { std::mem::zeroed() };
        let convert = |fault: &sys::AmsMelFaultV1| {
            let mut event = record(3);
            event.bit_status.faults = sys::AmsMelFaultSpanV1 {
                data: fault,
                size: 1,
            };
            event_from_raw(&event)
        };
        fault.severity = 5;
        assert_protocol(convert(&fault));
        fault.severity = 0;
        fault.state = 4;
        assert_protocol(convert(&fault));
        fault.state = 0;
        let mut group: sys::AmsMelFaultAmbiguityGroupV1 = unsafe { std::mem::zeroed() };
        fault.ambiguity_groups = sys::AmsMelFaultAmbiguityGroupSpanV1 {
            data: &group,
            size: 1,
        };
        group.diagnostic_test_ids.size = 1;
        fault.ambiguity_groups.data = &group;
        assert_protocol(convert(&fault));
        group.diagnostic_test_ids.size = 0;
        group.component_ids.size = usize::MAX;
        fault.ambiguity_groups.data = &group;
        assert_protocol(convert(&fault));
        group.component_ids.size = 0;
        let invalid = id(1, "bad\0component");
        group.component_ids = sys::AmsMelUciIdSpanV1 {
            data: &invalid,
            size: 1,
        };
        fault.ambiguity_groups.data = &group;
        assert_protocol(convert(&fault));
    }

    #[test]
    fn completed_bit_item_domains_and_nested_spans_are_checked() {
        let mut bit: sys::AmsMelCompletedBitV1 = unsafe { std::mem::zeroed() };
        let convert = |bit: &sys::AmsMelCompletedBitV1| {
            let mut event = record(3);
            event.bit_status.completed_bits = sys::AmsMelCompletedBitSpanV1 { data: bit, size: 1 };
            event_from_raw(&event)
        };
        for result in [5, 99, u32::MAX] {
            bit.result = result;
            assert_protocol(convert(&bit));
        }
        bit.result = 1;
        bit.time_tag_ns = i64::MIN;
        let mut item: sys::AmsMelCompletedBitItemV1 = unsafe { std::mem::zeroed() };
        for result in 0..=4 {
            item.result = result;
            bit.bit_items = sys::AmsMelCompletedBitItemSpanV1 {
                data: &item,
                size: 1,
            };
            let C2MetadataEvent::BitStatus(value) = convert(&bit).unwrap() else {
                panic!()
            };
            assert_eq!(value.completed_bits[0].time_tag_ns, i64::MIN);
            assert_eq!(value.completed_bits[0].bit_items[0].result.as_raw(), result);
        }
        item.result = 5;
        bit.bit_items.data = &item;
        assert_protocol(convert(&bit));
        bit.bit_items.data = ptr::null();
        assert_protocol(convert(&bit));
        bit.bit_items.data = ptr::without_provenance(1);
        assert_protocol(convert(&bit));
        bit.bit_items.data = ptr::NonNull::dangling().as_ptr();
        bit.bit_items.size = usize::MAX;
        assert_protocol(convert(&bit));
    }
    unsafe fn diagnostic(d: *mut c_char, c: usize, r: *mut usize) {
        // SAFETY: fake operations receive the helper's writable diagnostic slots.
        unsafe {
            if c > 0 {
                *d = 0;
            }
            if !r.is_null() {
                *r = 1;
            }
        }
    }

    struct FakeEvent {
        view: sys::AmsMelIrC2MetadataEventV1,
        closes: Rc<Cell<usize>>,
        mode: u8,
    }

    unsafe extern "C" fn fake_view(
        raw: *const sys::AmsMelIrC2MetadataEvent,
        out: *mut *const sys::AmsMelIrC2MetadataEventV1,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32 {
        // SAFETY: handle points at the uniquely guarded fake Box.
        unsafe {
            diagnostic(d, c, r);
            let fake = &*raw.cast::<FakeEvent>();
            *out = match fake.mode {
                3 => ptr::null(),
                4 => ptr::without_provenance(1),
                _ => &fake.view,
            };
            if fake.mode == 5 {
                sys::AMS_MEL_PROVIDER_EXCEPTION
            } else {
                sys::AMS_MEL_OK
            }
        }
    }

    unsafe extern "C" fn fake_event_close(
        raw: *mut *mut sys::AmsMelIrC2MetadataEvent,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32 {
        // SAFETY: exact boxed fake, freed only once. Mode 2 reports failure
        // after release: the caller must never make a second release attempt.
        unsafe {
            diagnostic(d, c, r);
            let fake = &mut *(*raw).cast::<FakeEvent>();
            fake.closes.set(fake.closes.get() + 1);
            let mode = fake.mode;
            let stale = mode == 1 || mode == 6 || mode == 7;
            if fake.mode == 6 && !r.is_null() {
                *r = 0;
            }
            drop(Box::from_raw((*raw).cast::<FakeEvent>()));
            if !stale {
                *raw = ptr::null_mut();
            }
            if mode == 2 || mode == 7 {
                sys::AMS_MEL_INTERNAL_ERROR
            } else {
                sys::AMS_MEL_OK
            }
        }
    }

    static FAKE_OPS: EventOps = EventOps {
        view: fake_view,
        close: fake_event_close,
    };

    fn fake_event(kind: u32, mode: u8) -> (NativeEvent, Rc<Cell<usize>>) {
        let closes = Rc::new(Cell::new(0));
        let raw = Box::into_raw(Box::new(FakeEvent {
            view: record(kind),
            closes: closes.clone(),
            mode,
        }))
        .cast();
        (
            NativeEvent {
                raw,
                ops: &FAKE_OPS,
            },
            closes,
        )
    }

    #[test]
    fn event_view_conversion_close_errors_and_unwind_cleanup() {
        for (kind, mode, expected, count) in [
            (1, 0, None, 1),
            (99, 0, Some(ErrorKind::ProtocolInconsistency), 1),
            (1, 1, Some(ErrorKind::ProtocolInconsistency), 1),
            (1, 2, Some(ErrorKind::InternalError), 1),
            (1, 3, Some(ErrorKind::ProtocolInconsistency), 1),
            (1, 4, Some(ErrorKind::ProtocolInconsistency), 1),
            (1, 5, Some(ErrorKind::ProviderException), 1),
            (1, 6, Some(ErrorKind::ProtocolInconsistency), 1),
            (1, 7, Some(ErrorKind::InternalError), 1),
        ] {
            let (event, closes) = fake_event(kind, mode);
            let result = event.copy_with(event_from_raw);
            if let Some(expected) = expected {
                assert_eq!(result.unwrap_err().kind(), &expected);
            } else {
                assert!(result.is_ok());
            }
            assert_eq!(closes.get(), count);
        }
        let (event, closes) = fake_event(1, 0);
        assert!(catch_unwind(AssertUnwindSafe(
            || event.copy_with(|_| panic!("conversion unwind"))
        ))
        .is_err());
        assert_eq!(closes.get(), 1);
    }

    unsafe extern "C" fn fake_stream_close(
        raw: *mut *mut sys::AmsMelIrC2Metadata,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32 {
        // SAFETY: stream fixture owns exactly this boxed shared counter.
        unsafe {
            diagnostic(d, c, r);
            let closes = Box::from_raw((*raw).cast::<Rc<Cell<usize>>>());
            closes.set(closes.get() + 1);
            *raw = ptr::null_mut();
            sys::AMS_MEL_OK
        }
    }

    fn fake_stream() -> (C2MetadataStream, Rc<Cell<usize>>) {
        let closes = Rc::new(Cell::new(0));
        (
            C2MetadataStream {
                raw: Box::into_raw(Box::new(closes.clone())).cast(),
                close_op: fake_stream_close,
                _not_send_sync: Rc::new(()),
            },
            closes,
        )
    }

    #[test]
    fn stream_drop_calls_native_close_exactly_once() {
        let (stream, closes) = fake_stream();
        drop(stream);
        assert_eq!(closes.get(), 1);
    }

    unsafe extern "C" fn failed_stream_close(
        raw: *mut *mut sys::AmsMelIrC2Metadata,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32 {
        // SAFETY: same boxed fixture as fake_stream_close. Deliberately leave
        // a stale handle after release to exercise uncertain failure handling.
        unsafe {
            diagnostic(d, c, r);
            let closes = Box::from_raw((*raw).cast::<Rc<Cell<usize>>>());
            closes.set(closes.get() + 1);
        }
        sys::AMS_MEL_INTERNAL_ERROR
    }

    #[test]
    fn failed_stream_close_is_consumed_and_never_retried() {
        let (mut stream, closes) = fake_stream();
        stream.close_op = failed_stream_close;
        assert_eq!(
            stream.close().unwrap_err().kind(),
            &ErrorKind::InternalError
        );
        assert!(!stream.is_open());
        stream.close().unwrap();
        drop(stream);
        assert_eq!(closes.get(), 1);
    }

    #[test]
    fn open_failures_preserve_categories_and_close_unexpected_owner() {
        for kind in [
            ErrorKind::ProviderFailed,
            ErrorKind::ProviderException,
            ErrorKind::InternalError,
        ] {
            let (stream, closes) = fake_stream();
            assert_eq!(
                stream
                    .finish_open(Err(Error::new(kind.clone(), "registration failure")))
                    .unwrap_err()
                    .kind(),
                &kind
            );
            assert_eq!(closes.get(), 1);
        }
        let (stream, closes) = fake_stream();
        drop(stream.finish_open(Ok(())).unwrap());
        assert_eq!(closes.get(), 1);
    }

    #[test]
    fn counters_preserve_full_width_saturation_and_distinct_fields() {
        let (mut stream, _) = fake_stream();
        for (received, dropped, malformed) in
            [(u64::MAX, u64::MAX - 1, 0x8000000000000001), (9, 3, 2)]
        {
            let calls = Cell::new(0);
            let actual = stream
                .counters_with(|out, d, c, r| {
                    calls.set(calls.get() + 1);
                    // SAFETY: exact writable counter and diagnostic storage.
                    unsafe {
                        diagnostic(d, c, r);
                        *out = sys::AmsMelIrC2MetadataCountersV1 {
                            events_received: received,
                            events_dropped_queue_full: dropped,
                            malformed_or_unsupported: malformed,
                        };
                    }
                    sys::AMS_MEL_OK
                })
                .unwrap();
            assert_eq!(
                actual,
                C2MetadataCounters {
                    events_received: received,
                    events_dropped_queue_full: dropped,
                    malformed_or_unsupported: malformed
                }
            );
            assert_eq!(calls.get(), 1);
        }
        assert_eq!(
            stream
                .counters_with(|_, d, c, r| {
                    // SAFETY: valid diagnostic slots; failure publishes no counters.
                    unsafe {
                        diagnostic(d, c, r);
                    }
                    sys::AMS_MEL_PROVIDER_FAILED
                })
                .unwrap_err()
                .kind(),
            &ErrorKind::ProviderFailed
        );
    }

    #[test]
    fn explicit_close_is_idempotent_and_closed_operations_reject() {
        let (mut stream, closes) = fake_stream();
        assert!(stream.is_open());
        stream.close().unwrap();
        stream.close().unwrap();
        assert!(!stream.is_open());
        assert_eq!(
            stream.receive(0).unwrap_err().kind(),
            &ErrorKind::ProviderFailed
        );
        assert_eq!(
            stream.counters().unwrap_err().kind(),
            &ErrorKind::ProviderFailed
        );
        drop(stream);
        assert_eq!(closes.get(), 1);
    }

    #[test]
    fn receive_once_preserves_status_and_cleans_unexpected_error_outputs() {
        let (mut stream, _) = fake_stream();
        for (status, kind) in [
            (sys::AMS_MEL_TIMEOUT, ErrorKind::Timeout),
            (sys::AMS_MEL_STREAM_STOPPED, ErrorKind::StreamStopped),
            (sys::AMS_MEL_PROVIDER_FAILED, ErrorKind::ProviderFailed),
            (
                sys::AMS_MEL_PROVIDER_EXCEPTION,
                ErrorKind::ProviderException,
            ),
            (sys::AMS_MEL_INTERNAL_ERROR, ErrorKind::InternalError),
            (sys::AMS_MEL_INVALID_ARGUMENT, ErrorKind::InvalidArgument),
        ] {
            let (mut event, closes) = fake_event(1, 0);
            let calls = Cell::new(0);
            let result = stream.receive_event_with(&FAKE_OPS, |out, d, c, r| {
                calls.set(calls.get() + 1);
                // SAFETY: writable slots; ownership moves into receive guard.
                unsafe {
                    diagnostic(d, c, r);
                    *out = event.raw;
                }
                event.raw = ptr::null_mut();
                status
            });
            assert_eq!(result.unwrap_err().kind(), &kind);
            assert_eq!(calls.get(), 1);
            assert_eq!(closes.get(), 1);
        }
        assert_protocol(stream.receive_event_with(&FAKE_OPS, |_, d, c, r| {
            // SAFETY: valid diagnostic slots, null event output deliberately unchanged.
            unsafe {
                diagnostic(d, c, r);
            }
            sys::AMS_MEL_OK
        }));
    }

    #[test]
    fn open_null_success_zero_capacity_and_null_parent_preflight() {
        assert_protocol(C2MetadataStream::open_with(1, |_, d, c, r| {
            // SAFETY: valid diagnostic slots, deliberately missing owner.
            unsafe {
                diagnostic(d, c, r);
            }
            sys::AMS_MEL_OK
        }));
        assert_eq!(
            C2MetadataStream::open_with(0, |_, _, _, _| panic!("must not register"))
                .unwrap_err()
                .kind(),
            &ErrorKind::InvalidArgument
        );
        let mut c2 = crate::ControlChannel {
            raw: ptr::null_mut(),
            _not_send_sync: Rc::new(()),
        };
        for capacity in [0, 1] {
            assert_eq!(
                c2.open_metadata(capacity).unwrap_err().kind(),
                &ErrorKind::ProviderFailed
            );
        }
    }
}
