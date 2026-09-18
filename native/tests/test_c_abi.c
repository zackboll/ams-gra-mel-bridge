#include <ams_mel/abi.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

_Static_assert(sizeof(ams_mel_status_t) == 4, "status must be 32 bits");
_Static_assert(sizeof(ams_mel_abi_version_v1) == 8, "version record layout");
_Static_assert(offsetof(ams_mel_abi_version_v1, minor) == 4, "minor offset");
_Static_assert(sizeof(ams_mel_ir_return_t) == 4, "IR Return must be 32 bits");
_Static_assert(sizeof(ams_mel_ir_return_result_v1) == 8, "return result layout");
_Static_assert(offsetof(ams_mel_ir_return_result_v1, value) == 0, "return offset");
_Static_assert(offsetof(ams_mel_ir_return_result_v1, error_code) == 4, "error offset");

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return EXIT_FAILURE; \
    } \
} while (0)

int main(void)
{
    ams_mel_abi_version_v1 version = {UINT32_MAX, UINT32_MAX};
    ams_mel_status_t (*submit_bit)(ams_mel_ir_c2 *, uint32_t,
        ams_mel_ir_return_request **, char *, size_t, size_t *) =
        ams_mel_ir_c2_submit_bit_noop;
    ams_mel_status_t (*wait_return)(const ams_mel_ir_return_request *, uint32_t,
        ams_mel_ir_return_result_v1 *, char *, size_t, size_t *) =
        ams_mel_ir_return_request_wait;
    ams_mel_status_t (*close_return)(ams_mel_ir_return_request **, char *,
        size_t, size_t *) = ams_mel_ir_return_request_close;
    (void)submit_bit; (void)wait_return; (void)close_return;
    CHECK(AMS_MEL_IR_RETURN_SUCCESS == 0U);
    CHECK(AMS_MEL_IR_RETURN_BAD_POINTER == 1U);
    CHECK(AMS_MEL_IR_RETURN_FAIL == 2U);
    CHECK(AMS_MEL_IR_RETURN_NOT_SUPPORTED == 3U);
    CHECK(AMS_MEL_IR_RETURN_NOT_IMPLEMENTED == 4U);
    CHECK(ams_mel_get_abi_version(NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_get_abi_version(&version) == AMS_MEL_OK);
    CHECK(version.major == AMS_MEL_ABI_VERSION_MAJOR);
    CHECK(version.minor == AMS_MEL_ABI_VERSION_MINOR);

    version.major = UINT32_MAX;
    version.minor = UINT32_MAX;
    CHECK(ams_mel_get_abi_version(&version) == AMS_MEL_OK);
    CHECK(version.major == 0 && version.minor == 1);
    puts("PASS: real C client -> C ABI -> C++ implementation");
    return EXIT_SUCCESS;
}
