#pragma once

#include <rfmel/rfmeltypes/RFMELTypes.h>
#include <rfmel/virtualaperture/VirtualApertureStatusReport.h>
#include <map>
#include <functional>
#include <vector>

namespace ams::iface::rfmel
{
	/// @brief Defines the BaseVirtualAperture type.
	/// @Required This class provides required RF MEL functionality for VA aperture definition,
	/// and must be provided by the implementer in all RF MEL implementations. See individual members for details.
	class BaseVirtualAperture
	{
	public:
		// Allows notifying Skills that a change in status has occurred.

		///////////////// Status / Instance Methods /////////////////
		BaseVirtualAperture() = default;
		virtual ~BaseVirtualAperture() = default;
		BaseVirtualAperture(const BaseVirtualAperture &) = default;
		BaseVirtualAperture(BaseVirtualAperture &&) = default;
		BaseVirtualAperture &operator=(const BaseVirtualAperture &) = default;
		BaseVirtualAperture &operator=(BaseVirtualAperture &&) = default;

		/// @brief Returns the Virtual Aperture ID associated with this object.
		/// This ID will be implicitly included in all job requests.
		/// @Required This function passes VA Apeture IDs and must be provided by the implementer for all RF MEL implementations.
		[[nodiscard]] virtual VirtualApertureDefinitionID getID() const = 0;

		/// @brief Registers a Skill to receive notice of status changes.
		/// @Required This function Registers a Skill and must be provided by the implementer for all RF MEL implementations.
		[[nodiscard]] virtual size_t addStatusCallback(const std::function<void(BaseVirtualAperture &)> &callback) = 0;

		/// @brief De-registers a Skill previously registered via addStatusCallback.
		/// @Required This function De-registers a Skill and must be provided by the implementer for all RF MEL implementations.
		virtual void removeStatusCallback(size_t key) = 0;

		/// @brief Returns the status of this Virtual Aperture.
		/// @Required This function passes VA Status and must be provided by the implementer for all RF MEL implementations.
		[[nodiscard]] virtual VirtualApertureStatus getStatus() const = 0;

		/// @brief Returns the status of the given instance of this Virtual Aperture.
		/// @Required This function passes VA Status and must be provided by the implementer for all RF MEL implementations.
		[[nodiscard]] virtual VirtualApertureStatus getInstanceStatus(VirtualApertureInstanceID vaInstanceID) const = 0;

		/// @brief Returns the list of all instances of this Virtual Aperture, each of
		/// which represents a specific set of resources consumed by a job request.
		/// @Required This function passes VA imstances list and must be provided by the implementer for all RF MEL implementations.
		[[nodiscard]] virtual std::vector<VirtualApertureInstanceID> getAllInstances() const = 0;

		/// @brief Returns the list of all instances of this Virtual Aperture that
		/// reside on the given MFA face.
		/// @Required This function passes VA instances list and must be provided by the implementer for all RF MEL implementations.
		[[nodiscard]] virtual std::vector<VirtualApertureInstanceID> getInstances(FaceID faceIndex) const = 0;

		/// @brief Get Virtual Aperture Instance Status Reports.
		/// @Required This function passes VA imstance status and must be provided by the implementer for all RF MEL implementations.
		[[nodiscard]] virtual VirtualApertureInstanceStatusReport getInstanceStatusReport(VirtualApertureInstanceID vaInstanceID) const = 0;
	};
} // namespace ams::iface::rfmel
