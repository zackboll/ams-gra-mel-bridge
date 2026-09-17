//! Safe Rust foundation for provider sessions through `ams_mel_c`.
//!
//! [`Session`] is intentionally neither `Send` nor `Sync`: the current MEL
//! contract does not establish arbitrary cross-thread session use.

use std::error;
use std::ffi::{c_char, CString};
use std::fmt;
use std::path::Path;
use std::ptr;
use std::rc::Rc;

use ams_mel_sys as sys;

const DIAGNOSTIC_CAPACITY: usize = 4096;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct AbiVersion {
    pub major: u32,
    pub minor: u32,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ProviderVersion {
    pub api_version: u32,
    pub library_version: u32,
    pub vendor: String,
    pub description: String,
}

#[derive(Clone, Debug, Eq, PartialEq)]
#[non_exhaustive]
pub enum ErrorKind {
    InvalidArgument,
    LibraryLoadFailed,
    SymbolNotFound,
    FactoryFailed,
    InitializationFailed,
    ProviderException,
    BufferTooSmall,
    ProviderFailed,
    InternalError,
    InvalidUtf8,
    ProtocolInconsistency,
    Unknown(i32),
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Error {
    kind: ErrorKind,
    diagnostic: Option<String>,
    diagnostic_required: Option<usize>,
}

impl Error {
    pub fn kind(&self) -> &ErrorKind {
        &self.kind
    }

    pub fn diagnostic(&self) -> Option<&str> {
        self.diagnostic.as_deref()
    }

    /// Returns the native diagnostic capacity required, including its trailing
    /// NUL. A value larger than the binding's capture capacity means that
    /// [`diagnostic`](Self::diagnostic) is `None`, because a partial diagnostic
    /// is never exposed as complete.
    pub fn diagnostic_required(&self) -> Option<usize> {
        self.diagnostic_required
    }

    fn new(kind: ErrorKind, diagnostic: impl Into<String>) -> Self {
        let diagnostic = diagnostic.into();
        Self {
            kind,
            diagnostic: (!diagnostic.is_empty()).then_some(diagnostic),
            diagnostic_required: None,
        }
    }

    fn nul(name: &str) -> Self {
        Self::new(
            ErrorKind::InvalidArgument,
            format!("{name} contains an embedded NUL"),
        )
    }
}

impl fmt::Display for Error {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        match &self.diagnostic {
            Some(message) => write!(formatter, "{:?}: {message}", self.kind),
            None => write!(formatter, "{:?}", self.kind),
        }
    }
}

impl error::Error for Error {}

pub fn abi_version() -> Result<AbiVersion, Error> {
    let mut raw = sys::AmsMelAbiVersionV1::default();
    // SAFETY: `raw` is valid writable storage for the exact C record.
    let status = unsafe { sys::ams_mel_get_abi_version(&mut raw) };
    if status == sys::AMS_MEL_OK {
        Ok(AbiVersion {
            major: raw.major,
            minor: raw.minor,
        })
    } else {
        Err(error_from_status(status, Some(String::new()), None))
    }
}

#[derive(Debug)]
pub struct Session {
    raw: *mut sys::AmsMelSession,
    _not_send_sync: Rc<()>,
}

impl Session {
    pub fn open(
        provider_library: impl AsRef<Path>,
        instance: impl AsRef<str>,
        aperture: impl AsRef<str>,
    ) -> Result<Self, Error> {
        let library = c_path(provider_library.as_ref())?;
        let instance = c_string(instance.as_ref(), "instance")?;
        let aperture = c_string(aperture.as_ref(), "aperture")?;
        let mut raw = ptr::null_mut();

        let status = call_with_diagnostic(|buffer, capacity, required| {
            // SAFETY: all C strings live through the call; `raw` is an initially
            // null writable owner and diagnostic storage matches its capacity.
            unsafe {
                sys::ams_mel_session_open(
                    library.as_ptr(),
                    instance.as_ptr(),
                    aperture.as_ptr(),
                    &mut raw,
                    buffer,
                    capacity,
                    required,
                )
            }
        });
        match status {
            Ok(()) if raw.is_null() => Err(Error::new(
                ErrorKind::ProtocolInconsistency,
                "session open succeeded without returning an owner",
            )),
            Ok(()) => Ok(Self {
                raw,
                _not_send_sync: Rc::new(()),
            }),
            Err(error) => {
                if !raw.is_null() {
                    // A malformed native implementation must still be given the
                    // opportunity to release an owner it published on failure.
                    best_effort_close(&mut raw);
                }
                Err(error)
            }
        }
    }

    pub fn provider_version(&self) -> Result<ProviderVersion, Error> {
        let mut raw = empty_version();
        let first = call_with_diagnostic(|buffer, capacity, required| {
            // SAFETY: the session owner is live and `raw` is writable. Null/zero
            // string buffers are the documented required-size query.
            unsafe {
                sys::ams_mel_session_get_provider_version(
                    self.raw, &mut raw, buffer, capacity, required,
                )
            }
        });
        match first {
            Err(error) if error.kind == ErrorKind::BufferTooSmall => {}
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

        let mut vendor = vec![0_u8; raw.vendor_required];
        let mut description = vec![0_u8; raw.description_required];
        raw.vendor = vendor.as_mut_ptr().cast::<c_char>();
        raw.vendor_capacity = vendor.len();
        raw.description = description.as_mut_ptr().cast::<c_char>();
        raw.description_capacity = description.len();
        let expected_vendor = raw.vendor_required;
        let expected_description = raw.description_required;

        call_with_diagnostic(|buffer, capacity, required| {
            // SAFETY: both output vectors remain allocated with the advertised
            // capacities, and the live session and record pointers are valid.
            unsafe {
                sys::ams_mel_session_get_provider_version(
                    self.raw, &mut raw, buffer, capacity, required,
                )
            }
        })?;
        if raw.vendor_required != expected_vendor
            || raw.description_required != expected_description
        {
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

    pub fn close(mut self) -> Result<(), Error> {
        self.close_inner()
    }

    fn close_inner(&mut self) -> Result<(), Error> {
        call_with_diagnostic(|buffer, capacity, required| {
            // SAFETY: `raw` is this Session's unique native owner. The facade
            // clears it when ownership is released.
            unsafe { sys::ams_mel_session_close(&mut self.raw, buffer, capacity, required) }
        })
    }
}

impl Drop for Session {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            best_effort_close(&mut self.raw);
        }
    }
}

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

fn c_path(path: &Path) -> Result<CString, Error> {
    let path = path.to_str().ok_or_else(|| {
        Error::new(
            ErrorKind::InvalidArgument,
            "provider_library must be valid UTF-8",
        )
    })?;
    c_string(path, "provider_library")
}

fn c_string(value: &str, name: &str) -> Result<CString, Error> {
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

fn call_with_diagnostic(
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
        unknown => ErrorKind::Unknown(unknown),
    };
    Error {
        kind,
        diagnostic: diagnostic.filter(|message| !message.is_empty()),
        diagnostic_required,
    }
}

fn best_effort_close(raw: &mut *mut sys::AmsMelSession) {
    // SAFETY: called only for this wrapper's unique owner. Null diagnostics are
    // explicitly supported. The result is ignored because Drop cannot report it.
    let _ = unsafe { sys::ams_mel_session_close(raw, ptr::null_mut(), 0, ptr::null_mut()) };
}
