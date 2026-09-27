#pragma once

#include <math/units/UTCTime.h>
#include <rfmel/jobs/CommonModulations.h>
#include <rfmel/jobs/PulseDetectionSettings.h>
#include <rfmel/jobs/WaveformStream.h>
#include <rfmel/jobs/Weights.h>

#include <map>
#include <vector>

namespace ams::iface::rfmel
{
	/// @brief The JobEvent is the lowest level of control a Service can command on the MFA.
	/// See also ams::iface::rfmel::TransmitEvent and ams::iface::rfmel::ReceiveEvent.
	/// @Required This class provides required RF MEL functionality for supporting Job Events,
	/// and must be provided by the implementer in all RF MEL implementations.
	/// See individual members for details.
	class JobEvent
	{
	public:
		JobEvent() = default;
		virtual ~JobEvent() = default;
		JobEvent(const JobEvent&) = default;			// copy constructor
		JobEvent(JobEvent&&) = default;					// move constructor
		JobEvent& operator=(const JobEvent&) = default; // copy operator
		JobEvent& operator=(JobEvent&&) = default;		// move operator

		/// @brief Indicates whether the given event transmits or receives
		/// @Required This enumeration provides data definition associated with Job Events
		/// and must be included as-is in all RF MEL implementations.
		enum class Direction
		{
			None,
			Transmit,
			Receive
		};

		/// @brief Indicates the type of termination for an event.
		/// @Required This enumeration provides data definition associated with Job Events
		/// and must be included as-is in all RF MEL implementations.
		enum class EventTerminationType
		{
			/// When a conditional event is executed while this event is in progress, the inhibit
			/// flag indicates that this event continues to execute in parallel with the conditional
			/// event, but all resulting outputs (IQ, PDW) associated with this event are suppresse
			/// while the conditional event is executing.
			InhibitEvent,
			/// When a conditional event is executed while this event is in progress, the interrupt
			/// flag indicates that this event is terminated. When the conditional event is executed
			/// before the start of this event, then this event is not executed.
			CancelEvent
		};

		/// @brief Get the start time of the JobEvent, relative to the start of the Sequence.
		[[nodiscard]] auto getStart() const
		{
			return this->start;
		}
		/// @brief Get the duration of the event.
		[[nodiscard]] auto getDuration() const
		{
			return this->duration;
		}
		/// @brief Gets Center Frequency of element groups.
		[[nodiscard]] auto getCenterFrequency() const
		{
			return this->centerFrequency;
		}
		/// @brief Gets the Polarization of the event.
		[[nodiscard]] const auto& getPolarization() const
		{
			return this->polarization;
		}
		/// @brief Gets the Polarization of the event.
		[[nodiscard]] auto& getPolarization()
		{
			return this->polarization;
		}
		/// @brief Gets the PolarizationBeemSteerCorrection, which determines whether the MFA
		/// should correct for the effects of beam steering on polarization.
		[[nodiscard]] auto getPolarizationBeemSteerCorrection() const
		{
			return this->enablePolarizationBeamSteerCorrection;
		}
		/// @brief Gets the Phase Offset of a JobEvent.
		[[nodiscard]] auto getPhaseOffset() const
		{
			return this->phaseOffset;
		}
		/// @brief Gets the index of the stabilization point (i.e. beam pointing) to be used for this JobEvent.
		[[nodiscard]] auto getStabPointIndex() const
		{
			return this->stabPointIndex;
		}
		/// @brief Gets the one-dimensional vector of weights which maps to a 2D region of an aperture face.
		/// This provides a means of accomplishing beam tapering.
		[[nodiscard]] const auto& getWeights() const
		{
			return this->weights;
		}
		/// @brief Indicates whether the given event transmits or receives.
		[[nodiscard]] const auto getDirection() const
		{
			return this->direction;
		}
		/// @brief Gets eventID which uniquely identifies this event.
		[[nodiscard]] auto getEventID() const
		{
			return this->eventID;
		}
		/// @brief Gets executionType indicating how the event is executed.
		/// @Required Implementation required for all RF MEL implementations
		[[nodiscard]] auto getExecutionType() const
		{
			return this->executionType;
		}
		/// @brief Gets eventTerminationType indicating how the event is terminated.
		[[nodiscard]] auto getEventTerminationType() const
		{
			return this->eventTerminationType;
		}
		/// @brief Gets allowDelayStart boolean indicating whether or not the event should be delayed.
		[[nodiscard]] auto getAllowDelayStart() const
		{
			return this->allowDelayStart;
		}
		/// @brief Gets iterationHoldCount which is the number of iterations
		/// needed before executing the sequence.
		[[nodiscard]] auto getIterationHoldCount() const
		{
			return this->iterationHoldCount;
		}
		/// @brief Gets iterationTerminationCount Number of iterations from the end of the
		/// sequenceRepeatCount in which this event is no longer executed.
		[[nodiscard]] auto getIterationTerminationCount() const
		{
			return this->iterationTerminationCount;
		}
		/// @brief Gets the channelization enabled/diabled status of the job event
		[[nodiscard]] auto getChannelizationEnabled() const
		{
			return this->channelizationEnabled;
		}
		/// @brief Gets the pulse detection settings of the job event
		[[nodiscard]] auto& getPulseDetectionSettings() const
		{
			return this->pulseDetectionSettings;
		}

