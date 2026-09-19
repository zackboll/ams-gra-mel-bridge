//===============================================================================
/// @file  InstrumentationReport.h
/// @brief This file includes the GRA functionality for the Instrumentation IR MEL

#pragma once

#include <irmel/library/irmel-types/CommonIR_MEL.h>
#include <chrono>
#include <cstdint>

/// @namespace ams::iface::irmel
/// This namespace contains all of the GRA functionality for the IR MEL
/// Of interest is the Command Type enum class,
/// which documents all commands that can be sent from the MFP to the MFA.
/// The Classes in the file
/// describe how the connections between the MFA and MFP are made.
namespace ams::iface::irmel
{
	/// @class InstrumentationReport
	/// @brief This struct is used to record data from the MFA
	/// @RequiredIfInstrumentation This class provides data definition in support of conditionally required IR MEL functionality
	/// to report instrumentation and must be included as-is in IR MEL implementations which support instrumentation
	class InstrumentationReport
	{
	public:
		InstrumentationReport() = default;
		~InstrumentationReport() = default;
		InstrumentationReport(const InstrumentationReport&) = default;
		InstrumentationReport(InstrumentationReport&&) = default;
		InstrumentationReport& operator=(const InstrumentationReport&) = default;
		InstrumentationReport& operator=(InstrumentationReport&&) = default;

		[[nodiscard]] std::uint32_t getCommandID() const
		{
			return this->commandID;
		}
		void setCommandID(std::uint32_t commandID_in)
		{
			this->commandID = commandID_in;
		}
		[[nodiscard]] std::uint32_t getSize() const
		{
			return this->size;
		}
		void setSize(std::uint32_t size_in)
		{
			this->size = size_in;
		}
		[[nodiscard]] std::chrono::nanoseconds getTimestamp() const
		{
			return this->timestamp;
		}
		void setTimestamp(std::chrono::nanoseconds timestamp_in)
		{
			this->timestamp = timestamp_in;
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
		std::uint32_t size{0};
		std::chrono::nanoseconds timestamp{0};
		Priority instrumentationPriority{Priority::Normal};
	};

} // end namespace ams::iface::irmel
