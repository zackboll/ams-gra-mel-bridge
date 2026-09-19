//===============================================================================
/// @file  IRSTTrackReport.h
/// @brief This file includes the definition of the IRST Track report as given by the MFA.

#pragma once

#include <irmel/library/irmel-types/CommonIR_MEL.h>
#include <mel/library/CommonMEL.h>
#include <chrono>
#include <cstdint>

/// @namespace ams::iface::irmel
/// This namespace contains all of the GRA functionality for the IR MEL
namespace ams::iface::irmel
{
	/// @class IRSTTrackReport
	/// @brief MFA Responds to an IRST task with this report
	/// @RequiredIfTrack This class provides data definition in support of conditionally required IR MEL functionality to report tracks
	/// and must be included as-is in all IR MEL implementations that support Tracks.
	class IRSTTrackReport
	{
	public:
		IRSTTrackReport() = default;
		~IRSTTrackReport() = default;
		IRSTTrackReport(const IRSTTrackReport&) = default;
		IRSTTrackReport(IRSTTrackReport&&) = default;
		IRSTTrackReport& operator=(const IRSTTrackReport&) = default;
		IRSTTrackReport& operator=(IRSTTrackReport&&) = default;

		[[nodiscard]] std::chrono::nanoseconds getSystemTime() const
		{
			return this->systemTime;
		}
		void setSystemTime(std::chrono::nanoseconds systemTime_in)
		{
			this->systemTime = systemTime_in;
		}
		[[nodiscard]] std::uint32_t getActivityId() const
		{
			return this->activityId;
		}
		void setActivityId(std::uint32_t activityId_in)
		{
			this->activityId = activityId_in;
		}
		[[nodiscard]] const ams::iface::mel::NorthEastDown& getMeasuredNed() const
		{
			return this->measuredNed;
		}
		void setMeasuredNed(ams::iface::mel::NorthEastDown measuredNed_in)
		{
			this->measuredNed = measuredNed_in;
		}
		[[nodiscard]] double getMeasuredIntensity() const
		{
			return this->measuredIntensity;
		}
		void setMeasuredIntensity(double measuredIntensity_in)
		{
			this->measuredIntensity = measuredIntensity_in;
		}
		[[nodiscard]] double getMeasuredSnr() const
		{
			return this->measuredSnr;
		}
		void setMeasuredSnr(double measuredSnr_in)
		{
			this->measuredSnr = measuredSnr_in;
		}
		[[nodiscard]] const ams::iface::mel::NorthEastDown& getFilteredNed() const
		{
			return this->filteredNed;
		}
		void setFilteredNed(ams::iface::mel::NorthEastDown filteredNed_in)
		{
			this->filteredNed = filteredNed_in;
		}
		[[nodiscard]] double getFilteredIntensity() const
		{
			return this->filteredIntensity;
		}
		void setFilteredIntensity(double filteredIntensity_in)
		{
			this->filteredIntensity = filteredIntensity_in;
		}
		[[nodiscard]] double getFilteredSnr() const
		{
			return this->filteredSnr;
		}
		void setFilteredSnr(double filteredSnr_in)
		{
			this->filteredSnr = filteredSnr_in;
		}
		[[nodiscard]] double getRange() const
		{
			return this->range;
		}
		void setRange(double range_in)
		{
			this->range = range_in;
		}
		[[nodiscard]] double getRangeError() const
		{
			return this->rangeError;
		}
		void setRangeError(double rangeError_in)
		{
			this->rangeError = rangeError_in;
		}
		[[nodiscard]] double getSpatialExtent() const
		{
			return this->spatialExtent;
		}
		void setSpatialExtent(double spatialExtent_in)
		{
			this->spatialExtent = spatialExtent_in;
		}

		[[nodiscard]] double getTrackQuality() const
		{
			return this->trackQuality;
		}

		void setTrackQuality(double trackQuality_in)
		{
			this->trackQuality = trackQuality_in;
		}

		[[nodiscard]] double getClutter() const
		{
			return this->clutter;
		}

		void setClutter(double clutter_in)
		{
			this->clutter = clutter_in;
		}

		[[nodiscard]] std::chrono::nanoseconds getAge() const
		{
			return this->age;
		}

		void setAge(std::chrono::nanoseconds age_in)
		{
			this->age = age_in;
		}

		[[nodiscard]] const IrstTrackState& getState() const
		{
			return this->state;
		}

		void setState(const IrstTrackState& state_in)
		{
			this->state = state_in;
		}

		[[nodiscard]] const IrstTrackMode& getMode() const
		{
			return this->mode;
		}

		void setMode(const IrstTrackMode& mode_in)
		{
			this->mode = mode_in;
		}

	private:
		std::chrono::nanoseconds systemTime{0};
		std::uint32_t activityId{0};
		ams::iface::mel::NorthEastDown measuredNed{}; ///< Latest Measurement information
		double measuredIntensity{0.0};				  ///< Latest Measurement information
		double measuredSnr{0.0};
		ams::iface::mel::NorthEastDown filteredNed{}; ///< Latest Measurement information
		double filteredIntensity{0.0};
		double filteredSnr{0.0};
		double range{0.0}; ///< Range to target in meters
		/// Error in Range.  For example if the current range estimate is 185200+/-27780 meters,
		/// Range is 185200 meters, RangeError is 27780 meters.
		double rangeError{0.0};
		double spatialExtent{0.0};		 ///< angular extent of the target in radians
		double trackQuality{0.0};		 ///< 0 to 1
		double clutter{0.0};
		std::chrono::nanoseconds age{0}; ///< Age of track (nanoseconds) since first detection
		IrstTrackState state{IrstTrackState::Idle};
		IrstTrackMode mode{IrstTrackMode::Idle};
	};
} // namespace ams::iface::irmel
