use super::*;
use std::cell::{Cell, RefCell};

#[derive(Clone, Copy, Debug, PartialEq)]
enum Fault {
    None,
    CreateNull,
    CreateError(bool),
    CountError,
    Count(usize),
    Reserve,
    CopyAlloc,
    GetError(bool),
    GetNull,
    ViewError,
    ViewNull,
    ViewMisaligned,
    Nested,
    Utf8,
    EntryClose(i32, bool, bool),
    ListClose(i32, bool, bool),
    CloseDiagnostic(bool),
    Diagnostic(usize, i32, u8),
}

// Handles are addresses of live independently owned tokens, not fabricated
// native pointers. No injected handle is ever passed to native code or
// dereferenced. Consumption and slot clearing are independently controllable.
struct Calls {
    fault: Fault,
    at: usize,
    index: Cell<usize>,
    list: RefCell<Option<Box<u8>>>,
    entry: RefCell<Option<Box<u8>>>,
    retained: RefCell<[Option<Box<u8>>; 4]>,
    events: RefCell<Vec<&'static str>>,
    raw: sys::AmsMelIrChannelCapabilityV1,
    malformed: Option<sys::AmsMelIrChannelCapabilityV1>,
}

fn diagnostic(d: *mut c_char, c: usize, r: *mut usize, status: i32) -> i32 {
    let text: &[u8] = if status == 0 {
        b"\0"
    } else {
        b"original failure\0"
    };
    // SAFETY: injected caller supplies writable diagnostic storage, or null/0
    // for diagnostic-free cleanup. No handle pointers are dereferenced here.
    unsafe {
        if !r.is_null() {
            *r = text.len();
        }
        if c != 0 {
            ptr::copy_nonoverlapping(text.as_ptr(), d.cast(), c.min(text.len()));
        }
    }
    status
}

impl Calls {
    fn new(fault: Fault, at: usize) -> Self {
        // Every field in this C record permits zero; all spans are empty.
        let mut raw: sys::AmsMelIrChannelCapabilityV1 = unsafe { mem::zeroed() };
        static TYPES: [u32; 4] = [2, 7, 1234, 2];
        raw.channel_types = sys::AmsMelU32SpanV1 {
            data: TYPES.as_ptr(),
            size: TYPES.len(),
        };
        raw.pixel_format = 99;
        Self {
            fault,
            at,
            index: Cell::new(0),
            list: RefCell::new(None),
            entry: RefCell::new(None),
            retained: RefCell::new(std::array::from_fn(|_| None)),
            events: RefCell::new(Vec::with_capacity(64)),
            raw,
            malformed: None,
        }
    }
    fn event(&self, event: &'static str) {
        self.events.borrow_mut().push(event);
    }
    fn active(&self) -> bool {
        self.index.get() == self.at
    }
    fn retain(&self, token: Box<u8>) {
        *self
            .retained
            .borrow_mut()
            .iter_mut()
            .find(|slot| slot.is_none())
            .unwrap() = Some(token);
    }
    fn occurrences(&self, event: &str) -> usize {
        self.events
            .borrow()
            .iter()
            .filter(|value| **value == event)
            .count()
    }
    fn error(&self, expected: ErrorKind) -> Error {
        let error = discover(self).expect_err("must not publish a partial list");
        assert_eq!(error.kind(), &expected, "{error:?}");
        assert!(self.entry.borrow().is_none(), "entry leak");
        assert!(self.list.borrow().is_none(), "list leak");
        error
    }
}

