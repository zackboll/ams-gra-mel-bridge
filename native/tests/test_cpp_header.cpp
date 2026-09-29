#include <ams_mel/abi.h>
#include <type_traits>

static_assert(noexcept(ams_mel_get_abi_version(nullptr)));
static_assert(std::is_standard_layout_v<ams_mel_abi_version_v1>);
static_assert(std::is_standard_layout_v<ams_mel_ir_return_result_v1>);
static_assert(std::is_standard_layout_v<ams_mel_u32_span_v1>);
static_assert(std::is_standard_layout_v<ams_mel_string_view_span_v1>);
static_assert(std::is_standard_layout_v<ams_mel_ir_scan_type_v1>);
static_assert(std::is_standard_layout_v<ams_mel_ir_scan_param_v1>);
static_assert(std::is_standard_layout_v<ams_mel_ir_mode_command_v1>);
static_assert(std::is_standard_layout_v<ams_mel_ir_bit_command_v1>);
static_assert(std::is_standard_layout_v<ams_mel_ir_config_set_command_v1>);
static_assert(std::is_standard_layout_v<ams_mel_fault_v1>);
static_assert(std::is_standard_layout_v<ams_mel_ir_c2_metadata_event_v1>);
static_assert(std::is_standard_layout_v<ams_mel_ir_channel_capability_v1>);
static_assert(std::is_standard_layout_v<ams_mel_ir_channel_comms_test_result_v1>);
static_assert(std::is_standard_layout_v<ams_mel_ir_frame_snapshot_v1>);
static_assert(noexcept(ams_mel_ir_c2_metadata_event_close(nullptr, nullptr, 0, nullptr)));
static_assert(noexcept(ams_mel_ir_return_request_close(nullptr, nullptr, 0, nullptr)));
static_assert(noexcept(ams_mel_ir_frame_snapshot_close(nullptr, nullptr, 0, nullptr)));

using provider_version_fn = ams_mel_status_t (*)(const ams_mel_session *,
    ams_mel_provider_version_v1 *, char *, size_t, size_t *) noexcept;
using health_open_fn = ams_mel_status_t (*)(const ams_mel_session *,
    const ams_mel_ir_health_config_v1 *, ams_mel_ir_health **, char *, size_t,
    size_t *) noexcept;
static_assert(std::is_same_v<decltype(&ams_mel_session_get_provider_version),
                             provider_version_fn>);
static_assert(std::is_same_v<decltype(&ams_mel_ir_health_open), health_open_fn>);
using snapshot_receive_fn = ams_mel_status_t (*)(ams_mel_ir_stream *, uint32_t,
    ams_mel_ir_frame_snapshot **, char *, size_t, size_t *) noexcept;
static_assert(std::is_same_v<decltype(&ams_mel_ir_stream_receive_snapshot), snapshot_receive_fn>);

/* Task 032B1 public common Channel view: exact C++ shapes, noexcept. */
static_assert(noexcept(ams_mel_ir_channel_close(nullptr, nullptr, 0, nullptr)));
using channel_from_c2_fn = ams_mel_status_t (*)(const ams_mel_ir_c2 *, ams_mel_ir_channel **,
    char *, size_t, size_t *) noexcept;
using channel_from_stream_fn = ams_mel_status_t (*)(const ams_mel_ir_stream *,
    ams_mel_ir_channel **, char *, size_t, size_t *) noexcept;
using channel_from_health_fn = ams_mel_status_t (*)(const ams_mel_ir_health *,
    ams_mel_ir_channel **, char *, size_t, size_t *) noexcept;
using channel_from_instrumentation_fn = ams_mel_status_t (*)(const ams_mel_ir_instrumentation *,
    ams_mel_ir_channel **, char *, size_t, size_t *) noexcept;
using channel_from_track_fn = ams_mel_status_t (*)(const ams_mel_ir_track *,
    ams_mel_ir_channel **, char *, size_t, size_t *) noexcept;
using channel_keepalive_fn = ams_mel_status_t (*)(const ams_mel_ir_channel *,
    ams_mel_ir_return_request **, char *, size_t, size_t *) noexcept;
using channel_comms_fn = ams_mel_status_t (*)(const ams_mel_ir_channel *,
    const ams_mel_ir_channel_comms_test_request_v1 *, ams_mel_ir_channel_comms_request **,
    char *, size_t, size_t *) noexcept;
