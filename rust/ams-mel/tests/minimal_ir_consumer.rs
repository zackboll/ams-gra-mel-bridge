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
    assert_eq!(stdout.matches("[OK] advertised[").count(), 4);
    assert_eq!(stdout.matches("[OK] attached identity/layout:").count(), 4);
    assert_eq!(
        stdout
            .matches("KeepAlive: Completed { value: Success }")
            .count(),
        4
    );
    for (operation, offset) in [("direct CommsTest", 0), ("C2-routed CommsTest", 10)] {
        assert_eq!(stdout.matches(&format!("[OK] {operation}:")).count(), 4);
        for (index, address) in [2, 4, 1, 6].into_iter().enumerate() {
            let command = 100 + index + offset;
            let request = 200 + index + offset;
            let destination = ["C2", "Health", "Image", "StackedImage"][index];
            assert!(stdout.contains(&format!(
                "[OK] {operation}: Completed {{ report: CommsTestReport {{ command_id: {command}, request_id: {request} }} }}, request=CommsTestRequest {{ command_id: {command}, channel_id: {address}, request_id: {request} }}, destination={destination}"
            )), "{stdout}");
            let path = if offset == 0 { "direct" } else { "C2-routed" };
            assert_eq!(stdout.matches(&format!(
                "[OK] actual {destination} CommsTest metadata ({path}): command_id={command} request_id={request}\n"
            )).count(), 1, "{stdout}");
        }
    }
    assert_eq!(stdout.matches("[OK] actual ").count(), 8);
    for destination in ["C2", "Health", "Image", "StackedImage"] {
        assert_eq!(
            stdout
                .matches(&format!("[OK] actual {destination} CommsTest metadata ("))
                .count(),
            2
        );
        assert_eq!(stdout.matches(&format!("[OK] {destination} CommsTest counters: events_received=2 events_dropped_queue_full=0 malformed_or_unsupported=0\n")).count(), 1);
        assert_eq!(
            stdout
                .matches(&format!(
                    "[OK] {destination} CommsTest queue empty (active Timeout)\n"
                ))
                .count(),
            1
        );
    }
    assert_eq!(stdout.matches("CommsTest counters:").count(), 4);
    assert!(stdout.contains("[OK] CommsTest parity: 8 correlated futures and 8 independently received callbacks across 4 channels"));
    assert!(stdout.contains("C2/Health/StackedImage enabled, Image started"));
    assert!(stdout.contains(
        "required metadata registered: C2=3, Health=6, Image=4; CommsTest=4 (dedicated C2 plus Health/Image/StackedImage common); total=17"
    ));
    assert!(stdout.contains("BIT_Command 300/NoOp: Completed { value: NotImplemented }"));
    assert!(stdout.contains(
        "ConfigSetCommand 301/zero timestamp/empty config: Completed { value: NotImplemented }"
    ));
    assert!(
        stdout.contains("ModeCmd 302/Standby/Unused/default scan: Rejected { code: InvalidState,")
    );
    assert!(
        stdout.contains("NavigationReport NotSet/neutral values: Rejected { code: InvalidState,")
    );
    assert!(stdout.contains("Image receive(0): established control-only error; no frame acquired"));
    assert!(stdout.contains("Image acquisition counters: frames_received=0 frames_dropped_queue_full=0 malformed_or_unsupported=0"));
    assert!(stdout.contains("all sensor metadata queues empty"));
    assert!(stdout.contains("StackedImage explicitly closed (typed disable/detach)"));
    assert!(stdout.contains("Session explicitly closed"));
    assert!(stdout.contains("four-channel reference-stub contract complete; no full C++ parity or hardware qualification"));
    assert!(!stdout.contains("advertised, not attached"));
    assert!(!stdout.contains("[OK] acquisition"));
    assert!(!stdout.contains("[ERROR]"));
    assert!(!stdout.contains("unexpected"));
    assert!(!stdout.contains("callbacks not exposed"));
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
