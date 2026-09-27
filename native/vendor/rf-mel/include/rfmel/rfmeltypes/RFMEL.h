#pragma once

#include <rfmel/rfmeltypes/InvalidTxPowerModeException.h>
#include <rfmel/rfmeltypes/RFMELTypes.h>
#include <rfmel/rfmeltypes/ResourceUseAfterExpiredException.h>

namespace ams::iface::rfmel
{
	/// @brief Enables communication with the MFA via the ABB.
	/// @Required This class provides basic RF MEL functionality, and must be provided by the implementer in all
	/// RF MEL implementations. See individual members for details.
	class RFMEL
	{
	public:
		virtual ~RFMEL() = default;

		/// @brief Returns the name of this MEL.
		/// @Required This function passes the MEL name and must be provided by the
		/// implementer for all RF MEL implementations.
		[[nodiscard]] virtual mel::VersionInfo getVersionInfo() const = 0;

		/// @brief Prepares an RFMEL to be shutdown.
		/// Additional requests (for Endpoints, VAs, Jobs, etc.) result in undefined behavior.
		/// @Required This function supports RFMEL shutdown and must be provided by the implementer for all RF MEL implementations.
		virtual void shutdown() = 0;

	protected:
		RFMEL() = default;
		RFMEL(const RFMEL&) = default;
		RFMEL(RFMEL&&) = default;
		RFMEL& operator=(const RFMEL&) = default;
		RFMEL& operator=(RFMEL&&) = default;
	};
} // namespace ams::iface::rfmel
