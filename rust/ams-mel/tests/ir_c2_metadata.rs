//! Child processes isolate existing mock-provider barriers, failpoints and logs.
//! No test-only native symbol is linked by this integration executable.
use ams_mel::*;
use std::{env, fs, path::PathBuf, process::Command};

const CHILD: &str = "AMS_MEL_RUST_C2_METADATA_CHILD";
const FAILURE: &str = "AMS_MEL_TEST_METADATA_CALLBACK_FAILURE";
const BARRIER: &str = "AMS_MEL_TEST_METADATA_CALLBACK_BARRIER";

fn provider() -> PathBuf {
    env::var_os("AMS_MEL_TEST_PROVIDER_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| {
            PathBuf::from(env!("CARGO_MANIFEST_DIR"))
                .join("../../native/build-tests/test-providers")
        })
        .join("libmock_ir_provider.so")
}

fn config() -> ControlConfig {
    ControlConfig::new(
        UciId::new([0; 16], "C2").unwrap(),
        UciId::new([1; 16], "platform").unwrap(),
        ComponentLocation::new(0.0, 0.0, 0.0, "key", "system").unwrap(),
    )
}

fn mock_id(seed: u8, label: &str) -> UciId {
    UciId::new(
        std::array::from_fn(|i| seed.wrapping_add((i as u8).wrapping_mul(7))),
        label,
    )
    .unwrap()
}

fn configuration() -> C2MetadataEvent {
    C2MetadataEvent::BitConfiguration(BitConfiguration {
        bit_types: vec![
            BitTypeDefinition {
                bit_id: mock_id(0x80, "BIT-α"),
                accepted_interface: BitControlInterface::SubsystemBitCommand,
                bit_item_names: vec!["sensor".into(), "optical-€".into()],
                subsystem_component_ids: vec![
                    mock_id(0x90, "component one"),
                    mock_id(0xa0, "component-β"),
                ],
                expected_duration_ns: 123456789,
            },
            BitTypeDefinition {
                bit_id: mock_id(0xf0, "startup"),
                accepted_interface: BitControlInterface::SubsystemInitiated,
                bit_item_names: vec![],
                subsystem_component_ids: vec![],
                expected_duration_ns: 0,
            },
        ],
    })
}

fn status(id: u32, state: CommandState, reason: CannotComply, text: &str) -> C2MetadataEvent {
    C2MetadataEvent::CommandStatus(CommandStatus {
        command_id: id,
        state,
        reason_id: reason,
        reason_description: text.into(),
    })
}

fn counters(received: u64, dropped: u64, malformed: u64) -> C2MetadataCounters {
    C2MetadataCounters {
        events_received: received,
        events_dropped_queue_full: dropped,
        malformed_or_unsupported: malformed,
    }
}

fn long_description() -> String {
    format!("{}€{}", "x".repeat(510), "y".repeat(100))
}

fn run(case: &str) {
    let output = Command::new(env::current_exe().unwrap())
        .args([
            "--exact",
            "child_entry",
            "--ignored",
            "--nocapture",
            "--test-threads=1",
        ])
        .env(CHILD, case)
        .env_remove(FAILURE)
        .env_remove(BARRIER)
        .env_remove("AMS_MEL_TEST_LIFETIME_LOG")
        .output()
        .unwrap();
    assert!(
        output.status.success(),
        "{case}:\n{}\n{}",
        String::from_utf8_lossy(&output.stdout),
        String::from_utf8_lossy(&output.stderr)
    );
    assert!(String::from_utf8_lossy(&output.stdout).contains("C2_METADATA_CHILD_COMPLETE"));
}

#[test]
fn complete_families_individually_and_together() {
    for case in ["rich", "command-states", "comms-high", "four-families"] {
        run(case);
    }
}

#[test]
fn overflow_malformed_timeout_and_failure() {
    for case in ["overflow", "malformed", "timeout"] {
        run(case);
    }
}

/// Uses existing contract-facade callback fault injection, not private exports.
#[test]
fn native_callback_allocation_failure() {
    run("allocation");
}

