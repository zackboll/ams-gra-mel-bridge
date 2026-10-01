// Task 034B1: declaration-only probe of the published Admin/C2/VA roots.
// No provider object is constructed and no job operation is invoked.
#include <rfmel/factory/RFCreateFunctions.h>
#include <rfmel/admin/AdminMEL.h>
#include <rfmel/admin/UCI_Control.h>
#include <rfmel/admin/StatusControl.h>
#include <rfmel/c2/C2MEL.h>
#include <rfmel/c2/VirtualAperture.h>

#include <memory>
#include <string_view>
#include <type_traits>

namespace rfmel = ams::iface::rfmel;

static_assert(std::is_same_v<rfmel::fnAdminMEL,
                             std::shared_ptr<rfmel::AdminMEL> (*)(std::string_view)>);
static_assert(std::is_same_v<rfmel::fnC2MEL,
                             std::shared_ptr<rfmel::C2MEL> (*)(std::string_view)>);
static_assert(std::is_abstract_v<rfmel::AdminMEL>);
static_assert(std::is_abstract_v<rfmel::C2MEL>);
static_assert(std::is_abstract_v<rfmel::VirtualAperture>);

int rf_admin_c2_header_compile_probe()
{
    return 0;
}