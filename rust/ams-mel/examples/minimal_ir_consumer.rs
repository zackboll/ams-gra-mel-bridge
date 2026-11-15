//! Qualified minimal-mel-example revision-1 stub consumer, not hardware qualification.
#![forbid(unsafe_code)]

use ams_mel::*;
use std::{env, ffi::OsString, path::Path, process::ExitCode};

const USAGE: &str = "Usage: minimal_ir_consumer /absolute/path/to/libminimal_ir_mel_provider.so\nExercises only the minimal-mel-example API-4 revision-1 reference-stub profile.";
// Bounds future waits, not provider send/close calls. Timeout is NOT cancellation.
const WAIT_MS: u32 = 5_000;
const QUEUE_CAPACITY: usize = 16;
type Result<T> = std::result::Result<T, String>;

trait Operation<T> {
    fn at(self, operation: &str) -> Result<T>;
}
impl<T> Operation<T> for std::result::Result<T, Error> {
    fn at(self, operation: &str) -> Result<T> {
        self.map_err(|e| format!("{operation}: {e:?}"))
    }
}

fn registration_error(
    error: &Error,
    registration: Option<CommandReturn>,
    operation: &str,
) -> String {
    format!("{operation}: {error:?}; provider registration Return={registration:?}")
}

impl<T> Operation<T> for std::result::Result<T, CommsMetadataOpenError> {
    fn at(self, operation: &str) -> Result<T> {
        self.map_err(|e| registration_error(e.error(), e.registration(), operation))
    }
}

#[derive(Debug, PartialEq)]
enum Arguments {
    Help,
    Provider(OsString),
}

fn arguments(args: impl IntoIterator<Item = OsString>) -> Result<Arguments> {
    let mut args = args.into_iter();
    match (args.next(), args.next()) {
        (Some(path), None) if path == "--help" => Ok(Arguments::Help),
        (Some(path), None) => Ok(Arguments::Provider(path)),
        _ => Err(USAGE.into()),
    }
}

fn main() -> ExitCode {
    let path = match arguments(env::args_os().skip(1)) {
        Ok(Arguments::Help) => {
            println!("{USAGE}");
            return ExitCode::SUCCESS;
        }
        Ok(Arguments::Provider(path)) => path,
        Err(message) => {
            eprintln!("[ERROR] {message}");
            return ExitCode::from(2);
        }
    };
    match run(Path::new(&path)) {
        Ok(()) => ExitCode::SUCCESS,
        Err(message) => {
            eprintln!("[ERROR] {message}");
            eprintln!("[ERROR] Contract incomplete; Drop attempts best-effort cleanup, not a guarantee of provider unload.");
            ExitCode::FAILURE
        }
    }
}

fn require(condition: bool, message: impl Into<String>) -> Result<()> {
    if condition {
        Ok(())
    } else {
        Err(message.into())
    }
}

fn profile(version: &ProviderVersion) -> Result<()> {
    require(
        version.vendor == "minimal-mel-example"
            && version.api_version == 4
            && version.library_version == 1,
        format!("reference-stub identity mismatch: {version:?}"),
    )
}

const REQUIRED: [ChannelType; 4] = [
    ChannelType::CommandAndControl,
    ChannelType::HealthAndStatus,
    ChannelType::IrstImage,
    ChannelType::StackedImage,
];

fn zero_layout(cap: &ChannelCapability) -> Result<()> {
    require(
        [
            cap.width,
            cap.height,
            cap.bit_depth,
            cap.row_pitch,
            cap.buffer_size,
            cap.image_size,
            cap.number_of_bands,
        ]
        .iter()
        .all(|&v| v == 0),
        format!("expected unconfigured zero layout: {cap:?}"),
    )
}

fn select(caps: &[ChannelCapability], kind: ChannelType) -> Result<&ChannelCapability> {
    let mut matches = caps.iter().filter(|c| c.channel_types.contains(&kind));
    let selected = matches
        .next()
        .ok_or_else(|| format!("missing required channel {kind:?}"))?;
    require(
        matches.next().is_none(),
        format!("ambiguous required channel {kind:?}"),
    )?;
    Ok(selected)
}

