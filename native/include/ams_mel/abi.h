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
typedef struct ams_mel_ir_navigation_request ams_mel_ir_navigation_request;
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
typedef struct ams_mel_ir_instrumentation ams_mel_ir_instrumentation;
typedef struct ams_mel_ir_instrumentation_request ams_mel_ir_instrumentation_request;
typedef struct ams_mel_ir_instrumentation_metadata ams_mel_ir_instrumentation_metadata;
typedef struct ams_mel_ir_instrumentation_metadata_event
    ams_mel_ir_instrumentation_metadata_event;
/* Conditionally required Track channel (@RequiredIfTrack) owner. This release
 * implements channel ownership/lifecycle (Open, Enable, ChannelCapability,
 * Close) plus exactly the @RequiredIfTrack IRSTTrackReport metadata callback
 * and the @RequiredIfTrackUpdate TrackDataUpdate send. SystemTrackDataResponse
 * is not implemented. */
typedef struct ams_mel_ir_track ams_mel_ir_track;
/* Owns public consumption of the retained IRSTTrackReport callback queue. The
 * callback-accessible state itself belongs to the Track channel, not to this
 * wrapper, because upstream provides no unregister operation. */
typedef struct ams_mel_ir_track_metadata ams_mel_ir_track_metadata;
typedef struct ams_mel_ir_track_metadata_event ams_mel_ir_track_metadata_event;
/* Owns one asynchronous TrackChannel::send(TrackDataUpdate)
 * (@RequiredIfTrackUpdate) outcome. It owns a shared terminal completion state
 * and never a raw ams_mel_ir_track pointer, so it remains valid independently
 * of the public Track and Session owners. */
typedef struct ams_mel_ir_track_update_request ams_mel_ir_track_update_request;
/* Opaque owner of one asynchronous send(SystemTrackDataResponse) outcome
 * (@Optional). It is a deliberately distinct public type from the
 * TrackDataUpdate request, because the two carry different upstream semantics;
 * the TrackDataUpdate request type is never reused under a System-response
 * name. Like that request it owns a shared terminal completion state and never
 * a raw ams_mel_ir_track pointer. */
typedef struct ams_mel_ir_track_system_response_request
    ams_mel_ir_track_system_response_request;

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

/* Complete published mel::PositionSolutionState. Only values >= MaxExclusive
 * are rejected; every other value, including NotSet, is accepted as-is. */
typedef uint32_t ams_mel_position_solution_state_t;
#define AMS_MEL_POSITION_SOLUTION_NOT_SET       UINT32_C(0)
#define AMS_MEL_POSITION_SOLUTION_ALIGNING      UINT32_C(1)
#define AMS_MEL_POSITION_SOLUTION_FREE_INERTIAL UINT32_C(2)
#define AMS_MEL_POSITION_SOLUTION_GPS           UINT32_C(3)
#define AMS_MEL_POSITION_SOLUTION_BLENDED       UINT32_C(4)
#define AMS_MEL_POSITION_SOLUTION_MAX_EXCLUSIVE UINT32_C(5)

typedef struct ams_mel_north_east_down_v1 {
    double north;
    double east;
    double down;
} ams_mel_north_east_down_v1;

typedef struct ams_mel_attitude_rate_v1 {
    ams_mel_euler_v1 attitude_rate;
    int64_t attitude_rate_time_ns;
} ams_mel_attitude_rate_v1;

/* Complete published mel::PositionVelocityCovariance. All 18 terms are named
 * explicitly to avoid matrix-order mistakes; none are represented as an
 * anonymous array. */
typedef struct ams_mel_position_velocity_covariance_v1 {
    double position_position_pn_pn;
    double position_position_pn_pe;
    double position_position_pn_pd;
    double position_position_pe_pe;
    double position_position_pe_pd;
    double position_position_pd_pd;
    double position_velocity_pn_vn;
    double position_velocity_pn_ve;
    double position_velocity_pn_vd;
    double position_velocity_pe_ve;
    double position_velocity_pe_vd;
    double position_velocity_pd_vd;
    double velocity_velocity_vn_vn;
    double velocity_velocity_vn_ve;
    double velocity_velocity_vn_vd;
    double velocity_velocity_ve_ve;
    double velocity_velocity_ve_vd;
    double velocity_velocity_vd_vd;
} ams_mel_position_velocity_covariance_v1;

/* Complete published mel::NavigationReport. No unit is invented for a field
 * whose upstream declaration does not state one (wander_angle_rad and
 * magnetic_heading/altitude_msl retain upstream naming exactly). Only
 * state >= AMS_MEL_POSITION_SOLUTION_MAX_EXCLUSIVE is rejected; other
 * floating-point values, including negative, NaN, or infinite, are copied
 * as-is. */
typedef struct ams_mel_navigation_report_v1 {
    int64_t system_time_ns;
    ams_mel_position_solution_state_t state;

    double latitude_rad;
    double longitude_rad;
    double altitude_m;

    ams_mel_euler_v1 attitude;
    ams_mel_attitude_rate_v1 attitude_rate;

    ams_mel_north_east_down_v1 speed;
    ams_mel_north_east_down_v1 acceleration;

    double wander_angle_rad;
    double magnetic_heading;
    double altitude_msl;

    ams_mel_position_velocity_covariance_v1
        position_velocity_covariance_uncertainty;
} ams_mel_navigation_report_v1;

/* Terminal NavigationReport request outcome. On AMS_MEL_OK, response is valid
 * and error_code is AMS_MEL_ERROR_NONE. On AMS_MEL_COMMAND_REJECTED,
 * error_code is valid, the per-call diagnostic contains the provider
 * rejection description, and response must be ignored. */
typedef struct ams_mel_ir_navigation_result_v1 {
    ams_mel_ir_navigation_response_v1 response;
    ams_mel_error_code_t error_code;
} ams_mel_ir_navigation_result_v1;

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

/* Conditionally required Instrumentation family (@RequiredIfInstrumentation).
 * Upstream Priority has exactly Normal=0 and Debug=1 and defines no
 * MaxExclusive value; any value above Debug is rejected. */
typedef uint32_t ams_mel_ir_priority_t;
#define AMS_MEL_IR_PRIORITY_NORMAL UINT32_C(0)
#define AMS_MEL_IR_PRIORITY_DEBUG  UINT32_C(1)

/* Complete InstrumentationLevelCmd: commandID and instrumentationPriority. */
typedef struct ams_mel_ir_instrumentation_level_command_v1 {
    uint32_t command_id;
    ams_mel_ir_priority_t priority;
} ams_mel_ir_instrumentation_level_command_v1;

/* Complete InstrumentationReport: commandID, size, timestamp, and
 * instrumentationPriority. timestamp_ns preserves signed
 * std::chrono::nanoseconds; command_id and size preserve full uint32 values.
 * This one canonical record carries both RequestFor<InstrumentationReport>
 * completions and InstrumentationReport metadata callbacks. */
