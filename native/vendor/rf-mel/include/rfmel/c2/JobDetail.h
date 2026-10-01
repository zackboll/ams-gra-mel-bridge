#pragma once

#include <math/units/UTCTime.h>
#include <rfmel/c2/C2MELTypes.h>
#include <rfmel/rfmeltypes/RFMELTypes.h>
#include <future>
#include <vector>
#include <cstdint>
#include <functional>

namespace ams::iface::rfmel
{
	/// @brief Represents a concrete set of resources allocated by the VAS in the form of a VA Instance and a time interval, and
	/// allows commands to be issued against the set of resources. Each interval of the Job is specified by invoking the
	/// addJobIntervals() method. The finalize() method is invoked last, which causes any cached job data to
	/// be sent, and the MFA is notified that no more intervals will be produced.
	/// @Required This class provides required RF MEL functionality for supporting queries of the allocated time windows and the
	/// selected Virtual Apperture instance, and must be provided by the implementer in all RF MEL implementations. See members for details.
	class JobDetail
	{
	public:
		JobDetail() = default;
		virtual ~JobDetail() = default;
		JobDetail(const JobDetail&) = default;
		JobDetail(JobDetail&&) = default;
		JobDetail& operator=(const JobDetail&) = default;
		JobDetail& operator=(JobDetail&&) = default;

		/// @brief Returns the actual job start time assigned by the Scheduler.
		/// @Required This function supports passing job temporal data and must be provided
		/// by the implementer for all RF MEL implementations.
		[[nodiscard]] virtual ams::util::math::UTCTime actualStartTime() const = 0;

		/// @brief Registers a callback to receive Job Interval Status and indicates
		/// if successful Job Interval status should be reported.
		/// @Required This function supports passing Job Interval data and must be provided by
		/// the implementer for all RF MEL implementations.
		[[nodiscard]] virtual const std::function<void(JobIntervalStatus)>& getJobIntervalStatusCallback() const = 0;

		/// @brief Returns the actual time duration assigned to the job by the Scheduler.
		/// @Required This function supports passing job temporal data and must be provided
		/// by the implementer for all RF MEL implementations.
		[[nodiscard]] virtual ams::util::math::Femtoseconds totalJobDuration() const = 0;

		/// @brief returns the specific Virtual Aperture instance assigned by the Scheduler.
		/// @Required This function supports passing Virtual Aperture instance data and must be
		/// provided by the implementer for all RF MEL implementations.
		[[nodiscard]] virtual VirtualApertureInstanceID getVAInstanceID() const = 0;

		/// @brief Returns the Virtual Aperture Definition ID.
		/// @Required This function supports passing Virtual Aperture IDs and must be provided
		/// by the implementer for all RF MEL implementations.
		[[nodiscard]] virtual VirtualApertureDefinitionID getVADefinitionID() const = 0;

		/// @brief Returns the Job Details ID.
		/// @Required This function supports passing Job Details IDs and must be provided by the
		/// implementer for all RF MEL implementations.
		[[nodiscard]] virtual uint32_t getJobDetailsID() const = 0;

		/// @brief Indicates that the job has been completely populated and is ready to be sent.
		/// @Required This function supports passing job status data and must be provided by the
		/// implementer for all RF MEL implementations.
		[[nodiscard]] virtual std::future<JobStatus> finalize() = 0;

		/// @brief JobIntervals that have been specified are sent out on the wire.
		/// @Required This function supports passing job interval information and must be provided
		/// by the implementer for all RF MEL implementations.
		virtual void flush() = 0;

		/// @brief registers a callback to receive Job Interval Status and indicates if successful Job Interval
		/// status should be reported.
		/// @Required This function supports passing Job Interval data and must be provided by the implementer for all RF MEL
		/// implementations.
		virtual void registerJobIntervalStatusCallback(const std::function<void(JobIntervalStatus)>& callback) = 0;

		/// @brief Adds Job Intervals to be carried out by the Job.
		/// @Required This function supports passing job interval information and must be provided
		/// by the implementer for all RF MEL implementations.
		virtual void addJobIntervals(const std::vector<JobInterval>& jobIntervals) = 0;

		/// @brief instructs the MFA to clear out any unexecuted jobIntervals attached to this JobDetail
		/// @Required This function supports canceling job interval and must be provided
		/// by the implementer for all RF MEL implementations.
		virtual void cancelRemainingJobIntervals() = 0;

		/// @brief instructs the MFA to cancel the job associated with the JobDetailsID attached to this JobDetail
		/// @Required This function supports canceling job and must be provided
		/// by the implementer for all RF MEL implementations.
		virtual CancelStatus cancelJob() = 0;

		/// @brief Extends the duration of a Job Event.
		/// @RequiredIfJobEventDurationExtension This function extends Job Events and must be provided
		/// by the implementer for all RF MEL implementations if the associated MFA supports Job Event
		/// duration extensions.
		virtual void extendJobEvent(uint32_t intervalID, JobEventID eventId, ams::util::math::Femtoseconds addedDuration) = 0;

		/// @brief Indicates the VITA49.2 Stream IDs this job will produce.
		/// The Virtual Aperture defines the number and meaning of each stream.
		/// @RequiredIfReceive This function passes Stream IDs in support of
		/// required RF MEL functionality specific to receive and must be
		/// provided by the implementer in all RF MEL implementations if the
		/// associated MFA supports receive.
		[[nodiscard]] virtual std::vector<StreamID> getRxStreamIDs(size_t groupNum = 0) const = 0;

		/// @brief Returns the Job Request ID.
		/// @Required This function supports getting the Job Request ID that was used to create the job and must be provided by the
		/// implementer for all RF MEL implementations.
		[[nodiscard]] virtual uint32_t getJobRequestId() const = 0;

		/// @brief Returns the lookahead time for the next JIB.
		/// @note See the related field in JobRequest for more information.
		/// @Optional This function supports optional passing of lookahead times to improve scheduling efficiency
		/// and may be provided in RF MEL implementations that wish to support this feature.
		[[nodiscard]] virtual ams::util::math::Femtoseconds getLookAheadTime() const = 0;
	};
} // namespace ams::iface::rfmel
