//! Safe IR HealthAndStatus channel lifecycle (RUST-IR-001) over the existing
//! `ams_mel_ir_health_open`, `ams_mel_ir_health_enable`,
//! `ams_mel_ir_health_get_capabilities`, `ams_mel_ir_health_close`, and
//! `ams_mel_ir_channel_from_health` C exports. No C ABI is added or changed.
//!
//! Scope: attachment, explicit enablement, complete capability snapshots,
//! Close/Drop, and the weak common [`ChannelView`] (KeepAlive, CommsTest, and
//! capabilities). The six HealthStatus metadata callback families are opened
//! through [`HealthStatusChannel::open_metadata`] (RUST-IR-002, implemented in
//! `ir_health_metadata.rs`); optional NUC reception is opened independently
//! through [`HealthStatusChannel::open_nuc`].

use std::ptr;
use std::rc::Rc;

use ams_mel_sys as sys;

use crate::{
    call_with_diagnostic, capability_snapshot, channel_view_from, protocol, raw_uci_id,
    string_view, ChannelCapability, ChannelView, ComponentLocation, Error, ErrorKind,
    HealthMetadataStream, Session, UciId,
};

const CLOSED: &str = "IR Health channel is closed";

/// Owned configuration for attaching one IR HealthAndStatus channel.
///
/// Constructing it does not contact a provider. Embedded NUL is already
/// rejected by [`UciId::new`] and [`ComponentLocation::new`]; the native facade
/// additionally validates the text during [`Session::open_health_channel`].
/// The native call borrows the string bytes only for the duration of Open and
/// copies them.
#[derive(Clone, Debug, PartialEq)]
pub struct HealthConfig {
    channel_id: UciId,
    platform_id: UciId,
    sensor_location: ComponentLocation,
}

impl HealthConfig {
    pub fn new(channel_id: UciId, platform_id: UciId, sensor_location: ComponentLocation) -> Self {
        Self {
            channel_id,
            platform_id,
            sensor_location,
        }
    }
}

impl Session {
    /// Attaches one IR HealthAndStatus channel and returns its unique typed
    /// owner. The channel is attached but NOT enabled; call
    /// [`HealthStatusChannel::enable`] explicitly.
    ///
    /// The facade verifies the attached channel's dynamic type and advertised
    /// HealthAndStatus capability and publishes an owner only on success.
    /// Native failures keep their categories, for example
    /// [`ErrorKind::FactoryFailed`] when the provider attaches no channel,
    /// [`ErrorKind::ProviderException`] when attachment throws, and
    /// [`ErrorKind::InitializationFailed`] for an incompatible channel that was
    /// detached again. A failed Open leaves no Rust owner.
    ///
    /// The returned owner does not borrow this `Session`: it may outlive
    /// [`Session::close`], because native family state retains the provider
    /// graph that it needs.
    pub fn open_health_channel(&self, config: &HealthConfig) -> Result<HealthStatusChannel, Error> {
        let raw_config = raw_health_config(config);
        let session = self.raw;
        let mut raw = ptr::null_mut();
        let result = call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: the Session is live; the exact C record and every string
            // it borrows from `config` remain valid for the duration of this
            // call; `raw` is an initially null writable owner.
            unsafe {
                sys::ams_mel_ir_health_open(
                    session,
                    &raw_config,
                    &mut raw,
                    diagnostic,
                    capacity,
                    required,
                )
            }
        });
        if let Err(error) = result {
            if !raw.is_null() {
                // A malformed native implementation must still be given the
                // opportunity to release an owner it published on failure.
                best_effort_close_health(&mut raw);
            }
            return Err(error);
        }
        if raw.is_null() {
            return Err(protocol("Health open succeeded without a channel owner"));
        }
        Ok(HealthStatusChannel {
            raw,
            _not_send_sync: Rc::new(()),
        })
    }
}

