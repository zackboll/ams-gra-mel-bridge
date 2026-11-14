//! Synchronous value-only Control discovery, with one-attempt temporary owners.
use super::*;

impl Session {
    /// Copies every advertised capability before channel attachment, in exact
    /// provider order, retaining duplicates and valid zero image layouts.
    ///
    /// All nested values are Rust-owned and survive Session closure and provider
    /// unload. No attachment, enablement, callback or buffer registration occurs.
    /// Advertisement is not evidence of operational sensor support or configured
    /// acquisition. Serialize against closure and conflicting provider operations;
    /// this does not change the Session's non-Send/non-Sync ownership policy.
    ///
    /// Native failures retain their status and diagnostic. Malformed owners,
    /// counts or views return a protocol error; invalid text returns InvalidUtf8.
    /// Allocation failures return InternalError. No partial result is returned.
    /// Each acquired temporary owner receives one release attempt; secondary
    /// cleanup errors never replace the original failure.
    ///
    /// ```no_run
    /// use ams_mel::{ChannelCapability, ChannelType, Error, Session};
    /// pub fn discover_required(session: &Session)
    ///     -> Result<Vec<ChannelCapability>, Error> {
    ///     let capabilities = session.capabilities()?;
    ///     for capability in &capabilities {
    ///         for (kind, label) in [
    ///             (ChannelType::CommandAndControl, "C2"),
    ///             (ChannelType::HealthAndStatus, "Health/Status"),
    ///             (ChannelType::IrstImage, "Image"),
    ///         ] {
    ///             if capability.channel_types.contains(&kind) {
    ///                 println!("{label}: {}", capability.channel_id.descriptive_label());
    ///             }
    ///         }
    ///     }
    ///     Ok(capabilities)
    /// }
    /// ```
    pub fn capabilities(&self) -> Result<Vec<ChannelCapability>, Error> {
        discover(&NativeDiscoveryCalls(self.raw))
    }
}

trait DiscoveryCalls: CapabilityCalls {
    fn create(
        &self,
        out: *mut *mut sys::AmsMelIrCapabilityList,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32;
    fn count(
        &self,
        list: *mut sys::AmsMelIrCapabilityList,
        out: *mut usize,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32;
    fn get(
        &self,
        list: *mut sys::AmsMelIrCapabilityList,
        index: usize,
        out: *mut *mut sys::AmsMelIrChannelCapability,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32;
    fn close_list(
        &self,
        out: *mut *mut sys::AmsMelIrCapabilityList,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32;
    fn snapshot(
        &self,
        list: *mut sys::AmsMelIrCapabilityList,
        index: usize,
    ) -> Result<ChannelCapability, Error>
    where
        Self: Sized,
    {
        capability_snapshot_with(|o, d, c, r| self.get(list, index, o, d, c, r), self)
    }
}

struct NativeDiscoveryCalls(*mut sys::AmsMelSession);

impl CapabilityCalls for NativeDiscoveryCalls {
    fn view(
        &self,
        o: *mut sys::AmsMelIrChannelCapability,
        v: *mut *const sys::AmsMelIrChannelCapabilityV1,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32 {
        NativeCapabilityCalls.view(o, v, d, c, r)
    }
    fn close(
        &self,
        o: *mut *mut sys::AmsMelIrChannelCapability,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32 {
        NativeCapabilityCalls.close(o, d, c, r)
    }
}

impl DiscoveryCalls for NativeDiscoveryCalls {
    fn create(
        &self,
        o: *mut *mut sys::AmsMelIrCapabilityList,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32 {
        // SAFETY: Session is owned by the caller, output starts null, and all
        // output/diagnostic storage has the exact ABI shape and lifetime.
        unsafe { sys::ams_mel_ir_capability_list_create(self.0, o, d, c, r) }
    }
    fn count(
        &self,
        l: *mut sys::AmsMelIrCapabilityList,
        o: *mut usize,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32 {
        // SAFETY: the list guard is live; output storage is writable.
        unsafe { sys::ams_mel_ir_capability_list_count(l, o, d, c, r) }
    }
    fn get(
        &self,
        l: *mut sys::AmsMelIrCapabilityList,
        i: usize,
        o: *mut *mut sys::AmsMelIrChannelCapability,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32 {
        // SAFETY: live list, checked index, initially null output slot.
        unsafe { sys::ams_mel_ir_capability_list_get(l, i, o, d, c, r) }
    }
    fn close_list(
        &self,
        o: *mut *mut sys::AmsMelIrCapabilityList,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32 {
        // SAFETY: guard transfers its unique list owner exactly once.
        unsafe { sys::ams_mel_ir_capability_list_close(o, d, c, r) }
    }
    fn snapshot(
        &self,
        list: *mut sys::AmsMelIrCapabilityList,
        index: usize,
    ) -> Result<ChannelCapability, Error> {
        capability_snapshot(|o, d, c, r| self.get(list, index, o, d, c, r))
    }
}

struct CapabilityListOwner<'a, C: DiscoveryCalls> {
    raw: *mut sys::AmsMelIrCapabilityList,
    calls: &'a C,
}

impl<C: DiscoveryCalls> CapabilityListOwner<'_, C> {
    fn close(&mut self) -> Result<(), Error> {
        let mut raw = mem::replace(&mut self.raw, ptr::null_mut());
        call_with_diagnostic(|d, c, r| self.calls.close_list(&mut raw, d, c, r))?;
        if !raw.is_null() {
            return Err(protocol(
                "capability list close succeeded without clearing the owner",
            ));
        }
        Ok(())
    }
}

impl<C: DiscoveryCalls> Drop for CapabilityListOwner<'_, C> {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            let mut raw = mem::replace(&mut self.raw, ptr::null_mut());
            self.calls
                .close_list(&mut raw, ptr::null_mut(), 0, ptr::null_mut());
        }
    }
}

fn discover(calls: &impl DiscoveryCalls) -> Result<Vec<ChannelCapability>, Error> {
    let mut owner = CapabilityListOwner {
        raw: ptr::null_mut(),
        calls,
    };
    call_with_diagnostic(|d, c, r| calls.create(&mut owner.raw, d, c, r))?;
    if owner.raw.is_null() {
        return Err(protocol(
            "capability list creation succeeded without an owner",
        ));
    }
    let mut count = 0;
    call_with_diagnostic(|d, c, r| calls.count(owner.raw, &mut count, d, c, r))?;
    count
        .checked_mul(mem::size_of::<ChannelCapability>())
        .filter(|bytes| *bytes <= isize::MAX as usize)
        .ok_or_else(|| protocol("native capability list count is too large"))?;
    // Checked size arithmetic precedes allocation and every entry retrieval.
    let mut values = reserve_owned(count, "capability list")?;
    for index in 0..count {
        values.push(calls.snapshot(owner.raw, index)?);
    }
    owner.close()?;
    Ok(values)
}

#[cfg(test)]
#[path = "ir_discovery_tests.rs"]
mod tests;
