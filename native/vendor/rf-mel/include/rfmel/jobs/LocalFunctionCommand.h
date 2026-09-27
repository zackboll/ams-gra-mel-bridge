#pragma once

#include <rfmel/rfmeltypes/RFMELTypes.h>

#include <vector>
#include <cstdint>

namespace ams::iface::rfmel
{
	/// @brief An offset into the address space of an LF Instance.
	/// @RequiredIfLFSupport This class provides data definition in support of required RF MEL functionality
	/// to handle local functions address space and must be included as-is in all RF MEL implementations if the associated
	/// MFA supports Local Functions.
	using LocalFunctionAddress = uint64_t;

	/// @brief A scalar value that can be written or read within the address space of an LF Instance.
	/// @RequiredIfLFSupport This class provides data definition in support of required RF MEL functionality
	/// to handle local functions address space and must be included as-is in all RF MEL implementations if the associated
	/// MFA supports Local Functions.
	using LocalFunctionValue = uint64_t;

	/// @brief A combination of address and value, used to write to a scalar value within the address space of a LF Instance.
	/// @RequiredIfLFSupport This class provides data definition in support of required RF MEL functionality
	/// to handle local functions address space and must be included as-is in all RF MEL implementations if the associated
	/// MFA supports Local Functions.
	class LFAddressValue
	{
	public:
		LFAddressValue() = default;
		~LFAddressValue() = default;
		LFAddressValue(const LFAddressValue&) = default;
		LFAddressValue(LFAddressValue&&) = default;
		LFAddressValue& operator=(const LFAddressValue&) = default;
		LFAddressValue& operator=(LFAddressValue&&) = default;

		/// @brief Get local function address.
		[[nodiscard]] auto getLfAddress() const
		{
			return this->address;
		}
		/// @brief Get local function value.
		[[nodiscard]] auto getLfValue() const
		{
			return this->value;
		}
		/// @brief Set local function address.
		void setLfAddress(LocalFunctionAddress lfAddress)
		{
			this->address = lfAddress;
		}
		/// @brief Set local function value.
		void setLfValue(LocalFunctionValue lfValue)
		{
			this->value = lfValue;
		}

	private:
		LocalFunctionAddress address{0};
		LocalFunctionValue value{0};
	};

	/// @brief Represents a series of write commands which target a
	/// single LF Instance, to be written as a set at a given time.
	/// @RequiredIfLFSupport This class provides data definition in support of required RF MEL functionality
	/// to command local functions and must be included as-is in all RF MEL implementations if the associated
	/// MFA supports Local Functions.
	class LocalFunctionCommand
	{
	public:
		/// @brief Get local function type.
		[[nodiscard]] auto getLfType() const
		{
			return this->type;
		}
		/// @brief Get local function instance.
		[[nodiscard]] auto getLfInstance() const
		{
			return this->instance;
		}
		/// @brief Get local function address value.
		[[nodiscard]] const auto& getLfAddrValues() const
		{
			return this->addressValue;
		}
		/// @brief Get local function address value.
		[[nodiscard]] auto& getLfAddrValues()
		{
			return addressValue;
		}
		/// @brief Set local function type.
		void setLfType(LocalFunctionTypeID lfType)
		{
			this->type = lfType;
		}
		/// @brief Set local function instance.
		void setLfInstance(size_t lfInstance)
		{
			this->instance = lfInstance;
		}
		/// @brief Set local function address value.
		void setLfAddrValues(const std::vector<LFAddressValue>& lfAddrValues)
		{
			this->addressValue = lfAddrValues;
		}

		LocalFunctionCommand() = default;

	private:
		LocalFunctionTypeID type = 0;
		size_t instance = 0;
		std::vector<LFAddressValue> addressValue;
	};
} // namespace ams::iface::rfmel
