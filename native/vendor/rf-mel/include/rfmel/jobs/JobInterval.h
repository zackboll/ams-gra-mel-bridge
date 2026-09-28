#pragma once

#include <math/units/UTCTime.h>
#include <rfmel/jobs/DataPipe.h>
#include <rfmel/jobs/ElementGroupToEndpointConnections.h>
#include <rfmel/jobs/JobEvent.h>
#include <rfmel/jobs/LocalFunctionCommand.h>
#include <rfmel/jobs/Pointing.h>
#include <rfmel/jobs/ProductStreamParams.h>
#include <rfmel/rfmeltypes/RFMELTypes.h>

#include <any>
#include <functional>
#include <memory>
#include <optional>
#include <vector>
#include <cstdint>
#include <unordered_map>

namespace ams::iface::rfmel
{
	/// @brief The Sequence is the core repitition loop of the Job, such as the PRI or Dwell.
	/// None of the events contained in a sequence are allowed to overlap any other event.
	/// Sufficient switching time must be scheduled between events; see RF_MFA_Info.
	/// @Required This class provides data definition in support of required RF MEL functionality
	/// to schedule switching time for sequencing and must be included as-is in all RF MEL implementations.
	/// See individual class members for additional details.
	class Sequence
	{
	public:
		Sequence() = default;
		~Sequence() = default;

		Sequence(const Sequence&) = default;
		Sequence(Sequence&&) = default;
		Sequence& operator=(const Sequence&) = default;
		Sequence& operator=(Sequence&&) = default;

		/// @brief Gets the total duration of this repeating sequence.
		/// The start+duration of each event must be less than this
		/// duration or the job will be rejected.
		/// Extending this duration to be longer than the start+duration
		/// of the last even inserts a "gap" in the timeline for
		/// each iteration.
		[[nodiscard]] auto getDuration() const
		{
			return this->duration;
		}
		/// @brief Gets the sequence of transmit events.
		[[nodiscard]] const auto& getTxEvents() const
		{
			return this->txEvents;
		}
		/// @brief Gets the sequence of transmit events.
		[[nodiscard]] auto& getTxEvents()
		{
			return this->txEvents;
		}
		/// @brief Gets the sequence of receive events.
		[[nodiscard]] const auto& getRxEvents() const
		{
			return this->rxEvents;
		}
		/// @brief Gets the sequence of receive events.
		[[nodiscard]] auto& getRxEvents()
		{
			return this->rxEvents;
		}

		/// @brief Sets the total duration of this repeating sequence.
		/// The start+duration of each event must be less than this
		/// duration or the job will be rejected.
		/// Extending this duration to be longer than the start+duration
		/// of the last even inserts a "gap" in the timeline for
		/// each iteration.
		/// Use quantizeDuration() when setting this value to ensure the
		/// MFA is able to execute as requested.
		void setDuration(ams::util::math::Femtoseconds duration_in)
		{
			this->duration = duration_in;
		}
		/// @brief Sets the sequence of transmit events.
		void setTxEvents(const std::vector<TransmitEvent>& txEvents_in)
		{
			this->txEvents = txEvents_in;
		}
		/// @brief Sets the sequence of receive events.
		void setRxEvents(const std::vector<ReceiveEvent>& rxEvents_in)
		{
			this->rxEvents = rxEvents_in;
		}

	private:
		// The total duration of this repeating sequence.
		// The start+duration of each event must be less than this
		// duration or the job will be rejected.
		// Extending this duration to be longer than the start+duration
		// of the last even inserts a "gap" in the timeline for
		// each iteration.
		// Use quantizeDuration() when setting this value to ensure the
		// MFA is able to execute as requested.
		ams::util::math::Femtoseconds duration{0};

		// The sequence of transmit events.
		std::vector<TransmitEvent> txEvents;

		// The sequence of receive events.
		std::vector<ReceiveEvent> rxEvents;
	};

