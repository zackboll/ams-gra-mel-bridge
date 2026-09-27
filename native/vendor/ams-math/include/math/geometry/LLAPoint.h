#pragma once

#include <cstddef> // size_t
#include <stdexcept>

/**
 *   @struct Holds a latitude/longitude/altitude tuple and exposes them via
 *   an API similar to the UCI Point2D_Type, etc.
 */
struct LLAPoint
{
	/**
	 *   @brief This method is the constructor.
	 */
	LLAPoint() = default;

	/**
	 *   @brief This method is the constructor.
	 *   @param[in] lat : the latitude of the point.
	 *   @param[in] lon : the longitude of the point.
	 *   @param[in] alt : the altitude of the point.
	 */
	LLAPoint(double lat, double lon, double alt)
	{
		lla[0] = lat;
		lla[1] = lon;
		lla[2] = alt;
	}

	double lla[3] = {0.0, 0.0, 0.0};

	/**
	 *   @brief returns the latitude of this point.
	 *   @return the latitude.
	 */
	[[nodiscard]] double getLatitude() const
	{
		return lla[0];
	}

	/**
	 *   @brief returns the longitude of this point.
	 *   @return the longitude.
	 */
	[[nodiscard]] double getLongitude() const
	{
		return lla[1];
	}

	/**
	 *   @brief returns the altitude of this point.
	 *   @return the altitude.
	 */
	[[nodiscard]] double getAltitude() const
	{
		return lla[2];
	}

	/**
	 *   @brief sets the latitude of this point.
	 *   @param[in] x : the desired latitude.
	 */
	void setLatitude(double x)
	{
		lla[0] = x;
	}

	/**
	 *   @brief sets the longitude of this point.
	 *   @param[in] x : the desired longitude.
	 */
	void setLongitude(double x)
	{
		lla[1] = x;
	}

	/**
	 *   @brief sets the altitude of this point.
	 *   @param[in] x : the desired altitude.
	 */
	void setAltitude(double x)
	{
		lla[2] = x;
	}

	/**
	 *   @brief does this point have an altitude?
	 *   @return a boolean whether or not this point has an altitude.
	 */
	[[nodiscard]] bool hasAltitude() const
	{
		return true;
	}

	/**
	 *   @brief enables the altitude attribute of this point.
	 *   @note this method does nothing currently.
	 */
	void enableAltitude()
	{
	} // do nothing

	double& operator[](size_t i)
	{
		if (i >= 3) throw std::out_of_range("Index out of bounds");
		return lla[i];
	}
	const double& operator[](size_t i) const
	{
		if (i >= 3) throw std::out_of_range("Index out of bounds");
		return lla[i];
	}

	/**
	 *   @brief Factory method for when 'other' definitely has an altitude present
	 *   @param[in] other : the point to convert.
	 *   @return the converted point.
	 */
	template <typename T>
	static LLAPoint fromLLA(const T& other)
	{
		return {other.getLatitude(), other.getLongitude(), other.getAltitude()};
	}

	/**
	 *   @brief Factory method for when 'other' has no altitude
	 *   @param[in] other : the point to convert.
	 *   @return the converted point.
	 */
	template <typename T>
	static LLAPoint fromLatLon(const T& other)
	{
		return {other.getLatitude(), other.getLongitude(), 0.0};
	}

	/**
	 *   @brief converts a LLA_Point_T to another type of point
	 *   @param[out] other : the other point to be filled out.
	 */
	template <typename T>
	void copyTo(T& other)
	{
		other.setLatitude(getLatitude());
		other.setLongitude(getLongitude());
		other.setAltitude(getAltitude());
	}
};
