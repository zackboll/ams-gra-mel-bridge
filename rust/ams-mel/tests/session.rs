use std::env;
#[cfg(unix)]
use std::ffi::OsString;
use std::fs;
#[cfg(unix)]
use std::os::unix::ffi::OsStringExt;
#[cfg(unix)]
use std::os::unix::fs::symlink;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Mutex;

use ams_mel::{abi_version, ErrorKind, Session};

static NEXT_LOG: AtomicU64 = AtomicU64::new(0);
static PROVIDER_TEST: Mutex<()> = Mutex::new(());

#[test]
fn reports_facade_version() {
    let version = abi_version().expect("ABI query succeeds");
    assert_eq!((version.major, version.minor), (0, 1));
}

#[test]
fn opens_queries_exact_version_and_closes_explicitly() {
    let _guard = PROVIDER_TEST.lock().expect("provider test lock");
    let log = LifetimeLog::new();
    let session = Session::open(mock_provider(), "success", "aperture-A").expect("open");
    let version = session.provider_version().expect("provider version");
    assert_eq!(version.api_version, 0x1234_5678);
    assert_eq!(version.library_version, 0x90ab_cdef);
    assert_eq!(version.vendor, "Mock IR Provider µ");
    assert_eq!(version.description, "Deterministic task 001 provider");
    session.close().expect("explicit close");
    assert_eq!(log.contents(), success_lifecycle());
}

#[test]
fn drop_closes_and_unloads_provider() {
    let _guard = PROVIDER_TEST.lock().expect("provider test lock");
    let log = LifetimeLog::new();
    {
        let _session = Session::open(mock_provider(), "success", "aperture-A").expect("open");
    }
    assert_eq!(log.contents(), success_lifecycle());
}

#[test]
fn preserves_open_failure_kinds_and_full_diagnostics() {
    let _guard = PROVIDER_TEST.lock().expect("provider test lock");
    let missing = Session::open("/definitely/missing/libirmel.so", "x", "aperture-A")
        .expect_err("missing provider fails");
    assert_eq!(missing.kind(), &ErrorKind::LibraryLoadFailed);
    assert!(missing.diagnostic().is_some_and(|text| !text.is_empty()));

    let symbol = Session::open(missing_symbol_provider(), "x", "aperture-A")
        .expect_err("missing symbol fails");
    assert_eq!(symbol.kind(), &ErrorKind::SymbolNotFound);
    assert!(symbol
        .diagnostic()
        .is_some_and(|text| text.contains("getControl")));

    let init = Session::open(mock_provider(), "init-fail", "aperture-A")
        .expect_err("initialization failure");
    assert_eq!(init.kind(), &ErrorKind::InitializationFailed);
    assert_eq!(init.diagnostic(), Some("Control::init failed"));

    let long = Session::open(mock_provider(), "throw-version-utf8", "aperture-A")
        .expect("open")
        .provider_version()
        .expect_err("version exception");
    assert_eq!(long.kind(), &ErrorKind::ProviderException);
    assert_eq!(long.diagnostic(), Some("mock µ exception"));
}

#[test]
fn rejects_embedded_nul_before_native_call() {
    let _guard = PROVIDER_TEST.lock().expect("provider test lock");
    let log = LifetimeLog::new();
    let error = Session::open(mock_provider(), "bad\0instance", "aperture-A")
        .expect_err("embedded NUL rejected");
    assert_eq!(error.kind(), &ErrorKind::InvalidArgument);
    assert_eq!(
        error.diagnostic(),
        Some("instance contains an embedded NUL")
    );
    assert_eq!(log.contents(), "");
}

#[test]
fn rejects_invalid_provider_version_utf8() {
    let _guard = PROVIDER_TEST.lock().expect("provider test lock");
    let session =
        Session::open(mock_provider(), "invalid-utf8-version", "aperture-A").expect("open");
    let error = session.provider_version().expect_err("invalid UTF-8 fails");
    assert_eq!(error.kind(), &ErrorKind::ProviderException);
    assert_eq!(
        error.diagnostic(),
        Some("provider version contains invalid UTF-8 or NUL")
    );
}

#[test]
fn preserves_status_and_required_size_for_oversized_diagnostic() {
    let _guard = PROVIDER_TEST.lock().expect("provider test lock");
    let error = Session::open(mock_provider(), "throw-version-oversized", "aperture-A")
        .expect("open")
        .provider_version()
        .expect_err("oversized provider exception");
    assert_eq!(error.kind(), &ErrorKind::ProviderException);
    assert_eq!(error.diagnostic(), None);
    assert_eq!(error.diagnostic_required(), Some(5003));
}

#[cfg(unix)]
#[test]
fn rejects_non_utf8_provider_path_before_native_call() {
    let _guard = PROVIDER_TEST.lock().expect("provider test lock");
    let log = LifetimeLog::new();
    let path = log
        .directory
        .join(OsString::from_vec(b"invalid-\xff-provider.so".to_vec()));
    symlink(
        mock_provider().canonicalize().expect("mock provider path"),
        &path,
    )
    .expect("create non-UTF-8 provider symlink");
    let error = Session::open(&path, "success", "aperture-A")
        .expect_err("non-UTF-8 provider path rejected");
    assert_eq!(error.kind(), &ErrorKind::InvalidArgument);
    assert_eq!(
        error.diagnostic(),
        Some("provider_library must be valid UTF-8")
    );
    assert_eq!(error.diagnostic_required(), None);
    assert_eq!(log.contents(), "");
    fs::remove_file(path).expect("remove provider symlink");
}

fn success_lifecycle() -> &'static str {
    "manager_factory_called\ncontrol_factory_called\ninit_called\n\
     control_destroyed\nmanager_destroyed\nlibrary_unloaded\n"
}

fn mock_provider() -> PathBuf {
    provider_path("libmock_ir_provider.so")
}

fn missing_symbol_provider() -> PathBuf {
    provider_path("libmissing_symbol_ir_provider.so")
}

fn provider_path(name: &str) -> PathBuf {
    let repository = Path::new(env!("CARGO_MANIFEST_DIR")).join("../..");
    env::var_os("AMS_MEL_TEST_PROVIDER_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| repository.join("native/build-tests/test-providers"))
        .join(name)
}

struct LifetimeLog {
    directory: PathBuf,
    path: PathBuf,
}

impl LifetimeLog {
    fn new() -> Self {
        let sequence = NEXT_LOG.fetch_add(1, Ordering::Relaxed);
        let directory =
            env::temp_dir().join(format!("ams-mel-rust-{}-{sequence}", std::process::id()));
        fs::create_dir(&directory).expect("create lifetime directory");
        let path = directory.join("lifetime.log");
        env::set_var("AMS_MEL_TEST_LIFETIME_LOG", &path);
        Self { directory, path }
    }

    fn contents(&self) -> String {
        fs::read_to_string(&self.path).unwrap_or_default()
    }
}

impl Drop for LifetimeLog {
    fn drop(&mut self) {
        env::remove_var("AMS_MEL_TEST_LIFETIME_LOG");
        let _ = fs::remove_file(&self.path);
        let _ = fs::remove_dir(&self.directory);
    }
}
