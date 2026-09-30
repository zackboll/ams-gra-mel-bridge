#pragma once

#include <mel/library/NameValuePair.h>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <variant>

namespace ams::iface::rfmel
{
	class StatusControl;
	class BIT_Control;
	class CalibrationControl;
	class SettingsControl;
	class PNT_Control;
	class EMCON_Control;

	/// @brief Provides the interface to an OMS Adapter that isolates the MFA from
	/// the Abstract Service Bus (ASB).
	/// @Required This class provides required RF MEL functionality for providing
	/// an MFA adapter interface, and must be provided by the implementer in all
	/// RF MEL implementations. See individual members for details.
	class UCI_Control
	{
	public:
		virtual ~UCI_Control() = default;
		UCI_Control(const UCI_Control &) = delete;
		UCI_Control(UCI_Control &&) = delete;
		UCI_Control &operator=(const UCI_Control &) = delete;
		UCI_Control &operator=(UCI_Control &&) = delete;

		/// @brief Enables commanding of status information by a service, i.e. the MEL OMS Adapter.
		/// @note If this type of information is not supported by the MFA, the function is expected to return a nullptr.
		/// @Required This function passes MFA adapter data and must be provided by the implementer for all RF MEL implementations.
		[[nodiscard]] virtual std::shared_ptr<StatusControl> getStatusControl() = 0;
		/// @brief Enables commanding of BIT information by a service, i.e. the MEL OMS Adapter.
		/// @note If this type of information is not supported by the MFA, the function is expected to return a nullptr.
		/// @Required This function passes MFA adapter data and must be provided by the implementer for all RF MEL implementations.
		[[nodiscard]] virtual std::shared_ptr<BIT_Control> getBITControl() = 0;
		/// @brief Enables commanding of calibration information by a service, i.e. the MEL OMS Adapter.
		/// @note If this type of information is not supported by the MFA, the function is expected to return a nullptr.
		/// @Required This function passes MFA adapter data and must be provided by the implementer for all RF MEL implementations.
		[[nodiscard]] virtual std::shared_ptr<CalibrationControl> getCalibrationControl() = 0;
		/// @brief Enables commanding of MFA settings by a service, i.e. the MEL OMS Adapter.
		/// @note If this type of information is not supported by the MFA, the function is expected to return a nullptr.
		/// @Required This function passes MFA adapter data and must be provided by the implementer for all RF MEL implementations.
		[[nodiscard]] virtual std::shared_ptr<SettingsControl> getSettingsControl() = 0;
		/// @brief Enables commanding of PNT information by a service, i.e. the MEL OMS Adapter.
		/// @note If this type of information is not supported by the MFA, the function is expected to return a nullptr.
		/// @Required This function passes MFA adapter data and must be provided by the implementer for all RF MEL implementations.
		[[nodiscard]] virtual std::shared_ptr<PNT_Control> getPNTControl() = 0;
		/// @brief Enables commanding of EMCON information by a service, i.e. the MEL OMS Adapter.
		/// @note If this type of information is not supported by the MFA, the function is expected to return a nullptr.
		/// @Required This function passes MFA adapter data and must be provided by the implementer for all RF MEL implementations.
		[[nodiscard]] virtual std::shared_ptr<EMCON_Control> getEMCONControl() = 0;

	protected:
		UCI_Control() = default;
	};

} // namespace ams::iface::rfmel
