#pragma once

#include <rfmel/rfmeltypes/RFMELTypes.h>
#include <set>

namespace ams::iface::rfmel
{
	/// @brief Represents a DataStream from the MFA
	/// @RequiredIfEndpointAssociation Implementation required for all RF MEL implementations that use endpoint association. See individual class
	/// members for additional details
	class DataPipe
	{
	public:
		DataPipe() = default;
		virtual ~DataPipe() = default;
		DataPipe(const DataPipe&) = default;
		DataPipe(DataPipe&&) = default;
		DataPipe& operator=(const DataPipe&) = default;
		DataPipe& operator=(DataPipe&&) = default;

		static constexpr auto DEFAULT = "default";
		/// @brief Overloaded Compare Operator
		/// @RequiredIfEndpointAssociation Implementation required for all RF MEL implementations that use endpoint association
		[[nodiscard]] virtual bool operator==(const DataPipe& label) const = 0;
		/// @brief Overloaded Compare Operator
		/// @RequiredIfEndpointAssociation Implementation required for all RF MEL implementations that use endpoint association
		[[nodiscard]] virtual bool operator==(const DataPipe&& label) const = 0;
		/// @brief connect the EndpointID to this DataPipe.  It is expected that this will setup Q-pairs for RDMA.
		/// @RequiredIfEndpointAssociation Implementation required for all RF MEL implementations that use endpoint association
		[[nodiscard]] virtual bool associateEndpoint(const EndpointID) = 0;
		/// @brief  connect the EndpointIDs in the set to this DataPipe.  It is expected that this will setup Q-pairs for RDMA.
		/// @RequiredIfEndpointAssociation Implementation required for all RF MEL implementations that use endpoint association
		[[nodiscard]] virtual bool associateEndpoints(const std::set<EndpointID>&) = 0;
		/// @brief connect the EndpointIDs in the set to this DataPipe.  It is expected that this will setup Q-pairs for RDMA.
		/// @RequiredIfEndpointAssociation Implementation required for all RF MEL implementations that use endpoint association
		[[nodiscard]] virtual std::set<EndpointID> getAssociatedEndpoints() const = 0;
		/// @brief Returns the label for the current DataPipe.
		/// @RequiredIfEndpointAssociation Implementation required for all RF MEL implementations that use endpoint association
		[[nodiscard]] virtual const DataPipeLabel getLabel() const = 0;
	};
} // namespace ams::iface::rfmel