using channel_capability_fn = ams_mel_status_t (*)(const ams_mel_ir_channel *,
    ams_mel_ir_channel_capability **, char *, size_t, size_t *) noexcept;
using channel_close_fn = ams_mel_status_t (*)(ams_mel_ir_channel **, char *, size_t,
    size_t *) noexcept;
static_assert(std::is_same_v<decltype(&ams_mel_ir_channel_from_c2), channel_from_c2_fn>);
static_assert(std::is_same_v<decltype(&ams_mel_ir_channel_from_stream), channel_from_stream_fn>);
static_assert(std::is_same_v<decltype(&ams_mel_ir_channel_from_health), channel_from_health_fn>);
static_assert(std::is_same_v<decltype(&ams_mel_ir_channel_from_instrumentation),
                             channel_from_instrumentation_fn>);
static_assert(std::is_same_v<decltype(&ams_mel_ir_channel_from_track), channel_from_track_fn>);
static_assert(std::is_same_v<decltype(&ams_mel_ir_channel_send_keepalive), channel_keepalive_fn>);
static_assert(std::is_same_v<decltype(&ams_mel_ir_channel_submit_comms_test), channel_comms_fn>);
static_assert(std::is_same_v<decltype(&ams_mel_ir_channel_get_capabilities),
                             channel_capability_fn>);
static_assert(std::is_same_v<decltype(&ams_mel_ir_channel_close), channel_close_fn>);

/* Task 033B RF DataMEL: exact C++ shapes, noexcept, standard layout. */
static_assert(std::is_standard_layout_v<ams_mel_rf_frequency_range_v1>);
static_assert(std::is_standard_layout_v<ams_mel_rf_frequency_range_span_v1>);
static_assert(std::is_standard_layout_v<ams_mel_rf_face_info_v1>);
static_assert(std::is_standard_layout_v<ams_mel_rf_face_info_span_v1>);
static_assert(std::is_standard_layout_v<ams_mel_rf_mfa_info_v1>);
static_assert(std::is_trivially_copyable_v<ams_mel_rf_mfa_info_v1>);
static_assert(std::is_same_v<ams_mel_rf_job_data_format_t, uint32_t>);
static_assert(AMS_MEL_RF_JOB_DATA_FORMAT_LF_TYPE3 == 13U);
using rf_open_fn = ams_mel_status_t (*)(const char *, const char *, ams_mel_rf_data **,
    char *, size_t, size_t *) noexcept;
using rf_version_fn = ams_mel_status_t (*)(const ams_mel_rf_data *,
    ams_mel_provider_version_v1 *, char *, size_t, size_t *) noexcept;
using rf_mfa_fn = ams_mel_status_t (*)(const ams_mel_rf_data *, ams_mel_rf_mfa_info **,
    char *, size_t, size_t *) noexcept;
using rf_view_fn = ams_mel_status_t (*)(const ams_mel_rf_mfa_info *,
    const ams_mel_rf_mfa_info_v1 **, char *, size_t, size_t *) noexcept;
using rf_info_close_fn = ams_mel_status_t (*)(ams_mel_rf_mfa_info **, char *, size_t,
    size_t *) noexcept;
using rf_close_fn = ams_mel_status_t (*)(ams_mel_rf_data **, char *, size_t,
    size_t *) noexcept;
static_assert(std::is_same_v<decltype(&ams_mel_rf_data_open), rf_open_fn>);
static_assert(std::is_same_v<decltype(&ams_mel_rf_data_get_provider_version), rf_version_fn>);
static_assert(std::is_same_v<decltype(&ams_mel_rf_data_get_mfa_info), rf_mfa_fn>);
static_assert(std::is_same_v<decltype(&ams_mel_rf_mfa_info_view), rf_view_fn>);
static_assert(std::is_same_v<decltype(&ams_mel_rf_mfa_info_close), rf_info_close_fn>);
static_assert(std::is_same_v<decltype(&ams_mel_rf_data_close), rf_close_fn>);

/* Task 033D RF ProductRxEndpoint receive: exact C++ shapes, noexcept, and
 * standard-layout, trivially copyable records. */
