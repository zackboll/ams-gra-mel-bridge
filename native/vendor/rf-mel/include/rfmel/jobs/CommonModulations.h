#pragma once

#include <math/geometry/Geometry.h>
#include <math/units/UTCTime.h>
#include <rfmel/jobs/CachedWaveform.h>
#include <rfmel/jobs/ModulationExtensionBase.h>
#include <rfmel/jobs/WaveformStream.h>
#include <rfmel/rfmeltypes/RFMELTypes.h>
#include <map>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

namespace ams::iface::rfmel
{
	/// @brief: Default pulse modulation; no modulation occurs.
	/// @RequiredIfTransmit This class provides data definition in support of required RF MEL Tone functionality
	/// and must be included as-is in all RF MEL implementations if the MFA supports transmit.
	class Tone
	{
	};

	/// @brief: Sequence of phase-modulated chips with each chip equal duration.
	/// @RequiredIfPhaseCode This class provides data definition in support of required RF MEL functionality
	/// for Phase-Modulated Chips and must be included as-is in all RF MEL implementations if the MFA supports Phase Coding.
	class PhaseCode
	{
	public:
		PhaseCode() = default;
		PhaseCode(const ams::util::math::Femtoseconds& dur, std::vector<double>& phase) : chipDuration{dur}, phaseChipData{phase}
		{
		}
		~PhaseCode() = default;
		PhaseCode(const PhaseCode&) = default;
		PhaseCode(PhaseCode&& source) noexcept : chipDuration{source.chipDuration}, phaseChipData{std::move(source.phaseChipData)}
		{
		}
		PhaseCode& operator=(const PhaseCode&) = default;
		PhaseCode& operator=(PhaseCode&& source) noexcept
		{
			if(this != &source)
			{
				chipDuration = source.chipDuration;
				phaseChipData = std::move(source.phaseChipData);
			}
			return *this;
		}

		/// @brief Gets the chip duration is fs.
		[[nodiscard]] const ams::util::math::Femtoseconds& getChipDuration() const
		{
			return this->chipDuration;
		}
		/// @brief Sets the chip duration in fs.
		void setChipDuration(ams::util::math::Femtoseconds newValue)
		{
			this->chipDuration = newValue;
		}

		/// @brief Gets the existing vector.
		[[nodiscard]] const std::vector<double>& getPhaseChipData() const
		{
			return this->phaseChipData;
		}
		/// @brief Replace the existing vector with a new vector.
		void setPhaseChipData(const std::vector<double>& newValue)
		{
			this->phaseChipData = newValue;
		}
		/// @brief Add a new element to the vector.
		void addPhaseChipData(double data)
		{
			this->phaseChipData.push_back(data);
		}

	private:
		// The duration of each phase chip of the pulse.
		ams::util::math::Femtoseconds chipDuration{0};

		// The phase offset to apply to each chip (radians).
		// Total vector size must be at least eventDuration/chipDuration.
		std::vector<double> phaseChipData;
	};

	/// @brief Linear Frequency Modulation (LFM); applies a linear frequency slope to the transmitted pulse.
	/// @note: Start Frequency = jobEvent.centerFrequency - (frequencyExtent / 2)
	/// End Frequency   = jobEvent.centerFrequency + (frequencyExtent / 2)
	/// where the pulseWidth is defined by TransmitEvent::duration.
	/// @RequiredIfLFM This class provides data definition in support of required RF MEL functionality
	/// for Linear Frequency Modulation and must be included as-is in all RF MEL implementations if the MFA supports LFM.
	class LinearFrequencyModulation
	{
	public:
		LinearFrequencyModulation() = default;
		explicit LinearFrequencyModulation(double freq) : frequencyExtent{freq}
		{
		}
		~LinearFrequencyModulation() = default;
		LinearFrequencyModulation(const LinearFrequencyModulation&) = default;
		LinearFrequencyModulation& operator=(const LinearFrequencyModulation&) = default;
		LinearFrequencyModulation(LinearFrequencyModulation&& other) noexcept : frequencyExtent{other.frequencyExtent}
		{
			other.frequencyExtent = 0;
		}
		LinearFrequencyModulation& operator=(LinearFrequencyModulation&& other) noexcept
		{
			if(this != &other)
			{
				frequencyExtent = other.frequencyExtent;
				other.frequencyExtent = 0;
			}
			return *this;
		}

