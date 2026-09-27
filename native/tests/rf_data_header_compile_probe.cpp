// Task 033A declaration-only probe for the pinned RF MEL DataMEL contract.
// It compiles the selected published declaration roots and implements no RF
// behavior: no provider object is instantiated and no RF type is faked.
#include <rfmel/factory/RFCreateFunctions.h>
#include <rfmel/rfmeltypes/RFMEL.h>
#include <rfmel/mfa/RFMFAInfo.h>
#include <rfmel/data/DataMEL.h>

#include <memory>
#include <string_view>
#include <type_traits>

namespace rfmel = ams::iface::rfmel;

// The pinned provider factory shape: C linkage for the symbol name, but a C++
// signature (std::shared_ptr result, std::string_view argument).
static_assert(std::is_same_v<rfmel::fnDataMEL, std::shared_ptr<rfmel::DataMEL> (*)(std::string_view)>);
static_assert(std::is_base_of_v<rfmel::RFMEL, rfmel::DataMEL>);
static_assert(std::is_abstract_v<rfmel::DataMEL>);
static_assert(std::is_abstract_v<rfmel::RFMFAInfo>);
static_assert(static_cast<int>(rfmel::JobDataFormat::ComplexINT16) == 3);

int rf_data_header_compile_probe()
{
    return 0;
}
