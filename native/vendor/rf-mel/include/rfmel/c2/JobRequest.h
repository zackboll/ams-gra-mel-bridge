#pragma once
#include <boost/lexical_cast.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <math/units/UTCTime.h>
#include <rfmel/rfmeltypes/RFMELTypes.h>

#include <rfmel/c2/ElementGroupCommand.h>
#include <rfmel/c2/ElementGroupCommandList.h>
#include <rfmel/jobs/Pointing.h>

#include <any>
#include <map>
#include <memory>
#include <set>
#include <vector>
#include <cstdint>
#include <functional>

namespace ams::iface::rfmel
{
	/// @brief A callback function type associated with Job Requests that prompts the sending of additional Job Interval Batches (JIBs)
	/// during Job Request execution. A JIB is a subset of a Job Interval Vector (JIV).
	/// @RequiredIfMFADrivenControls This type is intended for use only with the conditionally required functionality:
	using NextJIBFunction = std::function<void(const std::shared_ptr<ams::iface::rfmel::VirtualAperture>,
											   std::shared_ptr<ams::iface::rfmel::JobDetail>, const std::any, uint32_t lastJIBExecutedIdx)>;

	/// @brief Denotes the reason for rejection of a JobRequest
	/// @RequiredIfMFADrivenControls This enum supports the conditionally required RequestRejected callback.
	enum class RejectedReason
	{
		INVALID_REQUEST,	///< The job request was invalid
		JOB_INTERRUPTED,	///< The job was interupted
		UNABLE_TO_SCHEDULE, ///< The MFA was unable to schedule the job
		NO_REASON_PROVIDED, ///< The MFA did not provide a reason
	};

	/// @brief Describes how a subset of the Virtual Aperture is requested to be used,
	/// including frequency, bandwidth, and pointing.
	/// @note Every JobRequest must specify at least one ElementGroup.
	/// @Required This class provides data definition in support of Virtual Apertures and the Jobs Interface, which is
	/// required RF MEL functionality and must be included as-is in all RF MEL implementations.
	class JobRequest
	{
	public:
		JobRequest() = default;
		~JobRequest() = default;
		JobRequest(const JobRequest&) = default;
		JobRequest(JobRequest&&) = default;
		JobRequest& operator=(const JobRequest&) = default;
		JobRequest& operator=(JobRequest&&) = default;

		/// @brief Gets the priority of the Job Request.
		[[nodiscard]] auto getPriority() const
		{
			return this->priority;
		}
		/// @brief Gets the precedence (within priority) of the Job Request.
		[[nodiscard]] auto getPrecedenceWithinPriority() const
		{
			return this->precedenceWithinPriority;
		}
		/// @brief Gets the minimum start time of the Job Request.
		[[nodiscard]] auto getMinStartTime() const
		{
			return this->minStartTime;
		}
		/// @brief Gets the maximum time of completion of the Job Request.
		[[nodiscard]] auto getMaxCompleteTime() const
		{
			return this->maxCompleteTime;
		}
		/// @brief Gets the duration of the Job Request.
		[[nodiscard]] auto getDuration() const
		{
			return this->duration;
		}
		/// @brief Gets the estimated stab point of the Job Request.
		[[nodiscard]] const auto& getEstimatedStabPoint() const
		{
			return this->estimatedStabPoint;
		}
		/// @brief Gets the estimated stab point of the Job Request (non-const).
		[[nodiscard]] auto& getEstimatedStabPoint()
		{
			return estimatedStabPoint;
		}
		/// @brief Gets the receive (Rx) element groups of the Job Request.
		[[nodiscard]] const auto getRxElementGroups() const
		{
			ElementGroupCommandList result;
			for(std::shared_ptr<ElementGroupCommand> elementGroupCommand : elementGroupCommands)
			{
				if(elementGroupCommand->getMode() == Mode::RX)
				{
					result.push_back(elementGroupCommand);
				}
			}

			return result;
		}
		/// @brief Gets the transmit (Tx) element groups of the Job Request.
		[[nodiscard]] const auto getTxElementGroups() const
		{
			ElementGroupCommandList result;
			for(std::shared_ptr<ElementGroupCommand> elementGroupCommand : elementGroupCommands)
			{
				if(elementGroupCommand->getMode() == Mode::TX)
				{
					result.push_back(elementGroupCommand);
				}
			}

			return result;
		}
		/// @brief Gets the full list of element groups (Rx and Tx) of the Job Request.
		[[nodiscard]] const auto& getElementGroups() const
		{
			return this->elementGroupCommands;
		}
		/// @brief Gets the Capability UUID associated with the Job Request.
		[[nodiscard]] auto getCapabilityId() const
		{
			return this->capabilityId;
		}
		/// @brief Gets the Activity UUID associated with the Job Request.
		[[nodiscard]] auto getActivityId() const
		{
			return this->activityId;
		}
		/// @brief Gets the Job Request's ID.
		[[nodiscard]] auto getRequestId() const
		{
			return this->requestId;
		}
		/// @brief Gets the specific instance(s) of a Virtual Aperture associated with the Job Request.
		[[nodiscard]] const auto& getInstanceSelection() const
		{
			return this->instanceSelection;
		}
		/// @brief Gets the specific instance(s) of a Virtual Aperture associated with the Job Request (non-const).
		[[nodiscard]] auto& getInstanceSelection()
		{
			return instanceSelection;
		}
		/// @brief Gets whether the Job Request is interruptable.
		[[nodiscard]] auto getIsInterruptable() const
		{
			return this->isInterruptable;
		}
		/// @brief Gets the Transmit Power Mode IDs.
		[[nodiscard]] auto getTxPowerModeIDs() const
		{
			return this->txPowerModeIDs;
		}
		/// @brief Gets the callback function to be triggered when the MFA is ready for the next JIB.
		[[nodiscard]] const NextJIBFunction& getSendNextJIBatchCallback() const
		{
			return this->sendNextJIBatchCallback;
		}
		/// @brief Gets the number of sendNextJIBatchCallbacks to be triggered.
		[[nodiscard]] auto getNumJIBs() const
		{
			return this->numJIBs;
		}
		/// @brief Gets the Skill-defined context information associated with the Job.
		[[nodiscard]] const std::any& getCallbackContext() const
		{
			return this->callbackContext;
		}
		/// @brief Gets the Rejected Request Callback function.
		[[nodiscard]] const std::function<void(std::any, RejectedReason)>& getRequestRejectedCallback() const
		{
			return this->requestRejectedCallback;
		}
		/// @brief Gets the lookahead time.
		[[nodiscard]] const ams::util::math::Femtoseconds getLookAheadTime() const
		{
			return this->lookAheadTime;
		}

