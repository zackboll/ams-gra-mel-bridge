//! Shared provider-independent Common MEL Rust values for IR and RF.
//!
//! Owns shared versions, errors, UCI identities and component locations.
//! This crate does not load providers, link native libraries or own channels.
#![forbid(unsafe_code)]

use std::error;
use std::fmt;

/// Version of the consumer C façade, obtained without loading a provider.
///
/// This is not the Rust crate version or [`ProviderVersion`].
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct AbiVersion {
    /// C ABI major version.
    pub major: u32,
    /// C ABI minor version.
    pub minor: u32,
}

/// Owned snapshot of a provider's published version information.
///
/// Numeric versions are provider values, not Rust semantic versions. Discovery
/// does not certify C++ ABI compatibility or support for every MEL operation.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ProviderVersion {
    /// Provider-reported MEL API version, preserved as an unsigned 32-bit value.
    pub api_version: u32,
    /// Provider-reported library version, without an inferred encoding.
    pub library_version: u32,
    /// Complete UTF-8 vendor name.
    pub vendor: String,
    /// Complete UTF-8 provider description.
    pub description: String,
}

/// Binding or native façade failure category; inspect [`Error`] for diagnostics.
///
/// Explicit MEL command rejection is instead a terminal result such as
/// `ModeResult::Rejected`. Match with a fallback because this enum can grow.
#[derive(Clone, Debug, Eq, PartialEq)]
#[non_exhaustive]
pub enum ErrorKind {
    /// An argument violates the binding or native contract.
    InvalidArgument,
    /// The provider shared library could not be loaded.
    LibraryLoadFailed,
    /// A required provider factory symbol was absent.
    SymbolNotFound,
    /// A provider factory did not produce the required object.
    FactoryFailed,
    /// Initialization or attachment requirements were not met.
    InitializationFailed,
    /// The native boundary caught a provider exception.
    ProviderException,
    /// Native output storage was too small; not an asynchronous rejection.
    BufferTooSmall,
    /// A provider operation or owner lifecycle failed.
    ProviderFailed,
    /// An internal bridge/binding failure, including reported allocation failure.
    InternalError,
    /// No result arrived within the wait interval; provider work is not cancelled.
    Timeout,
    /// Reception has stopped and no queued value remains.
    StreamStopped,
    /// An admission/resource limit refused work; no implicit queue or retry.
    ResourceExhausted,
    /// Text returned across the boundary was not valid UTF-8.
    InvalidUtf8,
    /// Native output contradicted the protocol expected by this binding.
    ProtocolInconsistency,
    /// An unrecognized native status, preserving its signed 32-bit value.
    Unknown(i32),
}

/// Owned identity: all 16 UUID bytes plus a NUL-free UTF-8 descriptive label.
///
/// Empty labels and arbitrary UUID bytes are accepted without normalization or
/// UUID-version validation. The label is not substituted for the UUID.
///
/// ```
/// use ams_mel_common::UciId;
/// let id = UciId::new([0x42; 16], "sensor µ")?;
/// assert_eq!(id.uuid(), &[0x42; 16]);
/// assert_eq!(id.descriptive_label(), "sensor µ");
/// # Ok::<(), ams_mel_common::Error>(())
/// ```
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct UciId {
    uuid: [u8; 16],
    descriptive_label: String,
}

impl UciId {
    /// Retains the UUID and label verbatim.
    ///
    /// # Errors
    /// Returns [`ErrorKind::InvalidArgument`] if the label contains a NUL.
    pub fn new(uuid: [u8; 16], descriptive_label: impl Into<String>) -> Result<Self, Error> {
        let descriptive_label = descriptive_label.into();
        reject_nul(&descriptive_label, "descriptive_label")?;
        Ok(Self {
            uuid,
            descriptive_label,
        })
    }

