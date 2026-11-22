//! Only the mock controls use FFI; all consumer calls use the public safe API.
use ams_mel_ir::{
    BufferFlag, ChannelOperationError, ChannelView, ComponentLocation, ControlChannel,
    ControlConfig, Error, ErrorKind, HealthConfig, HealthStatusChannel, HostBuffer, ImageConfig,
    ImageControlConfig, ImageStream, Session, StackedChannel, StackedConfig, UciId,
};
use ams_mel_sys as sys;
use std::{
    cell::RefCell,
    env,
    ffi::{c_char, c_void, CString},
    fs,
    path::PathBuf,
    process::Command,
};

const CHILD: &str = "AMS_MEL_RUST020_CHILD";
const LOG: &str = "AMS_MEL_TEST_LIFETIME_LOG";
const BYTES: [u8; 4] = [0, 127, 128, 255];
const FLAGS: [BufferFlag; 3] = [BufferFlag::Host, BufferFlag::Gpu, BufferFlag::Host];
const FAMILIES: [&str; 4] = ["c2", "health", "stacked", "image"];

fn provider() -> PathBuf {
    env::var_os("AMS_MEL_TEST_PROVIDER_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| {
            PathBuf::from(env!("CARGO_MANIFEST_DIR"))
                .join("../../native/build-tests/test-providers")
        })
        .join("libmock_ir_provider.so")
}

fn session(instance: &str) -> Session {
    Session::open(provider(), instance, "").unwrap()
}

fn ids() -> (UciId, UciId, ComponentLocation) {
    (
        UciId::new([1; 16], "channel").unwrap(),
        UciId::new([2; 16], "platform").unwrap(),
        ComponentLocation::new(0.0, 0.0, 0.0, "sensor", "platform").unwrap(),
    )
}

enum Channel {
    C2(ControlChannel),
    Health(HealthStatusChannel),
    Stacked(StackedChannel),
    Image(ImageStream),
}

impl Channel {
    fn open(parent: &Session, family: &str) -> Self {
        let (id, platform, location) = ids();
        match family {
            "c2" => Self::C2(
                parent
                    .open_control_channel(&ControlConfig::new(id, platform, location))
                    .unwrap(),
            ),
            "health" => Self::Health(
                parent
                    .open_health_channel(&HealthConfig::new(id, platform, location))
                    .unwrap(),
            ),
            "stacked" => Self::Stacked(
                parent
                    .open_stacked_channel(&StackedConfig::new(id, platform, location))
                    .unwrap(),
            ),
            "image" => Self::Image(
                parent
                    .open_image_control(&ImageControlConfig::new(id, platform, location))
                    .unwrap(),
            ),
            _ => panic!("unknown family {family}"),
        }
    }

    fn view(&self) -> ChannelView {
        match self {
            Self::C2(c) => c.channel_view(),
            Self::Health(c) => c.channel_view(),
            Self::Stacked(c) => c.channel_view(),
            Self::Image(c) => c.channel_view(),
        }
        .unwrap()
    }

    fn enable(&mut self) -> Result<(), Error> {
        match self {
            Self::C2(c) => c.enable(),
            Self::Health(c) => c.enable(),
            Self::Stacked(c) => c.enable(),
            Self::Image(c) => c.start(),
        }
    }

    fn close(&mut self) -> Result<(), Error> {
        match self {
            Self::C2(c) => c.close(),
            Self::Health(c) => c.close(),
            Self::Stacked(c) => c.close(),
            Self::Image(c) => c.try_close(),
        }
    }
}

fn expected(result: Result<(), ChannelOperationError>, status: Option<i32>, value: Option<u32>) {
    let error = result.unwrap_err();
    let kind = if status == Some(sys::AMS_MEL_PROVIDER_EXCEPTION) {
        ErrorKind::ProviderException
    } else {
        ErrorKind::ProviderFailed
    };
    assert_eq!(error.error.kind(), &kind, "{error}");
    assert_eq!(error.native_status, status, "{error}");
    assert_eq!(error.provider_return, value, "{error}");
    assert!(!error.error.to_string().is_empty());
}

