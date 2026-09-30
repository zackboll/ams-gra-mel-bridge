#pragma once

#include <rfmel/c2/ElementGroupDescriptor.h>
#include <rfmel/rfmeltypes/RFMELTypes.h>
#include <map>
#include <memory>
#include <unordered_map>

namespace ams::iface::rfmel
{
	/// @brief ElementGroupDescriptorLookupMap class
	/// @Required This class provides data definition in support of data endpoints and must be included as-is in all RF MEL implementations
	class ElementGroupDescriptorLookupMap
	{
	public:
		ElementGroupDescriptorLookupMap() = default;
		~ElementGroupDescriptorLookupMap() = default;
		ElementGroupDescriptorLookupMap(const ElementGroupDescriptorLookupMap&) = default;
		ElementGroupDescriptorLookupMap(ElementGroupDescriptorLookupMap&&) = default;
		ElementGroupDescriptorLookupMap& operator=(const ElementGroupDescriptorLookupMap&) = default;
		ElementGroupDescriptorLookupMap& operator=(ElementGroupDescriptorLookupMap&&) = default;

		/// @brief Accesses the element in the map corresponding to the given key
		/// Makes the element if it does not exist
		std::shared_ptr<ElementGroupDescriptor>& operator[](const ElementGroupLabel& key)
		{
			return connections[key];
		}

		/// @brief Returns an iterator to the first set of data paths in the map
		[[nodiscard]] typename std::unordered_map<ElementGroupLabel, std::shared_ptr<ElementGroupDescriptor>>::iterator begin() noexcept
		{
			return connections.begin();
		}

		/// @brief Returns a const iterator to the first set of data paths in the map
		[[nodiscard]] typename std::unordered_map<ElementGroupLabel, std::shared_ptr<ElementGroupDescriptor>>::const_iterator begin() const noexcept
		{
			return connections.begin();
		}

		/// @brief Returns an iterator to the after the last set of data paths in the map
		[[nodiscard]] typename std::unordered_map<ElementGroupLabel, std::shared_ptr<ElementGroupDescriptor>>::iterator end() noexcept
		{
			return connections.end();
		}

		/// @brief Returns a const iterator to the after the last set of data paths in the map
		[[nodiscard]] typename std::unordered_map<ElementGroupLabel, std::shared_ptr<ElementGroupDescriptor>>::const_iterator end() const noexcept
		{
			return connections.end();
		}

		/// @brief Returns the number connections in the list of data paths
		[[nodiscard]] typename std::unordered_map<ElementGroupLabel, std::shared_ptr<ElementGroupDescriptor>>::size_type size() const noexcept
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
		void insert_or_assign(const ElementGroupLabel& key, const std::shared_ptr<ElementGroupDescriptor>& value)
		{
			connections[key] = value;
		}

	private:
		std::unordered_map<ElementGroupLabel, std::shared_ptr<ElementGroupDescriptor>> connections;
	};
} // namespace ams::iface::rfmel