		/// @brief Gets the frequency extent in Hz.
		[[nodiscard]] double getFrequencyExtent() const
		{
			return this->frequencyExtent;
		}
		/// @brief Sets the frequency extent in Hz.
		void setFrequencyExtent(double newValue)
		{
			this->frequencyExtent = newValue;
		}

	private:
		double frequencyExtent{0}; // Hz
	};

	/// @RequiredIfCachedWaveformJobInterface This class provides data definition in support of required RF MEL functionality
	/// for cached waveforms and must be included as-is in all RF MEL implementations if the associated
	/// MFA supports the use of cached waveforms for transmit jobs.
	/// @RequiredIfWaveformStreamSupport This class provides data definition in support of required RF MEL functionality
	/// for Waveform streams and must be included as-is in all RF MEL implementations if the associated
	/// MFA supports the use of waveform streams for transmit jobs.
	/// @brief Contains an array of IQ samples which define the transmitted waveform.
	/// @note The CachedArbitraryWaveform or StreamingArbitraryWaveform classes may be used instead, as needed. See class descriptions for more
	/// details.
	class WaveformBuffer
	{
	public:
		WaveformBuffer() = default;
		WaveformBuffer(DataSampleFormat f, JobTransmitVector& s) : format{f}, txSamples{s}
		{
		}
		~WaveformBuffer() = default;
		WaveformBuffer(const WaveformBuffer&) = default;
		WaveformBuffer(WaveformBuffer&& other) noexcept : format(other.format), txSamples(std::move(other.txSamples))
		{
		}
		WaveformBuffer& operator=(const WaveformBuffer&) = default;
		WaveformBuffer& operator=(WaveformBuffer&& other) noexcept
		{
			if(this != &other)
			{
				this->format = other.format;
				this->txSamples = std::move(other.txSamples);
			}
			return *this;
		}

		/// @brief Get the data sampling format.
		[[nodiscard]] const DataSampleFormat& getFormat() const
		{
			return this->format;
		}
		/// @brief Set data sampling format.
		void setFormat(DataSampleFormat newValue)
		{
			this->format = newValue;
		}

		/// @brief Get the existing vector.
		[[nodiscard]] const JobTransmitVector& getTxSamples() const
		{
			return this->txSamples;
		}
		/// @brief Replace the existing vector with a new vector.
		void setTxSamples(const JobTransmitVector& newValue)
		{
			this->txSamples = newValue;
		}

	private:
		/// Indicates the format of the txSamples field.
		DataSampleFormat format{DataSampleFormat::ComplexCartesian};

		/// @brief Describes the arbitrary waveform modulation as a
		/// vector of IQ samples.
		/// The duration of each sample depends on jobEvent.duration:
		/// sampleDuration = jobEvent.duration / samples.size()
		/// For format==Real, the imaginary component of each sample
		/// must equal zero.
		/// The JobTransmitVector is a variant of either a smart pointer to a vector of MEL Complex
		/// 8- or 16- bit integers.
		JobTransmitVector txSamples;
	};

	/// @RequiredIfOOK This class provides data definition in support of required RF MEL functionality for Pulse-Position
	/// Modulation and must be included as-is in all RF MEL implementations if the associated MFA supports the use of On-Off Keying (OOK).
	/// @brief On-Off Keying (Pulse-Position Modulation): Maps a series of enable/disable commands to a transmitted pulse
	/// event in a repeated JobInterval. Each repetition of the JobInterval maps to an entry in the 'pulseEnabled' vector, so
	/// its size must be equal to the JobInterval::sequenceRepeatCount.
	/// @note: If multiple Tx events occur in a JobInterval, they must not reference the same instance of OnOffKeying,
	/// in order to prevent ambiguity in the mapping of commands to pulses.
	class OnOffKeying
	{
	public:
		OnOffKeying() = default;
		explicit OnOffKeying(std::vector<bool>& enabled) : pulseEnabled{enabled}
		{
		}
		~OnOffKeying() = default;
		OnOffKeying(const OnOffKeying&) = default;
		OnOffKeying(OnOffKeying&& other) noexcept : pulseEnabled(std::move(other.pulseEnabled))
		{
		}
		OnOffKeying& operator=(const OnOffKeying&) = default;
		OnOffKeying& operator=(OnOffKeying&& other) noexcept
		{
			if(this != &other)
			{
				this->pulseEnabled = std::move(other.pulseEnabled);
			}
			return *this;
		}

