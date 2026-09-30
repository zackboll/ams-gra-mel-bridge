#pragma once

#include <rfmel/admin/UCI_Control.h>
#include <rfmel/rfmeltypes/RFMEL.h>
#include <rfmel/uci_common/subsystemtypes/SubsystemConfiguration.h>
#include <memory>
#include <string>

namespace ams::iface::rfmel
{
	/// @brief Provides access to the administrative functionality (i.e. configuration, UCI) of the MFA.
	/// @Required This class must be provided in all RF MEL implementations. See individual class members for details.
	class AdminMEL : public RFMEL
	{
	public:
		~AdminMEL() override = default;

		/// @brief Returns a non-const UCI interface to enable Command & Control.
		/// @exception rfmel::UnauthorizedUCICommandException The caller Service is not authorized
		/// @Required This function passes Command & Control data and must be provided by
		/// the implementer for all RF MEL implementations.
		[[nodiscard]] virtual std::shared_ptr<ams::iface::rfmel::UCI_Control> getUCIControl() = 0;

		/// @brief Allows Skills to request the MFA SubsystemConfiguration.
		/// Note that the current RF MFA Simulator implementation doesn't populate SubsystemID and About messages.
		/// @Required This function supports passing MFA Subsystem Configuration data and must be provided by the
		/// implementer for all RF MEL implementations.
		[[nodiscard]] virtual ams::iface::mel::RequestFor<SubsystemConfiguration> requestSubsystemConfiguration() = 0;

		/// @brief Creates an instance of the RF MEL.
		static std::shared_ptr<ams::iface::rfmel::AdminMEL> create(const std::string& melConfiguration);

	protected:
		AdminMEL() = default;
		AdminMEL(const AdminMEL&) = default;
		AdminMEL(AdminMEL&&) = default;
		AdminMEL& operator=(const AdminMEL&) = default;
		AdminMEL& operator=(AdminMEL&&) = default;
	};
} // namespace ams::iface::rfmel