		[[nodiscard]] auto getElementGroupLabel() const
		{
			return this->elementGroupLabel;
		}

		/// @brief Set the start time of the JobEvent, relative to the Sequence start time.
		/// Use quantizeDuration() when setting this value to ensure the MFA is able to
		/// execute as requested.
		void setStart(ams::util::math::Femtoseconds start_in)
		{
			this->start = start_in;
		}
		/// @brief Set the duration of the JobEvent.
		/// Use quantizeDuration() when setting this value to ensure the MFA is able to execute
		/// as requested.
		void setDuration(ams::util::math::Femtoseconds duration_in)
		{
			this->duration = duration_in;
		}
		/// @brief Sets the Center Frequency for the ElementGroups executing this JobEvent.
		void setCenterFrequency(Frequency centerFrequency_in)
		{
			this->centerFrequency = centerFrequency_in;
		}
		/// @brief Sets the Polarization for this JobEvent.
		void setPolarization(const std::vector<StokesVector>& polarization_in)
		{
			this->polarization = polarization_in;
		}
		/// @brief Sets the PolarizationBeemSteerCorrection which determines whether the MFA
		/// should correct for the effects of beam steering on polarization.
		void setPolarizationBeemSteerCorrectionEnabled(bool enable)
		{
			this->enablePolarizationBeamSteerCorrection = enable;
		}
		/// @brief Sets the Phase Offset of a JobEvent.
		void setPhaseOffset(Angle phaseOffset_in)
		{
			this->phaseOffset = phaseOffset_in;
		}
		/// @brief Sets the index of the stabilization point (i.e. beam pointing) to be
		/// used for this JobEvent.  The vector of stabilization points into which this
		/// indexes is defined on the JobInterval.
		void setStabPointIndex(size_t stabPointIndex_in)
		{
			this->stabPointIndex = stabPointIndex_in;
		}
		/// @brief Sets the one-dimensional vector of weights which maps to a 2D region of an aperture face.
		/// This provides a means of accomplishing beam tapering.
		void setWeights(const std::map<WeightType, Weights*>& weights_in)
		{
			this->weights = weights_in;
		}
		/// @brief Sets eventID which uniquely identifies this event.
		void setEventID(JobEventID id)
		{
			this->eventID = id;
		}
		/// @brief Sets executionType indicating how the event is executed.
		/// @Required Implementation required for all RF MEL implementations
		void setExecutionType(ExecutionType executionType_in)
		{
			this->executionType = executionType_in;
		}
		/// @brief Sets eventTerminationType indicating how the event is terminated.
		void setEventTerminationType(EventTerminationType eventTerminationType_in)
		{
			this->eventTerminationType = eventTerminationType_in;
		}
		/// @brief Sets allowDelayStart boolean indicating whether or not the event should be delayed.
		void setAllowDelayStart(bool enable)
		{
			this->allowDelayStart = enable;
		}
		/// @brief Sets iterationHoldCount which is the number of iterations
		/// needed before executing the sequence.
		void setIterationHoldCount(size_t holdCount)
		{
			this->iterationHoldCount = holdCount;
		}
		/// @brief Sets iterationTerminationCount Number of iterations from the end of the
		/// sequenceRepeatCount in which this event is no longer executed.
		void setIterationTerminationCount(size_t terminationCount)
		{
			this->iterationTerminationCount = terminationCount;
		}
		/// @brief Sets channelization as enabled/disabled for the job event
		void setChannelizationEnabled(bool channelizationEnabled_in)
		{
			this->channelizationEnabled = channelizationEnabled_in;
		}

