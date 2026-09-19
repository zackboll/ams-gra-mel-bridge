//===============================================================================
/// @file  TrackDataUpdate.h
/// @brief This file includes the data definition of the updated track information.
//===============================================================================
#pragma once

#include <irmel/library/irmel-types/CommonIR_MEL.h>
#include <irmel/library/track/TrackStatus.h>
#include <math/geometry/RangeAzEl.h>
#include <chrono>

#include <boost/numeric/ublas/io.hpp>
#include <boost/numeric/ublas/triangular.hpp>
#include <cstdint>

using namespace boost::numeric::ublas;

namespace ams::iface::irmel
{
	/// @class TrackDataUpdate
	/// @brief This class holds the track data update data type to be used to pass track information unsolicited from the service to the MFA.
	/// @RequiredIfTrackUpdate
	class TrackDataUpdate
	{
	public:
		TrackDataUpdate() = default;
		~TrackDataUpdate() = default;
		TrackDataUpdate(const TrackDataUpdate&) = default;
		TrackDataUpdate(TrackDataUpdate&&) = default;
		TrackDataUpdate& operator=(const TrackDataUpdate&) = default;
		TrackDataUpdate& operator=(TrackDataUpdate&&) = default;

		[[nodiscard]] std::uint32_t getPlatformId() const
		{
			return this->platformId;
		}
		void setPlatformId(std::uint32_t platformId_in)
		{
			this->platformId = platformId_in;
		}
		[[nodiscard]] ams::iface::mel::UCI_ID getCapabilityUUID() const
		{
			return this->capabilityUUID;
		}
		void setCapabilityUUID(ams::iface::mel::UCI_ID capabilityUUID_in)
		{
			this->capabilityUUID = capabilityUUID_in;
		}
		[[nodiscard]] ams::iface::mel::UCI_ID getActivityUUID() const
		{
			return this->activityUUID;
		}
		void setActivityUUID(ams::iface::mel::UCI_ID activityUUID_in)
		{
			this->activityUUID = activityUUID_in;
		}
		[[nodiscard]] std::uint32_t getTrackId() const
		{
			return this->trackId;
		}
		void setTrackId(std::uint32_t trackId_in)
		{
			this->trackId = trackId_in;
		}
		[[nodiscard]] ams::iface::mel::UCI_ID getEntityUUID() const
		{
			return this->entityUUID;
		}
		void setEntityUUID(ams::iface::mel::UCI_ID entityUUID_in)
		{
			this->entityUUID = entityUUID_in;
		}
		[[nodiscard]] TrackStatus getTrackStatus() const
		{
			return this->trackStatus;
		}
		void setTrackStatus(TrackStatus trackStatus_in)
		{
			this->trackStatus = trackStatus_in;
		}
		[[nodiscard]] double getTimeOfValidity() const
		{
			return this->timeOfValidity;
		}
		void setTimeOfValidity(double timeOfValidity_in)
		{
			this->timeOfValidity = timeOfValidity_in;
		}
		[[nodiscard]] double getTimeOfLastUpdate() const
		{
			return this->timeOfLastUpdate;
		}
		void setTimeOfLastUpdate(double timeOfLastUpdate_in)
		{
			this->timeOfLastUpdate = timeOfLastUpdate_in;
		}
		[[nodiscard]] ams::iface::mel::Directional getTrackPosition() const
		{
			return this->trackPosition;
		}
		void setTrackPosition(ams::iface::mel::Directional trackPosition_in)
		{
			this->trackPosition = trackPosition_in;
		}
		[[nodiscard]] ams::iface::mel::Directional getTrackVelocity() const
		{
			return this->trackVelocity;
		}
		void setTrackVelocity(ams::iface::mel::Directional trackVelocity_in)
		{
			this->trackVelocity = trackVelocity_in;
		}
		[[nodiscard]] double getTrackCovarianceXX() const
		{
			return this->trackCovarianceXX;
		}
		void setTrackCovarianceXX(const double trackCovarianceXX_in)
		{
			this->trackCovarianceXX = trackCovarianceXX_in;
		}
		[[nodiscard]] double getTrackCovarianceXY() const
		{
			return this->trackCovarianceXY;
		}
		void setTrackCovarianceXY(const double trackCovarianceXY_in)
		{
			this->trackCovarianceXY = trackCovarianceXY_in;
		}
		[[nodiscard]] double getTrackCovarianceXZ() const
		{
			return this->trackCovarianceXZ;
		}
		void setTrackCovarianceXZ(const double trackCovarianceXZ_in)
		{
			this->trackCovarianceXZ = trackCovarianceXZ_in;
		}
		[[nodiscard]] double getTrackCovarianceXVx() const
		{
			return this->trackCovarianceXVx;
		}
		void setTrackCovarianceXVx(const double trackCovarianceXVx_in)
		{
			this->trackCovarianceXVx = trackCovarianceXVx_in;
		}
		[[nodiscard]] double getTrackCovarianceXVy() const
		{
			return this->trackCovarianceXVy;
		}
		void setTrackCovarianceXVy(const double trackCovarianceXVy_in)
		{
			this->trackCovarianceXVy = trackCovarianceXVy_in;
		}
		[[nodiscard]] double getTrackCovarianceXVz() const
		{
			return this->trackCovarianceXVz;
		}
		void setTrackCovarianceXVz(const double trackCovarianceXVz_in)
		{
			this->trackCovarianceXVz = trackCovarianceXVz_in;
		}
		[[nodiscard]] double getTrackCovarianceYY() const
		{
			return this->trackCovarianceYY;
		}
		void setTrackCovarianceYY(const double trackCovarianceYY_in)
		{
			this->trackCovarianceYY = trackCovarianceYY_in;
		}
		[[nodiscard]] double getTrackCovarianceYZ() const
		{
			return this->trackCovarianceYZ;
		}
		void setTrackCovarianceYZ(const double trackCovarianceYZ_in)
		{
			this->trackCovarianceYZ = trackCovarianceYZ_in;
		}
		[[nodiscard]] double getTrackCovarianceYVx() const
		{
			return this->trackCovarianceYVx;
		}
		void setTrackCovarianceYVx(const double trackCovarianceYVx_in)
		{
			this->trackCovarianceYVx = trackCovarianceYVx_in;
		}
		[[nodiscard]] double getTrackCovarianceYVy() const
		{
			return this->trackCovarianceYVy;
		}
		void setTrackCovarianceYVy(const double trackCovarianceYVy_in)
		{
			this->trackCovarianceYVy = trackCovarianceYVy_in;
		}
		[[nodiscard]] double getTrackCovarianceYVz() const
		{
			return this->trackCovarianceYVz;
		}
		void setTrackCovarianceYVz(const double trackCovarianceYVz_in)
		{
			this->trackCovarianceYVz = trackCovarianceYVz_in;
		}
		[[nodiscard]] double getTrackCovarianceZZ() const
		{
			return this->trackCovarianceZZ;
		}
		void setTrackCovarianceZZ(const double trackCovarianceZZ_in)
		{
			this->trackCovarianceZZ = trackCovarianceZZ_in;
		}
		[[nodiscard]] double getTrackCovarianceZVx() const
		{
			return this->trackCovarianceZVx;
		}
		void setTrackCovarianceZVx(const double trackCovarianceZVx_in)
		{
			this->trackCovarianceZVx = trackCovarianceZVx_in;
		}
		[[nodiscard]] double getTrackCovarianceZVy() const
		{
			return this->trackCovarianceZVy;
		}
		void setTrackCovarianceZVy(const double trackCovarianceZVy_in)
		{
			this->trackCovarianceZVy = trackCovarianceZVy_in;
		}
		[[nodiscard]] double getTrackCovarianceZVz() const
		{
			return this->trackCovarianceZVz;
		}
		void setTrackCovarianceZVz(const double trackCovarianceZVz_in)
		{
			this->trackCovarianceZVz = trackCovarianceZVz_in;
		}
		[[nodiscard]] double getTrackCovarianceVxVx() const
		{
			return this->trackCovarianceVxVx;
		}
		void setTrackCovarianceVxVx(const double trackCovarianceVxVx_in)
		{
			this->trackCovarianceVxVx = trackCovarianceVxVx_in;
		}
		[[nodiscard]] double getTrackCovarianceVxVy() const
		{
			return this->trackCovarianceVxVy;
		}
		void setTrackCovarianceVxVy(const double trackCovarianceVxVy_in)
		{
			this->trackCovarianceVxVy = trackCovarianceVxVy_in;
		}
		[[nodiscard]] double getTrackCovarianceVxVz() const
		{
			return this->trackCovarianceVxVz;
		}
		void setTrackCovarianceVxVz(const double trackCovarianceVxVz_in)
		{
			this->trackCovarianceVxVz = trackCovarianceVxVz_in;
		}
		[[nodiscard]] double getTrackCovarianceVyVy() const
		{
			return this->trackCovarianceVyVy;
		}
		void setTrackCovarianceVyVy(const double trackCovarianceVyVy_in)
		{
			this->trackCovarianceVyVy = trackCovarianceVyVy_in;
		}
		[[nodiscard]] double getTrackCovarianceVyVz() const
		{
			return this->trackCovarianceVyVz;
		}
		void setTrackCovarianceVyVz(const double trackCovarianceVyVz_in)
		{
			this->trackCovarianceVyVz = trackCovarianceVyVz_in;
		}
		[[nodiscard]] double getTrackCovarianceVzVz() const
		{
			return this->trackCovarianceVzVz;
		}
		void setTrackCovarianceVzVz(const double trackCovarianceVzVz_in)
		{
			this->trackCovarianceVzVz = trackCovarianceVzVz_in;
		}
		[[nodiscard]] double getManeuverProbability() const
		{
			return this->maneuverProbability;
		}
		void setManeuverProbability(const double maneuverProbability_in)
		{
			this->maneuverProbability = maneuverProbability_in;
		}
		[[nodiscard]] double getTrackQuality() const
		{
			return this->trackQuality;
		}
		void setTrackQuality(const double trackQuality_in)
		{
			this->trackQuality = trackQuality_in;
		}

