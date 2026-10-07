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
#define AMS_MEL_RESOURCE_EXHAUSTED  INT32_C(13)

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
/* Zero preserves unlimited asynchronous RequestFor admission. */
typedef struct ams_mel_session_options_v1 {
    uint32_t max_async_requests;
} ams_mel_session_options_v1;
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
/* Task 032B1: a WEAK common Channel view of an existing typed family owner
 * (C2, Image stream, Health, Instrumentation, or Track). It is not a provider
 * Channel attachment, not a strong owner, and not a replacement for the typed
 * owner. See ams_mel_ir_channel_from_c2. */
typedef struct ams_mel_ir_channel ams_mel_ir_channel;
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
/* RF Admin uses the identical published Common MEL MFA_State domain. */
typedef ams_mel_ir_mfa_state_t ams_mel_rf_mfa_state_t;
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

typedef struct ams_mel_u64_span_v1 {
    const uint64_t *data;
    size_t size;
} ams_mel_u64_span_v1;

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

/* Options are copied at open; NULL options are invalid. A nonzero bound is
 * shared by all asynchronous RequestFor operations on this Session. Admission
 * refusal precedes provider send and reports AMS_MEL_RESOURCE_EXHAUSTED with
 * "async request limit reached". There is no internal queue or retry. */