		/// @brief Sets the priority of the Job Request.
		void setPriority(Priority priority_in)
		{
			this->priority = priority_in;
		}
		/// @brief Sets the precedence (within priority) of the Job Request.
		void setPrecedenceWithinPriority(PrecedenceWithinPriority precedenceWithinPriority_in)
		{
			this->precedenceWithinPriority = precedenceWithinPriority_in;
		}
		/// @brief Sets the minimum start time of the Job Request.
		void setMinStartTime(ams::util::math::UTCTime minStartTime_in)
		{
			this->minStartTime = minStartTime_in;
		}
		/// @brief Sets the maximum time of completion of the Job Request.
		void setMaxCompleteTime(ams::util::math::UTCTime maxCompleteTime_in)
		{
			this->maxCompleteTime = maxCompleteTime_in;
		}
		/// @brief Sets the duration of the Job Request.
		void setDuration(ams::util::math::Femtoseconds duration_in)
		{
			this->duration = duration_in;
		}
		/// @brief Sets the estimated stab point of the Job Request.
		void setEstimatedStabPoint(const PointingType& estimatedStabPoint_in)
		{
			this->estimatedStabPoint = estimatedStabPoint_in;
		}
		/// @brief Adds a single element group to the list of element groups for the Job Request.
		void addElementGroup(std::shared_ptr<ElementGroupCommand> elementGroup)
		{
			this->elementGroupCommands.emplace_back(elementGroup);
		}
		/// @brief Adds multiple element groups to the list of element groups for the Job Request.
		void addElementGroup(const ElementGroupCommandList& elementGroups)
		{
			for(std::shared_ptr<ElementGroupCommand> elementGroup : elementGroups)
			{
				this->addElementGroup(elementGroup);
			}
		}
		/// @brief Sets the Capability UUID associated with the Job Request.
		void setCapabilityId(const std::vector<uint8_t>& capabilityId_in)
		{
			this->capabilityId = capabilityId_in;
		}
		/// @brief Sets the Activity UUID associated with the Job Request.
		void setActivityId(const std::vector<uint8_t>& activityId_in)
		{
			this->activityId = activityId_in;
		}
		/// @brief Sets the Job Request's ID
		void setRequestId(const uint32_t requestId_in)
		{
			this->requestId = requestId_in;
		}
		/// @brief Sets the specific instance(s) of a Virtual Aperture to be associated with the Job Request.
		void setInstanceSelection(const std::vector<VirtualApertureInstanceID>& instanceList)
		{
			this->instanceSelection = instanceList;
		}
		/// @brief Sets whether the Job Request is interruptable.
		void setIsInterruptable(bool isInterruptable_in)
		{
			this->isInterruptable = isInterruptable_in;
		}
		/// @brief Sets the Transmit power mode IDs.
		void setTxPowerModeIDs(const std::set<TxPowerModeID>& txPowerModeIDs_in)
		{
			this->txPowerModeIDs.clear();
			this->txPowerModeIDs = txPowerModeIDs_in;
		}
		/// @brief Sets the Callback Function to be triggered when the MFA is ready for the next JIB
		/// and associated Skill-defined context data.
		void setSendNextJIBatchCallback(const NextJIBFunction& cb, const std::any& context = nullptr)
		{
			this->sendNextJIBatchCallback = cb;
			this->callbackContext = context;
		}
		/// @brief Sets the number of sendNextJIBatchCallbacks to be triggered.
		void setNumJIBs(const uint32_t numJIBs_in)
		{
			this->numJIBs = numJIBs_in;
		}
		/// @brief Sets the Rejected Request Callback function.
		void setRequestRejectedCallback(const std::function<void(std::any, RejectedReason)>& cb)
		{
			this->requestRejectedCallback = cb;
		}
		/// @brief Sets the lookahead time.
		void setLookAheadTime(ams::util::math::Femtoseconds lookAheadTime_in)
		{
			this->lookAheadTime = lookAheadTime_in;
		}

