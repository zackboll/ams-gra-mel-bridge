#include <ams_mel/abi.h>

#include <stddef.h>
#include <stdio.h>

int main(void)
{
    ams_mel_abi_version_v1 version = {0, 0};
    ams_mel_status_t abi_status;
    printf("statuses %d %d %d %d %d %d %d %d %d %d\n",
           AMS_MEL_OK, AMS_MEL_INVALID_ARGUMENT, AMS_MEL_LIBRARY_LOAD_FAILED,
           AMS_MEL_SYMBOL_NOT_FOUND, AMS_MEL_FACTORY_FAILED,
           AMS_MEL_INITIALIZATION_FAILED, AMS_MEL_PROVIDER_EXCEPTION,
           AMS_MEL_BUFFER_TOO_SMALL, AMS_MEL_INTERNAL_ERROR,
           AMS_MEL_PROVIDER_FAILED);
    printf("abi_layout %zu %zu %zu %zu\n",
           sizeof(ams_mel_abi_version_v1), _Alignof(ams_mel_abi_version_v1),
           offsetof(ams_mel_abi_version_v1, major),
           offsetof(ams_mel_abi_version_v1, minor));
    printf("provider_layout %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu\n",
           sizeof(ams_mel_provider_version_v1),
           _Alignof(ams_mel_provider_version_v1),
           offsetof(ams_mel_provider_version_v1, api_version),
           offsetof(ams_mel_provider_version_v1, library_version),
           offsetof(ams_mel_provider_version_v1, vendor),
           offsetof(ams_mel_provider_version_v1, vendor_capacity),
           offsetof(ams_mel_provider_version_v1, vendor_required),
           offsetof(ams_mel_provider_version_v1, description),
           offsetof(ams_mel_provider_version_v1, description_capacity),
           offsetof(ams_mel_provider_version_v1, description_required));
    abi_status = ams_mel_get_abi_version(&version);
    printf("abi_result %d %u %u\n", abi_status, version.major, version.minor);
    return 0;
}
