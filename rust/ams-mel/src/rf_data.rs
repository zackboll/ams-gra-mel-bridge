//! Safe RF DataMEL foundation and RFMFAInfo snapshots (RUST-MEL-002) over the
//! existing `ams_mel_rf_data_open`, `ams_mel_rf_data_get_provider_version`,
//! `ams_mel_rf_data_quantize_duration`, `ams_mel_rf_data_get_mfa_info`,
//! `ams_mel_rf_mfa_info_view`, `ams_mel_rf_mfa_info_close`, and
//! `ams_mel_rf_data_close` C exports. No C ABI is added or changed.

use std::mem;
use std::path::Path;
use std::ptr;
use std::rc::Rc;

use ams_mel_sys as sys;

use crate::{
    c_path, c_string, call_with_diagnostic, checked_bool, checked_span, copy_mapped,
    owned_u32_mapped, protocol, query_provider_version, Error, ErrorKind, ProviderVersion,
};

/// Raw upstream `rfmel::JobDataFormat` value reported in an [`RfMfaInfo`].
///
/// The fourteen published values map to named variants whose raw values are
/// exactly the `AMS_MEL_RF_JOB_DATA_FORMAT_*` constants. Upstream publishes no
/// `MaxExclusive` sentinel, so any other provider value, for example a future
/// `0xF0000001`, is preserved bit-for-bit as [`Unknown`](Self::Unknown) and is
/// never dropped or rejected.
///
/// [`from_raw`](Self::from_raw) is canonical: it never produces `Unknown` for
/// a published value. [`as_raw`](Self::as_raw) returns the exact raw value for
/// every variant, including `Unknown`.
#[derive(Clone, Copy, Debug, Eq, PartialEq, Hash)]
pub enum RfJobDataFormat {
    DirectInt8,
    DirectInt16,
    ComplexInt8,
    ComplexInt16,
    AmsVitaSmall,
    AmsVitaMedium,
    AmsVitaLarge,
    AmsVitaExtraLarge,
    PdwType1,
    PdwType2,
    PdwType3,
    LfType1,
    LfType2,
    LfType3,
    Unknown(u32),
}

impl RfJobDataFormat {
    /// Maps a raw native value without loss. Unpublished values become
    /// [`Unknown`](Self::Unknown) carrying the exact raw bits.
    pub const fn from_raw(raw: u32) -> Self {
        match raw {
            sys::AMS_MEL_RF_JOB_DATA_FORMAT_DIRECT_INT8 => Self::DirectInt8,
            sys::AMS_MEL_RF_JOB_DATA_FORMAT_DIRECT_INT16 => Self::DirectInt16,
            sys::AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT8 => Self::ComplexInt8,
            sys::AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT16 => Self::ComplexInt16,
            sys::AMS_MEL_RF_JOB_DATA_FORMAT_AMS_VITA_SMALL => Self::AmsVitaSmall,
            sys::AMS_MEL_RF_JOB_DATA_FORMAT_AMS_VITA_MEDIUM => Self::AmsVitaMedium,
            sys::AMS_MEL_RF_JOB_DATA_FORMAT_AMS_VITA_LARGE => Self::AmsVitaLarge,
            sys::AMS_MEL_RF_JOB_DATA_FORMAT_AMS_VITA_EXTRA_LARGE => Self::AmsVitaExtraLarge,
            sys::AMS_MEL_RF_JOB_DATA_FORMAT_PDW_TYPE1 => Self::PdwType1,
            sys::AMS_MEL_RF_JOB_DATA_FORMAT_PDW_TYPE2 => Self::PdwType2,
            sys::AMS_MEL_RF_JOB_DATA_FORMAT_PDW_TYPE3 => Self::PdwType3,
            sys::AMS_MEL_RF_JOB_DATA_FORMAT_LF_TYPE1 => Self::LfType1,
            sys::AMS_MEL_RF_JOB_DATA_FORMAT_LF_TYPE2 => Self::LfType2,
            sys::AMS_MEL_RF_JOB_DATA_FORMAT_LF_TYPE3 => Self::LfType3,
            unknown => Self::Unknown(unknown),
        }
    }

    /// Returns the exact raw `JobDataFormat` value.
    pub const fn as_raw(self) -> u32 {
        match self {
            Self::DirectInt8 => sys::AMS_MEL_RF_JOB_DATA_FORMAT_DIRECT_INT8,
            Self::DirectInt16 => sys::AMS_MEL_RF_JOB_DATA_FORMAT_DIRECT_INT16,
            Self::ComplexInt8 => sys::AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT8,
            Self::ComplexInt16 => sys::AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT16,
            Self::AmsVitaSmall => sys::AMS_MEL_RF_JOB_DATA_FORMAT_AMS_VITA_SMALL,
            Self::AmsVitaMedium => sys::AMS_MEL_RF_JOB_DATA_FORMAT_AMS_VITA_MEDIUM,
            Self::AmsVitaLarge => sys::AMS_MEL_RF_JOB_DATA_FORMAT_AMS_VITA_LARGE,
            Self::AmsVitaExtraLarge => sys::AMS_MEL_RF_JOB_DATA_FORMAT_AMS_VITA_EXTRA_LARGE,
            Self::PdwType1 => sys::AMS_MEL_RF_JOB_DATA_FORMAT_PDW_TYPE1,
            Self::PdwType2 => sys::AMS_MEL_RF_JOB_DATA_FORMAT_PDW_TYPE2,
            Self::PdwType3 => sys::AMS_MEL_RF_JOB_DATA_FORMAT_PDW_TYPE3,
            Self::LfType1 => sys::AMS_MEL_RF_JOB_DATA_FORMAT_LF_TYPE1,
            Self::LfType2 => sys::AMS_MEL_RF_JOB_DATA_FORMAT_LF_TYPE2,
            Self::LfType3 => sys::AMS_MEL_RF_JOB_DATA_FORMAT_LF_TYPE3,
            Self::Unknown(raw) => raw,
        }
    }
}

impl From<u32> for RfJobDataFormat {
    fn from(raw: u32) -> Self {
        Self::from_raw(raw)
    }
}

impl From<RfJobDataFormat> for u32 {
    fn from(format: RfJobDataFormat) -> Self {
        format.as_raw()
    }
}