typedef struct ams_mel_ir_instrumentation_report_v1 {
    uint32_t command_id;
    uint32_t size;
    int64_t timestamp_ns;
    ams_mel_ir_priority_t priority;
} ams_mel_ir_instrumentation_report_v1;

/* Terminal Instrumentation request outcome. On AMS_MEL_OK, report is valid and
 * error_code is AMS_MEL_ERROR_NONE. On AMS_MEL_COMMAND_REJECTED, error_code is
 * valid, the per-call diagnostic carries the provider rejection description,
 * and report must be ignored. AMS_MEL_TIMEOUT leaves the request pending. */
typedef struct ams_mel_ir_instrumentation_result_v1 {
    ams_mel_ir_instrumentation_report_v1 report;
    ams_mel_error_code_t error_code;
} ams_mel_ir_instrumentation_result_v1;

typedef uint32_t ams_mel_ir_instrumentation_metadata_kind_t;
#define AMS_MEL_IR_INSTRUMENTATION_METADATA_REPORT UINT32_C(1)

/* Stable event representation even though the Instrumentation-specific
 * conditional surface currently defines exactly one callback datatype. */
typedef struct ams_mel_ir_instrumentation_metadata_event_v1 {
    ams_mel_ir_instrumentation_metadata_kind_t kind;
    ams_mel_ir_instrumentation_report_v1 report;
} ams_mel_ir_instrumentation_metadata_event_v1;

/* Instrumentation channel configuration; follows the Health/C2 pattern.
 * channel_type must be AMS_MEL_IR_CHANNEL_INSTRUMENTATION. String views are
 * UTF-8 byte views copied during open. No image buffer fields belong here. */
typedef struct ams_mel_ir_instrumentation_config_v1 {
    ams_mel_uci_id_v1 channel_id;
    ams_mel_ir_channel_type_t channel_type;
    ams_mel_uci_id_v1 platform_id;
    ams_mel_component_location_v1 sensor_location;
} ams_mel_ir_instrumentation_config_v1;

/* The one canonical IR XYZ representation, shared by FrameHeader sensor/nav
 * state, by the TrackDataUpdate ECEF position/velocity, and by the
 * CandidateObject sensor-relative unit vector. No second XYZ representation
 * exists in this ABI. Declared here so the Track metadata event can reuse it;
 * the layout is unchanged. */
typedef struct ams_mel_ir_directional_v1 { double x, y, z; } ams_mel_ir_directional_v1;

/* The one canonical IR quaternion, uncertainty pair, and SensorInertialState.
 * These are shared verbatim between the FrameHeader snapshot and the
 * CandidateObjectMessage inertial state; no duplicate representation exists.
 * The declarations were relocated ahead of the Track metadata event so that
 * reuse is possible. No layout changed. */
typedef struct ams_mel_ir_quaternion_v1 { double x, y, z, w; } ams_mel_ir_quaternion_v1;
typedef struct ams_mel_ir_uncertainty_v1 {
    uint32_t sensor_uncertainties;
    uint32_t platform_uncertainties;
} ams_mel_ir_uncertainty_v1;
typedef struct ams_mel_ir_sensor_inertial_state_v1 {
    int64_t system_time_ns;
    ams_mel_ir_quaternion_v1 q_xyzw;
    ams_mel_ir_quaternion_v1 q_ecef_xyzw;
    ams_mel_ir_directional_v1 sensor_position;
    ams_mel_ir_directional_v1 sensor_velocity;
    ams_mel_ir_uncertainty_v1 uncertainties;
} ams_mel_ir_sensor_inertial_state_v1;

/* Track channel configuration; follows the Health/Instrumentation pattern.
 * channel_type must be AMS_MEL_IR_CHANNEL_IRST_TRACK. String views are UTF-8
 * byte views, need not be NUL-terminated, and are copied during open. No image
 * buffer or metadata queue fields belong here. */
typedef struct ams_mel_ir_track_config_v1 {
    ams_mel_uci_id_v1 channel_id;
    ams_mel_ir_channel_type_t channel_type;
    ams_mel_uci_id_v1 platform_id;
    ams_mel_component_location_v1 sensor_location;
} ams_mel_ir_track_config_v1;

/* Upstream IrstTrackState defines exactly Idle=0, Detected=1, Coast=2, and
 * Dropped=3 and declares no MaxExclusive value. Provider values greater than
 * Dropped are malformed and are never queued. */
typedef uint32_t ams_mel_ir_track_state_t;
#define AMS_MEL_IR_TRACK_STATE_IDLE     UINT32_C(0)
#define AMS_MEL_IR_TRACK_STATE_DETECTED UINT32_C(1)
#define AMS_MEL_IR_TRACK_STATE_COAST    UINT32_C(2)
#define AMS_MEL_IR_TRACK_STATE_DROPPED  UINT32_C(3)

/* Upstream IrstTrackMode defines exactly Idle=0, Scan=1, and Stare=2 and
 * declares no MaxExclusive value. Provider values greater than Stare are
 * malformed and are never queued. */
typedef uint32_t ams_mel_ir_track_mode_t;
#define AMS_MEL_IR_TRACK_MODE_IDLE  UINT32_C(0)
#define AMS_MEL_IR_TRACK_MODE_SCAN  UINT32_C(1)
#define AMS_MEL_IR_TRACK_MODE_STARE UINT32_C(2)

/* Complete IRSTTrackReport. Every upstream getter is represented exactly once:
 * getSystemTime, getActivityId, getMeasuredNed (north/east/down),
 * getMeasuredIntensity, getMeasuredSnr, getFilteredNed (north/east/down),
 * getFilteredIntensity, getFilteredSnr, getRange, getRangeError,
 * getSpatialExtent, getTrackQuality, getClutter, getAge, getState, and getMode.
 * system_time_ns and age_ns preserve signed std::chrono::nanoseconds counts.
 * Floating-point values are copied verbatim: no clamping and no normalization.
 * The canonical ams_mel_north_east_down_v1 record is reused for both NED
 * vectors; no second C NED representation exists. */
typedef struct ams_mel_ir_track_report_v1 {
    int64_t system_time_ns;
    uint32_t activity_id;

    ams_mel_north_east_down_v1 measured_ned;
    double measured_intensity;
    double measured_snr;

    ams_mel_north_east_down_v1 filtered_ned;
    double filtered_intensity;
    double filtered_snr;

    double range_m;
    double range_error_m;
    double spatial_extent_rad;
    double track_quality;
    double clutter;

    int64_t age_ns;

    ams_mel_ir_track_state_t state;
    ams_mel_ir_track_mode_t mode;
} ams_mel_ir_track_report_v1;

