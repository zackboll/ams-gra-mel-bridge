#pragma once

#include <rfmel/rfmeltypes/RFMELTypes.h>
#include <cstdint>
#include <utility>

namespace ams::iface::rfmel
{
	/// @brief Describes a source of waveform data residing within an application's memory local to a DPP, which is transferred
	/// to the MFA "just in time" for the AWG to translate the samples into a transmitted waveform. Depending on the MFA
	/// implementation, either the MFA or the DPP may act as the initiator of the transfer. See also: WaveformTxEndpoint
	/// @RequiredIfWaveformStreamSupport This class provides data definition in support of required RF MEL functionality
	/// for describing waveform data sources and must be included as-is in all RF MEL implementations if the associated
	/// MFA supports Waveform streams.
	class WaveformStream
	{
	public:
		WaveformStream() = default;
		~WaveformStream() = default;
		WaveformStream(const WaveformStream&) = default;
		WaveformStream(WaveformStream&& other) noexcept : WaveformStream()
		{
			*this = std::move(other);
		}
		WaveformStream& operator=(const WaveformStream&) = default;
		WaveformStream& operator=(WaveformStream&& other) noexcept
		{
			if(this != &other)
			{
				endpointID = other.endpointID;
				startAddr = other.startAddr;
				numBytes = other.numBytes;
				sampleFormat = other.sampleFormat;
				itemFormat = other.itemFormat;
				repeatMode = other.repeatMode;
			}
			return *this;
		}

		/// @brief Get the Endpoint from which to stream.
		[[nodiscard]] auto getEndpointID() const
		{
			return this->endpointID;
		}

		/// @brief Get the virtual address from which to begin streaming.
		[[nodiscard]] auto getStartAddress() const
		{
			return this->startAddr;
		}

		/// @brief Get the number of bytes to stream for each iteration of the JobInterval.
		/// Each subsequent JobInterval either repeats the same 'numBytes' or continues
		/// 'numBytes' past the previous iteration's starting point.
		[[nodiscard]] auto getNumBytes() const
		{
			return this->numBytes;
		}

		/// @brief Get SampleFormat which specifies real vs. interleaved IQ vs complex-polar.
		/// See RFMFAInfo for supported sample formats.
		[[nodiscard]] auto getSampleFormat() const
		{
			return this->sampleFormat;
		}

		/// @brief Get ItemFormat specifies the numerical format.
		/// See RFMFAInfo for supported item formats.
		[[nodiscard]] auto getItemFormat() const
		{
			return this->itemFormat;
		}

		/// @brief Get RepeatMode which indicates how the waveform read location behaves after each
		/// iteration of the JobInterval; i.e. increment 'numBytes' and continue, or repeat from the top.
		/// If repeatMode is 'RepeatFromTop', the buffer must be at least 'numBytes' in length.
		/// If repeatMode is 'ContinueFromPrevious', the buffer must be 'numBytes' times the repeat count.
		[[nodiscard]] auto getRepeatMode() const
		{
			return this->repeatMode;
		}

		/// @brief Set the Endpoint from which to stream.
		void setEndpointID(EndpointID id)
		{
			this->endpointID = id;
		}

		/// @brief Set the virtual address from which to begin streaming.
		void setStartAddress(uint64_t addr)
		{
			this->startAddr = addr;
		}

		/// @brief Set the number of bytes to stream for each iteration of the JobInterval.
		/// Each subsequent JobInterval either repeats the same 'numBytes' or continues
		/// 'numBytes' past the previous iteration's starting point.
		void setNumBytes(size_t bytes)
		{
			this->numBytes = bytes;
		}

		/// @brief Set SampleFormat which specifies real vs. interleaved IQ vs complex-polar.
		/// See RFMFAInfo for supported sample formats.
		void setSampleFormat(DataSampleFormat sampleFormat_in)
		{
			this->sampleFormat = sampleFormat_in;
		}

		/// @brief Set ItemFormat specifies the numerical format.
		/// See RFMFAInfo for supported item formats.
		void setItemFormat(DataItemFormat itemFormat_in)
		{
			this->itemFormat = itemFormat_in;
		}

		/// @brief Set RepeatMode which indicates how the waveform read location behaves after each iteration of
		/// the JobInterval; i.e. increment 'numBytes' and continue, or repeat from the top.
		/// If repeatMode is 'RepeatFromTop', the buffer must be at least
		/// 'numBytes' in length. If repeatMode is 'ContinueFromPrevious',
		/// the buffer must be 'numBytes' times the repeat count.
		void setRepeatMode(BufferRepeatMode repeatMode_in)
		{
			this->repeatMode = repeatMode_in;
		}

		// The Endpoint from which to stream
		EndpointID endpointID = 0;

		// The virtual address from which to begin streaming.
		uint64_t startAddr = 0;

		/// The number of bytes to stream for each iteration of the JobInterval.
		// Each subsequent JobInterval either repeats the same 'numBytes' or continues
		// 'numBytes' past the previous iteration's starting point.
		size_t numBytes = 0;

		// Specifies real vs. interleaved IQ vs complex-polar.
		// See RFMFAInfo for supported sample formats.
		DataSampleFormat sampleFormat = DataSampleFormat::ComplexCartesian;

		// Specifies the numerical format.
		// See RFMFAInfo for supported item formats.
		DataItemFormat itemFormat = DataItemFormat::FixedPointSigned;

		// Indicates how the waveform read location behaves after each iteration of
		// the JobInterval; i.e. increment 'numBytes' and continue, or repeat from the top.
		// If repeatMode is 'RepeatFromTop', the buffer must be at least
		// 'numBytes' in length. If repeatMode is 'ContinueFromPrevious',
		// the buffer must be 'numBytes' times the repeat count.
		BufferRepeatMode repeatMode = BufferRepeatMode::RepeatFromTop;
	};
} // namespace ams::iface::rfmel
