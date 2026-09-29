#pragma once

#include <rfmel/rfmeltypes/RFMELTypes.h>

namespace ams::iface::rfmel
{
	/// @brief Describes the attributes of a Memory Region used for communication with an MFA via an Endpoint object,
	/// when that Endpoint supports RDMA. If RDMA is unsupported, the reported region size will be zero.
	/// @RequiredIfRDMA This class provides data definition in support
	/// of required RF MEL functionality to discribe attributes of Memory Regions and must be
	/// included as-is in all RF MEL implementations if the associated
	/// MFA supports RDMA.
	class RDMAMemoryRegionParams
	{
	public:
		RDMAMemoryRegionParams() = default;
		~RDMAMemoryRegionParams() = default;
		RDMAMemoryRegionParams(const RDMAMemoryRegionParams&) = default;
		RDMAMemoryRegionParams(RDMAMemoryRegionParams&&) = default;
		RDMAMemoryRegionParams& operator=(const RDMAMemoryRegionParams&) = default;
		RDMAMemoryRegionParams& operator=(RDMAMemoryRegionParams&&) = default;

		/// @brief returns the size of the memory region owned by this Endpoint
		[[nodiscard]] size_t getMemoryRegionSize() const
		{
			return regionSizeBytes;
		}
		/// @brief returns the local address of the memory region owned by this Endpoint
		[[nodiscard]] char* getMemoryRegion() const
		{
			return memory;
		}
		/// @brief returns the virtual address used for remote access to this memory.
		[[nodiscard]] VirtualAddress getVirtualAddress() const
		{
			return DummyAddress;
		}
		/// @brief returns the access restrictions of the memory region owned by this Endpoint.
		[[nodiscard]] RemoteAccessType getAccessType() const
		{
			return accessType;
		}

		/// @brief Sets the size of the memory region owned by this Endpoint.
		void setMemoryRegionSize(size_t rSize)
		{
			this->regionSizeBytes = rSize;
		}
		/// @brief Sets the local address of the memory region owned by this Endpoint.
		void setMemoryRegion(char* address)
		{
			this->memory = address;
		}
		/// @brief Sets the virtual address used for remote access to this memory.
		void setVirtualAddress(VirtualAddress virtual_address)
		{
			this->DummyAddress = virtual_address;
		}
		/// @brief Sets the access restrictions of the memory region owned by this Endpoint.
		void setAccessType(RemoteAccessType access_type)
		{
			this->accessType = access_type;
		}

	private:
		// Points to the memory exposed to the memory region, which may be heap memory
		// owned by the Endpoint, or other types of memory managed elsewhere.
		char* memory{nullptr};

		size_t regionSizeBytes{0};

		RemoteAccessType accessType = RemoteAccessType::ReadWrite;

		VirtualAddress DummyAddress{0};
	};
} // namespace ams::iface::rfmel