/* Upstream RequestSystemTrackData (@Optional) is an inbound request the MFA
 * sends to request track information from the MFP through onMetadata: the
 * published TrackChannel declares it
 * only as a registerMetadataCallback overload and declares no
 * send(RequestSystemTrackData) overload, so it is delivered here rather than
 * through a request/wait handle. Every field is copied verbatim from the
 * published getters. systemTime is std::chrono::nanoseconds, whose
 * representation is signed, so it is carried as int64_t nanoseconds; the three
 * identifiers are uint32_t upstream and stay uint32_t here. */
typedef struct ams_mel_ir_request_system_track_data_v1 {
    int64_t system_time_ns;
    uint32_t command_id;
    uint32_t request_id;
    uint32_t track_id;
} ams_mel_ir_request_system_track_data_v1;

/* Upstream MAX_CANDIDATE_OBJECTS: the fixed storage length of the published
 * std::array<CandidateObject, 900> inside CandidateObjectMessage. The header's
 * numberOfCOs selects the MEANINGFUL PREFIX of that array; a numberOfCOs above
 * this bound is malformed and is never queued. */
#define AMS_MEL_IR_MAX_CANDIDATE_OBJECTS UINT32_C(900)

/* Upstream HotRegionTypeEnum defines exactly INVALID = 0, FLARE = 1,
 * SOLAR = 2, and MASK = 3 and declares no MaxExclusive value. A provider enum
 * representation outside 0..3 is malformed and the whole message is dropped. */
typedef uint32_t ams_mel_ir_hot_region_type_t;
#define AMS_MEL_IR_HOT_REGION_INVALID UINT32_C(0)
#define AMS_MEL_IR_HOT_REGION_FLARE   UINT32_C(1)
#define AMS_MEL_IR_HOT_REGION_SOLAR   UINT32_C(2)
#define AMS_MEL_IR_HOT_REGION_MASK    UINT32_C(3)

/* The one canonical IR row/column pair, matching upstream RowCol. RowCol is
 * deliberately NOT represented as an XYZ triple with a meaningless third
 * component: upstream publishes only getRow and getCol. */
typedef struct ams_mel_ir_row_col_v1 {
    double row;
    double column;
} ams_mel_ir_row_col_v1;

/* Complete HotRegion. Every published getter is represented exactly once:
 * getType, getSize, getTop, getLeft, getRight, and getBottom. The geometry and
 * pixel count stay uint16_t exactly as upstream declares them, and no
 * additional geometric semantics are invented. */
typedef struct ams_mel_ir_hot_region_v1 {
    ams_mel_ir_hot_region_type_t type;
    uint16_t size;
    uint16_t top;
    uint16_t left;
    uint16_t right;
    uint16_t bottom;
} ams_mel_ir_hot_region_v1;

/* Borrowed view of the event-owned HotRegion storage. The upstream vector has
 * no published fixed maximum, so the complete vector is deep-copied in its
 * published order. data stays valid until the event owner is closed. */
typedef struct ams_mel_ir_hot_region_span_v1 {
    const ams_mel_ir_hot_region_v1 *data;
    size_t size;
} ams_mel_ir_hot_region_span_v1;

/* Complete CandidateObjectHeader. Every published getter is represented
 * exactly once: getNumberOfCOs, getStackFrameIndex, getCFAR,
 * getValidityFlagBitField, getTOVutcNanoseconds, and getHotRegions (carried by
 * the message-level span).
 *
 * cfar is upstream `float` and is preserved as C binary32; it is deliberately
 * NOT widened to double. tov_utc_ns preserves the signed
 * std::chrono::nanoseconds count. validity_flag_bitfield is carried verbatim
 * and is deliberately NOT decoded. */
typedef struct ams_mel_ir_candidate_object_header_v1 {
    uint16_t number_of_cos;
    uint16_t stack_frame_index;
    float cfar;
    uint16_t validity_flag_bitfield;
    int64_t tov_utc_ns;
} ams_mel_ir_candidate_object_header_v1;

/* Complete CandidateObject. Every published getter is represented exactly
 * once: getSystemTime, getDetectionCategory, getSensorIndex, getSubpixel,
 * getIntensity, getSenRelUnit, getSignalToInterferenceRatio, and
 * getSignalToNoiseRatio.
 *
 * system_time_ns preserves the signed nanosecond count. The canonical
 * ams_mel_ir_row_col_v1 and ams_mel_ir_directional_v1 are reused; no second
 * representation exists. No floating-point value is clamped or normalized and
 * the sensor-relative unit vector is NOT renormalized, because the upstream
 * setters perform no such validation. */
typedef struct ams_mel_ir_candidate_object_v1 {
    int64_t system_time_ns;
    uint32_t detection_category;
    uint32_t sensor_index;
    ams_mel_ir_row_col_v1 subpixel;
    double intensity;
    ams_mel_ir_directional_v1 sensor_relative_unit;
    double signal_to_interference_ratio;
    double signal_to_noise_ratio;
} ams_mel_ir_candidate_object_v1;

/* Borrowed view of the event-owned CandidateObject storage. size is exactly
 * header.number_of_cos: only the meaningful prefix of the fixed 900-entry
 * upstream array is exposed, and trailing storage slots are neither read nor
 * converted. data stays valid until the event owner is closed. */
typedef struct ams_mel_ir_candidate_object_span_v1 {
    const ams_mel_ir_candidate_object_v1 *data;
    size_t size;
} ams_mel_ir_candidate_object_span_v1;

/* Complete CandidateObjectMessage. The message class itself is annotated
 * @RequiredIfBuiltInTracker, the TrackChannel callback that delivers it is
 * @RequiredIfDetectCandidateObjects, and the contained CandidateObject class is
 * @RequiredIfTrack; these are three DISTINCT upstream conditions. The callback
 * annotation is the contract that governs registration here.
 *
 * Upstream declares NO send(CandidateObjectMessage) and NO
 * RequestFor<CandidateObjectMessage>, so this is inbound callback metadata and
 * never an asynchronous request.
 *
 * The canonical ams_mel_ir_sensor_inertial_state_v1 is reused verbatim for
 * getInertialState. Both spans point into storage owned by the native event
 * owner and stay valid until event close, including after the provider channel
 * is destroyed and the provider library is unloaded. */
typedef struct ams_mel_ir_candidate_object_message_v1 {
    ams_mel_ir_candidate_object_header_v1 header;
    ams_mel_ir_sensor_inertial_state_v1 inertial_state;
    ams_mel_ir_hot_region_span_v1 hot_regions;
    ams_mel_ir_candidate_object_span_v1 candidate_objects;
} ams_mel_ir_candidate_object_message_v1;