	/// @brief The JobInterval describes how a Sequence is to be executed, including its loop cout, and any padding time required
	/// before or after the loop executes. If a Virtual Aperture contains multiple Tx/Rx element groups, then a JobInterval may
	/// target one or more of the element groups. JobIntervals targeting distinct element groups may be commanded
	/// to execute in parallel by setting the same interval_start parameter.
	/// @Required This class provides data definition in support of required RF MEL functionality to support Job intervals and must
	/// be included as-is in all RF MEL implementations. See individual class members for additional details.
	class JobInterval
	{
	public:
		JobInterval() = default;
		~JobInterval() = default;
		JobInterval(const JobInterval&) = default;
		JobInterval(JobInterval&&) = default;
		JobInterval& operator=(const JobInterval&) = default;
		JobInterval& operator=(JobInterval&&) = default;

		// When intervalStart is set to this constant value, it is a flag to
		// indicate this JobInterval starts immediately after the end of the prior
		// JobInterval, if one exists, or the start of the Job otherwise.
		static constexpr ams::util::math::Femtoseconds ContinueFromPrevious = std::numeric_limits<ams::util::math::Femtoseconds>::max();

		/// @brief Gets IntervalStart which indicates the start time of this interval, relative to the start of the job.
		/// Multiple JobIntervals may start at the same time only if they contain no element
		/// groups in common. By default, each interval immediately follows the previous interval.
		[[nodiscard]] auto getIntervalStart() const
		{
			return this->intervalStart;
		}
		/// @brief Gets the list of transmit element groups for Virtual Apertures. May be left empty.
		[[nodiscard]] const auto& getApplicableElementGroups() const
		{
			return this->dataPaths;
		}
		/// @brief Gets IntervalID which uniquely identifies this interval. Especially useful for tasks
		/// that get split into seprate job timelines.
		[[nodiscard]] auto getIntervalID() const
		{
			return this->intervalID;
		}
		/// @brief Gets the amount of gap time before the start of the sequence loop.
		[[nodiscard]] auto getIntervalStartingGap() const
		{
			return this->intervalStartingGap;
		}
		/// @brief Gets the sequence of events to repeat.
		[[nodiscard]] const auto& getSequence() const
		{
			return this->sequence;
		}
		/// @brief Gets the sequence of events to repeat.
		[[nodiscard]] auto& getSequence()
		{
			return this->sequence;
		}

