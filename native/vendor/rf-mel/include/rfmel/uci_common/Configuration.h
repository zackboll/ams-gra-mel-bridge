#pragma once

#include <mel/library/MFA_Component.h>
#include <mel/library/UCI_ID.h>
#include <vector>

namespace ams::iface::rfmel
{
	/// @class Configuration
	/// @brief This type is aligned with the OMS/UCI Configuration type
	/// @Required This class defines hardware and capabilities information structure of an MFA
	/// and must be included as-is in all RF MEL implementations.
	class Configuration
	{
	public:
		Configuration() = default;
		Configuration(const Configuration&) = default;
		Configuration(Configuration&&) = default;
		Configuration& operator=(const Configuration&) = default;
		Configuration& operator=(Configuration&&) = default;
		~Configuration() = default;
		[[nodiscard]] const ams::iface::mel::About& getAbout() const
		{
			return about;
		}
		void setAbout(const ams::iface::mel::About& about_in)
		{
			this->about = about_in;
		}
		[[nodiscard]] const std::vector<ams::iface::mel::UCI_ID>& getCapabilityIDs() const
		{
			return capabilityIDs;
		};
		void setCapabilityIDs(const std::vector<ams::iface::mel::UCI_ID>& newValue)
		{
			this->capabilityIDs = newValue;
		}
		void addCapabilityID(const ams::iface::mel::UCI_ID& id)
		{
			this->capabilityIDs.push_back(id);
		}

	private:
		/// @brief Contains hardware information
		ams::iface::mel::About about{};
		/// @brief Contains unique capabilities that are currently being used
		/// the uuid is used as the key when storing a capability id
		std::vector<ams::iface::mel::UCI_ID> capabilityIDs{};
	};
} // namespace ams::iface::rfmel