/* Upstream CandidateObjectPreProc::candidateObjectWithBackground is exactly
 * std::array<std::array<std::int16_t, 3>, 3>: a fixed 3 by 3 patch of
 * intensities around the candidate object. Both dimension constants are
 * published here so a consumer never has to hardcode the shape. */
#define AMS_MEL_IR_CANDIDATE_BACKGROUND_SIDE UINT32_C(3)
#define AMS_MEL_IR_CANDIDATE_BACKGROUND_SAMPLES UINT32_C(9)

/* The one explicit fixed C representation of the upstream 3 by 3 patch. A
 * C++ std::array object is never memcpy'd into this storage: all nine values
 * are copied individually through the published getter.
 *
 * The mapping is ROW-MAJOR and exact:
 *
 *     samples[row * AMS_MEL_IR_CANDIDATE_BACKGROUND_SIDE + column]
 *         == upstream[row][column]      for row, column in 0 .. 2
 *
 * The element width stays upstream int16_t and is deliberately NOT widened. */
typedef struct ams_mel_ir_candidate_background_v1 {
    int16_t samples[9];
} ams_mel_ir_candidate_background_v1;

/* Complete CandidateObjectPreProc. Every published getter is represented
 * exactly once: getSystemTime, getDetectionCategory, getSensorIndex,
 * getSubpixel, getIntensity, getSenRelUnit, getSignalToInterferenceRatio,
 * getSignalToNoiseRatio, getCandidateObjectWithBackground, getClutter,
 * getCandidateObjectQuality, getSirDelta, getInertialState, getEdge,
 * getAzSigma, getElSigma, and getBackgroundNormalizer.
 *
 * system_time_ns preserves the signed std::chrono::nanoseconds count. The
 * canonical ams_mel_ir_row_col_v1, ams_mel_ir_directional_v1, and
 * ams_mel_ir_sensor_inertial_state_v1 are reused; no second representation
 * exists. Each PreProc carries its OWN nested SensorInertialState, distinct
 * from the message-level one.
 *
 * No floating-point value is clamped or normalized: candidate_object_quality
 * is documented upstream as "0 to 1" but the published setter enforces
 * nothing, the sensor-relative unit vector is NOT renormalized, and the two
 * sigma values are carried in whatever units the provider supplied.
 * detection_category is an upstream bitfield and is deliberately NOT decoded.
 *
 * edge is upstream `bool` and is normalized to exactly 0 or 1 so no
 * indeterminate byte crosses the C boundary. */
typedef struct ams_mel_ir_candidate_object_preproc_v1 {
    int64_t system_time_ns;
    uint32_t detection_category;
    uint32_t sensor_index;
    ams_mel_ir_row_col_v1 subpixel;
    double intensity;
    ams_mel_ir_directional_v1 sensor_relative_unit;
    double signal_to_interference_ratio;
    double signal_to_noise_ratio;
    ams_mel_ir_candidate_background_v1 candidate_object_with_background;
    double clutter;
    double candidate_object_quality;
    double sir_delta;
    ams_mel_ir_sensor_inertial_state_v1 inertial_state;
    uint8_t edge;
    double az_sigma;
    double el_sigma;
    double background_normalizer;
} ams_mel_ir_candidate_object_preproc_v1;

/* Borrowed view of the event-owned CandidateObjectPreProc storage. Unlike
 * CandidateObjectMessage, the upstream container here is a
 * std::vector<CandidateObjectPreProc>, which already carries its own explicit
 * size, so size is EXACTLY that vector's size and the COMPLETE vector is
 * deep-copied in its published order.
 *
 * The header's number_of_cos is deliberately NOT used to truncate this span:
 * the pinned headers publish no invariant requiring
 * numberOfCOs == candidateObjectPreProcs.size(), so both values are preserved
 * verbatim rather than an undocumented consistency rule being invented.
 *
 * data stays valid until the event owner is closed. */
typedef struct ams_mel_ir_candidate_object_preproc_span_v1 {
    const ams_mel_ir_candidate_object_preproc_v1 *data;
    size_t size;
} ams_mel_ir_candidate_object_preproc_span_v1;

/* Complete CandidateObjectPreProcMessage. Every published getter is
 * represented exactly once: getCandidateObjectHeader, getSensorInertialState,
 * and getCandidateObjectPreProcs.
 *
 * The TrackChannel callback that delivers it is annotated @Optional and is
 * documented as intended for IR MFAs that use CandidateObjectPreProc.
 * Upstream declares NO send(CandidateObjectPreProcMessage) and NO
 * RequestFor<CandidateObjectPreProcMessage>, so this is inbound callback
 * metadata and never an asynchronous request.
 *
 * ams_mel_ir_candidate_object_header_v1, ams_mel_ir_sensor_inertial_state_v1,
 * and ams_mel_ir_hot_region_span_v1 are reused verbatim. inertial_state is the
 * MESSAGE-level state; each PreProc additionally carries its own. Both spans
 * point into storage owned by the native event owner and stay valid until
 * event close, including after the provider channel is destroyed and the
 * provider library is unloaded. */
typedef struct ams_mel_ir_candidate_object_preproc_message_v1 {
    ams_mel_ir_candidate_object_header_v1 header;
    ams_mel_ir_sensor_inertial_state_v1 inertial_state;
    ams_mel_ir_hot_region_span_v1 hot_regions;
    ams_mel_ir_candidate_object_preproc_span_v1 candidate_object_preprocs;
} ams_mel_ir_candidate_object_preproc_message_v1;

/* Versioned Track metadata event format. Every published TrackChannel-specific
 * metadata callback is implemented: the @RequiredIfTrack IRSTTrackReport
 * family, the @Optional RequestSystemTrackData request, the
 * @RequiredIfDetectCandidateObjects CandidateObjectMessage, and the @Optional
 * CandidateObjectPreProcMessage. A consumer must fail closed on an
 * unrecognized kind. Only the member selected by kind is populated; the others
 * stay zeroed, and every unselected span keeps a NULL data pointer and a zero
 * size.
 *
 * kind is the ONE discriminator for every version of this event. A kind may be
 * introduced whose payload has no storage in an older version record: such an
 * event is still delivered through the older view, with the older view's
 * members zeroed, and the payload is reachable only through the version that
 * declares it. AMS_MEL_IR_TRACK_METADATA_CANDIDATE_OBJECT_MESSAGE is exactly
 * that case for v1, and AMS_MEL_IR_TRACK_METADATA_CANDIDATE_OBJECT_PREPROC_MESSAGE
 * is exactly that case for both v1 and v2. */
typedef uint32_t ams_mel_ir_track_metadata_kind_t;
#define AMS_MEL_IR_TRACK_METADATA_IRST_TRACK_REPORT UINT32_C(1)
#define AMS_MEL_IR_TRACK_METADATA_REQUEST_SYSTEM_TRACK_DATA UINT32_C(2)
#define AMS_MEL_IR_TRACK_METADATA_CANDIDATE_OBJECT_MESSAGE UINT32_C(3)
#define AMS_MEL_IR_TRACK_METADATA_CANDIDATE_OBJECT_PREPROC_MESSAGE UINT32_C(4)

