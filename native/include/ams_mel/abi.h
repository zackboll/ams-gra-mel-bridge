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

#ifdef __cplusplus
}
#endif

#endif /* AMS_MEL_ABI_H */