AMS_MEL_API ams_mel_status_t ams_mel_session_open_with_options(
    const char *library_path,
    const char *instance,
    const char *aperture_config_id,
    const ams_mel_session_options_v1 *options,
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

/* Task 032B1 public common Channel view.
 *
 * A Channel view is a WEAK view of one existing typed family owner. Creating
 * one allocates the view only: no provider call, Session admission, family
 * lock, or lifetime change occurs, and the typed owner stays the only owner of
 * its provider Channel. The view never strongly owns typed family state, the
 * Session, the provider Channel, Control, or the provider library, so it may
 * outlive the typed owner and the Session without delaying teardown. After the
 * underlying family state is gone, every view operation returns
 * AMS_MEL_PROVIDER_FAILED with a NULL output.
 *
 * *out_channel must be NULL on entry and stays NULL on failure. Allocation
 * failure returns AMS_MEL_INTERNAL_ERROR. The source is borrowed only for the
 * duration of the call. Multiple independent views of one typed owner are
 * allowed. Operations on ONE view and ams_mel_ir_channel_close of that same
 * view must be externally serialized; distinct views need no serialization. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_channel_from_c2(
    const ams_mel_ir_c2 *source, ams_mel_ir_channel **out_channel,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_channel_from_stream(
    const ams_mel_ir_stream *source, ams_mel_ir_channel **out_channel,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_channel_from_health(
    const ams_mel_ir_health *source, ams_mel_ir_channel **out_channel,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_channel_from_instrumentation(
    const ams_mel_ir_instrumentation *source, ams_mel_ir_channel **out_channel,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_ir_channel_from_track(
    const ams_mel_ir_track *source, ams_mel_ir_channel **out_channel,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Inherited KeepAlive through the shared Return engine. Valid while the
 * family's common request lifecycle admits requests (C2/Health/
 * Instrumentation/Track: Attached or Enabled; Image: Attached or Running).
 * Uses the same per-Session admission (AMS_MEL_RESOURCE_EXHAUSTED when full)
 * and family request accounting as typed requests. Once admitted, the request
 * owns the family graph; the view does not. Use ams_mel_ir_return_request_wait
 * and ams_mel_ir_return_request_close. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_channel_send_keepalive(
    const ams_mel_ir_channel *channel, ams_mel_ir_return_request **out_request,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Inherited CommsTest through the shared Comms engine. All three uint32 IDs
 * are preserved exactly. Same lifecycle, admission, and ownership rules as
 * KeepAlive. Use ams_mel_ir_channel_comms_request_wait/close. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_channel_submit_comms_test(
    const ams_mel_ir_channel *channel,
    const ams_mel_ir_channel_comms_test_request_v1 *request,
    ams_mel_ir_channel_comms_request **out_request,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Temporarily locks the family state and applies exactly the corresponding
 * typed get_capabilities lifecycle and status mapping (C2/Health/
 * Instrumentation/Track: Attached or Enabled; Image: provider channel present
 * and not Stopping, Stopped, or Failed). The returned snapshot is independent
 * of the view, typed owner, Session, and provider library. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_channel_get_capabilities(
    const ams_mel_ir_channel *channel,
    ams_mel_ir_channel_capability **out_capability,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Idempotent and nonblocking. Destroys only the weak view and sets *channel to
 * NULL. It does not close the typed owner, cancel pending requests, or enable,
 * disable, or detach anything. */
AMS_MEL_API ams_mel_status_t ams_mel_ir_channel_close(
    ams_mel_ir_channel **channel, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* ------------------------------------------------------------------------
 * Task 033B RF DataMEL foundation.
 *
 * RF MEL is a provider family separate from the IR Session. An RF provider
 * DSO exports createDataMEL(config) -> DataMEL; there is no API_Manager,
 * Control, aperture configuration ID, or IR completion admission, and an
 * ams_mel_rf_data is never an ams_mel_session. The pinned RF MEL source is
 * open-arsenal RF MEL 762ce84c5555dd0f3ea66f36b321fecf8839b89f (Task 033A).
 *
 * The RF DataMEL surface includes live duration quantization as well as the
 * point-in-time MFA, PhysicalData and Tx power mode snapshots.
 * External/RDMA endpoints remain outside this RF slice; see docs/coverage.md
 * for the separately added ProductRx, Admin, C2, VA and Job surfaces.
 *
 * Threading: open shares no object. Version, MFA snapshot, quantization and Close on ONE
 * ams_mel_rf_data require external serialization (no internal lock is taken).
 * View and Close on ONE ams_mel_rf_mfa_info require external serialization;
 * independent snapshots may be read concurrently.
 * ------------------------------------------------------------------------ */

/* Unique public owner of one provider DataMEL and of the provider DSO that
 * created it. */
typedef struct ams_mel_rf_data ams_mel_rf_data;
/* Fully owned, immutable RFMFAInfo snapshot. After creation it references no
 * provider memory and is independent of the ams_mel_rf_data, the DataMEL, and
 * the provider DSO: it stays valid after RF Close and provider unload. */
typedef struct ams_mel_rf_mfa_info ams_mel_rf_mfa_info;

/* Raw upstream rfmel::JobDataFormat value. The 14 currently published values
 * are defined below; upstream publishes no MaxExclusive sentinel. Snapshot
 * storage keeps the raw value, so an unknown future value is preserved and
 * never dropped or rejected. */
typedef uint32_t ams_mel_rf_job_data_format_t;
#define AMS_MEL_RF_JOB_DATA_FORMAT_DIRECT_INT8 UINT32_C(0)
#define AMS_MEL_RF_JOB_DATA_FORMAT_DIRECT_INT16 UINT32_C(1)
#define AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT8 UINT32_C(2)
#define AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT16 UINT32_C(3)
#define AMS_MEL_RF_JOB_DATA_FORMAT_AMS_VITA_SMALL UINT32_C(4)
#define AMS_MEL_RF_JOB_DATA_FORMAT_AMS_VITA_MEDIUM UINT32_C(5)
#define AMS_MEL_RF_JOB_DATA_FORMAT_AMS_VITA_LARGE UINT32_C(6)
#define AMS_MEL_RF_JOB_DATA_FORMAT_AMS_VITA_EXTRA_LARGE UINT32_C(7)
#define AMS_MEL_RF_JOB_DATA_FORMAT_PDW_TYPE1 UINT32_C(8)
#define AMS_MEL_RF_JOB_DATA_FORMAT_PDW_TYPE2 UINT32_C(9)
#define AMS_MEL_RF_JOB_DATA_FORMAT_PDW_TYPE3 UINT32_C(10)
#define AMS_MEL_RF_JOB_DATA_FORMAT_LF_TYPE1 UINT32_C(11)
#define AMS_MEL_RF_JOB_DATA_FORMAT_LF_TYPE2 UINT32_C(12)
#define AMS_MEL_RF_JOB_DATA_FORMAT_LF_TYPE3 UINT32_C(13)

/* Upstream FrequencyRange copied verbatim, in Hz: min_hz is
 * getMinFrequency() and max_hz is getMaxFrequency(). Ranges are never
 * normalized, merged, sorted, clamped, or unit-converted, so min_hz > max_hz
 * is reported exactly as the provider produced it. */
typedef struct ams_mel_rf_frequency_range_v1 {
    double min_hz;
    double max_hz;
} ams_mel_rf_frequency_range_v1;

/* Spans in RF snapshots: data may be NULL only when size is 0. */
typedef struct ams_mel_rf_frequency_range_span_v1 {
    const ams_mel_rf_frequency_range_v1 *data;
    size_t size;
} ams_mel_rf_frequency_range_span_v1;

typedef struct ams_mel_rf_tx_power_mode_snapshot ams_mel_rf_tx_power_mode_snapshot;
typedef struct ams_mel_rf_tx_power_mode_v1 {
    uint32_t tx_power_mode_id;
    uint32_t is_linear_operation; /* exactly 0 or 1 */
    uint32_t tx_power_level;
    ams_mel_rf_frequency_range_span_v1 tx_frequency_ranges;
    double max_tx_duty_factor;
    int64_t max_tx_pulse_width_ns;
    double max_tx_atten;
    double tx_atten_step_size;
} ams_mel_rf_tx_power_mode_v1;
typedef struct ams_mel_rf_tx_power_mode_span_v1 {
    const ams_mel_rf_tx_power_mode_v1 *data;
    size_t size;
} ams_mel_rf_tx_power_mode_span_v1;

/* RequiredIfTransmit, independent published queries: no supportsTransmit gate.
 * Collection preserves provider order; empty is OK with {NULL,0}. Direct calls
 * the exact direct overload once and copies exactly one provider-returned mode,
 * even if its ID differs from the requested ID. All getters are read once.
 * Doubles and signed nanoseconds are copied without normalization/validation.
 * Snapshots own every mode/range; no provider reference or DataMEL child claim
 * survives creation. out_snapshot must point to NULL. Serialize Get with other
 * same-DataMEL operations/Close. bad_alloc => INTERNAL_ERROR; other exceptions
 * => PROVIDER_EXCEPTION. No partial owner escapes. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_data_get_tx_power_modes(
    const ams_mel_rf_data *data, uint32_t face_id,
    ams_mel_rf_tx_power_mode_snapshot **out_snapshot, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_data_get_tx_power_mode(
    const ams_mel_rf_data *data, uint32_t face_id, uint32_t tx_power_mode_id,
    ams_mel_rf_tx_power_mode_snapshot **out_snapshot, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* No allocation/provider calls. All spans remain valid until snapshot Close,
 * including after DataMEL Close/provider unload. Serialize View with Close on
 * the same owner. out_view must be non-NULL; its initial value is unrestricted. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_tx_power_mode_snapshot_view(
    const ams_mel_rf_tx_power_mode_snapshot *snapshot,
    ams_mel_rf_tx_power_mode_span_v1 *out_view, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Consumes/nulls caller handle; idempotent for an already-NULL owner. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_tx_power_mode_snapshot_close(
    ams_mel_rf_tx_power_mode_snapshot **snapshot, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* All input spans and strings are borrowed only during submission. Exactly
 * one provider-created RX element group is included in each Job request. */
typedef struct ams_mel_rf_rx_element_group_config_v1 {
    ams_mel_string_view_v1 label;
    double desired_duty_factor;
    ams_mel_rf_frequency_range_span_v1 expected_center_frequencies;
    ams_mel_u64_span_v1 endpoint_ids;
    ams_mel_string_view_v1 data_pipe_label;
} ams_mel_rf_rx_element_group_config_v1;
typedef struct ams_mel_rf_job_request_config_v1 {
    uint32_t request_id;
    uint32_t priority;
    uint32_t precedence_within_priority;
    uint32_t is_interruptable; /* exactly 0 or 1 */
    ams_mel_u32_span_v1 instance_selection;
    ams_mel_rf_rx_element_group_config_v1 rx_group;
} ams_mel_rf_job_request_config_v1;
/* V1 above is frozen. V2 inputs are borrowed only during submission and copied
 * before provider entry. UTC fractions must be canonical: 0 <= fs < 10^15;
 * seconds use the full signed int64 domain. No normalization or time ordering
 * check is performed. Duration/lookahead forward exact signed femtoseconds. */
typedef struct ams_mel_rf_utc_time_v1 {
    int64_t seconds;
    int64_t fractional_femtoseconds;
} ams_mel_rf_utc_time_v1;
/* Each entry causes one addEndpointIDs call, in entry order. Labels are complete
 * UTF-8 without NUL (empty allowed); endpoint sets are nonempty and duplicate
 * IDs within an entry are invalid. Repeated labels cause repeated calls. */
typedef struct ams_mel_rf_rx_data_pipe_endpoint_config_v1 {
    ams_mel_string_view_v1 data_pipe_label;
    ams_mel_u64_span_v1 endpoint_ids;
} ams_mel_rf_rx_data_pipe_endpoint_config_v1;
typedef struct ams_mel_rf_rx_data_pipe_endpoint_config_span_v1 {
    const ams_mel_rf_rx_data_pipe_endpoint_config_v1 *data;
    size_t size;
} ams_mel_rf_rx_data_pipe_endpoint_config_span_v1;
typedef struct ams_mel_rf_rx_element_group_config_v2 {
    ams_mel_string_view_v1 label;
    double desired_duty_factor;
    ams_mel_rf_frequency_range_span_v1 expected_center_frequencies;
    ams_mel_rf_rx_data_pipe_endpoint_config_span_v1 data_pipe_endpoint_configs;
} ams_mel_rf_rx_element_group_config_v2;
typedef struct ams_mel_rf_rx_element_group_config_span_v2 {
    const ams_mel_rf_rx_element_group_config_v2 *data;
    size_t size;
} ams_mel_rf_rx_element_group_config_span_v2;
typedef struct ams_mel_rf_job_request_config_v2 {
    uint32_t request_id;
    uint32_t priority;
    uint32_t precedence_within_priority;
    uint32_t is_interruptable; /* exactly 0 or 1 */
    ams_mel_u32_span_v1 instance_selection; /* ordered vector, duplicates kept */
    ams_mel_rf_rx_element_group_config_span_v2 rx_groups; /* nonempty, ordered */
    ams_mel_rf_utc_time_v1 min_start_time;
    ams_mel_rf_utc_time_v1 max_complete_time;
    int64_t duration_femtoseconds;
    ams_mel_u8_span_v1 capability_id; /* arbitrary bytes, empty allowed */
    ams_mel_u8_span_v1 activity_id; /* arbitrary bytes, empty allowed */
    ams_mel_u32_span_v1 tx_power_mode_ids; /* set: duplicates collapse */
    int64_t lookahead_femtoseconds;
} ams_mel_rf_job_request_config_v2;
/* V1/V2 are frozen. These bridge tags select the exact pinned PointingType
 * variant, not an upstream enum. No coordinate/unit conversion or physical
 * validation is performed; active doubles include signed zero/infinity/NaN.
 * Only the selected payload is active. All other payloads are ignored. */
typedef uint32_t ams_mel_rf_pointing_kind_t;
#define AMS_MEL_RF_POINTING_ECEF UINT32_C(0)
#define AMS_MEL_RF_POINTING_LLA UINT32_C(1)
#define AMS_MEL_RF_POINTING_PLATFORM_RELATIVE UINT32_C(2)
#define AMS_MEL_RF_POINTING_FACE_RELATIVE UINT32_C(3)
#define AMS_MEL_RF_POINTING_BASELINE_RELATIVE UINT32_C(4)
typedef struct ams_mel_rf_vector3_v1 {
    double x;
    double y;
    double z;
} ams_mel_rf_vector3_v1;
typedef struct ams_mel_rf_az_el_v1 {
    double azimuth_rad;
    double elevation_rad;
} ams_mel_rf_az_el_v1;
typedef struct ams_mel_rf_ecef_pointing_v1 {
    ams_mel_rf_vector3_v1 location_m;
    ams_mel_rf_vector3_v1 velocity_mps;
    ams_mel_rf_utc_time_v1 time_of_validity;
} ams_mel_rf_ecef_pointing_v1;
typedef struct ams_mel_rf_lla_pointing_v1 {
    double latitude_rad;
    double longitude_rad;
    double altitude_m;
    double velocity_north_mps;
    double velocity_east_mps;
    double velocity_down_mps;
    ams_mel_rf_utc_time_v1 time_of_validity;
} ams_mel_rf_lla_pointing_v1;
typedef struct ams_mel_rf_pointing_v1 {
    ams_mel_rf_pointing_kind_t kind;
    ams_mel_rf_ecef_pointing_v1 ecef;
    ams_mel_rf_lla_pointing_v1 lla;
    ams_mel_rf_az_el_v1 platform_relative;
    ams_mel_rf_az_el_v1 face_relative;
    double baseline_relative_conic_rad;
} ams_mel_rf_pointing_v1;
typedef struct ams_mel_rf_pointing_span_v1 {
    const ams_mel_rf_pointing_v1 *data;
    size_t size;
} ams_mel_rf_pointing_span_v1;
typedef struct ams_mel_rf_rx_element_group_config_v3 {
    ams_mel_rf_rx_element_group_config_v2 group;
    ams_mel_rf_pointing_span_v1 expected_pointing_angles;
} ams_mel_rf_rx_element_group_config_v3;
typedef struct ams_mel_rf_rx_element_group_config_span_v3 {
    const ams_mel_rf_rx_element_group_config_v3 *data;
    size_t size;
} ams_mel_rf_rx_element_group_config_span_v3;
typedef struct ams_mel_rf_job_request_config_v3 {
    uint32_t request_id;
    uint32_t priority;
    uint32_t precedence_within_priority;
    uint32_t is_interruptable; /* exactly 0 or 1 */
    ams_mel_u32_span_v1 instance_selection;
    ams_mel_rf_rx_element_group_config_span_v3 rx_groups;
    ams_mel_rf_utc_time_v1 min_start_time;
    ams_mel_rf_utc_time_v1 max_complete_time;
    int64_t duration_femtoseconds;
    ams_mel_u8_span_v1 capability_id;
    ams_mel_u8_span_v1 activity_id;
    ams_mel_u32_span_v1 tx_power_mode_ids;
    int64_t lookahead_femtoseconds;
    uint32_t has_estimated_stab_point; /* exactly 0 or 1; 0 ignores payload */
    ams_mel_rf_pointing_v1 estimated_stab_point;
} ams_mel_rf_job_request_config_v3;
/* Existing E3 mode domain, also used by the v4 command envelope. */
typedef uint32_t ams_mel_rf_element_group_mode_t;
#define AMS_MEL_RF_ELEMENT_GROUP_MODE_RX ((ams_mel_rf_element_group_mode_t)0)
#define AMS_MEL_RF_ELEMENT_GROUP_MODE_TX ((ams_mel_rf_element_group_mode_t)1)
/* TxPowerLevel is an opaque exact uint32, NOT a TxPowerModeID. Zero is valid.
 * Labels are complete UTF-8 without NUL (empty allowed); duty is finite (0,1].
 * Frequency bounds are finite and min <= max, retaining caller order. */
typedef struct ams_mel_rf_tx_element_group_config_v1 {
    ams_mel_string_view_v1 label;
    uint32_t tx_power_level;
    double desired_duty_factor;
    ams_mel_rf_frequency_range_span_v1 expected_center_frequencies;
} ams_mel_rf_tx_element_group_config_v1;
/* Only the mode-selected payload is read or validated. Inactive bytes are
 * ignored entirely. Unknown modes reject before provider command creation. */
typedef struct ams_mel_rf_job_element_group_config_v4 {
    ams_mel_rf_element_group_mode_t mode;
    ams_mel_rf_rx_element_group_config_v3 rx;
    ams_mel_rf_tx_element_group_config_v1 tx;
} ams_mel_rf_job_element_group_config_v4;
typedef struct ams_mel_rf_job_element_group_config_span_v4 {
    const ams_mel_rf_job_element_group_config_v4 *data;
    size_t size;
} ams_mel_rf_job_element_group_config_span_v4;
typedef struct ams_mel_rf_job_request_config_v4 {
    uint32_t request_id;
    uint32_t priority;
    uint32_t precedence_within_priority;
    uint32_t is_interruptable;
    ams_mel_u32_span_v1 instance_selection;
    ams_mel_rf_job_element_group_config_span_v4 element_groups;
    ams_mel_rf_utc_time_v1 min_start_time;
    ams_mel_rf_utc_time_v1 max_complete_time;
    int64_t duration_femtoseconds;
    ams_mel_u8_span_v1 capability_id;
    ams_mel_u8_span_v1 activity_id;
    ams_mel_u32_span_v1 tx_power_mode_ids;
    int64_t lookahead_femtoseconds;
    uint32_t has_estimated_stab_point;
    ams_mel_rf_pointing_v1 estimated_stab_point;
} ams_mel_rf_job_request_config_v4;
/* Pinned RF MEL 762ce84 uses numeric_limits<Femtoseconds>::max(), whose
 * count is zero, NOT the duration representation's maximum. This aliases
 * an ordinary zero relative start: callers cannot distinguish those intentions
 * in the provider scalar interface. INT64_MAX is NOT this pinned sentinel.
 * A future pin change requires explicit provider/ABI compatibility review. */
#define AMS_MEL_RF_JOB_INTERVAL_CONTINUE_FROM_PREVIOUS_FS INT64_C(0)
/* Receive-only borrowed inputs, valid only during Add. Empty spans may have
 * NULL data. Labels are UTF-8 without embedded NUL (empty allowed). Counts
 * must fit provider size_t. Signed femtoseconds and doubles are unchanged:
 * no automatic quantization or domain/timing validation is performed. */
typedef struct ams_mel_rf_receive_event_config_v1 {
    uint32_t event_id;
    ams_mel_string_view_v1 element_group_label;
    int64_t start_femtoseconds;
    int64_t duration_femtoseconds;
    double center_frequency_hz;
    double sample_frequency_hz;
    uint64_t agc_processing_iterations;
    uint64_t ignored_post_agc_iterations;
    int64_t max_extension_femtoseconds;
} ams_mel_rf_receive_event_config_v1;
typedef struct ams_mel_rf_receive_event_config_span_v1 {
    const ams_mel_rf_receive_event_config_v1 *data;
    size_t size;
} ams_mel_rf_receive_event_config_span_v1;
typedef struct ams_mel_rf_job_interval_config_v1 {
    int64_t interval_start_femtoseconds;
    uint32_t interval_id;
    int64_t interval_starting_gap_femtoseconds;
    int64_t sequence_duration_femtoseconds;
    uint64_t sequence_repeat_count;
    int64_t calibration_duration_femtoseconds;
    int64_t interval_ending_gap_femtoseconds;
    uint32_t phase_coherence_with_prior; /* exactly 0 or 1 */
    uint64_t iterations_per_signal;
    double max_data_rate_bps;
    double max_sample_rate_hz;
    uint32_t job_details_id;
    ams_mel_rf_receive_event_config_span_v1 receive_events;
} ams_mel_rf_job_interval_config_v1;
typedef struct ams_mel_rf_job_interval_config_span_v1 {
    const ams_mel_rf_job_interval_config_v1 *data;
    size_t size;
} ams_mel_rf_job_interval_config_span_v1;
/* v1 is frozen and keeps upstream Never. v2 preserves its complete layout. */
typedef uint32_t ams_mel_rf_job_interval_status_enable_t;
#define AMS_MEL_RF_INTERVAL_STATUS_NEVER UINT32_C(0)
#define AMS_MEL_RF_INTERVAL_STATUS_ALWAYS UINT32_C(1)
#define AMS_MEL_RF_INTERVAL_STATUS_ON_EXCEPTION UINT32_C(2)
typedef struct ams_mel_rf_job_interval_config_v2 {
    ams_mel_rf_job_interval_config_v1 interval;
    ams_mel_rf_job_interval_status_enable_t status_enable;
} ams_mel_rf_job_interval_config_v2;
typedef struct ams_mel_rf_job_interval_config_span_v2 {
    const ams_mel_rf_job_interval_config_v2 *data;
    size_t size;
} ams_mel_rf_job_interval_config_span_v2;
/* Spatial RX profile. All spans are borrowed only during Add; NULL/zero is
 * valid. Pointings retain the F2 active-only UTC policy and exact doubles.
 * Indices must fit provider size_t; no relationship/count validation occurs.
 * Ordered pointings and group indices preserve duplicates. v1/v2 are frozen. */
typedef struct ams_mel_rf_receive_event_config_v2 {
    ams_mel_rf_receive_event_config_v1 event;
    uint64_t stab_point_index;
    ams_mel_u64_span_v1 applicable_rx_element_groups;
} ams_mel_rf_receive_event_config_v2;
typedef struct ams_mel_rf_receive_event_config_span_v2 {
    const ams_mel_rf_receive_event_config_v2 *data;
    size_t size;
} ams_mel_rf_receive_event_config_span_v2;
typedef struct ams_mel_rf_job_interval_config_v3 {
    int64_t interval_start_femtoseconds;
    uint32_t interval_id;
    int64_t interval_starting_gap_femtoseconds;
    int64_t sequence_duration_femtoseconds;
    uint64_t sequence_repeat_count;
    int64_t calibration_duration_femtoseconds;
    int64_t interval_ending_gap_femtoseconds;
    uint32_t phase_coherence_with_prior;
    uint64_t iterations_per_signal;
    double max_data_rate_bps;
    double max_sample_rate_hz;
    uint32_t job_details_id;
    ams_mel_rf_job_interval_status_enable_t status_enable;
    ams_mel_rf_pointing_span_v1 stab_points;
    ams_mel_rf_receive_event_config_span_v2 receive_events;
} ams_mel_rf_job_interval_config_v3;
typedef struct ams_mel_rf_job_interval_config_span_v3 {
    const ams_mel_rf_job_interval_config_v3 *data;
    size_t size;
} ams_mel_rf_job_interval_config_span_v3;
/* Value-only RX controls. Doubles are forwarded without normalization or finite
 * checks. Polarization contains 0..2 ordered Stokes values. Booleans are 0/1;
 * enums must be known. Counts must fit size_t, with no relationship checks.
 * Activity ID is arbitrary binary data; TX power mode is not TxPowerLevel.
 * All spans are borrowed during synchronous Add. Older profiles remain frozen. */
typedef uint32_t ams_mel_rf_execution_type_t;
#define AMS_MEL_RF_EXECUTION_NORMAL UINT32_C(0)
#define AMS_MEL_RF_EXECUTION_CONDITIONAL UINT32_C(1)
typedef uint32_t ams_mel_rf_event_termination_type_t;
#define AMS_MEL_RF_EVENT_TERMINATION_INHIBIT UINT32_C(0)
#define AMS_MEL_RF_EVENT_TERMINATION_CANCEL UINT32_C(1)
typedef struct ams_mel_rf_stokes_vector_v1 {
    double s0;
    double s1;
    double s2;
    double s3;
} ams_mel_rf_stokes_vector_v1;
typedef struct ams_mel_rf_stokes_vector_span_v1 {
    const ams_mel_rf_stokes_vector_v1 *data;
    size_t size;
} ams_mel_rf_stokes_vector_span_v1;
typedef struct ams_mel_rf_receive_event_config_v3 {
    ams_mel_rf_receive_event_config_v2 event;
    ams_mel_rf_stokes_vector_span_v1 polarization;
    uint32_t polarization_beam_steer_correction;
    double phase_offset_rad;
    ams_mel_rf_execution_type_t execution_type;
    ams_mel_rf_event_termination_type_t termination_type;
    uint32_t allow_delay_start;
    uint64_t iteration_hold_count;
    uint64_t iteration_termination_count;
    uint32_t channelization_enabled;
} ams_mel_rf_receive_event_config_v3;
typedef struct ams_mel_rf_receive_event_config_span_v3 {
    const ams_mel_rf_receive_event_config_v3 *data;
    size_t size;
} ams_mel_rf_receive_event_config_span_v3;
typedef struct ams_mel_rf_job_interval_config_v4 {
    int64_t interval_start_femtoseconds;
    uint32_t interval_id;
    int64_t interval_starting_gap_femtoseconds;
    int64_t sequence_duration_femtoseconds;
    uint64_t sequence_repeat_count;
    int64_t calibration_duration_femtoseconds;
    int64_t interval_ending_gap_femtoseconds;
    uint32_t phase_coherence_with_prior;
    uint64_t iterations_per_signal;
    double max_data_rate_bps;
    double max_sample_rate_hz;
    uint32_t job_details_id;
    ams_mel_rf_job_interval_status_enable_t status_enable;
    ams_mel_rf_pointing_span_v1 stab_points;
    ams_mel_rf_receive_event_config_span_v3 receive_events;
    uint32_t tx_power_mode_id;
    ams_mel_u8_span_v1 activity_id;
    ams_mel_rf_execution_type_t execution_type;
} ams_mel_rf_job_interval_config_v4;
typedef struct ams_mel_rf_job_interval_config_span_v4 {
    const ams_mel_rf_job_interval_config_v4 *data;
    size_t size;
} ams_mel_rf_job_interval_config_span_v4;

typedef uint32_t ams_mel_rf_job_interval_completion_status_t;
#define AMS_MEL_RF_INTERVAL_COMPLETION_NONE UINT32_C(0)
#define AMS_MEL_RF_INTERVAL_COMPLETION_READY_FOR_NEXT_JOB_INTERVAL UINT32_C(1)
#define AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INTERRUPTED UINT32_C(2)
#define AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_TX_EVENT_SPATIAL_DATA UINT32_C(3)
#define AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_TX_EVENT_SIGNAL_DATA UINT32_C(4)
#define AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_TX_EVENT_TEMPORAL_DATA UINT32_C(5)
#define AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_TX_EVENT_IDENTIFIER_DATA UINT32_C(6)
#define AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_RX_EVENT_SPATIAL_DATA UINT32_C(7)
#define AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_RX_EVENT_SIGNAL_DATA UINT32_C(8)
#define AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_RX_EVENT_TEMPORAL_DATA UINT32_C(9)
#define AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_RX_EVENT_IDENTIFIER_DATA UINT32_C(10)
#define AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_SEQUENCE_TEMPORAL_DATA UINT32_C(11)
#define AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_INTERVAL_SPATIAL_DATA UINT32_C(12)
#define AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_INTERVAL_EVENT_SIGNAL_DATA UINT32_C(13)
#define AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_INTERVAL_EVENT_TEMPORAL_DATA UINT32_C(14)
#define AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_INTERVAL_EVENT_IDENTIFIER_DATA UINT32_C(15)
#define AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_TEMPORAL_DATA UINT32_C(16)
#define AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_IDENTIFIER_DATA UINT32_C(17)
#define AMS_MEL_RF_INTERVAL_COMPLETION_COMPLETED UINT32_C(18)
#define AMS_MEL_RF_INTERVAL_COMPLETION_CANCELLED UINT32_C(19)
#define AMS_MEL_RF_INTERVAL_COMPLETION_LATE_CONTROLS UINT32_C(20)
#define AMS_MEL_RF_INTERVAL_COMPLETION_INVALID_CONTROLS UINT32_C(21)
#define AMS_MEL_RF_INTERVAL_COMPLETION_ANTENNA_FOV_ERROR UINT32_C(22)
#define AMS_MEL_RF_INTERVAL_COMPLETION_TRANSMIT_RF_INHIBITED UINT32_C(23)
#define AMS_MEL_RF_INTERVAL_COMPLETION_STARTED UINT32_C(24)
typedef uint32_t ams_mel_rf_job_event_log_trigger_t;
#define AMS_MEL_RF_LOG_TRIGGER_NONE UINT32_C(0)
#define AMS_MEL_RF_LOG_TRIGGER_EVENT_EXTENDED UINT32_C(1)
#define AMS_MEL_RF_LOG_TRIGGER_EVENT_TRIGGERED UINT32_C(2)
#define AMS_MEL_RF_LOG_TRIGGER_EVENT_RESUMED UINT32_C(3)
#define AMS_MEL_RF_LOG_TRIGGER_EVENT_CANCELLED UINT32_C(4)
#define AMS_MEL_RF_LOG_TRIGGER_EVENT_INHIBITED UINT32_C(5)
#define AMS_MEL_RF_LOG_TRIGGER_EVENT_DELAYED_START UINT32_C(6)
#define AMS_MEL_RF_LOG_TRIGGER_EVENT_TYPE_NOT_SUPPORTED UINT32_C(7)
typedef struct ams_mel_rf_job_event_log_entry_v1 {
    uint32_t event_id;
    ams_mel_rf_job_event_log_trigger_t trigger;
    int64_t time_seconds;
    int64_t time_fractional_femtoseconds;
} ams_mel_rf_job_event_log_entry_v1;
typedef struct ams_mel_rf_job_event_log_span_v1 {
    const ams_mel_rf_job_event_log_entry_v1 *data;
    size_t size;
} ams_mel_rf_job_event_log_span_v1;
typedef struct ams_mel_rf_job_interval_status_v1 {
    uint32_t interval_id;
    ams_mel_rf_job_interval_completion_status_t completion_status;
    ams_mel_rf_job_event_log_span_v1 event_log;
    ams_mel_u8_span_v1 activity_id;
} ams_mel_rf_job_interval_status_v1;
/* Capacity > 0. Zero payload limits accept only empty respective payloads.
 * Bounds apply to queued bridge copies, not provider argument construction,
 * concurrent callback temporaries, or application-retained events. */
typedef struct ams_mel_rf_job_interval_status_options_v1 {
    size_t queue_capacity;
    size_t max_event_log_entries;
    size_t max_activity_id_bytes;
} ams_mel_rf_job_interval_status_options_v1;
typedef struct ams_mel_rf_job_interval_status_counters_v1 {
    uint64_t callback_entries;
    uint64_t events_queued;
    uint64_t events_delivered;
    uint64_t queue_full_drops;
    uint64_t malformed_drops;
    uint64_t oversize_drops;
    uint64_t allocation_failures;
    uint64_t callbacks_after_close;
} ams_mel_rf_job_interval_status_counters_v1;
typedef struct ams_mel_rf_job_interval_status ams_mel_rf_job_interval_status;
typedef struct ams_mel_rf_job_interval_status_event ams_mel_rf_job_interval_status_event;
typedef struct ams_mel_rf_job ams_mel_rf_job;
/* One registration attempt per Job, before Finalize/full Cancel. Provider
 * registration is void: OK means normal return, NOT a delivery acknowledgement.
 * Its const-reference callable, queue state and DSO pin are retained forever;
 * no JobDetail/VA/C2 claim is retained by that shell. No unregister/quiescence
 * contract exists. Stream is an observer; Close makes no provider call.
 * Job Close stops it before any deferred cleanup and does not implicitly cancel.
 * Same-handle operations require external serialization with destruction. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_job_interval_status_open(
    ams_mel_rf_job *job, const ams_mel_rf_job_interval_status_options_v1 *options,
    ams_mel_rf_job_interval_status **out_stream, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Poll at zero; TIMEOUT/STREAM_STOPPED transfer nothing. Invalid output does
 * not consume. FIFO DROP-INCOMING; success transfers one immutable owner. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_job_interval_status_receive(
    ams_mel_rf_job_interval_status *stream, uint32_t timeout_ms,
    ams_mel_rf_job_interval_status_event **out_event, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_job_interval_status_get_counters(
    const ams_mel_rf_job_interval_status *stream,
    ams_mel_rf_job_interval_status_counters_v1 *out_counters, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_job_interval_status_close(
    ams_mel_rf_job_interval_status **stream, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* View allocates/calls no provider. Spans live until Event Close, independently
 * of Job/stream/parents. Logs are event-ID key order, NOT chronological order.
 * Activity ID is arbitrary binary bytes, NOT UCI_ID/UUID/UTF-8. Both time counts
 * are exact signed int64; no timestamp recombination or normalization. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_job_interval_status_event_view(
    const ams_mel_rf_job_interval_status_event *event,
    const ams_mel_rf_job_interval_status_v1 **out_view, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_job_interval_status_event_close(
    ams_mel_rf_job_interval_status_event **event, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Always/OnException require this Job's registration to have returned normally
 * and its stream to be locally usable when checked, not proven provider delivery.
 * v1 remains Never. No incoming notification filtering by reporting mode. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_job_add_rx_intervals_v2(
    ams_mel_rf_job *job, ams_mel_rf_job_interval_config_span_v2 intervals,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Same ownership, lifecycle and locally usable status-registration gate as v2.
 * All values are prepared before the single provider Add; no hidden queries,
 * quantization, retry or TX construction. Provider semantic validation is final. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_job_add_rx_intervals_v3(
    ams_mel_rf_job *job, ams_mel_rf_job_interval_config_span_v3 intervals,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_job_add_rx_intervals_v4(
    ams_mel_rf_job *job, ams_mel_rf_job_interval_config_span_v4 intervals,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
typedef struct ams_mel_rf_job_result_v1 {
    ams_mel_error_code_t error_code;
} ams_mel_rf_job_result_v1;
/* Immutable bridge-owned snapshot; pointers remain valid until Job Close. */
typedef struct ams_mel_rf_job_info_v1 {
    int64_t actual_start_seconds;
    int64_t actual_start_femtoseconds;
    int64_t total_job_duration_femtoseconds;
    uint32_t va_instance_id;
    uint32_t va_definition_id;
    uint32_t job_details_id;
    uint32_t job_request_id;
    int64_t lookahead_femtoseconds;
    ams_mel_u32_span_v1 rx_stream_ids;
} ams_mel_rf_job_info_v1;
/* Published JobStatus is result data, including Failed* and InProgress. */
typedef uint32_t ams_mel_rf_job_status_t;
#define AMS_MEL_RF_JOB_STATUS_NONE UINT32_C(0)
#define AMS_MEL_RF_JOB_STATUS_IN_PROGRESS UINT32_C(1)
#define AMS_MEL_RF_JOB_STATUS_COMPLETE UINT32_C(2)
#define AMS_MEL_RF_JOB_STATUS_FAILED_INVALID_ID UINT32_C(3)
#define AMS_MEL_RF_JOB_STATUS_FAILED_INTERRUPTED UINT32_C(4)
#define AMS_MEL_RF_JOB_STATUS_FAILED_INVALID_STATE UINT32_C(5)
typedef uint32_t ams_mel_rf_cancel_error_t;
#define AMS_MEL_RF_CANCEL_ERROR_NONE UINT32_C(0)
typedef struct ams_mel_rf_job_cancel_result_v1 {
    uint32_t cancelled; /* exactly 0 or 1, independent of error_code */
    ams_mel_rf_cancel_error_t error_code;
} ams_mel_rf_job_cancel_result_v1;

/* One face reported by RFMFAInfo::getFaceIDs(). Booleans are 0/1. Every *_fs
 * field is the upstream ams::util::math::Femtoseconds count() (int64_t
 * femtoseconds) with no unit conversion. Frequency ranges are, in order,
 * getRxFrequencyRanges, getTxFrequencyRanges, and getSampleFrequencyRange for
 * face_id. */
typedef struct ams_mel_rf_face_info_v1 {
    uint32_t face_id;

    uint32_t supports_receive;
    uint32_t supports_transmit;
    uint32_t requires_endpoint_association;

    int64_t agc_processing_time_fs;
    int64_t min_job_request_lead_time_fs;
    int64_t max_job_request_lead_time_fs;
    int64_t min_job_detail_lead_time_fs;
    int64_t tx_rx_switching_time_fs;
    int64_t rx_tx_switching_time_fs;
    int64_t tx_tx_switching_time_fs;
    int64_t rx_rx_switching_time_fs;

    ams_mel_rf_frequency_range_span_v1 rx_frequency_ranges;
    ams_mel_rf_frequency_range_span_v1 tx_frequency_ranges;
    ams_mel_rf_frequency_range_span_v1 sample_frequency_ranges;
} ams_mel_rf_face_info_v1;

typedef struct ams_mel_rf_face_info_span_v1 {
    const ams_mel_rf_face_info_v1 *data;
    size_t size;
} ams_mel_rf_face_info_span_v1;

/* Point-in-time RFMFAInfo snapshot.
 *
 * reported_num_faces is getNumFaces() verbatim. faces is built independently
 * from getFaceIDs() in the provider's std::set order; face IDs are provider
 * values and need not be 0..N-1. Upstream publishes no requirement that the
 * two agree, so reported_num_faces and faces.size are both preserved and never
 * forced equal. contains_open_additions is 0/1. scheduler_resolution_fs is the
 * schedulerResolution() femtosecond count. max_user_defined_context_bytes is
 * getMaxNumUserDefinedContextBytes(). supported_data_formats holds the raw
 * JobDataFormat values of getSupportedDataFormats() in the provider's std::set
 * order (the upstream enum's underlying order), including unknown values. */
typedef struct ams_mel_rf_mfa_info_v1 {
    uint64_t reported_num_faces;
    uint32_t contains_open_additions;
    int64_t scheduler_resolution_fs;
    uint64_t max_user_defined_context_bytes;

    ams_mel_u32_span_v1 supported_data_formats;
    ams_mel_rf_face_info_span_v1 faces;
} ams_mel_rf_mfa_info_v1;

typedef struct ams_mel_rf_physical_data ams_mel_rf_physical_data;
typedef struct ams_mel_rf_euler_v1 {
    double roll_rad;
    double pitch_rad;
    double yaw_rad;
} ams_mel_rf_euler_v1;
typedef struct ams_mel_rf_component_location_v1 {
    double offset_x_m;
    double offset_y_m;
    double offset_z_m;
    ams_mel_string_view_v1 key;
    ams_mel_string_view_v1 system_name;
} ams_mel_rf_component_location_v1;
typedef struct ams_mel_rf_physical_data_v1 {
    double antenna_height_m;
    double antenna_width_m;
    double lattice_angle_rad;
    ams_mel_rf_component_location_v1 location;
    ams_mel_rf_euler_v1 orientation;
    ams_mel_rf_euler_v1 boresight;
} ams_mel_rf_physical_data_v1;

/* Point-in-time owned PhysicalData snapshot. All doubles are copied verbatim
 * in published meters/radians, without finite-value restrictions. Both ForeignKey
 * strings are copied completely and must be valid UTF-8 without embedded NUL;
 * malformed strings fail closed with PROVIDER_FAILED. Empty strings are valid.
 * No provider reference or DataMEL child claim survives creation. Serialize Get
 * with operations/Close on DataMEL. out_physical must be non-NULL and initially
 * NULL. Exceptions: bad_alloc => INTERNAL_ERROR, others => PROVIDER_EXCEPTION. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_data_get_physical_data(
    const ams_mel_rf_data *data, uint32_t face_id,
    ams_mel_rf_physical_data **out_physical, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* No allocation/provider call. The immutable record and string storage remain
 * valid until snapshot Close, including after DataMEL Close/provider unload.
 * out_view must be non-NULL (as for MFA View, its initial value is unrestricted).
 * Serialize View with Close on the same snapshot. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_physical_data_view(
    const ams_mel_rf_physical_data *physical,
    const ams_mel_rf_physical_data_v1 **out_view, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Consumes owner, nulls caller handle, idempotent for already-NULL owner.
 * No provider call; independent of DataMEL lifetime. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_physical_data_close(
    ams_mel_rf_physical_data **physical, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* RF Admin is a distinct createAdminMEL factory owner; no DataMEL is involved.
 * C strings use the RF Data open conventions (NUL-terminated UTF-8, no embedded
 * NUL). out_admin must be non-NULL and initially NULL. Close consumes the owner
 * even if shutdown throws. Operations on one owner must be externally serialized.
 * The provider's false commandState result is OK with *accepted=0; it is not
 * an exception. State 0..14 maps directly to Common MEL MFA_State, while
 * MAX_EXCLUSIVE and all larger values are invalid. */
typedef struct ams_mel_rf_admin ams_mel_rf_admin;
/* Distinct createC2MEL factory owner. Open takes NUL-terminated UTF-8 C
 * strings (as RF Admin); out_c2 must point to NULL. Close consumes the owner,
 * shuts down once and destroys C2 before unloading the provider. A throwing
 * shutdown retains the uncertain C2/DSO graph permanently. Calls on one owner
 * must be externally serialized. VA requests are supported below; Jobs are not. */
typedef struct ams_mel_rf_c2 ams_mel_rf_c2;
typedef struct ams_mel_rf_virtual_aperture_request ams_mel_rf_virtual_aperture_request;
typedef struct ams_mel_rf_virtual_aperture ams_mel_rf_virtual_aperture;
/* Change notification, NOT a callback-time status snapshot. The provider's
 * reference argument is ignored: no getter, address/identity inspection, or
 * application callback. Register first, query E1 on the application thread,
 * Wait, then query again. Queries do not clear pending; transitions coalesce
 * and a successful Wait need not imply the next query differs.
 * One registration attempt per public VA (a bridge profile, not an upstream
 * restriction). Exact size_t keys including 0 and SIZE_MAX remain private.
 * Each exposed attempt permanently retains the exact callable, fixed signal
 * state and DSO pin, but no VA/C2/child claim. Removal does not prove quiescence;
 * validity of the callback reference remains the provider's responsibility. */
typedef struct ams_mel_rf_va_status_subscription ams_mel_rf_va_status_subscription;
typedef struct ams_mel_rf_va_status_subscription_statistics_v1 {
    uint64_t callback_entries;       /* All invocations, including after stop. */
    uint64_t callbacks_coalesced;    /* Active invocations while already pending. */
    uint64_t notifications_delivered; /* Successful Wait consumptions. */
    uint64_t callbacks_after_stop;   /* Invocations counted and discarded after stop. */
    uint32_t pending;               /* Consistent snapshot: exactly 0 or 1. */
    uint32_t stopped;               /* Consistent snapshot: exactly 0 or 1. */
} ams_mel_rf_va_status_subscription_statistics_v1;
/* All counters saturate at UINT64_MAX; Boolean pending drives wakeup even then.
 * Open/Unsubscribe must be serialized with all same-VA calls/Close. Wait and
 * Statistics may overlap VA Close, but all subscription calls must be serialized
 * with destruction of their own wrapper. No lock-free/hard-real-time guarantee.
 * Open requires a null output and synthesizes no initial notification. Provider
 * synchronous registration invocation is observable after successful Open.
 * Stored-then-throw registration stops/retains the shell, publishes no owner,
 * consumes the attempt, and does not invent a key. bad_alloc => INTERNAL_ERROR;
 * other exceptions => PROVIDER_EXCEPTION. Mutations must never be auto-retried. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_va_status_subscription_open(
    ams_mel_rf_virtual_aperture *va, ams_mel_rf_va_status_subscription **out_subscription,
    char *diagnostic, size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* 0 polls; finite wait with no pending => TIMEOUT. Active pending is consumed
 * atomically => OK. Stop clears pending/wakes waiters and takes precedence =>
 * STREAM_STOPPED. Invalid arguments consume nothing. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_va_status_subscription_wait(
    ams_mel_rf_va_status_subscription *subscription, uint32_t timeout_ms,
    char *diagnostic, size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Consistent copy, no consumption/provider call/allocation; unchanged on error. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_va_status_subscription_get_statistics(
    const ams_mel_rf_va_status_subscription *subscription,
    ams_mel_rf_va_status_subscription_statistics_v1 *out_statistics,
    char *diagnostic, size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Matching live VA required; wrong-owner INVALID_ARGUMENT changes nothing.
 * Stop before exact-key removal, once only; outcome/diagnostic cached even on
 * exceptions. Wrapper remains open for Statistics; no Job cancel/finalize.
 * Provider removal may synchronously invoke callbacks or block. No drain wait. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_va_status_subscription_unsubscribe(
    ams_mel_rf_virtual_aperture *va, ams_mel_rf_va_status_subscription *subscription,
    char *diagnostic, size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Local stop/consume/null only; null-idempotent. NO provider removal. VA Close
 * automatically removes a known key once even after local Close, destroys VA
 * then releases its claim. Surviving observers remain stopped. VA Close reports
 * deferred C2 shutdown failure first, otherwise cached/current removal failure,
 * otherwise OK. Cleanup is not skipped; bounded diagnostics preserve both where
 * possible. C2 Close alone does not stop a still-open VA subscription. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_va_status_subscription_close(
    ams_mel_rf_va_status_subscription **subscription,
    char *diagnostic, size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
typedef struct ams_mel_rf_job_request ams_mel_rf_job_request;
/* All views and spans are borrowed only during Submit; the adapter copies
 * every string and UCI ID before calling the provider. A successful Submit
 * returns an owned request. Wait(0) polls, positive timeouts wait at most that
 * many milliseconds, and TIMEOUT does not cancel or consume the request or
 * modify the result. Terminal Wait results are cached; the diagnostic's
 * required size includes its NUL, even when the caller buffer is too small.
 * Claim uniquely transfers the C2 child to an owned VA after copying its
 * immutable snapshot; a second Claim fails. Request Close abandons an unclaimed
 * VA without blocking on its future. C2 Close consumes its owner and defers
 * shutdown until its last request/VA child is destroyed. VA Close consumes its
 * owner and reports a deferred shutdown failure; finalizers may ignore that
 * failure without unloading uncertain provider code. Access to the same owner
 * must be externally serialized. Job operations use the separate Job API. */
typedef struct ams_mel_rf_virtual_aperture_config_v1 {
    uint32_t va_definition_id;
    uint32_t priority;
    ams_mel_string_view_span_v1 local_function_info;
    ams_mel_string_view_v1 va_definition_file_info;
    ams_mel_uci_id_span_v1 capability_ids;
} ams_mel_rf_virtual_aperture_config_v1;
typedef struct ams_mel_rf_virtual_aperture_result_v1 {
    ams_mel_error_code_t error_code;
} ams_mel_rf_virtual_aperture_result_v1;
/* Immutable Claim-time borrowed view until VA Close; IDs iterate the provider set in
 * ascending order. Labels retain the provider vector's order. */
typedef struct ams_mel_rf_virtual_aperture_info_v1 {
    ams_mel_u32_span_v1 va_instance_ids;
    ams_mel_string_view_span_v1 element_group_labels;
    uint32_t is_single_group;
} ams_mel_rf_virtual_aperture_info_v1;
/* Live BaseVirtualAperture queries are distinct from the frozen Claim-time
 * info above. Same-owner calls must be externally serialized with VA Close.
 * The existing VA child claim allows querying after public C2 Close. Each
 * explicit query invokes its corresponding provider method once, without
 * caching, membership checks, or cross-call atomic consistency. */
typedef uint32_t ams_mel_rf_virtual_aperture_status_t;
#define AMS_MEL_RF_VA_STATUS_NONE UINT32_C(0)
#define AMS_MEL_RF_VA_STATUS_OPERATIONAL UINT32_C(1)
#define AMS_MEL_RF_VA_STATUS_DEGRADED UINT32_C(2)
#define AMS_MEL_RF_VA_STATUS_FAILED UINT32_C(3)
typedef struct ams_mel_rf_va_instance_list ams_mel_rf_va_instance_list;
/* @RequiredIfTransmit: four independent synchronous provider queries, not TX
 * execution or evidence of transmit support. Callers may inspect capabilities
 * separately; no capability gate, element-group lookup, Weights resource,
 * formula, reconciliation, caching or retry occurs here. WeightType is an ID.
 * Semantic upstream size_t inputs use stable uint64_t: unrepresentable values
 * fail INVALID_ARGUMENT before provider entry (never truncate). The live VA,
 * output pointer and diagnostic pairing are also validated before entry.
 * Radiated/peak results are dBW; gain/max attenuation results are dB.
 * Attenuation input is dB; frequency is Hz; u/v construct AnglePair{u,v}, the upstream
 * U,V line-of-sight/stabilization pair, without conversion or normalization.
 * Doubles including negative values, signed zero, infinity and NaN pass through;
 * outputs copy the provider double unchanged. NaN payload bits are not promised.
 * Every failure leaves the output untouched. bad_alloc => INTERNAL_ERROR;
 * other standard/unknown exceptions => PROVIDER_EXCEPTION. No result owner,
 * worker, registration or extra claim/DSO pin. Same-VA operations including Close
 * require external serialization. The existing claim permits parent-first calls
 * after public C2 Close; provider code executes outside bridge lifecycle locks. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_virtual_aperture_get_tx_radiated_power(
    const ams_mel_rf_virtual_aperture *va, uint64_t tx_element_group_id,
    uint32_t tx_power_mode_id, double tx_attenuation_db, uint64_t tx_weight_type,
    double center_frequency_hz, double u, double v, uint32_t va_instance_id,
    double *out_power_dbw, char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_virtual_aperture_get_tx_peak_radiated_power(
    const ams_mel_rf_virtual_aperture *va, uint64_t tx_element_group_id,
    uint32_t tx_power_mode_id, double tx_attenuation_db, double center_frequency_hz,
    uint32_t va_instance_id, double *out_power_dbw, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_virtual_aperture_get_tx_aperture_gain(
    const ams_mel_rf_virtual_aperture *va, uint64_t tx_element_group_id,
    uint32_t tx_power_mode_id, uint64_t tx_weight_type, double center_frequency_hz,
    double u, double v, uint32_t va_instance_id, double *out_gain_db,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_virtual_aperture_get_max_tx_attenuation(
    const ams_mel_rf_virtual_aperture *va, uint64_t tx_element_group_id,
    uint32_t tx_power_mode_id, uint32_t va_instance_id, double *out_attenuation_db,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Required live capability methods. False is successful data, not unsupported.
 * Each explicit call invokes the exact provider method once. Output is 0/1 on
 * success and untouched on failure. Same-owner serialization includes Close. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_virtual_aperture_is_cached_waveform_supported(
    const ams_mel_rf_virtual_aperture *va, uint32_t *out_supported, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_virtual_aperture_dynamic_weights_supported(
    const ams_mel_rf_virtual_aperture *va, uint32_t *out_supported, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
typedef struct ams_mel_rf_va_local_function_info_v1 {
    uint32_t local_function_type_id;
    uint64_t instance_count; /* exact widening of upstream size_t */
} ams_mel_rf_va_local_function_info_v1;
typedef struct ams_mel_rf_va_local_function_info_span_v1 {
    const ams_mel_rf_va_local_function_info_v1 *data;
    size_t size;
} ams_mel_rf_va_local_function_info_span_v1;
typedef struct ams_mel_rf_va_local_function_list ams_mel_rf_va_local_function_list;
typedef struct ams_mel_rf_va_local_function_status ams_mel_rf_va_local_function_status;
/* @RequiredIfLFSupport: independent live calls, each exact method called once.
 * No catalog/instance membership check or cross-call count reconciliation.
 * Catalog preserves ascending map keys, including zero counts. Status preserves
 * length/order/duplicates; index 0 is upstream LF instance 0. E1 reports are
 * independent. Empty results succeed with {NULL,0}. Creation output must be
 * initially NULL. Unknown status rejects the whole owner with PROVIDER_FAILED.
 * bad_alloc => INTERNAL_ERROR; other exceptions => PROVIDER_EXCEPTION.
 * Only bridge primitive storage survives creation: no new claim/worker/DSO pin.
 * Views borrow until Close, even after VA/C2 destruction and provider unload.
 * View/Close call no provider; Close consumes an owner and is null-idempotent. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_virtual_aperture_get_local_functions(
    const ams_mel_rf_virtual_aperture *va, ams_mel_rf_va_local_function_list **out_snapshot,
    char *diagnostic, size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_va_local_function_list_view(
    const ams_mel_rf_va_local_function_list *snapshot,
    ams_mel_rf_va_local_function_info_span_v1 *out_view,
    char *diagnostic, size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_va_local_function_list_close(
    ams_mel_rf_va_local_function_list **snapshot,
    char *diagnostic, size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_virtual_aperture_get_local_function_status(
    const ams_mel_rf_virtual_aperture *va, uint32_t va_instance_id, uint32_t local_function_type_id,
    ams_mel_rf_va_local_function_status **out_snapshot,
    char *diagnostic, size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Every u32 entry is validated in the existing ams_mel_rf_virtual_aperture_status_t
 * domain (0..3), not a second LF enum. View outputs are untouched on failure. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_va_local_function_status_view(
    const ams_mel_rf_va_local_function_status *snapshot, ams_mel_u32_span_v1 *out_view,
    char *diagnostic, size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_va_local_function_status_close(
    ams_mel_rf_va_local_function_status **snapshot,
    char *diagnostic, size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
typedef struct ams_mel_rf_va_instance_status_report ams_mel_rf_va_instance_status_report;
typedef struct ams_mel_rf_va_local_function_status_v1 {
    uint32_t local_function_type_id;
    /* Each element is a validated VirtualApertureStatus (0..3). Position is
     * provider LF-instance ordering, not a separately published instance ID. */
    ams_mel_u32_span_v1 statuses;
} ams_mel_rf_va_local_function_status_v1;
typedef struct ams_mel_rf_va_local_function_status_span_v1 {
    const ams_mel_rf_va_local_function_status_v1 *data;
    size_t size;
} ams_mel_rf_va_local_function_status_span_v1;
typedef struct ams_mel_rf_va_instance_status_report_v1 {
    uint32_t va_instance_id;
    ams_mel_rf_virtual_aperture_status_t status;
    ams_mel_rf_va_local_function_status_span_v1 local_functions;
} ams_mel_rf_va_instance_status_report_v1;
/* Known Degraded/Failed are OK result data; any unknown enum rejects the query
 * (the whole report, including nested values) with PROVIDER_FAILED. Scalar and
 * View outputs are untouched on failure. Creation outputs must point to NULL.
 * bad_alloc maps to INTERNAL_ERROR; other exceptions to PROVIDER_EXCEPTION. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_virtual_aperture_get_id(
    const ams_mel_rf_virtual_aperture *va, uint32_t *out_id, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_virtual_aperture_get_status(
    const ams_mel_rf_virtual_aperture *va, ams_mel_rf_virtual_aperture_status_t *out_status,
    char *diagnostic, size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_virtual_aperture_get_instance_status(
    const ams_mel_rf_virtual_aperture *va, uint32_t instance_id,
    ams_mel_rf_virtual_aperture_status_t *out_status, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Plain copied snapshots retain no VA/C2/provider/DSO. Vector order and
 * duplicates are preserved; empty lists/spans are {NULL,0}. View allocates
 * nothing and calls no provider. Borrowed views last until snapshot Close;
 * they survive VA/C2 teardown and unpinned provider DSO unload. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_virtual_aperture_get_all_instances(
    const ams_mel_rf_virtual_aperture *va, ams_mel_rf_va_instance_list **out_list,
    char *diagnostic, size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_virtual_aperture_get_instances(
    const ams_mel_rf_virtual_aperture *va, uint32_t face_id,
    ams_mel_rf_va_instance_list **out_list, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_va_instance_list_view(
    const ams_mel_rf_va_instance_list *list, ams_mel_u32_span_v1 *out_view,
    char *diagnostic, size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_va_instance_list_close(
    ams_mel_rf_va_instance_list **list, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Copy of ONE provider-returned value. Returned instance ID is authoritative,
 * even if different from the requested ID. Groups preserve ascending map-key
 * order, including empty vectors; inner status order/length is unchanged.
 * No separate LF queries, list reconciliation, or direct-status comparison. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_virtual_aperture_get_instance_status_report(
    const ams_mel_rf_virtual_aperture *va, uint32_t instance_id,
    ams_mel_rf_va_instance_status_report **out_report, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_va_instance_status_report_view(
    const ams_mel_rf_va_instance_status_report *report,
    const ams_mel_rf_va_instance_status_report_v1 **out_view, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Both snapshot Close operations consume the owner; already-NULL is OK. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_va_instance_status_report_close(
    ams_mel_rf_va_instance_status_report **report, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_c2_submit_virtual_aperture(
    ams_mel_rf_c2 *c2, const ams_mel_rf_virtual_aperture_config_v1 *config,
    ams_mel_rf_virtual_aperture_request **out_request, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_virtual_aperture_request_wait(
    const ams_mel_rf_virtual_aperture_request *request, uint32_t timeout_ms,
    ams_mel_rf_virtual_aperture_result_v1 *result, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_virtual_aperture_request_claim(
    ams_mel_rf_virtual_aperture_request *request,
    ams_mel_rf_virtual_aperture **out_va, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_virtual_aperture_request_close(
    ams_mel_rf_virtual_aperture_request **request, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_virtual_aperture_view(
    const ams_mel_rf_virtual_aperture *va,
    const ams_mel_rf_virtual_aperture_info_v1 **out_info, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_virtual_aperture_close(
    ams_mel_rf_virtual_aperture **va, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Same-owner calls (including VA Close) must be externally serialized.
 * Wait timeout is not cancellation. Request Close is nonblocking abandonment.
 * Claim is unique; Job Close does not call finalize or cancelJob. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_virtual_aperture_submit_job(
    ams_mel_rf_virtual_aperture *va, const ams_mel_rf_job_request_config_v1 *config,
    ams_mel_rf_job_request **out_request, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Same async owner/lifecycle as v1. At least one RX group is required; identical
 * labels are not deduplicated. No descriptor/DataPipe/capability lookup occurs.
 * Pointing and MFADrivenControls callback fields retain upstream defaults. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_virtual_aperture_submit_job_v2(
    ams_mel_rf_virtual_aperture *va, const ams_mel_rf_job_request_config_v2 *config,
    ams_mel_rf_job_request **out_request, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Same hardened async path/owners as v1/v2, with independent optional estimated
 * point and ordered RX group pointings (duplicates retained). Empty spans make
 * no pointing calls. Unknown kinds and noncanonical active ECEF/LLA UTC reject
 * before provider command creation; inactive payloads/times are not validated.
 * No capability auto-gate. Provider requestJob decides semantic validity.
 * With has_estimated_stab_point=0 the upstream default is never read or set.
 * MFADrivenControls/callback fields remain untouched. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_virtual_aperture_submit_job_v3(
    ams_mel_rf_virtual_aperture *va, const ams_mel_rf_job_request_config_v3 *config,
    ams_mel_rf_job_request **out_request, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* One nonempty ordered mixed RX/TX sequence, including repeated labels.
 * Same hardened submission/Wait/Claim/Close path. RX retains v3 behavior.
 * TX calls duty, power, frequencies only: no endpoints or expected pointing.
 * No capability lookup, callbacks, TX events, or TX interval execution. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_virtual_aperture_submit_job_v4(
    ams_mel_rf_virtual_aperture *va, const ams_mel_rf_job_request_config_v4 *config,
    ams_mel_rf_job_request **out_request, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_job_request_wait(
    const ams_mel_rf_job_request *request, uint32_t timeout_ms,
    ams_mel_rf_job_result_v1 *result, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_job_request_claim(
    ams_mel_rf_job_request *request, ams_mel_rf_job **out_job, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_job_request_close(
    ams_mel_rf_job_request **request, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_job_view(
    const ams_mel_rf_job *job, const ams_mel_rf_job_info_v1 **out_info,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Serialize calls on the same public owner with Close. Finalize and Cancel are
 * independently one-shot; Cancel attempted first prohibits Finalize. A pending
 * Wait never modifies out_status. Close does not cancel or wait for the worker. */