		/// @brief Sets pulse detection settings for the job event
		void setPulseDetectionSettings(const PulseDetectionSettings& pulseDetectionSettings_in)
		{
			this->pulseDetectionSettings = pulseDetectionSettings_in;
		}

		void setElementGroupLabel(const ElementGroupLabel& label)
		{
			this->elementGroupLabel = label;
		}

	protected:
		explicit JobEvent(const Direction direction_in) : direction(direction_in)
		{
		}

	private:
		/// Start time of the event. For a normal event the start is relative to the
		/// current sequence iteration start or to the initial sequence iteration.
		/// The initial sequence iteration starts after the intervalStartingGap specified in
		/// the jobInterval.
		/// Use quantizeDuration() when setting this value to ensure the MFA is able to execute
		/// as requested.
		ams::util::math::Femtoseconds start{0};

		// Duration of the job. Use quantizeDuration() when setting this value to
		// ensure the MFA is able to execute as requested.
		ams::util::math::Femtoseconds duration{0};

		Frequency centerFrequency{0};

		// Number of iterations that must be completed before this event will be
		// executed within the sequence. For example, if the iterationHoldCount is equal
		// to 5, then this event will not occur until the 6th iteration of the sequence.
		size_t iterationHoldCount{0};

		// Number of iterations from the end of the sequenceRepeatCount in which
		// this event is no longer executed within the sequence.
		// For example, if the iterationTerminationCount is equal to 5, then this event
		// will stop occurring when there are 5 iterations of the sequence remaining.
		size_t iterationTerminationCount{0};

		// The vector can be empty (default polarization), size 1 (single-pol),
		// or size 2 (dual-pol) depending on the capabilities of the MFA.
		// The reference frame for polarization is the array face.
		std::vector<StokesVector> polarization;

		// Determines whether the MFA should correct for the effects of beam steering on polarization.
		bool enablePolarizationBeamSteerCorrection = false;

		// Serves as the initial phase for phase-modulated waveforms
		Angle phaseOffset{0};

		size_t stabPointIndex{0};

		std::map<WeightType, Weights*> weights;

		Direction direction{Direction::None};

		/// Uniquely identifies this event. Especially useful for referencing
		/// conditionally executed events and reporting event-level status
		JobEventID eventID = 0;

		// Identifies the type of event to be executed. Events that are periodic or
		// are not executed at a specific time of day are tagged as a periodic event [default].
		// Events that must be executed at a specific time of day are tagged as time of day events.
		// Events that are only executed if triggered by an event are tagged as conditional events.
		ExecutionType executionType = ExecutionType::Normal;

		EventTerminationType eventTerminationType = EventTerminationType::InhibitEvent;

		/// When the conditional event is executed prior to the start of this event, then this event start may be delayed until
		/// after the conditional event is executed, as time in the Job Sequence allows and if supported by the MFA.
		bool allowDelayStart = false;

		/// Indicates if channelization is enabled for the job event
		bool channelizationEnabled{false};

		PulseDetectionSettings pulseDetectionSettings;

