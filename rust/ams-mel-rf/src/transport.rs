//! Private RF-only ABI transport helpers preserving existing status and error contracts.
//!
//! Kept separate from ams-mel-common so Common owns no native dependency.
//! Only ams-mel-sys emits the native library linkage.

use std::ffi::{c_char, CString};
use std::mem;
use std::path::Path;
use std::ptr;
use std::slice;

use ams_mel_sys as sys;
use crate::{Error, ErrorKind, ProviderVersion};

const DIAGNOSTIC_CAPACITY: usize = 4096;

fn empty_version() -> sys::AmsMelProviderVersionV1 {
    sys::AmsMelProviderVersionV1 {
        api_version: 0,
        library_version: 0,
        vendor: ptr::null_mut(),
        vendor_capacity: 0,
        vendor_required: 0,
        description: ptr::null_mut(),
        description_capacity: 0,
        description_required: 0,
    }
}

/// Two-stage `ams_mel_provider_version_v1` protocol used by RF `RfData`.
/// Matches the IR Session transport contract without accessing the IR crate.
///
/// The first `query` passes null/zero string buffers and must report
/// `AMS_MEL_BUFFER_TOO_SMALL` with nonzero `*_required` counts (each including
/// the terminal NUL). Buffers of exactly those sizes are then allocated
/// fallibly, and the second `query` must succeed with unchanged counts. Each
/// copied buffer must hold exactly `required - 1` bytes of NUL-free UTF-8 and
/// a terminal NUL. Every other native status keeps its category and complete
/// diagnostic; malformed output is `ProtocolInconsistency` or `InvalidUtf8`.
pub(crate) fn query_provider_version(
    mut query: impl FnMut(*mut sys::AmsMelProviderVersionV1, *mut c_char, usize, *mut usize) -> i32,
) -> Result<ProviderVersion, Error> {
    let mut raw = empty_version();
    let first = call_with_diagnostic(|buffer, capacity, required| {
        query(&mut raw, buffer, capacity, required)
    });
    match first {
        Err(error) if error.kind() == &ErrorKind::BufferTooSmall => {}
        Err(error) => return Err(error),
        Ok(()) => {
            return Err(Error::new(
                ErrorKind::ProtocolInconsistency,
                "provider version size query unexpectedly succeeded",
            ))
        }
    }
    validate_required(raw.vendor_required, "vendor")?;
    validate_required(raw.description_required, "description")?;

    let expected_vendor = raw.vendor_required;
    let expected_description = raw.description_required;
    let mut vendor = zeroed_buffer(expected_vendor, "vendor")?;
    let mut description = zeroed_buffer(expected_description, "description")?;
    raw.vendor = vendor.as_mut_ptr().cast::<c_char>();
    raw.vendor_capacity = vendor.len();
    raw.description = description.as_mut_ptr().cast::<c_char>();
    raw.description_capacity = description.len();

    call_with_diagnostic(|buffer, capacity, required| {
        // Both output vectors stay allocated with the advertised capacities
        // for the duration of this call.
        query(&mut raw, buffer, capacity, required)
    })?;
    if raw.vendor_required != expected_vendor || raw.description_required != expected_description {
        return Err(Error::new(
            ErrorKind::ProtocolInconsistency,
            "provider version sizes changed between query and copy",
        ));
    }

    Ok(ProviderVersion {
        api_version: raw.api_version,
        library_version: raw.library_version,
        vendor: decode_c_buffer(&vendor, "vendor")?,
        description: decode_c_buffer(&description, "description")?,
    })
}

/// Fallibly allocates exactly `size` zeroed bytes.
fn zeroed_buffer(size: usize, field: &str) -> Result<Vec<u8>, Error> {
    let mut buffer = reserve_owned(size, field)?;
    buffer.resize(size, 0);
    Ok(buffer)
}

/// Borrows a checked C span as a Rust slice. This is the only place a native
/// capability span or string view becomes a slice.
///
/// `size == 0` is empty whether `data` is null or not. `size > 0` requires a
/// non-null, aligned pointer and a length within the Rust slice bound
/// (`isize::MAX / size_of::<T>()`). Malformed spans are
/// `ProtocolInconsistency`.
///
/// # Safety
///
/// For a non-empty, well-formed span, `data` must point to `size` initialized
/// `T` values that remain valid and unmodified for `'a`.
pub(crate) unsafe fn checked_span<'a, T>(data: *const T, size: usize, field: &str) -> Result<&'a [T], Error> {
    if size == 0 {
        return Ok(&[]);
    }
    if data.is_null() {
        return Err(protocol(&format!(
            "native {field} span has a null pointer with nonzero size"
        )));
    }
    if !(data as usize).is_multiple_of(mem::align_of::<T>()) {
        return Err(protocol(&format!("native {field} span is misaligned")));
    }
    if size > isize::MAX as usize / mem::size_of::<T>().max(1) {
        return Err(protocol(&format!(
            "native {field} span length is too large"
        )));
    }
    // SAFETY: non-null, aligned, and within the Rust slice size bound; the
    // caller guarantees `size` initialized elements live for `'a`.
    Ok(unsafe { slice::from_raw_parts(data, size) })
}

fn reserve_owned<U>(size: usize, field: &str) -> Result<Vec<U>, Error> {
    let mut values = Vec::new();
    values.try_reserve_exact(size).map_err(|_| {
        Error::new(
            ErrorKind::InternalError,
            format!("unable to allocate owned {field} storage"),
        )
    })?;
    Ok(values)
}

