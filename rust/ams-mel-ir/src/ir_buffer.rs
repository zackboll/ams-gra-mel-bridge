//! Standalone host storage. The native bridge is the trusted allocation and
//! provider-lifetime boundary; Rust never dereferences provider addresses.

use std::{ffi::c_char, fmt, marker::PhantomData, ptr, rc::Rc};

use ams_mel_sys as sys;

use crate::{call_with_diagnostic, protocol, Error, ErrorKind, Session};

/// Exact provider bookkeeping flag. GPU/RDMA do not allocate device memory.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[repr(u32)]
pub enum BufferFlag {
    /// Host bookkeeping (zero).
    Host = sys::AMS_MEL_IR_BUFFER_HOST,
    /// GPU bookkeeping (one), not GPU-backed allocation.
    Gpu = sys::AMS_MEL_IR_BUFFER_GPU,
    /// RDMA bookkeeping (two), not registered RDMA memory.
    Rdma = sys::AMS_MEL_IR_BUFFER_RDMA,
}

/// Classification of the image starting address only; never a readable span.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ImageLocation {
    /// Null provider image address.
    Null,
    /// Starting address lies inside owned host storage.
    InsideStorage,
    /// Non-null starting address lies outside owned host storage.
    OutsideStorage,
}

/// Independent owned snapshot; getter discrepancies are not normalized.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct BufferInfo {
    /// Original allocation capacity in bytes, not provider-reported size.
    pub storage_size: u64,
    /// Complete signed context passed to initialization.
    pub initialized_context: i64,
    /// Actual signed provider size, even if negative or different from capacity.
    pub provider_size: i64,
    /// Actual signed provider context, even if different from initialization.
    pub provider_context: i64,
    /// Whether the provider buffer address equals owned host storage.
    pub buffer_address_matches: bool,
    /// Image starting-address classification, without layout or length inference.
    pub image_location: ImageLocation,
}

/// Open failure, keeping bridge status and upstream initialization Return separate.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct BufferOpenError {
    /// Existing transport/validation error, including complete diagnostics.
    pub error: Error,
    /// Actual native status, or None if rejected before calling native code.
    pub native_status: Option<i32>,
    /// Actual numeric init Return; None means no Return was obtained.
    /// The ABI reserves UINT32_MAX for this absence.
    pub provider_return: Option<u32>,
}

impl fmt::Display for BufferOpenError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{} (init Return {:?})", self.error, self.provider_return)
    }
}

impl std::error::Error for BufferOpenError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        Some(&self.error)
    }
}

/// Flag query failure; a failed query is not an empty successful flag list.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct BufferFlagsError {
    /// Existing transport, allocation or conversion error.
    pub error: Error,
    /// Actual status from the most recent native query, if any.
    pub native_status: Option<i32>,
    /// Actual getFlags Return from that query; UINT32_MAX means absent.
    pub provider_return: Option<u32>,
}

impl fmt::Display for BufferFlagsError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{} (getFlags Return {:?})",
            self.error, self.provider_return
        )
    }
}

impl std::error::Error for BufferFlagsError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        Some(&self.error)
    }
}

/// Unique standalone provider Buffer with fixed, zero-initialized host storage.
///
/// The native owner retains the Session/provider graph independently of the
/// originating Session. No borrowed storage, native address, registration,
/// checkout or unrestricted upstream `Buffer::release()` is exposed. This is
/// not a delivered image [`crate::Frame`]. GPU/RDMA flags are bookkeeping only.
///
/// Prefer explicit retryable [`Self::close`]. Drop attempts ordinary cleanup
/// without panicking or allocating diagnostics. If another provider shared owner
/// prevents cleanup, the native graph remains retained, possibly until process
/// exit. Drop is not proof of cleanup. Access is single-threaded by construction.
///
/// ```compile_fail
/// fn clone_owner(buffer: &ams_mel_ir::HostBuffer) { let _: ams_mel_ir::HostBuffer = buffer.clone(); }
/// ```
/// ```compile_fail
/// fn send<T: Send>() {}
/// send::<ams_mel_ir::HostBuffer>();
/// ```
/// ```compile_fail
/// fn share<T: Sync>() {}
/// share::<ams_mel_ir::HostBuffer>();
/// ```
#[derive(Debug)]
pub struct HostBuffer {
    raw: *mut sys::AmsMelIrBuffer,
    storage_size: u64,
    _not_send_sync: PhantomData<Rc<()>>,
}