/// Uniquely owned native IR HealthAndStatus channel (`ams_mel_ir_health *`).
///
/// Lifecycle, following the native contract exactly:
///
/// ```text
/// Session::open_health_channel  -> Attached (not enabled)
/// enable                        -> Enabled; repeated enable after success is a no-op
/// capabilities / channel_view   -> valid while Attached or Enabled
/// close                         -> disable (if enable was attempted), then detach
/// ```
///
/// A failed [`enable`](Self::enable) leaves the native family Failed: later
/// Enable and capability queries fail, and Close still performs cleanup.
///
/// Ownership:
///
/// * The owner does not borrow its [`Session`]. Closing the Session first does
///   not invalidate this channel; native family state keeps the provider
///   graph alive until this owner is closed.
/// * [`close`](Self::close) can fail with the native owner deliberately
///   retained (for example when the provider's detach fails). Then
///   [`is_open`](Self::is_open) stays `true` and the same handle is offered to
///   a later explicit `close` retry; a completed close is never reported
///   falsely.
/// * Drop performs one best-effort native Close without panicking. If that
///   Close cannot establish safe teardown, the native facade keeps the
///   uncertain provider graph for the rest of the process; Drop neither frees,
///   retries, nor reports it as cleaned up.
///
/// Weak common views:
///
/// [`channel_view`](Self::channel_view) creates independent weak
/// [`ChannelView`]s for inherited KeepAlive, CommsTest, and capabilities. A
/// view never owns or closes this typed owner and never delays its teardown;
/// closing or dropping a view leaves this owner untouched. After this owner is
/// closed or dropped, every operation on a surviving view returns
/// [`ErrorKind::ProviderFailed`].
///
/// Serialization: this owner is neither `Send` nor `Sync`, and every lifecycle
/// operation takes `&mut self`, which serializes Enable, capability queries,
/// and Close on this owner. That borrow does NOT extend to weak views: a
/// `ChannelView` has no Rust lifetime tied to this owner, so mutable access to
/// the owner alone cannot order view operations against owner operations or
/// against other views. Each `ChannelView` serializes its own operations and
/// Close through its own `&mut self`; operations across separately owned
/// views and this owner must also be serialized, which the safe API achieves
/// only by confining all of them to one thread (none is `Send` or `Sync`).
/// Code that circumvents that confinement must provide that serialization
/// itself.
///
/// HealthStatus metadata is received through an independent
/// [`HealthMetadataStream`] from [`open_metadata`](Self::open_metadata).
///
/// ```compile_fail
/// fn require_send<T: Send>() {}
/// require_send::<ams_mel::HealthStatusChannel>();
/// ```
///
/// ```compile_fail
/// fn require_sync<T: Sync>() {}
/// require_sync::<ams_mel::HealthStatusChannel>();
/// ```
#[derive(Debug)]
pub struct HealthStatusChannel {
    raw: *mut sys::AmsMelIrHealth,
    _not_send_sync: Rc<()>,
}

impl HealthStatusChannel {
    /// Registers optional NUC independently of ordinary metadata. Zero capacity
    /// does not consume the one-shot; every native attempt does, even failure.
    /// No automatic retry. Registration may synchronously deliver events.
    /// A closed owner returns [`ErrorKind::ProviderFailed`] before capacity
    /// validation or native registration, with no registration return.
    pub fn open_nuc(
        &mut self,
        queue_capacity: usize,
    ) -> Result<crate::NucStream, crate::NucOpenError> {
        let health = self.live().map_err(crate::NucOpenError::preflight)?;
        crate::NucStream::open_with(queue_capacity, |out, registration, d, c, r| {
            // SAFETY: exclusively borrowed typed owner, writable output slots.
            unsafe {
                sys::ams_mel_ir_health_nuc_open(health, queue_capacity, out, registration, d, c, r)
            }
        })
    }

    /// Returns whether this wrapper still owns its native Health handle.
    ///
    /// This reports local handle ownership only, not whether the channel is
    /// enabled or whether provider operations will succeed. It stays `true`
    /// after a Close that retained the handle for an explicit retry.
    pub fn is_open(&self) -> bool {
        !self.raw.is_null()
    }

