// This header defines the specific data types available to describe a location
// relative to the earth or the applicable platform.
#pragma once

#include <math/geometry/Geometry.h>
#include <math/geometry/LLAPoint.h>
#include <math/geometry/RangeAzEl.h>
#include <math/units/UTCTime.h>
#include <rfmel/rfmeltypes/RFMELTypes.h>

namespace ams::iface::rfmel
{
	/// @brief Specifies a point in space given in Earth-Centered, Earth-Fixed coordinate system (meters).
	/// @RequiredIfECEFPointing This class provides data definition in support
	/// of required RF MEL functionality specific to Geolocation and must be
	/// included as-is in all RF MEL implementations if the associated
	/// MFA supports ECEFPointing.
	class ECEFPointing
	{
	public:
		ECEFPointing() = default;
		~ECEFPointing() = default;
		/// @brief Default copy constructor
		ECEFPointing(const ECEFPointing&) = default;

		/// @brief Default copy assignment operator
		ECEFPointing& operator=(const ECEFPointing&) = default;

		/// @brief Move constructor
		ECEFPointing(ECEFPointing&& other) noexcept
			: locationEcef{other.locationEcef}, velocityEcef{other.velocityEcef}, timeOfValidity{other.timeOfValidity}
		{
		}

		/// @brief Move assignment operator
		ECEFPointing& operator=(ECEFPointing&& other) noexcept
		{
			if(this != &other)
			{
				locationEcef = other.locationEcef;
				velocityEcef = other.velocityEcef;
				timeOfValidity = other.timeOfValidity;
			}
			return *this;
		}

		/// @brief Get EcefPoint location.
		[[nodiscard]] const auto& getLocation() const
		{
			return this->locationEcef;
		}
		/// @brief Get Ecef velocity.
		[[nodiscard]] const auto& getVelocity() const
		{
			return this->velocityEcef;
		}
		/// @brief Get time of validity.
		[[nodiscard]] const auto& getTimeOfValidity() const
		{
			return this->timeOfValidity;
		}
		/// @brief Set EcefPoint location.
		void setLocation(const EcefPoint& locationEcef_in)
		{
			this->locationEcef = locationEcef_in;
		}
		/// @brief Set Ecef velocity.
		void setVelocity(const EcefVelocity& velocityEcef_in)
		{
			this->velocityEcef = velocityEcef_in;
		}
		/// @brief Set time of validity.
		void setTimeOfValidity(const ams::util::math::UTCTime& timeOfValidity_in)
		{
			this->timeOfValidity = timeOfValidity_in;
		}

	private:
		EcefPoint locationEcef{};
		EcefVelocity velocityEcef{};
		ams::util::math::UTCTime timeOfValidity;
	};

	/// @brief Specifies a point in space given in WGS-84 coordinates (radians and meters).
	/// @RequiredIfLLAPointing This class provides data definition in support
	/// of required RF MEL functionality specific to Geolocation and must be
	/// included as-is in all RF MEL implementations if the associated
	/// MFA supports LLAPointing.
	class LLAPointing
	{
	public:
		LLAPointing() = default;
		~LLAPointing() = default;
		/// @brief Default copy constructor
		LLAPointing(const LLAPointing&) = default;

		/// @brief Default copy assignment operator
		LLAPointing& operator=(const LLAPointing&) = default;

		/// @brief Move constructor
		LLAPointing(LLAPointing&& other) noexcept
			: locationLla{other.locationLla}, velocityNed{other.velocityNed}, timeOfValidity{other.timeOfValidity}
		{
			other.locationLla = LLAPoint();
			other.timeOfValidity = ams::util::math::UTCTime();
		}

		/// @brief Move assignment operator
		LLAPointing& operator=(LLAPointing&& other) noexcept
		{
			if(this != &other)
			{
				locationLla = other.locationLla;
				velocityNed = other.velocityNed;
				timeOfValidity = other.timeOfValidity;
			}
			return *this;
		}

		/// @brief Get LLAPoint location.
		[[nodiscard]] const auto& getLocation() const
		{
			return locationLla;
		}
		/// @brief Get Ned Velocity.
		[[nodiscard]] const auto& getVelocity() const
		{
			return velocityNed;
		}
		/// @brief Get time of validity.
		[[nodiscard]] auto getTimeOfValidity() const
		{
			return this->timeOfValidity;
		}
		/// @brief Set LLAPoint location.
		void setLocation(const LLAPoint& locationLla_in)
		{
			this->locationLla = locationLla_in;
		}
		/// @brief Set Ned Velocity.
		void setVelocity(const NedVelocity& velocityNed_in)
		{
			this->velocityNed = velocityNed_in;
		}
		/// @brief Set time of validity.
		void setTimeOfValidity(const ams::util::math::UTCTime& timeOfValidity_in)
		{
			this->timeOfValidity = timeOfValidity_in;
		}

