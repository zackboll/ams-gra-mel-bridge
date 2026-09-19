#ifndef AMS_MEL_ABI_H
#define AMS_MEL_ABI_H

#include <stddef.h>
#include <stdint.h>

/* This is our experimental C ABI, not an upstream MEL/provider version. */
#define AMS_MEL_ABI_VERSION_MAJOR UINT32_C(0)
#define AMS_MEL_ABI_VERSION_MINOR UINT32_C(1)

typedef int32_t ams_mel_status_t;
#define AMS_MEL_OK               INT32_C(0)
#define AMS_MEL_INVALID_ARGUMENT INT32_C(1)
#define AMS_MEL_LIBRARY_LOAD_FAILED INT32_C(2)
#define AMS_MEL_SYMBOL_NOT_FOUND    INT32_C(3)
#define AMS_MEL_FACTORY_FAILED      INT32_C(4)
#define AMS_MEL_INITIALIZATION_FAILED INT32_C(5)
#define AMS_MEL_PROVIDER_EXCEPTION  INT32_C(6)
#define AMS_MEL_BUFFER_TOO_SMALL    INT32_C(7)
#define AMS_MEL_INTERNAL_ERROR      INT32_C(8)
#define AMS_MEL_TIMEOUT             INT32_C(9)
#define AMS_MEL_STREAM_STOPPED      INT32_C(10)
#define AMS_MEL_PROVIDER_FAILED     INT32_C(11)
#define AMS_MEL_COMMAND_REJECTED    INT32_C(12)

#if defined(_WIN32)
#  if defined(AMS_MEL_BUILDING_LIBRARY)
#    define AMS_MEL_API __declspec(dllexport)
#  else
#    define AMS_MEL_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  define AMS_MEL_API __attribute__((visibility("default")))
#else
#  define AMS_MEL_API
#endif

