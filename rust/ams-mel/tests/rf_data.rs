//! RUST-MEL-002 safe RF DataMEL contract tests against the existing compiled
//! mock RF provider and missing-symbol RF provider.
//!
//! The mock's lifetime log, its process-global counters and the
//! `AMS_MEL_TEST_LIFETIME_LOG` / `AMS_MEL_TEST_RF_ALLOCATION_FAILURE`
//! variables are process-global, so every test here is serialized. A throwing
//! shutdown makes the native bridge retain the mock DSO for the rest of the
//! process; those cases run only in a re-executed child process so that
//! retention can never contaminate the unload assertions in this executable.
//!
//! Tests named `failpoint_*` need the test-enabled facade (native/build-tests)
//! because they use its existing allocation failpoints; every other test also
//! runs against the production facade.

use std::env;
use std::ffi::{c_char, c_int, c_void, CString, OsString};
use std::fs;
use std::os::unix::ffi::{OsStrExt, OsStringExt};
use std::os::unix::fs::symlink;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Mutex, MutexGuard, PoisonError};

use ams_mel::{
    ErrorKind, RfAdmin, RfData, RfFaceInfo, RfFrequencyRange, RfJobDataFormat, RfMfaInfo,
    RfMfaState,
};

static NEXT_LOG: AtomicU64 = AtomicU64::new(0);
static PROVIDER_TEST: Mutex<()> = Mutex::new(());

const CHILD_ENV: &str = "AMS_MEL_RUST_RF_DATA_CHILD";
const CHILD_DONE: &str = "RF_DATA_CHILD_COMPLETE";
const FAILPOINT_ENV: &str = "AMS_MEL_TEST_RF_ALLOCATION_FAILURE";

const OK_OPEN: &str = "rf_factory_called\n";
/// One shutdown, then DataMEL destruction (its RFMFAInfo member second), then
/// provider DSO unload.
const OK_CLOSE: &str = "rf_shutdown\nrf_data_destroyed\nrf_mfa_info_destroyed\nlibrary_unloaded\n";
const FAILED_OPEN: &str = "rf_factory_called\nlibrary_unloaded\n";

const RTLD_NOW: c_int = 2;
const RTLD_NOLOAD: c_int = 4;

#[link(name = "dl")]
extern "C" {
    fn dlopen(path: *const c_char, flags: c_int) -> *mut c_void;
    fn dlsym(handle: *mut c_void, symbol: *const c_char) -> *mut c_void;
    fn dlclose(handle: *mut c_void) -> c_int;
}

type Counter = unsafe extern "C" fn() -> u32;

/// Mock RF provider TEST-only observations, read without ever loading the
/// provider or extending its lifetime beyond the call.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
struct Observed {
    shutdowns: u32,
    forbidden: u32,
    getters: u32,
    quantizes: u32,
}

/// Returns `None` if the mock RF provider is not currently loaded. Otherwise
/// a `RTLD_NOLOAD` reference is taken only for the duration of the read and
/// is released before returning, so it cannot delay a later unload. Every
/// symbol is resolved and read before that reference is dropped.
fn observe() -> Option<Observed> {
    let path = CString::new(mock_provider().as_os_str().as_bytes()).expect("provider path");
    // SAFETY: NUL-terminated path; RTLD_NOLOAD never loads a new object.
    let handle = unsafe { dlopen(path.as_ptr(), RTLD_NOW | RTLD_NOLOAD) };
    if handle.is_null() {
        return None;
    }
    let read = |name: &str| {
        let symbol = CString::new(name).expect("symbol name");
        // SAFETY: `handle` is live; the symbol is the mock's exact
        // `unsigned (void)` TEST-only export.
        unsafe {
            let address = dlsym(handle, symbol.as_ptr());
            assert!(!address.is_null(), "missing mock export {name}");
            let counter = std::mem::transmute::<*mut c_void, Counter>(address);
            counter()
        }
    };
    let observed = Observed {
        shutdowns: read("mock_rf_shutdown_calls"),
        forbidden: read("mock_rf_forbidden_calls"),
        getters: read("mock_rf_getter_calls"),
        quantizes: read("mock_rf_quantize_calls"),
    };
    // SAFETY: releases exactly the reference acquired above.
    assert_eq!(unsafe { dlclose(handle) }, 0);
    Some(observed)
}

fn loaded(path: &Path) -> bool {
    let path = CString::new(path.as_os_str().as_bytes()).expect("provider path");
    // SAFETY: NUL-terminated path; RTLD_NOLOAD never loads a new object, and
    // any reference it returns is released immediately.
    unsafe {
        let handle = dlopen(path.as_ptr(), RTLD_NOW | RTLD_NOLOAD);
        if handle.is_null() {
            return false;
        }
        assert_eq!(dlclose(handle), 0);
        true
    }
}

fn provider_loaded() -> bool {
    loaded(&mock_provider())
}

fn lock() -> MutexGuard<'static, ()> {
    PROVIDER_TEST.lock().unwrap_or_else(PoisonError::into_inner)
}