	private:
		LLAPoint locationLla{};
		NedVelocity velocityNed{};
		ams::util::math::UTCTime timeOfValidity;
	};

	/// @brief Specifies a pointing vector relative to the platform of reference (radians).
	/// @RequiredIfPlatformRelativePointing This class provides data definition in support
	/// of required RF MEL functionality specific to Geolocation and must be
	/// included as-is in all RF MEL implementations if the associated
	/// MFA supports PlatformRelativePointing.
	class PlatformRelativePointing
	{
	public:
		PlatformRelativePointing() = default;
		explicit PlatformRelativePointing(AzEl azEl) : location(azEl)
		{
		}
		PlatformRelativePointing(const PlatformRelativePointing& other) = default;
		PlatformRelativePointing(const double az, const double el) : location(AzEl{az, el})
		{
		}
		~PlatformRelativePointing() = default;
		PlatformRelativePointing& operator=(const PlatformRelativePointing& other) = default;
		PlatformRelativePointing& operator=(PlatformRelativePointing&& other) noexcept
		{
			if(this != &other)
			{
				location = other.location;
			}
			return *this;
		}
		PlatformRelativePointing(PlatformRelativePointing&& other) noexcept : location(other.location)
		{
		}

		/// @brief Get Azimuth/Elevation location.
		[[nodiscard]] const AzEl& getLocation() const
		{
			return location;
		}
		/// @brief Set Azimuth/Elevation location.
		void setLocation(const AzEl& location_in)
		{
			this->location = location_in;
		}
		/// @brief Set Azimuth/Elevation location.
		void setLocation(double az, double el)
		{
			this->location = AzEl{az, el};
		}

	private:
		AzEl location;
	};

	/// @brief Specifies a pointing vector relative to the face of the aperture (radians).
	/// @Required This class provides data definition in support of required RF MEL functionality to provide
	/// pointing vectors and must be included as-is in all RF MEL implementations.
	class FaceRelativePointing
	{
	public:
		FaceRelativePointing() = default;
		explicit FaceRelativePointing(AzEl location_in) : location(location_in)
		{
		}
		FaceRelativePointing(const FaceRelativePointing& other) = default;
		FaceRelativePointing(FaceRelativePointing&& other) noexcept : location(other.location)
		{
		}
		~FaceRelativePointing() = default;
		FaceRelativePointing& operator=(const FaceRelativePointing& other) = default;
		FaceRelativePointing& operator=(FaceRelativePointing&& other) noexcept
		{
			if(this != &other)
			{
				location = other.location;
			}
			return *this;
		}

		/// @brief Get Azimuth/Elevation location.
		[[nodiscard]] const auto& getLocation() const
		{
			return location;
		}
		/// @brief Set Azimuth/Elevation location.
		void setLocation(const AzEl& location_in)
		{
			this->location = location_in;
		}

	private:
		AzEl location;
	};

	/// @brief Specifies a pointing vector relative to the baseline of the aperture (radians).
	/// @RequiredIfBaselineRelativePointing This class provides data definition in support
	/// of required RF MEL functionality specific to pointing vectors and must be
	/// included as-is in all RF MEL implementations if the associated
	/// MFA supports pointing vectors relative to the baseline of the aperture in radians.
	class BaselineRelativePointing
	{
	public:
		BaselineRelativePointing() = default;
		explicit BaselineRelativePointing(Conic location_in) : location(location_in)
		{
		}
		BaselineRelativePointing(const BaselineRelativePointing& other) = default;
		BaselineRelativePointing(BaselineRelativePointing&& other) noexcept : location(other.location)
		{
		}
		~BaselineRelativePointing() = default;
		BaselineRelativePointing& operator=(const BaselineRelativePointing& other) = default;
		BaselineRelativePointing& operator=(BaselineRelativePointing&& other) noexcept
		{
			if(this != &other)
			{
				location = other.location;
			}
			return *this;
		}

		/// @brief Get Conic location.
		[[nodiscard]] auto getLocation() const
		{
			return location;
		}
		/// @brief Set Conic location.
		void setLocation(Conic location_in)
		{
			this->location = location_in;
		}

	private:
		Conic location{0.0};
	};
} // namespace ams::iface::rfmel