fn refused(result: Result<(), ChannelOperationError>) {
    expected(result, Some(sys::AMS_MEL_PROVIDER_FAILED), None);
}

fn blocked(buffer: &mut HostBuffer) {
    for result in [
        buffer.read_storage(0, 4).unwrap_err(),
        buffer.write_storage(0, &BYTES).unwrap_err(),
        buffer.info().unwrap_err(),
        buffer.set_flags(&FLAGS).unwrap_err(),
        buffer.add_flag(BufferFlag::Rdma).unwrap_err(),
        buffer.close().unwrap_err(),
    ] {
        assert_eq!(result.kind(), &ErrorKind::ProviderFailed);
    }
    let flags = buffer.flags().unwrap_err();
    assert_eq!(flags.error.kind(), &ErrorKind::ProviderFailed);
    assert_eq!(flags.native_status, Some(sys::AMS_MEL_PROVIDER_FAILED));
    assert_eq!(flags.provider_return, None);
    assert!(buffer.is_open());
}

fn unchanged(buffer: &HostBuffer) {
    assert_eq!(buffer.read_storage(12, BYTES.len()).unwrap(), BYTES);
    assert_eq!(buffer.flags().unwrap(), FLAGS);
    let info = buffer.info().unwrap();
    assert_eq!(
        (info.initialized_context, info.provider_context),
        (-20, -20)
    );
    assert_eq!((info.storage_size, info.provider_size), (16, 16));
    assert!(info.buffer_address_matches);
}

fn events() -> String {
    fs::read_to_string(env::var_os(LOG).unwrap()).unwrap()
}

fn count(name: &str) -> usize {
    events().lines().filter(|line| *line == name).count()
}

fn run(family: &str, case: &str) {
    let log = env::temp_dir().join(format!(
        "rust020-{}-{family}-{case}.log",
        std::process::id()
    ));
    fs::write(&log, "").unwrap();
    let output = Command::new(env::current_exe().unwrap())
        .args(["--exact", "mock_child", "--nocapture"])
        .env(CHILD, format!("{family}:{case}"))
        .env(LOG, &log)
        .output()
        .unwrap();
    assert!(
        output.status.success(),
        "{family}/{case}: {}\n{}\n{}",
        String::from_utf8_lossy(&output.stdout),
        String::from_utf8_lossy(&output.stderr),
        fs::read_to_string(&log).unwrap()
    );
    fs::remove_file(log).unwrap();
}

#[test]
fn four_family_lifecycles() {
    for family in FAMILIES {
        for case in [
            "normal",
            "attached",
            "parent-first",
            "post-disable",
            "reentry",
            "retain-unregistered",
            "drop",
        ] {
            run(family, case);
        }
    }
}

#[test]
fn provider_refusals_keep_the_original_association() {
    for family in FAMILIES {
        for case in [
            "register-fail",
            "register-unsupported",
            "register-throw",
            "register-fail-no-retain",
            "unregister-fail",
            "unregister-unsupported",
            "unregister-throw",
        ] {
            run(family, case);
        }
    }
}

#[test]
fn disable_failure_is_sticky_and_close_is_explicitly_retryable() {
    for family in FAMILIES {
        for case in ["disable-fail", "disable-refuse", "disable-throw"] {
            run(family, case);
        }
    }
    run("c2", "failed-detach");
    for family in FAMILIES {
        run(family, "disable-reentry");
    }
}

#[test]
fn acquisition_view_is_not_a_control_only_view() {
    run("image", "acquisition");
}