#[test]
fn one_shot_base_and_optional_registration() {
    for case in [
        "register-fail",
        "comms-register-retain-fail",
        "zero-capacity",
        "enabled",
    ] {
        run(case);
    }
}

#[test]
fn parent_session_stream_first_and_drop_lifetimes() {
    for case in [
        "parent-first",
        "session-first",
        "stream-first",
        "drop",
        "disable-fail",
    ] {
        run(case);
    }
}

#[test]
fn callback_completion_precedes_provider_unload() {
    run("nonquiescing");
}

struct Scratch(PathBuf);
impl Scratch {
    fn new() -> Self {
        let dir = env::temp_dir().join(format!("ams-c2-metadata-{}", std::process::id()));
        fs::create_dir(&dir).unwrap();
        env::set_var("AMS_MEL_TEST_LIFETIME_LOG", dir.join("lifetime"));
        Self(dir)
    }
    fn events(&self) -> String {
        fs::read_to_string(self.0.join("lifetime")).unwrap_or_default()
    }
    fn order(&self, expected: &[&str]) {
        let events = self.events();
        let positions: Vec<_> = expected
            .iter()
            .map(|line| {
                events
                    .lines()
                    .position(|s| s == *line)
                    .unwrap_or_else(|| panic!("missing {line}: {events}"))
            })
            .collect();
        assert!(positions.windows(2).all(|p| p[0] < p[1]), "{events}");
    }
}
impl Drop for Scratch {
    fn drop(&mut self) {
        env::remove_var(FAILURE);
        env::remove_var(BARRIER);
        env::remove_var("AMS_MEL_TEST_LIFETIME_LOG");
        let _ = fs::remove_dir_all(&self.0);
    }
}

