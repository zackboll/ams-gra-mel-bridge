#pragma once

#include <unordered_map>
#include <map>
#include <memory>
#include <set>

#include <rfmel/rfmeltypes/RFMELTypes.h>
#include <rfmel/jobs/DataPipe.h>
#include <map>
#include <memory>
#include <set>

namespace ams::iface::rfmel
{
	/// @brief ElementGroupConnections class
	/// @RequiredIfEndpointAssociation This class provides data definition in support of data endpoints and must be included as-is in all RF MEL
	/// implementations the use endpoint association
	class ElementGroupConnections
	{
	public:
		ElementGroupConnections() = default;
		~ElementGroupConnections() = default;
		ElementGroupConnections(const ElementGroupConnections&) = default;
		ElementGroupConnections(ElementGroupConnections&&) = default;
		ElementGroupConnections& operator=(const ElementGroupConnections&) = default;
		ElementGroupConnections& operator=(ElementGroupConnections&&) = default;

		/// @brief Accesses the element in the map corresponding to the given key
		/// Makes the element if it does not exist
		std::map<DataPipeLabel, std::shared_ptr<DataPipe>>& operator[](const ElementGroupLabel& key)
		{
			return connections[key];
		}

		/// @brief Returns an iterator to the first set of data paths in the map
		[[nodiscard]] typename std::unordered_map<ElementGroupLabel, std::map<DataPipeLabel, std::shared_ptr<DataPipe>>>::iterator begin() noexcept
		{
			return connections.begin();
		}

		/// @brief Returns a const iterator to the first set of data paths in the map
		[[nodiscard]] typename std::unordered_map<ElementGroupLabel, std::map<DataPipeLabel, std::shared_ptr<DataPipe>>>::const_iterator begin()
			const noexcept
		{
			return connections.begin();
		}

		/// @brief Returns an iterator to the after the last set of data paths in the map
		[[nodiscard]] typename std::unordered_map<ElementGroupLabel, std::map<DataPipeLabel, std::shared_ptr<DataPipe>>>::iterator end() noexcept
		{
			return connections.end();
		}

		/// @brief Returns a const iterator to the after the last set of data paths in the map
		[[nodiscard]] typename std::unordered_map<ElementGroupLabel, std::map<DataPipeLabel, std::shared_ptr<DataPipe>>>::const_iterator end()
			const noexcept
		{
			return connections.end();
		}

		/// @brief Returns the number connections in the list of data paths
		[[nodiscard]] typename std::unordered_map<ElementGroupLabel, std::map<DataPipeLabel, std::shared_ptr<DataPipe>>>::size_type size()
			const noexcept
		{
			return connections.size();
		}

		/// @brief Clears all connections from the map
		void clear()
		{
			connections.clear();
		}

		/// @brief Creates a new Endpoint with a given ElementGroupLabel or reassigns an existing
		/// Startpoint to have a new endpoint
		void insert_or_assign(const ElementGroupLabel& key, const std::map<DataPipeLabel, std::shared_ptr<DataPipe>>& value)
		{
			connections[key] = value;
		}

	private:
		std::unordered_map<ElementGroupLabel, std::map<DataPipeLabel, std::shared_ptr<DataPipe>>> connections;
	};
} // namespace ams::iface::rfmel