		ElementGroupLabel elementGroupLabel;
	};
	/// @brief The TransmitEvent class contains parameters specific to transmit functionality.
	/// @RequiredIfTransmit This class provides data definition in support of required RF MEL functionality
	/// specific to transmit and must be included as-is in all RF MEL implementations if the associated
	/// MFA supports transmit.
	class TransmitEvent : public JobEvent
	{
	public:
		TransmitEvent() : JobEvent(Direction::Transmit)
		{
		}
		/// @brief Get transmit module index.
		[[nodiscard]] auto getModIndex() const
		{
			return this->modIndex;
		}
		/// @brief Get transmit attentuation in dB.
		[[nodiscard]] const auto& getTxAtten_dB() const
		{
			return this->txAtten_dB;
		}
		/// @brief Get transmit rise time duration.
		[[nodiscard]] auto getRiseDuration() const
		{
			return this->riseDuration;
		}
		/// @brief Get transmit fall time duration.
		[[nodiscard]] auto getFallDuration() const
		{
			return this->fallDuration;
		}
		/// @brief Gets the list of transmit element groups for a specific event. May be left empty.
		[[nodiscard]] const auto& getApplicableTxElementGroups() const
		{
			return this->applicableTxElementGroups;
		}
		/// @brief Set transmit module index.
		void setModIndex(const std::vector<int>& modIndex_in)
		{
			this->modIndex = modIndex_in;
		}
		/// @brief Set transmit attentuation in dB.
		void setTxAtten_dB(const double txAtten_dB_in)
		{
			this->txAtten_dB = txAtten_dB_in;
		}
		/// @brief Set transmit rise time duration.
		void setRiseDuration(ams::util::math::Femtoseconds duration_in)
		{
			this->riseDuration = duration_in;
		}
		/// @brief Set transmit fall time duration.
		void setFallDuration(ams::util::math::Femtoseconds duration_in)
		{
			this->fallDuration = duration_in;
		}

		/// @brief Sets the list of transmit element groups for a specific event. May be left empty.
		void setApplicableTxElementGroups(const std::vector<size_t>& elemGroups)
		{
			this->applicableTxElementGroups = elemGroups;
		}

	private:
		// The maximum available Tx attenuation (dB) is defined by the Tx Power Mode characteristics, but the
		// available Tx attenuation may change at runtime based on real-time calibration.
		// The Tx attenuation step size is defined by the Tx Power Mode characteristics. The
		// real-time maximum Tx attenuation for a given Tx Element Group is retrieved
		// by VirtualApertureDatabase::getMaxTxAttenuation
		double txAtten_dB{0};

		// A modIndex value of -1 represents no modulation
		static constexpr int NO_MODULATION{-1};

		// The index value(s) into the vector of modulations on the JobInterval. Value is initialized to NO_MODULATION by default
		// A repeating event may specify a list of modulations that are iterated for each event. When the end of the
		// list is reached, the list is repeated from the beginning.
		std::vector<int> modIndex{NO_MODULATION};

		// Rise duration of the leading edge of this event.  If set to 0 the MFA will not attempt to shape
		// the leading edge of the transmit pulse.  Depending on the MFA riseDuration may not be supported, in which
		// case this field will be ignored and instead the skill producing the IQ samples will be responsible for
		// shaping the IQ samples.  This field may also be used conditionally based on the Tx Power Mode.  The shape
		// of the slope is left up to the MFA implementation.  Note that riseDuration plus fallDuration cannot exceed
		// the event's total duration.  Failing this check will result in a failure reported via JobStatus and the
		// job interval the event is part of will not be executed.
		ams::util::math::Femtoseconds riseDuration{0};

		// Fall duration of the trailing edge of this event.  If set to 0 the MFA will not attempt to shape
		// the trailing edge of the transmit pulse.  Depending on the MFA fallDuration may not be supported, in which
		// case this field will be ignored and instead the skill producing the IQ samples will be responsible for
		// shaping the IQ samples.  This field may also be used conditionally based on the Tx Power Mode.  The shape
		// of the slope is left up to the MFA implementation.  Note that riseDuration plus fallDuration cannot exceed
		// the event's total duration.  Failing this check will result in a failure reported via JobStatus and the
		// job interval the event is part of will not be executed.
		ams::util::math::Femtoseconds fallDuration{0};

		// List of element groups for Virtual Apertures with multiple Tx
		// element groups. May be left empty otherwise.
		std::vector<size_t> applicableTxElementGroups;
	};
	/// @brief The ReceiveEvent class contains parameters specific to receive functionality.
	/// @RequiredIfReceive This class provides data definition in support of required RF MEL functionality
	/// specific to receive and must be included as-is in all RF MEL implementations if the associated
	/// MFA supports receive.
	class ReceiveEvent : public JobEvent
	{
	public:
		ReceiveEvent() : JobEvent(Direction::Receive)
		{
		}