/* FROZEN. This record has exactly three members and its layout is permanently
 * fixed at the layout published by task 029E. Nothing may ever be appended to
 * it again: docs/c-abi-policy.md prohibits appending fields to an existing
 * fixed-layout record, and the ABI probes assert this exact member set so an
 * accidental future append fails the build's compatibility tests.
 *
 * Historical note: task 029E did append request_system_track_data to this
 * record, which was itself inconsistent with the stated policy. That layout is
 * grandfathered as-is rather than broken a second time. Every later Track
 * metadata addition uses a new version record instead. */
typedef struct ams_mel_ir_track_metadata_event_v1 {
    ams_mel_ir_track_metadata_kind_t kind;
    ams_mel_ir_track_report_v1 track_report;
    ams_mel_ir_request_system_track_data_v1 request_system_track_data;
} ams_mel_ir_track_metadata_event_v1;

/* Track metadata event v2. The complete frozen v1 record is the first member,
 * so offsetof(v2, base) is 0, every v1 payload layout is reused rather than
 * duplicated, and base.kind remains the one discriminator. v2 adds only the
 * @RequiredIfDetectCandidateObjects CandidateObjectMessage payload, whose
 * variable-size storage stays owned by the native event owner exactly as the
 * rest of the event does.
 *
 * A v1 consumer needs no recompilation because CandidateObjectMessage exists:
 * it keeps calling ams_mel_ir_track_metadata_event_view and keeps receiving the
 * unchanged v1 record.
 *
 * FROZEN as of task 029G. v2 has exactly these two members and ends exactly
 * after candidate_object_message. The CandidateObjectPreProcMessage payload
 * was NOT appended here; it went into v3 instead, and the ABI probes assert
 * this exact member set so an accidental future append fails compatibility. */
typedef struct ams_mel_ir_track_metadata_event_v2 {
    ams_mel_ir_track_metadata_event_v1 base;
    ams_mel_ir_candidate_object_message_v1 candidate_object_message;
} ams_mel_ir_track_metadata_event_v2;

/* Track metadata event v3. The complete frozen v2 record is the first member,
 * so offsetof(v3, base) is 0 and sizeof(v3.base) == sizeof(v2). Every earlier
 * payload layout is reused rather than duplicated, and base.base.kind remains
 * the ONE discriminator for every version of this event. v3 adds only the
 * @Optional CandidateObjectPreProcMessage payload, whose variable-size storage
 * stays owned by the native event owner exactly as the rest of the event does.
 *
 * Neither a v1 nor a v2 consumer needs recompilation because
 * CandidateObjectPreProcMessage exists: each keeps calling its own view
 * operation and keeps receiving its own unchanged record. A PreProc event is
 * still delivered through those older views with kind 4, but the PreProc
 * payload is reachable only through
 * ams_mel_ir_track_metadata_event_view_v3. */
typedef struct ams_mel_ir_track_metadata_event_v3 {
    ams_mel_ir_track_metadata_event_v2 base;
    ams_mel_ir_candidate_object_preproc_message_v1
        candidate_object_preproc_message;
} ams_mel_ir_track_metadata_event_v3;

/* Upstream TrackStatus (@RequiredIfTrackUpdate) defines exactly Create = 0,
 * Update = 1, Predict = 2, and Delete = 3 and declares no MaxExclusive value.
 * Input values greater than Delete are AMS_MEL_INVALID_ARGUMENT. */
typedef uint32_t ams_mel_ir_track_status_t;
#define AMS_MEL_IR_TRACK_STATUS_CREATE  UINT32_C(0)
#define AMS_MEL_IR_TRACK_STATUS_UPDATE  UINT32_C(1)
#define AMS_MEL_IR_TRACK_STATUS_PREDICT UINT32_C(2)
#define AMS_MEL_IR_TRACK_STATUS_DELETE  UINT32_C(3)

/* Every published TrackDataUpdate covariance term, exactly 21 doubles. Each
 * upstream setter maps to exactly one field here; no value is clamped,
 * normalized, or reordered. */
typedef struct ams_mel_ir_track_covariance_v1 {
    double xx;
    double xy;
    double xz;
    double x_vx;
    double x_vy;
    double x_vz;

    double yy;
    double yz;
    double y_vx;
    double y_vy;
    double y_vz;

    double zz;
    double z_vx;
    double z_vy;
    double z_vz;

    double vx_vx;
    double vx_vy;
    double vx_vz;

    double vy_vy;
    double vy_vz;

    double vz_vz;
} ams_mel_ir_track_covariance_v1;

/* Complete TrackDataUpdate input. Every upstream setter is represented exactly
 * once: setPlatformId, setCapabilityUUID, setActivityUUID, setTrackId,
 * setEntityUUID, setTrackStatus, setTimeOfValidity, setTimeOfLastUpdate,
 * setTrackPosition, setTrackVelocity, the 21 covariance setters,
 * setManeuverProbability, and setTrackQuality.
 *
 * The two times stay in upstream epoch seconds; they are deliberately NOT
 * converted to nanoseconds. maneuver_probability, track_quality, the covariance
 * terms, and the position/velocity components are copied verbatim, because the
 * upstream setters perform no validation, clamping, or normalization.
 *
 * The canonical ams_mel_ir_directional_v1 is reused for both ECEF vectors; no
 * second XYZ representation exists. Every UCI_ID descriptive label is validated
 * as UTF-8 without an embedded NUL and copied before Submit returns, so no
 * borrowed application string outlives the submit call. */
typedef struct ams_mel_ir_track_data_update_v1 {
    uint32_t platform_id;

    ams_mel_uci_id_v1 capability_uuid;
    ams_mel_uci_id_v1 activity_uuid;

    uint32_t track_id;

    ams_mel_uci_id_v1 entity_uuid;

    ams_mel_ir_track_status_t track_status;

    double time_of_validity_seconds;
    double time_of_last_update_seconds;

    ams_mel_ir_directional_v1 track_position_ecef;
    ams_mel_ir_directional_v1 track_velocity_ecef;

    ams_mel_ir_track_covariance_v1 covariance;

    double maneuver_probability;
    double track_quality;
} ams_mel_ir_track_data_update_v1;

