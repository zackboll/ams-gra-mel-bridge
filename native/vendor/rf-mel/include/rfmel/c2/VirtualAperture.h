#pragma once

#include <rfmel/c2/C2MELTypes.h>
#include <rfmel/c2/ElementGroupCommand.h>
#include <rfmel/c2/ElementGroupConnections.h>
#include <rfmel/c2/ElementGroupDescriptorLookupMap.h>
#include <rfmel/jobs/LocalFunctionCommand.h>
#include <rfmel/rfmeltypes/RFMELTypes.h>
#include <rfmel/virtualaperture/BaseVirtualAperture.h>

#include <future>
#include <map>
#include <vector>
#include <memory>
#include <set>
#include <string>

namespace ams::iface::rfmel
{
	/// @brief Indicates whether a job was rejected, accepted and in progress,
	/// interrupted and failed, or complete.
	/// @Required This enumeration provides data definition associated with Jobs
	/// and must be included as-is in all RF MEL implementations
	enum class JobStatus
	{
		None,
		InProgress,
		Complete,
		FailedInvalidID,
		FailedInterrupted,
		FailedInvalidState
	};

	/// @brief Provides access to MFA resources, so that Skills may request jobs against the
	/// associated resources. Also reports status of the Virtual Aperture.
	/// @Required This class provides required RF MEL functionality for MFA resource access,
	/// and must be provided by the implementer in all RF MEL implementations. See individual
	/// members for details.
	class VirtualAperture : public BaseVirtualAperture
	{
	public:
		///////////////// Job-related Methods /////////////////

		/// @brief Returns a list of IDs Instances of this Virtual Aperture.
		/// @Required This function passes VA imstances IDs and must be provided by the implementer for all RF MEL implementations.
		[[nodiscard]] virtual std::set<ams::iface::rfmel::VirtualApertureInstanceID> getVAInstanceIDs() const = 0;

		/// @brief Returns a 'future' placeholder while the MFA processes the
		/// given job request. Once the future is made available, it holds either
		/// a JobDetail shared pointer or an enum describing why the job failed
		/// to get scheduled.
		/// @Required This function passes job request data and must be provided by the implementer for all RF MEL implementations.
		[[nodiscard]] virtual ams::iface::mel::RequestFor<JobDetail> requestJob(JobRequest &jobRequest) = 0;

		/// @brief Get list of ElementGroup Descriptors
		/// @Required Implementation required for all RF MEL implementations
		[[nodiscard]] virtual ElementGroupDescriptorLookupMap getElementGroups() const = 0;

		/// @brief Get list of ElementGroup Labels
		/// @Required Implementation required for all RF MEL implementations
		[[nodiscard]] virtual std::vector<ElementGroupLabel> getElementGroupLabels() const = 0;

		/// @brief Returns a map container of DataPipes mapped by Element Groups
		/// @RequiredIfEndpointAssociation Implementation required for all RF MEL implementations that use endpoint association
		[[nodiscard]] virtual ElementGroupConnections getDataPipes() = 0;

		/// @brief  Helper function to create ElementGroupCommand from an ElementGroupDescriptor
		/// @Required Implementation required for all RF MEL implementations
		[[nodiscard]] virtual std::shared_ptr<ElementGroupCommand> createElementGroupCommand(std::shared_ptr<ElementGroupDescriptor> egd) = 0;

		/// @brief Helper function to create ElementGroupCommand from an ElementGroupLabel
		/// @Required Implementation required for all RF MEL implementations
		[[nodiscard]] virtual std::shared_ptr<ElementGroupCommand> createElementGroupCommand(ElementGroupLabel egl) = 0;

		/// @brief returns true if the Virtual Aperture supports TransmitEvents which perform
		/// waveform modulation via the CachedWaveform object.
		/// @Required This function passes VA capability data and must be provided by the implementer for all RF MEL implementations.
		[[nodiscard]] virtual bool isCachedWaveformSupported() const = 0;

		/// @brief Retrieves a named set of weights defined at Virtual Aperture design time.
		/// @RequiredIfBeamTaperingWeights This function passes named weights data and must
		/// be provided by the implementer for all RF MEL implementations if the associated
		/// MFA supports controlling beam tapering of job events via setting aperture weights.
		[[nodiscard]] virtual std::shared_ptr<Weights> getStaticWeights(const std::string &name) const = 0;

		/// @brief Indicates whether 'createWeights()' is supported by this VA.
		/// @Required This function passes capability data and must be provided by the implementer for all RF MEL implementations.
		[[nodiscard]] virtual bool dynamicWeightsSupported() const = 0;

		/// @brief Allocates a set of weights to be used by any number of MFA Jobs.
		/// The types of weights supported are specific to each MFA. Some examples might include
		/// tapering at the element level, or combining subarrays to form monopulse channels.
		/// The caller is required to dynamic_cast the result into a type provided by the
		/// specific RF MEL implementation.
		/// @RequiredIfDynamicWeightsSupported This function passes weights data and must
		/// be provided by the implementer for all RF MEL implementations if the associated
		/// MFA supports controlling beam tapering of job events via dynamically setting aperture weights.
		[[nodiscard]] virtual std::shared_ptr<Weights> createWeights(const std::string &name, WeightType weight) = 0;

		///////////////// Element Group Methods /////////////////