		/// @brief Gets the number of iterations of this receive event that are needed to
		/// execute Automatic Gain Control(AGC) data collections.
		[[nodiscard]] auto getNumIterationProcessingAGC() const
		{
			return this->numIterationProcessingAGC;
		}
		/// @brief Gets the number of iterations of this receive event that are ignored
		/// after the Automatic Gain Control (AGC) data collections.
		[[nodiscard]] auto getNumIterationIgnoredPostAGC() const
		{
			return this->numIterationIgnoredPostAGC;
		}
		/// @brief Gets Sample Frequency from Rx Events.
		[[nodiscard]] auto getSampleFrequency() const
		{
			return this->sampleFrequency;
		}
		/// @brief Gets the list of receive element groups for specific event. May be left empty.
		[[nodiscard]] const auto& getApplicableRxElementGroups() const
		{
			return this->applicableRxElementGroups;
		}
		/// @brief Gets the maximum allowed amount of time for a local function to extend the Job event, if supported.
		/// A value of zero indicates that extending Job event is not allowed.
		[[nodiscard]] auto getMaxExtensionDuration() const
		{
			return this->maxExtensionDuration;
		}

		/// @brief Sets Sample Frequency for Rx Events.
		void setSampleFrequency(Frequency sampleFrequency_in)
		{
			this->sampleFrequency = sampleFrequency_in;
		};
		/// @brief sets the number of iterations of this receive event that are needed to execute
		/// Automatic Gain Control(AGC) data collections.
		void setNumIterationProcessingAGC(size_t numIterations)
		{
			this->numIterationProcessingAGC = numIterations;
		}
		/// @brief Sets the number of iterations of this receive event that are ignored after the
		/// Automatic Gain Control (AGC) data collections.
		void setNumIterationIgnoredPostAGC(size_t numIterationsPostAGC)
		{
			this->numIterationIgnoredPostAGC = numIterationsPostAGC;
		}
		/// @brief Sets the list of receive element groups for a specific event. May be left empty.
		void setApplicableRxElementGroups(const std::vector<size_t>& elemGroups)
		{
			this->applicableRxElementGroups = elemGroups;
		}
		/// @brief Sets the maximum allowed amount of time for a local function to extend the Job Event, if supported.
		/// A value of zero indicates that extending  Job Event is not allowed.
		/// Use quantizeDuration() when setting this value to ensure the MFA is able to execute
		/// as requested.
		void setMaxExtensionDuration(ams::util::math::Femtoseconds maxExtensionDuration_in)
		{
			this->maxExtensionDuration = maxExtensionDuration_in;
		}

	private:
		Frequency sampleFrequency{0};

		// The number of iterations of this receive event that are needed to execute Automatic Gain Control(AGC) data collections.
		// The execution of AGC does not start until after the completion of the iterationHoldCount for this Rx event
		size_t numIterationProcessingAGC{0};

		// The number of iterations of this receive event that are ignored after the Automatic Gain Control (AGC) data collections
		// (numIterationProcessingAGC) to allow for additional AGC processing time. This value should correspond to the returned value of
		//	getAGCProcessingTime(). The execution of AGC does not start until after the completion of both the iterationHoldCount and
		//	numIterationProcessingAGC for this Rx event.
		size_t numIterationIgnoredPostAGC{0};

		// List of element groups for Virtual Apertures with multiple Rx
		// element groups. May be left empty otherwise.
		std::vector<size_t> applicableRxElementGroups;

		// Allows the Service to specify an optional time extension for the Job Event, if supported.
		// A value of zero indicates that extending  Job Event is not allowed.
		// Note that allowing the Job Event to be extended may result in a timeline shift or deletion of subsequent
		// Job Events, based on the value of the eventTerminationType flag. See the Job Event status provided in JobDetail
		// to determine if a Job Event has been extended or subsequent Job Event deleted from the Job.
		// Use quantizeDuration() when setting this value to ensure the MFA is able to execute
		// as requested.
		ams::util::math::Femtoseconds maxExtensionDuration{0};
	};
} // namespace ams::iface::rfmel