#[test]
fn mock_child() {
    let Ok(scenario) = env::var(CHILD) else {
        return;
    };
    let (family, case) = scenario.split_once(':').unwrap();
    if let Some(mode) = case.strip_prefix("disable-") {
        sticky_disable(family, mode);
    } else if case == "failed-detach" {
        sticky_disable(family, case);
    } else if case == "acquisition" {
        let parent = session("normal");
        let (id, platform, loc) = ids();
        let image = parent
            .open_image_stream(&ImageConfig::new(id, platform, loc))
            .unwrap();
        let mut view = image.channel_view().unwrap();
        let mut buffer = parent.open_host_buffer(16, -20).unwrap();
        refused(view.register_buffer(&mut buffer));
        refused(view.unregister_buffer(&mut buffer));
        refused(view.disable());
        buffer.close().unwrap();
        image.close().unwrap();
        view.close().unwrap();
        parent.close().unwrap();
    } else {
        registration(family, case);
    }
}

fn provider_failure(result: Result<(), ChannelOperationError>, case: &str) {
    let (status, value) = if case.ends_with("throw") {
        (sys::AMS_MEL_PROVIDER_EXCEPTION, None)
    } else if case.ends_with("unsupported") || case == "refuse" {
        (
            sys::AMS_MEL_PROVIDER_FAILED,
            Some(sys::AMS_MEL_IR_RETURN_NOT_SUPPORTED),
        )
    } else {
        (
            sys::AMS_MEL_PROVIDER_FAILED,
            Some(sys::AMS_MEL_IR_RETURN_FAIL),
        )
    };
    expected(result, Some(status), value);
}

fn sticky_disable(family: &str, mode: &str) {
    let parent = session(&format!("common-meta-disable019-{mode}"));
    let mut typed = Channel::open(&parent, family);
    let mut first = typed.view();
    let mut second = typed.view();
    first.disable().unwrap();
    second.disable().unwrap();
    assert_eq!(count("standalone_disable_entered"), 0);
    typed.enable().unwrap();
    if mode == "reentry" {
        let mut during = typed.view();
        let mock = Mock::open();
        mock.install(c"mock_disable_reentry", move || refused(during.disable()));
        first.disable().unwrap();
        assert_reentered();
        second.disable().unwrap();
        assert_eq!(count("standalone_disable_entered"), 1);
        drop(mock);
        typed.close().unwrap();
        parent.close().unwrap();
        assert_eq!(count("library_unloaded"), 1);
        return;
    }
    provider_failure(first.disable(), mode);
    refused(second.disable());
    assert!(typed.enable().is_err());
    assert_eq!(
        typed.close().unwrap_err().kind(),
        &ErrorKind::ProviderFailed
    );
    if mode == "failed-detach" {
        assert_eq!(count("channel_detach_failed"), 1);
        assert_eq!(count("channel_detached"), 0);
        // The second Close completes detach but reports the sticky Disable error.
        assert_eq!(
            typed.close().unwrap_err().kind(),
            &ErrorKind::ProviderFailed
        );
    }
    assert_eq!(count("standalone_disable_entered"), 1);
    assert_eq!(count("channel_detached"), 1);
    typed.close().unwrap();
    refused(first.disable());
    parent.close().unwrap();
    assert_eq!(count("library_unloaded"), 1);
}