/// Upstream `FrequencyRange`, copied verbatim in Hz.
///
/// Ranges are never normalized, merged, sorted, clamped, or unit-converted:
/// `min_hz > max_hz` is reported exactly as the provider produced it.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct RfFrequencyRange {
    pub min_hz: f64,
    pub max_hz: f64,
}

/// One face reported by `RFMFAInfo::getFaceIDs()`.
///
/// `face_id` is the provider-assigned identifier, not a zero-based index.
/// Every `*_fs` field is the exact signed femtosecond count of the upstream
/// `Femtoseconds` value, with no unit conversion. The frequency-range vectors
/// are, in order, `getRxFrequencyRanges`, `getTxFrequencyRanges`, and
/// `getSampleFrequencyRange` for this face, in the provider's order.
#[derive(Clone, Debug, PartialEq)]
pub struct RfFaceInfo {
    pub face_id: u32,
    pub supports_receive: bool,
    pub supports_transmit: bool,
    pub requires_endpoint_association: bool,
    pub agc_processing_time_fs: i64,
    pub min_job_request_lead_time_fs: i64,
    pub max_job_request_lead_time_fs: i64,
    pub min_job_detail_lead_time_fs: i64,
    pub tx_rx_switching_time_fs: i64,
    pub rx_tx_switching_time_fs: i64,
    pub tx_tx_switching_time_fs: i64,
    pub rx_rx_switching_time_fs: i64,
    pub rx_frequency_ranges: Vec<RfFrequencyRange>,
    pub tx_frequency_ranges: Vec<RfFrequencyRange>,
    pub sample_frequency_ranges: Vec<RfFrequencyRange>,
}

/// Complete, fully Rust-owned point-in-time copy of
/// `ams_mel_rf_mfa_info_v1`.
///
/// No native snapshot owner or pointer is attached: the value stays valid
/// after [`RfData::close`], provider destruction, and provider library unload.
///
/// `reported_num_faces` is `getNumFaces()` verbatim and is deliberately
/// independent of `faces.len()`; upstream publishes no requirement that the
/// two agree. `supported_data_formats` and `faces` keep the provider's
/// collection order.
#[derive(Clone, Debug, PartialEq)]
pub struct RfMfaInfo {
    pub reported_num_faces: u64,
    pub contains_open_additions: bool,
    pub scheduler_resolution_fs: i64,
    pub max_user_defined_context_bytes: u64,
    pub supported_data_formats: Vec<RfJobDataFormat>,
    pub faces: Vec<RfFaceInfo>,
}

/// Safe owner of one independent RF `createDataMEL` provider object
/// (`ams_mel_rf_data *`) and of the provider library that created it.
///
/// An `RfData` is not an IR [`Session`](crate::Session) or an
/// [`RfAdmin`](crate::RfAdmin) and neither creates nor shares either. Raw
/// pointers and provider objects are never exposed.
///
/// Lifecycle:
///
/// - [`close`](Self::close) consumes the native owner exactly once, even when
///   provider shutdown throws. Once closed, further `close` calls return
///   `Ok(())` without calling native code, and every provider operation fails
///   with [`ErrorKind::ProviderFailed`] without a provider call.
/// - On successful shutdown the bridge destroys the provider DataMEL *before*
///   unloading its provider library.
/// - If shutdown throws, the error is returned and the owner is closed. The
///   native bridge intentionally retains the uncertain provider graph and
///   library for the rest of the process; this binding never retries shutdown
///   or unloads the provider itself.
/// - [`Drop`] performs the same Close for a still-open owner as best-effort
///   cleanup. It never panics and reports nothing; call `close` to observe
///   the outcome.
///
/// The owner is intentionally neither `Send` nor `Sync`, and every provider
/// operation and Close take `&mut self`, so the native requirement that
/// operations on one owner be externally serialized is enforced statically.
///
/// ```compile_fail
/// fn require_send<T: Send>() {}
/// require_send::<ams_mel::RfData>();
/// ```
///
/// ```compile_fail
/// fn require_sync<T: Sync>() {}
/// require_sync::<ams_mel::RfData>();
/// ```
///
/// ```no_run
/// use ams_mel::RfData;
///
/// # fn main() -> Result<(), ams_mel::Error> {
/// let mut data = RfData::open("/opt/provider/librfmel.so", "configuration")?;
/// let version = data.provider_version()?;
/// let quantized_fs = data.quantize_duration(1_000_000)?;
/// let info = data.mfa_info()?;
/// data.close()?;
/// // `info` is fully owned and remains usable after Close.
/// for face in &info.faces {
///     println!("face {} receive={}", face.face_id, face.supports_receive);
/// }
/// # let _ = (version, quantized_fs);
/// # Ok(())
/// # }
/// ```
#[derive(Debug)]
pub struct RfData {
    raw: *mut sys::AmsMelRfData,
    _not_send_sync: Rc<()>,
}

