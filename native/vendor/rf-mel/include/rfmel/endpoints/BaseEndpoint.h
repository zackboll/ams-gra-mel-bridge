#pragma once

#include <rfmel/rfmeltypes/RFMELTypes.h>

namespace ams::iface::rfmel
{
	/// @brief Provides a base interface for dealing with Endpoint resources
	/// representing one end of a communications channel between a Service and an MFA.
	/// @Required This class provides required RF MEL functionality for
	/// providing a Service/MFA communications channel, and must be provided
	/// by the implementer in all RF MEL implementations. See individual
	/// members for details
	class BaseEndpoint
	{
	public:
		virtual ~BaseEndpoint() = default;
		BaseEndpoint(const BaseEndpoint&) = delete;
		BaseEndpoint(BaseEndpoint&&) = delete;
		BaseEndpoint& operator=(const BaseEndpoint&) = delete;
		BaseEndpoint& operator=(BaseEndpoint&&) = delete;

		/// @brief Returns the Endpoint Identifier to be used in JobRequests, etc.
		/// @Required This function supports passing Endpoint data and must be provided by the implementer for all RF MEL implementations.
		[[nodiscard]] virtual EndpointID getEndpointID() const = 0;

	protected:
		BaseEndpoint() = default;
	};
} // namespace ams::iface::rfmel