    /// Explicitly enables the attached provider channel.
    ///
    /// Repeated Enable after a successful activation returns `Ok(())` without
    /// another provider call. A provider rejection or exception is returned as
    /// an error and leaves the native family Failed; it never implies enabled
    /// service. A closed owner returns [`ErrorKind::ProviderFailed`] without
    /// calling native code.
    pub fn enable(&mut self) -> Result<(), Error> {
        let health = self.live()?;
        call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: `health` is this wrapper's live unique owner; `&mut self`
            // serializes this call with Close.
            unsafe { sys::ams_mel_ir_health_enable(health, diagnostic, capacity, required) }
        })
    }

    /// Returns a complete, fully Rust-owned [`ChannelCapability`] snapshot
    /// while the channel is attached or enabled.
    ///
    /// The native snapshot is converted while alive and then closed; no native
    /// pointer survives this call, so the value remains valid after this owner,
    /// the Session, and the provider library are gone. A closed owner returns
    /// [`ErrorKind::ProviderFailed`] without calling native code.
    pub fn capabilities(&mut self) -> Result<ChannelCapability, Error> {
        let health = self.live()?;
        capability_snapshot(|owner, diagnostic, capacity, required| {
            // SAFETY: `health` is this wrapper's live unique owner, used
            // exclusively through `&mut self`; `owner` is the helper's
            // initially null writable capability owner.
            unsafe {
                sys::ams_mel_ir_health_get_capabilities(
                    health, owner, diagnostic, capacity, required,
                )
            }
        })
    }

    /// Creates an independent weak common [`ChannelView`] of this owner.
    ///
    /// Creation is allocation only: no provider call and no lifetime change.
    /// The View does not borrow or retain this owner, its Session, or any
    /// provider object. A closed owner returns [`ErrorKind::ProviderFailed`]
    /// without calling native code.
    pub fn channel_view(&self) -> Result<ChannelView, Error> {
        let source = self.live()?;
        channel_view_from(
            |out, diagnostic, capacity, required| {
                // SAFETY: `source` is this wrapper's live Health owner, borrowed
                // only for the duration of the call; `out` is an initially null
                // writable view owner.
                unsafe {
                    sys::ams_mel_ir_channel_from_health(source, out, diagnostic, capacity, required)
                }
            },
            "Health",
        )
    }

    /// Makes this channel's single HealthStatus metadata registration attempt
    /// and returns its independent [`HealthMetadataStream`].
    ///
    /// The native facade registers all six callback families in the published
    /// order (MFA Status, BIT Status, Subsystem Status, Discrete Status,
    /// Security Audit, MFA Status Detailed) into one bounded drop-incoming
    /// queue of `queue_capacity` events. Registration is valid while Attached
    /// or Enabled and may deliver events synchronously before this returns.
    ///
    /// `queue_capacity` must be nonzero; zero is rejected with
    /// [`ErrorKind::InvalidArgument`] before any native call and does not
    /// consume the registration attempt. Every other attempt, including a
    /// failed or partial one, consumes it: a later call on the same channel is
    /// refused by the native facade with [`ErrorKind::InvalidArgument`], and
    /// this crate never retries. A provider rejection is
    /// [`ErrorKind::ProviderFailed`] and a provider throw is
    /// [`ErrorKind::ProviderException`]. A failed Open publishes no stream;
    /// partially registered callback state remains retained inactive, because
    /// upstream has no unregister operation.
    ///
    /// The stream does not borrow this owner. It is independent of the
    /// optional NUC_TempData registration through [`Self::open_nuc`].
    /// A closed owner returns [`ErrorKind::ProviderFailed`] without a native
    /// call.
    pub fn open_metadata(&mut self, queue_capacity: usize) -> Result<HealthMetadataStream, Error> {
        let health = self.live()?;
        HealthMetadataStream::open(health, queue_capacity)
    }

    /// Stops new common requests, then disables (if Enable was attempted) and
    /// detaches the provider channel.
    ///
    /// Outcomes follow native ownership exactly:
    ///
    /// * `Ok(())`: the native owner was released and
    ///   [`is_open`](Self::is_open) is `false`. Repeated Close returns `Ok(())`
    ///   without a native call.
    /// * `Err` with `is_open() == true`: the provider detach failed and the
    ///   native facade retained the owner. Teardown did not complete; a later
    ///   explicit `close` retries it.
    /// * `Err` with `is_open() == false`: the owner was released but cleanup
    ///   reported a failure (for example a failed provider disable).
    ///
    /// With in-flight common requests, native cleanup is deferred until their
    /// completion; Close does not cancel them.
    pub fn close(&mut self) -> Result<(), Error> {
        if self.raw.is_null() {
            return Ok(());
        }
        call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: `raw` is this wrapper's unique owner. Native Close either
            // clears it or deliberately retains it for a later retry.
            unsafe { sys::ams_mel_ir_health_close(&mut self.raw, diagnostic, capacity, required) }
        })?;
        if !self.raw.is_null() {
            return Err(protocol(
                "Health close succeeded without releasing the owner",
            ));
        }
        Ok(())
    }

    fn live(&self) -> Result<*mut sys::AmsMelIrHealth, Error> {
        if self.raw.is_null() {
            Err(Error::new(ErrorKind::ProviderFailed, CLOSED))
        } else {
            Ok(self.raw)
        }
    }
}