impl RfData {
    /// Loads `provider_library`, resolves its `createDataMEL` factory and
    /// calls it once with `configuration`.
    ///
    /// The path must be valid UTF-8 and neither argument may contain an
    /// embedded NUL; both are rejected as [`ErrorKind::InvalidArgument`]
    /// before any native call. Native failures keep their categories, for
    /// example [`ErrorKind::LibraryLoadFailed`],
    /// [`ErrorKind::SymbolNotFound`], [`ErrorKind::FactoryFailed`] for a null
    /// factory result, [`ErrorKind::InternalError`] for allocation failure,
    /// and [`ErrorKind::ProviderException`] for a throwing factory. A failed
    /// open leaves no owner, and the bridge has already unloaded the provider.
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
                sys::ams_mel_rf_data_open(
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
                "RF Data open succeeded without returning an owner",
            )),
            Ok(()) => Ok(Self {
                raw,
                _not_send_sync: Rc::new(()),
            }),
            Err(error) => {
                if !raw.is_null() {
                    // A malformed native implementation must still be given
                    // the opportunity to release an owner it published.
                    best_effort_close_rf_data(&mut raw);
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

    /// Queries the provider's `getVersionInfo` with the same two-stage
    /// protocol as
    /// [`Session::provider_version`](crate::Session::provider_version): a
    /// size query, exactly sized buffers (lengths include the terminal NUL),
    /// then the complete copy. Strings are never truncated. Malformed native
    /// output is [`ErrorKind::ProtocolInconsistency`] or
    /// [`ErrorKind::InvalidUtf8`]; provider failures keep their native
    /// category and diagnostic length, and the owner stays open. A closed
    /// owner returns [`ErrorKind::ProviderFailed`] without a provider call.
    pub fn provider_version(&mut self) -> Result<ProviderVersion, Error> {
        let data = self.live()?;
        query_provider_version(|version, diagnostic, capacity, required| {
            // SAFETY: `data` is this wrapper's live unique owner and `&mut
            // self` serializes this call with Close. The helper passes a valid
            // version record whose buffers match their advertised capacities.
            unsafe {
                sys::ams_mel_rf_data_get_provider_version(
                    data, version, diagnostic, capacity, required,
                )
            }
        })
    }

    /// Live `RFMFAInfo::quantizeDuration` query.
    ///
    /// Input and output are exact signed femtosecond counts. The provider's
    /// result is authoritative: no quantization is computed in Rust and
    /// [`RfMfaInfo::scheduler_resolution_fs`] is never used as a substitute.
    /// Provider exceptions are [`ErrorKind::ProviderException`] and
    /// allocation failures [`ErrorKind::InternalError`]; the owner stays open.
    /// A closed owner returns [`ErrorKind::ProviderFailed`] without a provider
    /// call.
    pub fn quantize_duration(&mut self, femtoseconds: i64) -> Result<i64, Error> {
        let data = self.live()?;
        let mut quantized = 0_i64;
        call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: `data` is this wrapper's live unique owner and `&mut
            // self` serializes this call with Close. `quantized` is writable.
            unsafe {
                sys::ams_mel_rf_data_quantize_duration(
                    data,
                    femtoseconds,
                    &mut quantized,
                    diagnostic,
                    capacity,
                    required,
                )
            }
        })?;
        Ok(quantized)
    }

    /// Takes a complete point-in-time RFMFAInfo snapshot and returns it as
    /// fully Rust-owned data.
    ///
    /// The native snapshot is validated and copied while alive, then closed
    /// exactly once on every path, including conversion and allocation
    /// failures. If a provider getter throws, no partial snapshot is returned
    /// and the owner stays open, so a later snapshot may succeed. Malformed
    /// native spans or Booleans are [`ErrorKind::ProtocolInconsistency`]. A
    /// closed owner returns [`ErrorKind::ProviderFailed`] without a provider
    /// call.
    pub fn mfa_info(&mut self) -> Result<RfMfaInfo, Error> {
        let data = self.live()?;
        snapshot_with(data, mfa_info_from_raw)
    }

    /// Shuts down and releases the provider DataMEL exactly once.
    ///
    /// The owner is consumed whether or not provider shutdown succeeds; on
    /// return [`is_open`](Self::is_open) is always `false`. A throwing
    /// shutdown returns its error (normally [`ErrorKind::ProviderException`]
    /// with the provider diagnostic). The native bridge then intentionally
    /// retains the uncertain provider graph and library, and shutdown is never
    /// retried. Repeated calls return `Ok(())` without native calls.
    pub fn close(&mut self) -> Result<(), Error> {
        if self.raw.is_null() {
            return Ok(());
        }
        let result = call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: `raw` is this wrapper's unique live owner. Native Close
            // consumes and nulls it before shutdown, including on failure.
            unsafe { sys::ams_mel_rf_data_close(&mut self.raw, diagnostic, capacity, required) }
        });
        // The C contract always consumes the owner. Should an implementation
        // ever fail to clear it, the handle is deliberately abandoned rather
        // than offered to a second shutdown through Close or Drop.
        let retained = !self.raw.is_null();
        self.raw = ptr::null_mut();
        match result {
            Err(error) => Err(error),
            Ok(()) if retained => Err(protocol(
                "RF Data close succeeded without consuming its owner",
            )),
            Ok(()) => Ok(()),
        }
    }

    fn live(&self) -> Result<*mut sys::AmsMelRfData, Error> {
        if self.raw.is_null() {
            Err(Error::new(ErrorKind::ProviderFailed, "RF Data is closed"))
        } else {
            Ok(self.raw)
        }
    }
}

impl Drop for RfData {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            best_effort_close_rf_data(&mut self.raw);
            self.raw = ptr::null_mut();
        }
    }
}

/// Private RAII owner of one native `ams_mel_rf_mfa_info` snapshot. The
/// native owner is closed exactly once: explicitly through
/// [`close`](Self::close), or by Drop on every early return, conversion
/// failure, allocation failure, or unwind. Snapshot Close makes no provider
/// call and is independent of the RF Data lifetime.
struct MfaInfoOwner {
    raw: *mut sys::AmsMelRfMfaInfo,
}

impl MfaInfoOwner {
    fn close(&mut self) -> Result<(), Error> {
        if self.raw.is_null() {
            return Ok(());
        }
        note_native_snapshot_close();
        let result = call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: `raw` is this guard's unique snapshot owner and no
            // record borrowed from it is used after this call.
            unsafe { sys::ams_mel_rf_mfa_info_close(&mut self.raw, diagnostic, capacity, required) }
        });
        // Never offer the owner to a second close, even if a malformed
        // implementation failed to clear it.
        self.raw = ptr::null_mut();
        result
    }
}

impl Drop for MfaInfoOwner {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            note_native_snapshot_close();
            // SAFETY: unique snapshot owner; null diagnostics are supported.
            // The result cannot be reported from Drop.
            let _ = unsafe {
                sys::ams_mel_rf_mfa_info_close(&mut self.raw, ptr::null_mut(), 0, ptr::null_mut())
            };
            self.raw = ptr::null_mut();
        }
    }
}

