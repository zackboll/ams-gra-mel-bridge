//! Attaches, queries and enables a Scheduling channel without submitting tasks.
#![forbid(unsafe_code)]

use ams_mel_ir::{ChannelType, SchedulingConfig, Session};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let provider = std::env::args_os().nth(1).ok_or("expected provider path")?;
    let instance = std::env::args().nth(2).ok_or("expected instance name")?;
    let session = Session::open(provider, &instance, "")?;
    let discovered = session.capabilities()?;
    let advertised = discovered
        .iter()
        .find(|capability| capability.channel_types.contains(&ChannelType::Scheduling))
        .ok_or("provider does not advertise a Scheduling channel")?;
    let config = SchedulingConfig::new(
        advertised.channel_id.clone(),
        advertised.platform_id.clone(),
        advertised.sensor_location.clone(),
    );
    let mut scheduling = session.open_scheduling_channel(&config)?; // Attached
    let mut view = scheduling.channel_view()?;
    let attached = view.capabilities()?;
    scheduling.enable()?;
    println!(
        "Enabled {}; attached TaskScheduleDepth={}",
        attached.channel_id.descriptive_label(),
        attached.task_schedule_depth,
    );

    // The public Session can close first; the typed owner still owns native state.
    session.close()?;
    scheduling.close()?;
    view.close()?;
    Ok(())
}
