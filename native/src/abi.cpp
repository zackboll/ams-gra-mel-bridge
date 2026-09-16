#include <ams_mel/abi.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>

static_assert(std::is_standard_layout_v<ams_mel_abi_version_v1>);
static_assert(sizeof(ams_mel_abi_version_v1) == 2 * sizeof(std::uint32_t));
static_assert(offsetof(ams_mel_abi_version_v1, minor) == sizeof(std::uint32_t));

extern "C" ams_mel_status_t ams_mel_get_abi_version(
    ams_mel_abi_version_v1 *out_version) noexcept
{
    if (out_version == nullptr) {
        return AMS_MEL_INVALID_ARGUMENT;
    }

    *out_version = {AMS_MEL_ABI_VERSION_MAJOR, AMS_MEL_ABI_VERSION_MINOR};
    return AMS_MEL_OK;
}
