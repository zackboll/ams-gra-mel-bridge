/// @file include/rfmel/rfmeltypes/RFMELTypes.h
/// @brief This header defines the elementary types used by the MEL, and forward-declares the composite types.
/// Elementary types and member variables adhere to snake_case naming conventions, while composite
/// types follow the UpperCamelCase convention.
/// All units in (meters, seconds, radians, hertz) unless otherwise specified.
/// Random IDs are given as uint64_t to minimize the chance for collisions without introducing the
/// complexity of multi-word, 128-bit UUIDs.
/// @note Note the use of Variant requires C++17 or newer.

#pragma once

#include <chrono>
#include <cinttypes>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <variant>
#include <vector>

#include <math/geometry/Geometry.h>
#include <math/units/UTCTime.h>

#include <mel/library/CommonMEL.h>

#include <rfmel/rfmeltypes/JobDataFormat.h>
#include <rfmel/rfmeltypes/MELComplex.h>
#include <array>
#include <cstdint>
#include <string>
#include <utility>

namespace ams::iface::rfmel
{
	/// @brief Uniquely identifies a single Virtual Aperture.
	/// @Required This definition is required to be included as-is in all RF MEL implementations.
	using VirtualApertureDefinitionID = uint32_t;
	/// @brief Uniquely identifies an instance of a Virtual Aperture.
	/// @Required This definition is required to be included as-is in all RF MEL implementations.
	using VirtualApertureInstanceID = uint32_t;
	/// @brief Uniquely identifies an Inhibit Virtual Aperture Request.
	/// @RequiredIfCosite This definition is required to be included as-is in all RF MEL implementations that support COSITE.
	using InhibitRequestID = uint32_t;
	/// @brief Uniquely identifies an MFA Face.
	/// @Required This definition is required to be included as-is in all RF MEL implementations.
	using FaceID = uint32_t;
	/// @brief address in memory on a device accessible via RDMA.
	/// @RequiredIfRDMA This definition is required to be included as-is in all RF MEL implementations that support RDMA.
	using VirtualAddress = uint64_t;
	/// @brief Uniquely identifies a transmitting/receiving endpoint on the ABB.
	/// @Required This definition is required to be included as-is in all RF MEL implementations.
	using EndpointID = uint64_t;
	/// @brief Names a DataPipe
	/// @Required This definition is required to be included as-is in all RF MEL implementations.
	using DataPipeLabel = std::string;
	/// @brief Names an ElementGroup
	/// @RequiredIfEndpointAssociation This definition is required to be included as-is in all RF MEL implementations that support Endpoint
	/// Association.
	using ElementGroupLabel = std::string;
	/// @brief Indicates priority, with lower values indicating greater urgency
	/// @Required This definition is required to be included as-is in all RF MEL implementations.
	using Priority = uint32_t;
	/// @brief Indicates precedence, with lower values indicating greater precedence
	/// @Required This definition is required to be included as-is in all RF MEL implementations.
	using PrecedenceWithinPriority = uint32_t;
	/// @brief Uniquely identifies a Data Stream
	/// @Required This definition is required to be included as-is in all RF MEL implementations.
	using StreamID = uint32_t;
	/// @brief Frequency, in Hertz
	/// @Required This definition is required to be included as-is in all RF MEL implementations.
	using Frequency = double;
	/// @brief Specifies a range of angles, in Radians. The format is (min,max), defined in positive angle direction.
	/// @Required This definition is required to be included as-is in all RF MEL implementations.
	using AnglePair = std::pair<double, double>;

	/// @brief Pair of frequencies which represent a range. First value is lower bound, second
	/// value is higher bound
	/// @Required Implementation required for all RF MEL implementations
	class FrequencyRange
	{
	public:
		explicit FrequencyRange(Frequency min_in = std::numeric_limits<Frequency>::min(), Frequency max_in = std::numeric_limits<Frequency>::max())
			: min(min_in), max(max_in)
		{
		}

		// getters and setters (min, max)
		void setMinFrequency(ams::iface::rfmel::Frequency min_in)
		{
			this->min = min_in;
		}
		[[nodiscard]] ams::iface::rfmel::Frequency getMinFrequency() const
		{
			return this->min;
		}
		void setMaxFrequency(ams::iface::rfmel::Frequency max_in)
		{
			this->max = max_in;
		}
		[[nodiscard]] ams::iface::rfmel::Frequency getMaxFrequency() const
		{
			return this->max;
		}