		/// @brief Get the existing vector.
		[[nodiscard]] const std::vector<bool>& getPulseEnabled() const
		{
			return this->pulseEnabled;
		}
		/// @brief Replace the existing vector with a new vector.
		void setPulseEnabled(const std::vector<bool>& newValue)
		{
			this->pulseEnabled = newValue;
		}
		/// @brief Add a new element to the vector.
		void addPulseEnabled(bool enabled)
		{
			this->pulseEnabled.push_back(enabled);
		}

	private:
		// Each entry indicates whether the associated pulse should
		// execute during the associated iteration of the JobInterval.
		std::vector<bool> pulseEnabled;
	};

	/// @brief The CpfskPulse is used within the CustomWaveform modulation type to specify Continuous Phase Frequency
	/// Shift Keying (CPFSK) modulation.
	/// @RequiredIfCPFSK This class provides data definition in support of required RF MEL functionality for
	/// CustomWaveform CPFSK modulation and must be included as-is in all RF MEL implementations if the associated
	/// MFA supports the construction of a Custom Wave form.
	class CpfskPulse
	{
	public:
		CpfskPulse() = default;
		explicit CpfskPulse(const std::vector<bool>& chipData_in, ams::util::math::Femtoseconds pulseDuration_in,
							ams::util::math::Femtoseconds chipDuration_in, ams::iface::rfmel::Frequency freqOffset_in)
			: chipData{chipData_in}, pulseDuration{pulseDuration_in}, chipDuration{chipDuration_in}, frequencyOffset{freqOffset_in}
		{
		}

		~CpfskPulse() = default;
		CpfskPulse(const CpfskPulse&) = default;
		CpfskPulse(CpfskPulse&&) noexcept = default;
		CpfskPulse& operator=(const CpfskPulse&) = default;
		CpfskPulse& operator=(CpfskPulse&&) = default;

		/// @brief set the chip data.
		void setChipData(const std::vector<bool>& chipData_in)
		{
			this->chipData = chipData_in;
		}

		/// @brief return the chip data.
		[[nodiscard]] const std::vector<bool>& getChipData()
		{
			return this->chipData;
		}

		/// @brief set the pulse duration.
		void setPulseDuration(ams::util::math::Femtoseconds pulseDuration_in)
		{
			this->pulseDuration = pulseDuration_in;
		}

		/// @brief return the pulse duration.
		[[nodiscard]] ams::util::math::Femtoseconds getPulseDuration()
		{
			return this->pulseDuration;
		}

		/// @brief set the chip duration.
		void setChipDuration(ams::util::math::Femtoseconds chipDuration_in)
		{
			this->chipDuration = chipDuration_in;
		}

		/// @brief return the chip duration.
		[[nodiscard]] ams::util::math::Femtoseconds getChipDuration()
		{
			return this->chipDuration;
		}

		/// @brief set the frequency offset in Hertz.
		void setFrequencyOffset(ams::iface::rfmel::Frequency freqOffset)
		{
			this->frequencyOffset = freqOffset;
		}

		/// @brief return the frequency offset in Hertz.
		[[nodiscard]] ams::iface::rfmel::Frequency getFrequencyOffset()
		{
			return this->frequencyOffset;
		}

