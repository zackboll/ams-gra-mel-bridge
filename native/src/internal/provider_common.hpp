#pragma once

/* Provider-family-neutral façade helpers shared by the IR Session and RF
 * DataMEL entry points. Defined in provider.cpp. */

#include <ams_mel/abi.h>

#include <mel/library/CommonMEL.h>

#include <cstddef>
#include <exception>
#include <string_view>

namespace ams_mel::internal {

bool valid_utf8(std::string_view value, bool reject_nul = true) noexcept;

void write_diagnostic(std::string_view message, char *buffer,
                      std::size_t capacity, std::size_t *required) noexcept;

void write_exception_diagnostic(const std::exception& error,
                                std::string_view fallback, char *buffer,
                                std::size_t capacity,
                                std::size_t *required) noexcept;

void clear_diagnostic(char *buffer, std::size_t capacity,
                      std::size_t *required) noexcept;

/* The caller-owned string buffer pairing rule of ams_mel_provider_version_v1:
 * a NULL buffer requires zero capacity. */
bool valid_provider_version_output(
    const ams_mel_provider_version_v1 *out_version) noexcept;

/* Validates and publishes one upstream mel::VersionInfo exactly as the
 * provider-version contract requires: complete UTF-8 without NUL (otherwise
 * AMS_MEL_PROVIDER_EXCEPTION), required sizes including NUL, and
 * AMS_MEL_BUFFER_TOO_SMALL without partially publishing numeric or string
 * fields. */
ams_mel_status_t publish_provider_version(
    const ams::iface::mel::VersionInfo& value,
    ams_mel_provider_version_v1 *out_version, char *diagnostic,
    std::size_t diagnostic_capacity, std::size_t *diagnostic_required) noexcept;

/* Must be called only from inside a catch handler. Maps std::bad_alloc to
 * AMS_MEL_INTERNAL_ERROR ("allocation failed"), any other std::exception to
 * AMS_MEL_PROVIDER_EXCEPTION with its valid UTF-8 what() (else fallback), and
 * an unknown exception to AMS_MEL_PROVIDER_EXCEPTION with unknown_message. */
ams_mel_status_t translate_provider_exception(
    std::string_view fallback, std::string_view unknown_message,
    char *diagnostic, std::size_t diagnostic_capacity,
    std::size_t *diagnostic_required) noexcept;

} // namespace ams_mel::internal
