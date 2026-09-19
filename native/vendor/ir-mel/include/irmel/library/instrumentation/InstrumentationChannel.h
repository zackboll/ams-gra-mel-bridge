//===============================================================================
/// @file  InstrumentationChannel.h
/// @brief This file includes the Instrumentation Channel type.
//===============================================================================
#pragma once

#include <irmel/library/instrumentation/InstrumentationLevelCmd.h>
#include <irmel/library/instrumentation/InstrumentationReport.h>
#include <irmel/library/irmel-types/Channel.h>
#include <irmel/library/irmel-types/CommonIR_MEL.h>
#include <mel/library/CommonMEL.h>
#include <functional>

namespace ams::iface::irmel
{

	/// @class InstrumentationChannel
	/// @brief Instrumentation Channel used by services.
	/// @RequiredIfInstrumentation This class defines the conditionally required Instrumentation Channel, which must be
	/// provided by the implementer in IR MEL implementations which support instrumentation. See individual members for details
	class InstrumentationChannel : public virtual Channel
	{
	public:
		/// @RequiredIfInstrumentation A destructor for the conditionally required Instrumentation Channel must be
		/// provided by the implementer for all IR MEL implementations that support instrumentation
		~InstrumentationChannel() override = default;

		/// @brief Returns InstrumentationReport after sending InstrumentationLevelCmd
		/// @param[in] instrumentationLevelCmd - Command to send
		/// @RequiredIfInstrumentation This function allow for the specific command to be sent and must be provided by the implementer for all IR MEL
		/// implementations
		virtual ams::iface::mel::RequestFor<InstrumentationReport> send(InstrumentationLevelCmd instrumentationLevelCmd) = 0;

		/// @brief Callback Registration for InstrumentationReport
		/// @param callback std::function callback for InstrumentationReport
		/// @return Return::NotSupported if not implemented, Return::Success if implemented and registration succeeds, Return::Fail if a
		///    callback is already registered for this datatype on this channel
		/// @RequiredIfInstrumentation This function supports callback registration and must be provided by the
		/// implementer for IR MEL implementations which support instrumentation
		virtual Return registerMetadataCallback(std::function<void(Channel& channel, const InstrumentationReport* const)> callback) = 0;

		using Channel::registerMetadataCallback;
		using Channel::send;

		InstrumentationChannel() = default;
		InstrumentationChannel(const InstrumentationChannel&) = delete;
		InstrumentationChannel& operator=(InstrumentationChannel&) = delete;
		InstrumentationChannel(InstrumentationChannel&& other) = delete;
		InstrumentationChannel& operator=(InstrumentationChannel&& other) = delete;
	};
	// end interface InstrumentationChannel

} // end namespace ams::iface::irmel
