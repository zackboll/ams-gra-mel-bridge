#pragma once

#include <set>
#include <utility> // pair

#include <math/units/UTCTime.h>

#include <rfmel/mfa/TxPowerModeData.h>
#include <rfmel/rfmeltypes/RFMELTypes.h>
#include <vector>

namespace ams::iface::rfmel
{
	/// @brief Provides a run-time interface by which to query for key characteristics of the
	/// connected MFA, such as supported RF bands, supported sample rates, and information
	/// about each MFA Faces.
	/// @Required This class provides required RF MEL functionality for MFA data requests,
	/// and must be provided by the implementer in all RF MEL implementations. See
	/// individual members for details.
	class RFMFAInfo
	{
	public:
		virtual ~RFMFAInfo() = default;
		RFMFAInfo(const RFMFAInfo&) = delete;
		RFMFAInfo(RFMFAInfo&&) = delete;
		RFMFAInfo& operator=(const RFMFAInfo&) = delete;
		RFMFAInfo& operator=(RFMFAInfo&&) = delete;

		/// @brief  Indicates how much time is required to process Automatic Gain Control (AGC) after data collection.
		///	This value is used to set the numIterationIgnoredPostAGC on a receive event after the AGC data collection period is complete.
		/// @RequiredIfAGC This function passes temporal and gain data and must be provided by the implementer
		/// for all RF MEL implementations if the associated MFA supports Automatic Gain Control via the job interface.
		[[nodiscard]] virtual ams::util::math::Femtoseconds getAGCProcessingTime(ams::iface::rfmel::FaceID) const = 0;

		/// @brief Returns the [min,max] frequency supported by this aperture (Rx).
		/// @Required This function supports passing frequency data and must be provided by
		/// the implementer for all RF MEL implementations.
		[[nodiscard]] virtual std::vector<ams::iface::rfmel::FrequencyRange> getRxFrequencyRanges(ams::iface::rfmel::FaceID) const = 0;
		/// @brief Returns the [min,max] frequency supported by this aperture (Tx).
		/// @Required This function supports passing frequency data and must be provided by
		/// the implementer for all RF MEL implementations.
		[[nodiscard]] virtual std::vector<ams::iface::rfmel::FrequencyRange> getTxFrequencyRanges(ams::iface::rfmel::FaceID) const = 0;

		/// @brief Returns if Endpoint is required for this Face.
		/// @Required This function indicates whether an MFA requires association of its endpoints and
		/// data pipes. This function must be provided by the implementer for all RF MEL implementations.
		[[nodiscard]] virtual bool requiresEndpointAssociation(ams::iface::rfmel::FaceID) const = 0;

		/// @brief Returns true if open additions are included in this implementation.
		/// @Required Implementation required for all RF MEL implementations
		[[nodiscard]] virtual bool containsOpenAdditions() const = 0;

		/// @brief Returns TRUE if this aperture supports transmit operation.
		/// @Required This function supports passing operational capability information and must be provided by
		/// the implementer for all RF MEL implementations.
		[[nodiscard]] virtual bool supportsTransmit(ams::iface::rfmel::FaceID) const = 0;

		/// @brief Returns TRUE if this aperture supports receive operation.
		/// @Required This function supports passing operational capability information and must be provided by
		/// the implementer for all RF MEL implementations.
		[[nodiscard]] virtual bool supportsReceive(ams::iface::rfmel::FaceID) const = 0;

		/// @brief Returns number of faces supported by this aperture.
		/// @Required This function supports passing operational capability information and must be provided by
		/// the implementer for all RF MEL implementations.
		[[nodiscard]] virtual size_t getNumFaces() const = 0;

		/// @brief Time checking functionality
		/// This is the finest quanta of time supported by the scheduler when constructing
		/// a Job timeline; therefore, all Job timing durations should be a multiple of
		/// this resolution of time. Use the quantizeDuration() function to truncate
		/// all Job timing durations to the closest quantized value to ensure the MFA
		/// is able to execute the Job timeline as specified.
		/// @Required This function supports passing temporal information and must be provided by the implementer
		/// for all RF MEL implementations.
		[[nodiscard]] virtual ams::util::math::Femtoseconds schedulerResolution() const = 0;

		/// @brief Returns the specified duration of time quantized to the finest quanta of time
		/// supported by the MFA. This function should be used for all Job timing durations.
		/// @Required This function supports passing temporal information and must be provided by the implementer
		/// for all RF MEL implementations.
		[[nodiscard]] virtual ams::util::math::Femtoseconds quantizeDuration(ams::util::math::Femtoseconds unquantizedDuration) const = 0;

		/// @brief Indicates how long ahead of time a JobRequest must arrive at the MFA
		/// so that the scheduler has time to adjudicate the request.
		/// @Required This function supports passing temporal information and must be provided by the implementer
		/// for all RF MEL implementations.
		[[nodiscard]] virtual ams::util::math::Femtoseconds minJobRequestLeadTime(ams::iface::rfmel::FaceID) const = 0;