/// Requests one native snapshot from the live `data`, converts its record
/// with `convert` while the snapshot is alive, and then closes the snapshot.
/// The guard closes the native snapshot exactly once whether conversion
/// succeeds, fails, or unwinds. A conversion error takes precedence over a
/// snapshot-close error. Only the owned result of `convert` escapes.
fn snapshot_with<T>(
    data: *const sys::AmsMelRfData,
    convert: impl FnOnce(&sys::AmsMelRfMfaInfoV1) -> Result<T, Error>,
) -> Result<T, Error> {
    let mut owner = MfaInfoOwner {
        raw: ptr::null_mut(),
    };
    call_with_diagnostic(|diagnostic, capacity, required| {
        // SAFETY: `data` is the caller's live, exclusively borrowed owner;
        // `owner.raw` is an initially null writable owner that the guard
        // closes on every return path.
        unsafe {
            sys::ams_mel_rf_data_get_mfa_info(data, &mut owner.raw, diagnostic, capacity, required)
        }
    })?;
    if owner.raw.is_null() {
        return Err(protocol(
            "RF MFA snapshot succeeded without a snapshot owner",
        ));
    }
    let mut view: *const sys::AmsMelRfMfaInfoV1 = ptr::null();
    call_with_diagnostic(|diagnostic, capacity, required| {
        // SAFETY: `owner.raw` is a live snapshot owner and `view` is writable
        // storage for the borrowed record pointer.
        unsafe {
            sys::ams_mel_rf_mfa_info_view(owner.raw, &mut view, diagnostic, capacity, required)
        }
    })?;
    if view.is_null() {
        return Err(protocol(
            "RF MFA snapshot view succeeded without a snapshot record",
        ));
    }
    if !(view as usize).is_multiple_of(mem::align_of::<sys::AmsMelRfMfaInfoV1>()) {
        return Err(protocol("native RF MFA snapshot record is misaligned"));
    }
    let converted = {
        // SAFETY: `view` is non-null, aligned, and points to a record owned by
        // the live `owner`. The copied record and every span it references
        // are used only inside this block, before `owner` is closed.
        let raw = unsafe { view.read() };
        convert(&raw)
    };
    let closed = owner.close();
    let value = converted?;
    closed?;
    Ok(value)
}

/// Converts a complete native snapshot record into fully owned Rust data.
/// Every span is checked before a slice is formed and every nested vector is
/// copied with fallible allocation; no native pointer escapes.
fn mfa_info_from_raw(raw: &sys::AmsMelRfMfaInfoV1) -> Result<RfMfaInfo, Error> {
    // SAFETY: the span belongs to a native record kept alive by the caller.
    let faces = unsafe { checked_span(raw.faces.data, raw.faces.size, "faces")? };
    Ok(RfMfaInfo {
        reported_num_faces: raw.reported_num_faces,
        contains_open_additions: checked_bool(
            raw.contains_open_additions,
            "contains_open_additions",
        )?,
        scheduler_resolution_fs: raw.scheduler_resolution_fs,
        max_user_defined_context_bytes: raw.max_user_defined_context_bytes,
        supported_data_formats: owned_u32_mapped(
            raw.supported_data_formats,
            "supported_data_formats",
            RfJobDataFormat::from_raw,
        )?,
        faces: copy_mapped(faces, "faces", face_info_from_raw)?,
    })
}

fn face_info_from_raw(raw: &sys::AmsMelRfFaceInfoV1) -> Result<RfFaceInfo, Error> {
    let id = raw.face_id;
    let field = |name: &str| format!("face {id} {name}");
    Ok(RfFaceInfo {
        face_id: id,
        supports_receive: checked_bool(raw.supports_receive, &field("supports_receive"))?,
        supports_transmit: checked_bool(raw.supports_transmit, &field("supports_transmit"))?,
        requires_endpoint_association: checked_bool(
            raw.requires_endpoint_association,
            &field("requires_endpoint_association"),
        )?,
        agc_processing_time_fs: raw.agc_processing_time_fs,
        min_job_request_lead_time_fs: raw.min_job_request_lead_time_fs,
        max_job_request_lead_time_fs: raw.max_job_request_lead_time_fs,
        min_job_detail_lead_time_fs: raw.min_job_detail_lead_time_fs,
        tx_rx_switching_time_fs: raw.tx_rx_switching_time_fs,
        rx_tx_switching_time_fs: raw.rx_tx_switching_time_fs,
        tx_tx_switching_time_fs: raw.tx_tx_switching_time_fs,
        rx_rx_switching_time_fs: raw.rx_rx_switching_time_fs,
        rx_frequency_ranges: frequency_ranges(
            raw.rx_frequency_ranges,
            &field("rx_frequency_ranges"),
        )?,
        tx_frequency_ranges: frequency_ranges(
            raw.tx_frequency_ranges,
            &field("tx_frequency_ranges"),
        )?,
        sample_frequency_ranges: frequency_ranges(
            raw.sample_frequency_ranges,
            &field("sample_frequency_ranges"),
        )?,
    })
}

fn frequency_ranges(
    span: sys::AmsMelRfFrequencyRangeSpanV1,
    field: &str,
) -> Result<Vec<RfFrequencyRange>, Error> {
    // SAFETY: the span belongs to a native record kept alive by the caller.
    let ranges = unsafe { checked_span(span.data, span.size, field)? };
    copy_mapped(ranges, field, |range| {
        Ok(RfFrequencyRange {
            min_hz: range.min_hz,
            max_hz: range.max_hz,
        })
    })
}

fn best_effort_close_rf_data(raw: &mut *mut sys::AmsMelRfData) {
    // SAFETY: called only for a unique RF Data owner. Null diagnostics are
    // supported. Native Close consumes the owner even when shutdown throws, so
    // it is never retried; the result is ignored because Drop cannot report it.
    let _ = unsafe { sys::ams_mel_rf_data_close(raw, ptr::null_mut(), 0, ptr::null_mut()) };
}

#[cfg(test)]
thread_local! {
    /// Unit-test-only count of native snapshot Close calls issued by
    /// [`MfaInfoOwner`] on this thread. Compiled out of every other build.
    static NATIVE_SNAPSHOT_CLOSES: std::cell::Cell<u32> = const { std::cell::Cell::new(0) };
}

#[cfg(test)]
fn note_native_snapshot_close() {
    NATIVE_SNAPSHOT_CLOSES.with(|count| count.set(count.get() + 1));
}

#[cfg(not(test))]
fn note_native_snapshot_close() {}

#[cfg(test)]
mod tests {
    use super::*;

    use std::env;
    use std::panic::{self, AssertUnwindSafe};
    use std::path::PathBuf;

