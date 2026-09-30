#pragma once

#include <mel/library/NameValuePair.h>
#include <mel/library/UCI_ID.h>
#include <rfmel/rfmeltypes/RFMELStatusEnums.h>
#include <utility>

namespace ams::iface::rfmel
{
	/// @brief Abstract base for reporting Availability of a Capability.
	/// @Required This class provides data definition associated with status reporting
	/// and must be included as-is in all RF MEL implementations.
	class SupportCapabilityStatusBase
	{
	public:
		SupportCapabilityStatusBase() = default;
		SupportCapabilityStatusBase(ams::iface::mel::UCI_ID sup, CapabilityAvailability avail)
			: supportCapabilityID{std::move(sup)}, availability{avail}
		{
		}
		~SupportCapabilityStatusBase() = default;
		SupportCapabilityStatusBase(const SupportCapabilityStatusBase&) = default;
		SupportCapabilityStatusBase(SupportCapabilityStatusBase&&) = default;
		SupportCapabilityStatusBase& operator=(const SupportCapabilityStatusBase&) = default;
		SupportCapabilityStatusBase& operator=(SupportCapabilityStatusBase&&) = default;

		[[nodiscard]] const ams::iface::mel::UCI_ID& getSupportCapabilityID() const
		{
			return this->supportCapabilityID;
		}
		void setSupportCapabilityID(const ams::iface::mel::UCI_ID& newValue)
		{
			this->supportCapabilityID = newValue;
		}
		[[nodiscard]] const CapabilityAvailability& getAvailability() const
		{
			return this->availability;
		}
		void setAvailability(CapabilityAvailability newValue)
		{
			this->availability = newValue;
		}

	private:
		/// @brief Indicates the unique ID of the Support Capability corresponding to this message.
		ams::iface::mel::UCI_ID supportCapabilityID;
		/// @brief Indicates the availability of the Support Capability corresponding to this message.
		CapabilityAvailability availability{CapabilityAvailability::NotSet};
	};
} // namespace ams::iface::rfmel
