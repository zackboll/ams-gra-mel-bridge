#pragma once

#include <utility>

#include <boost/numeric/ublas/blas.hpp>
#include <boost/numeric/ublas/matrix.hpp>

#include "LLAPoint.h"
#include "Quaternion.h"
#include "RangeAzEl.h"

// 		it might be best to remove the "using"
using Matrix4x6 = boost::numeric::ublas::c_matrix<double, 4, 6>;

using Matrix6d = boost::numeric::ublas::c_matrix<double, 6, 6>;
using Matrix4d = boost::numeric::ublas::c_matrix<double, 4, 4>;
using Matrix3d = boost::numeric::ublas::c_matrix<double, 3, 3>;
using Vector3d = boost::numeric::ublas::c_vector<double, 3>;

using NedVelocity = Vector3d;
using EcefPoint = Vector3d;
using EcefVelocity = Vector3d;

namespace ams::util::math::geometry
{
	/**
	 *   @brief squares the passed in double.
	 *   @param[in] x : the double to square.
	 *   @return the square of the passed-in double.
	 */
	inline double sq(double x)
	{
		return x * x;
	}

	/**
	 *   @brief the radius of Earth at the given latitude.
	 *   @param[in] latitude : the inquiring latitude.
	 *   @return the radius of the Earth at the gievn latitude in radians.
	 */
	double earthRadius(double latitude);

	/**
	 *   @brief optimized version of earthRadius(double latitude)
	 *   @param[in] sinLat : the sine of a given latitude.
	 *   @param[in] cosLat : the cosine of a given latitude.
	 *   @return the radius of the Earth at the gievn latitude in radians.
	 */
	double earthRadius(double sinLat, double cosLat); // optimized version of the above

	/**
	 *   @brief converts Earth-Centered Earth-Fixed coordinates to a local rotation matrix
	 *   @param[in] lat : an ECEF latitude.
	 *   @param[in] lon : an ECEF longitude.
	 *   @return a local rotation matrix.
	 */
	Matrix3d ecef_to_llf_rot(double lat, double lon);

	/**
	 *   @brief converts Earth-Centered Earth-Fixed coordinates to a NED rotation matrix
	 *   @param[in] lat : an ECEF latitude.
	 *   @param[in] lon : an ECEF longitude.
	 *   @return a NED rotation matrix.
	 */
	Matrix3d ecef_to_ned_rot(double lat, double lon);

	/**
	 *   @brief converts geodetic coordinates to geocentric.
	 *   @param[in] lat : a geodetic latitude.
	 *   @param[in] lon : a geodetic longitude.
	 *   @param[in] alt : a geodetic altitude.
	 *   @return a geocentric coordinate.
	 */
	EcefPoint lla2ecef(double lat, double lon, double alt);

	/**
	 *   @brief converts geodetic coordinates to geocentric.
	 *   @param[in] lla : a geodetic coordinate.
	 *   @return a geocentric coordinate.
	 */
	inline EcefPoint lla2ecef(const LLAPoint &lla)
	{
		return lla2ecef(lla.getLatitude(), lla.getLongitude(), lla.getAltitude());
	}

	/**
	 *   @brief converts geocentric coordinates to geodetic.
	 *   @param[in] ecef : a geocentric coordinate.
	 *   @param[out] lat : a converted geodetic latitude.
	 *   @param[out] lon : a converted geodetic longitude.
	 *   @param[out] alt : a converted geodetic altitude.
	 */
	void ecef2lla(const EcefPoint &ecef, double &lat, double &lon, double &alt);

	/**
	 *   @brief converts geocentric coordinates to geodetic.
	 *   @param[in] ecef : a geocentric coordinate.
	 *   @return a converted geodetic coordinate.
	 */
	inline LLAPoint ecef2lla(const EcefPoint &ecef)
	{
		LLAPoint lla;
		ecef2lla(ecef, lla[0], lla[1], lla[2]);
		return lla;
	}

	/**
	 *   @brief converts geocentric coordinates to NED.
	 *   @param[in] lat : the latitude of the origin.
	 *   @param[in] lon : the longitude of the origin.
	 *   @param[in] alt : the altitude of the origin.
	 *   @param[in] pointEcef : a geocentric coordinate.
	 *   @return a converted NED coordinate.
	 */
	Vector3d ecef2ned(double lat, double lon, double alt, const EcefPoint &pointEcef);

	/**
	 *   @brief converts NED to Range, Az. El.
	 *   @param[in] ned : NED Coordinate
	 *   @return a Range, Az [0, 360), El  [-90, 90),
	 */
	RangeAzEl ned2rae(const Vector3d &ned);