impl Session {
    /// Allocates fixed zero-filled host bytes and initializes one provider Buffer.
    /// The returned owner does not borrow this Session. Context is signed i64.
    ///
    /// # Errors
    /// Zero or sizes exceeding signed/native allocation bounds are invalid.
    /// Allocation, factory and init failures preserve native status/diagnostics
    /// and the separate init Return when available. Success requires a non-null
    /// owner and an actual Success Return. Unexpected failure outputs receive one
    /// conservative best-effort Close; uncertain native graphs remain retained.
    pub fn open_host_buffer(&self, size: u64, context: i64) -> Result<HostBuffer, BufferOpenError> {
        open_with(
            size,
            |out, result, d, c, r| {
                // SAFETY: self owns the Session (or null after closure); writable
                // initialized output slots and diagnostics live throughout the call.
                unsafe {
                    sys::ams_mel_ir_buffer_open(self.raw, size, context, out, result, d, c, r)
                }
            },
            native_close,
        )
    }
}

type Close =
    unsafe extern "C" fn(*mut *mut sys::AmsMelIrBuffer, *mut c_char, usize, *mut usize) -> i32;
const NATIVE_CLOSE: Close = sys::ams_mel_ir_buffer_close;

fn native_close(raw: &mut *mut sys::AmsMelIrBuffer) {
    // SAFETY: unique output owner from native Open; null diagnostics supported.
    // Failed cleanup retains the native allocation, never calls checkout release.
    let _ = unsafe { NATIVE_CLOSE(raw, ptr::null_mut(), 0, ptr::null_mut()) };
}

fn open_with(
    size: u64,
    open: impl FnOnce(&mut *mut sys::AmsMelIrBuffer, &mut u32, *mut c_char, usize, *mut usize) -> i32,
    cleanup: impl FnOnce(&mut *mut sys::AmsMelIrBuffer),
) -> Result<HostBuffer, BufferOpenError> {
    if size == 0 || size > isize::MAX as u64 || size > i64::MAX as u64 {
        return Err(BufferOpenError {
            error: Error::new(
                ErrorKind::InvalidArgument,
                "invalid host Buffer allocation size",
            ),
            native_status: None,
            provider_return: None,
        });
    }
    let mut raw = ptr::null_mut();
    let mut result = u32::MAX;
    let mut status = sys::AMS_MEL_OK;
    let checked = call_with_diagnostic(|d, c, r| {
        status = open(&mut raw, &mut result, d, c, r);
        status
    })
    .and_then(|()| {
        if raw.is_null() || result != 0 {
            Err(protocol(
                "Buffer Open success without valid owner and initialization Return",
            ))
        } else {
            Ok(())
        }
    });
    if let Err(error) = checked {
        if !raw.is_null() {
            cleanup(&mut raw);
        }
        return Err(BufferOpenError {
            error,
            native_status: Some(status),
            provider_return: returned(result),
        });
    }
    Ok(HostBuffer {
        raw,
        storage_size: size,
        _not_send_sync: PhantomData,
    })
}

fn returned(value: u32) -> Option<u32> {
    (value != u32::MAX).then_some(value)
}

fn allocated<T: Clone>(count: usize, initial: T) -> Result<Vec<T>, Error> {
    let mut values = Vec::new();
    values.try_reserve_exact(count).map_err(|_| {
        Error::new(
            ErrorKind::ResourceExhausted,
            "host Buffer copy allocation failed",
        )
    })?;
    values.resize(count, initial);
    Ok(values)
}

impl HostBuffer {
    /// Local owner presence, not a provider-health or cleanup guarantee.
    pub fn is_open(&self) -> bool {
        !self.raw.is_null()
    }

    fn live(&self) -> Result<*mut sys::AmsMelIrBuffer, Error> {
        if self.is_open() {
            Ok(self.raw)
        } else {
            Err(Error::new(
                ErrorKind::ProviderFailed,
                "host Buffer is closed",
            ))
        }
    }

    /// Copies the actual provider getters without dereferencing their addresses.
    ///
    /// # Errors
    /// Closed owner, native failure, or unknown image-location/noncanonical bool.
    pub fn info(&self) -> Result<BufferInfo, Error> {
        let raw = self.live()?;
        let mut info = sys::AmsMelIrBufferInfoV1::default();
        call_with_diagnostic(|d, c, r| {
            // SAFETY: live unique owner and writable exact ABI record.
            unsafe { sys::ams_mel_ir_buffer_get_info(raw, &mut info, d, c, r) }
        })?;
        info_from_raw(info)
    }

    fn span(&self, offset: u64, length: usize) -> Result<(), Error> {
        self.live()?;
        if offset > self.storage_size
            || u64::try_from(length).map_or(true, |n| n > self.storage_size - offset)
        {
            Err(Error::new(
                ErrorKind::InvalidArgument,
                "host Buffer span exceeds original storage",
            ))
        } else {
            Ok(())
        }
    }

