/* IRMEL-ADA-011 declaration-only probe: the pinned SchedulingChannel.h and its
 * exact vendored dependency closure compile with the project warnings. It
 * implements no Scheduling operation and links into nothing. */
#include <irmel/library/scheduling/SchedulingChannel.h>

#include <type_traits>

namespace irmel = ams::iface::irmel;

static_assert(static_cast<unsigned>(irmel::ChannelType::Scheduling) == 3U);
static_assert(std::is_base_of_v<irmel::Channel, irmel::SchedulingChannel>);
static_assert(std::is_abstract_v<irmel::SchedulingChannel>);
static_assert(std::has_virtual_destructor_v<irmel::SchedulingChannel>);
static_assert(!std::is_copy_constructible_v<irmel::SchedulingChannel>);