static_assert(std::is_standard_layout_v<ams_mel_rf_product_rx_config_v1>);
static_assert(std::is_standard_layout_v<ams_mel_rf_complex_i16_v1>);
static_assert(std::is_standard_layout_v<ams_mel_rf_complex_i16_span_v1>);
static_assert(std::is_standard_layout_v<ams_mel_rf_product_rx_metadata_v1>);
static_assert(std::is_standard_layout_v<ams_mel_rf_product_rx_event_v1>);
static_assert(std::is_standard_layout_v<ams_mel_rf_product_rx_info_v1>);
static_assert(std::is_standard_layout_v<ams_mel_rf_product_rx_request_result_v1>);
static_assert(std::is_standard_layout_v<ams_mel_rf_product_rx_counters_v1>);
static_assert(std::is_trivially_copyable_v<ams_mel_rf_product_rx_event_v1>);
static_assert(std::is_trivially_copyable_v<ams_mel_rf_complex_i16_v1>);
static_assert(sizeof(ams_mel_rf_complex_i16_v1) == 4U);
using rx_submit_fn = ams_mel_status_t (*)(ams_mel_rf_data *,
    const ams_mel_rf_product_rx_config_v1 *, ams_mel_rf_product_rx_request **, char *, size_t,
    size_t *) noexcept;
using rx_wait_fn = ams_mel_status_t (*)(const ams_mel_rf_product_rx_request *, uint32_t,
    ams_mel_rf_product_rx_request_result_v1 *, char *, size_t, size_t *) noexcept;
using rx_claim_fn = ams_mel_status_t (*)(ams_mel_rf_product_rx_request *,
    ams_mel_rf_product_rx **, ams_mel_rf_product_rx_info_v1 *, char *, size_t,
    size_t *) noexcept;
using rx_request_close_fn = ams_mel_status_t (*)(ams_mel_rf_product_rx_request **, char *,
    size_t, size_t *) noexcept;
using rx_receive_fn = ams_mel_status_t (*)(ams_mel_rf_product_rx *, uint32_t,
    ams_mel_rf_product_rx_event **, char *, size_t, size_t *) noexcept;
using rx_counters_fn = ams_mel_status_t (*)(const ams_mel_rf_product_rx *,
    ams_mel_rf_product_rx_counters_v1 *, char *, size_t, size_t *) noexcept;
using rx_close_fn = ams_mel_status_t (*)(ams_mel_rf_product_rx **, char *, size_t,
    size_t *) noexcept;
using rx_view_fn = ams_mel_status_t (*)(const ams_mel_rf_product_rx_event *,
    const ams_mel_rf_product_rx_event_v1 **, char *, size_t, size_t *) noexcept;
using rx_event_close_fn = ams_mel_status_t (*)(ams_mel_rf_product_rx_event **, char *, size_t,
    size_t *) noexcept;
static_assert(std::is_same_v<decltype(&ams_mel_rf_data_submit_product_rx), rx_submit_fn>);
static_assert(std::is_same_v<decltype(&ams_mel_rf_product_rx_request_wait), rx_wait_fn>);
static_assert(std::is_same_v<decltype(&ams_mel_rf_product_rx_request_claim), rx_claim_fn>);
static_assert(std::is_same_v<decltype(&ams_mel_rf_product_rx_request_close),
                             rx_request_close_fn>);
static_assert(std::is_same_v<decltype(&ams_mel_rf_product_rx_receive), rx_receive_fn>);
static_assert(std::is_same_v<decltype(&ams_mel_rf_product_rx_get_counters), rx_counters_fn>);
static_assert(std::is_same_v<decltype(&ams_mel_rf_product_rx_close), rx_close_fn>);
static_assert(std::is_same_v<decltype(&ams_mel_rf_product_rx_event_view), rx_view_fn>);
static_assert(std::is_same_v<decltype(&ams_mel_rf_product_rx_event_close), rx_event_close_fn>);

int main()
{
    ams_mel_abi_version_v1 version{};
    if (ams_mel_get_abi_version(&version) != AMS_MEL_OK) {
        return 1;
    }
    ams_mel_ir_channel *channel = nullptr;
    if (ams_mel_ir_channel_close(&channel, nullptr, 0, nullptr) != AMS_MEL_OK || channel) {
        return 1;
    }
    return version.major == AMS_MEL_ABI_VERSION_MAJOR &&
                   version.minor == AMS_MEL_ABI_VERSION_MINOR ? 0 : 1;
}
