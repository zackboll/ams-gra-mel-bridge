#pragma once

#include <cstdint>
#include <vector>
#include <math/units/UTCTime.h>

namespace ams::iface::rfmel
{
	/// @brief Indicates the threshold reference type for a pulse detection
	/// @RequiredIfOEMPulseDetection This class provides data definition in support of
	/// the Pulse Detection OEM Function and must be included as-is in all RF MEL implementations
	/// if the associated MFA supports Pulse Detection.
	enum class PulseDetectionThresholdReference : uint8_t
	{
		DBQ,				///< Declare detection based on dbq value
		DB_ABOVE_NOISE,		///< Declare detection when DB exceeds thermal noise
		DB_BELOW_SATURATION ///< Declare detection when DB is below receiver saturation
	};

	/// @brief Defines a pulse detection threshold with leading and trailing edges (in Db)
	/// @RequiredIfOEMPulseDetection This class provides data definition in support of
	/// the Pulse Detection OEM Function and must be included as-is in all RF MEL implementations
	/// if the associated MFA supports Pulse Detection.
	class PulseDetectionThreshold
	{
	public:
		PulseDetectionThreshold() = default;
		~PulseDetectionThreshold() = default;
		PulseDetectionThreshold(const PulseDetectionThreshold&) = default;			  // copy constructor
		PulseDetectionThreshold(PulseDetectionThreshold&&) = default;				  // move constructor
		PulseDetectionThreshold& operator=(const PulseDetectionThreshold&) = default; // copy operator
		PulseDetectionThreshold& operator=(PulseDetectionThreshold&&) = default;	  // move operator

		[[nodiscard]] auto getleadingEdgeDb() const
		{
			return this->leadingEdgeDb;
		}

		void setleadingEdgeDb(const double leadingEdgeDb_in)
		{
			this->leadingEdgeDb = leadingEdgeDb_in;
		}
		[[nodiscard]] auto getTrailingEdgeDb() const
		{
			return this->trailingEdgeDb;
		}

		void setTrailingEdgeDb(const double trailingEdgeDb_in)
		{
			this->trailingEdgeDb = trailingEdgeDb_in;
		}

	private:
		double leadingEdgeDb{0.0};
		double trailingEdgeDb{0.0};
	};

	/// @brief Represents the M and N values for a pulse detection
	/// @RequiredIfOEMPulseDetection This class provides data definition in support of
	/// the Pulse Detection OEM Function and must be included as-is in all RF MEL implementations
	/// if the associated MFA supports Pulse Detection.
	class PulseDetectionMofN
	{
	public:
		PulseDetectionMofN() = default;
		~PulseDetectionMofN() = default;
		PulseDetectionMofN(const PulseDetectionMofN&) = default;			// copy constructor
		PulseDetectionMofN(PulseDetectionMofN&&) = default;					// move constructor
		PulseDetectionMofN& operator=(const PulseDetectionMofN&) = default; // copy operator
		PulseDetectionMofN& operator=(PulseDetectionMofN&&) = default;		// move operator

		[[nodiscard]] auto getm() const
		{
			return this->m;
		}

		void setm(const uint8_t m_in)
		{
			this->m = m_in;
		}

		[[nodiscard]] auto getn() const
		{
			return this->n;
		}

		void setn(const uint8_t n_in)
		{
			this->n = n_in;
		}

	private:
		uint8_t m{0};
		uint8_t n{0};
	};

	/// @brief Indicates the Pulse Detection Time Tag Amplitude Threshold
	/// @RequiredIfOEMPulseDetection This class provides data definition in support of
	/// the Pulse Detection OEM Function and must be included as-is in all RF MEL implementations
	/// if the associated MFA supports Pulse Detection.
	enum class PulseDetectionTimeTagAmplitudeThreshold : uint8_t
	{
		TIMETAG_50_PERCENT,
		TIMETAG_90_PERCENT
	};

	/// @brief Contains the settings relevant to pulse detection for a job event
	/// @RequiredIfOEMPulseDetection This class provides data definition in support of
	/// the Pulse Detection OEM Function and must be included as-is in all RF MEL implementations
	/// if the associated MFA supports Pulse Detection.
	class PulseDetectionSettings
	{
	public:
		PulseDetectionSettings() = default;
		~PulseDetectionSettings() = default;
		PulseDetectionSettings(const PulseDetectionSettings&) = default;			// copy constructor
		PulseDetectionSettings(PulseDetectionSettings&&) = default;					// move constructor
		PulseDetectionSettings& operator=(const PulseDetectionSettings&) = default; // copy operator
		PulseDetectionSettings& operator=(PulseDetectionSettings&&) = default;		// move operator

		[[nodiscard]] PulseDetectionThresholdReference getReference() const
		{
			return this->reference;
		}
		void setReference(const PulseDetectionThresholdReference& reference_in)
		{
			this->reference = reference_in;
		}

		[[nodiscard]] PulseDetectionMofN getLeadingEdgeMofN() const
		{
			return this->leadingEdgeMofN;
		}
		void setLeadingEdgeMofN(const PulseDetectionMofN& leadingEdgeMofN_in)
		{
			this->leadingEdgeMofN = leadingEdgeMofN_in;
		}

		[[nodiscard]] PulseDetectionMofN getTrailingEdgeMofN() const
		{
			return this->trailingEdgeMofN;
		}
		void setTrailingEdgeMofN(const PulseDetectionMofN& trailingEdgeMofN_in)
		{
			this->trailingEdgeMofN = trailingEdgeMofN_in;
		}

		[[nodiscard]] ams::util::math::Femtoseconds getMinPulseWidth() const
		{
			return this->minPulseWidth;
		}
		void setMinPulseWidth(const ams::util::math::Femtoseconds& minPulseWidth_in)
		{
			this->minPulseWidth = minPulseWidth_in;
		}

		[[nodiscard]] PulseDetectionTimeTagAmplitudeThreshold getTimetagAmplitudeThreshold() const
		{
			return this->timetagAmplitudeThreshold;
		}
		void setTimetagAmplitudeThreshold(const PulseDetectionTimeTagAmplitudeThreshold& timetagAmplitudeThreshold_in)
		{
			this->timetagAmplitudeThreshold = timetagAmplitudeThreshold_in;
		}

		[[nodiscard]] std::vector<PulseDetectionThreshold> getThresholds() const
		{
			return this->thresholds;
		}
		void setThresholds(const std::vector<PulseDetectionThreshold>& thresholds_in)
		{
			this->thresholds = thresholds_in;
		}

	private:
		PulseDetectionThresholdReference reference{};
		PulseDetectionMofN leadingEdgeMofN{};
		PulseDetectionMofN trailingEdgeMofN{};
		ams::util::math::Femtoseconds minPulseWidth{};
		PulseDetectionTimeTagAmplitudeThreshold timetagAmplitudeThreshold{};
		std::vector<PulseDetectionThreshold> thresholds{};
	};
} // namespace ams::iface::rfmel