		/// @brief Gets the number of times this Sequence should execute to perform Data Collection.
		[[nodiscard]] auto getSequenceRepeatCount() const
		{
			return this->sequenceRepeatCount;
		}
		/// @brief Gets the amount of time after the end of the sequence loop to perform Calibration.
		[[nodiscard]] auto getCalDuration() const
		{
			return this->calDuration;
		}
		/// @brief Gets the amount of gap time after the end of the sequence loop and Calibration.
		[[nodiscard]] auto getIntervalEndingGap() const
		{
			return this->intervalEndingGap;
		}
		/// @brief Gets the list of stabilization points for the events in this Interval.
		[[nodiscard]] const auto& getStabPoints() const
		{
			return this->stabPoints;
		}
		/// @brief Gets the list of stabilization points for the events in this Interval.
		[[nodiscard]] auto& getStabPoints()
		{
			return stabPoints;
		}
		/// @brief Gets PhaseCoherenceWithPrior which indicates the Interval should maintain phase coherency with the prior interval.
		[[nodiscard]] auto getPhaseCoherenceWithPrior() const
		{
			return this->phaseCoherenceWithPrior;
		}
		/// @brief Gets iterationsPerSignal which indicates the number of sequence repetitions to execute before signaling
		/// the processor of the availability of product data.
		/// If non-zero, the final iteration of this JobInterval signals the processor.
		/// If zero, do not signal the processor. However, regardless of this value, the final
		/// iteration of the final interval will always signal the processor.
		[[nodiscard]] auto getIterationsPerSignal() const
		{
			return this->iterationsPerSignal;
		}
		/// @brief Gets the maximum data rate per channel for this set of Element Groups.
		[[nodiscard]] auto getMaxDataRateBps() const
		{
			return this->maxDataRateBps;
		}
		/// @brief Gets the Maximum sample rate per channel for this set of Element Groups.
		[[nodiscard]] auto getMaxSampleRateHZ() const
		{
			return this->maxSampleRateHz;
		}
		/// @brief Gets an issue of a series of commands to any Local Functions
		/// associated with the selected Virtual Aperture Instance.
		/// @note there may be far more Local Function Instances than are associated with the
		/// selected VA Instance, but only the Local Functions under the purview of this VA
		/// can be commanded via the JobInterval API.
		/// These commands are executed at the start of a Job Interval,
		/// and there is no provision for manipulation of the Local Function during the interval.
		/// If the Tx/Rx events in the Event Sequence rely on these commands, then
		/// an appropriate delay may be supplied in the intervalStartingGap.
		/// If any error occurs in the issuance of the Local Function commands, the entire Job
		/// is considered to have failed.
		[[nodiscard]] const auto& getLfCommands() const
		{
			return this->localFunctionCommands;
		}
		/// @brief Specifies data to be provided inline with the output products resulting from
		/// execution of this JobInterval. The details of how this data is packaged are implementation defined.
		/// See MFAInfo::getMaxNumUserDefinedContextBytes() for limits.
		/// @note The data will be cast to the specified type and returned through std::optional. If that type is incompatible
		/// with the underlying type of userDefinedContextData, std::nullopt will be returned.
		/// including the handling of bad_any_cast exceptions.
		template <typename T>
		[[nodiscard]] const std::optional<std::reference_wrapper<const T>> getUserDefinedContextData() const
		{
			try
			{
				return std::optional<std::reference_wrapper<const T>>(std::cref(*std::any_cast<T>(&userDefinedContextData)));
			}
			catch(const std::bad_any_cast& e)
			{
				return std::nullopt;
			}
		}
		/// @brief Specifies data to be provided inline with the output products resulting from
		/// execution of this JobInterval. The details of how this data is packaged are implementation defined.
		/// See MFAInfo::getMaxNumUserDefinedContextBytes() for limits.
		/// @note The data will be cast to the specified type and returned through std::optional. If that type is incompatible
		/// with the underlying type of userDefinedContextData, std::nullopt will be returned.
		/// including the handling of bad_any_cast exceptions.
		template <typename T>
		[[nodiscard]] std::optional<std::reference_wrapper<T>> getUserDefinedContextData() // non-const
		{
			try
			{
				return std::optional<std::reference_wrapper<T>>(std::ref(*std::any_cast<T>(&userDefinedContextData)));
			}
			catch(const std::bad_any_cast& e)
			{
				return std::nullopt;
			}
		}
		/// @brief Gets the Endpoint Parameters relative to this Job.
		[[nodiscard]] const auto& getEndpointParameters() const
		{
			return this->productParams;
		}
		/// @brief Gets the Transmit power mode ID.
		[[nodiscard]] auto getTxPowerModeID() const
		{
			return this->txPowerModeID;
		}
		/// @brief gets the flag indicating if the Job Interval Status should be returned
		[[nodiscard]] auto getJobIntervalStatusEnable() const
		{
			return this->jobIntervalStatusEnable;
		}
		/// @brief Returns the total duration of this Job.
		[[nodiscard]] ams::util::math::Femtoseconds totalDuration() const
		{
			return intervalStartingGap + sequence.getDuration() * sequenceRepeatCount + calDuration + intervalEndingGap;
		}
		/// @brief Gets vector of Modulations associated with JobInterval.
		[[nodiscard]] auto& getModulations()
		{
			return this->modulations;
		}
		/// @brief Gets vector of Modulations associated with JobInterval.
		[[nodiscard]] const auto& getModulations() const
		{
			return this->modulations;
		}
		/// @brief Gets Activity ID.
		[[nodiscard]] const auto& getActivityId() const
		{
			return this->activityId;
		}

