/// @file include/rfmel/rfmeltypes/JobDataFormat.h
/// @brief This header defines the elementary types used by the Jobs-Based Protocol.
/// @note Note the use of Variant and AMS VITA packet storage requires C++20 or newer.

#pragma once

#include <chrono>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <variant>
#include <vector>

#include <ams/iface/vita/FixedDataPacket.h>
#include <rfmel/rfmeltypes/MELComplex.h>

namespace ams::iface::rfmel
{
	/// @brief This enumeration provides a 1-to-1 mapping to the types listed in the JobDataPointer variant.
	/// The JobDataFormat is used by the service when constructing an Endpoint, to assign that Endpoint to only
	/// be used for receiving or transmitting that particular data type.
	/// @Required This enumeration provides data definition associated with Jobs
	/// and must be included as-is in all RF MEL implementations
	enum class JobDataFormat
	{
		DirectINT8,		   ///< Direct sampling with signed 8 bit integers (int8_t)
		DirectINT16,	   ///< Direct sampling with signed 16 bit integers (int16_t)
		ComplexINT8,	   ///< Complex sampling (IQ, real/imaginary) with signed 8 bit integer pairs (int8_t, int8_t)
		ComplexINT16,	   ///< Complex sampling (IQ, real/imaginary) with signed 8 bit integer pairs (int16_t, int16_t)
		AMSVitaSmall,	   ///< AMS GRA VITA49.2 "Base Set" Signal Data Packet (Small,  512 byte packet)
		AMSVitaMedium,	   ///< AMS GRA VITA49.2 "Base Set" Signal Data Packet (Medium, 1024 byte packet)
		AMSVitaLarge,	   ///< AMS GRA VITA49.2 "Base Set" Signal Data Packet (Large,  8192 byte packet)
		AMSVitaExtraLarge, ///< AMS GRA VITA49.2 "Base Set" Signal Data Packet (Extra Large, 32768 byte packet)
		PDWType1,		   ///< Indicates a PDW type definition (type 1)
		PDWType2,		   ///< Indicates a PDW type definition (type 2)
		PDWType3,		   ///< Indicates a PDW type definition (type 3)
		LFType1,		   ///< Indicates a Local Function defined type (type 1)
		LFType2,		   ///< Indicates a Local Function defined type (type 2)
		LFType3			   ///< Indicates a Local Function defined type (type 3)
	};

	constexpr size_t Small = 512;       ///< Small AMS VITA packet storage capacity, in bytes.
	constexpr size_t Medium = 1024;     ///< Medium AMS VITA packet storage capacity, in bytes.
	constexpr size_t Large = 8192;      ///< Large AMS VITA packet storage capacity, in bytes.
	constexpr size_t ExtraLarge = 32768; ///< Extra-large AMS VITA packet storage capacity, in bytes.

	constexpr size_t AmsVitaDataPacketOverheadBytes = 32; ///< Seven-word prologue plus one-word trailer.
	template <size_t PacketBytes>
	constexpr size_t AmsVitaDataPacketPayloadBytes = PacketBytes - AmsVitaDataPacketOverheadBytes;

	/// @name DataPacket
	/// @brief The Common RF MEL provides four allowable AMS VITA data packet storage capacities over the Jobs interface.
	/// OEMs and skills can choose the most suitable capacity for their use case. For the complete
	/// AMS GRA VITA 49.2 tailoring, see the vendored AMS VITA specification source.
	///
	/// The storage capacity is not the valid serialized packet length. AMS VITA packet length is
	/// encoded in the packet header and read through `ams::iface::vita::DataPacketView::getPacketSize()`.
	///
	/// For reference, the AMS VITA data packet storage overhead is:
	/// - Seven-word prologue: 28 bytes.
	/// - Mandatory one-word trailer: 4 bytes.
	/// - Total non-payload overhead: 32 bytes.
	///
	/// Packet storage capacities and maximum payload sizes:
	/// - Small:        512 bytes storage, 480 bytes payload (120 words).
	/// - Medium:      1024 bytes storage, 992 bytes payload (248 words).
	/// - Large:       8192 bytes storage, 8160 bytes payload (2040 words).
	/// - Extra Large: 32768 bytes storage, 32736 bytes payload (8184 words).

