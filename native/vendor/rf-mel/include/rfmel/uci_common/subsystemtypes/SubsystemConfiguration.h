#pragma once

#include <mel/library/UCI_ID.h>
#include <rfmel/uci_common/Configuration.h>
namespace ams::iface::rfmel
{
	/// @class SubsystemConfiguration
	/// @brief This type is aligned with the OMS/UCI SubsystemConfiguration type
	/// @Required This class provides required RF MEL functionality by defining
	/// hardware information and capabilities that are being used in an MFA, and
	/// must be provided by the implementer in all RF MEL implementations. See
	/// individual members for details.
	class SubsystemConfiguration
	{
	public:
		SubsystemConfiguration() = default;
		SubsystemConfiguration(const SubsystemConfiguration&) = default;
		SubsystemConfiguration(SubsystemConfiguration&&) = default;
		SubsystemConfiguration& operator=(const SubsystemConfiguration&) = default;
		SubsystemConfiguration& operator=(SubsystemConfiguration&&) = default;
		virtual ~SubsystemConfiguration() = default;

		/// @Required This function supports hardware information and capabilities that are being used in an
		/// MFA and must be provided by the implementer for all RF MEL implementations.
		[[nodiscard]] virtual const ams::iface::mel::UCI_ID& getSubsystemID() const
		{
			return id;
		}
		void setSubsystemID(const ams::iface::mel::UCI_ID& id_in)
		{
			this->id = id_in;
		}

		/// @Required This function supports hardware information and capabilities that are being used in an
		/// MFA and must be provided by the implementer for all RF MEL implementations.
		[[nodiscard]] virtual const Configuration& getConfiguration() const
		{
			return configuration;
		}
		void setConfiguration(const Configuration& newValue)
		{
			this->configuration = newValue;
		}

	private:
		/// @brief Contains MFA SubsystemID information
		ams::iface::mel::UCI_ID id{};
		/// @brief Contains MFA Configuration information
		Configuration configuration{};
	};
} // namespace ams::iface::rfmel
