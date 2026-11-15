//! Safe RF MEL Admin and DataMEL provider bindings.
//!
//! RF Admin and DataMEL own independent native objects; they are distinct from
//! the IR Session. Native command acceptance is not proof of physical execution.
//! RFMFAInfo snapshots are Rust-owned. This crate does not implement RF C2,
//! ProductRx, Virtual Aperture, Job, transmit, waveform or DSP APIs.

pub use ams_mel_common::{Error, ErrorKind, ProviderVersion};

mod rf_admin;
mod rf_data;
mod transport;

pub use rf_admin::{RfAdmin, RfMfaState};
pub use rf_data::{RfData, RfFaceInfo, RfFrequencyRange, RfJobDataFormat, RfMfaInfo};

pub(crate) use transport::{
    c_path, c_string, call_with_diagnostic, checked_bool, checked_span, copy_mapped,
    owned_u32_mapped, protocol, query_provider_version,
};