	/**
	 *   @brief converts Range, Az. El to NED
	 *   @param[in] rae : Range, Az, El
	 *   @return a NED Coordinate
	 */
	Vector3d rae2ned(const RangeAzEl &rae);

	/**
	 *   @brief converts geocentric coordinates to NED.
	 *   @param[in] origin : the origin.
	 *   @param[in] pointEcef : a geocentric coordinate.
	 *   @return a converted NED coordinate.
	 */
	inline Vector3d ecef2ned(const LLAPoint &origin, const EcefPoint &pointEcef)
	{
		return ecef2ned(origin[0], origin[1], origin[2], pointEcef);
	}

	/**
	 *   @brief converts geocentric velocity to NED velocity.
	 *   @param[in] refLLA : the reference point.
	 *   @param[in] ecefVel : the geocentric velocity.
	 *   @return a converted NED velocity.
	 */
	NedVelocity ecefVel2NedVel(const LLAPoint &refLLA, const EcefPoint &ecefVel);

	/**
	 *   @brief returns a rotation matrix to orient to the local NED plane.
	 *   @param[in] lat : the NED latitude.
	 *   @param[in] lon : the NED longitude.
	 *   @return a rotation matrix to oriented to the local NED plane.
	 */
	Matrix3d phiRotationMatrixNED(double lat, double lon);

	/**
	 *   @brief returns a rotation matrix to orient to the local ENU plane.
	 *   @param[in] lat : the ENU latitude.
	 *   @param[in] lon : the ENU longitude.
	 *   @return a rotation matrix to oriented to the local ENU plane.
	 */
	Matrix3d phiRotationMatrixENU(double lat, double lon);

	/**
	 *   @brief returns a rotation matrix to perform heading/pitch/roll movement:
	 *   @param[in] roll : the roll to perform.
	 *   @param[in] pitch : the pitch to perform.
	 *   @param[in] heading : the heading to perform.
	 *   @return a rotation matrix to oriented to the given heading/pitch/roll.
	 */
	Matrix3d getEulerRPH(double roll, double pitch, double heading);

	/**
	 *   @brief returns a matrix rotated around the x-axis.
	 *   @param[in] phi : the rotation to perform in radians.
	 *   @return a matrix rotated around the x-axis.
	 */
	Matrix3d rotX(double phi);

	/**
	 *   @brief returns a matrix rotated around the y-axis.
	 *   @param[in] theta : the rotation to perform in radians.
	 *   @return a matrix rotated around the y-axis.
	 */
	Matrix3d rotY(double theta);

	/**
	 *   @brief returns a matrix rotated around the z-axis.
	 *   @param[in] psi : the rotation to perform in radians.
	 *   @return a matrix rotated around the z-axis.
	 */
	Matrix3d rotZ(double psi);

	/**
	 *   @brief returns the NED pitch from the given Lat/Lon and DIS orientation.
	 *   @param[in] lat : the latitude.
	 *   @param[in] lon : the longitude.
	 *   @param[in] psi : the yaw in radians.
	 *   @param[in] theta : the pitch in radians.
	 *   @return the NED pitch.
	 */
	double getTaitBryanPitchFromEuler(double lat, double lon, double psi, double theta);

	/**
	 *   @brief returns the NED yaw from the given Lat/Lon and DIS orientation.
	 *   @param[in] lat : the latitude.
	 *   @param[in] lon : the longitude.
	 *   @param[in] psi : the yaw in radians.
	 *   @param[in] theta : the pitch in radians.
	 *   @return the NED yaw.
	 */
	double getTaitBryanYawFromEuler(double lat, double lon, double psi, double theta);

	/**
	 *   @brief returns the NED roll from the given Lat/Lon and DIS orientation.
	 *   @param[in] lat : the latitude.
	 *   @param[in] lon : the longitude.
	 *   @param[in] psi : the yaw in radians.
	 *   @param[in] theta : the pitch in radians.
	 *   @param[in] phi : the roll in radians.
	 *   @return the NED roll.
	 */
	double getTaitBryanRollFromEuler(double lat, double lon, double psi, double theta, double phi);

	/**
	 *   @brief returns the Euler yaw from the given Lat/Lon and NED angle.
	 *   @param[in] lat : the latitude.
	 *   @param[in] lon : the longitude.
	 *   @param[in] yaw : the yaw in radians.
	 *   @param[in] pitch : the pitch in radians.
	 *   @return the Euler yaw.
	 */
	double getPsiYawFromTaitBryanAngles(double lat, double lon, double yaw, double pitch);

