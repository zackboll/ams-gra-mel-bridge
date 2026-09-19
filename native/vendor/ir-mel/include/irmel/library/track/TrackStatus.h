//===============================================================================
/// @file  TrackStatus.h
/// @brief This file includes the TrackStatus class definition.

#pragma once

#include <cstdint>

/// @namespace ams::iface::irmel
/// This namespace contains all of the GRA functionality for the IR MEL
namespace ams::iface::irmel
{
	/// @enum TrackStatus
	/// @brief This enum is used to indicate the status of the track
	/// @RequiredIfTrackUpdate
	enum class TrackStatus : uint32_t
	{
		Create,	 ///< Track created
		Update,	 ///< Track updated
		Predict, ///< Track predicted
		Delete	 ///< Track deleted
	};
} // namespace ams::iface::irmel
