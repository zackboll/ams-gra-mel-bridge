#include <memory>
#include <string>

#include <irmel/library/irmel-types/CommonIR_MEL.h>

extern "C" __attribute__((visibility("default")))
std::shared_ptr<API_Manager> getAPI_Manager(const std::string&)
{
    return {};
}