    const PUBLISHED: [(u32, RfJobDataFormat); 14] = [
        (0, RfJobDataFormat::DirectInt8),
        (1, RfJobDataFormat::DirectInt16),
        (2, RfJobDataFormat::ComplexInt8),
        (3, RfJobDataFormat::ComplexInt16),
        (4, RfJobDataFormat::AmsVitaSmall),
        (5, RfJobDataFormat::AmsVitaMedium),
        (6, RfJobDataFormat::AmsVitaLarge),
        (7, RfJobDataFormat::AmsVitaExtraLarge),
        (8, RfJobDataFormat::PdwType1),
        (9, RfJobDataFormat::PdwType2),
        (10, RfJobDataFormat::PdwType3),
        (11, RfJobDataFormat::LfType1),
        (12, RfJobDataFormat::LfType2),
        (13, RfJobDataFormat::LfType3),
    ];

    const PUBLISHED_CONSTANTS: [u32; 14] = [
        sys::AMS_MEL_RF_JOB_DATA_FORMAT_DIRECT_INT8,
        sys::AMS_MEL_RF_JOB_DATA_FORMAT_DIRECT_INT16,
        sys::AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT8,
        sys::AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT16,
        sys::AMS_MEL_RF_JOB_DATA_FORMAT_AMS_VITA_SMALL,
        sys::AMS_MEL_RF_JOB_DATA_FORMAT_AMS_VITA_MEDIUM,
        sys::AMS_MEL_RF_JOB_DATA_FORMAT_AMS_VITA_LARGE,
        sys::AMS_MEL_RF_JOB_DATA_FORMAT_AMS_VITA_EXTRA_LARGE,
        sys::AMS_MEL_RF_JOB_DATA_FORMAT_PDW_TYPE1,
        sys::AMS_MEL_RF_JOB_DATA_FORMAT_PDW_TYPE2,
        sys::AMS_MEL_RF_JOB_DATA_FORMAT_PDW_TYPE3,
        sys::AMS_MEL_RF_JOB_DATA_FORMAT_LF_TYPE1,
        sys::AMS_MEL_RF_JOB_DATA_FORMAT_LF_TYPE2,
        sys::AMS_MEL_RF_JOB_DATA_FORMAT_LF_TYPE3,
    ];

    fn range(min_hz: f64, max_hz: f64) -> sys::AmsMelRfFrequencyRangeV1 {
        sys::AmsMelRfFrequencyRangeV1 { min_hz, max_hz }
    }

    fn ranges(values: &[sys::AmsMelRfFrequencyRangeV1]) -> sys::AmsMelRfFrequencyRangeSpanV1 {
        sys::AmsMelRfFrequencyRangeSpanV1 {
            data: values.as_ptr(),
            size: values.len(),
        }
    }

    fn null_ranges(size: usize) -> sys::AmsMelRfFrequencyRangeSpanV1 {
        sys::AmsMelRfFrequencyRangeSpanV1 {
            data: ptr::null(),
            size,
        }
    }

    fn face(
        face_id: u32,
        rx: &[sys::AmsMelRfFrequencyRangeV1],
        tx: &[sys::AmsMelRfFrequencyRangeV1],
        sample: &[sys::AmsMelRfFrequencyRangeV1],
    ) -> sys::AmsMelRfFaceInfoV1 {
        sys::AmsMelRfFaceInfoV1 {
            face_id,
            supports_receive: 1,
            supports_transmit: 0,
            requires_endpoint_association: 1,
            agc_processing_time_fs: i64::MIN,
            min_job_request_lead_time_fs: -1,
            max_job_request_lead_time_fs: i64::MAX,
            min_job_detail_lead_time_fs: 0,
            tx_rx_switching_time_fs: 1,
            rx_tx_switching_time_fs: -2,
            tx_tx_switching_time_fs: 3,
            rx_rx_switching_time_fs: -4,
            rx_frequency_ranges: ranges(rx),
            tx_frequency_ranges: ranges(tx),
            sample_frequency_ranges: ranges(sample),
        }
    }

    fn info(formats: &[u32], faces: &[sys::AmsMelRfFaceInfoV1]) -> sys::AmsMelRfMfaInfoV1 {
        sys::AmsMelRfMfaInfoV1 {
            reported_num_faces: u64::MAX,
            contains_open_additions: 0,
            scheduler_resolution_fs: i64::MIN,
            max_user_defined_context_bytes: u64::MAX,
            supported_data_formats: sys::AmsMelU32SpanV1 {
                data: formats.as_ptr(),
                size: formats.len(),
            },
            faces: sys::AmsMelRfFaceInfoSpanV1 {
                data: faces.as_ptr(),
                size: faces.len(),
            },
        }
    }

    fn expect_protocol(raw: &sys::AmsMelRfMfaInfoV1, needle: &str) {
        let error = mfa_info_from_raw(raw).expect_err("malformed native snapshot");
        assert_eq!(error.kind(), &ErrorKind::ProtocolInconsistency, "{needle}");
        assert!(
            error.diagnostic().is_some_and(|text| text.contains(needle)),
            "{needle}: {:?}",
            error.diagnostic()
        );
    }

    fn bits(values: &[RfFrequencyRange]) -> Vec<(u64, u64)> {
        values
            .iter()
            .map(|range| (range.min_hz.to_bits(), range.max_hz.to_bits()))
            .collect()
    }

    fn native_bits(values: &[sys::AmsMelRfFrequencyRangeV1]) -> Vec<(u64, u64)> {
        values
            .iter()
            .map(|range| (range.min_hz.to_bits(), range.max_hz.to_bits()))
            .collect()
    }

    #[test]
    fn every_published_format_round_trips_and_unknown_values_are_preserved() {
        for (index, ((raw, format), constant)) in
            PUBLISHED.into_iter().zip(PUBLISHED_CONSTANTS).enumerate()
        {
            assert_eq!(raw as usize, index);
            assert_eq!(constant, raw);
            assert_eq!(RfJobDataFormat::from_raw(raw), format);
            assert_eq!(RfJobDataFormat::from(raw), format);
            assert_eq!(format.as_raw(), raw);
            assert_eq!(u32::from(format), raw);
        }
        for raw in [14, 15, 0x7FFF_FFFF, 0xF000_0001, u32::MAX] {
            let format = RfJobDataFormat::from_raw(raw);
            assert_eq!(format, RfJobDataFormat::Unknown(raw));
            assert_eq!(format.as_raw(), raw);
            assert_eq!(u32::from(format), raw);
        }
    }

