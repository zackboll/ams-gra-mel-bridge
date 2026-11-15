//! Compile the exact example's policy tests without adding a second app or FFI.
#[path = "../examples/minimal_ir_consumer.rs"]
mod application;

/// Optional real-provider executable qualification. Ordinary workspace tests
/// remain provider-free; this test never substitutes a mock for interoperability.
#[test]
#[ignore = "requires AMS_MEL_REFERENCE_PROVIDER and AMS_MEL_CONSUMER_EXE"]
fn reference_executable() {
    let exe = std::env::var_os("AMS_MEL_CONSUMER_EXE").expect("qualified example path");
    let provider = std::env::var_os("AMS_MEL_REFERENCE_PROVIDER").expect("reference provider path");
    let invoke = |args: &[&std::ffi::OsStr]| {
        std::process::Command::new(&exe)
            .args(args)
            .output()
            .unwrap()
    };
    assert_eq!(invoke(&["--help".as_ref()]).status.code(), Some(0));
    assert_eq!(invoke(&[]).status.code(), Some(2));
    assert_eq!(
        invoke(&[provider.as_ref(), "extra".as_ref()]).status.code(),
        Some(2)
    );
    let failed = invoke(&["/definitely/absent/provider.so".as_ref()]);
    assert_eq!(failed.status.code(), Some(1));
    assert!(String::from_utf8_lossy(&failed.stderr).contains("Session::open"));
    let output = invoke(&[provider.as_ref()]);
    assert_eq!(
        output.status.code(),
        Some(0),
        "stdout={}\nstderr={}",
        String::from_utf8_lossy(&output.stdout),
        String::from_utf8_lossy(&output.stderr)
    );
    let stdout = String::from_utf8(output.stdout).unwrap();
    assert_eq!(stdout.matches("actual C2 CommsTest metadata:").count(), 2);
    assert!(stdout.contains("Session explicitly closed"));
    assert!(!stdout.contains("[ERROR]"));
    assert!(output.stderr.is_empty());

    // Existing fixture tests only app-level profile rejection and Drop on that
    // early exit. This is NOT replacement evidence for the real provider run.
    if let Some(dir) = std::env::var_os("AMS_MEL_TEST_PROVIDER_DIR") {
        let log = std::env::temp_dir().join(format!(
            "minimal-consumer-cleanup-{}.log",
            std::process::id()
        ));
        let wrong = std::process::Command::new(&exe)
            .arg(std::path::Path::new(&dir).join("libmock_ir_provider.so"))
            .env("AMS_MEL_TEST_LIFETIME_LOG", &log)
            .output()
            .unwrap();
        assert_eq!(wrong.status.code(), Some(1));
        assert!(String::from_utf8_lossy(&wrong.stderr).contains("identity mismatch"));
        let events = std::fs::read_to_string(&log).unwrap();
        assert!(
            events.contains("control_destroyed\nmanager_destroyed\nlibrary_unloaded\n"),
            "{events}"
        );
        std::fs::remove_file(log).unwrap();
    }
}
