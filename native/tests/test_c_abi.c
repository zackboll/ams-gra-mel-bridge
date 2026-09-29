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
_Static_assert(sizeof(ams_mel_ir_channel *) == sizeof(void *), "common Channel view pointer");
_Static_assert(sizeof(ams_mel_ir_command_status_v1) >= 24, "command status layout");
_Static_assert(offsetof(ams_mel_fault_v1, ambiguity_groups) >
               offsetof(ams_mel_fault_v1, component_ids), "complete fault layout");
_Static_assert(offsetof(ams_mel_ir_c2_metadata_event_v1, bit_status) >
               offsetof(ams_mel_ir_c2_metadata_event_v1, bit_configuration), "event root layout");
_Static_assert(sizeof(ams_mel_ir_frame_snapshot *) == sizeof(void *), "snapshot owner pointer");
_Static_assert(offsetof(ams_mel_ir_frame_snapshot_v1, sensor_nav_states) >
               offsetof(ams_mel_ir_frame_snapshot_v1, sensor_inertial_states), "snapshot layout");

/* Task 033B RF DataMEL: opaque owners and the value/span records. */
_Static_assert(sizeof(ams_mel_rf_data *) == sizeof(void *), "RF Data owner pointer");
_Static_assert(sizeof(ams_mel_rf_mfa_info *) == sizeof(void *), "RF MFA owner pointer");
_Static_assert(sizeof(ams_mel_rf_job_data_format_t) == 4, "JobDataFormat width");
_Static_assert(sizeof(ams_mel_rf_frequency_range_v1) == 16, "frequency range layout");
_Static_assert(offsetof(ams_mel_rf_frequency_range_v1, max_hz) == 8, "max_hz offset");
_Static_assert(offsetof(ams_mel_rf_frequency_range_span_v1, size) == sizeof(void *),
               "range span layout");
_Static_assert(offsetof(ams_mel_rf_face_info_span_v1, size) == sizeof(void *),
               "face span layout");
_Static_assert(offsetof(ams_mel_rf_face_info_v1, agc_processing_time_fs) == 16,
               "face durations are 8-byte aligned after four uint32 fields");
_Static_assert(offsetof(ams_mel_rf_face_info_v1, rx_rx_switching_time_fs) == 72,
               "eight femtosecond fields");
_Static_assert(offsetof(ams_mel_rf_face_info_v1, rx_frequency_ranges) == 80,
               "range spans follow durations");
_Static_assert(offsetof(ams_mel_rf_face_info_v1, sample_frequency_ranges) >
               offsetof(ams_mel_rf_face_info_v1, tx_frequency_ranges), "face layout");
_Static_assert(offsetof(ams_mel_rf_mfa_info_v1, contains_open_additions) == 8,
               "MFA layout");
_Static_assert(offsetof(ams_mel_rf_mfa_info_v1, scheduler_resolution_fs) == 16,
               "MFA layout");
