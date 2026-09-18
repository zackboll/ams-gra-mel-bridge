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
_Static_assert(sizeof(ams_mel_ir_mfa_state_t) == 4, "MFA state width");
_Static_assert(sizeof(ams_mel_ir_scan_type_v1) == 12, "scan type layout");
_Static_assert(offsetof(ams_mel_ir_mode_command_v1, scan_parameters) >
               offsetof(ams_mel_ir_mode_command_v1, mode), "mode command layout");
_Static_assert(sizeof(ams_mel_ir_c2_metadata_kind_t) == 4, "metadata kind width");
_Static_assert(sizeof(ams_mel_ir_c2_metadata *) == sizeof(void *), "metadata owner pointer");
_Static_assert(sizeof(ams_mel_ir_c2_metadata_event *) == sizeof(void *), "event owner pointer");
_Static_assert(sizeof(ams_mel_ir_channel_comms_test_request_v1) == 12, "comms request layout");
_Static_assert(sizeof(ams_mel_ir_channel_comms_test_result_v1) == 12, "comms result layout");
_Static_assert(sizeof(ams_mel_ir_channel_comms_request *) == sizeof(void *), "comms owner pointer");
_Static_assert(sizeof(ams_mel_ir_channel_capability *) == sizeof(void *), "capability owner pointer");
_Static_assert(sizeof(ams_mel_ir_command_status_v1) >= 24, "command status layout");
_Static_assert(offsetof(ams_mel_fault_v1, ambiguity_groups) >
               offsetof(ams_mel_fault_v1, component_ids), "complete fault layout");
_Static_assert(offsetof(ams_mel_ir_c2_metadata_event_v1, bit_status) >
               offsetof(ams_mel_ir_c2_metadata_event_v1, bit_configuration), "event root layout");

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return EXIT_FAILURE; \
    } \
} while (0)

