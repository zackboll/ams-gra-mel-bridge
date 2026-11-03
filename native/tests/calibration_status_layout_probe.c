#include <ams_mel/abi.h>
#include <stddef.h>
#include <stdio.h>
/* IRMEL-ADA-006 shared C11 layout probe for the private Python and Rust mirrors. */
int main(void)
{
    printf("%zu %zu ", sizeof(ams_mel_ir_calibration_status_command_v1), _Alignof(ams_mel_ir_calibration_status_command_v1));
    printf("%zu ", offsetof(ams_mel_ir_calibration_status_command_v1, command_id));
    printf("%zu ", offsetof(ams_mel_ir_calibration_status_command_v1, request_id));
    printf("%zu %zu ", sizeof(ams_mel_ir_completed_calibration_item_v1), _Alignof(ams_mel_ir_completed_calibration_item_v1));
    printf("%zu ", offsetof(ams_mel_ir_completed_calibration_item_v1, item_name));
    printf("%zu ", offsetof(ams_mel_ir_completed_calibration_item_v1, result));
    printf("%zu ", offsetof(ams_mel_ir_completed_calibration_item_v1, fail_reason));
    printf("%zu %zu ", sizeof(ams_mel_ir_completed_calibration_item_span_v1), _Alignof(ams_mel_ir_completed_calibration_item_span_v1));
    printf("%zu ", offsetof(ams_mel_ir_completed_calibration_item_span_v1, data));
    printf("%zu ", offsetof(ams_mel_ir_completed_calibration_item_span_v1, size));
    printf("%zu %zu ", sizeof(ams_mel_ir_completed_calibration_v1), _Alignof(ams_mel_ir_completed_calibration_v1));
    printf("%zu ", offsetof(ams_mel_ir_completed_calibration_v1, calibration_id));
    printf("%zu ", offsetof(ams_mel_ir_completed_calibration_v1, time_tag_ns));
    printf("%zu ", offsetof(ams_mel_ir_completed_calibration_v1, result));
    printf("%zu ", offsetof(ams_mel_ir_completed_calibration_v1, fail_reason));
    printf("%zu ", offsetof(ams_mel_ir_completed_calibration_v1, calibration_items));
    printf("%zu %zu ", sizeof(ams_mel_ir_completed_calibration_span_v1), _Alignof(ams_mel_ir_completed_calibration_span_v1));
    printf("%zu ", offsetof(ams_mel_ir_completed_calibration_span_v1, data));
    printf("%zu ", offsetof(ams_mel_ir_completed_calibration_span_v1, size));
    printf("%zu %zu ", sizeof(ams_mel_ir_calibration_status_v1), _Alignof(ams_mel_ir_calibration_status_v1));
    printf("%zu ", offsetof(ams_mel_ir_calibration_status_v1, active_calibration_ids));
    printf("%zu ", offsetof(ams_mel_ir_calibration_status_v1, completed_calibrations));
    printf("%zu %zu ", sizeof(ams_mel_ir_calibration_status_result_v1), _Alignof(ams_mel_ir_calibration_status_result_v1));
    printf("%zu ", offsetof(ams_mel_ir_calibration_status_result_v1, status));
    printf("%zu ", offsetof(ams_mel_ir_calibration_status_result_v1, error_code));
    printf("%u %u %u %u %u\n", AMS_MEL_IR_CALIBRATION_RESULT_NOT_SET, AMS_MEL_IR_CALIBRATION_RESULT_COMPLETED,
           AMS_MEL_IR_CALIBRATION_RESULT_FAILED, AMS_MEL_IR_CALIBRATION_RESULT_INTERRUPTED,
           AMS_MEL_IR_CALIBRATION_RESULT_NOT_CALIBRATED);
    return 0;
}
