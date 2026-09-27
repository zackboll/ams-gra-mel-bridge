#pragma once

#include <rfmel/rfmeltypes/RFMELTypes.h>
#include <exception>
#include <iostream>

namespace ams::iface::rfmel
{
	/// @brief Defines the Direct Access Virtual Aperture used after expiration exception.
	/// @RequiredIfDEA This class provides data definition in support of required RF MEL functionality
	/// for expiration exception and must be included as-is in all RF MEL implementations if the MFA supports Direct Endpoint Access.
	class ResourceUseAfterExpiredException : public std::exception
	{
	public:
		/// @brief Returns a null terminated character sequence (of type char *) with
		/// a description of the exception.
		[[nodiscard]] const char *what() const noexcept override
		{
			return "Direct Access Virtual Aperture used after expiration.";
		}
	};
} // namespace ams::iface::rfmel
