#pragma once

#include <rfmel/rfmeltypes/RFMELTypes.h>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>

namespace ams::iface::rfmel
{
	/// @class DataPipeConnections
	/// @brief A data structure for storing associated endpoints, indexed by Data Pipe Labels.
	/// @RequiredIfEndpointAssociation This class provides data definition in support of endpoint association and must be included as-is in all
	/// RF MEL implementations that support this feature.
	class DataPipeConnections
	{
	public:
		DataPipeConnections() = default;
		~DataPipeConnections() = default;
		DataPipeConnections(const DataPipeConnections&) = default;
		DataPipeConnections(DataPipeConnections&&) = default;
		DataPipeConnections& operator=(const DataPipeConnections&) = default;
		DataPipeConnections& operator=(DataPipeConnections&&) = default;

		/// @brief Accesses the element in the map corresponding to the given key
		/// Makes the element if it does not exist
		std::set<EndpointID>& operator[](const DataPipeLabel& key)
		{
			return connections[key];
		}

		/// @brief Returns an iterator to the first set of data paths in the map
		[[nodiscard]] typename std::unordered_map<DataPipeLabel, std::set<EndpointID>>::iterator begin() noexcept
		{
			return connections.begin();
		}

		/// @brief Returns a const iterator to the first set of data paths in the map
		[[nodiscard]] typename std::unordered_map<DataPipeLabel, std::set<EndpointID>>::const_iterator begin() const noexcept
		{
			return connections.begin();
		}

		/// @brief Returns an iterator to the after the last set of data paths in the map
		[[nodiscard]] typename std::unordered_map<DataPipeLabel, std::set<EndpointID>>::iterator end() noexcept
		{
			return connections.end();
		}

		/// @brief Returns a const iterator to the after the last set of data paths in the map
		[[nodiscard]] typename std::unordered_map<DataPipeLabel, std::set<EndpointID>>::const_iterator end() const noexcept
		{
			return connections.end();
		}

		/// @brief Returns the number connections in the list of data paths
		[[nodiscard]] typename std::unordered_map<DataPipeLabel, std::set<EndpointID>>::size_type size() const noexcept
		{
			return connections.size();
		}

		/// @brief Clears all connections from the map
		void clear()
		{
			connections.clear();
		}

		/// @brief Creates a new Endpoint with a given DataPipeLabel or reassigns an existing
		/// DataPipeLabel to have a new endpoint
		void insert_or_assign(const DataPipeLabel& key, const std::set<EndpointID>& value)
		{
			connections[key] = value;
		}

	private:
		std::unordered_map<DataPipeLabel, std::set<EndpointID>> connections;
	};
} // namespace ams::iface::rfmel