	private:
		std::vector<bool> chipData{};
		ams::util::math::Femtoseconds pulseDuration{0};
		ams::util::math::Femtoseconds chipDuration{0}; // CPFSK chip duration of the pulse
		ams::iface::rfmel::Frequency frequencyOffset{0};
	};

	/// @brief The WaveformProfileIterations is used within the CustomWaveform modulation type to specify duration
	/// of the chip, the index corresponding to the waveformProfile to apply during this chip, and the number of
	/// iterations that this chip is to be repeated.
	/// @RequiredIfCustomWaveform This class provides data definition in support of required RF MEL functionality
	/// for CustomWaveform modulation and must be included as-is in all RF MEL implementations if the associated
	/// MFA supports the construction of a Custom Wave form.
	class WaveformProfileIterations
	{
	public:
		WaveformProfileIterations() = default;
		WaveformProfileIterations(int num, int wave) : numChipIterations{num}, waveformProfileIndex{wave}
		{
		}
		~WaveformProfileIterations() = default;
		WaveformProfileIterations(const WaveformProfileIterations&) = default;
		WaveformProfileIterations(WaveformProfileIterations&&) = default;
		WaveformProfileIterations& operator=(const WaveformProfileIterations&) = default;
		WaveformProfileIterations& operator=(WaveformProfileIterations&&) = default;

		/// @brief Get the number of chip iterations.
		[[nodiscard]] int getNumChipIterations() const
		{
			return this->numChipIterations;
		}
		/// @brief Set the number of chip iterations.
		void setNumChipIterations(int newValue)
		{
			this->numChipIterations = newValue;
		}

		/// @brief Get the waveform profile index.
		[[nodiscard]] int getWaveformProfileIndex() const
		{
			return this->waveformProfileIndex;
		}
		/// @brief Set the waveform profile index.
		void setWaveformProfileIndex(int newValue)
		{
			this->waveformProfileIndex = newValue;
		}

	private:
		// The numChipIterations specifies the number of times that the specified waveformProfile.
		// This pattern is applied during this set of WaveformProfileIterations.
		int numChipIterations{1};

		// The waveformProfileIndex specifies the index of the waveformProfile to be applied
		// during this set of WaveformProfileIterations.
		int waveformProfileIndex{0};
	};

	/// @brief The EventWaveformProfilePattern is used within the CustomWaveform modulation type to
	/// specify the list of waveformProfiles to be applied from chip to chip within an event.
	/// @RequiredIfCustomWaveform This class provides data definition in support of required RF MEL functionality
	/// for CustomWaveform modulation and must be included as-is in all RF MEL implementations if the associated
	/// MFA supports the construction of a Custom Wave form.
	class EventWaveformProfilePattern
	{
	public:
		EventWaveformProfilePattern() = default;
		explicit EventWaveformProfilePattern(std::vector<WaveformProfileIterations>& wave) : waveformProfilePattern{wave}
		{
		}
		~EventWaveformProfilePattern() = default;
		EventWaveformProfilePattern(const EventWaveformProfilePattern&) = default;
		EventWaveformProfilePattern(EventWaveformProfilePattern&&) = default;
		EventWaveformProfilePattern& operator=(const EventWaveformProfilePattern&) = default;
		EventWaveformProfilePattern& operator=(EventWaveformProfilePattern&&) = default;

		/// @brief Get the existing vector.
		[[nodiscard]] const std::vector<WaveformProfileIterations>& getWaveformProfilePattern() const
		{
			return this->waveformProfilePattern;
		}
		/// @brief Replace the existing vector with a new vector.
		void setWaveformProfilePattern(const std::vector<WaveformProfileIterations>& newValue)
		{
			this->waveformProfilePattern = newValue;
		}
		/// @brief Add a new element to the vector.
		void addWaveformProfilePattern(const WaveformProfileIterations& pattern)
		{
			this->waveformProfilePattern.push_back(pattern);
		}

	private:
		// The waveformProfilePattern specifies the list of waveformProfiles to be
		// applied from chip to chip within an event.
		std::vector<WaveformProfileIterations> waveformProfilePattern;
	};

