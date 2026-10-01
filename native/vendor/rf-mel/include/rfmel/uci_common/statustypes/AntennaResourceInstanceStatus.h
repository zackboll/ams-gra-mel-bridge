#pragma once

#include <mel/library/NameValuePair.h>
#include <rfmel/rfmeltypes/RFMELStatusEnums.h>
#include <utility>

namespace ams::iface::rfmel
{
	/// @brief Status of antenna resource instance.
	/// @Required This class provides data definition associated with status reporting
	/// and must be included as-is in all RF MEL implementations.
	class AntennaResourceInstanceStatus
	{
	public:
		AntennaResourceInstanceStatus() = default;
		AntennaResourceInstanceStatus(ams::iface::mel::ForeignKey fk, CapabilityAvailability cap)
			: antennaResourceInstanceIndex{std::move(fk)}, availability{cap}
		{
		}
		~AntennaResourceInstanceStatus() = default;
		AntennaResourceInstanceStatus(const AntennaResourceInstanceStatus&) = default;
		AntennaResourceInstanceStatus(AntennaResourceInstanceStatus&&) = default;
		AntennaResourceInstanceStatus& operator=(const AntennaResourceInstanceStatus&) = default;
		AntennaResourceInstanceStatus& operator=(AntennaResourceInstanceStatus&&) = default;

		/// @brief Gets the Antenna Resource Instance Index.
		[[nodiscard]] const ams::iface::mel::ForeignKey& getAntennaResourceInstanceIndex() const
		{
			return this->antennaResourceInstanceIndex;
		}
		/// @brief Sets the Antenna Resource Instance Index.
		void setAntennaResourceInstanceIndex(ams::iface::mel::ForeignKey newValue)
		{
			this->antennaResourceInstanceIndex = newValue;
		}
		/// @brief Gets the Availability.
		[[nodiscard]] const CapabilityAvailability& getAvailability() const
		{
			return this->availability;
		}
		/// @brief Sets the Availability.
		void setAvailability(CapabilityAvailability newValue)
		{
			this->availability = newValue;
		}

	private:
		// ID of the antenna resource instance
		ams::iface::mel::ForeignKey antennaResourceInstanceIndex;
		// Indicates the availability of the Antenna Resource Instance.
		CapabilityAvailability availability{CapabilityAvailability::NotSet};
	};
} // namespace ams::iface::rfmel
