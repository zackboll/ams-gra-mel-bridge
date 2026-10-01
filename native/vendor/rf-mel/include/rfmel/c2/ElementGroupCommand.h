#pragma once

#include <rfmel/c2/C2MELTypes.h>
#include <rfmel/c2/ElementGroupDescriptor.h>
#include <rfmel/jobs/DataPipeConnections.h>
#include <rfmel/jobs/Pointing.h>
#include <rfmel/rfmeltypes/RFMELTypes.h>

#include <set>
#include <vector>
namespace ams::iface::rfmel
{
	/// @brief defines functions that allow a Skill to command an element group and receive data back on an endpoint
	/// @Required Implementation required for all RF MEL implementations. See individual class members for additional details
	class ElementGroupCommand
	{
	public:
		ElementGroupCommand() = default;
		virtual ~ElementGroupCommand() = default;
		ElementGroupCommand(const ElementGroupCommand&) = default;
		ElementGroupCommand(ElementGroupCommand&&) = default;
		ElementGroupCommand& operator=(const ElementGroupCommand&) = default;
		ElementGroupCommand& operator=(ElementGroupCommand&&) = default;

		/// @brief Gets Center Frequency of this ElementGroupCommand in Hz.
		/// @Required Implementation required for all RF MEL implementations
		[[nodiscard]] virtual ElementGroupLabel getElementGroupLabel() const = 0;

		/// @brief Gets the Mode for Element Group
		/// @Required Implementation required for all RF MEL implementations
		[[nodiscard]] virtual Mode getMode() const = 0;

		/// @brief Gets a vector of  Exepected Centerfrequencies in Hz
		/// @Required Implementation required for all RF MEL implementations
		[[nodiscard]] virtual std::vector<FrequencyRange> getExpectedCenterFrequencies() const = 0;

		/// @brief Gets vector of pointers to Exepected Centerfrequencies  in Hz
		/// @Required Implementation required for all RF MEL implementations
		[[nodiscard]] virtual std::vector<FrequencyRange>& getRefExpectedCenterFrequencies() = 0;

		/// @brief Gets Transmit power from Tx element groups in Watts.
		/// @RequiredIfTransmit Implementation required if the MFA supports transmit
		[[nodiscard]] virtual TxPowerLevel getTxPower() const = 0;

		/// @brief Gets Max Duty Factor for this element group, which is on the interval 0 < maxDutyFactor <= 1, where 1 is CW.
		/// @Required Implementation required for all RF MEL implementations
		[[nodiscard]] virtual DutyFactor getDesiredDutyFactor() const = 0;

		/// @brief Get the list of endpoints which will receive the products output by this job.
		/// The required quantity of Endpoints is specified by the VirtualAperture.
		/// @RequiredIfReceive Implementation required if the MFA supports receive
		/// @RequiredIfEndpointAssociation Implementation required for all RF MEL implementations that use endpoint association
		[[nodiscard]] virtual DataPipeConnections getEndpointIDs() const = 0;

		/// @brief Gets the list of pointers to endpoint ids which will receive the products output by this job.
		/// @RequiredIfEndpointAssociation Implementation required for all RF MEL implementations that use endpoint association
		[[nodiscard]] virtual DataPipeConnections& getRefEndpointIDs() = 0;

		/// @brief Sets Center Frequency for this ElementGroupCommand in Hz.
		/// @Required Implementation required for all RF MEL implementations
		virtual void addExpectedCenterFrequencies(FrequencyRange freq_range) = 0;

		/// @brief Sets Transmit power from Tx element groups in Watts.
		/// @RequiredIfTransmit Implementation required if the MFA supports transmit
		virtual void setTxPower(TxPowerLevel txPower) = 0;

		/// @brief Gets Max Duty Factor.
		/// @Required Implementation required for all RF MEL implementations
		virtual void setDesiredDutyFactor(DutyFactor dutyFactor) = 0;

		/// @brief Set the list of endpoints which will receive the products output by this job.
		/// The required quantity of Endpoints is specified by the VirtualAperture.
		/// @RequiredIfReceive Implementation required if the MFA supports receive
		virtual void addEndpointIDs(const std::set<EndpointID>& endpointIDs, DataPipeLabel pdLable = DataPipe::DEFAULT) = 0;

		/// @brief Get the expected PointingType ECEFPointing, LLAPointing,
		/// PlatformRelativePointing, FaceRelativePointing, or BaselineRelativePointing
		/// @RequiredIfReceive Implementation required if the MFA supports receive
		virtual std::vector<PointingType> getExpectedPointingAngles() = 0;

		/// @brief Set Expected PointingType ECEFPointing, LLAPointing,
		/// PlatformRelativePointing, FaceRelativePointing, or BaselineRelativePointing
		/// @RequiredIfReceive Implementation required if the MFA supports receive
		virtual void addExpectedPointingAngle(PointingType const& newPoint) = 0;

		/// @brief Get vector of pointers of expected Pointing Types PointingType ECEFPointing, LLAPointing,
		/// PlatformRelativePointing, FaceRelativePointing, or BaselineRelativePointing
		/// @RequiredIfReceive Implementation required if the MFA supports receive
		virtual std::vector<PointingType>& getRefExpectedPointingAngles() = 0;
	};
} // namespace ams::iface::rfmel