/* Synchronous mutating operations: never retry for a longer diagnostic.
 * Add/Flush reject after Finalize or full Cancel has been attempted.
 * Cancel_Remaining is repeatable before/during/after Finalize, but rejects
 * after full Cancel has been attempted. Rejection is PROVIDER_FAILED.
 * Same-owner public operations/Close must be externally serialized. Provider
 * calls run outside JobState's mutex; no new worker/owner is created.
 * bad_alloc => INTERNAL_ERROR, all other exceptions => PROVIDER_EXCEPTION.
 * Unsupported fields retain upstream defaults, including status-enable Never. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_job_add_rx_intervals(
    ams_mel_rf_job *job, ams_mel_rf_job_interval_config_span_v1 intervals,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_job_flush(
    ams_mel_rf_job *job, char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_job_cancel_remaining_intervals(
    ams_mel_rf_job *job, char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Repeatable synchronous command, admitted before/during/after Finalize, but
 * blocked after any full Cancel attempt (even False/throw). OK means only that
 * the provider's void method returned without throwing: not acceptance, changed
 * scheduling, or notification. Status registration is optional. IDs and signed
 * femtoseconds are forwarded exactly, including zero and INT64_MIN/MAX; no local
 * lookup, allowance accounting, sentinel translation or quantization occurs.
 * Provider runs outside bridge locks and may deliver status before return.
 * Never automatically retry on exception or a short diagnostic buffer. Explicit
 * application retries require provider-specific knowledge of partial mutation.
 * Same-Job operations, including Close, must be externally serialized. The
 * immutable Job snapshot remains a point-in-time snapshot. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_job_extend_event(
    ams_mel_rf_job *job, uint32_t interval_id, uint32_t event_id,
    int64_t added_duration_femtoseconds, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_job_finalize(
    ams_mel_rf_job *job, char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_job_wait_status(
    const ams_mel_rf_job *job, uint32_t timeout_ms,
    ams_mel_rf_job_status_t *out_status, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_job_cancel(
    ams_mel_rf_job *job, ams_mel_rf_job_cancel_result_v1 *out_result,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_job_close(
    ams_mel_rf_job **job, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_c2_open(
    const char *library_path, const char *configuration,
    ams_mel_rf_c2 **out_c2, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_c2_close(
    ams_mel_rf_c2 **c2, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_admin_open(
    const char *library_path, const char *configuration,
    ams_mel_rf_admin **out_admin, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_admin_command_state(
    ams_mel_rf_admin *admin, ams_mel_rf_mfa_state_t state,
    uint32_t *accepted, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_admin_close(
    ams_mel_rf_admin **admin, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Opens library_path with the platform dynamic loader (RTLD_NOW |
 * RTLD_LOCAL), resolves the pinned RF MEL createDataMEL export, and calls it
 * exactly once from private C++ with configuration. configuration is a
 * provider-specific, NUL-terminated UTF-8 string passed verbatim as
 * std::string_view (same input conventions as ams_mel_session_open).
 * library_path, configuration, and out_data must be non-NULL and *out_data
 * must be NULL.
 *
 * Status: AMS_MEL_LIBRARY_LOAD_FAILED (dlopen), AMS_MEL_SYMBOL_NOT_FOUND (no
 * createDataMEL), AMS_MEL_FACTORY_FAILED (factory returned null),
 * AMS_MEL_INTERNAL_ERROR (std::bad_alloc), AMS_MEL_PROVIDER_EXCEPTION (any
 * other factory exception). On every failure *out_data stays NULL and the
 * provider DSO is unloaded. Diagnostics follow ams_mel_session_open. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_data_open(
    const char *library_path,
    const char *configuration,
    ams_mel_rf_data **out_data,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Queries RFMEL::getVersionInfo with exactly the semantics of
 * ams_mel_session_get_provider_version: caller-owned vendor/description
 * buffers, *_required counts include the NUL, AMS_MEL_BUFFER_TOO_SMALL writes
 * only the *_required fields, provider strings with invalid UTF-8 or an
 * embedded NUL are AMS_MEL_PROVIDER_EXCEPTION, and provider exceptions are
 * contained. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_data_get_provider_version(
    const ams_mel_rf_data *data,
    ams_mel_provider_version_v1 *out_version,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Live RFMFAInfo::quantizeDuration query. Both input and output are exact signed
 * int64 femtosecond counts; no unit conversion or snapshot is involved. On any
 * failure the output is untouched. Callers must externally serialize this call
 * with DataMEL Close and other operations on the same public owner. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_data_quantize_duration(
    const ams_mel_rf_data *data,
    int64_t unquantized_femtoseconds,
    int64_t *out_quantized_femtoseconds,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Takes a point-in-time RFMFAInfo snapshot (see ams_mel_rf_mfa_info_v1).
 * Only these getters are called: getNumFaces, containsOpenAdditions,
 * schedulerResolution, getMaxNumUserDefinedContextBytes,
 * getSupportedDataFormats, getFaceIDs and, for each reported face ID only,
 * supportsReceive, supportsTransmit, requiresEndpointAssociation,
 * getAGCProcessingTime, min/maxJobRequestLeadTime, minJobDetailLeadTime, the
 * four switching times, getRx/TxFrequencyRanges, and getSampleFrequencyRange.
 *
 * Some providers (including pinned Squall) answer frequency-range getters with
 * live I/O, so two snapshots may legitimately differ. A returned snapshot is
 * immutable. If any getter throws, the partial snapshot is discarded,
 * *out_info stays NULL, and AMS_MEL_PROVIDER_EXCEPTION is returned
 * (std::bad_alloc: AMS_MEL_INTERNAL_ERROR); the RF owner stays open. A size_t
 * provider value not representable as uint64_t is AMS_MEL_PROVIDER_FAILED.
 * out_info must be non-NULL and *out_info must be NULL. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_data_get_mfa_info(
    const ams_mel_rf_data *data,
    ams_mel_rf_mfa_info **out_info,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Returns the snapshot record. No provider call is made. The record and every
 * span it references stay valid and unchanged until
 * ams_mel_rf_mfa_info_close, including after RF Close and provider unload. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_mfa_info_view(
    const ams_mel_rf_mfa_info *info,
    const ams_mel_rf_mfa_info_v1 **out_view,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Destroys the snapshot and sets *info to NULL. Idempotent for a NULL owner,
 * makes no provider call, and is independent of RF DataMEL lifetime. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_mfa_info_close(
    ams_mel_rf_mfa_info **info,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Logical Close. The owner is consumed: after Close returns, successfully or
 * not, *data is NULL. Close calls DataMEL::shutdown() exactly once, then
 * destroys the DataMEL, and only then unloads the provider DSO. Close on a
 * NULL owner returns AMS_MEL_OK and never calls shutdown again. Upstream makes
 * requests after shutdown undefined, so once Close starts no provider
 * operation is initiated through this owner; callers must externally
 * serialize every RF Data operation with Close.
 *
 * If shutdown() throws, Close returns AMS_MEL_PROVIDER_EXCEPTION with *data
 * NULL, does NOT retry shutdown, and PERMANENTLY retains the DataMEL and the
 * loaded provider DSO (allocation-free), because the provider graph has no
 * proven shutdown boundary. Process exit is the cleanup boundary. There is no
 * retryable RF Close. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_data_close(
    ams_mel_rf_data **data,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* ------------------------------------------------------------------------
 * Task 033D RF ProductRxEndpoint ComplexINT16 receive.
 *
 * DataMEL::createProductRxEndpoint(ComplexINT16, region_size_bytes, nullptr)
 * is submitted asynchronously. The claimed endpoint receives provider
 * data-ready callbacks; each accepted callback is copied immediately into an
 * immutable, fully owned event on a bounded DROP-INCOMING queue. Only
 * ComplexINT16 is supported. No RDMA, external/host buffer,
 * getRDMAMemoryRegionParams, other JobDataFormat, RF C2, or Job API is used.
 *
 * Parent-first lifetime: every create request and claimed endpoint is a
 * child of its ams_mel_rf_data. ams_mel_rf_data_close with live children
 * consumes the public owner and returns AMS_MEL_OK; DataMEL::shutdown() runs
 * exactly once, when the final child is released.
 *
 * Provider-code lifetime: pinned RF MEL has no unregister and no callback-
 * quiescence primitive, and setDataReadyCallback takes a non-const lvalue
 * reference that a provider may copy, move from, or keep a reference to.
 * Once setDataReadyCallback has been called (and returned, or threw with
 * unprovable registration state) the bridge PERMANENTLY retains the exact
 * registered std::function object, its callback state, and the provider DSO.
 * It does not retain the DataMEL, the ProductRxEndpoint, the public endpoint
 * owner, or any event. This is deliberate fail-safe retention, not a leak.
 * ------------------------------------------------------------------------ */

