#pragma once

#include <rfmel/c2/C2MELTypes.h>
#include <rfmel/jobs/DataPipe.h>
#include <rfmel/jobs/Pointing.h>
#include <rfmel/rfmeltypes/RFMELTypes.h>

#include <map>
#include <vector>
namespace ams::iface::rfmel
{
	/// @brief This class tracks all the relevant descriptive information for element group,
	/// including the label, mode, bandwidth, etc. as well as the DataPipe map
	/// @Required Implementation required for all RF MEL implementations. See individual class members for additional details
	class ElementGroupDescriptor
	{
	public:
		ElementGroupDescriptor() = default;
		explicit ElementGroupDescriptor(const ElementGroupLabel& /*newLabel*/, Mode /*newMode*/, double /*newMaxRFBandwidth*/, double /*newMaxSampleRate*/,
										double /*newMaxDataRate*/, std::map<DataPipeLabel, std::shared_ptr<DataPipe>> const& /*newDataPipes*/)
		{
		}
		virtual ~ElementGroupDescriptor() = default;
		ElementGroupDescriptor(const ElementGroupDescriptor&) = default;
		ElementGroupDescriptor(ElementGroupDescriptor&&) = default;
		ElementGroupDescriptor& operator=(const ElementGroupDescriptor&) = default;
		ElementGroupDescriptor& operator=(ElementGroupDescriptor&&) = default;

		/// @brief Gets label associated with this element group
		/// @Required Implementation required for all RF MEL implementations
		[[nodiscard]] virtual ElementGroupLabel getElementGroupLabel() const = 0;

		/// @brief Gets mode of element group
		/// @Required Implementation required for all RF MEL implementations
		[[nodiscard]] virtual Mode getMode() const = 0;

		/// @brief Gets Max Radio Frequency bandwidth of this ElementGoup in Hz.
		/// @Required Implementation required for all RF MEL implementations
		[[nodiscard]] virtual double getMaxRfBandwidth() const = 0;

		/// @brief Gets Max Sample Rate of this ElementGroupDescriptor in samples per second.
		/// @Required Implementation required for all RF MEL implementations
		[[nodiscard]] virtual double getMaxSampleRate() const = 0;

		/// @brief Gets Max Data Rate of this ElementGroupDescriptor in bits per second.
		/// @Required Implementation required for all RF MEL implementations
		[[nodiscard]] virtual double getMaxDataRate() const = 0;

		/// @brief Gets Max Duty Factor for this element group, which is on the interval 0 < maxDutyFactor <= 1, where 1 is CW.
		/// @Required Implementation required for all RF MEL implementations
		[[nodiscard]] virtual DutyFactor getMaxDutyFactor() const = 0;

		/// @brief Returns the map of DataPipe pointers, using DataPipeLabels as the key
		/// @RequiredIfEndpointAssociation Implementation required for all RF MEL implementations that use endpoint association
		[[nodiscard]] virtual std::map<DataPipeLabel, std::shared_ptr<DataPipe>> getDataPipes() const = 0;
	};
} // namespace ams::iface::rfmel