#[test]
#[ignore]
fn child_entry() {
    let case = env::var(CHILD).expect("child case");
    let scratch = Scratch::new();
    if case == "nonquiescing" {
        env::set_var(BARRIER, scratch.0.join("barrier"));
        let session = Session::open(provider(), "metadata-nonquiescing-disable", "").unwrap();
        let mut c2 = session.open_control_channel(&config()).unwrap();
        let mut stream = c2.open_metadata(2).unwrap();
        c2.enable().unwrap();
        // Existing deterministic native barrier: Disable waits for callback
        // entry, destructor releases it and joins. No Rust sleeps or threads.
        c2.close().unwrap();
        stream.close().unwrap();
        session.close().unwrap();
        scratch.order(&[
            "metadata_callback_entered",
            "disable_returned_with_metadata_callback_active",
            "metadata_callback_returned",
            "c2_channel_destroyed",
            "library_unloaded",
        ]);
    } else if case == "register-fail" {
        let session = Session::open(provider(), "metadata-register-fail", "").unwrap();
        let mut c2 = session.open_control_channel(&config()).unwrap();
        assert_eq!(
            c2.open_metadata(2).unwrap_err().kind(),
            &ErrorKind::ProviderFailed
        );
        assert_eq!(
            c2.open_metadata(2).unwrap_err().kind(),
            &ErrorKind::InvalidArgument
        );
        c2.close().unwrap();
        session.close().unwrap();
        scratch.order(&[
            "retained_metadata_callback_invoked",
            "c2_channel_destroyed",
            "library_unloaded",
        ]);
    } else if case == "comms-register-retain-fail" {
        let session = Session::open(provider(), &case, "").unwrap();
        let mut c2 = session.open_control_channel(&config()).unwrap();
        let mut stream = c2.open_metadata(2).unwrap();
        assert_eq!(
            stream.enable_comms_test_events().unwrap_err().kind(),
            &ErrorKind::ProviderFailed
        );
        assert_eq!(
            stream.enable_comms_test_events().unwrap_err().kind(),
            &ErrorKind::InvalidArgument
        );
        stream.close().unwrap();
        c2.close().unwrap();
        session.close().unwrap();
        scratch.order(&[
            "retained_comms_callback_invoked",
            "c2_channel_destroyed",
            "library_unloaded",
        ]);
    } else {
        let scenario = match case.as_str() {
            "rich" | "four-families" | "parent-first" | "session-first" | "stream-first"
            | "drop" | "enabled" | "zero-capacity" => "metadata-rich",
            "overflow" | "allocation" => "metadata-overflow",
            "malformed" => "metadata-malformed",
            "command-states" => "metadata-command-states",
            "disable-fail" => "c2-disable-fail",
            _ => case.as_str(),
        };
        let session = Session::open(provider(), scenario, "").unwrap();
        let mut c2 = session.open_control_channel(&config()).unwrap();
        if case == "allocation" {
            env::set_var(FAILURE, "command-allocation");
        }
        if case == "enabled" {
            c2.enable().unwrap();
        }
        if case == "zero-capacity" {
            assert_eq!(
                c2.open_metadata(0).unwrap_err().kind(),
                &ErrorKind::InvalidArgument
            );
        }
        let mut stream = c2
            .open_metadata(if case == "overflow" { 1 } else { 8 })
            .unwrap();
        env::remove_var(FAILURE);
        assert!(stream.is_open());
        assert_eq!(
            c2.open_metadata(8).unwrap_err().kind(),
            &ErrorKind::InvalidArgument
        );
        let mut session = Some(session);
        match case.as_str() {
            "rich" | "four-families" | "parent-first" | "session-first" | "enabled"
            | "zero-capacity" => {
                if case == "session-first" {
                    session.take().unwrap().close().unwrap();
                }
                if case == "parent-first" {
                    c2.close().unwrap();
                }
                let configuration_value = stream.receive(0).unwrap();
                let bit_value = stream.receive(0).unwrap();
                assert_eq!(configuration_value, configuration());
                assert_eq!(bit_value, C2MetadataEvent::BitStatus(rich_bit_status()));
                assert_eq!(stream.counters().unwrap(), counters(2, 0, 0));
                if case == "rich" {
                    // Metadata Open must not implicitly enable C2.
                    assert_eq!(
                        c2.submit_operate(1).unwrap_err().kind(),
                        &ErrorKind::ProviderFailed
                    );
                }
                if case == "four-families" {
                    // Base Open did not register Comms; enabling does not send.
                    assert!(!scratch.events().contains("comms_sent"));
                    let mut view = c2.channel_view().unwrap();
                    let mut before = view.submit_comms_test(high()).unwrap();
                    before.wait(1000).unwrap();
                    assert_eq!(stream.receive(0).unwrap_err().kind(), &ErrorKind::Timeout);
                    stream.enable_comms_test_events().unwrap();
                    assert_eq!(
                        stream.enable_comms_test_events().unwrap_err().kind(),
                        &ErrorKind::InvalidArgument
                    );
                    c2.enable().unwrap();
                    let mut mode = c2.submit_operate(u32::MAX).unwrap();
                    mode.wait(1000).unwrap();
                    assert_eq!(
                        stream.receive(0).unwrap(),
                        status(u32::MAX, CommandState::Received, CannotComply::NotSet, "")
                    );
                    let mut comms = view.submit_comms_test(high()).unwrap();
                    let expected = high_report();
                    assert_eq!(
                        stream.receive(0).unwrap(),
                        C2MetadataEvent::CommsTest(expected)
                    );
                    assert_eq!(
                        comms.wait(1000).unwrap(),
                        CommsTestResult::Completed { report: expected }
                    );
                    assert_eq!(stream.counters().unwrap(), counters(4, 0, 0));
                }
                if case == "parent-first" {
                    assert_eq!(
                        stream.receive(0).unwrap_err().kind(),
                        &ErrorKind::StreamStopped
                    );
                    assert_eq!(
                        stream.enable_comms_test_events().unwrap_err().kind(),
                        &ErrorKind::ProviderFailed
                    );
                }
                // Fully owned graphs remain usable after all native owners close.
                stream.close().unwrap();
                c2.close().unwrap();
                if let Some(session) = session.take() {
                    session.close().unwrap();
                }
                scratch.order(&["c2_channel_destroyed", "library_unloaded"]);
                assert_eq!(configuration_value, configuration());
                assert_eq!(bit_value, C2MetadataEvent::BitStatus(rich_bit_status()));
            }
            "command-states" => {
                for (index, (state, reason, text)) in [
                    (CommandState::Accepted, CannotComply::NotSet, String::new()),
                    (
                        CommandState::Rejected,
                        CannotComply::InvalidInputParameter,
                        long_description(),
                    ),
                    (
                        CommandState::Cancelled,
                        CannotComply::Cancelled,
                        "cancelled".into(),
                    ),
                    (CommandState::Received, CannotComply::NotSet, String::new()),
                ]
                .into_iter()
                .enumerate()
                {
                    assert_eq!(
                        stream.receive(0).unwrap(),
                        status(0x80000001 + index as u32, state, reason, &text)
                    );
                }
                assert_eq!(stream.counters().unwrap(), counters(4, 0, 0));
            }
            "overflow" => {
                assert_eq!(stream.receive(0).unwrap(), configuration());
                assert_eq!(stream.counters().unwrap(), counters(3, 2, 0));
                assert_eq!(stream.receive(0).unwrap_err().kind(), &ErrorKind::Timeout);
            }
            "malformed" => {
                assert_eq!(
                    stream.receive(0).unwrap(),
                    status(5, CommandState::Cancelled, CannotComply::Cancelled, "valid")
                );
                assert_eq!(
                    stream.receive(0).unwrap(),
                    C2MetadataEvent::BitStatus(rich_bit_status())
                );
                assert_eq!(stream.counters().unwrap(), counters(6, 0, 4));
            }
            "allocation" => {
                assert_eq!(stream.receive(0).unwrap(), configuration());
                assert_eq!(
                    stream.receive(0).unwrap_err().kind(),
                    &ErrorKind::ProviderFailed
                );
                assert_eq!(stream.counters().unwrap(), counters(3, 0, 0));
            }
            "comms-high" => {
                assert_eq!(stream.receive(0).unwrap_err().kind(), &ErrorKind::Timeout);
                stream.enable_comms_test_events().unwrap();
                assert_eq!(
                    stream.enable_comms_test_events().unwrap_err().kind(),
                    &ErrorKind::InvalidArgument
                );
                assert!(!scratch.events().contains("comms_sent"));
                let mut view = c2.channel_view().unwrap();
                let mut request = view.submit_comms_test(high()).unwrap();
                // Closing the consumer neither cancels requests nor closes C2.
                assert_eq!(
                    stream.receive(0).unwrap(),
                    C2MetadataEvent::CommsTest(high_report())
                );
                stream.close().unwrap();
                assert!(c2.is_open());
                assert_eq!(
                    request.wait(1000).unwrap(),
                    CommsTestResult::Completed {
                        report: high_report()
                    }
                );
            }
            "stream-first" => {
                stream.close().unwrap();
                c2.enable().unwrap();
                let mut mode = c2.submit_operate(5).unwrap();
                mode.wait(1000).unwrap(); // late callback into inactive retained state
                mode.close().unwrap();
                assert!(c2.is_open());
            }
            "disable-fail" => {
                c2.enable().unwrap();
                assert_eq!(c2.close().unwrap_err().kind(), &ErrorKind::ProviderFailed);
                assert_eq!(
                    stream.receive(0).unwrap_err().kind(),
                    &ErrorKind::StreamStopped
                );
            }
            "drop" => {
                drop(stream);
                drop(c2);
                drop(session);
                scratch.order(&["c2_channel_destroyed", "library_unloaded"]);
                assert_eq!(
                    scratch
                        .events()
                        .lines()
                        .filter(|line| *line == "c2_channel_destroyed")
                        .count(),
                    1
                );
                println!("C2_METADATA_CHILD_COMPLETE");
                return;
            }
            "timeout" => {
                assert_eq!(stream.receive(0).unwrap_err().kind(), &ErrorKind::Timeout);
                assert_eq!(stream.receive(1).unwrap_err().kind(), &ErrorKind::Timeout);
            }
            _ => panic!("unknown case {case}"),
        }
        stream.close().unwrap();
        stream.close().unwrap();
        assert!(!stream.is_open());
        assert_eq!(
            stream.receive(0).unwrap_err().kind(),
            &ErrorKind::ProviderFailed
        );
        assert_eq!(
            stream.counters().unwrap_err().kind(),
            &ErrorKind::ProviderFailed
        );
        assert_eq!(
            stream.enable_comms_test_events().unwrap_err().kind(),
            &ErrorKind::ProviderFailed
        );
        c2.close().unwrap();
        if let Some(session) = session {
            session.close().unwrap();
        }
    }
    scratch.order(&["c2_channel_destroyed", "library_unloaded"]);
    println!("C2_METADATA_CHILD_COMPLETE");
}