	///@{
	/// @brief Defines small AMS VITA packet storage for use with the Jobs Interface.
	/// @Required This definition is required to be included as-is in all RF MEL implementations.
	using DataPacketSmall = ams::iface::vita::FixedDataPacket<Small>;
	/// @brief Defines medium AMS VITA packet storage for use with the Jobs Interface.
	/// @Required This definition is required to be included as-is in all RF MEL implementations.
	using DataPacketMedium = ams::iface::vita::FixedDataPacket<Medium>;
	/// @brief Defines large AMS VITA packet storage for use with the Jobs Interface.
	/// @Required This definition is required to be included as-is in all RF MEL implementations.
	using DataPacketLarge = ams::iface::vita::FixedDataPacket<Large>;
	/// @brief Defines extra-large AMS VITA packet storage for use with the Jobs Interface.
	/// @Required This definition is required to be included as-is in all RF MEL implementations.
	using DataPacketExtraLarge = ams::iface::vita::FixedDataPacket<ExtraLarge>;
	///@}

	// Compile-time checks that RF MEL packet storage capacities and object sizes match.
	static_assert(DataPacketSmall::byte_capacity == Small, "DataPacketSmall storage capacity is not specified size!");
	static_assert(DataPacketMedium::byte_capacity == Medium, "DataPacketMedium storage capacity is not specified size!");
	static_assert(DataPacketLarge::byte_capacity == Large, "DataPacketLarge storage capacity is not specified size!");
	static_assert(DataPacketExtraLarge::byte_capacity == ExtraLarge, "DataPacketExtraLarge storage capacity is not specified size!");
	static_assert(sizeof(DataPacketSmall) == Small, "DataPacketSmall object size is not specified size!");
	static_assert(sizeof(DataPacketMedium) == Medium, "DataPacketMedium object size is not specified size!");
	static_assert(sizeof(DataPacketLarge) == Large, "DataPacketLarge object size is not specified size!");
	static_assert(sizeof(DataPacketExtraLarge) == ExtraLarge, "DataPacketExtraLarge object size is not specified size!");

	// clang-format off
	/// @brief The JobDataPointer variant holds pointers to the actual defined types described by the MEL,
	/// and included from files above. JobDataPointer is used to enforce the pointer on the DataReadyCallback
	/// of a receive endpoint in the Jobs interface. AMS VITA alternatives point to packet storage buffers;
	/// callers must use AMS VITA views to read the header-derived valid packet length. Any data sent or
	/// received over the Jobs Interface between a service and MFA implementation must have its class
	/// equivalent type defined in the MEL. That type must also be included here in the variant to be usable
	/// on the endpoint.
	/// @Required This enumeration provides data definition associated with Jobs
	/// and must be included as-is in all RF MEL implementations
	/// @note Additional pointers to objects such as PDW's and LF's of various types should be added here as
	/// they get defined and added to the RF MEL.
	using JobDataPointer = std::variant<int8_t*,			  	// DirectINT8
										int16_t*,			  	// DirectINT16
										MELComplex<int8_t>*,  	// MELComplexINT8
										MELComplex<int16_t>*,  	// MELComplexINT16
										DataPacketSmall*, 		// AMS GRA VITA49.2 "Base Set" Signal Data Packet (Small)
										DataPacketMedium*,		// AMS GRA VITA49.2 "Base Set" Signal Data Packet (Medium)
										DataPacketLarge*, 		// AMS GRA VITA49.2 "Base Set" Signal Data Packet (Large)
										DataPacketExtraLarge* 	// AMS GRA VITA49.2 "Base Set" Signal Data Packet (Extra Large)
										>;
	// clang-format on

	/// @brief Defines a shared pointer to a vector of MELComplex 8-bit integers. This is meant to represent a JobTransmitVector
	/// that contains the 8-bit alternative
	/// @RequiredIfTransmit This variant describes a definiton for detecting a variant alternative in a Jobs transmit buffer
	using ComplexI8VecPtr = std::shared_ptr<std::vector<MELComplex<int8_t>>>;

	/// @brief Defines a shared pointer to a vector of MELComplex 16-bit integers. This is meant to represent a JobTransmitVector
	/// that contains the 16-bit alternative
	/// @RequiredIfTransmit This variant describes a definiton for detecting a variant alternative in a Jobs transmit buffer
	using ComplexI16VecPtr = std::shared_ptr<std::vector<MELComplex<int16_t>>>;

	/// @brief The JobTransmitVector variant holds a pointer to a vector of the data types allowed in
	/// a transmit job. The types are limited to complex cartesian 8- and 16-bit integers
	/// @RequiredIfTransmit This variant describes a tansmit data definition associates with Jobs
	using JobTransmitVector = std::variant<ComplexI8VecPtr, ComplexI16VecPtr>;

} // namespace ams::iface::rfmel
