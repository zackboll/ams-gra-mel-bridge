#pragma once

#include <mel/library/UCI_ID.h>
#include <rfmel/uci_common/statustypes/AntennaPerformanceProfile.h>
#include <rfmel/uci_common/statustypes/AntennaResourceInstanceStatus.h>
#include <rfmel/uci_common/statustypes/SupportCapabilityStatusBase.h>
#include <optional>
#include <utility>
#include <vector>

namespace ams::iface::rfmel
{
	/// @brief This type represents the status and settings for an Antenna that
	/// is relevant to mission operations.
	/// @Required This class provides data definition associated with status reporting
	/// and must be included as-is in all RF MEL implementations.
	class AntennaStatus : public SupportCapabilityStatusBase
	{
	public:
		AntennaStatus() = default;
		AntennaStatus(bool support, std::vector<ams::iface::mel::UCI_ID> cap, double power, AntennaPerformanceProfile profile,
					  std::vector<AntennaResourceInstanceStatus> status)
			: beamConfigSupport{support},
			  capabilityIDs{std::move(cap)},
			  powerConsumption{power},
			  antennaPerformanceProfile{profile},
			  antennaResourceStatus{std::move(status)}
		{
		}
		~AntennaStatus() = default;
		AntennaStatus(const AntennaStatus&) = default;
		AntennaStatus(AntennaStatus&&) = default;
		AntennaStatus& operator=(const AntennaStatus&) = default;
		AntennaStatus& operator=(AntennaStatus&&) = default;

		[[nodiscard]] bool getBeamConfigSupport() const
		{
			return this->beamConfigSupport;
		}
		void setBeamConfigSupport(bool newValue)
		{
			this->beamConfigSupport = newValue;
		}
		[[nodiscard]] const std::vector<ams::iface::mel::UCI_ID>& getCapabilityIDs() const
		{
			return this->capabilityIDs;
		}
		// replace the existing vector with a new vector
		void setCapabilityIDs(const std::vector<ams::iface::mel::UCI_ID>& newValue)
		{
			this->capabilityIDs = newValue;
		}
		// add a new element to the vector
		void addCapabilityID(const ams::iface::mel::UCI_ID& id)
		{
			this->capabilityIDs.push_back(id);
		}
		[[nodiscard]] std::optional<double> getPowerConsumption() const
		{
			return this->powerConsumption;
		}
		void setPowerConsumption(std::optional<double> newValue)
		{
			this->powerConsumption = newValue;
		}
		[[nodiscard]] std::optional<AntennaPerformanceProfile> getAntennaPerformanceProfile() const
		{
			return this->antennaPerformanceProfile;
		}
		void setAntennaPerformanceProfile(const std::optional<AntennaPerformanceProfile> newValue)
		{
			this->antennaPerformanceProfile = newValue;
		}
		[[nodiscard]] const std::vector<AntennaResourceInstanceStatus>& getAntennaResourceStatus() const
		{
			return this->antennaResourceStatus;
		}
		void setAntennaResourceStatus(const std::vector<AntennaResourceInstanceStatus>& newValue)
		{
			this->antennaResourceStatus = newValue;
		}

	private:
		/// @brief Indicates whether or not this antenna accept beam configuration
		/// through the RF_ThreadInstanceSetupCommand.
		bool beamConfigSupport{false};
		/// @brief Indicates the ID of a Capability whose use requires access to
		/// an antenna resource.
		std::vector<ams::iface::mel::UCI_ID> capabilityIDs;
		/// @brief Indicates power consumption in Watts.
		std::optional<double> powerConsumption;
		/// @brief Provides performance characterization on the AntennaID specified.
		std::optional<AntennaPerformanceProfile> antennaPerformanceProfile;
		std::vector<AntennaResourceInstanceStatus> antennaResourceStatus;
	};
} // namespace ams::iface::rfmel