int main(void)
{
    ams_mel_abi_version_v1 version = {UINT32_MAX, UINT32_MAX};
    ams_mel_status_t (*provider_version)(const ams_mel_session *,
        ams_mel_provider_version_v1 *, char *, size_t, size_t *) =
        ams_mel_session_get_provider_version;
    ams_mel_status_t (*health_open)(const ams_mel_session *,
        const ams_mel_ir_health_config_v1 *, ams_mel_ir_health **, char *,
        size_t, size_t *) = ams_mel_ir_health_open;
    (void)provider_version; (void)health_open;
    ams_mel_status_t (*submit_bit)(ams_mel_ir_c2 *, uint32_t,
        ams_mel_ir_return_request **, char *, size_t, size_t *) =
        ams_mel_ir_c2_submit_bit_noop;
    ams_mel_status_t (*wait_return)(const ams_mel_ir_return_request *, uint32_t,
        ams_mel_ir_return_result_v1 *, char *, size_t, size_t *) =
        ams_mel_ir_return_request_wait;
    ams_mel_status_t (*close_return)(ams_mel_ir_return_request **, char *,
        size_t, size_t *) = ams_mel_ir_return_request_close;
    (void)submit_bit; (void)wait_return; (void)close_return;
    ams_mel_status_t (*submit_mode)(ams_mel_ir_c2 *, const ams_mel_ir_mode_command_v1 *,
        ams_mel_ir_mode_request **, char *, size_t, size_t *) = ams_mel_ir_c2_submit_mode;
    ams_mel_status_t (*submit_full_bit)(ams_mel_ir_c2 *, const ams_mel_ir_bit_command_v1 *,
        ams_mel_ir_return_request **, char *, size_t, size_t *) = ams_mel_ir_c2_submit_bit;
    ams_mel_status_t (*submit_config)(ams_mel_ir_c2 *, const ams_mel_ir_config_set_command_v1 *,
        ams_mel_ir_return_request **, char *, size_t, size_t *) = ams_mel_ir_c2_submit_config_set;
    (void)submit_mode; (void)submit_full_bit; (void)submit_config;
    ams_mel_status_t (*metadata_open)(ams_mel_ir_c2 *, size_t,
        ams_mel_ir_c2_metadata **, char *, size_t, size_t *) = ams_mel_ir_c2_metadata_open;
    ams_mel_status_t (*metadata_receive)(ams_mel_ir_c2_metadata *, uint32_t,
        ams_mel_ir_c2_metadata_event **, char *, size_t, size_t *) = ams_mel_ir_c2_metadata_receive;
    ams_mel_status_t (*metadata_counters)(const ams_mel_ir_c2_metadata *,
        ams_mel_ir_c2_metadata_counters_v1 *, char *, size_t, size_t *) =
        ams_mel_ir_c2_metadata_get_counters;
    ams_mel_status_t (*metadata_close)(ams_mel_ir_c2_metadata **, char *, size_t,
        size_t *) = ams_mel_ir_c2_metadata_close;
    ams_mel_status_t (*event_view)(const ams_mel_ir_c2_metadata_event *,
        const ams_mel_ir_c2_metadata_event_v1 **, char *, size_t, size_t *) =
        ams_mel_ir_c2_metadata_event_view;
    ams_mel_status_t (*event_close)(ams_mel_ir_c2_metadata_event **, char *, size_t,
        size_t *) = ams_mel_ir_c2_metadata_event_close;
    (void)metadata_open; (void)metadata_receive; (void)metadata_counters;
    (void)metadata_close; (void)event_view; (void)event_close;
    ams_mel_status_t (*keepalive)(ams_mel_ir_c2 *, ams_mel_ir_return_request **,
        char *, size_t, size_t *) = ams_mel_ir_c2_send_keepalive;
    ams_mel_status_t (*submit_comms)(ams_mel_ir_c2 *,
        const ams_mel_ir_channel_comms_test_request_v1 *,
        ams_mel_ir_channel_comms_request **, char *, size_t, size_t *) =
        ams_mel_ir_c2_submit_comms_test;
    ams_mel_status_t (*wait_comms)(const ams_mel_ir_channel_comms_request *, uint32_t,
        ams_mel_ir_channel_comms_test_result_v1 *, char *, size_t, size_t *) =
        ams_mel_ir_channel_comms_request_wait;
    ams_mel_status_t (*close_comms)(ams_mel_ir_channel_comms_request **, char *,
        size_t, size_t *) = ams_mel_ir_channel_comms_request_close;
    ams_mel_status_t (*register_comms)(ams_mel_ir_c2_metadata *, char *, size_t,
        size_t *) = ams_mel_ir_c2_metadata_register_comms_test;
    ams_mel_status_t (*get_capability)(ams_mel_ir_c2 *, ams_mel_ir_channel_capability **,
        char *, size_t, size_t *) = ams_mel_ir_c2_get_capabilities;
    ams_mel_status_t (*view_capability)(const ams_mel_ir_channel_capability *,
        const ams_mel_ir_channel_capability_v1 **, char *, size_t, size_t *) =
        ams_mel_ir_channel_capability_view;
    ams_mel_status_t (*close_capability)(ams_mel_ir_channel_capability **, char *,
        size_t, size_t *) = ams_mel_ir_channel_capability_close;
    (void)keepalive; (void)submit_comms; (void)wait_comms; (void)close_comms;
    (void)register_comms; (void)get_capability; (void)view_capability; (void)close_capability;
    CHECK(AMS_MEL_IR_C2_METADATA_COMMAND_STATUS == 1U);
    CHECK(AMS_MEL_IR_C2_METADATA_BIT_CONFIGURATION == 2U);
    CHECK(AMS_MEL_IR_C2_METADATA_BIT_STATUS == 3U);
    CHECK(AMS_MEL_IR_COMMAND_NOT_SET == 0U);
    CHECK(AMS_MEL_IR_COMMAND_RECEIVED == 1U);
    CHECK(AMS_MEL_IR_COMMAND_ACCEPTED == 2U);
    CHECK(AMS_MEL_IR_COMMAND_REJECTED == 3U);
    CHECK(AMS_MEL_IR_COMMAND_CANCELLED == 4U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_NOT_SET == 0U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_ATTEMPTS == 1U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_ENDURANCE == 2U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_CLASSIFICATION == 3U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_FOR_FOV_LIMIT == 4U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_GATING == 5U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_MANEUVER_LIMIT == 6U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_OP == 7U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_OCCLUSION == 8U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_RANGE == 9U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_PERFORMANCE == 10U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_RF == 11U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_ROUTE == 12U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_SAFETY == 13U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_TARGET_ANGLE == 14U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_TIME == 15U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_SYSTEM == 16U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_INFEASIBLE_ROUTE == 17U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_MISSION_EVENT == 18U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_STATE_OR_SETTINGS == 19U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_STATE_OR_SETTINGS_CHANGE == 20U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_SYSTEM_UNAVAILABLE == 21U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_SYSTEM_FAULT == 22U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_SYSTEM_CONFLICT == 23U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_SUBSYSTEM_UNAVAILABLE == 24U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_SUBSYSTEM_FAULT == 25U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_FAULT == 26U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_PRECEDENCE == 27U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_UNAVAILABLE == 28U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_INSUFFICIENT_RESOURCES == 29U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_RANKING == 30U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_WEATHER == 31U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_INELIGIBLE_CONTROL_SOURCE == 32U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_DEPENDENCY_PREDECESSOR == 33U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_DEPENDENCY_ALL_OR_NOTHING == 34U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_DEPENDENCY_EITHER_OR == 35U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_INIT_CRITERIA_NOT_MET == 36U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_UNKNOWN_ID == 37U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_INVALID_INPUT_PARAMETER == 38U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_INPUT_OTHER == 39U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_MDF_ACTIVATION_ERROR == 40U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_MULTIPLE == 41U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_CANCELLED == 42U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_OTHER == 43U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_UNKNOWN == 44U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_ABORTED == 45U);
    CHECK(AMS_MEL_IR_CANNOT_COMPLY_ALIGNMENT_MANEUVER == 46U);
    CHECK(AMS_MEL_BIT_CONTROL_NOT_SET == 0U);
    CHECK(AMS_MEL_BIT_CONTROL_SUBSYSTEM_BIT_COMMAND == 1U);
    CHECK(AMS_MEL_BIT_CONTROL_SUBSYSTEM_STATE_COMMAND == 2U);
    CHECK(AMS_MEL_BIT_CONTROL_SUBSYSTEM_INITIATED == 3U);
    CHECK(AMS_MEL_BIT_RESULT_NOT_SET == 0U);
    CHECK(AMS_MEL_BIT_RESULT_PASS == 1U);
    CHECK(AMS_MEL_BIT_RESULT_FAIL == 2U);
    CHECK(AMS_MEL_BIT_RESULT_INTERRUPTED == 3U);
    CHECK(AMS_MEL_BIT_RESULT_NOT_TESTED == 4U);
    CHECK(AMS_MEL_FAULT_SEVERITY_NOT_SET == 0U);
    CHECK(AMS_MEL_FAULT_SEVERITY_NOMINAL == 1U);
    CHECK(AMS_MEL_FAULT_SEVERITY_CAUTION == 2U);
    CHECK(AMS_MEL_FAULT_SEVERITY_WARNING == 3U);
    CHECK(AMS_MEL_FAULT_SEVERITY_FAILED == 4U);
    CHECK(AMS_MEL_FAULT_STATE_NOT_SET == 0U);
    CHECK(AMS_MEL_FAULT_STATE_SET == 1U);
    CHECK(AMS_MEL_FAULT_STATE_CLEARED == 2U);
    CHECK(AMS_MEL_FAULT_STATE_UNKNOWN == 3U);
    CHECK(AMS_MEL_IR_MFA_STATE_NOT_SET == 0U);
    CHECK(AMS_MEL_IR_MFA_STATE_DEGRADED == 14U);
    CHECK(AMS_MEL_IR_MFA_STATE_MAX_EXCLUSIVE == 15U);
    CHECK(AMS_MEL_IR_COORD_FRAME_AIRCRAFT == 1U);
    CHECK(AMS_MEL_IR_DEGRADATION_REVISIT == 3U);
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
