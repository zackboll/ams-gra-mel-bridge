use std::env;
use std::process::ExitCode;

use ams_mel::{
    ComponentLocation, ControlChannel, ControlConfig, Error, ImageConfig, MfaMode, ModeResult,
    Session, UciId,
};

const CHANNEL_UUID: [u8; 16] = [
    0x00, 0x40, 0x04, 0x00, 0x11, 0x22, 0x43, 0x44, 0x85, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc,
];
const PLATFORM_UUID: [u8; 16] = [
    0x00, 0x40, 0x04, 0x01, 0x21, 0x32, 0x43, 0x54, 0x86, 0x67, 0x78, 0x89, 0x9a, 0xab, 0xbc, 0xcd,
];
const EXPECTED_WIDTH: u32 = 320;
const EXPECTED_HEIGHT: u32 = 200;
const EXPECTED_BYTES: usize = 64_000;

enum Failure {
    Input(String),
    Contract(String),
    Mel {
        operation: &'static str,
        error: Error,
    },
}

trait Operation<T> {
    fn at(self, operation: &'static str) -> Result<T, Failure>;
}

impl<T> Operation<T> for Result<T, Error> {
    fn at(self, operation: &'static str) -> Result<T, Failure> {
        self.map_err(|error| Failure::Mel { operation, error })
    }
}

fn main() -> ExitCode {
    match run() {
        Ok(()) => {
            println!("PASS: real Squall IR Rust integration");
            ExitCode::SUCCESS
        }
        Err(failure) => {
            print_failure(&failure);
            ExitCode::FAILURE
        }
    }
}

fn run() -> Result<(), Failure> {
    let arguments: Vec<String> = env::args().collect();
    if !(3..=5).contains(&arguments.len()) {
        return Err(Failure::Input(format!(
            "usage: {} PROVIDER_SO PROFILE_JSON [FRAME_COUNT] [TIMEOUT_MS]",
            arguments
                .first()
                .map_or("ams-mel-squall-ir", String::as_str)
        )));
    }
    let frame_count = parse_u32(arguments.get(3), 3, "frame count")?;
    let timeout_ms = parse_u32(arguments.get(4), 10_000, "timeout")?;
    if !(3..=1000).contains(&frame_count) {
        return Err(Failure::Input(
            "frame count must be an integer in 3..1000".to_owned(),
        ));
    }
    if timeout_ms == 0 {
        return Err(Failure::Input(
            "timeout must be a finite, nonzero u32 value".to_owned(),
        ));
    }

    let session = Session::open(&arguments[1], &arguments[2], "").at("open Session")?;
    let version = session.provider_version().at("query provider version")?;
    println!(
        "provider version: api={} library={} vendor={} description={}",
        version.api_version, version.library_version, version.vendor, version.description
    );

    let platform =
        UciId::new(PLATFORM_UUID, "Task 004 integration platform").at("construct platform ID")?;
    let location = ComponentLocation::new(
        0.0,
        0.0,
        0.0,
        "task-004-station",
        "ams-mel-squall-integration",
    )
    .at("construct component location")?;
    let image_config = ImageConfig::with_limits(
        UciId::new(CHANNEL_UUID, "Task 009 Rust IRSTImage").at("construct image channel ID")?,
        platform.clone(),
        location.clone(),
        4,
        1024 * 1024,
        8,
    )
    .at("construct image configuration")?;
    let control_config = ControlConfig::new(
        UciId::new(CHANNEL_UUID, "Task 009 Rust IR C2").at("construct C2 channel ID")?,
        platform,
        location,
    );

    let mut stream = session
        .open_image_stream(&image_config)
        .at("open ImageStream")?;
    stream.start().at("start ImageStream")?;
    let mut control = session
        .open_control_channel(&control_config)
        .at("open ControlChannel")?;
    control.enable().at("enable ControlChannel")?;
    let mut request = control
        .submit_operate(0x0040_0403)
        .at("submit Operate/TaskSched")?;
    require_task_sched(request.wait(5_000).at("wait for Operate/TaskSched")?)?;
    println!("C2 result: TASK_SCHED");

    session.close().at("close parent Session")?;
    require_task_sched(request.wait(0).at("repeat cached ModeRequest wait")?)?;

    let mut previous_id = None;
    for index in 1..=frame_count {
        let frame = stream.receive(timeout_ms).at("receive image frame")?;
        if frame.width != EXPECTED_WIDTH
            || frame.height != EXPECTED_HEIGHT
            || frame.bits_per_pixel != 8
            || frame.number_of_bands != 1
            || frame.pixels.len() != EXPECTED_BYTES
        {
            return Err(Failure::Contract(format!(
                "frame {index} has invalid Mono8 geometry: {}x{}, {} bits, {} bands, {} bytes",
                frame.width,
                frame.height,
                frame.bits_per_pixel,
                frame.number_of_bands,
                frame.pixels.len()
            )));
        }
        if previous_id.is_some_and(|id| frame.frame_id <= id) {
            return Err(Failure::Contract(format!(
                "frame {index} ID {} is not greater than its predecessor",
                frame.frame_id
            )));
        }
        previous_id = Some(frame.frame_id);
        println!(
            "frame {index}: id={} geometry={}x{} bytes={} checksum={:016x}",
            frame.frame_id,
            frame.width,
            frame.height,
            frame.pixels.len(),
            checksum(&frame.pixels)
        );
    }

    let counters = stream.counters().at("query ImageStream counters")?;
    println!(
        "counters: received={} dropped={} malformed={}",
        counters.frames_received,
        counters.frames_dropped_queue_full,
        counters.malformed_or_unsupported
    );
    if counters.frames_received < u64::from(frame_count) || counters.malformed_or_unsupported != 0 {
        return Err(Failure::Contract(format!(
            "stream counters violate contract: requested={frame_count}, received={}, malformed={}",
            counters.frames_received, counters.malformed_or_unsupported
        )));
    }

    request.close().at("close ModeRequest")?;
    close_control(&mut control)?;
    stream.close().at("close ImageStream")?;
    Ok(())
}

fn parse_u32(value: Option<&String>, default: u32, name: &str) -> Result<u32, Failure> {
    value.map_or(Ok(default), |text| {
        text.parse::<u32>()
            .map_err(|_| Failure::Input(format!("{name} must be a finite u32 integer")))
    })
}

fn require_task_sched(result: ModeResult) -> Result<(), Failure> {
    match result {
        ModeResult::Success {
            mode: MfaMode::TaskSched,
        } => Ok(()),
        ModeResult::Success { mode } => Err(Failure::Contract(format!(
            "C2 returned unexpected successful mode {mode:?}"
        ))),
        ModeResult::Rejected { code, description } => Err(Failure::Contract(format!(
            "C2 rejected Operate/TaskSched: code={code:?}, description={description}"
        ))),
    }
}

fn close_control(control: &mut ControlChannel) -> Result<(), Failure> {
    if let Err(first) = control.close() {
        if !control.is_open() {
            return Err(Failure::Mel {
                operation: "close ControlChannel",
                error: first,
            });
        }
        control.close().at("retry retained ControlChannel close")?;
    }
    if control.is_open() {
        return Err(Failure::Contract(
            "ControlChannel remained open after explicit close".to_owned(),
        ));
    }
    Ok(())
}

fn checksum(bytes: &[u8]) -> u64 {
    bytes.iter().fold(1_469_598_103_934_665_603, |value, byte| {
        (value ^ u64::from(*byte)).wrapping_mul(1_099_511_628_211)
    })
}

fn print_failure(failure: &Failure) {
    match failure {
        Failure::Input(message) | Failure::Contract(message) => eprintln!("FAIL: {message}"),
        Failure::Mel { operation, error } => eprintln!(
            "FAIL: {operation}: kind={:?} diagnostic={} diagnostic_required={}",
            error.kind(),
            error.diagnostic().unwrap_or("<absent>"),
            error
                .diagnostic_required()
                .map_or_else(|| "<absent>".to_owned(), |value| value.to_string())
        ),
    }
}