	/// @brief The WaveformProfileParams is used within the CustomWaveform modulation type to
	/// specify the frequency, phase, and amplitude waveform parameters to be applied
	/// to the waveform.
	/// @RequiredIfCustomWaveform This class provides data definition in support of required RF MEL functionality
	/// for CustomWaveform modulation and must be included as-is in all RF MEL implementations if the associated
	/// MFA supports the construction of a Custom Wave form.
	class WaveformProfileParams
	{
	public:
		WaveformProfileParams() = default;
		WaveformProfileParams(const ams::util::math::Femtoseconds& dur, bool upPhase, bool upDeltaP, bool upFreq, bool upDeltaF, bool upSlope,
							  bool upAmp, double phase, double deltaP, double freq, double deltaF, double slope, double amp)
			: chipDuration{dur},
			  updatePhase{upPhase},
			  updateDeltaPhase{upDeltaP},
			  updateFrequencyOffset{upFreq},
			  updateDeltaFrequency{upDeltaF},
			  updateFrequencySlope{upSlope},
			  updateAmplitudeFactor{upAmp},
			  phaseDeg{phase},
			  deltaPhaseDeg{deltaP},
			  frequencyOffsetHz{freq},
			  deltaFrequencyHz{deltaF},
			  frequencySlopeHzSec{slope},
			  amplitudeFactor{amp}
		{
		}
		~WaveformProfileParams() = default;
		WaveformProfileParams(const WaveformProfileParams&) = default;
		WaveformProfileParams(WaveformProfileParams&&) = default;
		WaveformProfileParams& operator=(const WaveformProfileParams&) = default;
		WaveformProfileParams& operator=(WaveformProfileParams&&) = default;

		/// @brief Get the chip duration in fs.
		[[nodiscard]] const ams::util::math::Femtoseconds& getChipDuration()
		{
			return this->chipDuration;
		}
		/// @brief Set the chip duration in fs.
		void setChipDuration(ams::util::math::Femtoseconds newValue)
		{
			this->chipDuration = newValue;
		}

		/// @brief Get update phase.
		[[nodiscard]] bool getUpdatePhase() const
		{
			return this->updatePhase;
		}
		/// @brief Set update phase.
		void setUpdatePhase(bool newValue)
		{
			this->updatePhase = newValue;
		}

		/// @brief Get update delta phase.
		[[nodiscard]] bool getUpdateDeltaPhase() const
		{
			return this->updateDeltaPhase;
		}
		/// @brief Set update delta phase.
		void setUpdateDeltaPhase(bool newValue)
		{
			this->updateDeltaPhase = newValue;
		}

		/// @brief Get update frequency offset.
		[[nodiscard]] bool getUpdateFrequencyOffset() const
		{
			return this->updateFrequencyOffset;
		}
		/// @brief Set update frequency offset.
		void setUpdateFrequencyOffset(bool newValue)
		{
			this->updateFrequencyOffset = newValue;
		}

		/// @brief Get update delta frequency.
		[[nodiscard]] bool getUpdateDeltaFrequency() const
		{
			return this->updateDeltaFrequency;
		}
		/// @brief Set update delta frequency.
		void setUpdateDeltaFrequency(bool newValue)
		{
			this->updateDeltaFrequency = newValue;
		}

		/// @brief Get update frequency slope.
		[[nodiscard]] bool getUpdateFrequencySlope() const
		{
			return this->updateFrequencySlope;
		}
		/// @brief Set update frequency slope.
		void setUpdateFrequencySlope(bool newValue)
		{
			this->updateFrequencySlope = newValue;
		}

		/// @brief Get update amplitude factor.
		[[nodiscard]] bool getUpdateAmplitudeFactor() const
		{
			return this->updateAmplitudeFactor;
		}
		/// @brief Set update amplitude factor.
		void setUpdateAmplitudeFactor(bool newValue)
		{
			this->updateAmplitudeFactor = newValue;
		}

		/// @brief Get phase degree.
		[[nodiscard]] double getPhaseDeg() const
		{
			return this->phaseDeg;
		}
		/// @brief Set phase degree.
		void setPhaseDeg(double newValue)
		{
			this->phaseDeg = newValue;
		}