	private:
		ams::iface::rfmel::Frequency min;
		ams::iface::rfmel::Frequency max;
	};

	/// @brief MFA-specific type-id of the table of weights (e.g. elements, subarrays)
	/// @Required This definition is required to be included as-is in all RF MEL implementations.
	using WeightType = size_t;
	/// @brief MFA-specific type-id of the modulation function (e.g. linear FM)
	/// @Required This definition is required to be included as-is in all RF MEL implementations.
	using ModulationType = size_t;
	/// @brief Angle, in radians.
	/// @Required This definition is required to be included as-is in all RF MEL implementations.
	using Angle = double;
	/// @brief Uniquely identifies MFA-specific commandable Transmit Power Configurations
	/// @RequiredIfTransmit This definition is required to be included as-is in all RF MEL implementations which support transmit.
	using TxPowerModeID = uint32_t;
	/// @brief Uniquely identifies MFA-specific commandable Transmit Power Level, if applicable.
	/// (e.g. full power, low power, etc.)
	/// @note A value of ZERO is reserved as the default power level.
	/// @RequiredIfTransmit This definition is required to be included as-is in all RF MEL implementations which support transmit.
	using TxPowerLevel = uint32_t;
	/// @brief Uniquely identifies MFA-specific commandable duty factor, if applicable
	/// @Required This definition is required to be included as-is in all RF MEL implementations.
	using DutyFactor = double;
	/// @brief Uniquely identifies a Job Event within a Job Interval
	/// @Required This definition is required to be included as-is in all RF MEL implementations.
	using JobEventID = uint32_t;
	/// @brief Within a Virtual Aperture Instance, a Local Function Instance is
	/// uniquely identified by the combination of LocalFunctionTypeID and an
	/// Instance Number. The numbering of Local Function instances will start at
	/// 0 and go to numberInstances - 1 for each LocalFunctionTypeID within
	/// the Virtual Aperture Instance.
	/// To uniquely identify a Local Function in a VA instance will need both
	/// LocalFunctionTypeID and the Local Function Instance index.
	/// @RequiredIfLFSupport  This definition is required to be included as-is in all RF MEL implementations that support Local Functions.
	using LocalFunctionTypeID = uint32_t;
	/// @brief The value is expressed in radians.
	/// @RequiredIfBaselineRelativePointing This definition is required to be included as-is in all RF MEL implementations that support
	/// Baseline Relative Pointing.
	using Conic = double;
	/// @brief Stokes parameters that are used to command polarization.

	/// @Required This definition is required to be included as-is in all RF MEL implementations.
	using StokesVector = std::array<double, 4>;

	/// @brief Forward class declarations. See full definitions for documentation.
	/// @Required This definition is required to be included as-is in all RF MEL implementations.
	enum class VirtualApertureFailReason;
	/// @brief Forward class declarations. See full definitions for documentation.
	/// @Required This definition is required to be included as-is in all RF MEL implementations.
	enum class VirtualApertureStatus;
	/// @brief Forward class declarations. See full definitions for documentation.
	/// @Required This definition is required to be included as-is in all RF MEL implementations.
	enum class EndpointFail;
	/// @brief Forward class declarations. See full definitions for documentation.
	/// @Required This definition is required to be included as-is in all RF MEL implementations.
	enum class JobStatus;

	/// @brief Provides the interface to an OMS Adapter that isolates the MFA from
	/// the Abstract Service Bus (ASB).
	/// @Required This class provides required RF MEL functionality for providing
	/// an MFA adapter interface, and must be provided by the implementer in all
	/// RF MEL implementations. See individual members for details.
	class UCI;

	/// @brief Describes the parameters needed to setup an external Endpoint with an MFA when that Endpoint supports RDMA.
	/// If RDMA is unsupported, the reported region size will be zero.
	/// @RequiredIfRDMA This class provides data definition in support of required RF MEL functionality to setup an external
	/// Endpoint and must be included as-is in all RF MEL implementations if the associated MFA supports RDMA.
	class RDMAExternalEndpointParams;

	/// @brief Describes the attributes of a Memory Region used for communication with an MFA via an Endpoint object,
	/// when that Endpoint supports RDMA. If RDMA is unsupported, the reported region size will be zero.
	/// @RequiredIfRDMA This class provides data definition in support
	/// of required RF MEL functionality to discribe attributes of Memory Regions and must be
	/// included as-is in all RF MEL implementations if the associated
	/// MFA supports RDMA.
	class RDMAMemoryRegionParams;

