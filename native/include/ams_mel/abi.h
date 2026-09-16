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

typedef uint32_t ams_mel_ir_channel_type_t;
#define AMS_MEL_IR_CHANNEL_IRST_IMAGE UINT32_C(1)
typedef uint32_t ams_mel_ir_pixel_format_t;
#define AMS_MEL_IR_PIXEL_MONO UINT32_C(0)
typedef uint32_t ams_mel_ir_image_type_t;
#define AMS_MEL_IR_IMAGE_STARING UINT32_C(0)
#define AMS_MEL_IR_IMAGE_SCANNING UINT32_C(1)
typedef uint32_t ams_mel_ir_image_flip_t;
#define AMS_MEL_IR_FLIP_NONE UINT32_C(0)
#define AMS_MEL_IR_FLIP_VERTICAL UINT32_C(1)
#define AMS_MEL_IR_FLIP_HORIZONTAL UINT32_C(2)
#define AMS_MEL_IR_FLIP_BOTH UINT32_C(3)

typedef struct ams_mel_string_view_v1 {
    const char *data;
    size_t size;
} ams_mel_string_view_v1;

typedef struct ams_mel_uci_id_v1 {
    uint8_t uuid[16];
    ams_mel_string_view_v1 descriptive_label;
} ams_mel_uci_id_v1;

typedef struct ams_mel_component_location_v1 {
    double offset_x_m;
    double offset_y_m;
    double offset_z_m;
    ams_mel_string_view_v1 key;
    ams_mel_string_view_v1 system_name;
} ams_mel_component_location_v1;

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

AMS_MEL_API ams_mel_status_t ams_mel_ir_stream_get_counters(
    const ams_mel_ir_stream *stream,
    ams_mel_ir_stream_counters_v1 *out_counters,
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

#ifdef __cplusplus
}
#endif

#endif /* AMS_MEL_ABI_H */
