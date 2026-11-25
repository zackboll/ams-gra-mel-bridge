//! Submits a synthetic ScanTask to the repository's compiled IR mock only.
#![forbid(unsafe_code)]

use ams_mel_ir::{
    ChannelType, ErrorKind, ReturnResult, ScanParameters, SchedulingConfig, SchedulingScanTask,
    SchedulingTaskInfo, Session, TaskListAction,
};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mock_dir = std::env::var_os("AMS_MEL_TEST_PROVIDER_DIR")
        .ok_or("set AMS_MEL_TEST_PROVIDER_DIR to the compiled mock directory")?;
    let mock_provider = std::path::PathBuf::from(mock_dir).join("libmock_ir_provider.so");
    let session = Session::open(mock_provider, "sched-scan-success", "")?;
    let advertised = session
        .capabilities()?
        .into_iter()
        .find(|capability| capability.channel_types.contains(&ChannelType::Scheduling))
        .ok_or("mock does not advertise Scheduling")?;
    let mut channel = session.open_scheduling_channel(&SchedulingConfig::new(
        advertised.channel_id,
        advertised.platform_id,
        advertised.sensor_location,
    ))?;
    channel.enable()?;

    let task = SchedulingScanTask {
        command_id: 17,
        parameters: ScanParameters {
            scan_id: 23,
            ..ScanParameters::default()
        },
        task_info: SchedulingTaskInfo {
            system_time_ns: 0,
            request_id: 93,
            action: TaskListAction::Normal,
            precedence_within_priority: 2,
            priority: 1,
        },
    };
    let mut request = channel.exec_scan(&task)?;
    channel.close()?;
    session.close()?;

    match request.wait(5_000) {
        Ok(ReturnResult::Completed { value }) => {
            println!("Mock provider returned {value:?}");
            println!("Completion does not establish physical scanning, image acquisition, or sensor performance.");
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