/* Owns one asynchronous createProductRxEndpoint completion until the created
 * endpoint is claimed or the request is closed. */
typedef struct ams_mel_rf_product_rx_request ams_mel_rf_product_rx_request;
/* Owns one claimed ProductRxEndpoint lifecycle. */
typedef struct ams_mel_rf_product_rx ams_mel_rf_product_rx;
/* Owns one immutable copied callback product. It retains no provider object,
 * provider memory, endpoint, or DataMEL, and stays readable after endpoint
 * Close, RF Data Close, DataMEL destruction, and provider unload. */
typedef struct ams_mel_rf_product_rx_event ams_mel_rf_product_rx_event;

/* data_format must be AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT16; anything else
 * is AMS_MEL_INVALID_ARGUMENT before any provider call. region_size_bytes is
 * passed to the provider verbatim (it must fit size_t); the bridge assigns no
 * meaning to any value, including 0. queue_capacity and
 * max_samples_per_event must be nonzero. */
typedef struct ams_mel_rf_product_rx_config_v1 {
    ams_mel_rf_job_data_format_t data_format;
    uint64_t region_size_bytes;
    size_t queue_capacity;
    size_t max_samples_per_event;
} ams_mel_rf_product_rx_config_v1;

/* The facade's own ComplexINT16 value. It makes NO layout-compatibility claim
 * with rfmel::MELComplex<int16_t> (not trivially copyable); every sample is
 * copied element-wise as real = source.real(), imag = source.imag(). */