		/// @brief Returns the JobDetails ID
		[[nodiscard]] auto getJobDetailsID() const
		{
			return this->jobDetailsID;
		}

		/// @brief Gets executionType indicating how the interval is executed.
		/// @Required Implementation required for all RF MEL implementations
		[[nodiscard]] auto getExecutionType() const
		{
			return this->executionType;
		}

		/// @brief Sets IntervalStart which indicates the start time of this interval, relative to the start of the job.
		/// Multiple JobIntervals may start at the same time only if they contain no element
		/// groups in common. By default, each interval immediately follows the previous interval.
		/// Use quantizeDuration() when setting this value to ensure the MFA is able to execute
		/// as requested.
		void setIntervalStart(ams::util::math::Femtoseconds intervalStart_in)
		{
			this->intervalStart = intervalStart_in;
		}
		/// @brief Sets the list of transmit element groups for Virtual Apertures. May be left empty.
		void setApplicableElementGroups(const std::vector<ElementGroupLabel>& elemGroups)
		{
			this->applicableElementGroupLabels = elemGroups;
		}
		/// @brief Sets IntervalID which uniquely identifies this interval. Especially useful for tasks
		/// that get split into seprate job timelines.
		void setIntervalID(uint32_t id)
		{
			this->intervalID = id;
		}
		/// @brief Sets the period of time before the start of the sequence loop with no transmit
		/// or receive functionality.
		/// Use quantizeDuration() when setting this value to ensure the MFA is able to execute
		/// as requested.
		void setIntervalStartingGap(ams::util::math::Femtoseconds startGap)
		{
			this->intervalStartingGap = startGap;
		}
		/// @brief Sets the sequence of events to repeat.
		void setSequence(const Sequence& seq)
		{
			this->sequence = seq;
		}

		/// @brief Sets the number of times this Sequence should execute to perform Data Collection.
		void setSequenceRepeatCount(size_t repCount)
		{
			this->sequenceRepeatCount = repCount;
		}
		/// @brief Sets the amount of time after the end of the sequence loop to perform Calibration.
		/// Use quantizeDuration() when setting this value to ensure the MFA is able to execute
		/// as requested.
		void setCalDuration(ams::util::math::Femtoseconds duration)
		{
			this->calDuration = duration;
		}
		/// @brief Sets the period of time at the end of the sequence loop and Calibration with
		/// no transmit or receive functionality.
		/// Use quantizeDuration() when setting this value to ensure the MFA is able to execute
		/// as requested.
		void setIntervalEndingGap(ams::util::math::Femtoseconds endGap)
		{
			this->intervalEndingGap = endGap;
		}
		/// @brief Sets the list of stabilization points for the events in this Interval.
		void setStabPoints(const std::vector<PointingType>& stabPoints_in)
		{
			this->stabPoints = stabPoints_in;
		}
		/// @brief Sets PhaseCoherenceWithPrior which indicates the Interval should maintain phase coherency with the prior interval.
		void setPhaseCoherenceWithPrior(bool phaseChorence)
		{
			this->phaseCoherenceWithPrior = phaseChorence;
		}
		/// @brief Sets iterationsPerSignal which indicates the number of sequence repetitions to execute before signaling
		/// the processor of the availability of product data.
		/// If non-zero, the final iteration of this JobInterval signals the processor.
		/// If zero, do not signal the processor. However, regardless of this value, the final
		/// iteration of the final interval will always signal the processor.
		void setIterationsPerSignal(size_t iterationsPerSignal_in)
		{
			this->iterationsPerSignal = iterationsPerSignal_in;
		}
		/// @brief Sets the maximum data rate per channel for this set of Element Groups.
		void setMaxDataRateBps(double maxRate)
		{
			this->maxDataRateBps = maxRate;
		}
		/// @brief Sets the Maximum sample rate per channel for this set of Element Groups
		void setMaxSampleRateHZ(Frequency maxSampleRate)
		{
			this->maxSampleRateHz = maxSampleRate;
		}
		/// @brief Sets an issue of a series of commands to any Local Functions associated with the
		/// selected Virtual Aperture Instance.
		/// @note there may be far more Local Function Instances than are associated with the
		/// selected VA Instance, but only the Local Functions under the purview of this VA
		/// can be commanded via the JobInterval API.
		/// These commands are executed at the start of a Job Interval,
		/// and there is no provision for manipulation of the Local Function during the interval.
		/// If the Tx/Rx events in the Event Sequence rely on these commands, then
		/// an appropriate delay may be supplied in the intervalStartingGap.
		/// If any error occurs in the issuance of the Local Function commands, the entire Job
		/// is considered to have failed.
		void setLfCommands(const std::vector<LocalFunctionCommand>& lf)
		{
			this->localFunctionCommands = lf;
		}
		/// @brief Specifies data to be provided inline with the output products resulting from
		/// execution of this JobInterval. The details of how this data is packaged are implementation defined.
		/// See MFAInfo::getMaxNumUserDefinedContextBytes() for limits.
		void setUserDefinedContextData(const std::any& words)
		{
			this->userDefinedContextData = words;
		}
		/// @brief Sets the Endpoint Parameters relative to this Job.
		void setEndpointParameters(const ProductStreamParams& params)
		{
			this->productParams = params;
		}
		/// @brief Sets the transmit power mode ID.
		void setTxPowerModeID(TxPowerModeID txPowerModeID_in)
		{
			this->txPowerModeID = txPowerModeID_in;
		}
		/// @brief Sets the flag indicating if the Job Interval Status should be returned.
		void setJobIntervalStatusEnable(JobIntervalStatusEnable jobIntervalStatusEnable_in)
		{
			this->jobIntervalStatusEnable = jobIntervalStatusEnable_in;
		}
		/// @brief Sets vector of Modulation associated with JobInterval.
		void setModulations(const std::vector<Modulation>& modulations_in)
		{
			this->modulations = modulations_in;
		}
		/// @brief Sets Activity ID.
		void setActivityId(const std::vector<uint8_t>& activityId_in)
		{
			this->activityId = activityId_in;
		}