		/// @brief Get delta phase degree.
		[[nodiscard]] double getDeltaPhaseDeg() const
		{
			return this->deltaPhaseDeg;
		}
		/// @brief Set delta phase degree.
		void setDeltaPhaseDeg(double newValue)
		{
			this->deltaPhaseDeg = newValue;
		}

		/// @brief Get frequency offset Hz.
		[[nodiscard]] double getFrequencyOffsetHz() const
		{
			return this->frequencyOffsetHz;
		}
		/// @brief Set frequency offset Hz.
		void setFrequencyOffsetHz(double newValue)
		{
			this->frequencyOffsetHz = newValue;
		}

		/// @brief Get delta frequency Hz.
		[[nodiscard]] double getDeltaFrequencyHz() const
		{
			return this->deltaFrequencyHz;
		}
		/// @brief Set delta frequency Hz.
		void setDeltaFrequencyHz(double newValue)
		{
			this->deltaFrequencyHz = newValue;
		}

		/// @brief Get frequency slope HzSec.
		[[nodiscard]] double getFrequencySlopeHzSec() const
		{
			return this->frequencySlopeHzSec;
		}
		/// @brief Set frequency slope HzSec.
		void setFrequencySlopeHzSec(double newValue)
		{
			this->frequencySlopeHzSec = newValue;
		}

		/// @brief Get amplitude factor.
		[[nodiscard]] double getAmplitudeFactor() const
		{
			return this->amplitudeFactor;
		}
		/// @brief Set amplitude factor.
		void setAmplitudeFactor(double newValue)
		{
			this->amplitudeFactor = newValue;
		}

	private:
		// The chipDuration specifies the period of time (femtoSec) that the
		// specified waveformProfile is performed for each iteration.
		// Use quantizeDuration() when setting this value to ensure the MFA is able to execute as requested.
		ams::util::math::Femtoseconds chipDuration{0};

		// The updatePhase specifies if the phaseDeg parameter should be updated
		// at the time this waveform profile is applied.
		bool updatePhase{false};

		// The updateDeltaPhase specifies if the deltaPhaseDeg parameter should be updated
		// at the time this waveform profile is applied.
		bool updateDeltaPhase{false};

		// The updateFrequencyOffset specifies if the frequencyOffsetHz parameter should be updated
		// at the time this waveform profile is applied.
		bool updateFrequencyOffset{false};

		// The updateDeltaFrequency specifies if the deltaFrequencyHz parameter should be updated
		// at the time this waveform profile is applied.
		bool updateDeltaFrequency{false};

		// The updateFrequencySlope specifies if the frequencySlopeHzSec parameter should be updated
		// at the time this waveform profile is applied.
		bool updateFrequencySlope{false};

		// The updateAmplitudeFactor specifies if the amplitudeFactor parameter should be updated
		// at the time this waveform profile is applied.
		bool updateAmplitudeFactor{false};

		// The phaseDeg specifies the phase (deg) to apply at the time this waveform
		// profile is applied.
		double phaseDeg{0.0};

		// The deltaPhaseDeg specifies the phase addition that is temporarily applied for the
		// duration of time that this waveform profile is applied.
		double deltaPhaseDeg{0.0};

		// The frequencyOffsetHz specifies offset from the Center Frequency that is applied at
		// the time that this waveform profile is applied.
		double frequencyOffsetHz{0.0};

		// The deltaFrequencyHz specifies the frequency addition that is temporarily
		// applied for the duration of time that this waveform profile is applied.
		double deltaFrequencyHz{0.0};

		// The frequencySlopeHzSec specifies the frequency slope that is applied at
		// the time that this waveform profile is applied.
		double frequencySlopeHzSec{0.0};

		// The amplitudeFactor specifies the amplitude factor that is applied at
		// the time that this waveform profile is applied. The amplitude factor is a
		// number from 0-1 that represents the percentage of amplitude reduction that
		// is applied at the time this waveform profile is applied.
		double amplitudeFactor{0.0};
	};