typedef struct ams_mel_rf_complex_i16_v1 {
    int16_t real;
    int16_t imag;
} ams_mel_rf_complex_i16_v1;

/* data may be NULL only when size is 0. size is an ELEMENT count. */
typedef struct ams_mel_rf_complex_i16_span_v1 {
    const ams_mel_rf_complex_i16_v1 *data;
    size_t size;
} ams_mel_rf_complex_i16_span_v1;

/* The fixed representable ProductRxMetadata subset, copied verbatim.
 * phase_coherence_with_prior is exactly 0 or 1. first_rx_event_start_s is
 * UTCTime::getIntegralSeconds().count() and first_rx_event_start_fs is
 * UTCTime::getFractionalFemtoseconds().count(); neither is converted or
 * normalized. rx_stream_ids is getRxStreamIDs() in provider order.
 *
 * Fail closed: a callback whose metadata has userDefinedData, any stabPoint,
 * any receiveEvent, or a present and NON-empty receiveEventAssociations is
 * rejected whole (malformed_or_unsupported) and never truncated. Absent or
 * present-but-empty associations are accepted and carry no data. */
typedef struct ams_mel_rf_product_rx_metadata_v1 {
    uint32_t mel_protocol_version_id;
    uint32_t va_definition_id;
    uint32_t va_instance_id;
    uint32_t job_details_id;
    uint32_t job_interval_id;
    uint32_t lf_type_id;
    uint32_t lf_instance_id;
    uint32_t phase_coherence_with_prior;
    int64_t first_rx_event_start_s;
    int64_t first_rx_event_start_fs;
    ams_mel_u32_span_v1 rx_stream_ids;
} ams_mel_rf_product_rx_metadata_v1;