	/// @brief Provides access to Virtual Apertures used by Skills to request time
	/// on resources within the MFA. Communicates with the MFA via the ABB API.
	/// @Required This class provides required RF MEL functionality for providing
	/// Virtual Apertures access, and must be provided by the implementer in all
	/// RF MEL implementations. See individual members for details.
	class RFMEL;

	/// @brief Provides a run-time interface by which to query for key characteristics of the
	/// connected MFA, such as supported RF bands, supported sample rates, and information
	/// about each MFA Faces.
	/// @Required This class provides required RF MEL functionality for MFA data requests,
	/// and must be provided by the implementer in all RF MEL implementations. See
	/// individual members for details.
	class RFMFAInfo;

	/// @brief The base class of MFA Face Physical Data including dimensions and mounting orientation.
	/// @Required This class provides data definition in support of required RF MEL functionality to provide
	/// mounting and/or orientation and must be included as-is in all RF MEL implementations.
	class PhysicalData;

	/// @brief The base class of MFA Tx Power Mode Data for a given Tx Power Mode ID.
	/// To prevent hardware damage, the Tx Power Mode ID is not to be used outside of
	/// of the listed characteristics.
	/// @RequiredIfTransmit This class provides data definition in support of required
	/// RF MEL functionality specific to transmit and must be included as-is in all RF
	/// MEL implementations if the associated MFA supports transmit.
	class TxPowerModeData;

	/// @brief Provides access to MFA resources, so that Skills may request jobs against the
	/// associated resources. Also reports status of the Virtual Aperture.
	/// @Required This class provides required RF MEL functionality for MFA resource access,
	/// and must be provided by the implementer in all RF MEL implementations. See individual
	/// members for details.
	class VirtualAperture;

	/// @brief Represents a concrete set of resources allocated by the VAS in the form of a VA Instance and a time interval, and
	/// allows commands to be issued against the set of resources. Each interval of the Job is specified by invoking the
	/// addJobIntervals() method. The finalize() method is invoked last, which causes any cached job data to
	/// be sent, and the MFA is notified that no more intervals will be produced.
	/// @Required This class provides required RF MEL functionality for supporting queries of the allocated time windows and the
	/// selected Virtual Apperture instance, and must be provided by the implementer in all RF MEL implementations. See members for details.
	class JobDetail;

	/// @brief Provides direct communication with the MFA over an abstracted communications bus,
	/// for both command and data packets.
	/// @RequiredIfDEA This class provides required RF MEL functionality for Direct Endpoint
	/// Access (DEA), and must be provided by the implementer in all RF MEL implementations if the
	/// associated MFA supports DEA. See individual members for details.
	class DirectAccessEndpoint;

	/// @brief Represents a communications channel to the MFA to an external endpoint.
	/// Class supports external endpoints for Rx, Tx and Direct Access.
	/// @Required This class provides data definition in support of required RF MEL functionality
	/// to support external endpoints and must be included as-is in all RF MEL implementations.
	class ExternalEndpoint;

	/// @RequiredIfReceive This class provides required RF MEL functionality for receive
	/// capabilities, and must be provided by the implementer in all RF MEL
	/// implementations if the associated MFA supports receive. See individual members for details.
	/// @brief Represents a communications channel from the MFA to the Service by which outputs from a job are received.
	/// If RDMA is supported, creates the Protection Domain / Memory Region, etc. to allow the MFA remote access to memory
	/// resident in the Skill's process space. It is assumed that any number of devices within the MFA may need to
	/// access this local memory region, so this class is responsible communicating with the infrastructure in the MFA for
	/// setting up all sockets, queue pairs, etc. necessary for any MFA device, selected by the Virtual Aperture Scheduler,
	/// to access the local memory region according to the commands in the MFA Job Request.
	class ProductRxEndpoint;

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
	class WaveformTxEndpoint;

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
	class CachedWaveform;

	/// @brief The base class of all Weights class specializations. Each RF MEL implementation is allowed to provide a
	/// different means of accomplishing beam tapering, etc. using a class that inherits from Weights.
	/// @RequiredIfBeamTaperingWeights This class provides data definition in support of required RF MEL functionality
	/// for all Weights class specializations and must be included as-is in all RF MEL implementations if the associated MFA
	/// supports controlling beam tapering of job events via setting aperture weights.
	class Weights;

	/// @brief Defines the ProductStreamParams type.
	/// @Required This enumeration provides data definition associated with Job Requests
	/// and must be included as-is in all RF MEL implementations.
	class ProductStreamParams;