	/// @brief If supported, the CustomWaveform modulation type provides the capbility to construct a unique waveform by
	/// allowing the user to set and modify the various frequency, phase, and amplitude waveform parameters at the chip level
	/// within an event. The combined duration of all of the chips defined in the eventWaveformProfilePattern must
	/// be equal to the duration of the corresponding event.
	/// @RequiredIfCustomWaveform This class provides data definition in support of required RF MEL functionality for CustomWaveform
	/// modulation and must be included as-is in all RF MEL implementations if the associated MFA supports the construction of a Custom Wave form.
	class CustomWaveform
	{
	public:
		CustomWaveform() = default;

		CustomWaveform(std::vector<EventWaveformProfilePattern>& pattern, std::vector<WaveformProfileParams>& profile)
			: eventWaveformProfilePattern{pattern}, waveformProfiles{profile}
		{
		}
		~CustomWaveform() = default;
		CustomWaveform(const CustomWaveform& other) = default;
		CustomWaveform(CustomWaveform&& other) noexcept
			: eventWaveformProfilePattern{std::move(other.eventWaveformProfilePattern)}, waveformProfiles{std::move(other.waveformProfiles)}
		{
		}
		CustomWaveform& operator=(const CustomWaveform&) = default;
		CustomWaveform& operator=(CustomWaveform&& other) noexcept
		{
			if(this != &other)
			{
				eventWaveformProfilePattern = std::move(other.eventWaveformProfilePattern);
				waveformProfiles = std::move(other.waveformProfiles);
			}
			return *this;
		}

		/// @brief Get the existing vector.
		[[nodiscard]] const std::vector<EventWaveformProfilePattern>& getEventWaveformProfilePattern() const
		{
			return this->eventWaveformProfilePattern;
		}
		/// @brief Replaces the existing vector with a new vector.
		void setEventWaveformProfilePattern(const std::vector<EventWaveformProfilePattern>& newValue)
		{
			this->eventWaveformProfilePattern = newValue;
		}
		/// @brief Add a new element to the vector.
		void addEventWaveformProfilePattern(const EventWaveformProfilePattern& pattern)
		{
			this->eventWaveformProfilePattern.push_back(pattern);
		}

		/// @brief Get the existing vector.
		[[nodiscard]] const std::vector<WaveformProfileParams>& getWaveformProfiles() const
		{
			return this->waveformProfiles;
		}
		/// @brief Replaces the existing vector with a new vector.
		void setWaveformProfiles(const std::vector<WaveformProfileParams>& newValue)
		{
			this->waveformProfiles = newValue;
		}
		/// @brief Add a new element to the vector.
		void addWaveformProfile(const WaveformProfileParams& profile)
		{
			this->waveformProfiles.push_back(profile);
		}

	private:
		// The eventWaveformProfilePattern specifies the list of waveformProfiles to be
		// applied from chip to chip for an event. Each time this modulation is applied
		// by an event, the eventWaveformProfilePattern is iterated. When the end of the
		// list is reached, the list is repeated from the beginning. This field allows for
		// a single definition of waveformProfiles to be utilized by multiple events with a
		// JobInterval.
		std::vector<EventWaveformProfilePattern> eventWaveformProfilePattern;

		// The waveformProfiles specifies the list of unique WaveformProfileParams to be
		// applied from chip to chip within an event.
		std::vector<WaveformProfileParams> waveformProfiles;
	};
	/// @brief Specifies all the built-in Modulations supported by the Common RF MEL.
	/// Not all MELs are required to support all built-in types. See: RF MEL IDD Section 1.4 Implementation Compliance.
	using CommonModulations = std::variant<Tone, PhaseCode, LinearFrequencyModulation, WaveformBuffer, OnOffKeying, CpfskPulse, CustomWaveform>;

	/// @brief Specifies the built-in modulation present
	using Modulation = std::variant<CommonModulations, std::shared_ptr<ModulationExtensionBase>, WaveformStream, std::shared_ptr<CachedWaveform>>;

} // namespace ams::iface::rfmel