fn discovery(caps: &[ChannelCapability]) -> Result<()> {
    // Check membership ambiguity before the count, for actionable diagnostics.
    for kind in REQUIRED {
        let cap = select(caps, kind)?;
        require(
            cap.channel_types == [kind],
            format!("unexpected reference channel types: {cap:?}"),
        )?;
        zero_layout(cap)?;
    }
    require(
        caps.len() == 4,
        "reference profile requires exactly four capabilities",
    )?;
    for (i, cap) in caps.iter().enumerate() {
        require(
            !caps[..i]
                .iter()
                .any(|other| other.channel_id.uuid() == cap.channel_id.uuid()),
            "reference capabilities reuse a channel UUID",
        )?;
    }
    Ok(())
}

fn attached(actual: &ChannelCapability, expected: &ChannelCapability) -> Result<()> {
    require(
        actual.channel_id == expected.channel_id
            && actual.platform_id == expected.platform_id
            && actual.sensor_location == expected.sensor_location
            && actual.channel_types == expected.channel_types,
        format!("attached identity differs from discovery: {actual:?}"),
    )?;
    zero_layout(actual)
}

fn stacked_config(capability: &ChannelCapability) -> StackedConfig {
    StackedConfig::new(
        capability.channel_id.clone(),
        capability.platform_id.clone(),
        capability.sensor_location.clone(),
    )
}

fn returned(result: &ReturnResult, expected: CommandReturn, operation: &str) -> Result<()> {
    require(
        *result == ReturnResult::Completed { value: expected },
        format!("{operation}: expected Completed({expected:?}), got {result:?}"),
    )
}

fn rejection(code: MelErrorCode, description: &str, operation: &str) -> Result<()> {
    require(code == MelErrorCode::InvalidState && !description.trim().is_empty(),
        format!("{operation}: expected InvalidState and provider description, got {code:?}: {description}"))
}

fn mode_result(result: &ModeResult) -> Result<()> {
    match result {
        ModeResult::Rejected { code, description } => rejection(*code, description, "ModeCmd"),
        _ => Err(format!(
            "ModeCmd: expected explicit rejection, got {result:?}"
        )),
    }
}

fn navigation_result(result: &NavigationResult) -> Result<()> {
    match result {
        NavigationResult::Rejected { code, description } => {
            rejection(*code, description, "NavigationReport")
        }
        _ => Err(format!(
            "NavigationReport: expected explicit rejection, got {result:?}"
        )),
    }
}

fn correlated(report: CommsTestReport, request: CommsTestRequest) -> Result<()> {
    require(
        report.command_id == request.command_id && report.request_id == request.request_id,
        format!("CommsTest correlation mismatch: request={request:?}, report={report:?}"),
    )
}

// ONLY this provider's documented local address convention. Never a UCI UUID conversion.
fn stub_address(kind: ChannelType) -> Result<u32> {
    match kind {
        ChannelType::CommandAndControl => Ok(2),
        ChannelType::HealthAndStatus => Ok(4),
        ChannelType::IrstImage => Ok(1),
        ChannelType::StackedImage => Ok(6),
        _ => Err(format!("no exercised reference-stub address for {kind:?}")),
    }
}

#[derive(Clone, Copy, Debug)]
enum Submission {
    Direct,
    C2Routed,
}

impl Submission {
    fn label(self) -> &'static str {
        match self {
            Self::Direct => "direct",
            Self::C2Routed => "C2-routed",
        }
    }
}

fn channel_label(kind: ChannelType) -> Result<&'static str> {
    match kind {
        ChannelType::CommandAndControl => Ok("C2"),
        ChannelType::HealthAndStatus => Ok("Health"),
        ChannelType::IrstImage => Ok("Image"),
        ChannelType::StackedImage => Ok("StackedImage"),
        _ => Err(format!("not an exercised destination: {kind:?}")),
    }
}

fn comms_request(kind: ChannelType, submission: Submission) -> Result<CommsTestRequest> {
    let index = REQUIRED
        .iter()
        .position(|&required| required == kind)
        .ok_or_else(|| format!("not an exercised destination: {kind:?}"))? as u32;
    let offset = match submission {
        Submission::Direct => 0,
        Submission::C2Routed => 10,
    };
    Ok(CommsTestRequest {
        command_id: 100 + index + offset,
        channel_id: stub_address(kind)?,
        request_id: 200 + index + offset,
    })
}

