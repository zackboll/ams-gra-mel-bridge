#pragma once

#include <rfmel/rfmeltypes/RFMELTypes.h>
#include <cstdint>
#include <vector>

namespace ams::iface::rfmel
{
	/// @brief Defines the EnpointParamters type.
	/// @RequiredIfReceive This class provides data definition in support
	/// of required RF MEL functionality specific to receive and must be
	/// included as-is in all RF MEL implementations if the associated
	/// MFA supports receive.
	class EndpointParameters
	{
	public:
		/// @brief Get the Endpoint ID to which the job products will stream.
		[[nodiscard]] auto getEndpointID() const
		{
			return this->endpointID;
		}
		/// @brief Get the starting virtual address to which the products will be written.
		[[nodiscard]] auto getStartAddress() const
		{
			return this->startAddress;
		}
		/// @brief Get the maximum number of bytes which can be written, relative to the
		/// startAddress, without overflowing the allocated memory region.
		[[nodiscard]] auto getMaxBytes() const
		{
			return this->maxBytes;
		}
		/// @brief Set the Endpoint ID to which the job products will stream.
		void setEndpointID(EndpointID endpointID_in)
		{
			this->endpointID = endpointID_in;
		}
		/// @brief Set the starting virtual address to which the products will be written.
		void setStartAddress(uint64_t startAddress_in)
		{
			this->startAddress = startAddress_in;
		}
		/// @brief Set the maximum number of bytes which can be written, relative to the
		/// startAddress, without overflowing the allocated memory region.
		void setMaxBytes(uint64_t maxBytes_in)
		{
			this->maxBytes = maxBytes_in;
		}

	private:
		// The Endpoint ID to which the job products will stream.
		EndpointID endpointID = 0;

		// The starting virtual address to which the products will be written.
		uint64_t startAddress = 0;

		// The maximum number of bytes which can be written, relative to the
		// startAddress, without overflowing the allocated memory region.
		uint64_t maxBytes = 0;
	};

	/// @brief Defines the ProductStreamParams type.
	/// @Required This enumeration provides data definition associated with Job Requests
	/// and must be included as-is in all RF MEL implementations.
	class ProductStreamParams
	{
	public:
		/// @brief Get list of element groups for Virtual Apertures with multiple Rx/Tx
		/// element groups. May be left empty otherwise.
		[[nodiscard]] const auto& getApplicableRxElementGroups() const
		{
			return this->applicableRxElementGroups;
		}
		/// @brief Get vector of endpoints that specify the destination
		/// endpoint parameters for products output by this job.
		/// Given a JobRequest.ElementGroup.endpointIDs vector, as indicated
		/// by the 'applicableRxElementGroups', this vector of endpoints
		/// MUST MATCH the size of the endpointIDs vector, and each of the
		/// endpoints[i].endpointID MUST MATCH the corresponding endpointIDs[i] entry.
		[[nodiscard]] const auto& getEndpoints() const
		{
			return this->endpoints;
		}
		/// @brief Get vector of endpoints that specify the destination
		/// endpoint parameters for products output by this job.
		/// Given a JobRequest.ElementGroup.endpointIDs vector, as indicated
		/// by the 'applicableRxElementGroups', this vector of endpoints
		/// MUST MATCH the size of the endpointIDs vector, and each of the
		/// endpoints[i].endpointID MUST MATCH the corresponding endpointIDs[i] entry.
		[[nodiscard]] auto& getEndpoints()
		{
			return endpoints;
		}

		/// @brief Set list of element groups for Virtual Apertures with multiple Rx/Tx
		/// element groups. May be left empty otherwise.
		void setApplicableRxElementGroups(const std::vector<size_t>& applicable_rx_element_groups)
		{
			this->applicableRxElementGroups = applicable_rx_element_groups;
		}
		/// @brief Set vector of endpoints that specify the destination
		/// endpoint parameters for products output by this job.
		/// Given a JobRequest.ElementGroup.endpointIDs vector, as indicated
		/// by the 'applicableRxElementGroups', this vector of endpoints
		/// MUST MATCH the size of the endpointIDs vector, and each of the
		/// endpoints[i].endpointID MUST MATCH the corresponding endpointIDs[i] entry.
		void setEndpoints(const std::vector<EndpointParameters>& endpoints_in)
		{
			this->endpoints = endpoints_in;
		}

	private:
		/// List of element groups for Virtual Apertures with multiple Rx/Tx
		/// element groups. May be left empty otherwise.
		std::vector<size_t> applicableRxElementGroups;

		/// Specifies the destination endpoint parameters for products output
		/// by this job.
		/// Given a JobRequest.ElementGroup.endpointIDs vector, as indicated
		/// by the 'applicableRxElementGroups', this vector of endpoints
		/// MUST MATCH the size of the endpointIDs vector, and each of the
		/// endpoints[i].endpointID MUST MATCH the corresponding endpointIDs[i] entry.
		std::vector<EndpointParameters> endpoints;
	};
} // namespace ams::iface::rfmel