    #[test]
    fn converts_every_field_exactly_in_provider_order() {
        let rx = [range(2.5, 1.25), range(-0.0, f64::MAX)];
        let sample = [range(f64::MIN_POSITIVE, 0.5), range(1e300, -1e300)];
        let tx = [range(f64::NAN, f64::INFINITY)];
        let faces = [face(42, &rx, &[], &sample), face(7, &[], &tx, &[])];
        let formats = [13, 0xF000_0001, 0, 7];
        let raw = info(&formats, &faces);
        let value = mfa_info_from_raw(&raw).expect("well-formed snapshot");

        assert_eq!(value.reported_num_faces, u64::MAX);
        assert_ne!(value.reported_num_faces, value.faces.len() as u64);
        assert!(!value.contains_open_additions);
        assert_eq!(value.scheduler_resolution_fs, i64::MIN);
        assert_eq!(value.max_user_defined_context_bytes, u64::MAX);
        assert_eq!(
            value.supported_data_formats,
            [
                RfJobDataFormat::LfType3,
                RfJobDataFormat::Unknown(0xF000_0001),
                RfJobDataFormat::DirectInt8,
                RfJobDataFormat::AmsVitaExtraLarge,
            ]
        );
        // Provider order and IDs: not sorted and not zero-based indexes.
        let ids: Vec<u32> = value.faces.iter().map(|face| face.face_id).collect();
        assert_eq!(ids, [42, 7]);
        let first = &value.faces[0];
        assert!(first.supports_receive && !first.supports_transmit);
        assert!(first.requires_endpoint_association);
        assert_eq!(
            [
                first.agc_processing_time_fs,
                first.min_job_request_lead_time_fs,
                first.max_job_request_lead_time_fs,
                first.min_job_detail_lead_time_fs,
                first.tx_rx_switching_time_fs,
                first.rx_tx_switching_time_fs,
                first.tx_tx_switching_time_fs,
                first.rx_rx_switching_time_fs,
            ],
            [i64::MIN, -1, i64::MAX, 0, 1, -2, 3, -4]
        );
        // Bit-exact copies: no normalization, clamping, or sign/NaN loss.
        assert_eq!(bits(&first.rx_frequency_ranges), native_bits(&rx));
        assert_eq!(bits(&first.sample_frequency_ranges), native_bits(&sample));
        assert!(first.tx_frequency_ranges.is_empty());
        assert_eq!(bits(&value.faces[1].tx_frequency_ranges), native_bits(&tx));
        assert!(value.faces[1].rx_frequency_ranges.is_empty());
        assert!(value.faces[1].sample_frequency_ranges.is_empty());
    }

    #[test]
    fn empty_spans_with_null_or_non_null_data_are_empty() {
        let unused = [range(1.0, 2.0)];
        let mut raw_face = face(9, &[], &[], &[]);
        raw_face.rx_frequency_ranges = null_ranges(0);
        raw_face.tx_frequency_ranges = sys::AmsMelRfFrequencyRangeSpanV1 {
            data: unused.as_ptr(),
            size: 0,
        };
        let faces = [raw_face];
        let mut raw = info(&[], &faces);
        raw.supported_data_formats = sys::AmsMelU32SpanV1 {
            data: ptr::null(),
            size: 0,
        };
        let value = mfa_info_from_raw(&raw).expect("empty spans");
        assert!(value.supported_data_formats.is_empty());
        assert!(value.faces[0].rx_frequency_ranges.is_empty());
        assert!(value.faces[0].tx_frequency_ranges.is_empty());

        let mut raw = info(&[], &[]);
        raw.faces = sys::AmsMelRfFaceInfoSpanV1 {
            data: ptr::null(),
            size: 0,
        };
        assert!(mfa_info_from_raw(&raw).expect("no faces").faces.is_empty());
    }

    #[test]
    fn nonempty_spans_with_null_data_are_rejected() {
        let mut raw = info(&[], &[]);
        raw.faces = sys::AmsMelRfFaceInfoSpanV1 {
            data: ptr::null(),
            size: 1,
        };
        expect_protocol(&raw, "faces span has a null pointer");

        let mut raw = info(&[], &[]);
        raw.supported_data_formats = sys::AmsMelU32SpanV1 {
            data: ptr::null(),
            size: 3,
        };
        expect_protocol(&raw, "supported_data_formats span has a null pointer");

        for which in 0..3 {
            let mut raw_face = face(5, &[], &[], &[]);
            let name = match which {
                0 => {
                    raw_face.rx_frequency_ranges = null_ranges(2);
                    "face 5 rx_frequency_ranges span has a null pointer"
                }
                1 => {
                    raw_face.tx_frequency_ranges = null_ranges(1);
                    "face 5 tx_frequency_ranges span has a null pointer"
                }
                _ => {
                    raw_face.sample_frequency_ranges = null_ranges(usize::MAX);
                    "face 5 sample_frequency_ranges span has a null pointer"
                }
            };
            let faces = [raw_face];
            expect_protocol(&info(&[], &faces), name);
        }
    }

    #[test]
    fn misaligned_spans_are_rejected_before_slicing() {
        // Over-aligned backing storage; a one-byte offset is never aligned for
        // u32, f64 ranges, or face records. Nothing is dereferenced.
        let storage = [0_u64; 16];
        let misaligned = storage.as_ptr().cast::<u8>().wrapping_add(1);

        let mut raw = info(&[], &[]);
        raw.supported_data_formats = sys::AmsMelU32SpanV1 {
            data: misaligned.cast(),
            size: 1,
        };
        expect_protocol(&raw, "supported_data_formats span is misaligned");

        let mut raw = info(&[], &[]);
        raw.faces = sys::AmsMelRfFaceInfoSpanV1 {
            data: misaligned.cast(),
            size: 1,
        };
        expect_protocol(&raw, "faces span is misaligned");

        let mut raw_face = face(3, &[], &[], &[]);
        raw_face.sample_frequency_ranges = sys::AmsMelRfFrequencyRangeSpanV1 {
            data: misaligned.cast(),
            size: 1,
        };
        let faces = [raw_face];
        expect_protocol(
            &info(&[], &faces),
            "face 3 sample_frequency_ranges span is misaligned",
        );
    }