fn registration(family: &str, case: &str) {
    let instance = format!("registration020-{case}");
    let mut parent = Some(session(&instance));
    let source = parent.as_ref().unwrap();
    let mut typed = Channel::open(source, family);
    let mut first = typed.view();
    let mut second = typed.view();
    let mut buffer = source.open_host_buffer(16, -20).unwrap();
    buffer.write_storage(12, &BYTES).unwrap();
    buffer.set_flags(&FLAGS).unwrap();
    first.disable().unwrap();
    refused(first.unregister_buffer(&mut buffer));
    if case == "normal" {
        let foreign = session(&instance);
        let mut foreign_buffer = foreign.open_host_buffer(16, -20).unwrap();
        refused(first.register_buffer(&mut foreign_buffer));
        refused(first.unregister_buffer(&mut foreign_buffer));
        foreign_buffer.close().unwrap();
        foreign.close().unwrap();
    }
    if case == "post-disable" {
        typed.enable().unwrap();
        refused(first.register_buffer(&mut buffer));
        assert_eq!(count("application_register_entered"), 0);
        first.disable().unwrap();
    }
    let mock = (case == "reentry").then(Mock::open);
    if case == "reentry" {
        let mut separate = source.open_host_buffer(16, -20).unwrap();
        let mut during = typed.view();
        mock.as_ref()
            .unwrap()
            .install(c"mock_registration_reentry", move || {
                refused(during.disable());
                refused(during.register_buffer(&mut separate));
                separate.close().unwrap();
            });
    }
    let registered = first.register_buffer(&mut buffer);
    if case == "reentry" {
        assert_reentered();
    }
    if case.starts_with("register-") {
        provider_failure(registered, case);
        assert!(typed.enable().is_err());
    } else {
        registered.unwrap();
    }
    blocked(&mut buffer);
    refused(first.register_buffer(&mut buffer));
    refused(second.register_buffer(&mut buffer));
    assert_eq!(count("application_register_entered"), 1);
    if case == "normal" || case.starts_with("register-") {
        let mut other = Channel::open(source, "c2");
        let mut wrong = other.view();
        refused(wrong.unregister_buffer(&mut buffer));
        refused(wrong.register_buffer(&mut buffer));
        assert_eq!(count("application_unregister_entered"), 0);
        other.close().unwrap();
    }
    assert_eq!(
        typed.close().unwrap_err().kind(),
        &ErrorKind::ProviderFailed
    );
    if let Channel::Image(image) = &mut typed {
        assert_eq!(image.stop().unwrap_err().kind(), &ErrorKind::ProviderFailed);
    }
    if case == "drop" {
        parent.take().unwrap().close().unwrap();
        drop(buffer);
        drop(typed);
        drop(first);
        drop(second);
        assert_eq!(count("application_unregister_entered"), 0);
        assert_eq!(count("foundation_buffer_destroyed_with_storage"), 0);
        assert_eq!(count("channel_detached"), 0);
        assert_eq!(count("library_unloaded"), 0);
        return;
    }
    if !case.starts_with("register-") && case != "attached" {
        for _ in 0..3 {
            typed.enable().unwrap();
            refused(second.unregister_buffer(&mut buffer));
            refused(first.register_buffer(&mut buffer));
            first.disable().unwrap();
            second.disable().unwrap();
        }
    }
    if matches!(case, "parent-first" | "retain-unregistered") {
        parent.take().unwrap().close().unwrap();
        assert_eq!(count("library_unloaded"), 0);
    }
    // Closing the original View does not affect the association.
    first.close().unwrap();
    expected(first.disable(), None, None);
    expected(first.register_buffer(&mut buffer), None, None);
    expected(first.unregister_buffer(&mut buffer), None, None);
    if case == "reentry" {
        let mut during = typed.view();
        mock.as_ref()
            .unwrap()
            .install(c"mock_registration_reentry", move || {
                refused(during.disable())
            });
    }
    let removed = second.unregister_buffer(&mut buffer);
    if case == "reentry" {
        assert_reentered();
    }
    if case.starts_with("unregister-") {
        provider_failure(removed, case);
        blocked(&mut buffer);
        assert!(typed.enable().is_err());
        assert_eq!(
            typed.close().unwrap_err().kind(),
            &ErrorKind::ProviderFailed
        );
        second.unregister_buffer(&mut buffer).unwrap();
    } else {
        removed.unwrap();
    }
    let attempts = if case.starts_with("unregister-") {
        2
    } else {
        1
    };
    assert_eq!(count("application_unregister_entered"), attempts);
    assert_eq!(count("application_identity_verified"), attempts);
    if case == "retain-unregistered" {
        blocked(&mut buffer);
        assert_eq!(
            typed.close().unwrap_err().kind(),
            &ErrorKind::ProviderFailed
        );
        assert_eq!(count("library_unloaded"), 0); // Parent closed; no test pin.
        Mock::open().relinquish();
    }
    drop(mock); // No test pin when measuring destruction/unload.
    assert_eq!(count("library_unloaded"), 0);
    unchanged(&buffer);
    refused(second.unregister_buffer(&mut buffer));
    let mut probe =
        (case == "normal").then(|| parent.as_ref().unwrap().open_host_buffer(16, -20).unwrap());
    buffer.close().unwrap();
    expected(second.register_buffer(&mut buffer), None, None);
    expected(second.unregister_buffer(&mut buffer), None, None);
    typed.close().unwrap();
    refused(second.disable());
    if let Some(probe) = &mut probe {
        refused(second.register_buffer(probe));
        refused(second.unregister_buffer(probe));
        probe.close().unwrap();
    }
    if let Some(parent) = parent {
        parent.close().unwrap();
    }
    let log = events();
    let mut last = 0;
    for event in [
        "foundation_buffer_destroyed_with_storage",
        "control_destroyed",
        "manager_destroyed",
        "library_unloaded",
    ] {
        let index = log
            .rfind(event)
            .unwrap_or_else(|| panic!("missing {event}: {log}"));
        assert!(index > last, "{log}");
        last = index;
    }
    assert_eq!(count("application_register_entered"), 1);
    assert_eq!(count("application_unregister_entered"), attempts);
    for forbidden in [
        "foundation_release_forbidden",
        "buffer_released",
        "buffer_registered",
        "frame_produced",
    ] {
        assert_eq!(count(forbidden), 0);
    }
    second.close().unwrap(); // weak view did not delay unload
}

