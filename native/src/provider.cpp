#include <ams_mel/abi.h>
#include "internal.hpp"

#include <algorithm>
#include <cstring>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
/* This token deliberately owns no provider-dependent state. */
struct ams_mel_test_admission_token {
    std::shared_ptr<CompletionAdmission> admission;
};

extern "C" __attribute__((visibility("default"))) int ams_mel_test_admission(
    const ams_mel_session *session, unsigned operation,
    ams_mel_test_admission_token **token, std::uint32_t *active,
    std::uint32_t *maximum) noexcept
{
    try {
        if (!token) return 0;
        if (operation == 0U) {
            if (!session || *token || !session->state->admission) return 0;
            *token = new ams_mel_test_admission_token{session->state->admission};
            return 1;
        }
        if (operation == 2U) { delete *token; *token = nullptr; return 1; }
        if (operation != 1U || !*token || !active || !maximum) return 0;
        *active = (*token)->admission->active.load(std::memory_order_acquire);
        *maximum = (*token)->admission->maximum;
        return 1;
    } catch (...) { return 0; }
}
#endif

namespace {

using ManagerFactory = std::shared_ptr<API_Manager> (*)(const std::string&);
using ControlFactory = std::shared_ptr<ams::iface::irmel::Control> (*)(
    std::string_view, std::shared_ptr<API_Manager>);

bool valid_utf8(std::string_view value, bool reject_nul = true) noexcept;

void write_diagnostic(std::string_view message, char *buffer,
                      std::size_t capacity, std::size_t *required) noexcept
{
    if (required != nullptr) {
        *required = message.size() + 1U;
    }
    if (buffer != nullptr && capacity != 0U) {
        std::size_t copied = std::min(message.size(), capacity - 1U);
        while (copied != 0U &&
               !valid_utf8(message.substr(0U, copied), false)) {
            --copied;
        }
        std::memcpy(buffer, message.data(), copied);
        buffer[copied] = '\0';
    }
}

void write_exception_diagnostic(const std::exception& error,
                                std::string_view fallback, char *buffer,
                                std::size_t capacity,
                                std::size_t *required) noexcept
{
    const char *const what = error.what();
    const std::string_view message = what == nullptr
        ? std::string_view{}
        : std::string_view{what};
    write_diagnostic(!message.empty() && valid_utf8(message)
                         ? message
                         : fallback,
                     buffer, capacity, required);
}

void clear_diagnostic(char *buffer, std::size_t capacity,
                      std::size_t *required) noexcept
{
    if (required != nullptr) {
        *required = 1U;
    }
    if (buffer != nullptr && capacity != 0U) {
        buffer[0] = '\0';
    }
}

bool valid_utf8(std::string_view value, bool reject_nul) noexcept
{
    std::size_t index = 0;
    while (index < value.size()) {
        const auto lead = static_cast<unsigned char>(value[index]);
        if (lead == 0U && reject_nul) {
            return false;
        }
        std::size_t trailing = 0;
        std::uint32_t code_point = 0;
        if (lead <= 0x7fU) {
            ++index;
            continue;
        } else if (lead >= 0xc2U && lead <= 0xdfU) {
            trailing = 1;
            code_point = lead & 0x1fU;
        } else if (lead >= 0xe0U && lead <= 0xefU) {
            trailing = 2;
            code_point = lead & 0x0fU;
        } else if (lead >= 0xf0U && lead <= 0xf4U) {
            trailing = 3;
            code_point = lead & 0x07U;
        } else {
            return false;
        }
        if (index + trailing >= value.size()) {
            return false;
        }
        for (std::size_t offset = 1; offset <= trailing; ++offset) {
            const auto byte = static_cast<unsigned char>(value[index + offset]);
            if ((byte & 0xc0U) != 0x80U) {
                return false;
            }
            code_point = (code_point << 6U) | (byte & 0x3fU);
        }
        if ((trailing == 2U && code_point < 0x800U) ||
            (trailing == 3U && code_point < 0x10000U) ||
            code_point > 0x10ffffU ||
            (code_point >= 0xd800U && code_point <= 0xdfffU)) {
            return false;
        }
        index += trailing + 1U;
    }
    return true;
}

} // namespace

