#pragma once

#include <rfmel/uci_common/uci_base/StatusBase.h>

namespace ams::iface::rfmel
{

	/// @brief API to command the MFA into different Subsystem States and retrieve its status.
	/// @note there is no SubsystemID in this API; the OMS Adapter is responsible for providing
	/// all Service/Subsystem IDs.
	/// @Required This class provides required RF MEL functionality for commanding the MFA and
	/// the selected VA instance, and must be provided by the implementer in all RF MEL
	/// implementations. See individual members for details.
	class StatusControl : public StatusBase
	{
	public:
		~StatusControl() override = default;
		StatusControl(const StatusControl &) = delete;
		StatusControl(StatusControl &&) = delete;
		StatusControl &operator=(const StatusControl &) = delete;
		StatusControl &operator=(StatusControl &&) = delete;

		/// Controller Methods:

		/// @brief Commands an MFA to transition to another of its states.
		/// @return True if command was accepted, false otherwise.
		/// @Required This function passes MFA Commands and must be provided by the implementer
		/// for all RF MEL implementations.
		[[nodiscard]] virtual bool commandState(ams::iface::mel::MFA_State state) = 0;

		/// @brief Commands an MFA to erase data from memory.
		/// @return true if command was accepted, false otherwise.
		/// Only used for MFAs that can perform erase processing independent of
		/// Shutdown processing.
		/// @RequiredIfCommandErase This function passes MFA Commands and must be provided by the implementer
		/// for all RF MEL implementations if the associated MFA supports data erasure on command.
		[[nodiscard]] virtual bool commandErase() = 0;

	protected:
		StatusControl() = default;
	};
} // namespace ams::iface::rfmel
