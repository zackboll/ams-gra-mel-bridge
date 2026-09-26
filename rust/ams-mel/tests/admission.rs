//! Separate test executable: retained mock DSO/flags cannot affect unload tests.
use ams_mel::{
    ComponentLocation, ControlConfig, ErrorKind, MelErrorCode, ModeResult, Session, SessionOptions,
    UciId,
};
use std::ffi::{c_char, c_int, c_void, CString};
use std::path::{Path, PathBuf};
use std::ptr;

type Gate = unsafe extern "C" fn(u32, u64, *mut u64, *mut u64) -> c_int;

#[link(name = "dl")]
extern "C" {
    fn dlopen(path: *const c_char, flags: c_int) -> *mut c_void;
    fn dlsym(handle: *mut c_void, symbol: *const c_char) -> *mut c_void;
    fn dlclose(handle: *mut c_void) -> c_int;
}
extern "C" {
    fn ams_mel_test_completion_boundary(
        family: u32,
        operation: u32,
        target: u64,
        counts: *mut u64,
    ) -> c_int;
    fn ams_mel_test_completion_owner(family: u32, target: u64, observed: *mut u64) -> c_int;
}

struct Mock {
    handle: *mut c_void,
    gate: Gate,
}
impl Mock {
    fn open(path: &Path) -> Self {
        use std::os::unix::ffi::OsStrExt;
        let path = CString::new(path.as_os_str().as_bytes()).unwrap();
        // SAFETY: NUL-terminated paths/symbols; RTLD_NOW; DSO held through all calls.
        unsafe {
            let handle = dlopen(path.as_ptr(), 2);
            assert!(!handle.is_null());
            let symbol = CString::new("mock_completion_gate").unwrap();
            let address = dlsym(handle, symbol.as_ptr());
            assert!(!address.is_null());
            Self {
                handle,
                gate: std::mem::transmute::<*mut c_void, Gate>(address),
            }
        }
    }
    fn control(&self, operation: u32, count: u64) {
        // SAFETY: exact test-provider signature; output pointers are optional.
        assert_eq!(
            unsafe { (self.gate)(operation, count, ptr::null_mut(), ptr::null_mut()) },
            1
        );
    }
    fn reclaimed(&self) {
        for family in [0, 1] {
            let mut counts = [0_u64; 4];
            let mut observed = 0;
            // SAFETY: exact test-only signatures and appropriately sized output storage.
            unsafe {
                assert_eq!(
                    ams_mel_test_completion_boundary(family, 3, 0, counts.as_mut_ptr()),
                    1
                );
                assert_eq!(
                    ams_mel_test_completion_owner(family, counts[2], &mut observed),
                    1
                );
            }
        }
    }
}
impl Drop for Mock {
    fn drop(&mut self) {
        // SAFETY: release this helper's reference after final-owner synchronization.
        unsafe {
            dlclose(self.handle);
        }
    }
}

#[test]
fn safe_admission_lifetime_and_provider_rejection() {
    let provider = std::env::var_os("AMS_MEL_TEST_PROVIDER_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| {
            Path::new(env!("CARGO_MANIFEST_DIR")).join("../../native/build-tests/test-providers")
        })
        .join("libmock_ir_provider.so");
    let mock = Mock::open(&provider);
    let config = ControlConfig::new(
        UciId::new([0; 16], "admission").unwrap(),
        UciId::new([0; 16], "platform").unwrap(),
        ComponentLocation::new(1.25, -2.5, 3.75, "station-1", "mock-aircraft").unwrap(),
    );
    assert_eq!(SessionOptions::default().max_async_requests, 0);
    for options in [
        None,
        Some(SessionOptions::default()),
        Some(SessionOptions {
            max_async_requests: 1,
        }),
    ] {
        let session = match options {
            None => Session::open(&provider, "completion-scale", ""),
            Some(options) => Session::open_with_options(&provider, "completion-scale", "", options),
        }
        .unwrap();
        let mut channel = session.open_control_channel(&config).unwrap();
        channel.enable().unwrap();
        let first = channel.submit_operate(1).unwrap();
        if options.is_some_and(|value| value.max_async_requests == 1) {
            let error = channel.submit_bit_noop(2).unwrap_err();
            assert_eq!(error.kind(), &ErrorKind::ResourceExhausted);
            assert!(error.to_string().contains("async request limit reached"));
            first.close().unwrap();
            assert_eq!(
                channel.submit_operate(3).unwrap_err().kind(),
                &ErrorKind::ResourceExhausted
            );
            mock.control(2, 1);
        } else {
            let mut second = channel.submit_bit_noop(2).unwrap();
            mock.control(2, 2);
            second.wait(15_000).unwrap();
            second.close().unwrap();
            first.close().unwrap();
        }
        mock.reclaimed();
        let mut retry = channel.submit_operate(4).unwrap();
        mock.control(2, 1);
        retry.wait(15_000).unwrap();
        mock.reclaimed();
        retry.close().unwrap();
        channel.close().unwrap();
        session.close().unwrap();
    }
    mock.control(4, 0);
    let session = Session::open_with_options(
        &provider,
        "completion-scale",
        "",
        SessionOptions {
            max_async_requests: 1,
        },
    )
    .unwrap();
    let mut channel = session.open_control_channel(&config).unwrap();
    channel.enable().unwrap();
    let mut request = channel.submit_operate(5).unwrap();
    mock.control(2, 1);
    assert_eq!(
        request.wait(15_000).unwrap(),
        ModeResult::Rejected {
            code: MelErrorCode::InsufficientResources,
            description: "provider resource rejection".into(),
        }
    );
    mock.reclaimed();
    request.close().unwrap();
    channel.close().unwrap();
    session.close().unwrap();
}