fn c2_comms_event(event: C2MetadataEvent) -> Result<CommsTestReport> {
    match event {
        C2MetadataEvent::CommsTest(report) => Ok(report),
        other => Err(format!("unexpected C2 sensor metadata: {other:?}")),
    }
}

fn comms_callback(
    received: std::result::Result<CommsTestReport, Error>,
    receiver: ChannelType,
    submission: Submission,
) -> Result<()> {
    // The receiver supplies channel association; the native report has only two IDs.
    let label = channel_label(receiver)?;
    let path = submission.label();
    let report = received.at(&format!(
        "receive actual {label} CommsTest metadata ({path})"
    ))?;
    correlated(report, comms_request(receiver, submission)?)?;
    println!(
        "[OK] actual {label} CommsTest metadata ({path}): command_id={} request_id={}",
        report.command_id, report.request_id
    );
    Ok(())
}

fn comms_result(
    result: &CommsTestResult,
    request: CommsTestRequest,
    operation: &str,
) -> Result<()> {
    match result {
        CommsTestResult::Completed { report } => correlated(*report, request),
        _ => Err(format!(
            "{operation}: expected correlated completion, got {result:?}"
        )),
    }
}

fn comms(view: &mut ChannelView, kind: ChannelType, submission: Submission) -> Result<()> {
    let request = comms_request(kind, submission)?;
    let operation = &format!("{} CommsTest", submission.label());
    let mut owner = view.submit_comms_test(request).at(operation)?;
    let result = owner.wait(WAIT_MS).at(operation)?;
    let checked = comms_result(&result, request, operation);
    owner.close().at("close CommsRequest")?;
    checked?;
    println!(
        "[OK] {operation}: {result:?}, request={request:?}, destination={}",
        channel_label(kind)?
    );
    Ok(())
}

fn no_telemetry<T: std::fmt::Debug>(
    result: std::result::Result<T, Error>,
    label: &str,
) -> Result<()> {
    match result {
        Err(e) if e.kind() == &ErrorKind::Timeout => Ok(()),
        other => Err(format!(
            "{label}: expected empty metadata queue, got {other:?}"
        )),
    }
}

fn control_only(result: std::result::Result<Frame, Error>) -> Result<()> {
    match result {
        Err(e)
            if e.kind() == &ErrorKind::InitializationFailed
                && e.diagnostic().is_some_and(|d| d.contains("control-only")) =>
        {
            Ok(())
        }
        other => Err(format!(
            "control-only receive: expected established error, got {other:?}"
        )),
    }
}

fn metadata_counts(received: u64, dropped: u64, malformed: u64, expected: u64) -> Result<()> {
    require(received == expected && dropped == 0 && malformed == 0,
        format!("metadata counters violate reference profile: received={received}, dropped={dropped}, malformed={malformed}, expected={expected}"))
}