	/// @brief Describes a source of waveform data residing within an application's memory local to a DPP, which is transferred
	/// to the MFA "just in time" for the AWG to translate the samples into a transmitted waveform. Depending on the MFA
	/// implementation, either the MFA or the DPP may act as the initiator of the transfer. See also: WaveformTxEndpoint
	/// @RequiredIfWaveformStreamSupport This class provides data definition in support of required RF MEL functionality
	/// for describing waveform data sources and must be included as-is in all RF MEL implementations if the associated
	/// MFA supports Waveform streams.
	class WaveformStream;

	/// @brief The JobEvent is the lowest level of control a Service can command on the MFA.
	/// See also ams::iface::rfmel::TransmitEvent and ams::iface::rfmel::ReceiveEvent.
	/// @Required This class provides required RF MEL functionality for supporting Job Events,
	/// and must be provided by the implementer in all RF MEL implementations.
	/// See individual members for details.
	class JobEvent;

	/// @brief Describes how a subset of the Virtual Aperture is requested to be used,
	/// including frequency, bandwidth, and pointing.
	/// @note Every JobRequest must specify at least one ElementGroup.
	/// @Required This class provides data definition in support of Virtual Apertures and the Jobs Interface, which is
	/// required RF MEL functionality and must be included as-is in all RF MEL implementations.
	class JobRequest;

	/// @brief The JobInterval describes how a Sequence is to be executed, including its loop cout, and any padding time required
	/// before or after the loop executes. If a Virtual Aperture contains multiple Tx/Rx element groups, then a JobInterval may
	/// target one or more of the element groups. JobIntervals targeting distinct element groups may be commanded
	/// to execute in parallel by setting the same interval_start parameter.
	/// @Required This class provides data definition in support of required RF MEL functionality to support Job intervals and must
	/// be included as-is in all RF MEL implementations. See individual class members for additional details.
	class JobInterval;

	/// @brief Indicates the completion status of a job interval based on the duration of its events.
	/// and logs the trigger type of conditional job events.
	/// @Required This class provides data definition in support of required RF MEL functionality
	/// to support Job interval completion status and must be included as-is in all RF MEL implementations.
	class JobIntervalStatus;

	/// @brief Specifies a point in space given in Earth-Centered, Earth-Fixed coordinate system (meters).
	/// @RequiredIfECEFPointing This class provides data definition in support
	/// of required RF MEL functionality specific to Geolocation and must be
	/// included as-is in all RF MEL implementations if the associated
	/// MFA supports ECEFPointing.
	class ECEFPointing;
	/// @brief Specifies a point in space given in WGS-84 coordinates (radians and meters).
	/// @RequiredIfLLAPointing This class provides data definition in support
	/// of required RF MEL functionality specific to Geolocation and must be
	/// included as-is in all RF MEL implementations if the associated
	/// MFA supports LLAPointing.
	class LLAPointing;
	/// @brief Specifies a pointing vector relative to the platform of reference (radians).
	/// @RequiredIfPlatformRelativePointing This class provides data definition in support
	/// of required RF MEL functionality specific to Geolocation and must be
	/// included as-is in all RF MEL implementations if the associated
	/// MFA supports PlatformRelativePointing.
	class PlatformRelativePointing;
	/// @brief Specifies a pointing vector relative to the face of the aperture (radians).
	/// @Required This class provides data definition in support of required RF MEL functionality to provide
	/// pointing vectors and must be included as-is in all RF MEL implementations.
	class FaceRelativePointing;
	/// @brief Specifies a pointing vector relative to the baseline of the aperture (radians).
	/// @RequiredIfBaselineRelativePointing This class provides data definition in support
	/// of required RF MEL functionality specific to pointing vectors and must be
	/// included as-is in all RF MEL implementations if the associated
	/// MFA supports pointing vectors relative to the baseline of the aperture in radians.
	class BaselineRelativePointing;

	/// @brief A general pointing type, which may be represented in ECEF, LLA, Platform Relative, Face Relative,
	/// and/or Baseline Relative format. See individual class defintiions for details.
	/// @Required This definition is required to be included as-is in all RF MEL implementations.
	using PointingType = std::variant<ECEFPointing, LLAPointing, PlatformRelativePointing, FaceRelativePointing, BaselineRelativePointing>;

	/// @brief Provides RX/TX switching
	/// @Required Implementation required for all RF MEL implementations
	enum class Mode : uint8_t
	{
		RX = 0,
		TX = 1
	};