/* One received product. data_format is always COMPLEX_INT16 in this version
 * and samples.size is the provider callback element count (not bytes). */
typedef struct ams_mel_rf_product_rx_event_v1 {
    uint64_t endpoint_id;
    ams_mel_rf_job_data_format_t data_format;
    ams_mel_rf_complex_i16_span_v1 samples;
    ams_mel_rf_product_rx_metadata_v1 metadata;
} ams_mel_rf_product_rx_event_v1;

/* getEndpointID() and getAssignedDataFormat(), each read exactly once after
 * the create future succeeded. assigned_data_format is always COMPLEX_INT16. */
typedef struct ams_mel_rf_product_rx_info_v1 {
    uint64_t endpoint_id;
    ams_mel_rf_job_data_format_t assigned_data_format;
} ams_mel_rf_product_rx_info_v1;

/* Written only for AMS_MEL_OK (error_code AMS_MEL_ERROR_NONE) and
 * AMS_MEL_PROVIDER_FAILED (the mapped MEL ErrorCode for an ErrorOr error;
 * AMS_MEL_ERROR_NONE for any other provider failure). */
typedef struct ams_mel_rf_product_rx_request_result_v1 {
    ams_mel_error_code_t error_code;
} ams_mel_rf_product_rx_request_result_v1;

