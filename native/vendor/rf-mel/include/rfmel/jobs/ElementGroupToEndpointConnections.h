#pragma once

#include <rfmel/rfmeltypes/RFMELTypes.h>
#include <unordered_map>

namespace ams::iface::rfmel
{
	/// @brief ElementGroupToEndpointConnections class
	/// @RequiredIfEndpointAssociation Implementation required for all RF MEL implementations that use endpoint association. See individual class
	/// members for additional details
	class ElementGroupToEndpointConnections
	{
	public:
		ElementGroupToEndpointConnections() = default;
		~ElementGroupToEndpointConnections() = default;
		ElementGroupToEndpointConnections(const ElementGroupToEndpointConnections&) = default;
		ElementGroupToEndpointConnections(ElementGroupToEndpointConnections&&) = default;
		ElementGroupToEndpointConnections& operator=(const ElementGroupToEndpointConnections&) = default;
		ElementGroupToEndpointConnections& operator=(ElementGroupToEndpointConnections&&) = default;

		/// @brief Accesses the element in the map corresponding to the given key
		/// Makes the element if it does not exist
		std::unordered_map<DataPipeLabel, EndpointID>& operator[](const ElementGroupLabel& key)
		{
			return connections[key];
		}

		/// @brief Returns an iterator to the first set of data paths in the map
		[[nodiscard]] typename std::unordered_map<ElementGroupLabel, std::unordered_map<DataPipeLabel, EndpointID>>::iterator begin() noexcept
		{
			return connections.begin();
		}

		/// @brief Returns a const iterator to the first set of data paths in the map
		[[nodiscard]] typename std::unordered_map<ElementGroupLabel, std::unordered_map<DataPipeLabel, EndpointID>>::const_iterator begin()
			const noexcept
		{
			return connections.begin();
		}

		/// @brief Returns an iterator to the after the last set of data paths in the map
		[[nodiscard]] typename std::unordered_map<ElementGroupLabel, std::unordered_map<DataPipeLabel, EndpointID>>::iterator end() noexcept
		{
			return connections.end();
		}

		/// @brief Returns a const iterator to the after the last set of data paths in the map
		[[nodiscard]] typename std::unordered_map<ElementGroupLabel, std::unordered_map<DataPipeLabel, EndpointID>>::const_iterator end()
			const noexcept
		{
			return connections.end();
		}

		/// @brief Returns the number connections in the list of data paths
		[[nodiscard]] typename std::unordered_map<ElementGroupLabel, std::unordered_map<DataPipeLabel, EndpointID>>::size_type size() const noexcept
		{
			return connections.size();
		}

		/// @brief Clears all connections from the map
		void clear()
		{
			connections.clear();
		}

		/// @brief Creates a new Endpoint with a given ElementGroupLabel or reassigns an existing
		/// ElementGroupLabel to have a new endpoint
		void insert_or_assign(const ElementGroupLabel& key, const std::unordered_map<DataPipeLabel, EndpointID>& value)
		{
			connections[key] = value;
		}

	private:
		std::unordered_map<ElementGroupLabel, std::unordered_map<DataPipeLabel, EndpointID>> connections;
	};
} // namespace ams::iface::rfmel
