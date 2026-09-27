#pragma once

#include <math/units/UTCTime.h>

#include <rfmel/jobs/JobEvent.h>
#include <rfmel/rfmeltypes/RFMELTypes.h>
#include <rfmel/jobs/Pointing.h>

#include <any>
#include <cstdint>
#include <optional>
#include <vector>

namespace ams::iface::rfmel
{
	/// @class ProductRxMetadata
	/// @brief Concrete class that describes the information received when a ProductRxEndpoint callback is triggered to return data.
	/// The metadata should contain all necessary information for all skills to correctly process the data products received by the endpoint.
	/// @RequiredIfReceive This class must be used by the implementer in all RF MEL implementations
	/// of the ProductRxEndpoint data ready callback if the associated MFA supports receive.
	class ProductRxMetadata
	{
	public:
		ProductRxMetadata() = default;
		~ProductRxMetadata() = default;
		ProductRxMetadata(const ProductRxMetadata&) = default;
		ProductRxMetadata(ProductRxMetadata&&) = default;
		ProductRxMetadata& operator=(const ProductRxMetadata&) = default;
		ProductRxMetadata& operator=(ProductRxMetadata&&) = default;

		/// @brief Returns the API version ID of the RF MEL implementation that constructed this object.
		/// This version ID should match the one in ams::iface::mel::VersionInfo.getAPIVersion()
		[[nodiscard]] auto getMelProtocolVersionID() const
		{
			return this->melProtocolVersionID;
		}

		/// @brief Sets the API version ID of the RF MEL implementation
		void setMelProtocolVersionID(uint32_t newValue)
		{
			this->melProtocolVersionID = newValue;
		}

		/// @brief Returns the Virtual Aperture Definition ID
		[[nodiscard]] VirtualApertureDefinitionID getVaDefinitionID() const
		{
			return this->vaDefinitionID;
		}

		/// @brief Sets the VA Definition ID
		void setVaDefinitionID(VirtualApertureDefinitionID newValue)
		{
			this->vaDefinitionID = newValue;
		}

		/// @brief Returns the VA Instance ID
		[[nodiscard]] VirtualApertureInstanceID getVaInstanceID() const
		{
			return this->vaInstanceID;
		}

		/// @brief Sets the VA Instance ID
		void setVaInstanceID(VirtualApertureInstanceID newValue)
		{
			this->vaInstanceID = newValue;
		}

		/// @brief Returns the JobDetails ID
		[[nodiscard]] auto getJobDetailsID() const
		{
			return this->jobDetailsID;
		}

		/// @brief Sets the Job Details ID
		void setJobDetailsID(uint32_t newValue)
		{
			this->jobDetailsID = newValue;
		}

		/// @brief Returns the Job Interval ID
		[[nodiscard]] auto getJobIntervalID() const
		{
			return this->jobIntervalID;
		}

		/// @brief Sets the Job Interval ID
		void setJobIntervalID(uint32_t newValue)
		{
			this->jobIntervalID = newValue;
		}

		/// @brief Returns the Local Function Type ID
		[[nodiscard]] LocalFunctionTypeID getLfTypeID() const
		{
			return this->lfTypeID;
		}

		/// @brief Sets the LF Type ID
		void setLfTypeID(LocalFunctionTypeID newValue)
		{
			this->lfTypeID = newValue;
		}

		/// @brief Returns the LF Instance ID
		[[nodiscard]] auto getLfInstanceID() const
		{
			return this->lfInstanceID;
		}

		/// @brief Sets the LF Instance ID
		void setLfInstanceID(uint32_t newValue)
		{
			this->lfInstanceID = newValue;
		}

		/// @brief Returns the User Defined Data
		[[nodiscard]] auto getUserDefinedData() const
		{
			return this->userDefinedData;
		}

		/// @brief Sets the User Defined Data
		void setUserDefinedData(std::any newValue)
		{
			this->userDefinedData = newValue;
		}

		/// @brief Returns if these data products are coherent in phase with the previous interval (from the same JobDetail), if applicable.
		[[nodiscard]] auto getPhaseCoherenceWithPrior() const
		{
			return this->phaseCoherenceWithPrior;
		}

		/// @brief Sets the boolean for if the data products are coherent in phase with the previous interval
		void setPhaseCoherenceWithPrior(bool newValue)
		{
			this->phaseCoherenceWithPrior = newValue;
		}

		/// @brief Returns the absolute UTC time the first ReceiveEvent in the vector began
		[[nodiscard]] auto getFirstReceiveEventStart() const
		{
			return this->firstEventStart;
		}

		/// @brief Sets the absolute UTC time the first ReceiveEvent in the vector began
		void setFirstReceiveEventStart(ams::util::math::UTCTime newValue)
		{
			this->firstEventStart = newValue;
		}

		/// @brief Returns the vector of stabilization points (beam pointing angles) that were used by the interval
		[[nodiscard]] auto getStabPoints() const
		{
			return this->stabPoints;
		}

		/// @brief Sets the vector of stabilization points (beam pointing angles) that were used by the interval
		void setStabPoints(const std::vector<PointingType>& newValue)
		{
			this->stabPoints = newValue;
		}

		/// @brief Add a new element to the vector.
		void addStabPoints(PointingType data)
		{
			this->stabPoints.push_back(data);
		}

