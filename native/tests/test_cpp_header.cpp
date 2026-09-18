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
static_assert(noexcept(ams_mel_ir_c2_metadata_event_close(nullptr, nullptr, 0, nullptr)));
static_assert(noexcept(ams_mel_ir_return_request_close(nullptr, nullptr, 0, nullptr)));

int main()
{
    ams_mel_abi_version_v1 version{};
    if (ams_mel_get_abi_version(&version) != AMS_MEL_OK) {
        return 1;
    }
    return version.major == AMS_MEL_ABI_VERSION_MAJOR &&
                   version.minor == AMS_MEL_ABI_VERSION_MINOR ? 0 : 1;
}