fn mock_provider() -> PathBuf {
    provider_path("libmock_rf_provider.so")
}

fn missing_symbol_provider() -> PathBuf {
    provider_path("libmissing_symbol_rf_provider.so")
}

fn provider_path(name: &str) -> PathBuf {
    let repository = Path::new(env!("CARGO_MANIFEST_DIR")).join("../..");
    env::var_os("AMS_MEL_TEST_PROVIDER_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| repository.join("native/build-tests/test-providers"))
        .join(name)
}

/// Per-test lifetime log. The mock appends without `O_CREAT`, so the file is
/// created here before any provider event can be recorded.
struct LifetimeLog {
    directory: PathBuf,
    path: PathBuf,
}

impl LifetimeLog {
    fn new() -> Self {
        let sequence = NEXT_LOG.fetch_add(1, Ordering::Relaxed);
        let directory = env::temp_dir().join(format!(
            "ams-mel-rust-rf-data-{}-{sequence}",
            std::process::id()
        ));
        fs::create_dir(&directory).expect("create lifetime directory");
        let path = directory.join("lifetime.log");
        fs::write(&path, "").expect("create lifetime log");
        env::set_var("AMS_MEL_TEST_LIFETIME_LOG", &path);
        Self { directory, path }
    }

    fn contents(&self) -> String {
        fs::read_to_string(&self.path).expect("read lifetime log")
    }

    /// No out-of-scope, post-shutdown, repeated-shutdown or unknown-face
    /// provider use was recorded.
    fn assert_no_violations(&self) {
        let log = self.contents();
        for event in [
            "rf_forbidden_call",
            "rf_call_after_shutdown",
            "rf_shutdown_repeated",
            "rf_unknown_face",
        ] {
            assert_eq!(count(&log, event), 0, "{event} in\n{log}");
        }
    }
}

impl Drop for LifetimeLog {
    fn drop(&mut self) {
        env::remove_var("AMS_MEL_TEST_LIFETIME_LOG");
        let _ = fs::remove_file(&self.path);
        let _ = fs::remove_dir(&self.directory);
    }
}

/// Sets a native allocation failpoint for its lifetime (test facade only).
struct Failpoint;

impl Failpoint {
    fn set(value: &str) -> Self {
        env::set_var(FAILPOINT_ENV, value);
        Self
    }
}

impl Drop for Failpoint {
    fn drop(&mut self) {
        env::remove_var(FAILPOINT_ENV);
    }
}

fn count(log: &str, event: &str) -> usize {
    log.lines().filter(|line| *line == event).count()
}

fn open(configuration: &str) -> RfData {
    let data = RfData::open(mock_provider(), configuration).expect(configuration);
    assert!(data.is_open());
    data
}

fn range(min_hz: f64, max_hz: f64) -> RfFrequencyRange {
    RfFrequencyRange { min_hz, max_hz }
}

/// The complete deterministic mock snapshot, exactly as the native C11 test
/// `verify_mock_snapshot` specifies it.
fn expected_snapshot(reported_num_faces: u64, future_format: bool) -> RfMfaInfo {
    let mut supported_data_formats = Vec::new();
    if future_format {
        // std::set<JobDataFormat> order: as an int, 0xF0000001 is negative,
        // so the provider reports it first.
        supported_data_formats.push(RfJobDataFormat::Unknown(0xF000_0001));
    }
    supported_data_formats.extend([
        RfJobDataFormat::DirectInt8,
        RfJobDataFormat::ComplexInt16,
        RfJobDataFormat::AmsVitaLarge,
        RfJobDataFormat::LfType3,
    ]);
    RfMfaInfo {
        reported_num_faces,
        contains_open_additions: true,
        scheduler_resolution_fs: 12_345,
        max_user_defined_context_bytes: 9_876,
        supported_data_formats,
        faces: vec![
            RfFaceInfo {
                face_id: 7,
                supports_receive: true,
                supports_transmit: false,
                requires_endpoint_association: true,
                agc_processing_time_fs: 1_001,
                min_job_request_lead_time_fs: 1_002,
                max_job_request_lead_time_fs: 1_003_000_000_000_000,
                min_job_detail_lead_time_fs: 1_004,
                tx_rx_switching_time_fs: 1_005,
                rx_tx_switching_time_fs: 1_006,
                tx_tx_switching_time_fs: 1_007,
                rx_rx_switching_time_fs: 1_008,
                rx_frequency_ranges: vec![
                    range(100_250_000.125, 200_500_000.5),
                    range(1_500_000_000.25, 2_750_000_000.75),
                ],
                tx_frequency_ranges: Vec::new(),
                sample_frequency_ranges: vec![range(1_024_000.5, 2_048_000.25)],
            },
            RfFaceInfo {
                face_id: 42,
                supports_receive: false,
                supports_transmit: true,
                requires_endpoint_association: false,
                agc_processing_time_fs: 0,
                min_job_request_lead_time_fs: 42_001,
                max_job_request_lead_time_fs: i64::MAX,
                min_job_detail_lead_time_fs: 42_003,
                tx_rx_switching_time_fs: 42_004,
                rx_tx_switching_time_fs: 42_005,
                tx_tx_switching_time_fs: 42_006,
                rx_rx_switching_time_fs: -42_007,
                rx_frequency_ranges: Vec::new(),
                tx_frequency_ranges: vec![range(433_125_000.5, 434_875_000.25)],
                // Provider order preserved; inverted ranges not normalized.
                sample_frequency_ranges: vec![range(3_000_000.75, 1_000_000.125), range(0.5, 0.25)],
            },
        ],
    }
}

fn bits(ranges: &[RfFrequencyRange]) -> Vec<(u64, u64)> {
    ranges
        .iter()
        .map(|range| (range.min_hz.to_bits(), range.max_hz.to_bits()))
        .collect()
}

/// Field-by-field and bit-exact comparison with the expected mock snapshot,
/// so a mismatch names the field and float equality cannot hide a sign or
/// rounding difference.
fn assert_snapshot(actual: &RfMfaInfo, expected: &RfMfaInfo) {
    assert_eq!(actual.reported_num_faces, expected.reported_num_faces);
    assert_eq!(
        actual.supported_data_formats,
        expected.supported_data_formats
    );
    assert_eq!(actual.faces.len(), expected.faces.len());
    for (face, want) in actual.faces.iter().zip(&expected.faces) {
        let id = want.face_id;
        assert_eq!(face.face_id, id);
        assert_eq!(
            bits(&face.rx_frequency_ranges),
            bits(&want.rx_frequency_ranges),
            "{id} rx"
        );
        assert_eq!(
            bits(&face.tx_frequency_ranges),
            bits(&want.tx_frequency_ranges),
            "{id} tx"
        );
        assert_eq!(
            bits(&face.sample_frequency_ranges),
            bits(&want.sample_frequency_ranges),
            "{id} sample"
        );
        assert_eq!(face, want, "face {id}");
    }
    assert_eq!(actual, expected);
}

#[test]
fn open_then_explicit_close_shuts_down_once_destroys_then_unloads() {
    let _guard = lock();
    let log = LifetimeLog::new();
    assert!(!provider_loaded());
    let mut data = open("success");
    let initial = observe().expect("provider loaded while RF Data is open");
    assert_eq!((initial.shutdowns, initial.forbidden), (0, 0));
    assert_eq!(log.contents(), OK_OPEN);

    data.close().expect("explicit close");
    assert!(!data.is_open());
    // Exact order: one shutdown, DataMEL destruction, then DSO unload.
    assert_eq!(log.contents(), format!("{OK_OPEN}{OK_CLOSE}"));
    assert!(!provider_loaded());

    // Repeated Close and Drop after Close are harmless and call nothing.
    data.close().expect("repeated close");
    data.close().expect("third close");
    drop(data);
    assert_eq!(log.contents(), format!("{OK_OPEN}{OK_CLOSE}"));
    assert_eq!(count(&log.contents(), "rf_shutdown"), 1);
    log.assert_no_violations();
}

#[test]
fn drop_without_close_performs_the_same_ordered_cleanup() {
    let _guard = lock();
    let log = LifetimeLog::new();
    {
        let mut data = open("success");
        data.quantize_duration(5).expect("live owner");
        assert_eq!(observe().expect("loaded").shutdowns, 0);
    }
    assert_eq!(log.contents(), format!("{OK_OPEN}{OK_CLOSE}"));
    assert!(!provider_loaded());
    log.assert_no_violations();
}

#[test]
fn repeated_open_close_cycles_reload_and_unload_cleanly() {
    let _guard = lock();
    for cycle in 0..10 {
        let log = LifetimeLog::new();
        let mut data = open("success");
        assert!(provider_loaded(), "cycle {cycle}");
        if cycle % 2 == 0 {
            data.close().expect("close");
        } else {
            drop(data);
        }
        assert_eq!(
            log.contents(),
            format!("{OK_OPEN}{OK_CLOSE}"),
            "cycle {cycle}"
        );
        assert!(!provider_loaded(), "cycle {cycle}");
    }
}

#[test]
fn every_operation_after_close_fails_without_a_provider_call() {
    let _guard = lock();
    let log = LifetimeLog::new();
    let mut data = open("success");
    data.close().expect("close");
    let closed = log.contents();
    assert_eq!(closed, format!("{OK_OPEN}{OK_CLOSE}"));

    let version = data.provider_version().expect_err("version after close");
    let quantize = data
        .quantize_duration(19)
        .expect_err("quantize after close");
    let snapshot = data.mfa_info().expect_err("snapshot after close");
    for error in [version, quantize, snapshot] {
        assert_eq!(error.kind(), &ErrorKind::ProviderFailed);
        assert_eq!(error.diagnostic(), Some("RF Data is closed"));
        assert_eq!(error.diagnostic_required(), None);
    }
    assert!(!data.is_open());
    // No provider was loaded or called: the log is unchanged.
    assert_eq!(log.contents(), closed);
    assert!(!provider_loaded());
}

#[test]
fn missing_library_and_missing_factory_keep_distinct_categories() {
    let _guard = lock();
    let log = LifetimeLog::new();
    let missing =
        RfData::open("/definitely/missing/librfmel.so", "success").expect_err("missing library");
    assert_eq!(missing.kind(), &ErrorKind::LibraryLoadFailed);
    assert!(missing.diagnostic().is_some_and(|text| !text.is_empty()));
    assert!(missing.diagnostic_required().is_some());

    let symbol = RfData::open(missing_symbol_provider(), "success").expect_err("missing factory");
    assert_eq!(symbol.kind(), &ErrorKind::SymbolNotFound);
    assert!(symbol
        .diagnostic()
        .is_some_and(|text| text.contains("createDataMEL")));
    assert!(!loaded(&missing_symbol_provider()));
    assert_eq!(log.contents(), "");
}

#[test]
fn factory_null_and_exceptions_keep_native_semantics_and_unload() {
    let _guard = lock();
    for (scenario, kind, diagnostic) in [
        (
            "factory-null",
            ErrorKind::FactoryFailed,
            "createDataMEL returned null",
        ),
        (
            "factory-throw",
            ErrorKind::ProviderException,
            "mock RF factory exception",
        ),
        (
            "factory-throw-unknown",
            ErrorKind::ProviderException,
            "unknown provider factory exception",
        ),
        (
            "factory-bad-alloc",
            ErrorKind::InternalError,
            "allocation failed",
        ),
    ] {
        let log = LifetimeLog::new();
        let error = RfData::open(mock_provider(), scenario).expect_err(scenario);
        assert_eq!(error.kind(), &kind, "{scenario}");
        assert_eq!(error.diagnostic(), Some(diagnostic), "{scenario}");
        assert_eq!(
            error.diagnostic_required(),
            Some(diagnostic.len() + 1),
            "{scenario}"
        );
        // Factory called once, nothing to shut down, library released.
        assert_eq!(log.contents(), FAILED_OPEN, "{scenario}");
        assert!(!provider_loaded(), "{scenario}");
    }
}

#[test]
fn embedded_nul_inputs_are_rejected_before_loading_the_provider() {
    let _guard = lock();
    let log = LifetimeLog::new();
    let configuration = RfData::open(mock_provider(), "succ\0ess").expect_err("configuration NUL");
    assert_eq!(configuration.kind(), &ErrorKind::InvalidArgument);
    assert_eq!(
        configuration.diagnostic(),
        Some("configuration contains an embedded NUL")
    );
    assert_eq!(configuration.diagnostic_required(), None);

    let mut path = mock_provider().into_os_string().into_vec();
    path.extend_from_slice(b"\0suffix");
    let library =
        RfData::open(PathBuf::from(OsString::from_vec(path)), "success").expect_err("path NUL");
    assert_eq!(library.kind(), &ErrorKind::InvalidArgument);
    assert_eq!(
        library.diagnostic(),
        Some("provider_library contains an embedded NUL")
    );
    assert_eq!(log.contents(), "");
    assert!(!provider_loaded());
}

#[test]
fn non_utf8_provider_path_is_rejected_before_loading_the_provider() {
    let _guard = lock();
    let log = LifetimeLog::new();
    let path = log
        .directory
        .join(OsString::from_vec(b"invalid-\xff-rf-provider.so".to_vec()));
    symlink(
        mock_provider().canonicalize().expect("mock provider path"),
        &path,
    )
    .expect("create non-UTF-8 provider symlink");
    let result = RfData::open(&path, "success");
    // Remove the symlink before any assertion so a failure cannot leak it.
    fs::remove_file(&path).expect("remove provider symlink");
    let error = result.expect_err("non-UTF-8 path");
    assert_eq!(error.kind(), &ErrorKind::InvalidArgument);
    assert_eq!(
        error.diagnostic(),
        Some("provider_library must be valid UTF-8")
    );
    assert_eq!(error.diagnostic_required(), None);
    assert_eq!(log.contents(), "");
    assert!(!provider_loaded());
}

#[test]
fn rf_data_is_independent_of_rf_admin_in_the_same_provider() {
    let _guard = lock();
    let log = LifetimeLog::new();
    let mut data = open("success");
    let mut admin = RfAdmin::open(mock_provider(), "admin:ok").expect("admin");
    assert!(admin.command_state(RfMfaState::Standby).expect("command"));
    // Closing RF Admin neither shuts down nor destroys the DataMEL, and the
    // provider stays loaded for the still-open RF Data owner.
    admin.close().expect("admin close");
    assert_eq!(count(&log.contents(), "rf_shutdown"), 0);
    assert!(provider_loaded());
    assert_eq!(data.quantize_duration(7).expect("still live"), 0);
    let version = data.provider_version().expect("still live");
    assert_eq!(version.vendor, "Mock RF µVendor");
    data.close().expect("data close");
    let contents = log.contents();
    assert_eq!(count(&contents, "rf_shutdown"), 1);
    assert_eq!(count(&contents, "rf_admin_shutdown"), 1);
    assert!(contents.ends_with("rf_data_destroyed\nrf_mfa_info_destroyed\nlibrary_unloaded\n"));
    assert!(!provider_loaded());
    log.assert_no_violations();
}

#[test]
fn provider_version_returns_complete_utf8_and_numeric_versions() {
    let _guard = lock();
    let log = LifetimeLog::new();
    let mut data = open("success");
    for _ in 0..2 {
        let version = data.provider_version().expect("provider version");
        assert_eq!(version.api_version, 0x0000_A5A5);
        assert_eq!(version.library_version, 0x5A5A_0000);
        assert_eq!(version.vendor, "Mock RF \u{b5}Vendor");
        assert_eq!(version.description, "Deterministic Task 033B RF DataMEL");
    }
    assert!(data.is_open());
    data.close().expect("close");
    assert_eq!(log.contents(), format!("{OK_OPEN}{OK_CLOSE}"));
    log.assert_no_violations();
}

#[test]
fn provider_version_copies_long_strings_without_truncation() {
    let _guard = lock();
    let log = LifetimeLog::new();
    let mut data = open("version-long");
    let version = data.provider_version().expect("long version");
    assert_eq!(version.api_version, 0xfedc_ba98);
    assert_eq!(version.library_version, 0x0123_4567);
    // 300 + 3 UTF-8 bytes and 1000 + 4 UTF-8 bytes: the native required
    // lengths (304 and 1005) include the terminal NUL.
    assert_eq!(version.vendor.len(), 303);
    assert_eq!(version.vendor, format!("{}\u{20ac}", "v".repeat(300)));
    assert_eq!(version.description.len(), 1004);
    assert_eq!(
        version.description,
        format!("{}\u{1f4e1}", "d".repeat(1000))
    );
    data.close().expect("close");
    assert_eq!(log.contents(), format!("{OK_OPEN}{OK_CLOSE}"));
}

#[test]
fn provider_version_failures_keep_categories_and_the_owner_open() {
    let _guard = lock();
    for (scenario, kind, diagnostic) in [
        (
            "version-invalid-utf8",
            ErrorKind::ProviderException,
            "provider version contains invalid UTF-8 or NUL",
        ),
        (
            "version-nul",
            ErrorKind::ProviderException,
            "provider version contains invalid UTF-8 or NUL",
        ),
        (
            "version-throw",
            ErrorKind::ProviderException,
            "mock RF version exception",
        ),
        (
            "version-throw-unknown",
            ErrorKind::ProviderException,
            "unknown provider exception",
        ),
        (
            "version-bad-alloc",
            ErrorKind::InternalError,
            "allocation failed",
        ),
    ] {
        let log = LifetimeLog::new();
        let mut data = open(scenario);
        let error = data.provider_version().expect_err(scenario);
        assert_eq!(error.kind(), &kind, "{scenario}");
        assert_eq!(error.diagnostic(), Some(diagnostic), "{scenario}");
        assert_eq!(
            error.diagnostic_required(),
            Some(diagnostic.len() + 1),
            "{scenario}"
        );
        // A contained failure leaves the owner open and usable.
        assert!(data.is_open(), "{scenario}");
        assert_eq!(data.quantize_duration(-19).expect("still usable"), -10);
        data.close().expect("close");
        assert_eq!(log.contents(), format!("{OK_OPEN}{OK_CLOSE}"), "{scenario}");
        log.assert_no_violations();
    }
}

#[test]
fn quantization_is_the_providers_live_signed_result() {
    let _guard = lock();
    let log = LifetimeLog::new();
    let mut data = open("quantize");
    let before = observe().expect("loaded").quantizes;
    // The mock truncates toward zero in units of ten femtoseconds.
    let cases: [(i64, i64); 15] = [
        (0, 0),
        (1, 0),
        (9, 0),
        (10, 10),
        (19, 10),
        (-1, 0),
        (-9, 0),
        (-10, -10),
        (-19, -10),
        (1_234_567_890_123_456_789, 1_234_567_890_123_456_780),
        (-1_234_567_890_123_456_789, -1_234_567_890_123_456_780),
        (i64::MAX, 9_223_372_036_854_775_800),
        (i64::MIN, -9_223_372_036_854_775_800),
        (i64::MAX - 1, 9_223_372_036_854_775_800),
        (i64::MIN + 1, -9_223_372_036_854_775_800),
    ];
    for (index, (input, expected)) in cases.into_iter().enumerate() {
        assert_eq!(
            data.quantize_duration(input).expect("quantize"),
            expected,
            "{input}"
        );
        // Exactly one live provider call per query: nothing computed in Rust.
        assert_eq!(
            observe().expect("loaded").quantizes,
            before + index as u32 + 1
        );
    }
    // The live answer is authoritative even though this scenario's snapshot
    // also reports a 10 fs scheduler resolution.
    let snapshot = data.mfa_info().expect("snapshot");
    assert_eq!(snapshot.scheduler_resolution_fs, 10);
    data.close().expect("close");
    assert_eq!(log.contents(), format!("{OK_OPEN}{OK_CLOSE}"));
    log.assert_no_violations();
}

#[test]
fn quantization_never_substitutes_the_snapshot_scheduler_resolution() {
    let _guard = lock();
    let log = LifetimeLog::new();
    // In this scenario the snapshot reports 12345 fs while the provider's
    // live rule still truncates toward zero in units of 10 fs. Only the live
    // provider answer may be returned; it is neither the input unchanged nor
    // a multiple of the snapshot resolution.
    let mut data = open("success");
    let resolution = data.mfa_info().expect("snapshot").scheduler_resolution_fs;
    assert_eq!(resolution, 12_345);
    for (input, expected) in [
        (12_349, 12_340),
        (-12_349, -12_340),
        (37_049, 37_040),
        (i64::MAX, 9_223_372_036_854_775_800),
    ] {
        let quantized = data.quantize_duration(input).expect("quantize");
        assert_eq!(quantized, expected, "{input}");
        assert_ne!(quantized, input, "{input}");
        assert_ne!(quantized % resolution, 0, "{input}");
    }
    data.close().expect("close");
    assert_eq!(log.contents(), format!("{OK_OPEN}{OK_CLOSE}"));
    log.assert_no_violations();
}

#[test]
fn quantization_failures_keep_categories_and_the_owner_open() {
    let _guard = lock();
    for (scenario, kind, diagnostic) in [
        (
            "quantize-throw",
            ErrorKind::ProviderException,
            "mock RF quantize exception",
        ),
        (
            "quantize-unknown",
            ErrorKind::ProviderException,
            "unknown provider quantizeDuration exception",
        ),
        (
            "quantize-alloc",
            ErrorKind::InternalError,
            "allocation failed",
        ),
    ] {
        let log = LifetimeLog::new();
        let mut data = open(scenario);
        let before = observe().expect("loaded").quantizes;
        for attempt in 1..=2 {
            let error = data.quantize_duration(-19).expect_err(scenario);
            assert_eq!(error.kind(), &kind, "{scenario}");
            assert_eq!(error.diagnostic(), Some(diagnostic), "{scenario}");
            assert_eq!(
                error.diagnostic_required(),
                Some(diagnostic.len() + 1),
                "{scenario}"
            );
            assert_eq!(observe().expect("loaded").quantizes, before + attempt);
        }
        assert!(data.is_open(), "{scenario}");
        // Other operations on the same owner still work.
        let version = data.provider_version().expect("version");
        assert_eq!(version.api_version, 0x0000_A5A5);
        data.close().expect("close");
        assert_eq!(log.contents(), format!("{OK_OPEN}{OK_CLOSE}"), "{scenario}");
        log.assert_no_violations();
    }
}

#[test]
fn snapshot_copies_every_field_of_faces_7_and_42_exactly() {
    let _guard = lock();
    let log = LifetimeLog::new();
    let mut data = open("success");
    let before = observe().expect("loaded");
    let info = data.mfa_info().expect("snapshot");
    let after = observe().expect("loaded");
    assert!(after.getters > before.getters);
    assert_eq!(after.forbidden, 0);
    assert_snapshot(&info, &expected_snapshot(2, false));

    // Non-contiguous provider IDs in provider (std::set) order, not indexes.
    let ids: Vec<u32> = info.faces.iter().map(|face| face.face_id).collect();
    assert_eq!(ids, [7, 42]);
    // Empty spans become empty vectors; multiple, fractional and inverted
    // ranges are kept exactly; signed extremes are untouched.
    assert!(info.faces[0].tx_frequency_ranges.is_empty());
    assert!(info.faces[1].rx_frequency_ranges.is_empty());
    let inverted = &info.faces[1].sample_frequency_ranges;
    assert!(inverted.iter().all(|range| range.min_hz > range.max_hz));
    assert_eq!(info.faces[1].max_job_request_lead_time_fs, i64::MAX);
    assert_eq!(info.faces[1].rx_rx_switching_time_fs, -42_007);

    // Reading the owned value makes no provider call.
    let clone = info.clone();
    assert_eq!(clone, info);
    assert_eq!(observe().expect("loaded").getters, after.getters);
    data.close().expect("close");
    assert_eq!(log.contents(), format!("{OK_OPEN}{OK_CLOSE}"));
    log.assert_no_violations();
}

#[test]
fn reported_face_count_is_preserved_independently_of_faces() {
    let _guard = lock();
    let log = LifetimeLog::new();
    // getNumFaces() == 5 while getFaceIDs() == {7, 42}: both preserved.
    let mut data = open("inconsistent-faces");
    let info = data.mfa_info().expect("snapshot");
    assert_eq!(info.reported_num_faces, 5);
    assert_eq!(info.faces.len(), 2);
    assert_snapshot(&info, &expected_snapshot(5, false));
    data.close().expect("close");
    log.assert_no_violations();
}

#[test]
fn unknown_future_data_format_is_preserved_exactly() {
    let _guard = lock();
    let log = LifetimeLog::new();
    let mut data = open("future-format");
    let info = data.mfa_info().expect("snapshot");
    assert_snapshot(&info, &expected_snapshot(2, true));
    assert_eq!(
        info.supported_data_formats[0],
        RfJobDataFormat::Unknown(0xF000_0001)
    );
    let raw: Vec<u32> = info
        .supported_data_formats
        .iter()
        .map(|format| format.as_raw())
        .collect();
    assert_eq!(raw, [0xF000_0001, 0, 3, 6, 13]);
    data.close().expect("close");
    log.assert_no_violations();
}

#[test]
fn two_snapshots_are_independent_owned_values() {
    let _guard = lock();
    let log = LifetimeLog::new();
    let mut data = open("success");
    let first = data.mfa_info().expect("first");
    let mut second = data.mfa_info().expect("second");
    assert_eq!(first, second);
    // Distinct storage: no shared native or Rust buffers.
    assert_ne!(first.faces.as_ptr(), second.faces.as_ptr());
    assert_ne!(
        first.faces[0].rx_frequency_ranges.as_ptr(),
        second.faces[0].rx_frequency_ranges.as_ptr()
    );
    second.faces[0].rx_frequency_ranges.clear();
    second
        .supported_data_formats
        .push(RfJobDataFormat::PdwType1);
    assert_snapshot(&first, &expected_snapshot(2, false));
    drop(second);
    assert_snapshot(&first, &expected_snapshot(2, false));
    data.close().expect("close");
    assert_snapshot(&first, &expected_snapshot(2, false));
    assert_eq!(log.contents(), format!("{OK_OPEN}{OK_CLOSE}"));
}

#[test]
fn snapshot_outlives_rf_close_and_actual_provider_unload() {
    let _guard = lock();
    let log = LifetimeLog::new();
    let info = {
        let mut data = open("success");
        let info = data.mfa_info().expect("snapshot");
        data.close().expect("close");
        info
    };
    // The DataMEL was destroyed and the provider DSO is really gone.
    assert_eq!(log.contents(), format!("{OK_OPEN}{OK_CLOSE}"));
    assert!(!provider_loaded());
    // The entire owned snapshot is still readable and exact.
    assert_snapshot(&info, &expected_snapshot(2, false));
    let moved = info;
    assert_snapshot(&moved, &expected_snapshot(2, false));
    drop(moved);
    // Dropping the owned snapshot touches nothing native.
    assert_eq!(log.contents(), format!("{OK_OPEN}{OK_CLOSE}"));
}

#[test]
fn snapshot_getter_failures_publish_nothing_and_the_owner_recovers() {
    let _guard = lock();
    for (scenario, diagnostic, recovers) in [
        ("mfa-throw-once", "mock RFMFAInfo exception", true),
        (
            "mfa-throw-unknown-once",
            "unknown provider RFMFAInfo exception",
            true,
        ),
        ("mfa-reference-throw", "mock getRFMFAInfo exception", false),
    ] {
        let log = LifetimeLog::new();
        let mut data = open(scenario);
        let error = data.mfa_info().expect_err(scenario);
        assert_eq!(error.kind(), &ErrorKind::ProviderException, "{scenario}");
        assert_eq!(error.diagnostic(), Some(diagnostic), "{scenario}");
        assert_eq!(
            error.diagnostic_required(),
            Some(diagnostic.len() + 1),
            "{scenario}"
        );
        assert!(data.is_open(), "{scenario}");
        // Other operations still work on the same owner.
        let version = data.provider_version().expect("version after failure");
        assert_eq!(version.api_version, 0x0000_A5A5, "{scenario}");
        if recovers {
            // One-shot failure: the next snapshot is complete.
            let info = data.mfa_info().expect("recovered snapshot");
            data.close().expect("close");
            assert_snapshot(&info, &expected_snapshot(2, false));
        } else {
            let again = data.mfa_info().expect_err("reference throws again");
            assert_eq!(again.kind(), &ErrorKind::ProviderException);
            data.close().expect("close");
        }
        assert_eq!(log.contents(), format!("{OK_OPEN}{OK_CLOSE}"), "{scenario}");
        assert!(!provider_loaded(), "{scenario}");
        log.assert_no_violations();
    }
}

/// Uses the test facade's existing `AMS_MEL_TEST_RF_ALLOCATION_FAILURE`
/// failpoints; not applicable to the production facade.
#[test]
fn failpoint_open_allocation_failure_exposes_no_owner() {
    let _guard = lock();
    let log = LifetimeLog::new();
    let failpoint = Failpoint::set("open");
    let error = RfData::open(mock_provider(), "success").expect_err("open allocation");
    drop(failpoint);
    assert_eq!(error.kind(), &ErrorKind::InternalError);
    assert_eq!(error.diagnostic(), Some("allocation failed"));
    // Failed before the factory: no provider object and the DSO is unloaded.
    assert_eq!(log.contents(), "library_unloaded\n");
    assert!(!provider_loaded());
}

/// Uses the test facade's existing snapshot allocation failpoints. Native
/// snapshot allocation failure publishes nothing, and the owner stays usable.
#[test]
fn failpoint_snapshot_allocation_failures_publish_nothing() {
    let _guard = lock();
    let log = LifetimeLog::new();
    let mut data = open("success");
    for failure in ["snapshot-owner", "snapshot-range"] {
        let failpoint = Failpoint::set(failure);
        let error = data.mfa_info().expect_err(failure);
        drop(failpoint);
        assert_eq!(error.kind(), &ErrorKind::InternalError, "{failure}");
        assert_eq!(error.diagnostic(), Some("allocation failed"), "{failure}");
        assert!(data.is_open(), "{failure}");
    }
    let info = data.mfa_info().expect("snapshot after allocation failures");
    data.close().expect("close");
    assert_snapshot(&info, &expected_snapshot(2, false));
    assert_eq!(log.contents(), format!("{OK_OPEN}{OK_CLOSE}"));
    assert!(!provider_loaded());
    log.assert_no_violations();
}

/// Runs one uncertain-shutdown case in a fresh process. A throwing shutdown
/// permanently retains the mock DSO in whichever process performed it, so it
/// must never happen in this test executable's own process.
fn run_uncertain_shutdown_child(case: &str) {
    let output = Command::new(env::current_exe().expect("test executable"))
        .args([
            "--exact",
            "uncertain_shutdown_child_entry",
            "--ignored",
            "--nocapture",
            "--test-threads=1",
        ])
        .env(CHILD_ENV, case)
        .env_remove("AMS_MEL_TEST_LIFETIME_LOG")
        .env_remove(FAILPOINT_ENV)
        .output()
        .expect("spawn uncertain-shutdown child");
    let stdout = String::from_utf8_lossy(&output.stdout);
    let stderr = String::from_utf8_lossy(&output.stderr);
    assert!(
        output.status.success(),
        "child {case} failed\nstdout:\n{stdout}\nstderr:\n{stderr}"
    );
    assert!(
        stdout.contains(&format!("{CHILD_DONE}:{case}")),
        "child {case} did not run its case\nstdout:\n{stdout}\nstderr:\n{stderr}"
    );
}

#[test]
fn shutdown_exception_on_close_consumes_owner_without_second_shutdown() {
    let _guard = lock();
    run_uncertain_shutdown_child("close");
}

#[test]
fn unknown_shutdown_exception_on_close_consumes_owner_without_second_shutdown() {
    let _guard = lock();
    run_uncertain_shutdown_child("close-unknown");
}

#[test]
fn shutdown_exception_during_drop_does_not_panic_or_shut_down_twice() {
    let _guard = lock();
    run_uncertain_shutdown_child("drop");
}

/// Child-process entry for the three tests above; it does nothing unless they
/// select a case. Ignored so it never runs inside this executable's process.
#[test]
#[ignore = "child-process entry, run only by the uncertain-shutdown tests"]
fn uncertain_shutdown_child_entry() {
    let Some(case) = env::var_os(CHILD_ENV) else {
        return;
    };
    let case = case.into_string().expect("child case");
    let log = LifetimeLog::new();
    assert!(!provider_loaded());
    let (scenario, diagnostic) = match case.as_str() {
        "close-unknown" => (
            "shutdown-throw-unknown",
            "unknown provider shutdown exception",
        ),
        _ => ("shutdown-throw", "mock RF shutdown exception"),
    };
    let mut data = open(scenario);
    let info = data.mfa_info().expect("snapshot before close");
    assert_eq!(log.contents(), OK_OPEN);
    let retained = format!("{OK_OPEN}rf_shutdown\n");

    match case.as_str() {
        "close" | "close-unknown" => {
            let error = data.close().expect_err("shutdown exception");
            assert_eq!(error.kind(), &ErrorKind::ProviderException);
            assert_eq!(error.diagnostic(), Some(diagnostic));
            assert_eq!(error.diagnostic_required(), Some(diagnostic.len() + 1));
            // The public owner is consumed despite the exception.
            assert!(!data.is_open());
            assert_eq!(observe().expect("retained").shutdowns, 1);

            // Neither repeated Close, an operation, nor Drop reaches the
            // provider.
            data.close().expect("repeated close after failed shutdown");
            for error in [
                data.quantize_duration(10).expect_err("quantize"),
                data.mfa_info().expect_err("snapshot"),
                data.provider_version().expect_err("version"),
            ] {
                assert_eq!(error.kind(), &ErrorKind::ProviderFailed);
            }
            drop(data);
        }
        "drop" => drop(data),
        other => panic!("unknown child case {other}"),
    }

    let observed = observe().expect("uncertain provider graph remains retained");
    assert_eq!((observed.shutdowns, observed.forbidden), (1, 0));
    // Exactly one shutdown; the uncertain DataMEL was neither destroyed nor
    // unloaded, and nothing was retried.
    assert_eq!(log.contents(), retained);
    assert!(provider_loaded());
    log.assert_no_violations();
    // The owned snapshot never depended on the provider graph.
    assert_snapshot(&info, &expected_snapshot(2, false));
    println!("{CHILD_DONE}:{case}");
}