    #[test]
    fn impossible_span_sizes_are_rejected_before_slicing() {
        let formats = [0_u32];
        let mut raw = info(&formats, &[]);
        raw.supported_data_formats.size = usize::MAX / 2;
        expect_protocol(&raw, "supported_data_formats span length is too large");

        let faces = [face(1, &[], &[], &[])];
        let mut raw = info(&[], &faces);
        raw.faces.size = usize::MAX / 8;
        expect_protocol(&raw, "faces span length is too large");

        let one_range = [range(1.0, 2.0)];
        let mut raw_face = face(11, &one_range, &[], &[]);
        raw_face.rx_frequency_ranges.size = usize::MAX / 4;
        let faces = [raw_face];
        expect_protocol(
            &info(&[], &faces),
            "face 11 rx_frequency_ranges span length is too large",
        );
    }

    #[test]
    fn booleans_accept_exactly_zero_or_one() {
        for invalid in [2, 77, u32::MAX] {
            let mut raw = info(&[], &[]);
            raw.contains_open_additions = invalid;
            expect_protocol(
                &raw,
                &format!("contains_open_additions has invalid boolean value {invalid}"),
            );

            for field in 0..3 {
                let mut raw_face = face(8, &[], &[], &[]);
                let name = match field {
                    0 => {
                        raw_face.supports_receive = invalid;
                        "face 8 supports_receive"
                    }
                    1 => {
                        raw_face.supports_transmit = invalid;
                        "face 8 supports_transmit"
                    }
                    _ => {
                        raw_face.requires_endpoint_association = invalid;
                        "face 8 requires_endpoint_association"
                    }
                };
                let faces = [raw_face];
                expect_protocol(
                    &info(&[], &faces),
                    &format!("{name} has invalid boolean value {invalid}"),
                );
            }
        }
        let mut raw_face = face(8, &[], &[], &[]);
        raw_face.supports_receive = 0;
        raw_face.supports_transmit = 1;
        raw_face.requires_endpoint_association = 0;
        let faces = [raw_face];
        let mut raw = info(&[], &faces);
        raw.contains_open_additions = 1;
        let value = mfa_info_from_raw(&raw).expect("valid booleans");
        assert!(value.contains_open_additions);
        let face = &value.faces[0];
        assert!(!face.supports_receive && face.supports_transmit);
        assert!(!face.requires_endpoint_association);
    }

    #[test]
    fn a_malformed_later_face_discards_the_whole_snapshot() {
        let good = [range(1.0, 2.0)];
        let mut bad = face(99, &[], &[], &[]);
        bad.tx_frequency_ranges = null_ranges(1);
        let faces = [face(1, &good, &good, &good), bad];
        expect_protocol(
            &info(&[0, 1], &faces),
            "face 99 tx_frequency_ranges span has a null pointer",
        );
    }

    #[test]
    fn owned_snapshot_types_carry_no_native_pointer() {
        fn owned<T: Send + Sync + 'static>() {}
        owned::<RfMfaInfo>();
        owned::<RfFaceInfo>();
        owned::<RfFrequencyRange>();
        owned::<RfJobDataFormat>();
    }

    /// Scripted native `get_provider_version` behaviour for the shared
    /// two-stage protocol. The first call reports the required sizes; the
    /// second copies `vendor`/`description` bytes (followed by a NUL when
    /// room remains) and may report different sizes.
    struct FakeVersion {
        first_status: i32,
        vendor_required: usize,
        description_required: usize,
        second_status: i32,
        second_vendor_required: usize,
        vendor: &'static [u8],
        description: &'static [u8],
        capacities: Vec<(usize, usize)>,
        calls: u32,
    }

    impl FakeVersion {
        fn exact(vendor: &'static [u8], description: &'static [u8]) -> Self {
            Self {
                first_status: sys::AMS_MEL_BUFFER_TOO_SMALL,
                vendor_required: vendor.len() + 1,
                description_required: description.len() + 1,
                second_status: sys::AMS_MEL_OK,
                second_vendor_required: vendor.len() + 1,
                vendor,
                description,
                capacities: Vec::new(),
                calls: 0,
            }
        }

        fn run(&mut self) -> Result<ProviderVersion, Error> {
            query_provider_version(|version, diagnostic, capacity, required| {
                self.calls += 1;
                // SAFETY: the helper passes a valid writable record and
                // diagnostic storage of `capacity` bytes; on the second call
                // the string buffers have exactly the advertised capacities.
                unsafe {
                    assert!(capacity >= 1);
                    *diagnostic = 0;
                    *required = 1;
                    let record = &mut *version;
                    if self.calls == 1 {
                        assert!(record.vendor.is_null() && record.vendor_capacity == 0);
                        assert!(record.description.is_null() && record.description_capacity == 0);
                        record.vendor_required = self.vendor_required;
                        record.description_required = self.description_required;
                        return self.first_status;
                    }
                    self.capacities
                        .push((record.vendor_capacity, record.description_capacity));
                    if self.second_status != sys::AMS_MEL_OK {
                        return self.second_status;
                    }
                    for (target, capacity, bytes) in [
                        (record.vendor, record.vendor_capacity, self.vendor),
                        (
                            record.description,
                            record.description_capacity,
                            self.description,
                        ),
                    ] {
                        let target = target.cast::<u8>();
                        let count = bytes.len().min(capacity);
                        ptr::copy_nonoverlapping(bytes.as_ptr(), target, count);
                        if count < capacity {
                            *target.add(count) = 0;
                        }
                    }
                    record.api_version = 0xDEAD_BEEF;
                    record.library_version = 0x0102_0304;
                    record.vendor_required = self.second_vendor_required;
                    sys::AMS_MEL_OK
                }
            })
        }
    }

