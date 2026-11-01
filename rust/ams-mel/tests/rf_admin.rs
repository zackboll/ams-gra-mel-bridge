//! RUST-MEL-001 safe RF Admin contract tests against the existing compiled
//! mock RF provider and missing-symbol RF provider.
//!
//! The mock's lifetime log and the `AMS_MEL_TEST_LIFETIME_LOG` variable are
//! process-global, so every test here is serialized. A throwing shutdown makes
//! the native bridge retain the mock DSO for the rest of the process; those
//! cases run only in a re-executed child process so that retention can never
//! contaminate the unload assertions in this executable.

use std::env;
use std::ffi::{c_char, c_int, c_void, CString, OsString};
use std::fs;
use std::os::unix::ffi::{OsStrExt, OsStringExt};
use std::os::unix::fs::symlink;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Mutex, MutexGuard, PoisonError};

use ams_mel::{ErrorKind, RfAdmin, RfMfaState};

static NEXT_LOG: AtomicU64 = AtomicU64::new(0);
static PROVIDER_TEST: Mutex<()> = Mutex::new(());

const CHILD_ENV: &str = "AMS_MEL_RUST_RF_ADMIN_CHILD";
const CHILD_DONE: &str = "RF_ADMIN_CHILD_COMPLETE";

const OK_OPEN: &str = "rf_admin_factory_called\n";
const OK_CLOSE: &str = "rf_admin_shutdown\nrf_admin_destroyed\nlibrary_unloaded\n";

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
    commands: u32,
    shutdowns: u32,
    last_state: u32,
}

/// Returns `None` if the mock RF provider is not currently loaded. Otherwise a
/// `RTLD_NOLOAD` reference is taken only for the duration of the read and is
/// released before returning, so it cannot delay a later unload.
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
        // `unsigned/uint32_t (void)` TEST-only export.
        unsafe {
            let address = dlsym(handle, symbol.as_ptr());
            assert!(!address.is_null(), "missing mock export {name}");
            let counter = std::mem::transmute::<*mut c_void, Counter>(address);
            counter()
        }
    };
    let observed = Observed {
        commands: read("mock_rf_admin_command_calls"),
        shutdowns: read("mock_rf_admin_shutdown_calls"),
        last_state: read("mock_rf_admin_last_state"),
    };
    // SAFETY: releases exactly the reference acquired above.
    assert_eq!(unsafe { dlclose(handle) }, 0);
    Some(observed)
}

fn provider_loaded() -> bool {
    observe().is_some()
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
            "ams-mel-rust-rf-admin-{}-{sequence}",
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
}

impl Drop for LifetimeLog {
    fn drop(&mut self) {
        env::remove_var("AMS_MEL_TEST_LIFETIME_LOG");
        let _ = fs::remove_file(&self.path);
        let _ = fs::remove_dir(&self.directory);
    }
}

