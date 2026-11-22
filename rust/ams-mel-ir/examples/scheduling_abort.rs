//! Submits one AbortTask through the public Scheduling API.
#![forbid(unsafe_code)]

use ams_mel_ir::{
    ChannelType, ErrorKind, ReturnResult, SchedulingConfig, SchedulingTaskInfo, Session,
    TaskListAction,
};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut args = std::env::args().skip(1);
    let provider = args.next().ok_or("expected provider path")?;
    let instance = args.next().ok_or("expected instance name")?;
    let request_id = args.next().ok_or("expected request ID")?.parse()?;
    let system_time_ns = args.next().ok_or("expected signed nanoseconds")?.parse()?;
    let precedence_within_priority = args.next().ok_or("expected precedence")?.parse()?;
    let priority = args.next().ok_or("expected priority")?.parse()?;

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
    let mut request = channel.exec_abort(&SchedulingTaskInfo {
        system_time_ns,
        request_id,
        action: TaskListAction::Normal, // AbortTask does not require action=Abort.
        precedence_within_priority,
        priority,
    })?;
    channel.close()?;
    session.close()?;

    match request.wait(5_000) {
        Ok(ReturnResult::Completed { value }) => println!("Provider returned {value:?}"),
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