		/// @brief Sets JobDetailsID
		void setJobDetailsId(uint32_t jobDetailsID_in)
		{
			this->jobDetailsID = jobDetailsID_in;
		}

		/// @brief Sets executionType indicating how the interval is executed.
		/// @Required Implementation required for all RF MEL implementations
		void setExecutionType(ExecutionType executionType_in)
		{
			this->executionType = executionType_in;
		}

		/// @brief Add endpoint with EndpointID and DataPipeLabel to element with ElementGroupLabel
		void addEndpoint(const ElementGroupLabel& elementGroupLabel, EndpointID id, DataPipeLabel pipeLabel = ams::iface::rfmel::DataPipe::DEFAULT)
		{
			dataPaths[elementGroupLabel].emplace(pipeLabel, id);
		}
		/// @brief Add endpoints with EndpointID and DataPipeLabel to element with ElementGroupLabel
		void addEndpoints(const ElementGroupLabel& elementGroupLabel, std::unordered_map<DataPipeLabel, EndpointID> const& map)
		{
			dataPaths[elementGroupLabel] = map;
		}
		/// @brief Set endpoints for each ElementGroupLabel
		void setEndpoints(const ElementGroupToEndpointConnections& endpoints)
		{
			dataPaths = endpoints;
		}
		/// @brief Get all endpoints
		const auto& getEndpoints() const
		{
			return this->dataPaths;
		}
		/// @brief Get all endpoints
		const auto& getEndpoints()
		{
			return this->dataPaths;
		}

	private:
		// Indicates the start time of this interval, relative to the start of the job.
		// Multiple JobIntervals may start at the same time only if they contain no element
		// groups in common.
		// By default, each interval immediately follows the previous interval.
		ams::util::math::Femtoseconds intervalStart = ContinueFromPrevious;