		/// @brief Returns the vector of all the ReceiveEvents that actually occurred as a result of the commanded interval.
		/// @note If the interval defined a sequence, and also said that it was repeated, then all of the events that ocurred will be
		/// included in this returned vector.
		/// Ex. If interval defined a sequence with 5 ReceiveEvents, and that is be repeated 3 times, then this vector
		/// should contain 15 individual receive events that are sequentially ordered in time (oldest is index 0).
		/// @note If the vector is empty, then something strange has happened.
		[[nodiscard]] auto getReceiveEvents() const
		{
			return this->rxEvents;
		}

		/// @brief Sets the vector of all ReceiveEvents that actually occurred as a result of the commanded interval.
		void setReceiveEvents(const std::vector<ams::iface::rfmel::ReceiveEvent>& newValue)
		{
			this->rxEvents = newValue;
		}

		/// @brief Add a new element to the vector.
		void addReceiveEvents(ams::iface::rfmel::ReceiveEvent data)
		{
			this->rxEvents.push_back(data);
		}

		/// @brief Returns a std::optional containing a vector to associate each VITA packet with a specific JobEventID it was generated from.
		/// @note This vector should be empty if VITA packets were not used as the data product on the endpoint. If VITA packets were used,
		/// then this vector should have the same size as there are packets returned in the data ready callback of the endpoint.
		/// @note Each JobEventID in the vector will correspond exactly to the same index packet in the buffer returned by the data ready callback.
		/// @note All JobEventIDs should correspond to a ReceiveEventID. TransmitEvents are not included in data ready callback buffer.
		[[nodiscard]] auto getReceiveEventAssociations() const
		{
			return this->rxEventIDAssociations;
		}

		/// @brief Sets the Event ID Associations
		void setReceiveEventAssociations(const std::optional<std::vector<JobEventID>>& newValue)
		{
			this->rxEventIDAssociations = newValue;
		}

		/// @brief Indicates the VITA49.2 Stream IDs these products were produced from.
		/// The Virtual Aperture defines the number and meaning of each stream.
		/// @RequiredIfReceive This function passes Stream IDs in support of
		/// required RF MEL functionality specific to receive and must be
		/// provided by the implementer in all RF MEL implementations if the
		/// associated MFA supports receive.
		[[nodiscard]] auto getRxStreamIDs() const
		{
			return this->rxStreamIDs;
		}

		/// @brief Sets the vector of VITA49.2 Stream IDs
		void setReceiveEvents(const std::vector<ams::iface::rfmel::StreamID>& newValue)
		{
			this->rxStreamIDs = newValue;
		}

	private:
		// Remember, these are fields for what "already happened" for that interval,
		// not commanding the MFA what we want to happen (i.e. JobInterval).
		// In other words, this metadata should represent the absolute truth of the collection.

		// start of what is defined in the RF MEL IDD

		// PROTOCOL METADATA
		uint32_t melProtocolVersionID{0};

		// JOB INTERVAL INFO

		/// linkage to uniquely identify single VA
		VirtualApertureDefinitionID vaDefinitionID{0};

		/// linkage to uniquely identify single VAI
		VirtualApertureInstanceID vaInstanceID{0};

		/// detail that contained the interval
		uint32_t jobDetailsID{0};

		/// interval that generated all of this received data
		uint32_t jobIntervalID{0};

		/// used in conjunction with LF Instance ID to link LF in a VAI
		LocalFunctionTypeID lfTypeID{0};

		/// used in conjunction with LF Type ID to link LF in a VAI
		uint32_t lfInstanceID{0};

		std::any userDefinedData;

		/// did this interval maintain phase coherence with previous interval?
		bool phaseCoherenceWithPrior = false;

		/// the absolute UTC time that the first sample in the returned data buffer occurred at
		ams::util::math::UTCTime firstEventStart;

		/// stab point vector for holding directions for all the events
		/// (cheaper than holding a pointingType per event, in case some events
		/// share the same direction)
		std::vector<PointingType> stabPoints;

		/// the ACTUAL RxEvents that occurred during this interval's collection, including
		/// ALL repeated sequences, etc. If I/Q samples returned, then the sampling rate and times should
		/// for each of these events should add up to the total number of samples returned.
		std::vector<ams::iface::rfmel::ReceiveEvent> rxEvents;

		/// @brief This field exists because in the event we are receiving VITA data packets on this endpoint,
		/// a single ReceiveEvent (dwell) may be long enough to generate more than one VITA data packet. If
		/// that is the case, we need this vector to associate each packet with a specific event ID it was generated from.
		/// The vector will contain a list of JobEventIDs, with each ID element corresponding the VITA packet in the
		/// data buffer at that same element index. Ex. If the first ReceiveEvent had an ID of 1, and from it generated
		/// the first 5 VITA packets in the buffer, then the first 5 entries in this vector would be <1,1,1,1,1, ... >
		/// @note This field is only relevant if this endpoint is receiving ams::vita signal data packets.
		/// Otherwise, this vector should be empty, and optional should have no value.
		/// @note This vector's size must ALWAYS equal the number of elements in the returned data buffer.
		std::optional<std::vector<JobEventID>> rxEventIDAssociations;

		/// @brief Indicates the VITA49.2 Stream IDs these products were produced from.
		/// The Virtual Aperture defines the number and meaning of each stream.
		/// @RequiredIfReceive This function passes Stream IDs in support of
		/// required RF MEL functionality specific to receive and must be
		/// provided by the implementer in all RF MEL implementations if the
		/// associated MFA supports receive.
		std::vector<ams::iface::rfmel::StreamID> rxStreamIDs;
	};
} // namespace ams::iface::rfmel
