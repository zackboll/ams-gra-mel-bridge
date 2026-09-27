#pragma once

#include <rfmel/rfmeltypes/RFMELTypes.h>
#include <complex>
#include <map>
#include <vector>

namespace ams::iface::rfmel
{
	/// @brief The base class of all Weights class specializations. Each RF MEL implementation is allowed to provide a
	/// different means of accomplishing beam tapering, etc. using a class that inherits from Weights.
	/// @RequiredIfBeamTaperingWeights This class provides data definition in support of required RF MEL functionality
	/// for all Weights class specializations and must be included as-is in all RF MEL implementations if the associated MFA
	/// supports controlling beam tapering of job events via setting aperture weights.
	class Weights
	{
	public:
		Weights() = default;
		~Weights() = default;
		Weights(const Weights&) = default;
		Weights(Weights&&) = default;
		Weights& operator=(const Weights&) = default;
		Weights& operator=(Weights&&) = default;
	};

	/// @note TwoDimWeights is a data structure providing a normative example of how a MEL might allow
	/// a series of complex weights to be directly applied to the elements within a one-
	/// or two-dimensional Virtual Aperture.
	/// This format may or may not be supported by a given MEL implementation; check the
	/// documentation.
	/// A one-dimensional vector of weights which maps to a 2D region of an aperture face.
	/// Weights are stored in row-major order; neighboring entries in the vector map to
	/// elements within the same physical row (except at the boundaries between rows).
	/// In the event the face is non-rectangular, the rows and columns are assumed to form
	/// a bounding rectangle around the elements, and the vector will contain unused entries
	/// mapping to the areas within the rectangle but outside the face.
	/// @brief TwoDimWeights class
	/// @Optional Implementation optional if desired by the MFA Provider
	class TwoDimWeights : public Weights
	{
	public:
		TwoDimWeights() = default;
		~TwoDimWeights() = default;
		TwoDimWeights(const TwoDimWeights&) = delete;
		TwoDimWeights(TwoDimWeights&&) = delete;
		TwoDimWeights& operator=(const TwoDimWeights&) = delete;
		TwoDimWeights& operator=(TwoDimWeights&&) = delete;

		/// @brief Gets rows.
		/// @Optional Implementation optional if desired by the MFA Provider
		[[nodiscard]] auto getRows() const
		{
			return nRows;
		}

		/// @brief Gets columns.
		/// @Optional Implementation optional if desired by the MFA Provider
		[[nodiscard]] auto getColumns() const
		{
			return nColumns;
		}

		/// @brief Gets weights.
		/// @Optional Implementation optional if desired by the MFA Provider
		[[nodiscard]] const auto& getWeights() const
		{
			return weights;
		}

		/// @brief Gets weights.
		/// @Optional Implementation optional if desired by the MFA Provider
		[[nodiscard]] auto& getWeights()
		{
			return weights;
		}

		/// @brief Sets rows.
		/// @Optional Implementation optional if desired by the MFA Provider
		void setRows(const size_t rows)
		{
			nRows = rows;
		}

		/// @brief Sets columns.
		/// @Optional Implementation optional if desired by the MFA Provider
		void setColumns(const size_t columns)
		{
			nColumns = columns;
		}

		/// @brief Sets weights.
		/// @Optional Implementation optional if desired by the MFA Provider
		void setWeights(const std::vector<std::complex<double>>& weights_in)
		{
			this->weights = weights_in;
		}

	private:
		size_t nRows{0};
		size_t nColumns{0};
		std::vector<std::complex<double>> weights;
	};

} // namespace ams::iface::rfmel
