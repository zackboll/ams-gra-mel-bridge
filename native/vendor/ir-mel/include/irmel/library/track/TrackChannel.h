//===============================================================================
/// @file  TrackChannel.h
/// @brief This file includes the Track Channel type.
//===============================================================================
#pragma once

#include <irmel/library/irmel-types/CandidateObjectMessage.h>
#include <irmel/library/irmel-types/CandidateObjectPreProcMessage.h>
#include <irmel/library/irmel-types/Channel.h>
#include <irmel/library/irmel-types/CommandStatus.h>
#include <irmel/library/irmel-types/CommonIR_MEL.h>
#include <irmel/library/irmel-types/RequestSystemTrackData.h>
#include <irmel/library/irmel-types/SystemTrackDataResponse.h>
#include <irmel/library/track/IRSTTrackReport.h>
#include <irmel/library/track/TrackDataUpdate.h>
#include <mel/library/CommonMEL.h>
#include <functional>

namespace ams::iface::irmel
{
	/// @class TrackChannel
	/// @brief Track Channel used by services.
	/// @RequiredIfTrack This class defines the conditionally required Track Channel interface and must be provided by the
	/// implementer in all IR MEL implementations that support this channel. See individual members for details.
	class TrackChannel : public virtual Channel
	{
	public:
		/// @Required This function supports channel and must be provided by the implementer for all IR MEL implementations
		~TrackChannel() override = default;

		/// @brief Returns CommandStatus after sending SystemTrackDataResponse
		/// @param[in] systemTrackDataResponse - Command to send
		/// @Optional This function allow for the specific command to be sent and must be provided by the implementer for all IR MEL implementations
		virtual ams::iface::mel::RequestFor<CommandStatus> send(SystemTrackDataResponse systemTrackDataResponse) = 0;

		/// @brief Returns CommandStatus after sending TrackDataUpdate
		/// @param[in] trackDataUpdate - Command to send
		/// @RequiredIfTrackUpdate This function allows the service to provide track updates to the MFA and is required for MFAs that allow for track
		/// updates.
		virtual ams::iface::mel::RequestFor<CommandStatus> send(TrackDataUpdate trackDataUpdate) = 0;

		/// @brief Callback Registration for CandidateObjectMessage
		/// @param callback std::function callback for CandidateObjectMessage
		/// @return Return::NotSupported if not implemented, Return::Success if implemented and registration succeeds, Return::Fail if a
		///         callback is already registered for this datatype on this channel
		/// @RequiredIfDetectCandidateObjects This function supports callback registration and must be provided by the implementer
		/// if the MFA provides Candidate Object detections
		virtual Return registerMetadataCallback(std::function<void(Channel& channel, const CandidateObjectMessage* const)> callback) = 0;

		/// @brief Callback Registration for IRSTTrackReport
		/// @param callback std::function callback for IRSTTrackReport
		/// @return Return::NotSupported if not implemented, Return::Success if implemented and registration succeeds, Return::Fail if a
		///         callback is already registered for this datatype on this channel
		/// @RequiredIfTrack This function supports callback registration and must be provided by the implementer for IR MFAs that support tracking
		virtual Return registerMetadataCallback(std::function<void(Channel& channel, const IRSTTrackReport* const)> callback) = 0;

		/// @brief Callback Registration for RequestSystemTrackData
		/// @param callback std::function callback for RequestSystemTrackData
		/// @return Return::NotSupported if not implemented, Return::Success if implemented and registration succeeds, Return::Fail if a
		///         callback is already registered for this datatype on this channel
		/// @Optional This function supports callback registration and is optional for IR MEL implementations
		/// IR MFAs that do not support track can still request data to support pointing Algorithms
		virtual Return registerMetadataCallback(std::function<void(Channel& channel, const RequestSystemTrackData* const)> callback) = 0;

		/// @brief Callback Registration for CandidateObjectPreProcMessage
		/// @param callback std::function callback for CandidateObjectPreProcMessage
		/// @return Return::NotSupported if not implemented, Return::Success if implemented and registration succeeds, Return::Fail if a
		///         callback is already registered for this datatype on this channel
		/// @Optional This function supports callback registration and IR MEL implementation is optional.
		/// Intended for use by IR MFAs that use CandidateObjectPreProc
		virtual Return registerMetadataCallback(std::function<void(Channel& channel, const CandidateObjectPreProcMessage* const)> callback) = 0;

		using Channel::registerMetadataCallback;
		using Channel::send;

		TrackChannel() = default;
		TrackChannel(const TrackChannel&) = delete;
		TrackChannel& operator=(TrackChannel&) = delete;
		TrackChannel(TrackChannel&& other) = delete;
		TrackChannel& operator=(TrackChannel&& other) = delete;
	};
	// end interface TrackChannel

} // end namespace ams::iface::irmel
