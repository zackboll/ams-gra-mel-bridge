#pragma once

#include <rfmel/c2/ElementGroupCommand.h>
#include <memory>
#include <vector>

namespace ams::iface::rfmel
{
	/// @brief This class acts as a vector of ElementGroupCommands.
	/// Interface has basic compliance with a std container. Allows for
	/// range-based for loops over class, index access, and element construction.
	/// @Required This class provides data definition in support of data endpoints and must be included as-is in all RF MEL implementations
	class ElementGroupCommandList
	{
	public:
		ElementGroupCommandList() = default;
		~ElementGroupCommandList() = default;
		ElementGroupCommandList(const ElementGroupCommandList&) = default;
		ElementGroupCommandList(ElementGroupCommandList&&) = default;
		ElementGroupCommandList& operator=(const ElementGroupCommandList&) = default;
		ElementGroupCommandList& operator=(ElementGroupCommandList&&) = default;

		/// @brief Accesses the element in the list at the given index
		std::shared_ptr<ElementGroupCommand>& operator[](size_t idx)
		{
			return list[idx];
		}

		/// @brief Accesses the element in the list at the given index
		const std::shared_ptr<ElementGroupCommand>& operator[](size_t idx) const
		{
			return list[idx];
		}

		/// @brief Adds the command to the ElementGroupCommandList
		void push_back(const std::shared_ptr<ElementGroupCommand>& item)
		{
			list.push_back(item);
		}

		/// @brief Adds the command to the ElementGroupCommandList in place
		void emplace_back(const std::shared_ptr<ElementGroupCommand>& item)
		{
			list.emplace_back(item);
		}

		/// @brief Returns an iterator to the first command in the list
		[[nodiscard]] std::vector<std::shared_ptr<ElementGroupCommand>>::iterator begin() noexcept
		{
			return list.begin();
		}

		/// @brief Returns a const iterator to the first command in the list
		[[nodiscard]] std::vector<std::shared_ptr<ElementGroupCommand>>::const_iterator begin() const noexcept
		{
			return list.begin();
		}

		/// @brief Returns an iterator to after the last command in the list
		[[nodiscard]] std::vector<std::shared_ptr<ElementGroupCommand>>::iterator end() noexcept
		{
			return list.end();
		}

		/// @brief Returns a const iterator to after the last command in the list
		[[nodiscard]] std::vector<std::shared_ptr<ElementGroupCommand>>::const_iterator end() const noexcept
		{
			return list.end();
		}

		/// @brief True if the list is empty, false otherwise
		[[nodiscard]] bool empty() const noexcept
		{
			return list.empty();
		}

		/// @brief Returns the number of commands in the list
		[[nodiscard]] std::vector<std::shared_ptr<ElementGroupCommand>>::size_type size() const noexcept
		{
			return list.size();
		}

	private:
		std::vector<std::shared_ptr<ElementGroupCommand>> list;
	};
} // namespace ams::iface::rfmel
