#pragma once

#include <mel/library/MFA_Component.h>

#include <rfmel/rfmeltypes/RFMELTypes.h>

namespace ams::iface::rfmel
{
	/// @brief The base class of MFA Face Physical Data including dimensions and mounting /// orientation.
	/// @Required This class provides data definition in support of required RF MEL functionality to provide
	/// mounting and/or orientation and must be included as-is in all RF MEL implementations.
	class PhysicalData
	{
	public:
		PhysicalData() = default;
		~PhysicalData() = default;
		PhysicalData(const PhysicalData&) = default;
		PhysicalData(PhysicalData&&) = default;
		PhysicalData& operator=(const PhysicalData&) = default;
		PhysicalData& operator=(PhysicalData&&) = default;

		/// @brief Get Antenna Height (short axis) (meters).
		[[nodiscard]] const auto getAntennaHeight() const
		{
			return antennaHeight;
		}
		/// @brief Get Antenna Width (long axis) (meters).
		[[nodiscard]] const auto getAntennaWidth() const
		{
			return antennaWidth;
		}
		/// @brief Get the Antenna lattice angle which descibes the orientation of
		/// the elements relative to each other in an element grid (i.e. slanted grid).
		/// It is used to compute mainbeam shape. (radians)
		[[nodiscard]] const auto getLatticeAngle() const
		{
			return latticeAngle;
		}

		/// @brief Returns the Installation Details consisting of location, orientation, and boresight
		[[nodiscard]] const auto& getInstallationDetails() const
		{
			return installationDetails;
		}

		/// @brief Returns the Installation Details consisting of location, orientation, and boresight
		[[nodiscard]] auto& getInstallationDetails() // non-const
		{
			return installationDetails;
		}

		/// @brief Set Antenna Height (short axis) (meters).
		void setAntennaHeight(const double antennaHeight_in)
		{
			this->antennaHeight = antennaHeight_in;
		}
		/// @brief Set Antenna Width (long axis) (meters).
		void setAntennaWidth(const double antennaWidth_in)
		{
			this->antennaWidth = antennaWidth_in;
		}
		/// @brief Set the Antenna lattice angle which descibes the orientation of
		/// the elements relative to each other in an element grid (i.e. slanted grid).
		/// It is used to compute mainbeam shape. (radians)
		void setLatticeAngle(const double latticeAngle_in)
		{
			this->latticeAngle = latticeAngle_in;
		}

		/// @brief Sets the Installation Details consisting of location, orientation, and boresight
		void setInstallationDetails(ams::iface::mel::InstallationDetails installationDetails_in)
		{
			this->installationDetails = installationDetails_in;
		}

	private:
		/// Antenna Height (short axis) (meters)
		double antennaHeight{0.0};

		/// Antenna Width (long axis) (meters)
		double antennaWidth{0.0};

		/// Holds the installation details (location, orientation, boresight)
		ams::iface::mel::InstallationDetails installationDetails;

		/// The Antenna lattice angle descibes the orientation of
		/// the elements relative to each other in an element grid (i.e. slanted grid).
		/// It is used to compute mainbeam shape. (radians)
		double latticeAngle{0.0};
	};

} // namespace ams::iface::rfmel