	private:
		uint32_t platformId{0};						  ///< Integer ID of the platform
		ams::iface::mel::UCI_ID capabilityUUID;		  ///< UUID of the capability
		ams::iface::mel::UCI_ID activityUUID;		  ///< UUID of the activity
		uint32_t trackId{0};						  ///< Integer ID of the track
		ams::iface::mel::UCI_ID entityUUID;			  ///< UUID of the entity
		TrackStatus trackStatus{TrackStatus::Create}; ///< Status of the track
		double timeOfValidity{0.0};					  ///< Time of validity in epoch seconds
		double timeOfLastUpdate{0.0};				  ///< Time of last update in epoch seconds
		ams::iface::mel::Directional trackPosition;	  ///< track position in ECEF coordinates
		ams::iface::mel::Directional trackVelocity;	  ///< track velocity in ECEF coordinates
		double trackCovarianceXX{0.0};
		double trackCovarianceXY{0.0};
		double trackCovarianceXZ{0.0};
		double trackCovarianceXVx{0.0};
		double trackCovarianceXVy{0.0};
		double trackCovarianceXVz{0.0};
		double trackCovarianceYY{0.0};
		double trackCovarianceYZ{0.0};
		double trackCovarianceYVx{0.0};
		double trackCovarianceYVy{0.0};
		double trackCovarianceYVz{0.0};
		double trackCovarianceZZ{0.0};
		double trackCovarianceZVx{0.0};
		double trackCovarianceZVy{0.0};
		double trackCovarianceZVz{0.0};
		double trackCovarianceVxVx{0.0};
		double trackCovarianceVxVy{0.0};
		double trackCovarianceVxVz{0.0};
		double trackCovarianceVyVy{0.0};
		double trackCovarianceVyVz{0.0};
		double trackCovarianceVzVz{0.0};
		double maneuverProbability{0.0}; ///< Probability of maneuver, 0.0 - 1.0  0.0=unlikely maneuvering; 1.0=likely maneuvering
		double trackQuality{0.0};		 ///< Provides information about the quality of a track. Allowed values from 0-15.
	};
} // end namespace ams::iface::irmel
