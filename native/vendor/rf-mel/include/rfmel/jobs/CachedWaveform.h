#pragma once

#include <rfmel/rfmeltypes/RFMELTypes.h>

#include <complex>
#include <vector>

namespace ams::iface::rfmel
{
	/// @note: MFA implementations will vary in terms of the number of copies of the waveform held
	/// within the MFA (per Face, per Waveform Generator, etc.) and the format of the samples.
	/// @RequiredIfCachedWaveformJobInterface This class provides required RF MEL functionality for
	/// managing waveform definitions usable by any Virtual Aperture that supports cached TX waveforms,
	/// and must be provided by the implementer in all RF MEL implementations if the associated MFA
	/// supports cached waveforms for transmit jobs. See individual members for details.
	/// @brief Manages a waveform definition, cached within the MFA, on behalf of the Service.
	/// The waveform is not specific to a Virtual Aperture; it is usable by any Transmit Event in
	/// any JobInterval on any VirtualAperture that supports cached Tx waveforms.
	/// The number of samples and the sample rate are static and determined at the time the
	/// waveform is instantiated.
	/// The waveform may be referenced by any number of TxEvent objects until the object is destroyed.
	class CachedWaveform
	{
	public:
		virtual ~CachedWaveform() = default;

		// Copy/move are disallowed:
		CachedWaveform(const CachedWaveform&) = delete;
		CachedWaveform& operator=(const CachedWaveform&) = delete;
		CachedWaveform(CachedWaveform&& /*other*/) noexcept : CachedWaveform()
		{
		}
		CachedWaveform& operator=(CachedWaveform&& other) noexcept
		{
			if(this != &other)
			{
			}
			return *this;
		}

		/// @brief Returns the number of samples comprising the waveform.
		/// @RequiredIfCachedWaveformJobInterface This function is required for all RF MEL implementations that cached waveform for transmit jobs.
		[[nodiscard]] virtual size_t getNumSamples() const = 0;

		/// @brief Returns the sample rate associated with the waveform.
		/// @RequiredIfCachedWaveformJobInterface This function is required for all RF MEL implementations that cached waveform for transmit jobs.
		[[nodiscard]] virtual Frequency getSampleRate() const = 0;

	protected:
		CachedWaveform() = default;
	};
} // namespace ams::iface::rfmel