	/**
	 *   @brief returns the Euler pitch from the given Lat/Lon and NED angle.
	 *   @param[in] lat : the latitude.
	 *   @param[in] yaw : the yaw in radians.
	 *   @param[in] pitch : the pitch in radians.
	 *   @return the Euler pitch.
	 */
	double getThetaPitchFromTaitBryanAngles(double lat, double yaw, double pitch);

	/**
	 *   @brief returns the Euler roll from the given Lat/Lon and NED angle.
	 *   @param[in] lat : the latitude.
	 *   @param[in] yaw : the yaw in radians.
	 *   @param[in] pitch : the pitch in radians.
	 *   @param[in] roll : the roll in radians.
	 *   @return the Euler roll.
	 */
	double getPhiRollFromTaitBryanAngles(double lat, double yaw, double pitch, double roll);

	struct TaitBryanAngles
	{
		double yaw;
		double pitch;
		double roll;
	};

	/**
	 *   @brief returns the NED angles given DIS Lat/Lon and orientation.
	 *   @param[in] lat : the latitude.
	 *   @param[in] lon : the longitude.
	 *   @param[in] disPhiRoll : the roll in radians.
	 *   @param[in] disThetaPitch : the pitch in radians.
	 *   @param[in] disPsiYaw : the yaw in radians.
	 *   @return the NED angles
	 */
	TaitBryanAngles determineTaitBryanAngles(double lat, double lon, double disPhiRoll, double disThetaPitch, double disPsiYaw);

	/**
	 *   @brief determines the distance between two points.
	 *   @param[in] a : the first point.
	 *   @param[in] b : the second point.
	 *   @return the distance between the two points.
	 */
	inline double distance(const Vector3d &a, const Vector3d &b)
	{
		return boost::numeric::ublas::norm_2(b - a);
	}

	/**
	 *   @brief Given an input lat/lon, range and bearing, return a new lat/lon at the desired R/B (at the same optional altitude)
	 *   @param[in] lat : the latitude.
	 *   @param[in] lon : the longitude.
	 *   @param[in] range : the range.
	 *   @param[in] bearing : the bearing.
	 *   @param[out] projLat : the projected latitude.
	 *   @param[out] projLon : the projected longitude.
	 */
	void projectRangeBearing(double lat, double lon, double range, double bearing, double &projLat, double &projLon);

	/**
	 *   @brief Given an input lat/lon, range and bearing, return a new lat/lon at the desired R/B (at the same optional altitude)
	 *   @param[in] orig : the original latitude and longitude.
	 *   @param[in] range : the range.
	 *   @param[in] bearing : the bearing.
	 *   @return the projected coordinates.
	 */
	inline LLAPoint projectRangeBearing(const LLAPoint &orig, double range, double bearing)
	{
		LLAPoint lla;
		projectRangeBearing(orig[0], orig[1], range, bearing, lla[0], lla[1]);
		lla[2] = orig[2];
		return lla;
	}

	/**
	 *   @brief given a pair of lat/lon points, compute the range/bearing between them
	 *   @param[in] lat1 : the latitude of the first point
	 *   @param[in] lon1 : the longitude of the first point
	 *   @param[in] lat2 : the latitude of the second point
	 *   @param[in] lon2 : the longitude of the second point
	 *   @param[out] range : the range between the two points.
	 *   @param[out] bearing : the bearing between the two points.
	 */
	void computeRangeBearing(double lat1, double lon1, double lat2, double lon2, double &range, double &bearing);

	/**
	 *   @brief given a pair of lat/lon points, compute the range/bearing between them
	 *   @param[in] pt1 : the coordinates of the first point.
	 *   @param[in] pt2 : the coordinates of the second point.
	 *   @return the range and bearing between the two points.
	 */
	inline std::pair<double, double> computeRangeBearing(const LLAPoint &pt1, const LLAPoint &pt2)
	{
		double range = 0, bearing = 0;
		computeRangeBearing(pt1[0], pt1[1], pt2[0], pt2[1], range, bearing);
		return {range, bearing};
	}

	/**
	 *   @brief Given a pair of lat/lon points and a pair of headings, compute aspect angle and lead angle
	 *   @param[in] latLonHead1 : a vector containing the latitude, longitude, and heading of the first point
	 *   @param[in] latLonHead2 : a vector containing the latitude, longitude, and heading of the second point
	 *   @param[out] range : the range between the two points.
	 *   @param[out] bearing : the bearing between the two points.
	 *   @param[out] aspectAngle : the angle between heading1 from point lat1,lon1 to the point lat2,lon2
	 *   @param[out] leadAngle : the same as aspect angle, except using heading 2 from lat2,lon2 to lat1,lon1
	 */
	void computeAspectAndLeadAngle(const Vector3d &latLonHead1, const Vector3d &latLonHead2, double &range, double &bearing, double &aspectAngle,
								   double &leadAngle);