		// List of element groups for Virtual Apertures with multiple Rx/Tx
		// element groups. May be left empty otherwise.
		std::vector<ElementGroupLabel> applicableElementGroupLabels;

		// Uniquely identifies this interval. Especially useful for tasks
		// that get split into seprate job timelines.
		uint32_t intervalID = 0;

		// The period of time before the start of the sequence loop with no transmit
		// or receive functionality.
		// Use quantizeDuration() when setting this value to ensure the MFA is able to execute
		// as requested.
		ams::util::math::Femtoseconds intervalStartingGap{0};

		// The sequence of events to repeat
		Sequence sequence;

		// The number of times this Sequence should execute to perform Data Collection.
		size_t sequenceRepeatCount{1};

		// The amount of time after the end of the sequence loop to perform Calibration
		// Use quantizeDuration() when setting this value to ensure the MFA is able to execute
		// as requested.
		ams::util::math::Femtoseconds calDuration{0};

		// The period of time at the end of the sequence loop and Calibration with
		// no transmit or receive functionality.
		// Use quantizeDuration() when setting this value to ensure the MFA is able to execute
		// as requested.
		ams::util::math::Femtoseconds intervalEndingGap{0};

		// List of stabilization points for the events in this Interval
		std::vector<PointingType> stabPoints;

		ProductStreamParams productParams;

		// Indicates the Interval should maintain phase coherency with the prior interval
		bool phaseCoherenceWithPrior = false;

		// Indicates the number of sequence repetitions to execute before signaling
		// the processor of the availability of product data.
		// If non-zero, the final iteration of this JobInterval signals the processor.
		// If zero, do not signal the processor.
		// However, regardless of this value, the final
		// iteration of the final interval will always signal the processor.
		size_t iterationsPerSignal{0};

		// The maximum data rate per channel for this set of Element Groups
		double maxDataRateBps{0}; // bits per second

		// The Maximum sample rate per channel for this set of Element Groups
		Frequency maxSampleRateHz{0}; // samples per second

		// uniquely identifies MFA-specific commandable Transmit Power Configuration
		// Refer to MFA provider documentation for any known limitations wrt switching between
		// Tx Power Mode IDs.
		TxPowerModeID txPowerModeID{0};

		// Issues a series of commands to any Local Functions associated with the
		// selected Virtual Aperture Instance.
		// @note there may be far more Local Function Instances than are associated with the
		// selected VA Instance, but only the Local Functions under the purview of this VA
		// can be commanded via the JobInterval API.
		// These commands are executed at the start of a Job Interval,
		// and there is no provision for manipulation of the Local Function during the interval.
		// If the Tx/Rx events in the Event Sequence rely on these commands, then
		// an appropriate delay may be supplied in the intervalStartingGap.
		// If any error occurs in the issuance of the Local Function commands, the entire Job
		// is considered to have failed.
		std::vector<LocalFunctionCommand> localFunctionCommands;

		// Specifies data to be provided inline with the output products resulting from
		// execution of this JobInterval. The details of how this data is packaged are implementation defined.
		// See MFAInfo::getMaxNumUserDefinedContextBytes() for limits.
		std::any userDefinedContextData;

		// Specify if Job Interval Status should be provided to the jobIntervalStatusCallback upon
		// completion of the Job Interval, regardless of success, or only upon a failure/extension event.
		// If Always, the Job Interval Status is always returned at the completion of the Job Interval.
		// If OnException, the Job Interval Status is only returned if the Job Interval failed or impacted by an extension event.
		// If Never, the Job Interval Status is never returned.
		JobIntervalStatusEnable jobIntervalStatusEnable = JobIntervalStatusEnable::Never;

		std::vector<Modulation> modulations;

		std::vector<uint8_t> activityId;

		uint32_t jobDetailsID = 0;

		ElementGroupToEndpointConnections dataPaths;
		/// Identifies the type of interval to be executed.
		ExecutionType executionType = ExecutionType::Normal;
	};
} // namespace ams::iface::rfmel