impl Drop for HealthStatusChannel {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            best_effort_close_health(&mut self.raw);
        }
    }
}

/// Builds the exact `ams_mel_ir_health_config_v1` record. Every string view
/// borrows `value`, which must outlive the native call that receives it.
fn raw_health_config(value: &HealthConfig) -> sys::AmsMelIrHealthConfigV1 {
    sys::AmsMelIrHealthConfigV1 {
        channel_id: raw_uci_id(&value.channel_id),
        channel_type: sys::AMS_MEL_IR_CHANNEL_HEALTH_AND_STATUS,
        platform_id: raw_uci_id(&value.platform_id),
        sensor_location: sys::AmsMelComponentLocationV1 {
            offset_x_m: value.sensor_location.offset_x_m,
            offset_y_m: value.sensor_location.offset_y_m,
            offset_z_m: value.sensor_location.offset_z_m,
            key: string_view(&value.sensor_location.key),
            system_name: string_view(&value.sensor_location.system_name),
        },
    }
}

fn best_effort_close_health(raw: &mut *mut sys::AmsMelIrHealth) {
    // SAFETY: called only for this wrapper's unique Health owner; null
    // diagnostics are supported. Native Close may retain the owner when safe
    // detach cannot be established. Drop then deliberately leaves it (and the
    // provider graph it retains) in place rather than freeing, retrying, or
    // reporting cleanup. The result is ignored because Drop cannot report it.
    let _ = unsafe { sys::ams_mel_ir_health_close(raw, ptr::null_mut(), 0, ptr::null_mut()) };
}

#[cfg(test)]
mod tests {
    use std::ffi::c_char;
    use std::slice;

    use super::*;

    fn bytes(view: &sys::AmsMelStringViewV1) -> &[u8] {
        if view.data.is_null() {
            assert_eq!(view.size, 0);
            return &[];
        }
        // SAFETY: test-only; every inspected view borrows a configuration that
        // outlives the returned slice.
        unsafe { slice::from_raw_parts(view.data.cast::<u8>(), view.size) }
    }

    fn null_owner() -> HealthStatusChannel {
        HealthStatusChannel {
            raw: ptr::null_mut(),
            _not_send_sync: Rc::new(()),
        }
    }

    #[test]
    fn raw_config_has_exact_health_channel_type_identity_and_location() {
        let mut channel_uuid = [0_u8; 16];
        let mut platform_uuid = [0_u8; 16];
        for (index, (channel, platform)) in channel_uuid
            .iter_mut()
            .zip(platform_uuid.iter_mut())
            .enumerate()
        {
            *channel = index as u8;
            *platform = 0xf0 + index as u8;
        }
        let config = HealthConfig::new(
            UciId::new(channel_uuid, "IR Health channel \u{3b1}").expect("channel ID"),
            UciId::new(platform_uuid, "test platform").expect("platform ID"),
            ComponentLocation::new(1.25, -2.5, 3.75, "station-1", "mock-aircraft")
                .expect("location"),
        );
        let raw = raw_health_config(&config);
        assert_eq!(sys::AMS_MEL_IR_CHANNEL_HEALTH_AND_STATUS, 4);
        assert_eq!(raw.channel_type, sys::AMS_MEL_IR_CHANNEL_HEALTH_AND_STATUS);
        assert_eq!(raw.channel_id.uuid, channel_uuid);
        assert_eq!(
            bytes(&raw.channel_id.descriptive_label),
            "IR Health channel \u{3b1}".as_bytes()
        );
        assert_eq!(raw.platform_id.uuid, platform_uuid);
        assert_eq!(bytes(&raw.platform_id.descriptive_label), b"test platform");
        let location = &raw.sensor_location;
        assert_eq!(
            (
                location.offset_x_m,
                location.offset_y_m,
                location.offset_z_m
            ),
            (1.25, -2.5, 3.75)
        );
        assert_eq!(bytes(&location.key), b"station-1");
        assert_eq!(bytes(&location.system_name), b"mock-aircraft");
        // The views borrow the owned configuration rather than copying it.
        assert_eq!(
            raw.platform_id.descriptive_label.data,
            config
                .platform_id
                .descriptive_label()
                .as_ptr()
                .cast::<c_char>()
        );
    }