    #[test]
    fn provider_version_allocates_exactly_the_reported_sizes() {
        let mut fake = FakeVersion::exact("µ-vendor".as_bytes(), "📡 description".as_bytes());
        let version = fake.run().expect("exact sizes");
        assert_eq!(fake.calls, 2);
        // Capacity is exactly the required size, which includes the NUL.
        assert_eq!(
            fake.capacities,
            [("µ-vendor".len() + 1, "📡 description".len() + 1)]
        );
        assert_eq!(version.vendor, "µ-vendor");
        assert_eq!(version.description, "📡 description");
        assert_eq!(
            (version.api_version, version.library_version),
            (0xDEAD_BEEF, 0x0102_0304)
        );

        // Minimal boundary: required == 1 is the empty string.
        let mut fake = FakeVersion::exact(b"", b"");
        let version = fake.run().expect("empty strings");
        assert_eq!(fake.capacities, [(1, 1)]);
        assert!(version.vendor.is_empty() && version.description.is_empty());

        // Long strings are never truncated to a fixed buffer.
        static LONG: [u8; 70_000] = [b'x'; 70_000];
        let mut fake = FakeVersion::exact(&LONG, b"d");
        let version = fake.run().expect("long vendor");
        assert_eq!(fake.capacities, [(70_001, 2)]);
        assert_eq!(version.vendor.len(), 70_000);
    }

    #[test]
    fn provider_version_rejects_malformed_native_output() {
        let check = |mut fake: FakeVersion, kind: ErrorKind, needle: &str| {
            let error = fake.run().expect_err(needle);
            assert_eq!(error.kind(), &kind, "{needle}");
            assert!(
                error.diagnostic().is_some_and(|text| text.contains(needle)),
                "{needle}: {:?}",
                error.diagnostic()
            );
        };
        let mut fake = FakeVersion::exact(b"v", b"d");
        fake.first_status = sys::AMS_MEL_OK;
        check(
            fake,
            ErrorKind::ProtocolInconsistency,
            "unexpectedly succeeded",
        );

        let mut fake = FakeVersion::exact(b"v", b"d");
        fake.vendor_required = 0;
        check(fake, ErrorKind::ProtocolInconsistency, "zero vendor size");

        let mut fake = FakeVersion::exact(b"v", b"d");
        fake.description_required = 0;
        check(
            fake,
            ErrorKind::ProtocolInconsistency,
            "zero description size",
        );

        let mut fake = FakeVersion::exact(b"v", b"d");
        fake.second_vendor_required = 3;
        check(fake, ErrorKind::ProtocolInconsistency, "sizes changed");

        // One byte too long: the terminal NUL slot holds data.
        let mut fake = FakeVersion::exact(b"vendor", b"d");
        fake.vendor = b"vendorX";
        check(
            fake,
            ErrorKind::ProtocolInconsistency,
            "unterminated vendor",
        );

        // Shorter than reported: a NUL inside the reported length.
        let mut fake = FakeVersion::exact(b"vendor", b"description");
        fake.description = b"desc";
        check(
            fake,
            ErrorKind::ProtocolInconsistency,
            "embedded NUL in description",
        );

        check(
            FakeVersion::exact(b"bad\xC3\x28", b"d"),
            ErrorKind::InvalidUtf8,
            "invalid UTF-8 in vendor",
        );
    }

    #[test]
    fn provider_version_preserves_native_failure_categories() {
        for (status, kind) in [
            (
                sys::AMS_MEL_PROVIDER_EXCEPTION,
                ErrorKind::ProviderException,
            ),
            (sys::AMS_MEL_INTERNAL_ERROR, ErrorKind::InternalError),
            (sys::AMS_MEL_INVALID_ARGUMENT, ErrorKind::InvalidArgument),
        ] {
            let mut fake = FakeVersion::exact(b"v", b"d");
            fake.first_status = status;
            assert_eq!(fake.run().expect_err("first").kind(), &kind);
            assert_eq!(fake.calls, 1);

            let mut fake = FakeVersion::exact(b"v", b"d");
            fake.second_status = status;
            assert_eq!(fake.run().expect_err("second").kind(), &kind);
            assert_eq!(fake.calls, 2);
        }
        // A second-stage BUFFER_TOO_SMALL is reported, never retried.
        let mut fake = FakeVersion::exact(b"v", b"d");
        fake.second_status = sys::AMS_MEL_BUFFER_TOO_SMALL;
        assert_eq!(
            fake.run().expect_err("second too small").kind(),
            &ErrorKind::BufferTooSmall
        );
        assert_eq!(fake.calls, 2);
    }

    fn mock_rf_provider() -> PathBuf {
        let repository = Path::new(env!("CARGO_MANIFEST_DIR")).join("../..");
        env::var_os("AMS_MEL_TEST_PROVIDER_DIR")
            .map(PathBuf::from)
            .unwrap_or_else(|| repository.join("native/build-tests/test-providers"))
            .join("libmock_rf_provider.so")
    }

    fn closes() -> u32 {
        NATIVE_SNAPSHOT_CLOSES.with(|count| count.get())
    }

    /// Exercises the private RAII guard against a live native snapshot from
    /// the existing mock RF provider: the native snapshot is closed exactly
    /// once on success, on conversion failure, and on unwind, and the RF
    /// owner is unaffected. The counter is thread-local, so concurrently
    /// running unit tests cannot perturb it.
    #[test]
    fn snapshot_guard_closes_native_snapshot_exactly_once_on_every_path() {
        let mut data = RfData::open(mock_rf_provider(), "success").expect("open mock RF");
        let live = data.live().expect("live owner");

        let before = closes();
        let faces = snapshot_with(live, |raw| Ok(raw.faces.size)).expect("snapshot");
        assert_eq!(faces, 2);
        assert_eq!(closes(), before + 1);

        let error = snapshot_with(live, |raw| -> Result<(), Error> {
            assert_eq!(raw.faces.size, 2);
            Err(protocol("injected conversion failure"))
        })
        .expect_err("conversion failure");
        assert_eq!(error.kind(), &ErrorKind::ProtocolInconsistency);
        assert_eq!(error.diagnostic(), Some("injected conversion failure"));
        assert_eq!(closes(), before + 2);

        let unwound = panic::catch_unwind(AssertUnwindSafe(|| {
            let _ = snapshot_with(live, |_| -> Result<(), Error> { panic!("injected unwind") });
        }));
        assert!(unwound.is_err());
        assert_eq!(closes(), before + 3);

        // A null guard never calls native Close.
        drop(MfaInfoOwner {
            raw: ptr::null_mut(),
        });
        assert_eq!(closes(), before + 3);

        let value = data
            .mfa_info()
            .expect("public snapshot after guard cleanup");
        assert_eq!(closes(), before + 4);
        data.close().expect("close");
        assert_eq!(value.faces.len(), 2);
        assert_eq!(closes(), before + 4);
    }
}
