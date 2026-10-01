#pragma once

#include <rfmel/virtualaperture/VirtualApertureInstanceStatusReport.h>
#include <vector>

namespace ams::iface::rfmel
{
	/// @brief Provides the status of every instance of this VA, as well as every
	/// LF instance owned by each of the VA instances.
	/// @Required This class provides data definition in support of required RF MEL functionality
	/// for VA Status reporting and must be included as-is in all RF MEL implementations.
	class VirtualApertureStatusReport
	{
	public:
		VirtualApertureStatusReport() = default;

		/// @brief Get Virtual Aperture Id.
		[[nodiscard]] auto getVaID() const
		{
			return this->vaDefID;
		}
		/// @brief Get vector of Virtual Aperture Instance Status Reports.
		[[nodiscard]] const auto &getVAInstanceIDs() const
		{
			return this->virtualApertureInstanceIDs;
		}
		/// @brief Set Virtual Aperture Id.
		void setVaID(VirtualApertureDefinitionID vaDefID_in)
		{
			this->vaDefID = vaDefID_in;
		}
		/// @brief Set vector of Virtual Aperture Instance Status Reports.
		void setVAInstanceIDs(const std::vector<VirtualApertureInstanceStatusReport> &virtualApertureInstanceIDs_in)
		{
			this->virtualApertureInstanceIDs = virtualApertureInstanceIDs_in;
		}

	private:
		VirtualApertureDefinitionID vaDefID{0};

		std::vector<VirtualApertureInstanceStatusReport> virtualApertureInstanceIDs{};
	};
} // namespace ams::iface::rfmel