    /// Borrows the original UUID bytes, without text or byte-order conversion.
    pub fn uuid(&self) -> &[u8; 16] {
        &self.uuid
    }

    /// Borrows the complete original label.
    pub fn descriptive_label(&self) -> &str {
        &self.descriptive_label
    }
}

/// Owned component offsets in metres and provider-defined reference text.
///
/// Offsets are retained as `f64`, without finite/range checks or coordinate
/// conversion. Empty key/system names are valid; neither establishes a frame
/// convention beyond the provider's published configuration.
#[derive(Clone, Debug, PartialEq)]
pub struct ComponentLocation {
    offset_x_m: f64,
    offset_y_m: f64,
    offset_z_m: f64,
    key: String,
    system_name: String,
}

impl ComponentLocation {
    /// Constructs a location without physical validation of the metre offsets.
    ///
    /// # Errors
    /// Returns [`ErrorKind::InvalidArgument`] if either text argument contains NUL.
    pub fn new(
        offset_x_m: f64,
        offset_y_m: f64,
        offset_z_m: f64,
        key: impl Into<String>,
        system_name: impl Into<String>,
    ) -> Result<Self, Error> {
        let key = key.into();
        let system_name = system_name.into();
        reject_nul(&key, "key")?;
        reject_nul(&system_name, "system_name")?;
        Ok(Self {
            offset_x_m,
            offset_y_m,
            offset_z_m,
            key,
            system_name,
        })
    }

    /// Returns the X offset in metres.
    pub fn offset_x_m(&self) -> f64 {
        self.offset_x_m
    }

    /// Returns the Y offset in metres.
    pub fn offset_y_m(&self) -> f64 {
        self.offset_y_m
    }

    /// Returns the Z offset in metres.
    pub fn offset_z_m(&self) -> f64 {
        self.offset_z_m
    }

    /// Borrows the provider-defined location key.
    pub fn key(&self) -> &str {
        &self.key
    }

    /// Borrows the provider-defined system name.
    pub fn system_name(&self) -> &str {
        &self.system_name
    }
}

/// Owned failure with a stable category and optional complete UTF-8 diagnostic.
///
/// [`Self::kind`] is suitable for decisions; diagnostic text is explanatory,
/// not a machine-readable protocol. A diagnostic that exceeds capture capacity
/// is omitted rather than exposed partially; inspect [`Self::diagnostic_required`].
/// Command rejection is normally represented by a request result, not this type.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Error {
    kind: ErrorKind,
    diagnostic: Option<String>,
    diagnostic_required: Option<usize>,
}

impl Error {
    /// Returns the failure category, preserving unknown native status values.
    pub fn kind(&self) -> &ErrorKind {
        &self.kind
    }

    /// Borrows a complete captured diagnostic, if available.
    /// `None` does not imply success or the absence of a native diagnostic.
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

    /// Internal constructor used by binding transport and validation.
    #[doc(hidden)]
    pub fn new(kind: ErrorKind, diagnostic: impl Into<String>) -> Self {
        let diagnostic = diagnostic.into();
        Self {
            kind,
            diagnostic: (!diagnostic.is_empty()).then_some(diagnostic),
            diagnostic_required: None,
        }
    }

    /// Internal NUL-error construction used by binding validation.
    #[doc(hidden)]
    pub fn nul(name: &str) -> Self {
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


impl Error {
    /// Internal transport conversion preserving complete native diagnostic metadata.
    #[doc(hidden)]
    pub fn from_native_parts(
        kind: ErrorKind,
        diagnostic: Option<String>,
        diagnostic_required: Option<usize>,
    ) -> Self {
        Self {
            kind,
            diagnostic: diagnostic.filter(|message| !message.is_empty()),
            diagnostic_required,
        }
    }
}

fn reject_nul(value: &str, name: &str) -> Result<(), Error> {
    if value.as_bytes().contains(&0) {
        Err(Error::nul(name))
    } else {
        Ok(())
    }
}
