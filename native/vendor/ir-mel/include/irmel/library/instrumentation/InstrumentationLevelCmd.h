//===============================================================================
/// @file  InstrumentationLevelCmd.h
/// @brief This file includes the data definition of the Instrumentation Level Command.

#pragma once

#include <irmel/library/irmel-types/CommonIR_MEL.h>
#include <cstdint>

/// @namespace ams::iface::irmel
/// This namespace contains all of the GRA functionality for the IR MEL
namespace ams::iface::irmel
{
	/// @class InstrumentationLevelCmd
	/// @brief This class is used to control how much data is recorded from the
	/// MFA - currently debug or normal.
	/// @RequiredIfInstrumentation This class provides data definition in support of conditionally required IR MEL functionality to command
	/// instrumentation levels and must be included as-is in IR MEL implementations which support instrumentation
	class InstrumentationLevelCmd
	{
	public:
		InstrumentationLevelCmd() = default;
		~InstrumentationLevelCmd() = default;
		InstrumentationLevelCmd(const InstrumentationLevelCmd&) = default;
		InstrumentationLevelCmd(InstrumentationLevelCmd&&) = default;
		InstrumentationLevelCmd& operator=(const InstrumentationLevelCmd&) = default;
		InstrumentationLevelCmd& operator=(InstrumentationLevelCmd&&) = default;

		[[nodiscard]] std::uint32_t getCommandID() const
		{
			return this->commandID;
		}
		void setCommandID(std::uint32_t commandID_in)
		{
			this->commandID = commandID_in;
		}
		[[nodiscard]] const Priority& getInstrumentationPriority() const
		{
			return this->instrumentationPriority;
		}
		void setInstrumentationPriority(const Priority& instrumentationPriority_in)
		{
			this->instrumentationPriority = instrumentationPriority_in;
		}

	private:
		std::uint32_t commandID{0};
		Priority instrumentationPriority{Priority::Normal};
	};
} // end namespace ams::iface::irmel
