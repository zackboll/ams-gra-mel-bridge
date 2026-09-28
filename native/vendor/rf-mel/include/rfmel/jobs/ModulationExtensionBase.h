#pragma once

#include <utility>

namespace ams::iface::rfmel
{
	/// @brief base class of all Modulation class specializations. By declaring a virtual destructor, we guarantee a vtable exists,
	/// allowing a MEL implementation to determine the underlying derived type by attempting a series of dynamic_cast operations.
	/// @RequiredIfTransmit This class provides required RF MEL functionality for transmit specifically, and must be provided by
	/// the implementer in all RF MEL implementations if the associated MFA supports transmit.
	class ModulationExtensionBase
	{
	public:
		ModulationExtensionBase() = default;
		virtual ~ModulationExtensionBase() = default;

		ModulationExtensionBase(ModulationExtensionBase&& other) noexcept : ModulationExtensionBase()
		{
			*this = std::move(other);
		}
		ModulationExtensionBase& operator=(ModulationExtensionBase&& other) noexcept
		{
			if(this != &other)
			{
			}
			return *this;
		}
		ModulationExtensionBase(const ModulationExtensionBase&) = delete;
		ModulationExtensionBase& operator=(const ModulationExtensionBase&) = delete;
		ModulationExtensionBase& operator=(const ModulationExtensionBase&& other) noexcept = delete;
	};

} // namespace ams::iface::rfmel
