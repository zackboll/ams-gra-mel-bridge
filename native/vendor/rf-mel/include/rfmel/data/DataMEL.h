#pragma once

#include <rfmel/mfa/RFMFAInfo.h>
#include <rfmel/rfmeltypes/RFMEL.h>
#include <rfmel/rfmeltypes/RFMELTypes.h>
#include <memory>
#include <string>

namespace ams::iface::rfmel
{
	/// @brief Provides access to the data reception functionality of the MFA.
	/// @Required This class must be provided in all RF MEL implementations. See individual class members for details.
	class DataMEL : public RFMEL
	{
	public:
		/// @brief Creates an Endpoint object that allows Skills to receive product data (IQ) from the MFA.
		/// The underlying transport utilizes RDMA, if available.
		/// The supplied address may be:
		/// - Heap memory addresses returned by malloc(), memalign(), etc.
		/// - GPU memory addresses, allocated by cudaMalloc(), etc.
		/// - FPGA Memory, with addresses determined by firmware.
		/// If the underlying transport is RDMA, then the RDMA provider must support
		/// the memory type referenced by regionAddress.
		/// Passing a nullptr for regionAddress instructs the MEL to manage the memory buffer internally.
		/// @RequiredIfReceive This function Creates Endpoint objects in support of required RF MEL
		/// functionality that allows Skills to receive product data (IQ) and must be must be provided by
		/// the implementer for all RF MEL implementations if the associated MFA supports receive.
		[[nodiscard]] virtual ams::iface::mel::RequestFor<ProductRxEndpoint> createProductRxEndpoint(JobDataFormat dataType, size_t regionSizeBytes,
																									 char* regionAddress = nullptr) = 0;

		/// @brief Function passes RDMA parameters to the MFA to register an external receive endpoint.
		/// @RequiredIfReceive This function passes RDMA parameters in support of required RF MEL functionality specific to receive
		/// and must be included as-is in all RF MEL implementations if the associated MFA supports receive and RDMA.
		/// @RequiredIfRDMA This function passes RDMA parameters in support of required RF MEL functionality and must be must be
		/// provided by the implementer for all RF MEL implementations if the associated MFA supports RDMA and Receive.
		[[nodiscard]] virtual ams::iface::mel::RequestFor<ExternalEndpoint> registerExternalRxEndpoint(
			JobDataFormat dataType, const RDMAExternalEndpointParams& externalParams) = 0;

		~DataMEL() override = default;

		/// @brief Returns information about the MFA.
		/// @Required This function passes MFA data and must be provided by the implementer
		/// for all RF MEL implementations.
		[[nodiscard]] virtual const RFMFAInfo& getRFMFAInfo() const = 0;

		/// @brief Creates an instance of the RF MEL.
		static std::shared_ptr<ams::iface::rfmel::DataMEL> create(const std::string& melConfiguration);

	protected:
		DataMEL() = default;
		DataMEL(const DataMEL&) = default;
		DataMEL(DataMEL&&) = default;
		DataMEL& operator=(const DataMEL&) = default;
		DataMEL& operator=(DataMEL&&) = default;
	};

} // namespace ams::iface::rfmel