    /// Copies an independent byte vector, never an image view. Empty end reads work.
    ///
    /// # Errors
    /// Closed owner, out-of-bounds span, fallible allocation or native failure.
    /// No bytes are returned on failure, including a partially written native output.
    pub fn read_storage(&self, offset: u64, length: usize) -> Result<Vec<u8>, Error> {
        self.span(offset, length)?;
        read_with(length, |bytes, d, c, r| {
            // SAFETY: bounded initialized writable vector; native copies only.
            unsafe {
                sys::ams_mel_ir_buffer_read_storage(
                    self.raw,
                    offset,
                    bytes.as_mut_ptr(),
                    length,
                    d,
                    c,
                    r,
                )
            }
        })
    }

    /// Copies bytes into fixed native host storage, without relocation or borrowing.
    /// Empty writes at capacity are valid. No provider address is dereferenced here.
    ///
    /// # Errors
    /// Closed owner, out-of-bounds span or native failure.
    pub fn write_storage(&mut self, offset: u64, bytes: &[u8]) -> Result<(), Error> {
        self.span(offset, bytes.len())?;
        call_with_diagnostic(|d, c, r| {
            // SAFETY: bounded borrowed input lives through synchronous native copy.
            unsafe {
                sys::ams_mel_ir_buffer_write_storage(
                    self.raw,
                    offset,
                    bytes.as_ptr(),
                    bytes.len(),
                    d,
                    c,
                    r,
                )
            }
        })
    }

    /// Copies ordered flags, retaining duplicates and empty lists. Uses at most
    /// two native calls (count, copy); a growing list is an error, not a retry loop.
    ///
    /// # Errors
    /// Closed owner, native refusal/exception, allocation failure, unknown flag,
    /// missing/non-Success Return, or a successful count exceeding capacity.
    /// The most recent native status and getFlags Return are separate fields.
    pub fn flags(&self) -> Result<Vec<BufferFlag>, BufferFlagsError> {
        let raw = self.live().map_err(|error| BufferFlagsError {
            error,
            native_status: None,
            provider_return: None,
        })?;
        flags_with(|values, count, result, d, c, r| {
            let (pointer, capacity) = match values {
                Some(values) => (values.as_mut_ptr(), values.len()),
                None => (ptr::null_mut(), 0),
            };
            // SAFETY: live owner, initialized writable count/Return slots; the
            // optional slice exactly bounds the native copy's output capacity.
            unsafe {
                sys::ams_mel_ir_buffer_get_flags(raw, pointer, capacity, count, result, d, c, r)
            }
        })
    }

    /// Replaces the ordered bookkeeping list, including duplicates or empty.
    /// GPU/RDMA do not change backing allocation. The provider method is void.
    ///
    /// # Errors
    /// Closed owner, copy allocation failure or native failure; no Return is invented.
    pub fn set_flags(&mut self, flags: &[BufferFlag]) -> Result<(), Error> {
        let raw = self.live()?;
        let mut values = allocated(flags.len(), 0_u32)?;
        for (out, flag) in values.iter_mut().zip(flags) {
            *out = *flag as u32;
        }
        call_with_diagnostic(|d, c, r| {
            // SAFETY: exact numeric flags and borrowed initialized input span.
            unsafe { sys::ams_mel_ir_buffer_set_flags(raw, values.as_ptr(), values.len(), d, c, r) }
        })
    }

    /// Appends a bookkeeping flag, even if duplicated. Provider operation is void.
    ///
    /// # Errors
    /// Closed owner or native failure; no upstream Return is invented.
    pub fn add_flag(&mut self, flag: BufferFlag) -> Result<(), Error> {
        let raw = self.live()?;
        call_with_diagnostic(|d, c, r| {
            // SAFETY: live unique owner and exact supported numeric flag.
            unsafe { sys::ams_mel_ir_buffer_add_flag(raw, flag as u32, d, c, r) }
        })
    }

    /// Idempotent ordinary destruction, never upstream checkout release.
    ///
    /// # Errors
    /// Native failure preserves the native output handle for retry. In particular,
    /// another provider shared owner refuses cleanup without freeing storage.
    /// Success must clear the handle; otherwise a protocol error retains it.
    pub fn close(&mut self) -> Result<(), Error> {
        close_with(&mut self.raw, |out, d, c, r| {
            // SAFETY: exclusive owner slot; native Close is idempotent/retryable.
            unsafe { NATIVE_CLOSE(out, d, c, r) }
        })
    }
}

