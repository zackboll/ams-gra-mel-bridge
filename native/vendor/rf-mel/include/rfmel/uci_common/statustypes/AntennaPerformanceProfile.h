#pragma once

#include <mel/library/NameValuePair.h>
#include <rfmel/rfmeltypes/RFMELTypes.h>
#include <cstdint>
#include <utility>
namespace ams::iface::rfmel
{
	/// @brief Defines the AntennaPerformanceProfile type.
	/// @Required This class provides data definition associated with status reporting
	/// and must be included as-is in all RF MEL implementations.
	class AntennaPerformanceProfile
	{
	public:
		AntennaPerformanceProfile() = default;
		AntennaPerformanceProfile(AnglePair el, AnglePair az, double duty, uint32_t beam, FrequencyRange b)
			: elevationFieldOfRegard{std::move(el)}, azimuthFieldOfRegard{std::move(az)}, dutyFactorLimit{duty}, beamLimit{beam}, band{b}
		{
		}
		~AntennaPerformanceProfile() = default;
		AntennaPerformanceProfile(const AntennaPerformanceProfile&) = default;
		AntennaPerformanceProfile(AntennaPerformanceProfile&&) = default;
		AntennaPerformanceProfile& operator=(const AntennaPerformanceProfile&) = default;
		AntennaPerformanceProfile& operator=(AntennaPerformanceProfile&&) = default;

		/// @brief Gets the Elevation Field Of Regard.
		[[nodiscard]] const AnglePair& getElevationFieldOfRegard() const
		{
			return this->elevationFieldOfRegard;
		}
		/// @brief Sets the Elevation Field Of Regard.
		void setElevationFieldOfRegard(AnglePair newValue)
		{
			this->elevationFieldOfRegard = newValue;
		}
		/// @brief Gets the Azimuth Field Of Regard.
		[[nodiscard]] const AnglePair& getAzimuthFieldOfRegard() const
		{
			return this->azimuthFieldOfRegard;
		}
		/// @brief Sets the Azimuth Field Of Regard.
		void setAzimuthFieldOfRegard(AnglePair newValue)
		{
			this->azimuthFieldOfRegard = newValue;
		}
		/// @brief Gets the Duty Factor Limit.
		[[nodiscard]] DutyFactor getDutyFactorLimit() const
		{
			return this->dutyFactorLimit;
		}
		/// @brief Sets the Duty Factor Limit.
		void setDutyFactorLimit(DutyFactor newValue)
		{
			this->dutyFactorLimit = newValue;
		}
		/// @brief Gets the Beam Limit.
		[[nodiscard]] uint32_t getBeamLimit() const
		{
			return this->beamLimit;
		}
		/// @brief Gets the Beam Limit.
		void setBeamLimit(uint32_t newValue)
		{
			this->beamLimit = newValue;
		}
		/// @brief Gets the frequency band.
		[[nodiscard]] const FrequencyRange& getBand() const
		{
			return this->band;
		}
		/// @brief Sets the frequency band.
		void setBand(FrequencyRange newValue)
		{
			this->band = newValue;
		}

	private:
		// Indicates the elevation field of regard relative to its installation orientation/boresight.
		AnglePair elevationFieldOfRegard;
		// Indicates the azimuth field of regard relative to its installation orientation/boresight.
		AnglePair azimuthFieldOfRegard;
		// Indicates the Maximum duty factor.
		DutyFactor dutyFactorLimit{0.0};
		// Indicates the Maximum number of beams.
		uint32_t beamLimit{0};
		// Indicates the bandwidth.
		FrequencyRange band;
	};
} // namespace ams::iface::rfmel
