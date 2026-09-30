#pragma once

#include <rfmel/endpoints/BaseEndpoint.h>
#include <rfmel/rfmeltypes/RFMELTypes.h>

namespace ams::iface::rfmel
{
	/// @RequiredIfDynamicWaveformTransmitJobInterface This class provides required RF
	/// MEL functionality to provide a communications channel, and must be provided by
	/// the implementer in all RF MEL implementations if the associated MFA supports
	/// transmit of dynamic Tx waveforms. See individual members for details.
	/// @brief Represents a communications channel to the MFA from the Service by which dynamic waveform definitions are sent.
	/// If RDMA is supported, creates the Protection Domain / Memory Region, etc. to allow the MFA remote access to memory resident
	/// in the Skill's process space. It is assumed that any number of devices within the MFA may need to access this local memory
	/// region, so this class is responsible communicating with the infrastructure in the MFA for setting up all sockets, queue pairs,
	/// etc. Necessary for any MFA device, selected by the Virtual Aperture Scheduler, to access the local memory region according
	/// to the commands in the MFA Job Request.
	class WaveformTxEndpoint : public BaseEndpoint
	{
	public:
		/// @brief When constructing the endpoint, you must specify the binary format of the
		/// data products you want to give the MFA when transmitting (raw IQ, VITA, etc).
		/// @note Do not give PDW's to the Tx Endpoint, that would probably make no sense. However, it is up to
		/// an implementation to protect against this and reject the construction of an Tx endpoint like that.
		explicit WaveformTxEndpoint(JobDataFormat dataType);

		~WaveformTxEndpoint() override = default;
		WaveformTxEndpoint(const WaveformTxEndpoint&) = delete;
		WaveformTxEndpoint(WaveformTxEndpoint&&) = delete;
		WaveformTxEndpoint& operator=(const WaveformTxEndpoint&) = delete;
		WaveformTxEndpoint& operator=(WaveformTxEndpoint&&) = delete;

		/// @brief Gets the enum of this Endpoint's assigned data type (raw I/Q, VITA, etc)
		/// @RequiredIfTransmit Implementation required for receive RF MEL implementations
		[[nodiscard]] virtual JobDataFormat getAssignedDataFormat() const = 0;

		/// @brief Returns structure of the memory region owned by this Endpoint.
		/// @RequiredIfDynamicWaveformTransmitJobInterface This function passes Endpoint data
		/// in support of required RF MEL functionality and must be must be provided by the
		/// implementer for all RF MEL implementations if the associated MFA supports transmit
		/// of dynamic Tx waveforms.
		[[nodiscard]] virtual RDMAMemoryRegionParams getRDMAMemoryRegionParams() const = 0;

	protected:
		WaveformTxEndpoint() = default;
	};
} // namespace ams::iface::rfmel
