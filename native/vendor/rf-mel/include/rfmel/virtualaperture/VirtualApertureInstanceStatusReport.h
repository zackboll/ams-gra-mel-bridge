#pragma once

#include <rfmel/rfmeltypes/RFMELTypes.h>
#include <map>
#include <vector>
namespace ams::iface::rfmel
{
	/// @brief Indicates whether the Virtual Aperture is nominally operational,
	/// degraded, or failed.
	/// @Required This enumeration provides data definition associated with Virtual Apertures
	/// and must be included as-is in all RF MEL implementations
	enum class VirtualApertureStatus
	{
		None,
		Operational,
		Degraded,
		Failed
	};

	/// @brief Reports the status of a single instance of a given VA,
	/// including all of its local functions.
	/// @Required This class provides data definition in support of required RF MEL functionality
	/// for VA Status reporting and must be included as-is in all RF MEL implementations.
	/// See individual class members for additional details.
	class VirtualApertureInstanceStatusReport
	{
	public:
		VirtualApertureInstanceStatusReport() = default;

		/// @brief Get virtual aperture instance.
		[[nodiscard]] auto getVAInstanceID() const
		{
			return this->virtualApertureInstanceID;
		}
		/// @brief Get virtual aperture status.
		[[nodiscard]] auto getStatus() const
		{
			return this->status;
		}
		/// @brief Get local function status for each type of LF used by the
		/// VA, a status for each instance of that LF owned by this VA instance.
		[[nodiscard]] auto getLFStatus() const
		{
			return this->lfStatus;
		}

		/// @brief Set virtual aperture instance.
		void setVAInstanceID(VirtualApertureInstanceID virtualApertureInstanceID_in)
		{
			this->virtualApertureInstanceID = virtualApertureInstanceID_in;
		}
		/// @brief Set virtual aperture status.
		void setStatus(VirtualApertureStatus status_in)
		{
			this->status = status_in;
		}
		/// @brief Set local function status.
		/// for each type of LF used by the VA, a status for each instance
		/// of that LF owned by this VA instance.
		void setLFStatus(const std::map<LocalFunctionTypeID, std::vector<VirtualApertureStatus>> &lfStatus_in)
		{
			this->lfStatus = lfStatus_in;
		}

	private:
		VirtualApertureInstanceID virtualApertureInstanceID{0};
		VirtualApertureStatus status = VirtualApertureStatus::None;

		// For each type of LF used by the VA, a status for each instance
		// of that LF owned by this VA instance.
		std::map<LocalFunctionTypeID, std::vector<VirtualApertureStatus>> lfStatus;
	};
} // namespace ams::iface::rfmel
