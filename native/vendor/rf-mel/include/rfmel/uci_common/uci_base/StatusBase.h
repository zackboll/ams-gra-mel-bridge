#pragma once

#include <mel/library/DiscreteStatus.h>
#include <mel/library/MFA_Component.h>
#include <mel/library/MFA_StatusDetailed.h>
#include <mel/library/securityauditrecord/MFA_SecurityAuditRecord.h>
#include <rfmel/uci_common/statustypes/AntennaStatus.h>
#include <rfmel/uci_common/subsystemtypes/SubsystemConfiguration.h>
#include <functional>

namespace ams::iface::rfmel
{

	/// @brief API to command the MFA into different Subsystem States and retrieve its status.
	/// @note there is no SubsystemID in this API; the OMS Adapter is responsible for providing
	/// all Service/Subsystem IDs.
	/// @Required This class provides required RF MEL functionality for commanding the MFA and
	/// the selected VA instance, and must be provided by the implementer in all RF MEL
	/// implementations. See individual members for details.
	class StatusBase
	{
	public:
		virtual ~StatusBase() = default;
		StatusBase(const StatusBase &) = delete;
		StatusBase(StatusBase &&) = delete;
		StatusBase &operator=(const StatusBase &) = delete;
		StatusBase &operator=(StatusBase &&) = delete;

		/// Observer Methods:

		/// @brief Indicates an MFA's current operating state.
		/// @Required This function passes operating state data and must be provided by the implementer
		/// for all RF MEL implementations.
		[[nodiscard]] virtual ams::iface::mel::MFA_Status getStatus() const = 0;

		/// @brief Registers a function to handle Status updates.
		/// @Required This function passes function regestration data and must be provided by the
		/// implementer for all RF MEL implementations.
		virtual void setCallback(std::function<void(const ams::iface::mel::MFA_Status &)> cb) = 0;

		/// @brief Current detailed status of the reporting MFA; MFA-unique
		/// data that cannot be reported with other messages.
		/// @Optional Implementation optional if desired by the MFA Provider, i.e., if there is
		/// any MFA-unique data that cannot be reported with other messages.
		[[nodiscard]] virtual ams::iface::mel::MFA_StatusDetailed getStatusDetailed() const = 0;

		/// @brief Registers a function to handle DetailedStatus updates.
		/// @Required This function passes function regestration data and must be provided by the
		/// implementer for all RF MEL implementations.
		virtual void setCallback(std::function<void(const ams::iface::mel::MFA_StatusDetailed &)> cb) = 0;

		/// @brief Support-Capability Status of a single Antenna.
		/// @Required This function passes function status data and must be provided by the implementer
		/// for all RF MEL implementations.
		[[nodiscard]] virtual AntennaStatus getAntennaStatus() const = 0;

		/// @brief Registers a function to handle AntennaStatus updates.
		/// @Required This function passes function regestration data and must be provided by the
		/// implementer for all RF MEL implementations.
		virtual void setCallback(std::function<void(const AntennaStatus &)> cb) = 0;

		/// @brief Current status of a discrete within the system.
		/// @Required This function passes function status data and must be provided by the implementer
		/// for all RF MEL implementations.
		[[nodiscard]] virtual ams::iface::mel::DiscreteStatus getDiscreteStatus() const = 0;

		/// @brief Registers a function to handle DiscreteStatus updates.
		/// @Required This function passes function regestration data and must be provided by the
		/// implementer for all RF MEL implementations.
		virtual void setCallback(std::function<void(const ams::iface::mel::DiscreteStatus &)> cb) = 0;

		/// @brief Current MFA configuration.
		/// @Required This function passes MFA configuration data and must be provided by the
		/// implementer for all RF MEL implementations.
		[[nodiscard]] virtual SubsystemConfiguration getSubsystemConfiguration() const = 0;

		/// @brief Registers a function to handle MFA configuration updates.
		/// @Required This function passes function regestration data and must be provided by the
		/// implementer for all RF MEL implementations.
		virtual void setCallback(std::function<void(const SubsystemConfiguration &)> cb) = 0;

		/// @brief Current MFA SecurityAuditRecord
		/// @Required This function passes MFA security audit record data and must be provided by the
		/// implementer for all RF MEL implementations.
		[[nodiscard]] virtual ams::iface::mel::MFA_SecurityAuditRecord getSecurityAuditRecord() const = 0;

		/// @brief Registers a function to handle security audit records
		/// @Required This function passes function regestration data and must be provided by the
		/// implementer for all RF MEL implementations.
		virtual void setCallback(std::function<void(const ams::iface::mel::MFA_SecurityAuditRecord &)> cb) = 0;

	protected:
		StatusBase() = default;
	};
} // namespace ams::iface::rfmel
