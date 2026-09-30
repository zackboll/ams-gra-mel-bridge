#pragma once

#include <mel/library/CommonMEL.h>
#include <rfmel/rfmeltypes/RFMELTypes.h>
#include <chrono>
#include <optional>
#include <string>
#include <variant>
#include <cstdint>

namespace ams::iface::rfmel
{
	/// @brief Indicates the availability state of the defined capability.
	/// @Required This enumeration provides data definition associated with status reporting
	/// and must be included as-is in all RF MEL implementations.
	enum class CapabilityAvailability : uint32_t
	{
		NotSet, /// enum has not been set
		Available,
		Unavailable,
		TemporarilyUnavailable,
		Expended,
		Disabled,
		Faulted,
		MaxExclusive /// maximun enum item
	};

	/// @brief Indicates Capability and Service independent settings for Subsystems.
	/// @Required This enumeration provides data definition associated with system settings
	/// and must be included as-is in all RF MEL implementations.
	enum class SubsystemSetting : uint32_t
	{
		NotSet, /// enum has not been set
		Live,
		Exercise,
		Simulation,
		Test,
		TestInstrumentation,
		DataRecording,
		DebugReporting,
		TransmitLevel,
		MaxExclusive /// maximun enum item
	};

	/// @brief Indicates an accepted state command for the Capability.
	/// @Required This enumeration provides data definition associated with system settings
	/// and must be included as-is in all RF MEL implementations.
	enum class CapabilityStateCommand : uint32_t
	{
		NotSet, /// enum has not been set
		Disable,
		Enable,
		Reset,
		MaxExclusive /// maximun enum item

	};

	/// @brief Indicates the setting of the associated command.
	/// @Required This enumeration provides data definition associated with system settings
	/// and must be included as-is in all RF MEL implementations.
	enum class ComponentSetting : uint32_t
	{
		NotSet, /// enum has not been set
		Parameter,
		Power,
		Test,
		TestInstrumentation,
		DataRecording,
		DebugReporting,
		TransmitLevel,
		MaxExclusive /// maximun enum item
	};

	/// @brief Indicates the state of the message.
	/// @Required This enumeration provides data definition associated with messaging
	/// and must be included as-is in all RF MEL implementations.
	enum class MessageState : uint32_t
	{
		NotSet, /// enum has not been set
		New,
		Update,
		Remove,
		MaxExclusive /// maximun enum item
	};

	/// @brief Indicates the type of line projection.
	/// @Required This enumeration provides data definition associated with UCI and must be included as-is in all RF MEL implementations.
	enum class LineProjection : uint32_t
	{
		NotSet, /// enum has not been set
		GreatCirlce,
		Rhumb,
		MaxExclusive /// maximun enum item
	};

} // namespace ams::iface::rfmel
