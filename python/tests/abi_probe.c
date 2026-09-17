#include <ams_mel/abi.h>

#include <stddef.h>
#include <stdio.h>

#define VALUE(value) printf("%llu\n", (unsigned long long)(value))
#define LAYOUT(type) VALUE(sizeof(type)); VALUE(_Alignof(type))
#define FIELD(type, field) VALUE(offsetof(type, field))

int main(void)
{
    ams_mel_abi_version_v1 version = {0, 0};

    VALUE(AMS_MEL_OK);
    VALUE(AMS_MEL_INVALID_ARGUMENT);
    VALUE(AMS_MEL_LIBRARY_LOAD_FAILED);
    VALUE(AMS_MEL_SYMBOL_NOT_FOUND);
    VALUE(AMS_MEL_FACTORY_FAILED);
    VALUE(AMS_MEL_INITIALIZATION_FAILED);
    VALUE(AMS_MEL_PROVIDER_EXCEPTION);
    VALUE(AMS_MEL_BUFFER_TOO_SMALL);
    VALUE(AMS_MEL_INTERNAL_ERROR);
    VALUE(AMS_MEL_PROVIDER_FAILED);

    LAYOUT(ams_mel_abi_version_v1);
    FIELD(ams_mel_abi_version_v1, major);
    FIELD(ams_mel_abi_version_v1, minor);
    LAYOUT(ams_mel_provider_version_v1);
    FIELD(ams_mel_provider_version_v1, api_version);
    FIELD(ams_mel_provider_version_v1, library_version);
    FIELD(ams_mel_provider_version_v1, vendor);
    FIELD(ams_mel_provider_version_v1, vendor_capacity);
    FIELD(ams_mel_provider_version_v1, vendor_required);
    FIELD(ams_mel_provider_version_v1, description);
    FIELD(ams_mel_provider_version_v1, description_capacity);
    FIELD(ams_mel_provider_version_v1, description_required);

    VALUE(ams_mel_get_abi_version(&version));
    VALUE(AMS_MEL_ABI_VERSION_MAJOR);
    VALUE(AMS_MEL_ABI_VERSION_MINOR);
    VALUE(version.major);
    VALUE(version.minor);
    return 0;
}