/* Saturating (UINT64_MAX) counters. Every provider callback increments
 * callbacks_received once and, unless queued, exactly one other counter. */
typedef struct ams_mel_rf_product_rx_counters_v1 {
    uint64_t callbacks_received;
    uint64_t products_queued;
    uint64_t products_dropped_queue_full;
    uint64_t malformed_or_unsupported;
    uint64_t allocation_failures;
    uint64_t callbacks_after_close;
} ams_mel_rf_product_rx_counters_v1;

/* Admits one child create operation, then calls
 * createProductRxEndpoint(ComplexINT16, region_size_bytes, nullptr) outside
 * every bridge lock; one worker thread is the only future.get() caller.
 * *out_request must be NULL. A synchronous provider exception is contained
 * (AMS_MEL_PROVIDER_EXCEPTION; std::bad_alloc: AMS_MEL_INTERNAL_ERROR) and an
 * invalid (future.valid() == false) future is AMS_MEL_PROVIDER_FAILED; then
 * no request is published. Externally serialize with the other operations on
 * the same ams_mel_rf_data. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_data_submit_product_rx(
    ams_mel_rf_data *data,
    const ams_mel_rf_product_rx_config_v1 *config,
    ams_mel_rf_product_rx_request **out_request,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Waits up to timeout_ms (0 polls; a timeout never cancels). The terminal
 * outcome is cached, and every later Wait returns the same status, result,
 * and diagnostic:
 *   AMS_MEL_OK                 created; assigned format validated
 *   AMS_MEL_PROVIDER_FAILED    ErrorOr error (diagnostic = its description),
 *                              unknown ErrorCode, null endpoint, or assigned
 *                              format != ComplexINT16
 *   AMS_MEL_PROVIDER_EXCEPTION future.get() or an endpoint getter threw
 *   AMS_MEL_INTERNAL_ERROR     bridge-owned completion failure
 *   AMS_MEL_TIMEOUT            still pending
 * out_result is untouched for TIMEOUT, PROVIDER_EXCEPTION and INTERNAL_ERROR.
 * Wait and Close on one request must not race. A request stays valid after
 * its ams_mel_rf_data is closed. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_product_rx_request_wait(
    const ams_mel_rf_product_rx_request *request,
    uint32_t timeout_ms,
    ams_mel_rf_product_rx_request_result_v1 *out_result,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Uniquely claims the created endpoint without blocking: AMS_MEL_TIMEOUT if
 * pending; the cached failure status if creation failed. The first successful
 * claim registers the data-ready callback exactly once (all callback state
 * exists first, so a callback delivered synchronously during registration is
 * queued) and transfers the provider endpoint, its RF child claim, and the
 * cached endpoint ID and format into *out_endpoint. A later claim returns
 * AMS_MEL_PROVIDER_FAILED "RF ProductRx endpoint already claimed".
 *
 * If setDataReadyCallback throws, *out_endpoint stays NULL, the registration
 * is retained permanently (its state is unprovable), the provider endpoint is
 * destroyed, and the failure is cached; registration is never retried.
 * out_endpoint and out_info must be non-NULL and *out_endpoint NULL. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_product_rx_request_claim(
    ams_mel_rf_product_rx_request *request,
    ams_mel_rf_product_rx **out_endpoint,
    ams_mel_rf_product_rx_info_v1 *out_info,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Idempotent, nonblocking, and NOT cancellation; sets *request to NULL. An
 * endpoint that was created but never claimed is destroyed by the completion
 * worker (never the caller), without callback registration, and its RF child
 * claim is released. A pending future stays owned by its worker. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_product_rx_request_close(
    ams_mel_rf_product_rx_request **request,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Moves the oldest queued event into *out_event (which must be NULL) and
 * returns AMS_MEL_OK. Empty and receiving: waits up to timeout_ms (0 polls),
 * then AMS_MEL_TIMEOUT. Closed: AMS_MEL_STREAM_STOPPED. At most one Receive
 * may run per endpoint; endpoint Close may race it and wakes it. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_product_rx_receive(
    ams_mel_rf_product_rx *endpoint,
    uint32_t timeout_ms,
    ams_mel_rf_product_rx_event **out_event,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Consistent counter snapshot; may run concurrently with provider callbacks.
 * Externally serialize with ams_mel_rf_product_rx_close. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_product_rx_get_counters(
    const ams_mel_rf_product_rx *endpoint,
    ams_mel_rf_product_rx_counters_v1 *out_counters,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Idempotent; sets *endpoint to NULL. Order: (1) logical Close: Closed,
 * queued events discarded, Receive woken; (2) wait until no bridge callback
 * body is executing (current in_flight == 0); (3) only then drop the
 * provider ProductRxEndpoint, outside every bridge lock; (4) release the RF
 * child claim. The drain precedes endpoint destruction because a callback
 * already admitted while Receiving may still be consuming callback-scoped
 * provider memory (samples, metadata) that the provider may own through the
 * ProductRxEndpoint. That drain is NOT provider callback quiescence and does
 * not relax the permanent callback-registration/DSO retention rule; a later
 * provider callback only counts callbacks_after_close and reads no payload.
 * If the released child was the last child of a closed ams_mel_rf_data, the
 * deferred DataMEL::shutdown() runs here (a throwing shutdown returns
 * AMS_MEL_PROVIDER_EXCEPTION and retains the complete DataMEL graph). */