/* Terminal TrackDataUpdate outcome. Reuses the one generic
 * ams_mel_ir_command_status_v1 layout.
 *
 * AMS_MEL_OK: status is valid and error_code is AMS_MEL_ERROR_NONE. A
 * successful CommandStatus whose own state is AMS_MEL_IR_COMMAND_REJECTED is
 * still AMS_MEL_OK: CommandStatus::Rejected is not an ErrorOr rejection.
 * status.reason_description points into immutable request-owned cached storage
 * that stays valid across repeated Wait calls until request_close, never into
 * provider-owned memory.
 *
 * AMS_MEL_COMMAND_REJECTED: error_code is valid, status must be ignored, and
 * the diagnostic carries the provider Error description.
 *
 * AMS_MEL_TIMEOUT: the request is still pending and this record is untouched. */
typedef struct ams_mel_ir_track_update_result_v1 {
    ams_mel_ir_command_status_v1 status;
    ams_mel_error_code_t error_code;
} ams_mel_ir_track_update_result_v1;

/* Complete SystemTrackDataResponse input (@Optional). This is neither
 * @RequiredIfTrack nor @RequiredIfTrackUpdate: it is an optional upstream
 * TrackChannel operation.
 *
 * Every upstream setter is represented exactly once: setSystemTime,
 * setCommandID, setRequestId, setTrackId, setRange, setRangeRate,
 * setRangeError, setRangeRateError, setAzElValid, setInertialAzEl,
 * setAzElError, and setRangeValid.
 *
 * system_time_ns stays signed nanoseconds, matching the upstream
 * chrono::nanoseconds field. The range, range-rate, and error values are
 * copied verbatim in upstream meters and meters/second, and the azimuth and
 * elevation values are copied verbatim in radians: the upstream setters
 * perform no validation, clamping, or normalization, so neither does this
 * ABI.
 *
 * The canonical ams_mel_ir_az_el_v1 is reused for both angle pairs; no second
 * azimuth/elevation representation exists in this ABI.
 *
 * az_el_valid and range_valid use the established uint8_t representation for
 * published bool values and accept only 0 or 1. Any other value is
 * AMS_MEL_INVALID_ARGUMENT. */
typedef struct ams_mel_ir_system_track_data_response_v1 {
    int64_t system_time_ns;

    uint32_t command_id;
    uint32_t request_id;
    uint32_t track_id;

    double range_m;
    double range_rate_mps;
    double range_error_m;
    double range_rate_error_mps;

    uint8_t az_el_valid;
    uint8_t range_valid;

    ams_mel_ir_az_el_v1 inertial_az_el;
    ams_mel_ir_az_el_v1 az_el_error;
} ams_mel_ir_system_track_data_response_v1;

/* Terminal SystemTrackDataResponse outcome. It intentionally matches the
 * TrackDataUpdate result shape, because both upstream operations return
 * RequestFor<CommandStatus>, but it remains a semantically distinct public
 * type rather than an alias.
 *
 * The AMS_MEL_OK / AMS_MEL_COMMAND_REJECTED / AMS_MEL_TIMEOUT semantics are
 * exactly those of ams_mel_ir_track_update_result_v1: a successful
 * CommandStatus whose own state is AMS_MEL_IR_COMMAND_REJECTED is still
 * AMS_MEL_OK, status.reason_description points into immutable request-owned
 * cached storage, and a timeout leaves this record untouched. */
typedef struct ams_mel_ir_track_system_response_result_v1 {
    ams_mel_ir_command_status_v1 status;
    ams_mel_error_code_t error_code;
} ams_mel_ir_track_system_response_result_v1;

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
typedef struct ams_mel_ir_nav_error_v1 { double x, y, z, w; } ams_mel_ir_nav_error_v1;
typedef uint32_t ams_mel_ir_orientation_kind_t;
#define AMS_MEL_IR_ORIENTATION_EULER UINT32_C(0)
#define AMS_MEL_IR_ORIENTATION_QUATERNION UINT32_C(1)
typedef struct ams_mel_ir_orientation_v1 {
    ams_mel_ir_orientation_kind_t kind;
    ams_mel_euler_v1 euler;
    ams_mel_ir_quaternion_v1 quaternion;
} ams_mel_ir_orientation_v1;
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