fn count(log: &str, event: &str) -> usize {
    log.lines().filter(|line| *line == event).count()
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

fn command_events(raw: u32) -> String {
    match raw {
        6 | 8 | 14 => format!("rf_admin_command\nrf_admin_state_{raw}\n"),
        _ => "rf_admin_command\n".to_owned(),
    }
}

#[test]
fn commands_states_then_explicit_close_shuts_down_once_before_unload() {
    let _guard = lock();
    let log = LifetimeLog::new();
    assert!(!provider_loaded());
    let mut admin = RfAdmin::open(mock_provider(), "admin:ok").expect("open");
    assert!(admin.is_open());
    let initial = observe().expect("provider loaded while Admin is open");
    assert_eq!((initial.commands, initial.shutdowns), (0, 0));

    let mut expected = OK_OPEN.to_owned();
    for (index, state) in [
        RfMfaState::Standby,
        RfMfaState::OperateRxOnly,
        RfMfaState::Degraded,
    ]
    .into_iter()
    .enumerate()
    {
        assert!(admin.command_state(state).expect("command"));
        let observed = observe().expect("provider loaded");
        assert_eq!(observed.last_state, state.as_raw());
        assert_eq!(observed.commands, index as u32 + 1);
        assert_eq!(observed.shutdowns, 0);
        expected.push_str(&command_events(state.as_raw()));
    }
    assert_eq!(
        (
            RfMfaState::Standby.as_raw(),
            RfMfaState::OperateRxOnly.as_raw(),
            RfMfaState::Degraded.as_raw()
        ),
        (6, 8, 14)
    );
    assert_eq!(log.contents(), expected);

    admin.close().expect("explicit close");
    assert!(!admin.is_open());
    expected.push_str(OK_CLOSE);
    // Exact sequence: one shutdown, then Admin destruction, then DSO unload.
    assert_eq!(log.contents(), expected);
    assert!(!provider_loaded());

    // Repeated Close and Drop after Close are harmless and call nothing.
    admin.close().expect("repeated close");
    admin.close().expect("third close");
    drop(admin);
    assert_eq!(log.contents(), expected);
    assert_eq!(count(&log.contents(), "rf_admin_shutdown"), 1);
}

#[test]
fn drop_without_close_performs_the_same_ordered_cleanup() {
    let _guard = lock();
    let log = LifetimeLog::new();
    {
        let mut admin = RfAdmin::open(mock_provider(), "admin:ok").expect("open");
        assert!(admin.command_state(RfMfaState::Operate).expect("command"));
        assert_eq!(observe().expect("loaded").shutdowns, 0);
    }
    assert_eq!(
        log.contents(),
        format!("{OK_OPEN}{}{OK_CLOSE}", command_events(7))
    );
    assert!(!provider_loaded());
}

#[test]
fn command_after_close_fails_without_a_provider_call() {
    let _guard = lock();
    let log = LifetimeLog::new();
    let mut admin = RfAdmin::open(mock_provider(), "admin:ok").expect("open");
    admin.close().expect("close");
    let closed = log.contents();
    assert_eq!(closed, format!("{OK_OPEN}{OK_CLOSE}"));

    let error = admin
        .command_state(RfMfaState::Standby)
        .expect_err("command after close");
    assert_eq!(error.kind(), &ErrorKind::ProviderFailed);
    assert_eq!(error.diagnostic(), Some("RF Admin is closed"));
    assert!(!admin.is_open());
    // No provider was loaded or called: the log is unchanged.
    assert_eq!(log.contents(), closed);
    assert!(!provider_loaded());
}

#[test]
fn every_published_state_reaches_the_provider_with_its_exact_value() {
    let _guard = lock();
    let _log = LifetimeLog::new();
    let mut admin = RfAdmin::open(mock_provider(), "admin:ok").expect("open");
    for (index, state) in RfMfaState::ALL.into_iter().enumerate() {
        assert_eq!(state.as_raw(), index as u32);
        assert_eq!(RfMfaState::try_from(index as u32).expect("raw"), state);
        assert!(admin.command_state(state).expect("command"));
        let observed = observe().expect("loaded");
        assert_eq!(observed.last_state, index as u32);
        assert_eq!(observed.commands, index as u32 + 1);
    }
    for raw in [15, 16, u32::MAX] {
        let error = RfMfaState::try_from(raw).expect_err("unpublished state");
        assert_eq!(error.kind(), &ErrorKind::InvalidArgument);
    }
    // Rejected raw conversions never reached the provider.
    assert_eq!(observe().expect("loaded").commands, 15);
    admin.close().expect("close");
    assert!(!provider_loaded());
}

#[test]
fn provider_rejection_is_ok_false_not_an_error() {
    let _guard = lock();
    let log = LifetimeLog::new();
    let mut admin = RfAdmin::open(mock_provider(), "admin:reject").expect("open");
    assert!(!admin
        .command_state(RfMfaState::Operate)
        .expect("rejection is Ok"));
    assert!(!admin
        .command_state(RfMfaState::Standby)
        .expect("rejection is Ok"));
    let observed = observe().expect("loaded");
    assert_eq!((observed.commands, observed.last_state), (2, 6));
    assert!(admin.is_open());
    admin.close().expect("close");
    assert_eq!(
        log.contents(),
        format!(
            "{OK_OPEN}{}{}{OK_CLOSE}",
            command_events(7),
            command_events(6)
        )
    );
}

#[test]
fn missing_library_and_missing_factory_keep_distinct_categories() {
    let _guard = lock();
    let log = LifetimeLog::new();
    let missing =
        RfAdmin::open("/definitely/missing/librfmel.so", "admin:ok").expect_err("missing library");
    assert_eq!(missing.kind(), &ErrorKind::LibraryLoadFailed);
    assert!(missing.diagnostic().is_some_and(|text| !text.is_empty()));

    let symbol = RfAdmin::open(missing_symbol_provider(), "admin:ok").expect_err("missing factory");
    assert_eq!(symbol.kind(), &ErrorKind::SymbolNotFound);
    assert!(symbol
        .diagnostic()
        .is_some_and(|text| text.contains("createAdminMEL")));
    assert!(!loaded(&missing_symbol_provider()));
    assert_eq!(log.contents(), "");
}

#[test]
fn factory_null_and_exceptions_keep_native_semantics_and_unload() {
    let _guard = lock();
    for (scenario, kind, diagnostic) in [
        (
            "admin:factory-null",
            ErrorKind::FactoryFailed,
            "createAdminMEL returned null",
        ),
        (
            "admin:factory-throw",
            ErrorKind::ProviderException,
            "mock RF factory exception",
        ),
        (
            "admin:factory-throw-unknown",
            ErrorKind::ProviderException,
            "unknown provider factory exception",
        ),
    ] {
        let log = LifetimeLog::new();
        let error = RfAdmin::open(mock_provider(), scenario).expect_err(scenario);
        assert_eq!(error.kind(), &kind, "{scenario}");
        assert_eq!(error.diagnostic(), Some(diagnostic), "{scenario}");
        assert_eq!(
            error.diagnostic_required(),
            Some(diagnostic.len() + 1),
            "{scenario}"
        );
        // Factory called once, nothing to shut down, library released.
        assert_eq!(
            log.contents(),
            "rf_admin_factory_called\nlibrary_unloaded\n",
            "{scenario}"
        );
        assert!(!provider_loaded(), "{scenario}");
    }
}

#[test]
fn missing_uci_control_or_status_control_is_provider_failure() {
    let _guard = lock();
    for (scenario, diagnostic) in [
        ("admin:no-uci", "AdminMEL::getUCIControl returned null"),
        (
            "admin:no-status",
            "UCI_Control::getStatusControl returned null",
        ),
    ] {
        let log = LifetimeLog::new();
        let mut admin = RfAdmin::open(mock_provider(), scenario).expect(scenario);
        let before = observe().expect("loaded").commands;
        let error = admin
            .command_state(RfMfaState::Operate)
            .expect_err(scenario);
        assert_eq!(error.kind(), &ErrorKind::ProviderFailed, "{scenario}");
        assert_eq!(error.diagnostic(), Some(diagnostic), "{scenario}");
        // StatusControl::commandState was never reached.
        assert_eq!(observe().expect("loaded").commands, before, "{scenario}");
        assert!(admin.is_open(), "{scenario}");
        admin.close().expect("close");
        assert_eq!(log.contents(), format!("{OK_OPEN}{OK_CLOSE}"), "{scenario}");
    }
}

#[test]
fn command_exception_is_an_error_and_keeps_the_same_owner() {
    let _guard = lock();
    let log = LifetimeLog::new();
    let mut admin = RfAdmin::open(mock_provider(), "admin:command-throw").expect("open");
    let error = admin
        .command_state(RfMfaState::Standby)
        .expect_err("commandState exception");
    assert_eq!(error.kind(), &ErrorKind::ProviderException);
    assert_eq!(error.diagnostic(), Some("mock Admin command exception"));
    // No acceptance value is invented and the owner is not replaced: the same
    // provider object handles the next command and the single shutdown.
    assert!(admin.is_open());
    let second = admin
        .command_state(RfMfaState::Degraded)
        .expect_err("second exception");
    assert_eq!(second.kind(), &ErrorKind::ProviderException);
    assert_eq!(observe().expect("loaded").last_state, 14);
    admin.close().expect("close");
    assert_eq!(
        log.contents(),
        format!(
            "{OK_OPEN}{}{}{OK_CLOSE}",
            command_events(6),
            command_events(14)
        )
    );
    assert_eq!(count(&log.contents(), "rf_admin_factory_called"), 1);
}

#[test]
fn embedded_nul_inputs_are_rejected_before_loading_the_provider() {
    let _guard = lock();
    let log = LifetimeLog::new();
    let configuration =
        RfAdmin::open(mock_provider(), "admin:\0ok").expect_err("configuration NUL");
    assert_eq!(configuration.kind(), &ErrorKind::InvalidArgument);
    assert_eq!(
        configuration.diagnostic(),
        Some("configuration contains an embedded NUL")
    );
    assert_eq!(configuration.diagnostic_required(), None);

    let mut path = mock_provider().into_os_string().into_vec();
    path.extend_from_slice(b"\0suffix");
    let library =
        RfAdmin::open(PathBuf::from(OsString::from_vec(path)), "admin:ok").expect_err("path NUL");
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
    let result = RfAdmin::open(&path, "admin:ok");
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
fn shutdown_exception_during_drop_does_not_panic_or_shut_down_twice() {
    let _guard = lock();
    run_uncertain_shutdown_child("drop");
}

/// Child-process entry for the two tests above; it does nothing unless they
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
    let mut admin = RfAdmin::open(mock_provider(), "admin:shutdown-throw").expect("open");
    assert!(admin.command_state(RfMfaState::Standby).expect("command"));
    let before_close = format!("{OK_OPEN}{}", command_events(6));
    assert_eq!(log.contents(), before_close);
    let retained = format!("{before_close}rf_admin_shutdown\n");

    match case.as_str() {
        "close" => {
            let error = admin.close().expect_err("shutdown exception");
            assert_eq!(error.kind(), &ErrorKind::ProviderException);
            assert_eq!(error.diagnostic(), Some("mock Admin shutdown exception"));
            // The public owner is consumed despite the exception.
            assert!(!admin.is_open());
            assert_eq!(observe().expect("retained").shutdowns, 1);

            // Neither repeated Close, a command, nor Drop reaches the provider.
            admin.close().expect("repeated close after failed shutdown");
            let closed = admin
                .command_state(RfMfaState::Operate)
                .expect_err("command after failed shutdown");
            assert_eq!(closed.kind(), &ErrorKind::ProviderFailed);
            drop(admin);
        }
        "drop" => drop(admin),
        other => panic!("unknown child case {other}"),
    }

    let observed = observe().expect("uncertain provider graph remains retained");
    assert_eq!((observed.shutdowns, observed.commands), (1, 1));
    // Exactly one shutdown; the uncertain Admin was neither destroyed nor
    // unloaded, and nothing was retried.
    assert_eq!(log.contents(), retained);
    assert!(provider_loaded());
    println!("{CHILD_DONE}:{case}");
}