_Static_assert(offsetof(ams_mel_rf_mfa_info_v1, faces) >
               offsetof(ams_mel_rf_mfa_info_v1, supported_data_formats), "MFA layout");

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
    ams_mel_status_t (*receive_snapshot)(ams_mel_ir_stream *, uint32_t,
        ams_mel_ir_frame_snapshot **, char *, size_t, size_t *) =
        ams_mel_ir_stream_receive_snapshot;
    ams_mel_status_t (*snapshot_view)(const ams_mel_ir_frame_snapshot *,
        const ams_mel_ir_frame_snapshot_v1 **, char *, size_t, size_t *) =
        ams_mel_ir_frame_snapshot_view;
    ams_mel_status_t (*snapshot_close)(ams_mel_ir_frame_snapshot **, char *,
        size_t, size_t *) = ams_mel_ir_frame_snapshot_close;
    (void)receive_snapshot; (void)snapshot_view; (void)snapshot_close;
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
    /* Task 032B1: exact public common Channel declarations. */
    ams_mel_status_t (*channel_from_c2)(const ams_mel_ir_c2 *, ams_mel_ir_channel **,
        char *, size_t, size_t *) = ams_mel_ir_channel_from_c2;
    ams_mel_status_t (*channel_from_stream)(const ams_mel_ir_stream *, ams_mel_ir_channel **,
        char *, size_t, size_t *) = ams_mel_ir_channel_from_stream;
    ams_mel_status_t (*channel_from_health)(const ams_mel_ir_health *, ams_mel_ir_channel **,
        char *, size_t, size_t *) = ams_mel_ir_channel_from_health;
    ams_mel_status_t (*channel_from_instrumentation)(const ams_mel_ir_instrumentation *,
        ams_mel_ir_channel **, char *, size_t, size_t *) = ams_mel_ir_channel_from_instrumentation;
    ams_mel_status_t (*channel_from_track)(const ams_mel_ir_track *, ams_mel_ir_channel **,
        char *, size_t, size_t *) = ams_mel_ir_channel_from_track;
    ams_mel_status_t (*channel_keepalive)(const ams_mel_ir_channel *, ams_mel_ir_return_request **,
        char *, size_t, size_t *) = ams_mel_ir_channel_send_keepalive;
    ams_mel_status_t (*channel_comms)(const ams_mel_ir_channel *,
        const ams_mel_ir_channel_comms_test_request_v1 *, ams_mel_ir_channel_comms_request **,
        char *, size_t, size_t *) = ams_mel_ir_channel_submit_comms_test;
    ams_mel_status_t (*channel_capability)(const ams_mel_ir_channel *,
        ams_mel_ir_channel_capability **, char *, size_t, size_t *) =
        ams_mel_ir_channel_get_capabilities;
    ams_mel_status_t (*channel_close)(ams_mel_ir_channel **, char *, size_t, size_t *) =
        ams_mel_ir_channel_close;
    ams_mel_ir_channel *channel = NULL;
    CHECK(channel_from_c2(NULL, &channel, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT && !channel);
    CHECK(channel_close(&channel, NULL, 0, NULL) == AMS_MEL_OK && !channel);
    (void)channel_from_stream; (void)channel_from_health; (void)channel_from_instrumentation;
    (void)channel_from_track; (void)channel_keepalive; (void)channel_comms; (void)channel_capability;
    /* Task 033B: exact RF DataMEL declarations and JobDataFormat values. */
    {
        ams_mel_status_t (*rf_open)(const char *, const char *, ams_mel_rf_data **,
            char *, size_t, size_t *) = ams_mel_rf_data_open;
        ams_mel_status_t (*rf_version)(const ams_mel_rf_data *,
            ams_mel_provider_version_v1 *, char *, size_t, size_t *) =
            ams_mel_rf_data_get_provider_version;
        ams_mel_status_t (*rf_mfa)(const ams_mel_rf_data *, ams_mel_rf_mfa_info **,
            char *, size_t, size_t *) = ams_mel_rf_data_get_mfa_info;
        ams_mel_status_t (*rf_view)(const ams_mel_rf_mfa_info *,
            const ams_mel_rf_mfa_info_v1 **, char *, size_t, size_t *) =
            ams_mel_rf_mfa_info_view;
        ams_mel_status_t (*rf_info_close)(ams_mel_rf_mfa_info **, char *, size_t,
            size_t *) = ams_mel_rf_mfa_info_close;
        ams_mel_status_t (*rf_close)(ams_mel_rf_data **, char *, size_t, size_t *) =
            ams_mel_rf_data_close;
        ams_mel_rf_data *rf_data = NULL;
        ams_mel_rf_mfa_info *rf_info = NULL;
        const ams_mel_rf_job_data_format_t formats[] = {
            AMS_MEL_RF_JOB_DATA_FORMAT_DIRECT_INT8,
            AMS_MEL_RF_JOB_DATA_FORMAT_DIRECT_INT16,
            AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT8,
            AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT16,
            AMS_MEL_RF_JOB_DATA_FORMAT_AMS_VITA_SMALL,
            AMS_MEL_RF_JOB_DATA_FORMAT_AMS_VITA_MEDIUM,
            AMS_MEL_RF_JOB_DATA_FORMAT_AMS_VITA_LARGE,
            AMS_MEL_RF_JOB_DATA_FORMAT_AMS_VITA_EXTRA_LARGE,
            AMS_MEL_RF_JOB_DATA_FORMAT_PDW_TYPE1,
            AMS_MEL_RF_JOB_DATA_FORMAT_PDW_TYPE2,
            AMS_MEL_RF_JOB_DATA_FORMAT_PDW_TYPE3,
            AMS_MEL_RF_JOB_DATA_FORMAT_LF_TYPE1,
            AMS_MEL_RF_JOB_DATA_FORMAT_LF_TYPE2,
            AMS_MEL_RF_JOB_DATA_FORMAT_LF_TYPE3,
        };
        CHECK(sizeof formats / sizeof formats[0] == 14U);
        for (uint32_t index = 0; index < 14U; ++index) CHECK(formats[index] == index);
        CHECK(rf_open(NULL, "", &rf_data, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(rf_data == NULL);
        CHECK(rf_version(NULL, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(rf_mfa(NULL, &rf_info, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(rf_view(NULL, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(rf_info_close(&rf_info, NULL, 0, NULL) == AMS_MEL_OK && rf_info == NULL);
        CHECK(rf_close(&rf_data, NULL, 0, NULL) == AMS_MEL_OK && rf_data == NULL);
    }
    {
        /* Task 033D RF ProductRxEndpoint receive: exact C11 shapes and the
         * NULL preconditions that need no provider. */
        ams_mel_status_t (*rx_submit)(ams_mel_rf_data *, const ams_mel_rf_product_rx_config_v1 *,
            ams_mel_rf_product_rx_request **, char *, size_t, size_t *) =
            ams_mel_rf_data_submit_product_rx;
        ams_mel_status_t (*rx_wait)(const ams_mel_rf_product_rx_request *, uint32_t,
            ams_mel_rf_product_rx_request_result_v1 *, char *, size_t, size_t *) =
            ams_mel_rf_product_rx_request_wait;
        ams_mel_status_t (*rx_claim)(ams_mel_rf_product_rx_request *, ams_mel_rf_product_rx **,
            ams_mel_rf_product_rx_info_v1 *, char *, size_t, size_t *) =
            ams_mel_rf_product_rx_request_claim;
        ams_mel_status_t (*rx_request_close)(ams_mel_rf_product_rx_request **, char *, size_t,
            size_t *) = ams_mel_rf_product_rx_request_close;
        ams_mel_status_t (*rx_receive)(ams_mel_rf_product_rx *, uint32_t,
            ams_mel_rf_product_rx_event **, char *, size_t, size_t *) =
            ams_mel_rf_product_rx_receive;
        ams_mel_status_t (*rx_counters)(const ams_mel_rf_product_rx *,
            ams_mel_rf_product_rx_counters_v1 *, char *, size_t, size_t *) =
            ams_mel_rf_product_rx_get_counters;
        ams_mel_status_t (*rx_close)(ams_mel_rf_product_rx **, char *, size_t, size_t *) =
            ams_mel_rf_product_rx_close;
        ams_mel_status_t (*rx_view)(const ams_mel_rf_product_rx_event *,
            const ams_mel_rf_product_rx_event_v1 **, char *, size_t, size_t *) =
            ams_mel_rf_product_rx_event_view;
        ams_mel_status_t (*rx_event_close)(ams_mel_rf_product_rx_event **, char *, size_t,
            size_t *) = ams_mel_rf_product_rx_event_close;
        ams_mel_rf_product_rx_config_v1 config = {AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT16,
                                                  4096U, 4U, 64U};
        ams_mel_rf_product_rx_request *rx_request = NULL;
        ams_mel_rf_product_rx *rx_endpoint = NULL;
        ams_mel_rf_product_rx_event *rx_event = NULL;
        ams_mel_rf_product_rx_request_result_v1 rx_result = {0};
        ams_mel_rf_product_rx_info_v1 rx_info = {0, 0};
        ams_mel_rf_product_rx_counters_v1 rx_count = {0, 0, 0, 0, 0, 0};
        const ams_mel_rf_product_rx_event_v1 *rx_record = NULL;
        ams_mel_rf_complex_i16_v1 sample = {INT16_MIN, INT16_MAX};
        ams_mel_rf_complex_i16_span_v1 samples = {&sample, 1U};
        CHECK(sizeof(ams_mel_rf_complex_i16_v1) == 4U);
        CHECK(samples.data[0].real == INT16_MIN && samples.data[0].imag == INT16_MAX);
        CHECK(sizeof(ams_mel_rf_product_rx_counters_v1) == 6U * sizeof(uint64_t));
        CHECK(rx_submit(NULL, &config, &rx_request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(rx_request == NULL);
        CHECK(rx_wait(NULL, 0U, &rx_result, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(rx_claim(NULL, &rx_endpoint, &rx_info, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(rx_endpoint == NULL);
        CHECK(rx_receive(NULL, 0U, &rx_event, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(rx_event == NULL);
        CHECK(rx_counters(NULL, &rx_count, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(rx_view(NULL, &rx_record, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(rx_record == NULL);
        CHECK(rx_request_close(&rx_request, NULL, 0, NULL) == AMS_MEL_OK && rx_request == NULL);
        CHECK(rx_close(&rx_endpoint, NULL, 0, NULL) == AMS_MEL_OK && rx_endpoint == NULL);
        CHECK(rx_event_close(&rx_event, NULL, 0, NULL) == AMS_MEL_OK && rx_event == NULL);
        CHECK(rx_request_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(rx_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(rx_event_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    }
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