		/// @brief Returns true if the Virtual Aperture contains zero or one.
		/// Tx Element Groups and zero or one Rx Element Groups.
		/// Such virtual apertures can omit the Element Group arguments
		/// for Job Requests.
		/// @Required This function passes Virtual Aperture data and must be provided by the implementer for all RF MEL implementations.
		[[nodiscard]] virtual bool isSingleGroup() const = 0;

		///////////////// Transmit Power Methods /////////////////

		/// @brief Effective Radiated Power (ERP) is the product of the peak
		/// radiated output power and the transmit aperture gain at the mainbeam
		/// scanned direction of (U,V).
		/// ERP(U,V) = Ppk * Gt(U,V), where Ppk is the peak radiated output
		/// power is the radiating element peak output power (array average)
		/// multiplied by the number of radiating elements in the aperature.
		/// Gt(U,V) is the transmit aperature gain measured in free space
		/// from the output of the transmit element. Transmit aperature gain
		/// includes RF ohmic loss, projection loss, element mismatch loss,
		/// and aperture efficiency and power loss due to transmit weighting.
		/// Tx Power Mode ID is defined by the MFA. Characteristics of the
		/// Tx Power Mode ID, including available Tx Attenuation ranges (db)
		/// and valid frequencies ranges can be retrieved via the RFMFAInfo class.

		/// @brief Returns the Effective Radiated Power (ERP) (dBW) during Transmit
		/// for a specified Transmit Element Group ID, Power Mode ID,
		/// Tx Attenuation (dB), Tx Weights, center Frequency (Hz),
		/// (u,v) stabilazation (aka pointing) vector of the antenna
		/// unit Line of Sight (LOS), and an optional VA Instance selection.
		/// @RequiredIfTransmit This function passes ERP data and
		/// must be provided by the implementer for all RF MEL implementations
		/// if the associated MFA supports transmit.
		[[nodiscard]] virtual double getTxRadiatedPower(size_t txElementGroupID, TxPowerModeID txPowerModeID, double txAttenuation_dB,
														WeightType txWeightType, double centerFreq_Hz, ams::iface::rfmel::AnglePair uvLineOfSight,
														VirtualApertureInstanceID vaInstanceID = 0) const = 0;

		/// @brief Returns the peak radiated output power during Transmit (Ppk) (dBW)
		/// during Transmit for a specified Transmit Element Group ID, Power Mode ID,
		/// Tx Attenuation (dB), center Frequency (Hz), and an optional VA Instance selection.
		/// @RequiredIfTransmit This function passes output power data and
		/// must be provided by the implementer for all RF MEL implementations
		/// if the associated MFA supports transmit.
		[[nodiscard]] virtual double getTxPeakRadiatedPower(size_t txElementGroupID, TxPowerModeID txPowerModeID, double txAttenuation_dB,
															double centerFreq_Hz, VirtualApertureInstanceID vaInstanceID = 0) const = 0;

		/// @brief Returns the transmit aperture gain, Gt(U,V) (dB), during Transmit
		/// for a specified Transmit Element Group ID, Power Mode ID, Tx Weights,
		/// Center Frequency (Hz), (u,v) stabilazation (aka pointing) vector of
		/// the antenna unit Line of Sight (LOS), and an optional VA Instance selection.
		/// @RequiredIfTransmit This function passes transmit data and
		/// must be provided by the implementer for all RF MEL implementations
		/// if the associated MFA supports transmit.
		[[nodiscard]] virtual double getTxApertureGain(size_t txElementGroupID, TxPowerModeID txPowerModeID, WeightType txWeightType,
													   double centerFreq_Hz, ams::iface::rfmel::AnglePair uvLineOfSight,
													   VirtualApertureInstanceID vaInstanceID = 0) const = 0;

		/// @brief Returns the available maximum Tx Attenuation
		/// for a specified Transmit Element Group ID, Power Mode ID,
		/// and an optional VA Instance selection. The maximum
		/// available attenuation may change at runtime based on real-time calibration.
		/// @RequiredIfTransmit This function passes transmit data and
		/// must be provided by the implementer for all RF MEL implementations
		/// if the associated MFA supports transmit.
		[[nodiscard]] virtual double getMaxTxAttenuation(size_t txElementGroupID, TxPowerModeID txPowerModeID,
														 VirtualApertureInstanceID vaInstanceID = 0) const = 0;

		///////////////// Local Function Methods /////////////////

		/// @brief Returns an associative container where each key value is
		/// the type ID of a LF associated with this VA, and the mapped
		/// value indicates the number of instances of the given type
		/// contained within each instance of the VA.
		/// @RequiredIfLFSupport This function passes container data and must be provided by the implementer
		/// for all RF MEL implementations if the associated MFA supports Local Functions.
		[[nodiscard]] virtual std::map<LocalFunctionTypeID, size_t> getLocalFunctions() const = 0;

		/// @brief Returns the status of each LF instance of the given type,
		/// associated with the given VA instance.
		/// Iterate over all vaInstanceID's in getAllInstances(), and all
		/// localFunctionID's in getLocalFunctions()'s key set, to obtain the
		/// status of every local function.
		/// @RequiredIfLFSupport This function passes LF status data and must be provided by the implementer
		/// for all RF MEL implementations if the associated MFA supports Local Functions.
		[[nodiscard]] virtual std::vector<VirtualApertureStatus> getLocalFunctionStatus(VirtualApertureInstanceID vaInstanceID,
																						LocalFunctionTypeID localFunctionID) const = 0;
	};

} // namespace ams::iface::rfmel