impl Drop for HostBuffer {
    fn drop(&mut self) {
        if self.is_open() {
            native_close(&mut self.raw);
        }
    }
}

fn read_with(
    length: usize,
    read: impl FnOnce(&mut [u8], *mut c_char, usize, *mut usize) -> i32,
) -> Result<Vec<u8>, Error> {
    let mut bytes = allocated(length, 0_u8)?;
    call_with_diagnostic(|d, c, r| read(&mut bytes, d, c, r))?;
    Ok(bytes)
}

fn close_with(
    raw: &mut *mut sys::AmsMelIrBuffer,
    close: impl FnOnce(&mut *mut sys::AmsMelIrBuffer, *mut c_char, usize, *mut usize) -> i32,
) -> Result<(), Error> {
    call_with_diagnostic(|d, c, r| close(raw, d, c, r))?;
    if raw.is_null() {
        Ok(())
    } else {
        Err(protocol("successful Buffer Close retained owner"))
    }
}

fn info_from_raw(raw: sys::AmsMelIrBufferInfoV1) -> Result<BufferInfo, Error> {
    let image_location = match raw.image_location {
        sys::AMS_MEL_IR_BUFFER_IMAGE_NULL => ImageLocation::Null,
        sys::AMS_MEL_IR_BUFFER_IMAGE_IN_STORAGE => ImageLocation::InsideStorage,
        sys::AMS_MEL_IR_BUFFER_IMAGE_OUTSIDE_STORAGE => ImageLocation::OutsideStorage,
        _ => return Err(protocol("unknown Buffer image-location value")),
    };
    if raw.buffer_address_matches > 1 {
        return Err(protocol("noncanonical Buffer address-match boolean"));
    }
    Ok(BufferInfo {
        storage_size: raw.storage_size,
        initialized_context: raw.initialized_context,
        provider_size: raw.provider_size,
        provider_context: raw.provider_context,
        buffer_address_matches: raw.buffer_address_matches == 1,
        image_location,
    })
}

fn flags_with(
    mut get: impl FnMut(Option<&mut [u32]>, &mut usize, &mut u32, *mut c_char, usize, *mut usize) -> i32,
) -> Result<Vec<BufferFlag>, BufferFlagsError> {
    let mut count = 0;
    let mut result = u32::MAX;
    let mut status = sys::AMS_MEL_OK;
    let mut query = |values: Option<&mut [u32]>, count: &mut usize| {
        result = u32::MAX;
        let checked = call_with_diagnostic(|d, c, r| {
            status = get(values, count, &mut result, d, c, r);
            status
        });
        (checked, status, returned(result))
    };
    let (checked, status, provider_return) = query(None, &mut count);
    let failure = |error, status, provider_return| BufferFlagsError {
        error,
        native_status: Some(status),
        provider_return,
    };
    if let Err(error) = checked {
        if error.kind() != &ErrorKind::BufferTooSmall || status != sys::AMS_MEL_BUFFER_TOO_SMALL {
            return Err(failure(error, status, provider_return));
        }
    }
    if provider_return != Some(0) {
        return Err(failure(
            protocol("Buffer flags count without Success Return"),
            status,
            provider_return,
        ));
    }
    if (status == sys::AMS_MEL_OK && count != 0)
        || (status == sys::AMS_MEL_BUFFER_TOO_SMALL && count == 0)
    {
        return Err(failure(
            protocol("inconsistent Buffer flags count-query status"),
            status,
            provider_return,
        ));
    }
    let mut values =
        allocated(count, u32::MAX).map_err(|error| failure(error, status, provider_return))?;
    let capacity = values.len();
    let (checked, status, provider_return) = query(Some(&mut values), &mut count);
    checked.map_err(|error| failure(error, status, provider_return))?;
    if provider_return != Some(0) || count > capacity {
        return Err(failure(
            protocol("invalid successful Buffer flags output"),
            status,
            provider_return,
        ));
    }
    let mut flags = allocated(count, BufferFlag::Host)
        .map_err(|error| failure(error, status, provider_return))?;
    for (out, value) in flags.iter_mut().zip(values) {
        *out = match value {
            sys::AMS_MEL_IR_BUFFER_HOST => BufferFlag::Host,
            sys::AMS_MEL_IR_BUFFER_GPU => BufferFlag::Gpu,
            sys::AMS_MEL_IR_BUFFER_RDMA => BufferFlag::Rdma,
            _ => {
                return Err(failure(
                    protocol("unknown Buffer flag value"),
                    status,
                    provider_return,
                ))
            }
        };
    }
    Ok(flags)
}

#[cfg(test)]
mod tests;
