#include <ams_mel/abi.h>

#include <irmel/library/irmel-types/Control.h>

#include <dlfcn.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

class SharedLibrary {
public:
    explicit SharedLibrary(const char *path) : handle_{dlopen(path, RTLD_NOW | RTLD_LOCAL)}
    {
        if (handle_ == nullptr) {
            const char *message = dlerror();
            throw std::runtime_error(message == nullptr ? "dlopen failed" : message);
        }
    }

    ~SharedLibrary()
    {
        if (handle_ != nullptr) {
            (void)dlclose(handle_);
        }
    }

    SharedLibrary(const SharedLibrary&) = delete;
    SharedLibrary& operator=(const SharedLibrary&) = delete;

    template<typename Function>
    Function symbol(const char *name) const
    {
        dlerror();
        void *address = dlsym(handle_, name);
        const char *error = dlerror();
        if (error != nullptr || address == nullptr) {
            throw std::runtime_error(error == nullptr ? "symbol not found" : error);
        }
        Function function{};
        static_assert(sizeof(function) == sizeof(address));
        std::memcpy(&function, &address, sizeof(function));
        return function;
    }

private:
    void *handle_;
};

using ManagerFactory = std::shared_ptr<API_Manager> (*)(const std::string&);
using ControlFactory = std::shared_ptr<ams::iface::irmel::Control> (*)(
    std::string_view, std::shared_ptr<API_Manager>);

void write_diagnostic(const std::string& message, char *buffer,
                      std::size_t capacity, std::size_t *required) noexcept
{
    if (required != nullptr) {
        *required = message.size() + 1U;
    }
    if (buffer != nullptr && capacity != 0U) {
        const std::size_t copied = std::min(message.size(), capacity - 1U);
        std::memcpy(buffer, message.data(), copied);
        buffer[copied] = '\0';
    }
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

bool valid_utf8(const std::string& value) noexcept
{
    std::size_t index = 0;
    while (index < value.size()) {
        const auto lead = static_cast<unsigned char>(value[index]);
        if (lead == 0U) {
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

struct ams_mel_session {
    std::unique_ptr<SharedLibrary> library;
    std::shared_ptr<API_Manager> manager;
    std::shared_ptr<ams::iface::irmel::Control> control;
};

extern "C" ams_mel_status_t ams_mel_session_open(
    const char *library_path, const char *instance,
    const char *aperture_config_id, ams_mel_session **out_session,
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
    } catch (const std::exception& error) {
        write_diagnostic(error.what(), diagnostic, diagnostic_capacity,
                         diagnostic_required);
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
    } catch (const std::exception& error) {
        write_diagnostic(error.what(), diagnostic, diagnostic_capacity,
                         diagnostic_required);
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

        auto session = std::make_unique<ams_mel_session>();
        session->library = std::move(library);
        session->manager = std::move(manager);
        session->control = std::move(control);
        *out_session = session.release();
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        write_diagnostic("allocation failed", diagnostic, diagnostic_capacity,
                         diagnostic_required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (const std::exception& error) {
        write_diagnostic(error.what(), diagnostic, diagnostic_capacity,
                         diagnostic_required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    } catch (...) {
        write_diagnostic("unknown provider exception", diagnostic,
                         diagnostic_capacity, diagnostic_required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
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
            session->control->getVersionInfo();
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
        write_diagnostic(error.what(), diagnostic, diagnostic_capacity,
                         diagnostic_required);
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
        write_diagnostic(error.what(), diagnostic, diagnostic_capacity,
                         diagnostic_required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    } catch (...) {
        write_diagnostic("unknown provider cleanup exception", diagnostic,
                         diagnostic_capacity, diagnostic_required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}