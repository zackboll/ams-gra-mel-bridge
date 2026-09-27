#pragma once

#include <rfmel/rfmeltypes/RFMEL.h>

#include <functional>
#include <map>
#include <string>
#include <type_traits>
#include <memory>
#include <string_view>

namespace ams::iface::rfmel
{
	class AdminMEL;
	class C2MEL;
	class COSITEMEL;
	class DataMEL;
	class DEAMEL;
	class MonitorMEL;

	/// @brief Pointer that represents a specific create AdminMEL method from a MEL implementation
	using fnAdminMEL = std::shared_ptr<AdminMEL> (*)(std::string_view);

	/// @brief Pointer that represents a specific create C2MEL method from a MEL implementation
	using fnC2MEL = std::shared_ptr<C2MEL> (*)(std::string_view);

	/// @brief Pointer that represents a specific create COSITEMEL method from a MEL implementation
	using fnCOSITEMEL = std::shared_ptr<COSITEMEL> (*)(std::string_view);

	/// @brief Pointer that represents a specific create DataMEL method from a MEL implementation
	using fnDataMEL = std::shared_ptr<DataMEL> (*)(std::string_view);

	/// @brief Pointer that represents a specific create DEAMEL method from a MEL implementation
	using fnDEAMEL = std::shared_ptr<DEAMEL> (*)(std::string_view);

	/// @brief Pointer that represents a specific create MonitorMEL method from a MEL implementation
	using fnMonitorMEL = std::shared_ptr<MonitorMEL> (*)(std::string_view);
} // namespace ams::iface::rfmel
