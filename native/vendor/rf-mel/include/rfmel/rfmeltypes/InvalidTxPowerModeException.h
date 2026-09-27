#pragma once

#include <rfmel/rfmeltypes/RFMELTypes.h>
#include <exception>
#include <iostream>

namespace ams::iface::rfmel
{
	/// @brief Defines the Invalid Transmit Power Mode requested exception.
	/// @RequiredIfTransmit This class provides data definition in support of required RF MEL functionality
	/// to Transmit Power requests and must be included as-is in all RF MEL implementations if the associated
	/// MFA supports transmit.
	class InvalidTxPowerModeException : public std::exception
	{
	public:
		/// @brief Returns a null terminated character sequence (of type char *) with
		/// a description of the exception.
		[[nodiscard]] const char *what() const noexcept override
		{
			return "Invalid Transmit Power Mode requested.";
		}
	};
} // namespace ams::iface::rfmel