pub(crate) fn copy_mapped<T, U>(
    values: &[T],
    field: &str,
    mut map: impl FnMut(&T) -> Result<U, Error>,
) -> Result<Vec<U>, Error> {
    let mut owned = reserve_owned(values.len(), field)?;
    for value in values {
        owned.push(map(value)?);
    }
    Ok(owned)
}

pub(crate) fn checked_bool(value: u32, field: &str) -> Result<bool, Error> {
    match value {
        0 => Ok(false),
        1 => Ok(true),
        other => Err(protocol(&format!(
            "native {field} has invalid boolean value {other}"
        ))),
    }
}

pub(crate) fn owned_u32_mapped<U>(
    span: sys::AmsMelU32SpanV1,
    field: &str,
    map: impl Fn(u32) -> U,
) -> Result<Vec<U>, Error> {
    // SAFETY: the span belongs to a native record kept alive by the caller.
    let values = unsafe { checked_span(span.data, span.size, field)? };
    copy_mapped(values, field, |value| Ok(map(*value)))
}

pub(crate) fn protocol(message: &str) -> Error {
    Error::new(ErrorKind::ProtocolInconsistency, message)
}

pub(crate) fn c_path(path: &Path) -> Result<CString, Error> {
    let path = path.to_str().ok_or_else(|| {
        Error::new(
            ErrorKind::InvalidArgument,
            "provider_library must be valid UTF-8",
        )
    })?;
    c_string(path, "provider_library")
}

pub(crate) fn c_string(value: &str, name: &str) -> Result<CString, Error> {
    CString::new(value).map_err(|_| Error::nul(name))
}

fn validate_required(required: usize, field: &str) -> Result<(), Error> {
    if required == 0 {
        Err(Error::new(
            ErrorKind::ProtocolInconsistency,
            format!("native provider returned zero {field} size"),
        ))
    } else {
        Ok(())
    }
}

fn decode_c_buffer(buffer: &[u8], field: &str) -> Result<String, Error> {
    let Some((&0, bytes)) = buffer.split_last() else {
        return Err(Error::new(
            ErrorKind::ProtocolInconsistency,
            format!("native provider returned unterminated {field}"),
        ));
    };
    if bytes.contains(&0) {
        return Err(Error::new(
            ErrorKind::ProtocolInconsistency,
            format!("native provider returned embedded NUL in {field}"),
        ));
    }
    String::from_utf8(bytes.to_vec()).map_err(|_| {
        Error::new(
            ErrorKind::InvalidUtf8,
            format!("native provider returned invalid UTF-8 in {field}"),
        )
    })
}

pub(crate) fn call_with_diagnostic(
    call: impl FnOnce(*mut c_char, usize, *mut usize) -> i32,
) -> Result<(), Error> {
    let mut diagnostic = [0_u8; DIAGNOSTIC_CAPACITY];
    let mut required = 0_usize;
    let status = call(
        diagnostic.as_mut_ptr().cast::<c_char>(),
        diagnostic.len(),
        &mut required,
    );
    if required == 0 {
        return Err(Error::new(
            ErrorKind::ProtocolInconsistency,
            format!("native diagnostic requires invalid capacity {required}"),
        ));
    }
    if required > diagnostic.len() {
        if status == sys::AMS_MEL_OK {
            return Err(Error::new(
                ErrorKind::ProtocolInconsistency,
                "successful native call returned an oversized diagnostic",
            ));
        }
        return Err(error_from_status(status, None, Some(required)));
    }
    let message = decode_diagnostic(&diagnostic[..required])?;
    if status == sys::AMS_MEL_OK {
        if required != 1 || !message.is_empty() {
            return Err(Error::new(
                ErrorKind::ProtocolInconsistency,
                "successful native call returned a diagnostic",
            ));
        }
        Ok(())
    } else {
        Err(error_from_status(status, Some(message), Some(required)))
    }
}

fn decode_diagnostic(buffer: &[u8]) -> Result<String, Error> {
    decode_c_buffer(buffer, "diagnostic")
}

fn error_from_status(
    status: i32,
    diagnostic: Option<String>,
    diagnostic_required: Option<usize>,
) -> Error {
    let kind = match status {
        sys::AMS_MEL_INVALID_ARGUMENT => ErrorKind::InvalidArgument,
        sys::AMS_MEL_LIBRARY_LOAD_FAILED => ErrorKind::LibraryLoadFailed,
        sys::AMS_MEL_SYMBOL_NOT_FOUND => ErrorKind::SymbolNotFound,
        sys::AMS_MEL_FACTORY_FAILED => ErrorKind::FactoryFailed,
        sys::AMS_MEL_INITIALIZATION_FAILED => ErrorKind::InitializationFailed,
        sys::AMS_MEL_PROVIDER_EXCEPTION => ErrorKind::ProviderException,
        sys::AMS_MEL_BUFFER_TOO_SMALL => ErrorKind::BufferTooSmall,
        sys::AMS_MEL_PROVIDER_FAILED => ErrorKind::ProviderFailed,
        sys::AMS_MEL_INTERNAL_ERROR => ErrorKind::InternalError,
        sys::AMS_MEL_TIMEOUT => ErrorKind::Timeout,
        sys::AMS_MEL_STREAM_STOPPED => ErrorKind::StreamStopped,
        sys::AMS_MEL_RESOURCE_EXHAUSTED => ErrorKind::ResourceExhausted,
        unknown => ErrorKind::Unknown(unknown),
    };
    Error::from_native_parts(kind, diagnostic, diagnostic_required)
}

