//! Discovers Scheduling and submits one StareTask through the safe Rust API.
#![forbid(unsafe_code)]

use ams_mel_ir::{
    ChannelType, CoordinateFrameReference, ErrorKind, ReturnResult, SchedulingConfig,
    SchedulingStareParameters, SchedulingStareTask, SchedulingTaskInfo, Session, TaskListAction,
};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut args = std::env::args().skip(1);
    let provider = args.next().ok_or("expected provider path")?;
    let instance = args.next().ok_or("expected instance name")?;

    let session = Session::open(provider, &instance, "")?;
    let advertised = session
        .capabilities()?
        .into_iter()
        .find(|capability| capability.channel_types.contains(&ChannelType::Scheduling))
        .ok_or("provider does not advertise Scheduling")?;
    let config = SchedulingConfig::new(
        advertised.channel_id,
        advertised.platform_id,
        advertised.sensor_location,
    );
    let mut channel = session.open_scheduling_channel(&config)?;
    channel.enable()?;

    // Illustrative values for the mock, not a pointing or timing prescription.
    let task = SchedulingStareTask {
        command_id: 17,
        parameters: SchedulingStareParameters {
            center_azimuth_rad: 0.125,
            center_elevation_rad: -0.25,
            elevation_frame: CoordinateFrameReference::Inertial,
            azimuth_frame: CoordinateFrameReference::Aircraft,
            duration_ns: 1_000_000_000,
        },
        task_info: SchedulingTaskInfo {
            system_time_ns: 0,
            request_id: 93,
            action: TaskListAction::Normal,
            precedence_within_priority: 2,
            priority: 1,
        },
    };
    let mut request = channel.exec_stare(&task)?;
    channel.close()?;
    session.close()?;

    match request.wait(5_000) {
        Ok(ReturnResult::Completed { value }) => {
            println!("Provider returned {value:?}");
            println!("Command completion does not verify physical staring, pointing accuracy, or image acquisition.");
        }
        Ok(ReturnResult::Rejected { code, description }) => {
            println!("MEL rejected the task: {code:?}: {description}");
        }
        Err(error) if error.kind() == &ErrorKind::Timeout => {
            println!("Still pending; closing the observer does not cancel the task");
        }
        Err(error) => return Err(error.into()),
    }
    request.close()?;
    Ok(())
}