impl DiscoveryCalls for Calls {
    fn create(
        &self,
        out: *mut *mut sys::AmsMelIrCapabilityList,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32 {
        self.event("create");
        assert!(unsafe { (*out).is_null() });
        if !matches!(self.fault, Fault::CreateNull | Fault::CreateError(false)) {
            let mut token = Box::new(1);
            unsafe {
                *out = (&mut *token as *mut u8).cast();
            }
            *self.list.borrow_mut() = Some(token);
        }
        let status = if matches!(self.fault, Fault::CreateError(_)) {
            11
        } else {
            0
        };
        if let Fault::Diagnostic(required, status, byte) = self.fault {
            unsafe {
                *r = required;
                *d = byte as c_char;
            }
            return status;
        }
        diagnostic(d, c, r, status)
    }
    fn count(
        &self,
        _: *mut sys::AmsMelIrCapabilityList,
        out: *mut usize,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32 {
        self.event("count");
        unsafe {
            *out = if let Fault::Count(n) = self.fault {
                n
            } else {
                4
            };
        }
        let status = diagnostic(
            d,
            c,
            r,
            if self.fault == Fault::CountError {
                11
            } else {
                0
            },
        );
        if self.fault == Fault::Reserve {
            crate::test_allocator::FAIL_SIZE
                .with(|v| v.set(4 * mem::size_of::<ChannelCapability>()));
        }
        status
    }
    fn get(
        &self,
        _: *mut sys::AmsMelIrCapabilityList,
        index: usize,
        out: *mut *mut sys::AmsMelIrChannelCapability,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32 {
        self.event("get");
        self.index.set(index);
        assert!(self.entry.borrow().is_none(), "earlier entry not closed");
        assert!(unsafe { (*out).is_null() });
        if !(self.active() && matches!(self.fault, Fault::GetNull | Fault::GetError(false))) {
            let mut token = Box::new(2);
            unsafe {
                *out = (&mut *token as *mut u8).cast();
            }
            *self.entry.borrow_mut() = Some(token);
        }
        diagnostic(
            d,
            c,
            r,
            if self.active() && matches!(self.fault, Fault::GetError(_)) {
                11
            } else {
                0
            },
        )
    }
    fn close_list(
        &self,
        out: *mut *mut sys::AmsMelIrCapabilityList,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32 {
        self.event("list-close");
        assert!(self.entry.borrow().is_none());
        assert!(self.list.borrow().is_some(), "double list release");
        let (status, consume, clear) = if let Fault::ListClose(s, a, b) = self.fault {
            (s, a, b)
        } else if self.fault == Fault::Nested && self.active() {
            (11, true, false)
        } else {
            (0, true, true)
        };
        let token = self.list.borrow_mut().take().unwrap();
        if !consume {
            self.retain(token);
        }
        if clear {
            unsafe {
                *out = ptr::null_mut();
            }
        }
        if self.fault == Fault::CloseDiagnostic(false) {
            unsafe {
                *r = usize::MAX;
            }
            return 0;
        }
        diagnostic(d, c, r, status)
    }
}

impl CapabilityCalls for Calls {
    fn view(
        &self,
        _: *mut sys::AmsMelIrChannelCapability,
        out: *mut *const sys::AmsMelIrChannelCapabilityV1,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32 {
        self.event("view");
        let active = self.active();
        if !(active && self.fault == Fault::ViewNull) {
            // Misalignment is within a live record, and is rejected before load.
            let view = if active && self.fault == Fault::ViewMisaligned {
                (&self.raw as *const sys::AmsMelIrChannelCapabilityV1)
                    .cast::<u8>()
                    .wrapping_add(1)
                    .cast()
            } else if active {
                self.malformed.as_ref().unwrap_or(&self.raw)
            } else {
                &self.raw
            };
            unsafe {
                *out = view;
            }
        }
        let status = diagnostic(
            d,
            c,
            r,
            if active && self.fault == Fault::ViewError {
                11
            } else {
                0
            },
        );
        if active && self.fault == Fault::CopyAlloc {
            crate::test_allocator::FAIL_SIZE.with(|v| v.set(4 * mem::size_of::<ChannelType>()));
        }
        status
    }
    fn close(
        &self,
        out: *mut *mut sys::AmsMelIrChannelCapability,
        d: *mut c_char,
        c: usize,
        r: *mut usize,
    ) -> i32 {
        self.event("entry-close");
        assert!(self.entry.borrow().is_some(), "double entry release");
        let (status, consume, clear) = if let Fault::EntryClose(s, a, b) = self.fault {
            if self.active() {
                (s, a, b)
            } else {
                (0, true, true)
            }
        } else if self.fault == Fault::Nested && self.active() {
            (11, true, false)
        } else {
            (0, true, true)
        };
        let token = self.entry.borrow_mut().take().unwrap();
        if !consume {
            self.retain(token);
        }
        if clear {
            unsafe {
                *out = ptr::null_mut();
            }
        }
        if self.fault == Fault::CloseDiagnostic(true) && self.active() {
            unsafe {
                *r = 0;
            }
            return 0;
        }
        diagnostic(d, c, r, status)
    }
}

#[test]
fn discovery_null_and_error_list_owners() {
    for (fault, kind, releases) in [
        (Fault::CreateNull, ErrorKind::ProtocolInconsistency, 0),
        (Fault::CreateError(false), ErrorKind::ProviderFailed, 0),
        (Fault::CreateError(true), ErrorKind::ProviderFailed, 1),
        (Fault::CountError, ErrorKind::ProviderFailed, 1),
    ] {
        let calls = Calls::new(fault, 0);
        let error = calls.error(kind);
        if fault != Fault::CreateNull {
            assert_eq!(error.diagnostic(), Some("original failure"));
        }
        assert_eq!(calls.occurrences("list-close"), releases);
        assert_eq!(calls.occurrences("get"), 0);
    }
}

#[test]
fn discovery_count_bounds_and_fallible_reservation() {
    for count in [
        usize::MAX,
        isize::MAX as usize,
        isize::MAX as usize / mem::size_of::<ChannelCapability>() + 1,
    ] {
        let calls = Calls::new(Fault::Count(count), 0);
        calls.error(ErrorKind::ProtocolInconsistency);
        assert_eq!(calls.occurrences("get"), 0);
        assert_eq!(calls.occurrences("list-close"), 1);
    }
    let calls = Calls::new(Fault::Reserve, 0);
    calls.error(ErrorKind::InternalError);
    assert_eq!(calls.occurrences("get"), 0);
    assert_eq!(calls.occurrences("list-close"), 1);
}

#[test]
fn discovery_failure_at_entry_n_releases_prefix_current_and_list_once() {
    for at in [0, 1, 3] {
        for (fault, kind, current) in [
            (Fault::GetError(false), ErrorKind::ProviderFailed, 0),
            (Fault::GetError(true), ErrorKind::ProviderFailed, 1),
            (Fault::GetNull, ErrorKind::ProtocolInconsistency, 0),
            (Fault::ViewError, ErrorKind::ProviderFailed, 1),
            (Fault::ViewNull, ErrorKind::ProtocolInconsistency, 1),
            (Fault::ViewMisaligned, ErrorKind::ProtocolInconsistency, 1),
            (Fault::CopyAlloc, ErrorKind::InternalError, 1),
        ] {
            let calls = Calls::new(fault, at);
            let error = calls.error(kind);
            if matches!(fault, Fault::GetError(_) | Fault::ViewError) {
                assert_eq!(error.diagnostic(), Some("original failure"));
            }
            assert_eq!(calls.occurrences("entry-close"), at + current);
            assert_eq!(calls.occurrences("get"), at + 1);
            assert_eq!(calls.occurrences("list-close"), 1);
            assert_eq!(calls.events.borrow().last(), Some(&"list-close"));
        }
    }
}

#[test]
fn discovery_nested_data_and_utf8_errors_cleanup() {
    for at in [0, 1, 3] {
        for (fault, kind) in [
            (Fault::Nested, ErrorKind::ProtocolInconsistency),
            (Fault::Utf8, ErrorKind::InvalidUtf8),
        ] {
            let mut calls = Calls::new(fault, at);
            let mut raw = calls.raw;
            if fault == Fault::Nested {
                raw.image_bands.size = 1;
            } else {
                raw.channel_id.descriptive_label = sys::AmsMelStringViewV1 {
                    data: b"\xff".as_ptr().cast(),
                    size: 1,
                };
            }
            calls.malformed = Some(raw);
            calls.error(kind);
            assert_eq!(calls.occurrences("entry-close"), at + 1);
            assert_eq!(calls.occurrences("list-close"), 1);
        }
    }
}

#[test]
fn discovery_close_uncertainty_never_retries_consumed_or_retained_tokens() {
    for entry in [false, true] {
        for status in [0, 11] {
            for consume in [false, true] {
                for clear in [false, true] {
                    let fault = if entry {
                        Fault::EntryClose(status, consume, clear)
                    } else {
                        Fault::ListClose(status, consume, clear)
                    };
                    let calls = Calls::new(fault, 1);
                    let result = discover(&calls);
                    if status != 0 {
                        let error = result.unwrap_err();
                        assert_eq!(error.kind(), &ErrorKind::ProviderFailed);
                        assert_eq!(error.diagnostic(), Some("original failure"));
                    } else if !clear {
                        assert_eq!(
                            result.unwrap_err().kind(),
                            &ErrorKind::ProtocolInconsistency
                        );
                    } else {
                        assert!(result.is_ok());
                    }
                    assert_eq!(calls.occurrences("list-close"), 1);
                    assert_eq!(
                        calls.occurrences("entry-close"),
                        if entry && (status != 0 || !clear) {
                            2
                        } else {
                            4
                        }
                    );
                    // If release lied about ownership, rescue only the token in
                    // test storage; production must not touch uncertain handles.
                    calls.entry.borrow_mut().take();
                    calls.list.borrow_mut().take();
                    assert_eq!(
                        calls.retained.borrow().iter().flatten().count(),
                        usize::from(!consume)
                    );
                }
            }
        }
    }
}

#[test]
fn discovery_malformed_diagnostics_and_unknown_status_do_not_panic() {
    for (required, status, byte, kind) in [
        (0, 0, 0, ErrorKind::ProtocolInconsistency),
        (usize::MAX, 0, 0, ErrorKind::ProtocolInconsistency),
        (usize::MAX, -765, 0, ErrorKind::Unknown(-765)),
        (1, -765, 0, ErrorKind::Unknown(-765)),
        (1, 11, b'x', ErrorKind::ProtocolInconsistency),
        (2, 11, 255, ErrorKind::InvalidUtf8),
    ] {
        let calls = Calls::new(Fault::Diagnostic(required, status, byte), 0);
        calls.error(kind);
        assert_eq!(calls.occurrences("list-close"), 1);
    }
}

#[test]
fn discovery_close_diagnostic_errors_do_not_retry_consumed_owners() {
    for entry in [false, true] {
        let calls = Calls::new(Fault::CloseDiagnostic(entry), 1);
        calls.error(ErrorKind::ProtocolInconsistency);
        assert_eq!(calls.occurrences("entry-close"), if entry { 2 } else { 4 });
        assert_eq!(calls.occurrences("list-close"), 1);
    }
}

#[test]
fn discovery_duplicates_unknowns_and_independent_results() {
    let calls = Calls::new(Fault::None, 0);
    let mut first = discover(&calls).unwrap();
    let second = discover(&calls).unwrap();
    assert_eq!(first, second);
    assert_eq!(first.len(), 4);
    assert_eq!(first[0], first[3]);
    assert_eq!(first[0].pixel_format, PixelFormat::Unknown(99));
    assert_eq!(
        first[0].channel_types,
        [
            ChannelType::CommandAndControl,
            ChannelType::Reserved1,
            ChannelType::Unknown(1234),
            ChannelType::CommandAndControl
        ]
    );
    first[0].channel_types.clear();
    assert_eq!(second[0].channel_types.len(), 4);
    assert_eq!(calls.occurrences("entry-close"), 8);
    assert_eq!(calls.occurrences("list-close"), 2);
    assert_eq!(
        &calls.events.borrow()[..15],
        &[
            "create",
            "count",
            "get",
            "view",
            "entry-close",
            "get",
            "view",
            "entry-close",
            "get",
            "view",
            "entry-close",
            "get",
            "view",
            "entry-close",
            "list-close",
        ]
    );
}

#[test]
fn discovery_empty_closes_list_without_get_or_view() {
    let calls = Calls::new(Fault::Count(0), 0);
    assert!(discover(&calls).unwrap().is_empty());
    assert_eq!(*calls.events.borrow(), ["create", "count", "list-close"]);
    assert!(calls.list.borrow().is_none());
    assert!(calls.entry.borrow().is_none());
}