    #[test]
    fn empty_config_text_is_a_null_zero_length_view() {
        let config = HealthConfig::new(
            UciId::new([0; 16], "").expect("channel ID"),
            UciId::new([0; 16], "").expect("platform ID"),
            ComponentLocation::new(0.0, 0.0, 0.0, "", "").expect("location"),
        );
        let raw = raw_health_config(&config);
        for view in [
            raw.channel_id.descriptive_label,
            raw.platform_id.descriptive_label,
            raw.sensor_location.key,
            raw.sensor_location.system_name,
        ] {
            assert!(view.data.is_null());
            assert_eq!(view.size, 0);
        }
    }

    /// A null typed owner is rejected by the wrapper before any native call,
    /// and Close/Drop of a null owner are no-ops.
    #[test]
    fn null_owner_operations_fail_without_native_calls_and_close_is_idempotent() {
        let mut health = null_owner();
        assert!(!health.is_open());
        for error in [
            health.enable().expect_err("enable on null owner"),
            health
                .capabilities()
                .expect_err("capabilities on null owner"),
            health.channel_view().expect_err("view of null owner"),
        ] {
            assert_eq!(error.kind(), &ErrorKind::ProviderFailed);
            assert_eq!(error.diagnostic(), Some(CLOSED));
            assert_eq!(error.diagnostic_required(), None);
        }
        health.close().expect("close of null owner");
        health.close().expect("repeated close of null owner");
        assert!(!health.is_open());
        drop(health);
    }

    #[test]
    fn null_owner_nuc_preflight_rejects_before_native_call_and_capacity_validation() {
        let mut health = null_owner();
        for capacity in [4, 0] {
            let error = health.open_nuc(capacity).expect_err("closed Health NUC");
            assert_eq!(error.error().kind(), &ErrorKind::ProviderFailed);
            assert_eq!(error.error().diagnostic(), Some(CLOSED));
            // A native diagnostic call would populate diagnostic_required;
            // the safe-layer liveness error has no native diagnostic record.
            assert_eq!(error.error().diagnostic_required(), None);
            assert_eq!(error.registration(), None);
            let metadata_error = health.open_metadata(capacity).expect_err("closed metadata");
            assert_eq!(error.error(), &metadata_error);
        }
    }

    /// The native facade independently rejects a null Health owner, so the
    /// wrapper guard above is defence in depth rather than the only check.
    /// Uses only production exports; no provider is loaded.
    #[test]
    fn native_facade_rejects_null_health_owner() {
        let mut capability = ptr::null_mut();
        let mut view = ptr::null_mut();
        let mut health: *mut sys::AmsMelIrHealth = ptr::null_mut();
        let nul = ptr::null_mut();
        // SAFETY: null owners and null diagnostics are documented inputs, and
        // each output is initially null writable storage.
        unsafe {
            assert_eq!(
                sys::ams_mel_ir_health_enable(ptr::null_mut(), nul, 0, ptr::null_mut()),
                sys::AMS_MEL_INVALID_ARGUMENT
            );
            assert_eq!(
                sys::ams_mel_ir_health_get_capabilities(
                    ptr::null_mut(),
                    &mut capability,
                    nul,
                    0,
                    ptr::null_mut()
                ),
                sys::AMS_MEL_INVALID_ARGUMENT
            );
            assert!(capability.is_null());
            assert_eq!(
                sys::ams_mel_ir_channel_from_health(
                    ptr::null(),
                    &mut view,
                    nul,
                    0,
                    ptr::null_mut()
                ),
                sys::AMS_MEL_INVALID_ARGUMENT
            );
            assert!(view.is_null());
            // Close of an already-null owner is the idempotent no-op that the
            // wrapper's repeated Close relies on; a null owner slot is invalid.
            assert_eq!(
                sys::ams_mel_ir_health_close(&mut health, nul, 0, ptr::null_mut()),
                sys::AMS_MEL_OK
            );
            assert!(health.is_null());
            assert_eq!(
                sys::ams_mel_ir_health_close(ptr::null_mut(), nul, 0, ptr::null_mut()),
                sys::AMS_MEL_INVALID_ARGUMENT
            );
        }
    }
}