	private:
		/// The priority of the Job Request.
		Priority priority{0};
		/// The precedence (within priority) of the Job Request, which may be used sort Job Requests of the same priority level.
		PrecedenceWithinPriority precedenceWithinPriority{0};
		/// The earliest possible start time of the Job.
		ams::util::math::UTCTime minStartTime;
		/// The latest possible time of completion of the Job.
		ams::util::math::UTCTime maxCompleteTime;
		/// The full duration of the Job, in fsec.
		ams::util::math::Femtoseconds duration{0};
		/// The estimated stab point of the Job.
		PointingType estimatedStabPoint;
		/// The list of element groups associated with the Job Request.
		ElementGroupCommandList elementGroupCommands;
		/// The Capability UUID associated with the Job Request.
		/// @note This Capability UUID corresponds to a Capability previously commanded by the Mission Processing Subsystem (MPS)
		/// using a UCI X_Command message.
		std::vector<uint8_t> capabilityId{0};
		/// The Activity UUID associated with the Job Request.
		/// @note This Activity UUID corresponds to a Capability previously published by the Skill/Service using a UCI X_Activity message.
		std::vector<uint8_t> activityId{0};
		/// The Job Request ID.
		/// @note This ID must be unique for the corresponding Capability and Activity ID.
		std::uint32_t requestId{0};
		/// A specific instance or instances of a Virtual Aperture to be associated with the Job Request.
		/// @note If none of the selected instances could be allocated, then the request will be rejected.
		/// @note Leaving this list empty allows the Scheduler to select any instance that satisfies
		/// the job request criteria.
		std::vector<VirtualApertureInstanceID> instanceSelection;
		/// Indicates whether this job should be resumed (`true`) or terminated (`false`) after a high priority interruption.
		bool isInterruptable = false;
		/// The list of requested Transmit Power Mode IDs required during the Job.
		/// @note If an invalid or unavailable Tx Power Mode ID is requested, the Job Request will be rejected.
		std::set<TxPowerModeID> txPowerModeIDs = {0};
		/// @brief The callback function to be triggered when the MFA is ready for a new Job Interval Batch (JIB).
		/// A JIB is a subset of a Job Interval Vector (JIV).
		/// @note This field is intended for use only with the conditionally required functionality: RequiredIfMFADrivenControls.
		/// For implementations that do not support this functionality, this field should be ignored and retain the default value of `nullptr`.
		NextJIBFunction sendNextJIBatchCallback{nullptr};
		/// The number of times the sendNextJIBatchCallback function will be called for the duration of the Job.
		/// It may be used in conjunction with the `lastJIBExecutedIdx` parameter of the function to track remaining JIBs.
		/// @note This field is intended for use only with the conditionally required functionality: RequiredIfMFADrivenControls.
		/// The value must be at least 1 to use this functionality (i.e. at least one JIB callback must be made).
		/// For implementations that do not support this functionality, this field should be ignored and retain the default value of 0.
		std::uint32_t numJIBs{0};
		/// Skill-Defined Context information associated with the Job.
		/// @note This field is set in conjuction with sendNextJIBatchCallback and is otherwise Read-Only.
		/// @note This field is intended for use only with the conditionally required functionality: RequiredIfMFADrivenControls.
		/// For implementations that do not support this functionality, this field should be ignored and retain the default value of `nullptr`.
		std::any callbackContext{nullptr};
		/// The Callback Function to be triggered if the Job Request is rejected between sendNextJIBatchCallbacks.
		/// @note This field is intended for use only with the conditionally required functionality: RequiredIfMFADrivenControls.
		/// For implementations that do not support this functionality, this field should be ignored and retain the default value of `nullptr`.
		std::function<void(std::any, RejectedReason)> requestRejectedCallback{nullptr};
		/// The controls-generation time for the next JIB, which may optionally be provided by a service to improve scheduling efficiency.
		ams::util::math::Femtoseconds lookAheadTime{0};
	};
} // namespace ams::iface::rfmel
