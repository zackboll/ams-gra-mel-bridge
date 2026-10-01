#pragma once

#include <mel/library/CommonMEL.h>

#include <rfmel/c2/C2MELTypes.h>
#include <rfmel/c2/CancelStatus.h>
#include <rfmel/c2/JobIntervalStatus.h>
#include <rfmel/c2/JobRequest.h>
#include <rfmel/c2/VirtualAperture.h>
#include <rfmel/c2/WaveformTxEndpoint.h>
#include <rfmel/endpoints/ExternalEndpoint.h>
#include <rfmel/jobs/JobEvent.h>
#include <rfmel/jobs/JobInterval.h>
#include <rfmel/mfa/RFMFAInfo.h>
#include <rfmel/rfmeltypes/RFMEL.h>
#include <memory>
#include <string>
#include <vector>

namespace ams::iface::rfmel
{
	/// @brief Provides access to the Command & Control (C2) and waveform transmission functionality of the MFA.
	/// @Required This class must be provided in all RF MEL implementations. See individual class members for details.
	class C2MEL : public RFMEL
	{
	public:
		~C2MEL() override = default;

		/// @brief Allows Skills to request a Virtual Aperture for Antenna Jobs.
		/// @note that the local function info vector can be left empty if the Skill does not need to send local
		/// function information to the MFA.
		/// @note that the VA Definition File Info can be left empty if the Skill does not need to send VA
		/// configuration information to the MFA.
		/// @Required This function supports passing Job Interval data and must be provided by the implementer
		/// for all RF MEL implementations.
		[[nodiscard]] virtual ams::iface::mel::RequestFor<VirtualAperture> requestVirtualAperture(
			VirtualApertureDefinitionID vaDefID, Priority priority, const std::vector<std::string>& localFunctionInfo,
			const std::string& vaDefFileInfo, const std::vector<ams::iface::mel::UCI_ID>& capabilityIDs) = 0;

		/// @brief Requests the allocation of cache memory inside the MFA to store a waveform definition.
		/// The exact data format and storage requirements vary by implementation.
		/// Once the returned shared_ptr is no longer owned by any user objects or JobDetails objects,
		/// the memory on the MFA is subject to being reclaimed and may not be used in future jobs.
		/// @RequiredIfCachedWaveformJobInterface This function Requests cache memory in support of
		/// required RF MEL functionality to store waveform definitions and must be must be provided by the implementer
		/// for all RF MEL implementations if the associated MFA supports cached waveforms.
		[[nodiscard]] virtual ams::iface::mel::RequestFor<CachedWaveform> requestCachedWaveform(Priority priority,
																								const std::vector<std::complex<double>>& samples,
																								Frequency sampleRate) = 0;

		/// @brief Creates an Endpoint object that allows Skills to transmit dynamic Tx waveforms the MFA.
		/// The underlying transport utilizes RDMA, if available.
		/// The supplied address may be:
		/// - Heap memory addresses returned by malloc(), memalign(), etc.
		/// - GPU memory addresses, allocated by cudaMalloc(), etc.
		/// - FPGA Memory, with addresses determined by firmware.
		/// If the underlying transport is RDMA, then the RDMA provider must support the memory type referenced
		/// by regionAddress.
		/// Passing a nullptr for regionAddress instructs the MEL to manage the memory buffer internally.
		/// @RequiredIfDynamicWaveformTransmitJobInterface This function Creates Endpoint objects in support
		/// of required RF MEL functionality and must be must be provided by the implementer for all RF MEL
		/// implementations if the associated MFA supports transmit of dynamic Tx waveforms.
		[[nodiscard]] virtual ams::iface::mel::RequestFor<WaveformTxEndpoint> createWaveformTxEndpoint(JobDataFormat dataType, size_t regionSizeBytes,
																									   char* regionAddress = nullptr) = 0;

		/// @brief Function passes RDMA parameters to the MFA to register an external transmit endpoint.
		/// @RequiredIfTransmit This function passes RDMA parameters in support of required RF MEL functionality specific to transmit
		/// and must be included as-is in all RF MEL implementations if the associated MFA supports transmit and RDMA.
		/// @RequiredIfRDMA This function passes RDMA parameters in support of required RF MEL functionality and must be must be
		/// provided by the implementer for all RF MEL implementations if the associated MFA supports RDMA and transmit.
		[[nodiscard]] virtual ams::iface::mel::RequestFor<ExternalEndpoint> registerExternalTxEndpoint(
			JobDataFormat dataType, const RDMAExternalEndpointParams& externalParams) = 0;

		/// @brief Returns information about the MFA.
		/// @Required This function passes MFA data and must be provided by the implementer
		/// for all RF MEL implementations.
		[[nodiscard]] virtual const RFMFAInfo& getRFMFAInfo() const = 0;

		/// @brief Creates an instance of the RF MEL.
		static std::shared_ptr<ams::iface::rfmel::C2MEL> create(const std::string& melConfiguration);

	protected:
		C2MEL() = default;
		C2MEL(const C2MEL&) = default;
		C2MEL(C2MEL&&) = default;
		C2MEL& operator=(const C2MEL&) = default;
		C2MEL& operator=(C2MEL&&) = default;
	};

} // namespace ams::iface::rfmel