fn run(path: &Path) -> Result<()> {
    let session = Session::open(path, "minimal-consumer", "").at("Session::open")?;
    let version = session.provider_version().at("provider_version")?;
    profile(&version)?;
    println!(
        "[OK] reference-stub identity: {version:?}; supplied library={}",
        path.display()
    );
    let caps = session.capabilities().at("Control discovery")?;
    for (i, cap) in caps.iter().enumerate() {
        println!("[OK] advertised[{i}] {cap:?}");
    }
    discovery(&caps)?;
    let c = select(&caps, ChannelType::CommandAndControl)?;
    let h = select(&caps, ChannelType::HealthAndStatus)?;
    let i = select(&caps, ChannelType::IrstImage)?;
    let s = select(&caps, ChannelType::StackedImage)?;
    let mut c2 = session
        .open_control_channel(&ControlConfig::new(
            c.channel_id.clone(),
            c.platform_id.clone(),
            c.sensor_location.clone(),
        ))
        .at("attach C2")?;
    let mut health = session
        .open_health_channel(&HealthConfig::new(
            h.channel_id.clone(),
            h.platform_id.clone(),
            h.sensor_location.clone(),
        ))
        .at("attach Health")?;
    let mut image = session
        .open_image_control(&ImageControlConfig::new(
            i.channel_id.clone(),
            i.platform_id.clone(),
            i.sensor_location.clone(),
        ))
        .at("attach Image control-only")?;
    let mut stacked = session
        .open_stacked_channel(&stacked_config(s))
        .at("attach StackedImage")?;
    let mut c2_view = c2.channel_view().at("C2 View")?;
    let mut health_view = health.channel_view().at("Health View")?;
    let mut image_view = image.channel_view().at("Image View")?;
    let actual = stacked
        .capabilities()
        .at("typed StackedImage capabilities")?;
    attached(&actual, s)?;
    let mut stacked_view = stacked.channel_view().at("StackedImage View")?;
    require(
        stacked_view
            .capabilities()
            .at("StackedImage View capabilities")?
            == actual,
        "StackedImage View capability snapshot differs from typed capability",
    )?;
    attached(&health.capabilities().at("typed Health capabilities")?, h)?;
    for (view, cap) in [
        (&mut c2_view, c),
        (&mut health_view, h),
        (&mut image_view, i),
        (&mut stacked_view, s),
    ] {
        attached(&view.capabilities().at("attached capabilities")?, cap)?;
        println!(
            "[OK] attached identity/layout: {}",
            cap.channel_id.descriptive_label()
        );
    }
    let mut c2_metadata = c2
        .open_metadata(QUEUE_CAPACITY)
        .at("register C2 BIT_Configuration, CommandStatus, BIT_Status")?;
    c2_metadata
        .enable_comms_test_events()
        .at("register C2 CommsTest metadata")?;
    let mut health_metadata = health
        .open_metadata(QUEUE_CAPACITY)
        .at("register six Health metadata families")?;
    let mut image_metadata = image.open_metadata(QUEUE_CAPACITY).at(
        "register Image BadPixelList, LineOfSightReport, LineOfSightEuler, NavigationReportResp",
    )?;
    let mut health_comms = health_view
        .open_comms_metadata(QUEUE_CAPACITY)
        .at("register Health CommsTest metadata")?;
    let mut image_comms = image_view
        .open_comms_metadata(QUEUE_CAPACITY)
        .at("register Image CommsTest metadata")?;
    let mut stacked_comms = stacked_view
        .open_comms_metadata(QUEUE_CAPACITY)
        .at("register StackedImage CommsTest metadata")?;
    println!("[OK] required metadata registered: C2=3, Health=6, Image=4; CommsTest=4 (dedicated C2 plus Health/Image/StackedImage common); total=17 (not sensor measurements)");
    c2.enable().at("enable C2")?;
    health.enable().at("enable Health")?;
    image.start().at("start Image control-only")?;
    stacked.enable().at("enable StackedImage")?;
    println!("[OK] C2/Health/StackedImage enabled, Image started (enablement only, no acquisition buffers)");
    control_only(image.receive(0))?;
    println!("[STUB] Image receive(0): established control-only error; no frame acquired");

    for (view, cap) in [
        (&mut c2_view, c),
        (&mut health_view, h),
        (&mut image_view, i),
        (&mut stacked_view, s),
    ] {
        let mut request = view.send_keepalive().at("KeepAlive submit")?;
        let result = request.wait(WAIT_MS).at("KeepAlive wait")?;
        let checked = returned(&result, CommandReturn::Success, "KeepAlive");
        request.close().at("close KeepAlive request")?;
        checked?;
        println!(
            "[OK] {} KeepAlive: {result:?}",
            cap.channel_id.descriptive_label()
        );
    }
    for kind in REQUIRED {
        for submission in [Submission::Direct, Submission::C2Routed] {
            let view = match (submission, kind) {
                (Submission::C2Routed, _) | (_, ChannelType::CommandAndControl) => &mut c2_view,
                (_, ChannelType::HealthAndStatus) => &mut health_view,
                (_, ChannelType::IrstImage) => &mut image_view,
                _ => &mut stacked_view,
            };
            comms(view, kind, submission)?;
            // Receive independently after EACH future, never infer delivery from completion.
            let received = match kind {
                ChannelType::CommandAndControl => Ok(c2_comms_event(
                    c2_metadata
                        .receive(WAIT_MS)
                        .at("receive C2 CommsTest metadata")?,
                )?),
                ChannelType::HealthAndStatus => health_comms.receive(WAIT_MS),
                ChannelType::IrstImage => image_comms.receive(WAIT_MS),
                _ => stacked_comms.receive(WAIT_MS),
            };
            comms_callback(received, kind, submission)?;
        }
    }

    let mut bit = c2
        .submit_bit(&BitCommand {
            command_id: 300,
            action: BitAction::NoOp,
        })
        .at("BIT_Command submit 300/NoOp")?;
    let result = bit.wait(WAIT_MS).at("BIT_Command wait")?;
    let checked = returned(&result, CommandReturn::NotImplemented, "BIT_Command");
    bit.close().at("close BIT request")?;
    checked?;
    println!("[STUB] BIT_Command 300/NoOp: {result:?}; no BIT performed");
    let mut config = c2
        .submit_config_set(&ConfigSetCommand::new(301))
        .at("ConfigSetCommand submit 301/zero timestamp/empty config")?;
    let result = config.wait(WAIT_MS).at("ConfigSetCommand wait")?;
    let checked = returned(&result, CommandReturn::NotImplemented, "ConfigSetCommand");
    config.close().at("close ConfigSet request")?;
    checked?;
    println!("[STUB] ConfigSetCommand 301/zero timestamp/empty config: {result:?}; no configuration applied");
    let mut mode = c2
        .submit_mode(&ModeCommand {
            command_id: 302,
            state: MfaState::Standby,
            mode: MfaMode::Unused,
            scan_parameters: ScanParameters::default(),
        })
        .at("ModeCmd submit 302/Standby/Unused/default scan")?;
    let result = mode.wait(WAIT_MS).at("ModeCmd wait")?;
    let checked = mode_result(&result);
    mode.close().at("close Mode request")?;
    checked?;
    println!("[STUB] ModeCmd 302/Standby/Unused/default scan: {result:?}");
    let mut navigation = image
        .submit_navigation_report(&neutral_navigation())
        .at("NavigationReport submit NotSet")?;
    let result = navigation.wait(WAIT_MS).at("NavigationReport wait")?;
    let checked = navigation_result(&result);
    navigation.close().at("close Navigation request")?;
    checked?;
    println!("[STUB] NavigationReport NotSet/neutral values: {result:?}; not an INS fix or delivered alignment");
    no_telemetry(c2_metadata.receive(0), "C2")?;
    no_telemetry(health_metadata.receive(0), "Health")?;
    no_telemetry(image_metadata.receive(0), "Image")?;
    let frames = image.counters().at("control-only Image counters")?;
    require(
        frames.frames_received == 0
            && frames.frames_dropped_queue_full == 0
            && frames.malformed_or_unsupported == 0,
        format!("unexpected control-only image activity: {frames:?}"),
    )?;
    let counts = c2_metadata.counters().at("C2 metadata counters")?;
    metadata_counts(
        counts.events_received,
        counts.events_dropped_queue_full,
        counts.malformed_or_unsupported,
        2,
    )?;
    println!("[OK] C2 CommsTest counters: events_received=2 events_dropped_queue_full=0 malformed_or_unsupported=0");
    no_telemetry(c2_metadata.receive(0), "C2 CommsTest")?;
    println!("[OK] C2 CommsTest queue empty (active Timeout)");
    for (label, metadata) in [
        ("Health", &mut health_comms),
        ("Image", &mut image_comms),
        ("StackedImage", &mut stacked_comms),
    ] {
        let counts = metadata
            .counters()
            .at(&format!("{label} CommsTest counters"))?;
        metadata_counts(
            counts.events_received,
            counts.events_dropped_queue_full,
            counts.malformed_or_unsupported,
            2,
        )?;
        println!("[OK] {label} CommsTest counters: events_received=2 events_dropped_queue_full=0 malformed_or_unsupported=0");
        no_telemetry(metadata.receive(0), &format!("{label} CommsTest"))?;
        println!("[OK] {label} CommsTest queue empty (active Timeout)");
    }
    let counts = health_metadata.counters().at("Health metadata counters")?;
    metadata_counts(
        counts.events_received,
        counts.events_dropped_queue_full,
        counts.malformed_or_unsupported,
        0,
    )?;
    let counts = image_metadata.counters().at("Image metadata counters")?;
    metadata_counts(
        counts.events_received,
        counts.events_dropped_queue_full,
        counts.malformed_or_unsupported,
        0,
    )?;
    println!("[STUB] Image acquisition counters: frames_received=0 frames_dropped_queue_full=0 malformed_or_unsupported=0");
    println!("[STUB] all sensor metadata queues empty at nonblocking observation; registration is not telemetry");

    image.stop().at("stop Image")?;
    c2_view.close().at("close C2 View")?;
    health_view.close().at("close Health View")?;
    image_view.close().at("close Image View")?;
    stacked_view.close().at("close StackedImage View")?;
    c2.close().at("close C2 (typed disable/detach)")?;
    health.close().at("close Health (typed disable/detach)")?;
    image.close().at("close Image")?;
    stacked
        .close()
        .at("close StackedImage (typed disable/detach)")?;
    println!("[OK] StackedImage explicitly closed (typed disable/detach)");
    // Retain metadata consumption owners through typed channel cleanup.
    c2_metadata.close().at("close C2 metadata")?;
    health_metadata.close().at("close Health metadata")?;
    image_metadata.close().at("close Image metadata")?;
    health_comms.close().at("close Health CommsTest metadata")?;
    image_comms.close().at("close Image CommsTest metadata")?;
    stacked_comms
        .close()
        .at("close StackedImage CommsTest metadata")?;
    session.close().at("close Session")?;
    println!("[OK] requests, Views, typed channels, metadata and Session explicitly closed");
    println!("[OK] CommsTest parity: 8 correlated futures and 8 independently received callbacks across 4 channels");
    println!("[OK] four-channel reference-stub contract complete; no full C++ parity or hardware qualification");
    Ok(())
}

