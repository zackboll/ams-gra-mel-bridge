// Task 034B1/034B2A: declaration-only probe of the published Admin/C2/VA/JobDetail roots.
// No provider object is constructed and no job operation is invoked.
#include <rfmel/factory/RFCreateFunctions.h>
#include <rfmel/admin/AdminMEL.h>
#include <rfmel/admin/UCI_Control.h>
#include <rfmel/admin/StatusControl.h>
#include <rfmel/c2/C2MEL.h>
#include <rfmel/c2/VirtualAperture.h>
#include <rfmel/c2/JobDetail.h>
#include <rfmel/mfa/PhysicalData.h>

#include <memory>
#include <string_view>
#include <type_traits>
#include <cstdint>
#include <chrono>

namespace rfmel = ams::iface::rfmel;

// Task 034C3: exact pinned const TxPowerModeData getter/representation surface.
using TxMode = const rfmel::TxPowerModeData&;
static_assert(std::is_same_v<rfmel::TxPowerModeID, std::uint32_t>);
static_assert(std::is_same_v<rfmel::TxPowerLevel, std::uint32_t>);
static_assert(std::is_same_v<rfmel::DutyFactor, double>);
static_assert(std::is_same_v<rfmel::Frequency, double>);
static_assert(std::is_same_v<std::chrono::nanoseconds::rep, std::int64_t>);
static_assert(std::is_integral_v<std::chrono::nanoseconds::rep>);
static_assert(std::is_signed_v<std::chrono::nanoseconds::rep>);
static_assert(std::is_same_v<decltype(std::declval<TxMode>().getTxPowerModeID()), std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<TxMode>().getIsLinearOperation()), bool>);
static_assert(std::is_same_v<decltype(std::declval<TxMode>().getTxPowerLevel()), std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<TxMode>().getTxFrequencyRanges(0)),
                             const std::vector<rfmel::FrequencyRange>&>);
static_assert(std::is_same_v<decltype(std::declval<TxMode>().getMaxTxDutyFactor()), double>);
static_assert(std::is_same_v<decltype(std::declval<TxMode>().getMaxTxPulseWidth()), std::chrono::nanoseconds>);
static_assert(std::is_same_v<decltype(std::declval<TxMode>().getMaxTxAtten()), double>);
static_assert(std::is_same_v<decltype(std::declval<TxMode>().getTxAttenStepSize()), double>);

static_assert(std::is_same_v<rfmel::fnAdminMEL,
                             std::shared_ptr<rfmel::AdminMEL> (*)(std::string_view)>);
static_assert(std::is_same_v<rfmel::fnC2MEL,
                             std::shared_ptr<rfmel::C2MEL> (*)(std::string_view)>);
static_assert(std::is_abstract_v<rfmel::AdminMEL>);
static_assert(std::is_abstract_v<rfmel::C2MEL>);
static_assert(std::is_abstract_v<rfmel::VirtualAperture>);
static_assert(std::is_abstract_v<rfmel::JobDetail>);

// Check the complete const getter surface consumed by the PhysicalData snapshot.
using Physical = const rfmel::PhysicalData&;
using Installation = decltype(std::declval<Physical>().getInstallationDetails());
using Location = decltype(std::declval<Installation>().getLocation());
using Key = decltype(std::declval<Location>().getLocationId());
using Orientation = decltype(std::declval<Installation>().getOrientation());
using Boresight = decltype(std::declval<Installation>().getBoresight());
static_assert(std::is_same_v<decltype(std::declval<Physical>().getAntennaHeight()), double>);
static_assert(std::is_same_v<decltype(std::declval<Physical>().getAntennaWidth()), double>);
static_assert(std::is_same_v<decltype(std::declval<Physical>().getLatticeAngle()), double>);
static_assert(std::is_same_v<decltype(std::declval<Location>().getOffsetX()), double>);
static_assert(std::is_same_v<decltype(std::declval<Location>().getOffsetY()), double>);
static_assert(std::is_same_v<decltype(std::declval<Location>().getOffsetZ()), double>);
static_assert(std::is_same_v<decltype(std::declval<Key>().getKey()), const std::string&>);
static_assert(std::is_same_v<decltype(std::declval<Key>().getSystemName()), const std::string&>);
static_assert(std::is_same_v<decltype(std::declval<Orientation>().getRoll()), double>);
static_assert(std::is_same_v<decltype(std::declval<Orientation>().getPitch()), double>);
static_assert(std::is_same_v<decltype(std::declval<Orientation>().getYaw()), double>);
static_assert(std::is_same_v<decltype(std::declval<Boresight>().getRoll()), double>);
static_assert(std::is_same_v<decltype(std::declval<Boresight>().getPitch()), double>);
static_assert(std::is_same_v<decltype(std::declval<Boresight>().getYaw()), double>);

int rf_admin_c2_header_compile_probe()
{
    return 0;
}