AMS_MEL_API ams_mel_status_t ams_mel_rf_product_rx_close(
    ams_mel_rf_product_rx **endpoint,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Returns the event record without any provider call. Every pointer stays
 * valid and unchanged until ams_mel_rf_product_rx_event_close. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_product_rx_event_view(
    const ams_mel_rf_product_rx_event *event,
    const ams_mel_rf_product_rx_event_v1 **out_view,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Idempotent and provider-independent; sets *event to NULL. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_product_rx_event_close(
    ams_mel_rf_product_rx_event **event,
    char *diagnostic,
    size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Task 034E3: provider-independent, immutable element-group descriptor values.
 * Lookup keys and returned labels are distinct, validated UTF-8 without NUL.
 * Outer entries are ordered by unsigned UTF-8 lookup-key bytes (no locale or
 * normalization); nested pipes preserve std::map key traversal and endpoint
 * IDs ascend unsigned numerically. Aliased objects remain separate occurrences.
 * Empty spans are {NULL, 0}. All pointers borrow snapshot-owned storage. */
typedef struct ams_mel_rf_element_group_snapshot ams_mel_rf_element_group_snapshot;
typedef struct ams_mel_rf_element_group_snapshot_options_v1 {
    uint32_t include_data_pipes; /* exactly 0 or 1; options pointer required */
} ams_mel_rf_element_group_snapshot_options_v1;

typedef struct ams_mel_rf_data_pipe_info_v1 {
    ams_mel_string_view_v1 lookup_label;
    ams_mel_string_view_v1 label;
    ams_mel_u64_span_v1 associated_endpoint_ids;
} ams_mel_rf_data_pipe_info_v1;
typedef struct ams_mel_rf_data_pipe_info_span_v1 {
    const ams_mel_rf_data_pipe_info_v1 *data;
    size_t size;
} ams_mel_rf_data_pipe_info_span_v1;

typedef struct ams_mel_rf_element_group_descriptor_v1 {
    ams_mel_string_view_v1 lookup_label;
    ams_mel_string_view_v1 label;
    ams_mel_rf_element_group_mode_t mode;
    double max_rf_bandwidth_hz;
    double max_sample_rate_samples_per_second;
    double max_data_rate_bits_per_second;
    double max_duty_factor;
    ams_mel_rf_data_pipe_info_span_v1 data_pipes;
} ams_mel_rf_element_group_descriptor_v1;
typedef struct ams_mel_rf_element_group_descriptor_span_v1 {
    const ams_mel_rf_element_group_descriptor_v1 *data;
    size_t size;
} ams_mel_rf_element_group_descriptor_span_v1;
typedef struct ams_mel_rf_element_group_snapshot_v1 {
    uint32_t data_pipes_included;
    ams_mel_rf_element_group_descriptor_span_v1 descriptors;
} ams_mel_rf_element_group_snapshot_v1;

/* One explicit synchronous getElementGroups call, then mandatory getters once
 * per represented occurrence. No atomic multi-getter consistency is promised.
 * include_data_pipes=0 NEVER calls descriptor getDataPipes or any pipe method.
 * =1 copies all pipe keys/labels/endpoint IDs, or rejects the entire snapshot.
 * Inclusion is meaningful even when descriptors/pipes are empty; false means
 * not queried, not unsupported. No association/equality/VA-level pipe/Job calls.
 * Numeric values are forwarded without arithmetic, conversion or normalization:
 * Hz, samples/second, bits/second, dimensionless duty factor. Upstream states
 * 0 < duty factor <= 1; this descriptive API does not clamp/repair values.
 * Negative values, signed zero, infinity and NaN are preserved as values;
 * NaN payload bits are not promised. RX/TX data does not extend Job support.
 * Same-VA external serialization includes Close. No bridge mutex is held during
 * provider calls. An open VA works after public C2 Close. output must be nonnull
 * and initially NULL; invalid arguments make no provider call. Unknown mode,
 * null objects or malformed strings => PROVIDER_FAILED; bad_alloc =>
 * INTERNAL_ERROR; other exceptions => PROVIDER_EXCEPTION. No partial owner.
 * Snapshots retain no provider graph/DSO and survive actual provider unload. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_virtual_aperture_get_element_groups(
    const ams_mel_rf_virtual_aperture *va,
    const ams_mel_rf_element_group_snapshot_options_v1 *options,
    ams_mel_rf_element_group_snapshot **out_snapshot,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* No allocation/provider call. output must be nonnull and initially NULL;
 * invalid arguments leave it unchanged. View stays immutable until Close. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_element_group_snapshot_view(
    const ams_mel_rf_element_group_snapshot *snapshot,
    const ams_mel_rf_element_group_snapshot_v1 **out_view,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Consumes/nulls the owner; NULL-idempotent; no provider call. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_element_group_snapshot_close(
    ams_mel_rf_element_group_snapshot **snapshot,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

/* Task 034E5: VA-level connections, DISTINCT from optional E3 descriptor pipes.
 * Only copied strings/endpoints/records are owned; no provider pipe handle or
 * object identity escapes. Outer groups ascend lexicographic unsigned UTF-8
 * key bytes; inner pipes preserve std::map traversal; endpoints ascend uint64.
 * Keys and returned labels remain distinct. Aliases preserve every occurrence.
 * Empty spans/string views are {NULL,0}; all pointers borrow owner storage. */
typedef struct ams_mel_rf_va_data_pipe_connections_snapshot
    ams_mel_rf_va_data_pipe_connections_snapshot;
typedef struct ams_mel_rf_va_data_pipe_group_v1 {
    ams_mel_string_view_v1 element_group_lookup_label;
    ams_mel_rf_data_pipe_info_span_v1 data_pipes;
} ams_mel_rf_va_data_pipe_group_v1;
typedef struct ams_mel_rf_va_data_pipe_group_span_v1 {
    const ams_mel_rf_va_data_pipe_group_v1 *data;
    size_t size;
} ams_mel_rf_va_data_pipe_group_span_v1;
typedef struct ams_mel_rf_va_data_pipe_connections_snapshot_v1 {
    ams_mel_rf_va_data_pipe_group_span_v1 groups;
} ams_mel_rf_va_data_pipe_connections_snapshot_v1;

/* Exactly one VA::getDataPipes and each pipe getter once per occurrence.
 * Output must be initially NULL. Malformed UTF-8/NUL keys/labels or null pipes
 * reject the entire snapshot (PROVIDER_FAILED). No provider/claim/DSO retained;
 * View/Close allocate and call no provider, even after actual DSO unload. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_virtual_aperture_get_data_pipes(
    const ams_mel_rf_virtual_aperture *va,
    ams_mel_rf_va_data_pipe_connections_snapshot **out_snapshot,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_va_data_pipe_connections_snapshot_view(
    const ams_mel_rf_va_data_pipe_connections_snapshot *snapshot,
    const ams_mel_rf_va_data_pipe_connections_snapshot_v1 **out_view,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
AMS_MEL_API ams_mel_status_t ams_mel_rf_va_data_pipe_connections_snapshot_close(
    ams_mel_rf_va_data_pipe_connections_snapshot **snapshot,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Synchronous key-addressed commands: validate/copy inputs, obtain exactly one
 * fresh VA::getDataPipes value, select exact keys, invoke the exact mutation
 * once. Missing keys/null target => PROVIDER_FAILED, no mutation. No capability
 * auto-gate, cache, retry or readback. Boolean false/true is OK with accepted 0/1
 * and means ONLY the provider method returned false/true, not RDMA/Q-pairs,
 * connectivity, hardware routing or persistence across queries. Outputs remain
 * untouched on failure. bad_alloc => INTERNAL_ERROR; other exceptions =>
 * PROVIDER_EXCEPTION. Same-VA operations including Close externally serialized;
 * provider calls hold no bridge locks. Open VA remains usable after C2 Close. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_virtual_aperture_associate_data_pipe_endpoint(
    ams_mel_rf_virtual_aperture *va,
    ams_mel_string_view_v1 element_group_lookup_label,
    ams_mel_string_view_v1 data_pipe_lookup_label,
    uint64_t endpoint_id, uint32_t *out_accepted,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;
/* Input is converted to upstream std::set BEFORE the provider query: ordering
 * insignificant, duplicates collapse, empty set valid and forwarded once. */
AMS_MEL_API ams_mel_status_t ams_mel_rf_virtual_aperture_associate_data_pipe_endpoints(
    ams_mel_rf_virtual_aperture *va,
    ams_mel_string_view_v1 element_group_lookup_label,
    ams_mel_string_view_v1 data_pipe_lookup_label,
    ams_mel_u64_span_v1 endpoint_ids, uint32_t *out_accepted,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) AMS_MEL_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#endif /* AMS_MEL_ABI_H */