		/// @brief Indicates how long ahead of the job start time that the MFA is willing
		/// to accept a JobRequest.
		/// Combined with the minJobRequestLeadTime, describes the window of opportunity
		/// during which a JobRequest must be received by the MFA to be considered for scheduling.
		/// @Required This function supports passing temporal information and must be provided by the implementer
		/// for all RF MEL implementations.
		[[nodiscard]] virtual ams::util::math::Femtoseconds maxJobRequestLeadTime(ams::iface::rfmel::FaceID) const = 0;

		/// @brief Relative to a job's actual start time, and start time of each successive
		/// interval, indicates how long ahead of time the Interval must be received by the MFA.
		/// @Required This function supports passing temporal information and must be provided by the implementer for
		/// all RF MEL implementations.
		[[nodiscard]] virtual ams::util::math::Femtoseconds minJobDetailLeadTime(ams::iface::rfmel::FaceID) const = 0;

		/// @brief Minimum gap duration between any transmit event and subsequent receive event.
		/// @Required This function supports passing temporal information and must be provided by the implementer
		/// for all RF MEL implementations.
		[[nodiscard]] virtual ams::util::math::Femtoseconds txRxSwitchingTime(ams::iface::rfmel::FaceID) const = 0;

		/// @brief Minimum gap duration between any receive event and subsequent transmit event.
		/// @Required This function supports passing temporal information and must be provided by the implementer
		/// for all RF MEL implementations.
		[[nodiscard]] virtual ams::util::math::Femtoseconds rxTxSwitchingTime(ams::iface::rfmel::FaceID) const = 0;

		/// @brief Minimum gap duration between any transmit event and subsequent transmit event.
		/// @Required This function supports passing temporal information and must be provided by the implementer
		/// for all RF MEL implementations.
		[[nodiscard]] virtual ams::util::math::Femtoseconds txTxSwitchingTime(ams::iface::rfmel::FaceID) const = 0;

		/// @brief Minimum gap duration between any receive event and subsequent receive event.
		/// @Required This function supports passing temporal information and must be provided by the implementer
		/// for all RF MEL implementations.
		[[nodiscard]] virtual ams::util::math::Femtoseconds rxRxSwitchingTime(ams::iface::rfmel::FaceID) const = 0;

		/// @brief Returns sample frequency range supported by this aperture.
		/// @Required This function supports passing temporal information and must be provided by the implementer for all RF MEL implementations.
		[[nodiscard]] virtual std::vector<ams::iface::rfmel::FrequencyRange> getSampleFrequencyRange(ams::iface::rfmel::FaceID) const = 0;

		/// @brief Returns the maximum number of bytes that may be specified
		/// in the JobInterval user-defined context data.
		/// @RequiredIfUserDefinedContextData This function passes JobInterval data and must be provided by the implimenter
		/// for all RF MEL implementations if the associated MFA supports user defined context data.
		[[nodiscard]] virtual size_t getMaxNumUserDefinedContextBytes() const = 0;

		/// @brief Returns a list of various data formats supported by the MFA implementation.
		/// @Required This function must be provided by the implementer for all RF MEL implementations.
		[[nodiscard]] virtual std::set<ams::iface::rfmel::JobDataFormat> getSupportedDataFormats() const = 0;

		/// @brief Array of Face data structure with dimensions and mounting orientation
		/// @Required This function supports passing Face data and must be provided by
		/// the implementer for all RF MEL implementations.
		[[nodiscard]] virtual const PhysicalData& getPhysicalData(ams::iface::rfmel::FaceID) const = 0;

		/// @brief Retrieve the list of Tx Power Mode Characteristics for each
		/// available Power Mode ID. Power Mode IDs are assigned by MFA provider.
		/// @RequiredIfTransmit This function passes Tx power data in support of required RF MEL functionality
		/// specific to transmit and must be provided by the implimenter for all RF MEL implementations if the associated
		/// MFA supports transmit.
		[[nodiscard]] virtual const std::vector<TxPowerModeData>& getTxPowerModeCharacteristics(ams::iface::rfmel::FaceID) const = 0;

		/// @brief Retrieve Tx Power Mode Characteristics for specified Power Mode ID.
		/// @RequiredIfTransmit This function passes Tx power data in support of required RF MEL functionality
		/// specific to transmit and must be provided by the implimenter for all RF MEL implementations if the associated
		/// MFA supports transmit.
		[[nodiscard]] virtual const TxPowerModeData& getTxPowerModeCharacteristics(TxPowerModeID txPowerModeID, ams::iface::rfmel::FaceID) const = 0;

		/// @brief Returns a list of MFA Faces.
		/// @Required This function passes Face data and must be provided by
		/// the implementer for all RF MEL implementations.
		[[nodiscard]] virtual std::set<ams::iface::rfmel::FaceID> getFaceIDs() const = 0;

	protected:
		RFMFAInfo() = default;
	};
} // namespace ams::iface::rfmel
