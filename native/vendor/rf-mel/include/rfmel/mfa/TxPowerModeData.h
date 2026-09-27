#pragma once

#include <chrono>
#include <vector>

#include <rfmel/rfmeltypes/RFMELTypes.h>

namespace ams::iface::rfmel
{
	/// @brief The base class of MFA Tx Power Mode Data for a given Tx Power Mode ID.
	/// To prevent hardware damage, the Tx Power Mode ID is not to be used outside of
	/// of the listed characteristics.
	/// @RequiredIfTransmit This class provides data definition in support of required
	/// RF MEL functionality specific to transmit and must be included as-is in all RF
	/// MEL implementations if the associated MFA supports transmit.
	class TxPowerModeData
	{
	public:
		TxPowerModeData() = default;
		~TxPowerModeData() = default;
		TxPowerModeData(const TxPowerModeData&) = default;
		TxPowerModeData(TxPowerModeData&&) = default;
		TxPowerModeData& operator=(const TxPowerModeData&) = default;
		TxPowerModeData& operator=(TxPowerModeData&&) = default;

		/// @brief Gets the Tx power mode ID.
		[[nodiscard]] const auto getTxPowerModeID() const
		{
			return txPowerModeID;
		}

		/// @brief Returns if Linear Operation.
		[[nodiscard]] const auto getIsLinearOperation() const
		{
			return isLinearOperation;
		}

		/// @brief Gets Tx power level.
		[[nodiscard]] const auto getTxPowerLevel() const
		{
			return txPowerLevel;
		}

		/// @brief returns the [min,max] frequency supported by this Tx Power Mode.
		[[nodiscard]] const std::vector<ams::iface::rfmel::FrequencyRange>& getTxFrequencyRanges(ams::iface::rfmel::FaceID /*faceId*/) const
		{
			return txFrequencyRanges;
		}

		/// @brief Gets the Trasmit frequency ranges.
		[[nodiscard]] std::vector<ams::iface::rfmel::FrequencyRange>& getTxFrequencyRanges(ams::iface::rfmel::FaceID /*faceId*/) // non-const
		{
			return txFrequencyRanges;
		}

		/// @brief Get the Maximum Transmit duty factor.
		[[nodiscard]] auto getMaxTxDutyFactor() const
		{
			return this->maxTxDutyFactor;
		}

		/// @brief Get the Maximum Transmit pulse width.
		[[nodiscard]] auto getMaxTxPulseWidth() const
		{
			return this->maxTxPulseWidth;
		}

		/// @brief Get the Maximum Transmit attentuation.
		[[nodiscard]] auto getMaxTxAtten() const
		{
			return this->maxTxAtten;
		}

		/// @brief Get the Transmit attentuation commandable step size.
		[[nodiscard]] auto getTxAttenStepSize() const
		{
			return this->txAttenStepSize;
		}

		/// @brief Set the Transmit power mode identifier.
		void setTxPowerModeID(const TxPowerModeID txPowerModeID_in)
		{
			this->txPowerModeID = txPowerModeID_in;
		}

		/// @brief Set the Transmit operating mode.
		void setIsLinearOperation(const bool isLinearOperation_in)
		{
			this->isLinearOperation = isLinearOperation_in;
		}

		/// @brief Set the Transmit power level.
		void setTxPowerLevel(const TxPowerLevel txPowerLevel_in)
		{
			this->txPowerLevel = txPowerLevel_in;
		}

		/// @brief Set the Transmit frequency ranges.
		void setTxFrequencyRanges(const std::vector<ams::iface::rfmel::FrequencyRange>& txFrequencyRanges_in)
		{
			this->txFrequencyRanges = txFrequencyRanges_in;
		}

		/// @brief Set the Transmit Maximum duty factor.
		void setMaxTxDutyFactor(const DutyFactor maxTxDutyFactor_in)
		{
			this->maxTxDutyFactor = maxTxDutyFactor_in;
		}

		/// @brief Set the Transmit Maximum pulse width.
		void setMaxTxPulseWidth(const std::chrono::nanoseconds maxTxPulseWidth_in)
		{
			this->maxTxPulseWidth = maxTxPulseWidth_in;
		}

		/// @brief Set the Maximum Transmit attentuation.
		void setMaxTxAtten(const double maxTxAtten_in)
		{
			this->maxTxAtten = maxTxAtten_in;
		}

		/// @brief Set the Transmit attentuation commandable step size.
		void setTxAttenStepSize(const double txAttenStepSize_in)
		{
			this->txAttenStepSize = txAttenStepSize_in;
		}

	private:
		// Unique Tx Power Mode ID associated with this set of Tx Power Mode data.
		// Tx Power Mode IDs are assigned by the MFA provider.
		TxPowerModeID txPowerModeID{0};

		// Indicates if the Tx Power Mode ID corresponds to Linear or Compressed Operation.
		// TRUE  => Tx Power Mode ID corresponds to Linear Operation.
		// FALSE => Tx Power Mode ID corresponds to Compressed Operation.
		bool isLinearOperation = false;

		// Indicates the Tx Power Level corresponding to the Tx Power Mode ID.
		// A value of ZERO is reserved as the default power level. If applicable, the
		// the MFA provider may choose to assign additional transmit power levels.
		TxPowerLevel txPowerLevel{0};

		// The Tx Power Mode ID is valid for use when transmitting
		// this list of pre-defined of frequency ranges (Hz) provided by the MFA.
		// To prevent hardware damage, the Tx Power Mode ID is not to be used outside of
		// of the listed frequency ranges.
		std::vector<ams::iface::rfmel::FrequencyRange> txFrequencyRanges;

		// The maximum Tx Duty allowed when using this Tx Power Mode.
		DutyFactor maxTxDutyFactor{0.0};

		// The maximum duration of the Transmit pulse allowed when using this Tx Power Mode.
		std::chrono::nanoseconds maxTxPulseWidth{0};

		// The maximum Tx attenuation available in this Tx Power Mode. The maximum
		// available attenuation may change at runtime based on real-time calibration.
		double maxTxAtten{0.0};

		// @brief The Tx attenuation step size. The minimum increment in which the Tx
		// attenuation may be commanded.
		double txAttenStepSize{0.0};
	};

} // namespace ams::iface::rfmel
