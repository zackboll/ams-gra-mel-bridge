#pragma once

#include <rfmel/endpoints/BaseEndpoint.h>

namespace ams::iface::rfmel
{
	/// @brief Represents a communications channel to the MFA to an external endpoint.
	/// Class supports external endpoints for Rx, Tx, and Direct Access.
	/// @RequiredIfEndpointAssociation This class provides data definition in support of required RF MEL functionality
	/// to support external endpoints and must be included as-is in all RF MEL implementations which support endpoint association.
	class ExternalEndpoint : public BaseEndpoint
	{
	public:
		/// @brief When constructing the endpoint, you must specify the binary format of the
		/// data products you want to send to or receive from the MFA
		explicit ExternalEndpoint(JobDataFormat dataType);

		~ExternalEndpoint() override = default;
		ExternalEndpoint(const ExternalEndpoint&) = delete;
		ExternalEndpoint(ExternalEndpoint&&) = delete;
		ExternalEndpoint& operator=(const ExternalEndpoint&) = delete;
		ExternalEndpoint& operator=(ExternalEndpoint&&) = delete;

		/// @brief Gets the Endpoint ID.
		[[nodiscard]] EndpointID getEndpointID() const override = 0;

		/// @brief Gets the enum of this Endpoint's assigned data type (PDW, etc.)
		/// @Required This function is required to be implemented for all RF MEL implementations
		[[nodiscard]] virtual JobDataFormat getAssignedDataFormat() const = 0;

	protected:
		ExternalEndpoint() = default;
	};
} // namespace ams::iface::rfmel