fn neutral_navigation() -> NavigationReport {
    NavigationReport {
        system_time_ns: 0,
        state: PositionSolutionState::NotSet,
        latitude_rad: 0.0,
        longitude_rad: 0.0,
        altitude_m: 0.0,
        attitude: Euler {
            roll: 0.0,
            pitch: 0.0,
            yaw: 0.0,
        },
        attitude_rate: AttitudeRate {
            attitude_rate: Euler {
                roll: 0.0,
                pitch: 0.0,
                yaw: 0.0,
            },
            attitude_rate_time_ns: 0,
        },
        speed: NorthEastDown {
            north: 0.0,
            east: 0.0,
            down: 0.0,
        },
        acceleration: NorthEastDown {
            north: 0.0,
            east: 0.0,
            down: 0.0,
        },
        wander_angle_rad: 0.0,
        magnetic_heading: 0.0,
        altitude_msl: 0.0,
        position_velocity_covariance_uncertainty: PositionVelocityCovariance {
            position_position_pn_pn: 0.0,
            position_position_pn_pe: 0.0,
            position_position_pn_pd: 0.0,
            position_position_pe_pe: 0.0,
            position_position_pe_pd: 0.0,
            position_position_pd_pd: 0.0,
            position_velocity_pn_vn: 0.0,
            position_velocity_pn_ve: 0.0,
            position_velocity_pn_vd: 0.0,
            position_velocity_pe_ve: 0.0,
            position_velocity_pe_vd: 0.0,
            position_velocity_pd_vd: 0.0,
            velocity_velocity_vn_vn: 0.0,
            velocity_velocity_vn_ve: 0.0,
            velocity_velocity_vn_vd: 0.0,
            velocity_velocity_ve_ve: 0.0,
            velocity_velocity_ve_vd: 0.0,
            velocity_velocity_vd_vd: 0.0,
        },
    }
}

#[cfg(test)]
#[path = "../tests/minimal_ir_consumer/policy.rs"]
mod tests;
