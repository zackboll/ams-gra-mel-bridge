#include <ams_mel/abi.h>
#include <stddef.h>
#include <stdio.h>
int main(void)
{
    printf("%zu %zu ", sizeof(ams_mel_ir_calibration_configuration_command_v1), _Alignof(ams_mel_ir_calibration_configuration_command_v1));
    printf("%zu ", offsetof(ams_mel_ir_calibration_configuration_command_v1, command_id));
    printf("%zu ", offsetof(ams_mel_ir_calibration_configuration_command_v1, request_id));
    printf("%zu %zu ", sizeof(ams_mel_ir_subsystem_calibration_v1), _Alignof(ams_mel_ir_subsystem_calibration_v1));
    printf("%zu ", offsetof(ams_mel_ir_subsystem_calibration_v1, calibration_id));
    printf("%zu ", offsetof(ams_mel_ir_subsystem_calibration_v1, accepted_interface));
    printf("%zu ", offsetof(ams_mel_ir_subsystem_calibration_v1, calibration_item_names));
    printf("%zu ", offsetof(ams_mel_ir_subsystem_calibration_v1, subsystem_component_ids));
    printf("%zu ", offsetof(ams_mel_ir_subsystem_calibration_v1, capability_ids));
    printf("%zu %zu ", sizeof(ams_mel_ir_calibration_configuration_v1), _Alignof(ams_mel_ir_calibration_configuration_v1));
    printf("%zu ", offsetof(ams_mel_ir_calibration_configuration_v1, data));
    printf("%zu ", offsetof(ams_mel_ir_calibration_configuration_v1, size));
    printf("%zu %zu ", sizeof(ams_mel_ir_calibration_configuration_result_v1), _Alignof(ams_mel_ir_calibration_configuration_result_v1));
    printf("%zu ", offsetof(ams_mel_ir_calibration_configuration_result_v1, configuration));
    printf("%zu ", offsetof(ams_mel_ir_calibration_configuration_result_v1, error_code));
    printf("%u %u %u\n", AMS_MEL_IR_CALIBRATION_NOT_SET, AMS_MEL_IR_CALIBRATION_COMMAND, AMS_MEL_IR_CALIBRATION_STATE_COMMAND);
    return 0;
}
