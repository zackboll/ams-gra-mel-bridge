#pragma once

#include <rfmel/endpoints/BaseEndpoint.h>
#include <rfmel/jobs/JobInterval.h>
#include <rfmel/rfmeltypes/ProductRxMetadata.h>
#include <rfmel/rfmeltypes/RFMELTypes.h>

#include <memory>
#include <functional>
#include <variant>

namespace ams::iface::rfmel
{
	/// @RequiredIfReceive This class provides required RF MEL functionality for receive
	/// capabilities, and must be provided by the implementer in all RF MEL
	/// implementations if the associated MFA supports receive. See individual members for details.
	/// @brief Represents a communications channel from the MFA to the Service by which outputs from a job are received.
	/// If RDMA is supported, creates the Protection Domain / Memory Region, etc. to allow the MFA remote access to memory
	/// resident in the Skill's process space. It is assumed that any number of devices within the MFA may need to
	/// access this local memory region, so this class is responsible communicating with the infrastructure in the MFA for
	/// setting up all sockets, queue pairs, etc. necessary for any MFA device, selected by the Virtual Aperture Scheduler,
	/// to access the local memory region according to the commands in the MFA Job Request.
	class ProductRxEndpoint : public BaseEndpoint
	{
	public:
		// Defines the function signature used to indicate data is available.
		// (has been remotely written to) for the application to consume.

		/// @brief When constructing the endpoint, you must specify the binary format of the
		/// data products you want the MFA to return for each JobInterval.
		explicit ProductRxEndpoint(JobDataFormat dataType);

		~ProductRxEndpoint() override = default;
		ProductRxEndpoint(const ProductRxEndpoint&) = delete;
		ProductRxEndpoint(ProductRxEndpoint&&) = delete;
		ProductRxEndpoint& operator=(const ProductRxEndpoint&) = delete;
		ProductRxEndpoint& operator=(ProductRxEndpoint&&) = delete;

		/// @brief Returns the Endpoint Identifier to be used in JobRequests.
		[[nodiscard]] EndpointID getEndpointID() const override = 0;

		/// @brief Gets the enum of this Endpoint's assigned data type (PDW, etc.)
		/// @RequiredIfReceive Implementation required for receive RF MEL implementations
		[[nodiscard]] virtual JobDataFormat getAssignedDataFormat() const = 0;

		/// @brief Returns structure of the memory region owned by this Endpoint.
		/// @RequiredIfRDMA This function passes Endpoint memory region data and must be
		/// provided by the implementer for all RF MEL implementations if the associated
		/// MFA supports RDMA and Receive.
		[[nodiscard]] virtual RDMAMemoryRegionParams getRDMAMemoryRegionParams() const = 0;

		/// @brief Registers the given function object as the callback for newly.
		/// arrived data. Only one callback may be registered to an Endpoint.
		/// The callback function signature takes in three (3) arguments:
		/// (1) Shared pointer to a ProductRxMetadata object. This should contain all information necessary for a skill
		/// to properly parse the data buffer provided by the second argument.
		/// (2) JobDataPointer, which is a std::variant that can be a pointer to an approved
		/// and defined data type for use on the Job Interface. This could be a MELComplex<T>, PDW, AMS VITA data packet, or other object.
		/// The JobDataPointer therefore refers to the starting address of a contiguous buffer of these objects/elements in memory.
		/// (3) The number of actual objects/elements in the buffer. NOT THE NUMBER OF BYTES!
		/// To get the number of bytes, you should be able to multiply this argument times the sizeof() an object pointed to by the JobDataPointer.
		/// @RequiredIfReceive This function passes registration data and must be
		/// provided by the implementer for all RF MEL implementations if the associated
		/// MFA supports Receive.
		virtual void setDataReadyCallback(std::function<void(std::shared_ptr<ProductRxMetadata>, JobDataPointer, size_t)>& cb) = 0;

	protected:
		ProductRxEndpoint() = default;
	};
} // namespace ams::iface::rfmel