static ams_mel_status_t open_session_impl(
    const char *library_path, const char *instance,
    const char *aperture_config_id, std::uint32_t maximum,
    ams_mel_session **out_session,
    char *diagnostic, std::size_t diagnostic_capacity,
    std::size_t *diagnostic_required) noexcept
{
    clear_diagnostic(diagnostic, diagnostic_capacity, diagnostic_required);
    if (library_path == nullptr || instance == nullptr ||
        aperture_config_id == nullptr || out_session == nullptr ||
        *out_session != nullptr ||
        (diagnostic == nullptr && diagnostic_capacity != 0U)) {
        write_diagnostic("invalid argument", diagnostic, diagnostic_capacity,
                         diagnostic_required);
        return AMS_MEL_INVALID_ARGUMENT;
    }

    std::unique_ptr<SharedLibrary> library;
    try {
        library = std::make_unique<SharedLibrary>(library_path);
    } catch (const std::bad_alloc&) {
        write_diagnostic("allocation failed", diagnostic, diagnostic_capacity,
                         diagnostic_required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (const std::exception& error) {
        write_exception_diagnostic(error, "library load failed", diagnostic,
                                   diagnostic_capacity, diagnostic_required);
        return AMS_MEL_LIBRARY_LOAD_FAILED;
    } catch (...) {
        write_diagnostic("unknown library-load exception", diagnostic,
                         diagnostic_capacity, diagnostic_required);
        return AMS_MEL_LIBRARY_LOAD_FAILED;
    }

    ManagerFactory manager_factory{};
    ControlFactory control_factory{};
    try {
        manager_factory = library->symbol<ManagerFactory>("getAPI_Manager");
        control_factory = library->symbol<ControlFactory>("getControl");
    } catch (const std::bad_alloc&) {
        write_diagnostic("allocation failed", diagnostic, diagnostic_capacity,
                         diagnostic_required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (const std::exception& error) {
        write_exception_diagnostic(error, "symbol resolution failed", diagnostic,
                                   diagnostic_capacity, diagnostic_required);
        return AMS_MEL_SYMBOL_NOT_FOUND;
    } catch (...) {
        write_diagnostic("unknown symbol-resolution exception", diagnostic,
                         diagnostic_capacity, diagnostic_required);
        return AMS_MEL_SYMBOL_NOT_FOUND;
    }

    try {
        std::shared_ptr<API_Manager> manager = manager_factory(instance);
        if (!manager) {
            write_diagnostic("getAPI_Manager returned null", diagnostic,
                             diagnostic_capacity, diagnostic_required);
            return AMS_MEL_FACTORY_FAILED;
        }
        std::shared_ptr<ams::iface::irmel::Control> control =
            control_factory(instance, manager);
        if (!control) {
            write_diagnostic("getControl returned null", diagnostic,
                             diagnostic_capacity, diagnostic_required);
            return AMS_MEL_FACTORY_FAILED;
        }
        if (control->init(aperture_config_id) !=
            ams::iface::irmel::Return::Success) {
            write_diagnostic("Control::init failed", diagnostic,
                             diagnostic_capacity, diagnostic_required);
            return AMS_MEL_INITIALIZATION_FAILED;
        }

        auto state = std::make_shared<SessionState>();
        if (maximum != 0U)
            state->admission = std::make_shared<CompletionAdmission>(maximum);
        state->library = std::move(library);
        state->manager = std::move(manager);
        state->control = std::move(control);
        state->instance = instance;
        auto session = std::make_unique<ams_mel_session>();
        session->state = std::move(state);
        *out_session = session.release();
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        write_diagnostic("allocation failed", diagnostic, diagnostic_capacity,
                         diagnostic_required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (const std::exception& error) {
        write_exception_diagnostic(error, "provider exception", diagnostic,
                                   diagnostic_capacity, diagnostic_required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    } catch (...) {
        write_diagnostic("unknown provider exception", diagnostic,
                         diagnostic_capacity, diagnostic_required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}

extern "C" ams_mel_status_t ams_mel_session_open(
    const char *library_path, const char *instance, const char *aperture_config_id,
    ams_mel_session **out_session, char *diagnostic, std::size_t capacity,
    std::size_t *required) noexcept
{
    return open_session_impl(library_path, instance, aperture_config_id, 0U,
                             out_session, diagnostic, capacity, required);
}

extern "C" ams_mel_status_t ams_mel_session_open_with_options(
    const char *library_path, const char *instance, const char *aperture_config_id,
    const ams_mel_session_options_v1 *options, ams_mel_session **out_session,
    char *diagnostic, std::size_t capacity, std::size_t *required) noexcept
{
    if (!options) {
        clear_diagnostic(diagnostic, capacity, required);
        write_diagnostic("invalid argument", diagnostic, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    return open_session_impl(library_path, instance, aperture_config_id,
                             options->max_async_requests, out_session,
                             diagnostic, capacity, required);
}

extern "C" ams_mel_status_t ams_mel_session_get_provider_version(
    const ams_mel_session *session, ams_mel_provider_version_v1 *out_version,
    char *diagnostic, std::size_t diagnostic_capacity,
    std::size_t *diagnostic_required) noexcept
{
    clear_diagnostic(diagnostic, diagnostic_capacity, diagnostic_required);
    if (session == nullptr || out_version == nullptr ||
        (diagnostic == nullptr && diagnostic_capacity != 0U) ||
        (out_version != nullptr &&
         ((out_version->vendor == nullptr && out_version->vendor_capacity != 0U) ||
          (out_version->description == nullptr &&
           out_version->description_capacity != 0U)))) {
        write_diagnostic("invalid argument", diagnostic, diagnostic_capacity,
                         diagnostic_required);
        return AMS_MEL_INVALID_ARGUMENT;
    }

    try {
        const ams::iface::mel::VersionInfo value =
            session->state->control->getVersionInfo();
        const std::string& vendor = value.getVendor();
        const std::string& description = value.getDescription();
        if (!valid_utf8(vendor) || !valid_utf8(description)) {
            write_diagnostic("provider version contains invalid UTF-8 or NUL",
                             diagnostic, diagnostic_capacity,
                             diagnostic_required);
            return AMS_MEL_PROVIDER_EXCEPTION;
        }
        const std::size_t vendor_required = vendor.size() + 1U;
        const std::size_t description_required = description.size() + 1U;
        out_version->vendor_required = vendor_required;
        out_version->description_required = description_required;
        if (out_version->vendor == nullptr ||
            out_version->vendor_capacity < vendor_required ||
            out_version->description == nullptr ||
            out_version->description_capacity < description_required) {
            write_diagnostic("provider version buffer too small", diagnostic,
                             diagnostic_capacity, diagnostic_required);
            return AMS_MEL_BUFFER_TOO_SMALL;
        }

        out_version->api_version = value.getAPIVersion();
        out_version->library_version = value.getLibVersion();
        std::memcpy(out_version->vendor, vendor.c_str(), vendor_required);
        std::memcpy(out_version->description, description.c_str(),
                    description_required);
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        write_diagnostic("allocation failed", diagnostic, diagnostic_capacity,
                         diagnostic_required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (const std::exception& error) {
        write_exception_diagnostic(error, "provider exception", diagnostic,
                                   diagnostic_capacity, diagnostic_required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    } catch (...) {
        write_diagnostic("unknown provider exception", diagnostic,
                         diagnostic_capacity, diagnostic_required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}

extern "C" ams_mel_status_t ams_mel_session_close(
    ams_mel_session **session, char *diagnostic,
    std::size_t diagnostic_capacity, std::size_t *diagnostic_required) noexcept
{
    clear_diagnostic(diagnostic, diagnostic_capacity, diagnostic_required);
    if (session == nullptr ||
        (diagnostic == nullptr && diagnostic_capacity != 0U)) {
        write_diagnostic("invalid argument", diagnostic, diagnostic_capacity,
                         diagnostic_required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    ams_mel_session *owned = *session;
    *session = nullptr;
    try {
        delete owned;
        return AMS_MEL_OK;
    } catch (const std::exception& error) {
        write_exception_diagnostic(error, "provider cleanup exception", diagnostic,
                                   diagnostic_capacity, diagnostic_required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    } catch (...) {
        write_diagnostic("unknown provider cleanup exception", diagnostic,
                         diagnostic_capacity, diagnostic_required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}