// The hook is synchronous and one-shot. It reenters with independent safe
// wrappers and never aliases the &mut arguments of the outer operation.
thread_local! {
    static HOOK: RefCell<Option<Box<dyn FnOnce()>>> = const { RefCell::new(None) };
    static OUTCOME: RefCell<Option<std::thread::Result<()>>> = const { RefCell::new(None) };
}

extern "C" fn reenter() {
    let outcome = std::panic::catch_unwind(|| HOOK.take().unwrap()());
    OUTCOME.set(Some(outcome));
}

fn assert_reentered() {
    if let Err(panic) = OUTCOME.take().expect("mock did not call Rust hook") {
        std::panic::resume_unwind(panic);
    }
}

unsafe extern "C" {
    fn dlopen(path: *const c_char, flags: i32) -> *mut c_void;
    fn dlsym(handle: *mut c_void, name: *const c_char) -> *mut c_void;
    fn dlclose(handle: *mut c_void) -> i32;
}

struct Mock(*mut c_void);

impl Mock {
    fn open() -> Self {
        let path = CString::new(provider().as_os_str().as_encoded_bytes()).unwrap();
        // SAFETY: path is terminated; this test owns one DSO pin (RTLD_NOW).
        let pin = unsafe { dlopen(path.as_ptr(), 2) };
        assert!(!pin.is_null());
        Self(pin)
    }

    fn install(&self, name: &std::ffi::CStr, hook: impl FnOnce() + 'static) {
        HOOK.set(Some(Box::new(hook)));
        // SAFETY: exact signature of this pinned, test-only mock symbol.
        unsafe {
            let symbol = dlsym(self.0, name.as_ptr());
            assert!(!symbol.is_null());
            let install: unsafe extern "C" fn(extern "C" fn()) = std::mem::transmute(symbol);
            install(reenter);
        }
    }

    fn relinquish(&self) {
        // SAFETY: exact signature of this pinned, test-only mock symbol.
        unsafe {
            let symbol = dlsym(self.0, c"mock_registration_drop_retained".as_ptr());
            assert!(!symbol.is_null());
            let relinquish: unsafe extern "C" fn() = std::mem::transmute(symbol);
            relinquish();
        }
    }
}

impl Drop for Mock {
    fn drop(&mut self) {
        // SAFETY: closes the test's unique dlopen reference.
        assert_eq!(unsafe { dlclose(self.0) }, 0);
    }
}