	/// @brief Data Sample Format specifier from VITA-49.
	/// @RequiredIfWaveformStreamSupport This enumeration provides data definition associated with waveform streaming
	/// and must be included as-is in all RF MEL implementations for RF MFAs that support waveform streams for transmit jobs.
	enum class DataSampleFormat
	{
		Real,
		ComplexCartesian,
		ComplexPolar
	};

	/// @brief Indicates the type of event.
	/// @Required This enumeration provides data definition associated with Job Events
	/// and must be included as-is in all RF MEL implementations.
	enum class ExecutionType
	{
		Normal,		/// [default] Event execution is not tied to a specific conditional trigger
		Conditional /// Event is conditional and will only be executed when the conditions to execute the event are met
	};

	/// @brief Data Item format specifier from VITA-49.
	/// @RequiredIfWaveformStreamSupport This enumeration provides data definition associated with waveform streaming
	/// and must be included as-is in all RF MEL implementations for RF MFAs that support waveform streams for transmit jobs.
	enum class DataItemFormat
	{
		FixedPointUnsigned,
		FixedPointSigned,
		// See VITA-49.2 Appendix D for VRT floating-point formats.
		VRT_unsignedFloat,
		VRT_signedFloat,
		// IEEE 754
		Float32,
		Float64,
		Float16,
		// See VITA-49.2 spectral formats:
		FixedPointUnsignedNonNormalized,
		FixedPointSignedNonNormalized
	};

	/// @brief Indicates the allowable format of data packets output from a virtual aperture
	/// @Required This enumeration provides data definition associated with Virtual Apertures
	/// and must be included as-is in all RF MEL implementations
	enum class DataPacketType
	{
		IQ,
		Real,
		PDW,
		Other
	};

	/// @brief Indicates remote access restrictions to a local resource.
	/// @RequiredIfRDMA This enumeration provides data definition associated with RDMA
	/// and must be included as-is in all RF MEL implementations for RF MFAs that support RDMA.
	enum class RemoteAccessType
	{
		None = 0,
		Read = 1,
		Write = 2,
		ReadWrite = 3
	};

	/// @brief Indicates how a given buffer should be iterated over, whether treated as
	/// a single piece of data repeated in a loop, or sequential data to be incrementally
	/// processed on each iteration of the loop.
	/// @RequiredIfWaveformStreamSupport This enumeration provides data definition associated with waveform streaming
	/// and must be included as-is in all RF MEL implementations for RF MFAs that support waveform streams for transmit jobs.
	enum class BufferRepeatMode
	{
		RepeatFromTop,
		ContinueFromPrevious
	};

	/// @brief Indicates the completion status of a job interval
	/// @Required This enumeration provides data definition associated with Jobs
	/// and must be included as-is in all RF MEL implementations
	enum class JobIntervalCompletionStatus
	{
		None,
		ReadyForNextJobInterval,
		FailedInterrupted,
		FailedInvalidTxEventSpatialData,
		FailedInvalidTxEventSignalData,
		FailedInvalidTxEventTemporalData,
		FailedInvalidTxEventIdentifierData,
		FailedInvalidRxEventSpatialData,
		FailedInvalidRxEventSignalData,
		FailedInvalidRxEventTemporalData,
		FailedInvalidRxEventIdentifierData,
		FailedInvalidSequenceTemporalData,
		FailedInvalidJobIntervalSpatialData,
		FailedInvalidJobIntervalEventSignalData,
		FailedInvalidJobIntervalEventTemporalData,
		FailedInvalidJobIntervalEventIdentifierData,
		FailedInvalidJobTemporalData,
		FailedInvalidJobIdentifierData,
		Completed,
		Cancelled,
		lateControls,
		invalidControls,
		antennaFovError,
		TransmitRfInhibited,
		Started
	};

	/// @brief Indicates the reason for triggering a job event log
	/// @Required This enumeration provides data definition associated with Jobs
	/// and must be included as-is in all RF MEL implementations
	enum class JobEventLogTriggerType
	{
		None,
		eventExtended,
		eventTriggered,
		eventResumed,
		eventCancelled,
		eventInhibited,
		eventDelayedStart,
		eventTypeNotSupported
	};

	/// @brief Indicates when a job interval status is given
	/// @Required This enumeration provides data definition associated with Jobs
	/// and must be included as-is in all RF MEL implementations
	enum class JobIntervalStatusEnable
	{
		Never,
		Always,
		OnException
	};

} // namespace ams::iface::rfmel
