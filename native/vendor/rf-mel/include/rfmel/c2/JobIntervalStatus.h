#pragma once

#include <rfmel/rfmeltypes/RFMELTypes.h>

#include <map>
#include <cstdint>
#include <vector>

namespace ams::iface::rfmel
{
	/// @brief Logs changes to job events when conditional events are executed.
	/// @Required This class provides data definition in support of required RF MEL functionality
	/// to support Job interval change logs and must be included as-is in all RF MEL implementations.
	class JobEventLogInfo
	{
	public:
		JobEventLogInfo() = default;
		~JobEventLogInfo() = default;

		JobEventLogInfo(const JobEventLogInfo&) = default;
		JobEventLogInfo(JobEventLogInfo&&) = default;
		JobEventLogInfo& operator=(const JobEventLogInfo&) = default;
		JobEventLogInfo& operator=(JobEventLogInfo&&) = default;

		/// @brief Gets the reason for triggering the Job Event log.
		[[nodiscard]] const auto& getJobEventLogTrigger() const
		{
			return this->jobEventLogTrigger;
		}
		/// @brief Gets the time of day that the JobEvent trigger occurred.
		[[nodiscard]] const auto& getJobEventLogTime()
		{
			return this->jobEventLogTime;
		}

		/// @brief Sets the Job Event UTC time.
		void setJobEventLogTime(const ams::util::math::UTCTime jobEventLogTime_in)
		{
			this->jobEventLogTime = jobEventLogTime_in;
		}
		/// @brief Sets the reason for triggering the Job Event log.
		void setJobEventLogTrigger(const JobEventLogTriggerType jobEventLogTrigger_in)
		{
			this->jobEventLogTrigger = jobEventLogTrigger_in;
		}

	private:
		// Time of day that the JobEvent trigger occurred.
		ams::util::math::UTCTime jobEventLogTime;

		// Log to indicate if a conditional event was inhibited, an event was delayed, or an event was cancelled.
		JobEventLogTriggerType jobEventLogTrigger = JobEventLogTriggerType::None;
	};

	/// @brief Indicates the completion status of a job interval based on the duration of its events.
	/// and logs the trigger type of conditional job events.
	/// @Required This class provides data definition in support of required RF MEL functionality
	/// to support Job interval completion status and must be included as-is in all RF MEL implementations.
	class JobIntervalStatus
	{
	public:
		JobIntervalStatus() = default;
		~JobIntervalStatus() = default;

		JobIntervalStatus(const JobIntervalStatus&) = default;
		JobIntervalStatus(JobIntervalStatus&&) noexcept = default;
		JobIntervalStatus& operator=(const JobIntervalStatus&) = default;
		JobIntervalStatus& operator=(JobIntervalStatus&&) = default;

		/// @brief Gets the Job Interval ID corresponding to this status.
		[[nodiscard]] auto& getJobIntervalID() const
		{
			return this->jobIntervalID;
		}
		/// @brief Gets the Job Interval completion status.
		[[nodiscard]] const auto& getJobIntervalCompletionStatus() const
		{
			return this->jobIntervalCompletionStatus;
		}
		/// @brief Gets the job event log of all the job events.
		[[nodiscard]] auto& getJobEventLog() const
		{
			return this->jobEventLog;
		}
		/// @brief Gets the Activity ID.
		[[nodiscard]] const auto& getActivityId() const
		{
			return this->activityId;
		}
		/// @brief Sets the Job Interval ID corresponding to this status.
		void setJobIntervalID(const uint32_t jobIntervalID_in)
		{
			this->jobIntervalID = jobIntervalID_in;
		}
		/// @brief Sets the Job Interval completion status.
		void setJobIntervalCompletionStatus(const JobIntervalCompletionStatus jobIntervalCompletionStatus_in)
		{
			this->jobIntervalCompletionStatus = jobIntervalCompletionStatus_in;
		}
		/// @brief Adds an entry to the Job Event Log.
		void addJobEventLog(JobEventID jobEventID, const JobEventLogInfo jobEventLogInfo)
		{
			this->jobEventLog.emplace(jobEventID, jobEventLogInfo);
		}
		/// @brief Clears the Job Event Log.
		void clearJobEventLog()
		{
			jobEventLog.clear();
		}
		/// @brief Sets the Activity ID
		void setActivityId(const std::vector<uint8_t>& activityId_in)
		{
			this->activityId = activityId_in;
		}

	private:
		// Uniquely identifies the Job Interval corresponding to this status entry.
		uint32_t jobIntervalID = 0;

		// Current status of the Job Interval.
		JobIntervalCompletionStatus jobIntervalCompletionStatus = JobIntervalCompletionStatus::None;

		/// Log to indicate if a conditional event was executed, an event was inhibited, or an
		/// event was cancelled. The log may be empty if no conditional events were executed.
		std::map<JobEventID, JobEventLogInfo> jobEventLog;

		std::vector<uint8_t> activityId;
	};
} // namespace ams::iface::rfmel