fn high() -> CommsTestRequest {
    CommsTestRequest {
        command_id: 0x80000001,
        channel_id: 0xf0000002,
        request_id: 0xe0000003,
    }
}
fn high_report() -> CommsTestReport {
    CommsTestReport {
        command_id: 0x80000001,
        request_id: 0xe0000003,
    }
}
fn rich_bit_status() -> BitStatus {
    let item = |name: &str, result, reason: &str| CompletedBitItem {
        bit_item_name: name.into(),
        result,
        fail_reason: reason.into(),
    };
    let data = |key: &str, value: &str, format: &str, units: &str| FaultData {
        key: key.into(),
        value: value.into(),
        format: format.into(),
        units: units.into(),
    };
    BitStatus {
        active_bits: vec![
            ActiveBit {
                bit_id: mock_id(0x81, "active negative"),
                estimated_completion_time_ns: -5,
                estimated_percent_complete: 1.25,
            },
            ActiveBit {
                bit_id: mock_id(0x82, "active zero"),
                estimated_completion_time_ns: 0,
                estimated_percent_complete: 0.0,
            },
            ActiveBit {
                bit_id: mock_id(0x83, "active positive"),
                estimated_completion_time_ns: 987_654_321,
                estimated_percent_complete: 0.5,
            },
        ],
        completed_bits: vec![
            CompletedBit {
                bit_id: mock_id(0x91, "completed pass"),
                time_tag_ns: 42,
                result: BitResult::Pass,
                fail_reason: String::new(),
                bit_items: vec![
                    item("item pass", BitResult::Pass, ""),
                    item("item-\u{3b3}", BitResult::Fail, "item reason"),
                ],
            },
            CompletedBit {
                bit_id: mock_id(0x92, "completed fail"),
                time_tag_ns: -99,
                result: BitResult::Fail,
                fail_reason: "failure-\u{20ac}".into(),
                bit_items: Vec::new(),
            },
        ],
        faults: vec![Fault {
            fault_id: mock_id(0xd0, "fault-\u{3b6}"),
            severity: FaultSeverity::Warning,
            state: FaultState::Set,
            fault_data: vec![
                data("temperature", "101", "integer", "\u{b0}C"),
                data("phase-\u{3b4}", "bad", "text", ""),
            ],
            detection_time_ns: -1_234_567,
            fault_code: "F-42".into(),
            fault_description: "overheat-\u{20ac}".into(),
            component_ids: vec![
                mock_id(0xe1, "fault component one"),
                mock_id(0xe2, "fault component two"),
            ],
            ambiguity_groups: vec![
                FaultAmbiguityGroup {
                    diagnostic_test_ids: vec![
                        mock_id(0xb1, "diagnostic one"),
                        mock_id(0xb2, "diagnostic two"),
                    ],
                    component_ids: vec![
                        mock_id(0xc1, "ambiguous one"),
                        mock_id(0xc2, "ambiguous two"),
                    ],
                },
                FaultAmbiguityGroup {
                    diagnostic_test_ids: vec![mock_id(0xb3, "diagnostic three")],
                    component_ids: vec![mock_id(0xc3, "ambiguous three")],
                },
            ],
        }],
    }
}