	/**
	 *   @brief Given a pair of headings and a bearing, compute aspect angle and lead angle
	 *   @param[in] heading1 : the heading of the first point
	 *   @param[in] heading2 : the heading of the second point
	 *   @param[in] bearing : the bearing between the two points.
	 *   @param[out] aspectAngle : the angle between heading1 and bearing
	 *   @param[out] leadAngle : the same as aspect angle, except using heading2
	 */
	void computeAspectAndLeadAngle(double heading1, double heading2, double bearing, double &aspectAngle, double &leadAngle);

	/**
	 *   @brief utility function that normalizes an angle in radians to [-pi .. pi).
	 *   @param[in] rads : angle in radians.
	 *   @return the normalized angle in radians.
	 */
	double wrap_pi_pi(double rads);

	/**
	 *   @brief utility function that normalizes an angle in radians to [0 .. 2*pi).
	 *   @param[in] rads : angle in radians.
	 *   @return the normalized angle in radians.
	 */
	double wrap_0_2pi(double rads);

	/**
	 *   @brief utility function that normalizes an angle in radians to [0 .. pi).
	 *   @param[in] rads : angle in radians.
	 *   @return the normalized angle in radians.
	 */
	double wrap_0_pi(double rads);

	/**
	 *   @brief utility function that returns the magnitude of a given vector.
	 *   @param[in] x : the vector.
	 *   @return the magnitude of the vector.
	 */
	inline double magnitude(const Vector3d &x)
	{
		return boost::numeric::ublas::norm_2(x);
	}

	/**
	 *   @brief utility function that returns the distance between two points.
	 *   @param[in] pt1 : the first point.
	 *   @param[in] pt2 : the second point.
	 *   @return the distance between the two points.
	 */
	inline double dist(const Vector3d &pt1, const Vector3d &pt2)
	{
		return magnitude(pt2 - pt1);
	}

	/**
	 *   @brief utility function that converts an angle from degrees to radians.
	 *   @param[in] deg : an angle in degrees.
	 *   @returns the angle converted to radians.
	 */
	double deg_2_rad(double deg);

	/**
	 *   @brief returns the midpoint between two wrapped values, given the bounds of the circle
	 *   @param[in] minValue : lesser (leftmost/counterclockwise) point
	 *   @param[in] maxValue : greater (rightmost/clockwise) point
	 *   @param[in] minBound : the lower bound of the wrap
	 *   @param[in] maxBound : the upper bound of the wrap
	 *   @return the midpoint between a1 and a2
	 */
	double midpointWrapped(double minValue, double maxValue, double minBound, double maxBound);

	/**
	 *   @brief returns the midpoint between two angles over the domain 0 to 2Pi
	 *   @param[in] minAngle : counterclockwise (left) angle
	 *   @param[in] maxAngle : clockwise (right) angle
	 *   @return the midpoint between a1 and a2
	 */
	double angleMidpoint_0_2pi(double minAngle, double maxAngle);

	/**
	 *   @brief returns the midpoint between two angles over the domain -pi to pi
	 *   @param[in] minAngle : counterclockwise (left) angle
	 *   @param[in] maxAngle : clockwise (right) angle
	 *   @return the midpoint between a1 and a2
	 */
	double angleMidpoint_pi_pi(double minAngle, double maxAngle);

	/**
	 *   @brief returns the difference between two angles over the domain -pi to pi
	 *   @param[in] minAngle : counterclockwise (left) angle
	 *   @param[in] maxAngle : clockwise (right) angle
	 *   @return the difference between a1 and a2
	 */
	double angleDif_pi_pi(double minAngle, double maxAngle);
} // namespace ams::util::math::geometry

/**
 *   @brief takes an object with an overloaded [] operator and converts it to a Vector3d.
 *   @param[in] v : the object to convert.
 *   @return the converted vector.
 */
template <typename T>
Vector3d makeVector(const T &v)
{
	Vector3d out;
	for(int i = 0; i < 3; i++)
	{
		out[i] = v[i];
	}
	return out;
}

/**
 *   @brief takes a list with at least 3 values and converts it to a Vector3d.
 *   @param[in] v : the list to convert.
 *   @return the converted vector.
 */
inline Vector3d makeVector(const std::initializer_list<double> &v)
{
	Vector3d out;
	assert(v.size() >= 3);
	auto it = v.begin();
	for(size_t i = 0; i < 3; i++)
	{
		out[i] = *it++;
	}
	return out;
}