/* NavigationReport input is synchronously copied before send. Submission is
 * valid only while the stream is logically Attached or Running; it does not
 * require a prior Start. Provider send() may synchronously invoke the
 * registered NavigationReportResp metadata callback before returning the
 * future, so the adapter calls send() with neither the frame callback mutex
 * nor the Image metadata mutex held. The returned request remains valid
 * independently of the public Image_Stream and Session owners; Image_Stream
 * Close with a pending request performs logical close and defers provider
 * teardown to final request completion. Provider unload is forbidden while
 * any request/future/callback may still use provider code or provider-owned
 * objects. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_stream_submit_navigation_report(
    ams_mel_ir_stream *stream,
    const ams_mel_navigation_report_v1 *report,
    ams_mel_ir_navigation_request **out_request,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Waits finitely for completion. A zero timeout polls. Timeout is not
 * cancellation and never consumes the pending request; the adapter completion
 * worker is the only future::get() caller. A terminal result is cached
 * permanently, so Wait may be repeated, including Wait(0) after terminal
 * completion, and returns the identical cached result even with a differently
 * sized diagnostic buffer. Close must not race Wait using the same request
 * handle. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_navigation_request_wait(
    const ams_mel_ir_navigation_request *request,
    uint32_t timeout_ms,
    ams_mel_ir_navigation_result_v1 *out_result,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Drops only the public request owner. It is idempotent, nonblocking, and is
 * not cancellation: a pending worker/future continues to own its completion
 * state and the Image stream state until the future itself completes. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_navigation_request_close(
    ams_mel_ir_navigation_request **request,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

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

/* Instrumentation owner. This façade implements only the Instrumentation-
 * specific conditional surface (@RequiredIfInstrumentation) plus Enable and
 * ChannelCapability. Instrumentation-specific copies of the inherited generic
 * Channel services (KeepAlive, CommsTest, ChannelCommsTest callback, and
 * registerBuffer/unregisterBuffer) are deliberately absent; those should be
 * generalized across non-C2 channel families rather than cloned per family.
 *
 * Lifecycle is Attached -> Enabled -> Failed/Closed. Open attaches the upstream
 * channel. Capabilities is valid while Attached or Enabled. Metadata
 * registration may occur while Attached. Enable explicitly calls upstream
 * Channel::enable(). Instrumentation-specific submission requires Enabled. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_instrumentation_open(
    const ams_mel_session *session,
    const ams_mel_ir_instrumentation_config_v1 *config,
    ams_mel_ir_instrumentation **out_instrumentation, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_instrumentation_enable(
    ams_mel_ir_instrumentation *instrumentation, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_instrumentation_get_capabilities(
    ams_mel_ir_instrumentation *instrumentation,
    ams_mel_ir_channel_capability **out_capability, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* The command is synchronously copied before send. Submission requires Enabled.
 * Provider send() may synchronously invoke the registered InstrumentationReport
 * metadata callback, so the adapter never holds the channel lifecycle mutex
 * across it. The returned request remains valid independently of the public
 * channel and Session owners. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_instrumentation_submit_level(
    ams_mel_ir_instrumentation *instrumentation,
    const ams_mel_ir_instrumentation_level_command_v1 *command,
    ams_mel_ir_instrumentation_request **out_request, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Waits finitely. A zero timeout polls. Timeout is not cancellation and never
 * consumes the pending request; the adapter completion worker is the only
 * future::get() caller. A terminal result is cached permanently, so Wait may be
 * repeated with a differently sized diagnostic buffer and returns identically.
 * Close must not race Wait using the same request handle. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_instrumentation_request_wait(
    const ams_mel_ir_instrumentation_request *request, uint32_t timeout_ms,
    ams_mel_ir_instrumentation_result_v1 *out_result, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Drops only the public request owner. Idempotent, nonblocking, and not
 * cancellation. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_instrumentation_request_close(
    ams_mel_ir_instrumentation_request **request, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Registers the InstrumentationReport callback. Callback state belongs to the
 * channel state, not to this public owner, and there is no upstream
 * unregister. The callback state is published before the lifecycle lock is
 * released and before provider registration, so a provider that invokes the
 * callback synchronously inside registerMetadataCallback cannot deadlock.
 * The queue is bounded FIFO with DROP-INCOMING and saturating counters. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_instrumentation_metadata_open(
    ams_mel_ir_instrumentation *instrumentation, size_t queue_capacity,
    ams_mel_ir_instrumentation_metadata **out_metadata, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_instrumentation_metadata_receive(
    ams_mel_ir_instrumentation_metadata *metadata, uint32_t timeout_ms,
    ams_mel_ir_instrumentation_metadata_event **out_event, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_instrumentation_metadata_get_counters(
    const ams_mel_ir_instrumentation_metadata *metadata,
    ams_mel_ir_metadata_counters_v1 *out_counters, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Deactivates public consumption only. It does not unregister the provider
 * callback; channel destruction remains the callback-quiescence boundary. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_instrumentation_metadata_close(
    ams_mel_ir_instrumentation_metadata **metadata, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_instrumentation_metadata_event_view(
    const ams_mel_ir_instrumentation_metadata_event *event,
    const ams_mel_ir_instrumentation_metadata_event_v1 **out_view,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_instrumentation_metadata_event_close(
    ams_mel_ir_instrumentation_metadata_event **event, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Stops new submissions and deactivates public metadata consumption. With no
 * pending requests it disables, detaches, destroys the provider channel, and
 * establishes callback quiescence synchronously. With requests pending it
 * releases the public owner and defers provider teardown to final request
 * completion. A deferred detach failure after the public owner is gone retains
 * the complete channel/provider/callback graph permanently. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_instrumentation_close(
    ams_mel_ir_instrumentation **instrumentation, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Conditionally required Track channel (@RequiredIfTrack). This release
 * implements the ownership/lifecycle foundation (Open, Enable,
 * ChannelCapability, Close) plus the @RequiredIfTrack IRSTTrackReport metadata
 * callback, the @RequiredIfTrackUpdate TrackDataUpdate send, the @Optional
 * SystemTrackDataResponse send, and the @Optional inbound
 * RequestSystemTrackData metadata callback, which shares the one bounded Track
 * metadata queue with IRSTTrackReport. CandidateObjectMessage and
 * CandidateObjectPreProcMessage are deliberately not implemented here, so the
 * Track API as a whole is not complete.
 *
 * Open attaches the upstream channel with ChannelType::IRSTTrack, requires the
 * concrete TrackChannel type, and requires that the reported ChannelCapability
 * channel types contain IRSTTrack. A rollback detach that succeeds reports
 * AMS_MEL_INITIALIZATION_FAILED; when detach ownership cannot be proven the
 * complete provider graph is retained permanently and AMS_MEL_PROVIDER_FAILED
 * is reported instead. No provider object is destructed while detach ownership
 * remains uncertain. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_track_open(
    const ams_mel_session *session, const ams_mel_ir_track_config_v1 *config,
    ams_mel_ir_track **out_track, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Attached -> Enabled. An already enabled channel returns AMS_MEL_OK. A failed
 * or closed channel returns AMS_MEL_PROVIDER_FAILED. A non-Success provider
 * enable marks the channel failed and returns AMS_MEL_PROVIDER_FAILED; a
 * throwing enable marks it failed and returns AMS_MEL_PROVIDER_EXCEPTION. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_track_enable(
    ams_mel_ir_track *track, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Capability snapshots are valid while attached or enabled. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_track_get_capabilities(
    ams_mel_ir_track *track, ams_mel_ir_channel_capability **out_capability,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Marks any Track metadata inactive and wakes its receivers immediately, then,
 * with no pending update requests, disables when enable was attempted, detaches
 * and destroys the provider channel. A failed disable does not prove ownership
 * safety, so detach is still attempted; when that detach succeeds the caller
 * owner is cleared and AMS_MEL_PROVIDER_FAILED is returned. A failed
 * synchronous detach leaves the caller owner non-null, retains the complete
 * callback/provider graph, marks the metadata failed, and permits a later close
 * retry. After a successful detach the provider channel is destroyed first,
 * then callback quiescence is awaited, and only then does the metadata
 * lifecycle become stopped.
 *
 * With pending TrackDataUpdate requests, Close clears the public Track owner,
 * returns AMS_MEL_OK, and defers physical provider teardown to final request
 * completion; the request owns the Track state graph meanwhile. A deferred
 * detach failure after the public owner is gone retains the complete graph
 * permanently and makes that request's terminal result AMS_MEL_PROVIDER_FAILED. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_track_close(
    ams_mel_ir_track **track, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Registers the @RequiredIfTrack IRSTTrackReport callback and opens bounded
 * owned polling of it. Valid while the Track channel is attached or enabled;
 * registration may occur before Enable.
 *
 * Registration is one-shot: upstream declares no unregister operation, so only
 * one attempt per Track owner is permitted. Every later call returns
 * AMS_MEL_INVALID_ARGUMENT, including after a failed attempt.
 *
 * The callback state is published and the Track lifecycle lock released before
 * the provider registration call, so a provider that invokes the callback
 * synchronously from inside registerMetadataCallback cannot deadlock and cannot
 * lose that first report.
 *
 * A non-Success provider registration returns AMS_MEL_PROVIDER_FAILED and a
 * throwing registration returns AMS_MEL_PROVIDER_EXCEPTION; in both cases no
 * metadata owner escapes while the callback-accessible state stays retained by
 * the Track channel.
 *
 * The queue is bounded FIFO with DROP-INCOMING: once full, the incoming report
 * is dropped and the already queued reports are preserved in arrival order.
 * Counters saturate rather than wrap. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_track_metadata_open(
    ams_mel_ir_track *track, size_t queue_capacity,
    ams_mel_ir_track_metadata **out_metadata, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Returns a queued event first, whatever the lifecycle. Otherwise an empty
 * active queue reports AMS_MEL_TIMEOUT, an empty inactive or stopped queue
 * reports AMS_MEL_STREAM_STOPPED, and an empty failed queue reports
 * AMS_MEL_PROVIDER_FAILED. A zero timeout is a non-blocking poll. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_track_metadata_receive(
    ams_mel_ir_track_metadata *metadata, uint32_t timeout_ms,
    ams_mel_ir_track_metadata_event **out_event, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Reuses the one shared saturating metadata counter record. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_track_metadata_get_counters(
    const ams_mel_ir_track_metadata *metadata,
    ams_mel_ir_metadata_counters_v1 *out_counters, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Idempotent and nonblocking. Close marks public consumption inactive, prevents
 * future public enqueueing, wakes receivers, and deletes the wrapper. It does
 * NOT unregister the provider callback, which has no upstream unregister: the
 * retained callback may still be invoked safely afterwards and simply queues
 * nothing. Provider channel destruction remains the quiescence boundary. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_track_metadata_close(
    ams_mel_ir_track_metadata **metadata, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Borrows immutable adapter-owned event storage valid only until event_close.
 * Values copied out of the view remain valid independently of the Track
 * channel, the Session, and the provider library. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_track_metadata_event_view(
    const ams_mel_ir_track_metadata_event *event,
    const ams_mel_ir_track_metadata_event_v1 **out_view, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Same borrowed event, viewed through the v2 record. Identical ownership,
 * validity, and diagnostic rules as the v1 view; the returned pointer is the
 * same storage, because offsetof(v2, base) is 0 and the v1 view returns
 * &view->base. Every event kind is viewable through v2. Only v2 exposes the
 * CandidateObjectMessage payload and its two event-owned spans. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_track_metadata_event_view_v2(
    const ams_mel_ir_track_metadata_event *event,
    const ams_mel_ir_track_metadata_event_v2 **out_view, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Same borrowed event, viewed through the v3 record. Identical ownership,
 * validity, and diagnostic rules as the v1 and v2 views; the returned pointer
 * is the same storage, because offsetof(v3, base) is 0 and offsetof(v2, base)
 * is 0, so all three views address one allocation. Every event kind is
 * viewable through v3. Only v3 exposes the CandidateObjectPreProcMessage
 * payload and its two event-owned spans. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_track_metadata_event_view_v3(
    const ams_mel_ir_track_metadata_event *event,
    const ams_mel_ir_track_metadata_event_v3 **out_view, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_track_metadata_event_close(
    ams_mel_ir_track_metadata_event **event, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Conditionally required TrackChannel::send(TrackDataUpdate)
 * (@RequiredIfTrackUpdate). This is a distinct upstream condition from
 * @RequiredIfTrack itself.
 *
 * The complete update, including every borrowed UCI_ID descriptive label, is
 * validated and copied before the provider send, so nothing borrowed from the
 * application outlives this call. A track_status greater than
 * AMS_MEL_IR_TRACK_STATUS_DELETE, an invalid label, or any null required
 * pointer is AMS_MEL_INVALID_ARGUMENT.
 *
 * Submission requires the Track lifecycle to be Enabled; an attached, failed,
 * or closed Track reports AMS_MEL_PROVIDER_FAILED.
 *
 * The Track lifecycle mutex is released before the provider send, because a
 * provider is permitted to invoke the registered IRSTTrackReport metadata
 * callback synchronously from inside send(). Everything needed to own the
 * returned future is allocated before the send. The returned request remains
 * valid independently of the public Track and Session owners. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_track_submit_update(
    ams_mel_ir_track *track, const ams_mel_ir_track_data_update_v1 *update,
    ams_mel_ir_track_update_request **out_request, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Waits finitely. A zero timeout polls. Timeout means only "not ready yet": it
 * is never cancellation, request consumption, or provider interruption, and it
 * leaves *out_result untouched. Exactly one adapter completion worker calls
 * future::get(). A terminal result is cached permanently, so repeated Wait
 * calls return the identical terminal result and may use a differently sized
 * diagnostic buffer. Close must not race Wait on the same request handle. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_track_update_request_wait(
    const ams_mel_ir_track_update_request *request, uint32_t timeout_ms,
    ams_mel_ir_track_update_result_v1 *out_result, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Drops only the public request owner. Idempotent, nonblocking, and not
 * cancellation: pending provider work, the future, the Track state, the
 * provider channel, and the provider library all survive. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_track_update_request_close(
    ams_mel_ir_track_update_request **request, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Optional TrackChannel::send(SystemTrackDataResponse) (@Optional). This is
 * neither @RequiredIfTrack nor @RequiredIfTrackUpdate.
 *
 * The complete response is validated and copied before the provider send. An
 * az_el_valid or range_valid value other than 0 or 1, or any null required
 * pointer, is AMS_MEL_INVALID_ARGUMENT. No numeric value is clamped or
 * normalized.
 *
 * Submission requires the Track lifecycle to be Enabled; an attached, failed,
 * or closed Track reports AMS_MEL_PROVIDER_FAILED.
 *
 * The Track lifecycle mutex is released before the provider send, because a
 * provider is permitted to invoke the registered IRSTTrackReport metadata
 * callback synchronously from inside send(). Everything needed to own the
 * returned future is allocated before the send. The returned request remains
 * valid independently of the public Track and Session owners, and it shares
 * the single Track pending-request accounting domain with TrackDataUpdate
 * requests: physical Track teardown is deferred until the total pending count
 * of both request families reaches zero. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_track_submit_system_track_data_response(
    ams_mel_ir_track *track,
    const ams_mel_ir_system_track_data_response_v1 *response,
    ams_mel_ir_track_system_response_request **out_request, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Waits finitely. A zero timeout polls. Timeout means only "not ready yet": it
 * is never cancellation, request consumption, or provider interruption, and it
 * leaves *out_result untouched. Exactly one adapter completion worker calls
 * future::get(). A terminal result is cached permanently, so repeated Wait
 * calls return the identical terminal result and may use a differently sized
 * diagnostic buffer. Close must not race Wait on the same request handle. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_track_system_response_request_wait(
    const ams_mel_ir_track_system_response_request *request, uint32_t timeout_ms,
    ams_mel_ir_track_system_response_result_v1 *out_result, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Drops only the public request owner. Idempotent, nonblocking, and not
 * cancellation: pending provider work, the future, the Track state, the
 * provider channel, and the provider library all survive. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_track_system_response_request_close(
    ams_mel_ir_track_system_response_request **request, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#endif /* AMS_MEL_ABI_H */