#ifdef __cplusplus
#  define AMS_MEL_NOEXCEPT noexcept
extern "C" {
#else
#  define AMS_MEL_NOEXCEPT
#endif

/* In-process ABI value record; never serialize its raw bytes as a wire format.
 * The layout is fixed for this versioned type. Do not append fields to it.
 */
typedef struct ams_mel_abi_version_v1 {
    uint32_t major;
    uint32_t minor;
} ams_mel_abi_version_v1;

typedef struct ams_mel_session ams_mel_session;
typedef struct ams_mel_ir_stream ams_mel_ir_stream;
typedef struct ams_mel_ir_frame_snapshot ams_mel_ir_frame_snapshot;
typedef struct ams_mel_ir_image_metadata ams_mel_ir_image_metadata;
typedef struct ams_mel_ir_image_metadata_event ams_mel_ir_image_metadata_event;
typedef uint32_t ams_mel_ir_image_flag_t;
#define AMS_MEL_IR_IMAGE_FLAG_SCAN_FIRST UINT32_C(0)
#define AMS_MEL_IR_IMAGE_FLAG_SCAN_LAST UINT32_C(1)
#define AMS_MEL_IR_IMAGE_FLAG_STARE_SNAPSHOT UINT32_C(2)
#define AMS_MEL_IR_IMAGE_FLAG_STARE_ROLLING UINT32_C(3)
#define AMS_MEL_IR_IMAGE_RESERVED13 UINT32_C(2)
typedef struct ams_mel_ir_c2 ams_mel_ir_c2;
typedef struct ams_mel_ir_mode_request ams_mel_ir_mode_request;
typedef struct ams_mel_ir_return_request ams_mel_ir_return_request;
typedef struct ams_mel_ir_channel_comms_request ams_mel_ir_channel_comms_request;
typedef struct ams_mel_ir_channel_capability ams_mel_ir_channel_capability;
typedef struct ams_mel_ir_c2_metadata ams_mel_ir_c2_metadata;
typedef struct ams_mel_ir_c2_metadata_event ams_mel_ir_c2_metadata_event;
typedef struct ams_mel_ir_health ams_mel_ir_health;
typedef struct ams_mel_ir_health_metadata ams_mel_ir_health_metadata;
typedef struct ams_mel_ir_health_metadata_event ams_mel_ir_health_metadata_event;

typedef uint32_t ams_mel_ir_channel_type_t;
#define AMS_MEL_IR_CHANNEL_IRST_TRACK UINT32_C(0)
#define AMS_MEL_IR_CHANNEL_IRST_IMAGE UINT32_C(1)
#define AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL UINT32_C(2)
#define AMS_MEL_IR_CHANNEL_SCHEDULING UINT32_C(3)
#define AMS_MEL_IR_CHANNEL_HEALTH_AND_STATUS UINT32_C(4)
#define AMS_MEL_IR_CHANNEL_INSTRUMENTATION UINT32_C(5)
#define AMS_MEL_IR_CHANNEL_STACKED_IMAGE UINT32_C(6)
#define AMS_MEL_IR_CHANNEL_RESERVED_1 UINT32_C(7)
#define AMS_MEL_IR_CHANNEL_RESERVED_2 UINT32_C(8)
typedef uint32_t ams_mel_ir_mfa_mode_t;
#define AMS_MEL_IR_MFA_MODE_UNUSED UINT32_C(0)
#define AMS_MEL_IR_MFA_MODE_TASK_SCHED UINT32_C(1)
#define AMS_MEL_IR_MFA_MODE_SCAN_VOLUME_SCHED UINT32_C(2)
#define AMS_MEL_IR_MFA_MODE_SCAN_BAR_SCHED UINT32_C(3)
typedef uint32_t ams_mel_ir_mfa_state_t;
#define AMS_MEL_IR_MFA_STATE_NOT_SET UINT32_C(0)
#define AMS_MEL_IR_MFA_STATE_UNKNOWN UINT32_C(1)
#define AMS_MEL_IR_MFA_STATE_NOT_INSTALLED UINT32_C(2)
#define AMS_MEL_IR_MFA_STATE_OFF UINT32_C(3)
#define AMS_MEL_IR_MFA_STATE_PRE_INITIALIZATION UINT32_C(4)
#define AMS_MEL_IR_MFA_STATE_INITIALIZATION UINT32_C(5)
#define AMS_MEL_IR_MFA_STATE_STANDBY UINT32_C(6)
#define AMS_MEL_IR_MFA_STATE_OPERATE UINT32_C(7)
#define AMS_MEL_IR_MFA_STATE_OPERATE_RX_ONLY UINT32_C(8)
#define AMS_MEL_IR_MFA_STATE_OPERATE_TX_ONLY UINT32_C(9)
#define AMS_MEL_IR_MFA_STATE_MAINTENANCE UINT32_C(10)
#define AMS_MEL_IR_MFA_STATE_CALIBRATION UINT32_C(11)
#define AMS_MEL_IR_MFA_STATE_INITIATED_BIT UINT32_C(12)
#define AMS_MEL_IR_MFA_STATE_SHUTDOWN UINT32_C(13)
#define AMS_MEL_IR_MFA_STATE_DEGRADED UINT32_C(14)
#define AMS_MEL_IR_MFA_STATE_MAX_EXCLUSIVE UINT32_C(15)
typedef uint32_t ams_mel_state_transition_status_t;
#define AMS_MEL_STATE_TRANSITION_NOT_SET UINT32_C(0)
#define AMS_MEL_STATE_TRANSITION_NOT_TRANSITIONING UINT32_C(1)
#define AMS_MEL_STATE_TRANSITION_SHUTTING_DOWN UINT32_C(2)
#define AMS_MEL_STATE_TRANSITION_TRANSITIONING UINT32_C(3)
typedef uint32_t ams_mel_component_state_t;
#define AMS_MEL_COMPONENT_STATE_NOT_SET UINT32_C(0)
#define AMS_MEL_COMPONENT_STATE_UNKNOWN UINT32_C(1)
#define AMS_MEL_COMPONENT_STATE_NOT_INSTALLED UINT32_C(2)
#define AMS_MEL_COMPONENT_STATE_OFF UINT32_C(3)
#define AMS_MEL_COMPONENT_STATE_INITIALIZING UINT32_C(4)
#define AMS_MEL_COMPONENT_STATE_OPERATIONAL UINT32_C(5)
#define AMS_MEL_COMPONENT_STATE_DEGRADED UINT32_C(6)
#define AMS_MEL_COMPONENT_STATE_DISABLED UINT32_C(7)
#define AMS_MEL_COMPONENT_STATE_FAULTED UINT32_C(8)
typedef uint32_t ams_mel_temperature_state_t;
#define AMS_MEL_TEMPERATURE_STATE_NOT_SET UINT32_C(0)
#define AMS_MEL_TEMPERATURE_STATE_UNDER_TEMP UINT32_C(1)
#define AMS_MEL_TEMPERATURE_STATE_NORMAL UINT32_C(2)
#define AMS_MEL_TEMPERATURE_STATE_OVER_TEMP_WARNING UINT32_C(3)
#define AMS_MEL_TEMPERATURE_STATE_OVER_TEMP_DEGRADED UINT32_C(4)
#define AMS_MEL_TEMPERATURE_STATE_OVER_TEMP_SHUTDOWN UINT32_C(5)
typedef uint32_t ams_mel_ir_failure_t;
#define AMS_MEL_IR_FAILURE_NA UINT32_C(0)
#define AMS_MEL_IR_FAILURE_CRITICAL UINT32_C(1)
#define AMS_MEL_IR_FAILURE_MAJOR UINT32_C(2)
#define AMS_MEL_IR_FAILURE_PARAMETRIC UINT32_C(3)
#define AMS_MEL_IR_FAILURE_INFORMATIONAL UINT32_C(4)
#define AMS_MEL_IR_FAILURE_AVAILABLE UINT32_C(5)
#define AMS_MEL_IR_FAILURE_NOT_PRESENT UINT32_C(6)
typedef uint32_t ams_mel_ir_csci_mode_t;
#define AMS_MEL_IR_CSCI_MODE_UNKNOWN UINT32_C(0)
#define AMS_MEL_IR_CSCI_MODE_UNUSED UINT32_C(1)
#define AMS_MEL_IR_CSCI_MODE_INITIALIZATION UINT32_C(2)
#define AMS_MEL_IR_CSCI_MODE_MAINTENANCE UINT32_C(3)
#define AMS_MEL_IR_CSCI_MODE_IDLE UINT32_C(4)
#define AMS_MEL_IR_CSCI_MODE_OPERATIONAL UINT32_C(5)
#define AMS_MEL_IR_CSCI_MODE_VSA UINT32_C(6)
#define AMS_MEL_IR_CSCI_MODE_QUICK_LOOK UINT32_C(7)
#define AMS_MEL_IR_CSCI_MODE_TRACK UINT32_C(8)
#define AMS_MEL_IR_CSCI_MODE_IMAGING UINT32_C(9)
#define AMS_MEL_IR_CSCI_MODE_NOISE UINT32_C(10)
typedef uint32_t ams_mel_security_event_kind_t;
#define AMS_MEL_SECURITY_EVENT_NONE UINT32_C(0)
#define AMS_MEL_SECURITY_EVENT_AUTHENTICATION UINT32_C(1)
#define AMS_MEL_SECURITY_EVENT_INTEGRITY UINT32_C(2)
#define AMS_MEL_SECURITY_EVENT_FILE_MANAGEMENT UINT32_C(3)
#define AMS_MEL_SECURITY_EVENT_KEY_MANAGEMENT UINT32_C(4)
#define AMS_MEL_SECURITY_EVENT_SYSTEM UINT32_C(5)
#define AMS_MEL_SECURITY_EVENT_SANITIZATION UINT32_C(6)
typedef uint32_t ams_mel_security_outcome_t;
#define AMS_MEL_SECURITY_OUTCOME_NOT_SET UINT32_C(0)
#define AMS_MEL_SECURITY_OUTCOME_FAILURE UINT32_C(1)
#define AMS_MEL_SECURITY_OUTCOME_SUCCESS UINT32_C(2)
typedef uint32_t ams_mel_security_severity_t;
#define AMS_MEL_SECURITY_SEVERITY_NOT_SET UINT32_C(0)
#define AMS_MEL_SECURITY_SEVERITY_CRITICAL UINT32_C(1)
#define AMS_MEL_SECURITY_SEVERITY_ERROR UINT32_C(2)
#define AMS_MEL_SECURITY_SEVERITY_INFORMATIONAL UINT32_C(3)
#define AMS_MEL_SECURITY_SEVERITY_WARNING UINT32_C(4)
typedef uint32_t ams_mel_security_category_t;
typedef uint32_t ams_mel_ir_coord_frame_ref_t;
#define AMS_MEL_IR_COORD_FRAME_INERTIAL UINT32_C(0)
#define AMS_MEL_IR_COORD_FRAME_AIRCRAFT UINT32_C(1)
typedef uint32_t ams_mel_ir_degradation_method_t;
#define AMS_MEL_IR_DEGRADATION_CAPACITY UINT32_C(0)
#define AMS_MEL_IR_DEGRADATION_VOLUME UINT32_C(1)
#define AMS_MEL_IR_DEGRADATION_RANGE UINT32_C(2)
#define AMS_MEL_IR_DEGRADATION_REVISIT UINT32_C(3)
typedef uint32_t ams_mel_ir_return_t;
#define AMS_MEL_IR_RETURN_SUCCESS UINT32_C(0)
#define AMS_MEL_IR_RETURN_BAD_POINTER UINT32_C(1)
#define AMS_MEL_IR_RETURN_FAIL UINT32_C(2)
#define AMS_MEL_IR_RETURN_NOT_SUPPORTED UINT32_C(3)
#define AMS_MEL_IR_RETURN_NOT_IMPLEMENTED UINT32_C(4)
typedef uint32_t ams_mel_error_code_t;
#define AMS_MEL_ERROR_NONE UINT32_C(0)
#define AMS_MEL_ERROR_INVALID_ID UINT32_C(1)
#define AMS_MEL_ERROR_INVALID_STATE UINT32_C(2)
#define AMS_MEL_ERROR_INVALID_PARAMETERS UINT32_C(3)
#define AMS_MEL_ERROR_INSUFFICIENT_PERMISSIONS UINT32_C(4)
#define AMS_MEL_ERROR_INSUFFICIENT_RESOURCES UINT32_C(5)
#define AMS_MEL_ERROR_INSUFFICIENT_LOCAL_RESOURCES UINT32_C(6)
#define AMS_MEL_ERROR_INSUFFICIENT_REMOTE_RESOURCES UINT32_C(7)
#define AMS_MEL_ERROR_UNSUPPORTED UINT32_C(8)
typedef uint32_t ams_mel_ir_pixel_format_t;
#define AMS_MEL_IR_PIXEL_MONO UINT32_C(0)
#define AMS_MEL_IR_PIXEL_RGB UINT32_C(1)
#define AMS_MEL_IR_PIXEL_BAYER UINT32_C(2)
typedef uint32_t ams_mel_ir_sensor_type_t;
#define AMS_MEL_IR_SENSOR_UNSPECIFIED UINT32_C(0)
#define AMS_MEL_IR_SENSOR_GIMBAL_HORIZONTAL UINT32_C(1)
#define AMS_MEL_IR_SENSOR_GIMBAL_VERTICAL UINT32_C(2)
#define AMS_MEL_IR_SENSOR_GIMBAL_ROTATION UINT32_C(3)
#define AMS_MEL_IR_SENSOR_STEP_STARE UINT32_C(4)
typedef uint32_t ams_mel_ir_band_type_t;
#define AMS_MEL_IR_BAND_INVALID UINT32_C(0)
#define AMS_MEL_IR_BAND_MULTIBAND UINT32_C(1)
#define AMS_MEL_IR_BAND_IR_FAR UINT32_C(2)
#define AMS_MEL_IR_BAND_IR_NEAR UINT32_C(3)
#define AMS_MEL_IR_BAND_IR_LONGWAVE UINT32_C(4)
#define AMS_MEL_IR_BAND_IR_MIDWAVE UINT32_C(5)
#define AMS_MEL_IR_BAND_IR_SHORTWAVE UINT32_C(6)
#define AMS_MEL_IR_BAND_VISIBLE_WHITE UINT32_C(7)
#define AMS_MEL_IR_BAND_VISIBLE_RED UINT32_C(8)
#define AMS_MEL_IR_BAND_VISIBLE_GREEN UINT32_C(9)
#define AMS_MEL_IR_BAND_VISIBLE_BLUE UINT32_C(10)
#define AMS_MEL_IR_BAND_UVA UINT32_C(11)
#define AMS_MEL_IR_BAND_UVB UINT32_C(12)
#define AMS_MEL_IR_BAND_UVC UINT32_C(13)
#define AMS_MEL_IR_BAND_UV_VACUUM UINT32_C(14)
typedef uint32_t ams_mel_ir_coordinate_system_type_t;
#define AMS_MEL_IR_COORDINATE_LLA UINT32_C(0)
#define AMS_MEL_IR_COORDINATE_ECEF UINT32_C(1)
#define AMS_MEL_IR_COORDINATE_NED_PLATFORM UINT32_C(2)
#define AMS_MEL_IR_COORDINATE_NED_SENSOR UINT32_C(3)
typedef uint32_t ams_mel_ir_channel_metadata_capability_t;
#define AMS_MEL_IR_METADATA_BAD_PIXEL_LIST UINT32_C(0)
#define AMS_MEL_IR_METADATA_OPTICAL_DISTORTION_MAP UINT32_C(1)
#define AMS_MEL_IR_METADATA_LF_STATUS UINT32_C(2)
#define AMS_MEL_IR_METADATA_LINE_OF_SIGHT_REPORT UINT32_C(3)
#define AMS_MEL_IR_METADATA_LINE_OF_SIGHT_QUATERNION UINT32_C(4)
#define AMS_MEL_IR_METADATA_LINE_OF_SIGHT_EULER UINT32_C(5)
#define AMS_MEL_IR_METADATA_MFA_STATUS UINT32_C(6)
#define AMS_MEL_IR_METADATA_MFA_STATUS_DETAILED UINT32_C(7)
#define AMS_MEL_IR_METADATA_BIT_CONFIGURATION UINT32_C(8)
#define AMS_MEL_IR_METADATA_COMMAND_STATUS UINT32_C(9)
#define AMS_MEL_IR_METADATA_BIT_STATUS UINT32_C(10)
#define AMS_MEL_IR_METADATA_CANDIDATE_OBJECT_MESSAGE UINT32_C(11)
#define AMS_MEL_IR_METADATA_TASK_EXECUTING_REP UINT32_C(12)
#define AMS_MEL_IR_METADATA_SUBSYSTEM_STATUS_RESP UINT32_C(13)
#define AMS_MEL_IR_METADATA_EXECUTE_TASK_ACK UINT32_C(14)
#define AMS_MEL_IR_METADATA_SCHED_CREATED_REP UINT32_C(15)
#define AMS_MEL_IR_METADATA_IRST_TRACK_REPORT UINT32_C(16)
#define AMS_MEL_IR_METADATA_CHANNEL_COMMS_TEST_REP UINT32_C(17)
#define AMS_MEL_IR_METADATA_CAMERA_COMMAND_RESP UINT32_C(18)
#define AMS_MEL_IR_METADATA_CAMERA_PROTECT_CMD_RESP UINT32_C(19)
#define AMS_MEL_IR_METADATA_INSTRUMENTATION_REPORT UINT32_C(20)
#define AMS_MEL_IR_METADATA_NAVIGATION_REPORT_RESP UINT32_C(21)
#define AMS_MEL_IR_METADATA_REQUEST_SYSTEM_TRACK_DATA UINT32_C(22)
#define AMS_MEL_IR_METADATA_UPDATE_TRACK_LIST_RESPONSE UINT32_C(23)
#define AMS_MEL_IR_METADATA_LOS_3D_KINEMATICS_TYPE UINT32_C(24)
#define AMS_MEL_IR_METADATA_CANDIDATE_OBJECT_PREPROC_MESSAGE UINT32_C(25)
#define AMS_MEL_IR_METADATA_TASK_EVENTS UINT32_C(26)
#define AMS_MEL_IR_METADATA_SCAN_PERFORMANCE_REPORT UINT32_C(27)
#define AMS_MEL_IR_METADATA_RESERVED_3 UINT32_C(28)
#define AMS_MEL_IR_METADATA_RESERVED_5 UINT32_C(29)
#define AMS_MEL_IR_METADATA_RESERVED_9 UINT32_C(30)
#define AMS_MEL_IR_METADATA_RESERVED_10 UINT32_C(31)
typedef uint32_t ams_mel_ir_image_type_t;
#define AMS_MEL_IR_IMAGE_STARING UINT32_C(0)
#define AMS_MEL_IR_IMAGE_SCANNING UINT32_C(1)
typedef uint32_t ams_mel_ir_image_flip_t;
#define AMS_MEL_IR_FLIP_NONE UINT32_C(0)
#define AMS_MEL_IR_FLIP_VERTICAL UINT32_C(1)
#define AMS_MEL_IR_FLIP_HORIZONTAL UINT32_C(2)
#define AMS_MEL_IR_FLIP_BOTH UINT32_C(3)

typedef uint32_t ams_mel_ir_c2_metadata_kind_t;
#define AMS_MEL_IR_C2_METADATA_COMMAND_STATUS UINT32_C(1)
#define AMS_MEL_IR_C2_METADATA_BIT_CONFIGURATION UINT32_C(2)
#define AMS_MEL_IR_C2_METADATA_BIT_STATUS UINT32_C(3)
#define AMS_MEL_IR_C2_METADATA_CHANNEL_COMMS_TEST UINT32_C(4)
typedef uint32_t ams_mel_ir_image_metadata_kind_t;
/* Append future Image metadata kinds without changing BadPixelList. */
#define AMS_MEL_IR_IMAGE_METADATA_BAD_PIXEL_LIST UINT32_C(1)
#define AMS_MEL_IR_IMAGE_METADATA_LINE_OF_SIGHT_REPORT UINT32_C(2)
#define AMS_MEL_IR_IMAGE_METADATA_LINE_OF_SIGHT_EULER UINT32_C(3)
#define AMS_MEL_IR_IMAGE_METADATA_NAVIGATION_RESPONSE UINT32_C(4)
typedef uint32_t ams_mel_ir_bad_pixel_reason_t;
#define AMS_MEL_IR_BAD_PIXEL_REASON_UNKNOWN UINT32_C(0)
typedef uint32_t ams_mel_ir_command_state_t;
#define AMS_MEL_IR_COMMAND_NOT_SET UINT32_C(0)
#define AMS_MEL_IR_COMMAND_RECEIVED UINT32_C(1)
#define AMS_MEL_IR_COMMAND_ACCEPTED UINT32_C(2)
#define AMS_MEL_IR_COMMAND_REJECTED UINT32_C(3)
#define AMS_MEL_IR_COMMAND_CANCELLED UINT32_C(4)
typedef uint32_t ams_mel_ir_cannot_comply_t;
#define AMS_MEL_IR_CANNOT_COMPLY_NOT_SET UINT32_C(0)
#define AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_ATTEMPTS UINT32_C(1)
#define AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_ENDURANCE UINT32_C(2)
#define AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_CLASSIFICATION UINT32_C(3)
#define AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_FOR_FOV_LIMIT UINT32_C(4)
#define AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_GATING UINT32_C(5)
#define AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_MANEUVER_LIMIT UINT32_C(6)
#define AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_OP UINT32_C(7)
#define AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_OCCLUSION UINT32_C(8)
#define AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_RANGE UINT32_C(9)
#define AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_PERFORMANCE UINT32_C(10)
#define AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_RF UINT32_C(11)
#define AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_ROUTE UINT32_C(12)
#define AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_SAFETY UINT32_C(13)
#define AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_TARGET_ANGLE UINT32_C(14)
#define AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_TIME UINT32_C(15)
#define AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_SYSTEM UINT32_C(16)
#define AMS_MEL_IR_CANNOT_COMPLY_INFEASIBLE_ROUTE UINT32_C(17)
#define AMS_MEL_IR_CANNOT_COMPLY_MISSION_EVENT UINT32_C(18)
#define AMS_MEL_IR_CANNOT_COMPLY_STATE_OR_SETTINGS UINT32_C(19)
#define AMS_MEL_IR_CANNOT_COMPLY_STATE_OR_SETTINGS_CHANGE UINT32_C(20)
#define AMS_MEL_IR_CANNOT_COMPLY_SYSTEM_UNAVAILABLE UINT32_C(21)
#define AMS_MEL_IR_CANNOT_COMPLY_SYSTEM_FAULT UINT32_C(22)
#define AMS_MEL_IR_CANNOT_COMPLY_SYSTEM_CONFLICT UINT32_C(23)
#define AMS_MEL_IR_CANNOT_COMPLY_SUBSYSTEM_UNAVAILABLE UINT32_C(24)
#define AMS_MEL_IR_CANNOT_COMPLY_SUBSYSTEM_FAULT UINT32_C(25)
#define AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_FAULT UINT32_C(26)
#define AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_PRECEDENCE UINT32_C(27)
#define AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_UNAVAILABLE UINT32_C(28)
#define AMS_MEL_IR_CANNOT_COMPLY_INSUFFICIENT_RESOURCES UINT32_C(29)
#define AMS_MEL_IR_CANNOT_COMPLY_RANKING UINT32_C(30)
#define AMS_MEL_IR_CANNOT_COMPLY_WEATHER UINT32_C(31)
#define AMS_MEL_IR_CANNOT_COMPLY_INELIGIBLE_CONTROL_SOURCE UINT32_C(32)
#define AMS_MEL_IR_CANNOT_COMPLY_DEPENDENCY_PREDECESSOR UINT32_C(33)
#define AMS_MEL_IR_CANNOT_COMPLY_DEPENDENCY_ALL_OR_NOTHING UINT32_C(34)
#define AMS_MEL_IR_CANNOT_COMPLY_DEPENDENCY_EITHER_OR UINT32_C(35)
#define AMS_MEL_IR_CANNOT_COMPLY_INIT_CRITERIA_NOT_MET UINT32_C(36)
#define AMS_MEL_IR_CANNOT_COMPLY_UNKNOWN_ID UINT32_C(37)
#define AMS_MEL_IR_CANNOT_COMPLY_INVALID_INPUT_PARAMETER UINT32_C(38)
#define AMS_MEL_IR_CANNOT_COMPLY_INPUT_OTHER UINT32_C(39)
#define AMS_MEL_IR_CANNOT_COMPLY_MDF_ACTIVATION_ERROR UINT32_C(40)
#define AMS_MEL_IR_CANNOT_COMPLY_MULTIPLE UINT32_C(41)
#define AMS_MEL_IR_CANNOT_COMPLY_CANCELLED UINT32_C(42)
#define AMS_MEL_IR_CANNOT_COMPLY_OTHER UINT32_C(43)
#define AMS_MEL_IR_CANNOT_COMPLY_UNKNOWN UINT32_C(44)
#define AMS_MEL_IR_CANNOT_COMPLY_ABORTED UINT32_C(45)
#define AMS_MEL_IR_CANNOT_COMPLY_ALIGNMENT_MANEUVER UINT32_C(46)
typedef uint32_t ams_mel_bit_control_interface_t;
#define AMS_MEL_BIT_CONTROL_NOT_SET UINT32_C(0)
#define AMS_MEL_BIT_CONTROL_SUBSYSTEM_BIT_COMMAND UINT32_C(1)
#define AMS_MEL_BIT_CONTROL_SUBSYSTEM_STATE_COMMAND UINT32_C(2)
#define AMS_MEL_BIT_CONTROL_SUBSYSTEM_INITIATED UINT32_C(3)
typedef uint32_t ams_mel_bit_result_t;
#define AMS_MEL_BIT_RESULT_NOT_SET UINT32_C(0)
#define AMS_MEL_BIT_RESULT_PASS UINT32_C(1)
#define AMS_MEL_BIT_RESULT_FAIL UINT32_C(2)
#define AMS_MEL_BIT_RESULT_INTERRUPTED UINT32_C(3)
#define AMS_MEL_BIT_RESULT_NOT_TESTED UINT32_C(4)
typedef uint32_t ams_mel_fault_severity_t;
#define AMS_MEL_FAULT_SEVERITY_NOT_SET UINT32_C(0)
#define AMS_MEL_FAULT_SEVERITY_NOMINAL UINT32_C(1)
#define AMS_MEL_FAULT_SEVERITY_CAUTION UINT32_C(2)
#define AMS_MEL_FAULT_SEVERITY_WARNING UINT32_C(3)
#define AMS_MEL_FAULT_SEVERITY_FAILED UINT32_C(4)
typedef uint32_t ams_mel_fault_state_t;
#define AMS_MEL_FAULT_STATE_NOT_SET UINT32_C(0)
#define AMS_MEL_FAULT_STATE_SET UINT32_C(1)
#define AMS_MEL_FAULT_STATE_CLEARED UINT32_C(2)
#define AMS_MEL_FAULT_STATE_UNKNOWN UINT32_C(3)

typedef struct ams_mel_string_view_v1 {
    const char *data;
    size_t size;
} ams_mel_string_view_v1;

typedef struct ams_mel_u32_span_v1 {
    const uint32_t *data;
    size_t size;
} ams_mel_u32_span_v1;

typedef struct ams_mel_u8_span_v1 {
    const uint8_t *data;
    size_t size;
} ams_mel_u8_span_v1;

typedef struct ams_mel_string_view_span_v1 {
    const ams_mel_string_view_v1 *data;
    size_t size;
} ams_mel_string_view_span_v1;

typedef struct ams_mel_ir_scan_type_v1 {
    uint32_t continuous_scan;
    uint32_t returning;
    uint32_t agile_scan;
} ams_mel_ir_scan_type_v1;

typedef struct ams_mel_ir_scan_param_v1 {
    uint32_t elevation_defined_with_range_and_altitude;
    double center_az_rad;
    double center_el_rad;
    ams_mel_ir_coord_frame_ref_t center_frame_ref_el;
    ams_mel_ir_coord_frame_ref_t center_frame_ref_az;
    double scan_width_rad;
    double scan_height_rad;
    ams_mel_ir_scan_type_v1 scan_type;
    uint32_t scan_id;
    double scan_rate_rad_per_second;
    double preferred_revisit_interval_seconds;
    double required_revisit_interval_seconds;
    uint32_t max_range_of_interest_m;
    uint32_t min_range_of_interest_m;
    uint32_t elevation_scan_center_altitude_m;
    uint32_t elevation_scan_center_range_m;
    ams_mel_ir_degradation_method_t degradation_method;
} ams_mel_ir_scan_param_v1;

typedef struct ams_mel_ir_mode_command_v1 {
    uint32_t command_id;
    ams_mel_ir_mfa_state_t state;
    ams_mel_ir_mfa_mode_t mode;
    ams_mel_ir_scan_param_v1 scan_parameters;
} ams_mel_ir_mode_command_v1;

typedef struct ams_mel_ir_bit_command_v1 {
    uint32_t command_id;
    ams_mel_u32_span_v1 initiate_bit_ids;
    ams_mel_u32_span_v1 cancel_bit_ids;
    ams_mel_string_view_span_v1 clear_fault_codes;
} ams_mel_ir_bit_command_v1;

typedef struct ams_mel_ir_config_set_command_v1 {
    uint32_t command_id;
    int64_t system_time_ns;
    ams_mel_string_view_v1 config;
} ams_mel_ir_config_set_command_v1;

typedef struct ams_mel_uci_id_v1 {
    uint8_t uuid[16];
    ams_mel_string_view_v1 descriptive_label;
} ams_mel_uci_id_v1;

#define AMS_MEL_DECLARE_SPAN(name, element) \
    typedef struct name { const element *data; size_t size; } name
AMS_MEL_DECLARE_SPAN(ams_mel_uci_id_span_v1, ams_mel_uci_id_v1);

typedef struct ams_mel_ir_command_status_v1 {
    uint32_t command_id;
    ams_mel_ir_command_state_t state;
    ams_mel_ir_cannot_comply_t reason_id;
    ams_mel_string_view_v1 reason_description;
} ams_mel_ir_command_status_v1;
typedef struct ams_mel_bit_type_v1 {
    ams_mel_uci_id_v1 bit_id;
    ams_mel_bit_control_interface_t accepted_interface;
    ams_mel_string_view_span_v1 bit_item_names;
    ams_mel_uci_id_span_v1 subsystem_component_ids;
    int64_t expected_duration_ns;
} ams_mel_bit_type_v1;
AMS_MEL_DECLARE_SPAN(ams_mel_bit_type_span_v1, ams_mel_bit_type_v1);
typedef struct ams_mel_bit_configuration_v1 {
    ams_mel_bit_type_span_v1 bit_types;
} ams_mel_bit_configuration_v1;
typedef struct ams_mel_active_bit_v1 {
    ams_mel_uci_id_v1 bit_id;
    int64_t estimated_completion_time_ns;
    double estimated_percent_complete;
} ams_mel_active_bit_v1;
AMS_MEL_DECLARE_SPAN(ams_mel_active_bit_span_v1, ams_mel_active_bit_v1);
typedef struct ams_mel_completed_bit_item_v1 {
    ams_mel_string_view_v1 bit_item_name;
    ams_mel_bit_result_t result;
    ams_mel_string_view_v1 fail_reason;
} ams_mel_completed_bit_item_v1;
AMS_MEL_DECLARE_SPAN(ams_mel_completed_bit_item_span_v1, ams_mel_completed_bit_item_v1);
typedef struct ams_mel_completed_bit_v1 {
    ams_mel_uci_id_v1 bit_id;
    int64_t time_tag_ns;
    ams_mel_bit_result_t result;
    ams_mel_string_view_v1 fail_reason;
    ams_mel_completed_bit_item_span_v1 bit_items;
} ams_mel_completed_bit_v1;
AMS_MEL_DECLARE_SPAN(ams_mel_completed_bit_span_v1, ams_mel_completed_bit_v1);
typedef struct ams_mel_fault_data_v1 {
    ams_mel_string_view_v1 key, value, format, units;
} ams_mel_fault_data_v1;
AMS_MEL_DECLARE_SPAN(ams_mel_fault_data_span_v1, ams_mel_fault_data_v1);
typedef struct ams_mel_fault_ambiguity_group_v1 {
    ams_mel_uci_id_span_v1 diagnostic_test_ids;
    ams_mel_uci_id_span_v1 component_ids;
} ams_mel_fault_ambiguity_group_v1;
AMS_MEL_DECLARE_SPAN(ams_mel_fault_ambiguity_group_span_v1, ams_mel_fault_ambiguity_group_v1);
typedef struct ams_mel_fault_v1 {
    ams_mel_uci_id_v1 fault_id;
    ams_mel_fault_severity_t severity;
    ams_mel_fault_state_t state;
    ams_mel_fault_data_span_v1 fault_data;
    int64_t detection_time_ns;
    ams_mel_string_view_v1 fault_code;
    ams_mel_string_view_v1 fault_description;
    ams_mel_uci_id_span_v1 component_ids;
    ams_mel_fault_ambiguity_group_span_v1 ambiguity_groups;
} ams_mel_fault_v1;
AMS_MEL_DECLARE_SPAN(ams_mel_fault_span_v1, ams_mel_fault_v1);
typedef struct ams_mel_bit_status_v1 {
    ams_mel_active_bit_span_v1 active_bits;
    ams_mel_completed_bit_span_v1 completed_bits;
    ams_mel_fault_span_v1 faults;
} ams_mel_bit_status_v1;
typedef struct ams_mel_ir_channel_comms_test_report_v1 {
    uint32_t command_id;
    uint32_t request_id;
} ams_mel_ir_channel_comms_test_report_v1;
typedef struct ams_mel_ir_c2_metadata_event_v1 {
    ams_mel_ir_c2_metadata_kind_t kind;
    ams_mel_ir_command_status_v1 command_status;
    ams_mel_bit_configuration_v1 bit_configuration;
    ams_mel_bit_status_v1 bit_status;
    ams_mel_ir_channel_comms_test_report_v1 channel_comms_test;
} ams_mel_ir_c2_metadata_event_v1;
typedef struct ams_mel_ir_bad_pixel_v1 {
    uint32_t row;
    uint32_t column;
    ams_mel_ir_bad_pixel_reason_t reason;
} ams_mel_ir_bad_pixel_v1;
AMS_MEL_DECLARE_SPAN(ams_mel_ir_bad_pixel_span_v1, ams_mel_ir_bad_pixel_v1);
typedef struct ams_mel_ir_bad_pixel_list_v1 {
    uint32_t reported_size;
    uint32_t reported_count;
    ams_mel_ir_bad_pixel_span_v1 pixels;
} ams_mel_ir_bad_pixel_list_v1;
typedef struct ams_mel_euler_v1 { double roll, pitch, yaw; } ams_mel_euler_v1;
typedef struct ams_mel_ir_az_el_v1 {
    double azimuth_rad;
    double elevation_rad;
} ams_mel_ir_az_el_v1;
typedef struct ams_mel_ir_line_of_sight_report_v1 {
    int64_t system_time_ns;
    ams_mel_ir_az_el_v1 pointing_angle;
    ams_mel_ir_az_el_v1 pointing_angle_rates;
    uint8_t at_speed;
    uint8_t in_tolerance;
    ams_mel_euler_v1 platform_attitude;
    uint32_t validity_flag_bitfield;
    double image_rotation_rad;
} ams_mel_ir_line_of_sight_report_v1;
typedef struct ams_mel_ir_line_of_sight_euler_v1 {
    int64_t system_time_ns;
    ams_mel_euler_v1 attitude;
    ams_mel_euler_v1 attitude_rates;
} ams_mel_ir_line_of_sight_euler_v1;
typedef struct ams_mel_ir_navigation_response_v1 {
    int64_t system_time_ns;
    uint32_t command_id;
    uint32_t request_id;
} ams_mel_ir_navigation_response_v1;
typedef struct ams_mel_ir_image_metadata_event_v1 {
    ams_mel_ir_image_metadata_kind_t kind;
    ams_mel_ir_bad_pixel_list_v1 bad_pixel_list;
    ams_mel_ir_line_of_sight_report_v1 line_of_sight_report;
    ams_mel_ir_line_of_sight_euler_v1 line_of_sight_euler;
    ams_mel_ir_navigation_response_v1 navigation_response;
} ams_mel_ir_image_metadata_event_v1;
typedef struct ams_mel_ir_c2_metadata_counters_v1 {
    uint64_t events_received;
    uint64_t events_dropped_queue_full;
    uint64_t malformed_or_unsupported;
} ams_mel_ir_c2_metadata_counters_v1;
typedef ams_mel_ir_c2_metadata_counters_v1 ams_mel_ir_metadata_counters_v1;
#undef AMS_MEL_DECLARE_SPAN

typedef struct ams_mel_component_location_v1 {
    double offset_x_m;
    double offset_y_m;
    double offset_z_m;
    ams_mel_string_view_v1 key;
    ams_mel_string_view_v1 system_name;
} ams_mel_component_location_v1;

typedef struct ams_mel_ir_channel_comms_test_request_v1 {
    uint32_t command_id;
    uint32_t channel_id;
    uint32_t request_id;
} ams_mel_ir_channel_comms_test_request_v1;
typedef struct ams_mel_ir_channel_comms_test_result_v1 {
    uint32_t command_id;
    uint32_t request_id;
    ams_mel_error_code_t error_code;
} ams_mel_ir_channel_comms_test_result_v1;
typedef struct ams_mel_ir_band_info_v1 {
    ams_mel_ir_band_type_t type;
    double min_wavelength_m;
    double max_wavelength_m;
} ams_mel_ir_band_info_v1;
typedef struct ams_mel_ir_band_info_span_v1 {
    const ams_mel_ir_band_info_v1 *data;
    size_t size;
} ams_mel_ir_band_info_span_v1;
typedef struct ams_mel_ir_image_band_v1 {
    uint32_t band_index;
    ams_mel_ir_band_info_span_v1 bands;
} ams_mel_ir_image_band_v1;
typedef struct ams_mel_ir_image_band_span_v1 {
    const ams_mel_ir_image_band_v1 *data;
    size_t size;
} ams_mel_ir_image_band_span_v1;
typedef struct ams_mel_ir_channel_capability_v1 {
    ams_mel_uci_id_v1 channel_id;
    uint32_t height, width, bit_depth, row_pitch, buffer_size, image_size;
    uint32_t number_of_bands;
    ams_mel_ir_pixel_format_t pixel_format;
    ams_mel_u32_span_v1 sensor_types;
    ams_mel_uci_id_v1 platform_id;
    ams_mel_component_location_v1 sensor_location;
    ams_mel_u32_span_v1 channel_types;
    uint32_t task_schedule_depth;
    uint32_t odc_available;
    uint32_t nuc_available;
    ams_mel_u32_span_v1 metadata_capabilities;
    ams_mel_ir_image_band_span_v1 image_bands;
    ams_mel_u32_span_v1 nav_frames;
} ams_mel_ir_channel_capability_v1;

/* Host-memory-only IRSTImage configuration. Labels and location strings are
 * UTF-8 byte views, need not be NUL-terminated, and are copied during open.
 * buffer_size bounds every provider buffer; queue_capacity uses DROP-INCOMING.
 */
typedef struct ams_mel_ir_stream_config_v1 {
    ams_mel_ir_channel_type_t channel_type;
    ams_mel_uci_id_v1 channel_id;
    ams_mel_uci_id_v1 platform_id;
    ams_mel_component_location_v1 sensor_location;
    size_t buffer_count;
    size_t buffer_size;
    size_t queue_capacity;
} ams_mel_ir_stream_config_v1;

/* Command-and-control configuration. No image listener or image-buffer fields
 * belong to this channel. String views are copied during open and use the same
 * UTF-8/no-embedded-NUL contract as the image configuration. */
typedef struct ams_mel_ir_c2_config_v1 {
    ams_mel_ir_channel_type_t channel_type;
    ams_mel_uci_id_v1 channel_id;
    ams_mel_uci_id_v1 platform_id;
    ams_mel_component_location_v1 sensor_location;
} ams_mel_ir_c2_config_v1;

typedef struct ams_mel_ir_health_config_v1 {
    ams_mel_uci_id_v1 channel_id;
    ams_mel_ir_channel_type_t channel_type;
    ams_mel_uci_id_v1 platform_id;
    ams_mel_component_location_v1 sensor_location;
} ams_mel_ir_health_config_v1;

typedef struct ams_mel_foreign_key_v1 {
    ams_mel_string_view_v1 key, system_name;
} ams_mel_foreign_key_v1;
typedef struct ams_mel_installation_details_v1 {
    ams_mel_component_location_v1 location;
    ams_mel_euler_v1 orientation, boresight;
} ams_mel_installation_details_v1;
typedef struct ams_mel_temperature_status_v1 {
    double temperature_c;
    ams_mel_temperature_state_t state;
} ams_mel_temperature_status_v1;
typedef struct ams_mel_mfa_component_v1 {
    ams_mel_uci_id_v1 component_id;
    ams_mel_component_state_t state;
    ams_mel_temperature_status_v1 temperature;
    ams_mel_foreign_key_v1 installation_location_id;
    ams_mel_installation_details_v1 installation_details;
} ams_mel_mfa_component_v1;
typedef struct ams_mel_mfa_component_span_v1 {
    const ams_mel_mfa_component_v1 *data; size_t size;
} ams_mel_mfa_component_span_v1;
typedef struct ams_mel_about_v1 {
    ams_mel_string_view_v1 model, serial_number, software_version;
    ams_mel_string_view_v1 bootloader_software_version, hardware_version;
} ams_mel_about_v1;
typedef struct ams_mel_mfa_status_v1 {
    ams_mel_ir_mfa_state_t state;
    ams_mel_string_view_v1 state_description, mode_description;
    ams_mel_state_transition_status_t transition_status;
    ams_mel_about_v1 about;
    ams_mel_mfa_component_span_v1 components;
} ams_mel_mfa_status_v1;
typedef struct ams_mel_ir_subsystem_dep_info_v1 {
    uint32_t subsystem_id, criticality;
    ams_mel_ir_failure_t failure;
} ams_mel_ir_subsystem_dep_info_v1;
typedef struct ams_mel_ir_subsystem_dep_info_span_v1 {
    const ams_mel_ir_subsystem_dep_info_v1 *data; size_t size;
} ams_mel_ir_subsystem_dep_info_span_v1;
typedef struct ams_mel_ir_version_v1 {
    uint32_t source, major_revision, minor_revision, engineering_revision;
} ams_mel_ir_version_v1;
typedef struct ams_mel_ir_subsystem_csci_info_v1 {
    ams_mel_string_view_v1 csci;
    ams_mel_ir_csci_mode_t mode;
    ams_mel_ir_version_v1 version;
    uint32_t criticality;
    ams_mel_ir_failure_t failure;
    uint32_t bit_report, connection_established;
} ams_mel_ir_subsystem_csci_info_v1;
typedef struct ams_mel_ir_subsystem_csci_info_span_v1 {
    const ams_mel_ir_subsystem_csci_info_v1 *data; size_t size;
} ams_mel_ir_subsystem_csci_info_span_v1;
typedef struct ams_mel_ir_subsystem_status_v1 {
    uint32_t subsystem_id, criticality, status_sequence_number;
    ams_mel_ir_failure_t failure;
    uint32_t subsystem_count;
    ams_mel_ir_subsystem_dep_info_span_v1 subsystems;
    uint32_t csci_count;
    ams_mel_ir_subsystem_csci_info_span_v1 csci;
} ams_mel_ir_subsystem_status_v1;
typedef struct ams_mel_name_value_pair_v1 {
    ams_mel_string_view_v1 name, value;
} ams_mel_name_value_pair_v1;
typedef struct ams_mel_name_value_pair_span_v1 {
    const ams_mel_name_value_pair_v1 *data; size_t size;
} ams_mel_name_value_pair_span_v1;
typedef struct ams_mel_security_artifact_v1 {
    ams_mel_uci_id_v1 component_id, associated_id;
} ams_mel_security_artifact_v1;
typedef struct ams_mel_security_artifact_span_v1 {
    const ams_mel_security_artifact_v1 *data; size_t size;
} ams_mel_security_artifact_span_v1;
typedef struct ams_mel_security_event_v1 {
    ams_mel_security_event_kind_t kind;
    ams_mel_security_category_t category;
    ams_mel_string_view_v1 details;
    ams_mel_uci_id_v1 subsystem_id, service_id, mdf_id;
} ams_mel_security_event_v1;
typedef struct ams_mel_security_audit_record_v1 {
    ams_mel_uci_id_v1 security_event_id;
    int64_t event_timestamp_ns;
    ams_mel_uci_id_v1 subsystem_id;
    ams_mel_security_artifact_span_v1 artifacts;
    ams_mel_security_event_v1 event;
    ams_mel_security_outcome_t outcome;
    ams_mel_security_severity_t severity;
} ams_mel_security_audit_record_v1;
typedef uint32_t ams_mel_ir_health_metadata_kind_t;
#define AMS_MEL_IR_HEALTH_METADATA_MFA_STATUS UINT32_C(1)
#define AMS_MEL_IR_HEALTH_METADATA_BIT_STATUS UINT32_C(2)
#define AMS_MEL_IR_HEALTH_METADATA_SUBSYSTEM_STATUS UINT32_C(3)
#define AMS_MEL_IR_HEALTH_METADATA_DISCRETE_STATUS UINT32_C(4)
#define AMS_MEL_IR_HEALTH_METADATA_SECURITY_AUDIT UINT32_C(5)
#define AMS_MEL_IR_HEALTH_METADATA_MFA_STATUS_DETAILED UINT32_C(6)
typedef struct ams_mel_ir_health_metadata_event_v1 {
    ams_mel_ir_health_metadata_kind_t kind;
    ams_mel_mfa_status_v1 mfa_status;
    ams_mel_bit_status_v1 bit_status;
    ams_mel_ir_subsystem_status_v1 subsystem_status;
    ams_mel_name_value_pair_span_v1 discrete_status;
    ams_mel_security_audit_record_v1 security_audit;
    ams_mel_name_value_pair_span_v1 mfa_status_detailed;
} ams_mel_ir_health_metadata_event_v1;

/* Terminal ModeCmd value. On COMMAND_REJECTED, error_code is populated and
 * the per-call diagnostic contains the provider's validated description. */
typedef struct ams_mel_ir_mode_result_v1 {
    ams_mel_ir_mfa_mode_t mode;
    ams_mel_error_code_t error_code;
} ams_mel_ir_mode_result_v1;

/* On AMS_MEL_OK, value is the completed upstream Return; Return::Fail is still
 * AMS_MEL_OK, and error_code is AMS_MEL_ERROR_NONE for every normal Return
 * completion. On AMS_MEL_COMMAND_REJECTED, error_code contains the mapped MEL
 * error and the per-call diagnostic contains its description. For provider or
 * facade failure statuses, output fields must not be treated as successful
 * values. */
typedef struct ams_mel_ir_return_result_v1 {
    ams_mel_ir_return_t value;
    ams_mel_error_code_t error_code;
} ams_mel_ir_return_result_v1;

/* Metadata copied with each Mono8 frame. Times retain upstream nanoseconds;
 * FOV values retain upstream radians. image_flags is a bitset (1 << ImageFlag).
 */
typedef struct ams_mel_ir_frame_v1 {
    int64_t system_time_ns;
    int64_t integration_time_ns;
    uint32_t width;
    uint32_t height;
    uint32_t bits_per_pixel;
    uint32_t number_of_bands;
    double horizontal_fov_rad;
    double vertical_fov_rad;
    ams_mel_ir_pixel_format_t pixel_format;
    uint32_t frame_id;
    uint32_t subframe_id;
    uint32_t subframe_total;
    ams_mel_ir_image_type_t image_type;
    ams_mel_ir_image_flip_t image_flip;
    uint32_t image_flags;
    double dither_row;
    double dither_column;
    uint32_t row_offset;
    uint32_t column_offset;
    uint8_t band_index;
    uint8_t reserved[7];
    uint8_t *pixels;
    size_t pixel_capacity;
    size_t pixel_required;
} ams_mel_ir_frame_v1;

/* Complete immutable FrameHeader snapshot.  Unlike the legacy frame_v1 this
 * record is borrowed from an opaque owner and may contain nested spans. */
typedef struct ams_mel_ir_contributing_sensor_v1 {
    ams_mel_component_location_v1 location;
    uint32_t sensor_id;
} ams_mel_ir_contributing_sensor_v1;
typedef struct ams_mel_ir_directional_v1 { double x, y, z; } ams_mel_ir_directional_v1;
typedef struct ams_mel_ir_quaternion_v1 { double x, y, z, w; } ams_mel_ir_quaternion_v1;
typedef struct ams_mel_ir_nav_error_v1 { double x, y, z, w; } ams_mel_ir_nav_error_v1;
typedef struct ams_mel_ir_uncertainty_v1 {
    uint32_t sensor_uncertainties;
    uint32_t platform_uncertainties;
} ams_mel_ir_uncertainty_v1;
typedef uint32_t ams_mel_ir_orientation_kind_t;
#define AMS_MEL_IR_ORIENTATION_EULER UINT32_C(0)
#define AMS_MEL_IR_ORIENTATION_QUATERNION UINT32_C(1)
typedef struct ams_mel_ir_orientation_v1 {
    ams_mel_ir_orientation_kind_t kind;
    ams_mel_euler_v1 euler;
    ams_mel_ir_quaternion_v1 quaternion;
} ams_mel_ir_orientation_v1;
typedef struct ams_mel_ir_sensor_inertial_state_v1 {
    int64_t system_time_ns;
    ams_mel_ir_quaternion_v1 q_xyzw;
    ams_mel_ir_quaternion_v1 q_ecef_xyzw;
    ams_mel_ir_directional_v1 sensor_position;
    ams_mel_ir_directional_v1 sensor_velocity;
    ams_mel_ir_uncertainty_v1 uncertainties;
} ams_mel_ir_sensor_inertial_state_v1;
typedef struct ams_mel_ir_sensor_nav_state_v1 {
    ams_mel_ir_directional_v1 position;
    ams_mel_ir_nav_error_v1 position_error;
    ams_mel_ir_directional_v1 velocity;
    ams_mel_ir_nav_error_v1 velocity_error;
    ams_mel_ir_directional_v1 acceleration;
    ams_mel_ir_nav_error_v1 acceleration_error;
    ams_mel_ir_orientation_v1 orientation;
    ams_mel_ir_nav_error_v1 orientation_error;
    ams_mel_ir_orientation_v1 orientation_velocity;
    ams_mel_ir_nav_error_v1 orientation_velocity_error;
    ams_mel_ir_orientation_v1 orientation_acceleration;
    ams_mel_ir_nav_error_v1 orientation_acceleration_error;
    uint32_t coordinate_system;
} ams_mel_ir_sensor_nav_state_v1;
typedef struct ams_mel_ir_sensor_inertial_state_span_v1 {
    const ams_mel_ir_sensor_inertial_state_v1 *data; size_t size;
} ams_mel_ir_sensor_inertial_state_span_v1;
typedef struct ams_mel_ir_sensor_nav_state_span_v1 {
    const ams_mel_ir_sensor_nav_state_v1 *data; size_t size;
} ams_mel_ir_sensor_nav_state_span_v1;
typedef struct ams_mel_ir_frame_snapshot_v1 {
    int64_t system_time_ns, integration_time_ns;
    uint32_t width, height, bits_per_pixel, number_of_bands;
    double horizontal_fov_rad, vertical_fov_rad;
    ams_mel_ir_contributing_sensor_v1 contributing_sensor;
    uint32_t pixel_format, frame_id, subframe_id, subframe_total, image_type, image_flip;
    ams_mel_u32_span_v1 image_flags;
    double dither_row, dither_column;
    uint32_t row_offset, column_offset;
    ams_mel_ir_sensor_inertial_state_span_v1 sensor_inertial_states;
    ams_mel_ir_sensor_nav_state_span_v1 sensor_nav_states;
    uint8_t band_index;
    ams_mel_u8_span_v1 pixels;
} ams_mel_ir_frame_snapshot_v1;

typedef struct ams_mel_ir_stream_counters_v1 {
    uint64_t frames_received;
    uint64_t frames_dropped_queue_full;
    uint64_t malformed_or_unsupported_frames;
} ams_mel_ir_stream_counters_v1;

/* Complete upstream mel::VersionInfo value represented without C++ storage.
 * api_version and library_version are provider values, not facade versions.
 * String capacities and required sizes count bytes including the trailing NUL.
 * Strings are UTF-8 in this profile and are copied into caller-owned buffers.
 */
typedef struct ams_mel_provider_version_v1 {
    uint32_t api_version;
    uint32_t library_version;
    char *vendor;
    size_t vendor_capacity;
    size_t vendor_required;
    char *description;
    size_t description_capacity;
    size_t description_required;
} ams_mel_provider_version_v1;

/* Writes both fields on success. NULL returns AMS_MEL_INVALID_ARGUMENT.
 * out_version must point to valid, writable storage for this exact record.
 * No allocation, provider load, global mutation, or callback is performed.
 * Safe to call concurrently with independent output records.
 */
AMS_MEL_API ams_mel_status_t ams_mel_get_abi_version(
    ams_mel_abi_version_v1 *out_version) AMS_MEL_NOEXCEPT;

/* Opens library_path with the platform dynamic loader, resolves the pinned IR
 * MEL getAPI_Manager/getControl C-linkage exports, creates both objects, and
 * calls Control::init(aperture_config_id). All input strings must be valid,
 * NUL-terminated UTF-8. out_session must point to a NULL owner. On every
 * failure it remains NULL. A successful handle is uniquely owned by the caller.
 * diagnostic is optional. Diagnostics are valid UTF-8. When supplied,
 * diagnostic_required receives the untruncated byte count including NUL;
 * diagnostic may be truncated to a valid UTF-8 prefix that fits capacity.
 */
AMS_MEL_API ams_mel_status_t ams_mel_session_open(
    const char *library_path,
    const char *instance,
    const char *aperture_config_id,
    ams_mel_session **out_session,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Queries Control::getVersionInfo. On success every field is written and both
 * strings are complete. With NULL/zero string buffers, or insufficient
 * capacity, returns AMS_MEL_BUFFER_TOO_SMALL, writes only *_required, and does
 * not modify numeric fields or buffers. The session must be a live handle
 * returned by open; arbitrary or dangling pointers are outside the contract.
 */
AMS_MEL_API ams_mel_status_t ams_mel_session_get_provider_version(
    const ams_mel_session *session,
    ams_mel_provider_version_v1 *out_version,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Destroys Control, then API_Manager, then unloads the provider library, and
 * clears the caller's owner. Passing a valid owner already cleared to NULL is
 * a successful no-op. session itself must not be NULL. No other thread may use
 * the same session while this operation runs.
 */
AMS_MEL_API ams_mel_status_t ams_mel_session_close(
    ams_mel_session **session,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Creates and attaches one IRSTImage channel. The stream retains the provider
 * lifetime independently of the parent session. The session and stream calls
 * must be externally serialized during open/close. out_stream must be NULL.
 * Provider callbacks are internally synchronized with receive and counters.
 * At most one thread may execute receive for a given stream at a time.
 * Start/stop/close on the same stream must otherwise be externally serialized;
 * stop may run while one receive waits and will wake it as STREAM_STOPPED.
 */
AMS_MEL_API ams_mel_status_t ams_mel_ir_stream_open(
    const ams_mel_session *session,
    const ams_mel_ir_stream_config_v1 *config,
    ams_mel_ir_stream **out_stream,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Calls getBuffer/init/registerBuffer for every configured host buffer, then
 * enables the channel. Partial failure poisons and safely tears down the stream;
 * Start cannot retry a failed provider path.
 */
AMS_MEL_API ams_mel_status_t ams_mel_ir_stream_start(
    ams_mel_ir_stream *stream,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Waits at most timeout_ms for a copied frame. A zero timeout polls. Timeout is
 * not cancellation. pixels is caller-owned. BUFFER_TOO_SMALL writes only
 * pixel_required and leaves the queued frame available; no partial frame is
 * returned. One successful call removes one frame from the queue. At most one
 * receive operation may consume a stream at a time. Frames already queued are
 * drained before STREAM_STOPPED or PROVIDER_FAILED is returned.
 * The copied output remains valid in caller storage until the caller changes it.
 */
AMS_MEL_API ams_mel_status_t ams_mel_ir_stream_receive(
    ams_mel_ir_stream *stream,
    uint32_t timeout_ms,
    ams_mel_ir_frame_v1 *out_frame,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Both receive operations consume the same FIFO; externally serialize all
 * receive calls for a stream. The returned immutable owner is independent of
 * stream, Session, and provider lifetime until snapshot_close. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_stream_receive_snapshot(
    ams_mel_ir_stream *stream, uint32_t timeout_ms,
    ams_mel_ir_frame_snapshot **out_snapshot, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Deep-copy ImageChannel capability snapshot. The returned owner remains valid
 * after Image_Stream, Session, and provider-library teardown. This call must be
 * externally serialized with stream_start, stream_stop, and stream_close. It
 * may execute while provider frame callbacks are occurring. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_stream_get_capabilities(
    ams_mel_ir_stream *stream, ams_mel_ir_channel_capability **out_capability,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_frame_snapshot_view(
    const ams_mel_ir_frame_snapshot *snapshot,
    const ams_mel_ir_frame_snapshot_v1 **out_view, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_frame_snapshot_close(
    ams_mel_ir_frame_snapshot **snapshot, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

AMS_MEL_API ams_mel_status_t ams_mel_ir_stream_get_counters(
    const ams_mel_ir_stream *stream,
    ams_mel_ir_stream_counters_v1 *out_counters,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* BadPixelList is the sole Image metadata event in Task 024. The FIFO is
 * bounded DROP-INCOMING; at most one receive may execute per owner. Metadata
 * open must be externally serialized with stream_start, stream_stop, and
 * stream_close. Provider callbacks may execute synchronously during open. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_image_metadata_open(
    ams_mel_ir_stream *stream, size_t queue_capacity,
    ams_mel_ir_image_metadata **out_metadata, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_image_metadata_receive(
    ams_mel_ir_image_metadata *metadata, uint32_t timeout_ms,
    ams_mel_ir_image_metadata_event **out_event, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_image_metadata_get_counters(
    const ams_mel_ir_image_metadata *metadata,
    ams_mel_ir_metadata_counters_v1 *out_counters, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Idempotent and nonblocking. It does not unregister the provider callback.
 * Close must not race receive or counters using the same public metadata
 * handle. Provider callbacks may race public close safely because callback
 * state is stream-owned. Event handles remain independent. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_image_metadata_close(
    ams_mel_ir_image_metadata **metadata, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_image_metadata_event_view(
    const ams_mel_ir_image_metadata_event *event,
    const ams_mel_ir_image_metadata_event_v1 **out_view, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_image_metadata_event_close(
    ams_mel_ir_image_metadata_event **event, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Stops acceptance, calls disable, detaches, destroys the provider channel,
 * waits for adapter callbacks already in flight, then destroys buffers/storage.
 * Calls are idempotent after a successful stop. disable is not treated as a
 * callback-quiescence boundary. Provider cleanup failure poisons the stream and
 * is reported as PROVIDER_FAILED; callback-accessible resources are retained
 * when a safe channel-destruction boundary cannot be established.
 */
AMS_MEL_API ams_mel_status_t ams_mel_ir_stream_stop(
    ams_mel_ir_stream *stream,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Stops if necessary and releases retained provider state. The unique owner is
 * cleared after safe teardown even when teardown reports PROVIDER_FAILED.
 * If safe teardown cannot be established, the owner remains for a later retry.
 * Closing an already-cleared owner succeeds.
 */
AMS_MEL_API ams_mel_status_t ams_mel_ir_stream_close(
    ams_mel_ir_stream **stream,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Attaches only a published CommandAndControl channel after checking both the
 * Control capability list and attached channel type/capability. The C2 owner
 * retains SessionState independently of the public Session owner. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_c2_open(
    const ams_mel_session *session,
    const ams_mel_ir_c2_config_v1 *config,
    ams_mel_ir_c2 **out_c2,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Explicitly enables the attached C2 channel. Submission never implicitly
 * enables it. Enable, submit, and close calls using the same C2 owner must be
 * externally serialized. Parent Session close rules remain unchanged. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_c2_enable(
    ams_mel_ir_c2 *c2,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Sends exactly Operate/TaskSched with default ScanParam. On success publishes
 * an asynchronous request owner without waiting for its future. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_c2_submit_operate(
    ams_mel_ir_c2 *c2,
    uint32_t command_id,
    ams_mel_ir_mode_request **out_request,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Copies a complete published ModeCmd before provider send. Unknown enum
 * values, MaxExclusive, and Boolean values other than 0/1 are rejected. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_c2_submit_mode(
    ams_mel_ir_c2 *c2,
    const ams_mel_ir_mode_command_v1 *command,
    ams_mel_ir_mode_request **out_request,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Waits finitely for completion. TIMEOUT neither consumes nor cancels. A
 * terminal result is cached, so repeated waits are inspectable and future::get
 * is performed once by the adapter completion worker. Wait may be repeated,
 * but close must not race a wait using the same request handle. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_mode_request_wait(
    const ams_mel_ir_mode_request *request,
    uint32_t timeout_ms,
    ams_mel_ir_mode_result_v1 *out_result,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Drops only the public request owner. It is idempotent, nonblocking, and does
 * not cancel pending provider work. Internal ownership survives to completion. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_mode_request_close(
    ams_mel_ir_mode_request **request,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Sends the pinned-provider-compatible BIT profile: command ID plus empty
 * initiate, cancel, and clear-fault lists. Payload-bearing BIT is not exposed. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_c2_submit_bit_noop(
    ams_mel_ir_c2 *c2,
    uint32_t command_id,
    ams_mel_ir_return_request **out_request,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* All borrowed spans and UTF-8 views are synchronously copied before send.
 * Null span data is valid only for size zero. Multiple populated BIT choices
 * are preserved so the provider can apply the published precedence rule. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_c2_submit_bit(
    ams_mel_ir_c2 *c2,
    const ams_mel_ir_bit_command_v1 *command,
    ams_mel_ir_return_request **out_request,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Config is an opaque validated UTF-8 string and may be empty. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_c2_submit_config_set(
    ams_mel_ir_c2 *c2,
    const ams_mel_ir_config_set_command_v1 *command,
    ams_mel_ir_return_request **out_request,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Common inherited Channel services are valid while attached or enabled. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_c2_send_keepalive(
    ams_mel_ir_c2 *c2, ams_mel_ir_return_request **out_request,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_c2_submit_comms_test(
    ams_mel_ir_c2 *c2,
    const ams_mel_ir_channel_comms_test_request_v1 *request,
    ams_mel_ir_channel_comms_request **out_request,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_channel_comms_request_wait(
    const ams_mel_ir_channel_comms_request *request, uint32_t timeout_ms,
    ams_mel_ir_channel_comms_test_result_v1 *out_result,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_channel_comms_request_close(
    ams_mel_ir_channel_comms_request **request, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_c2_get_capabilities(
    ams_mel_ir_c2 *c2, ams_mel_ir_channel_capability **out_capability,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_channel_capability_view(
    const ams_mel_ir_channel_capability *capability,
    const ams_mel_ir_channel_capability_v1 **out_view,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_channel_capability_close(
    ams_mel_ir_channel_capability **capability, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Registers the three required C2-specific metadata callbacks in this order:
 * BIT_Configuration, CommandStatus, BIT_Status. The queue exists before the
 * first registration and uses bounded FIFO DROP-INCOMING. Registration is
 * attempted at most once per C2 channel. There is no upstream unregister;
 * callback state survives public close and partial registration failure until
 * provider C2 channel destruction. This call does not enable C2. Metadata open
 * must be externally serialized with C2 enable, submit, and close operations. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_c2_metadata_open(
    ams_mel_ir_c2 *c2, size_t queue_capacity,
    ams_mel_ir_c2_metadata **out_metadata, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Registers the inherited CommsTest callback into this existing queue exactly
 * once. Failure does not release callback-accessible state and cannot be retried. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_c2_metadata_register_comms_test(
    ams_mel_ir_c2_metadata *metadata, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Finite wait; zero polls and timeout is not cancellation. At most one receive
 * may run per owner. Queued events drain before STOPPED or PROVIDER_FAILED. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_c2_metadata_receive(
    ams_mel_ir_c2_metadata *metadata, uint32_t timeout_ms,
    ams_mel_ir_c2_metadata_event **out_event, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_c2_metadata_get_counters(
    const ams_mel_ir_c2_metadata *metadata,
    ams_mel_ir_c2_metadata_counters_v1 *out_counters, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Idempotent and nonblocking. It deactivates public consumption/enqueueing but
 * cannot unregister provider callbacks and does not close C2 or requests.
 * Close must not race another call using this public metadata handle. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_c2_metadata_close(
    ams_mel_ir_c2_metadata **metadata, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* The view and all nested pointers borrow immutable adapter-owned event storage
 * and remain valid only until event_close. The event itself is independent of
 * metadata, C2, Session, and provider-library lifetime. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_c2_metadata_event_view(
    const ams_mel_ir_c2_metadata_event *event,
    const ams_mel_ir_c2_metadata_event_v1 **out_view, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_c2_metadata_event_close(
    ams_mel_ir_c2_metadata_event **event, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Generic RequestFor<Return> wait. Return::Fail is a normal AMS_MEL_OK result;
 * it is not a provider/facade failure. Terminal results are cached exactly as
 * for mode requests, and timeout neither consumes nor cancels the request. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_return_request_wait(
    const ams_mel_ir_return_request *request,
    uint32_t timeout_ms,
    ams_mel_ir_return_result_v1 *out_result,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Idempotent and nonblocking. Drops only the public owner and does not cancel
 * pending provider work. Close must not race wait on the same owner. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_return_request_close(
    ams_mel_ir_return_request **request,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Stops new submissions. With no requests, disables and detaches synchronously.
 * In-flight requests defer cleanup until the final completion and retain the
 * provider/library meanwhile. A synchronous detach failure retains the public
 * owner for a later close retry. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_c2_close(
    ams_mel_ir_c2 **c2,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* HealthAndStatus owner. Metadata registration and capability snapshots are
 * valid while attached or enabled. Close retains the owner when detach fails. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_health_open(
    const ams_mel_session *session, const ams_mel_ir_health_config_v1 *config,
    ams_mel_ir_health **out_health, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_health_enable(
    ams_mel_ir_health *health, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_health_get_capabilities(
    ams_mel_ir_health *health, ams_mel_ir_channel_capability **out_capability,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_health_close(
    ams_mel_ir_health **health, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Registration order is MFA_Status, BIT_Status, SubsystemStatusResp,
 * DiscreteStatus, MFA_SecurityAuditRecord, MFA_StatusDetailed. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_health_metadata_open(
    ams_mel_ir_health *health, size_t queue_capacity,
    ams_mel_ir_health_metadata **out_metadata, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_health_metadata_receive(
    ams_mel_ir_health_metadata *metadata, uint32_t timeout_ms,
    ams_mel_ir_health_metadata_event **out_event, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_health_metadata_get_counters(
    const ams_mel_ir_health_metadata *metadata,
    ams_mel_ir_metadata_counters_v1 *out_counters, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_health_metadata_close(
    ams_mel_ir_health_metadata **metadata, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_health_metadata_event_view(
    const ams_mel_ir_health_metadata_event *event,
    const ams_mel_ir_health_metadata_event_v1 **out_view, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_health_metadata_event_close(
    ams_mel_ir_health_metadata_event **event, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#endif /* AMS_MEL_ABI_H */
