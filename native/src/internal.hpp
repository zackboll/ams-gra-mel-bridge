#pragma once

#include <irmel/library/irmel-types/Control.h>

#include <dlfcn.h>

#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

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

struct SessionState {
    std::unique_ptr<SharedLibrary> library;
    std::shared_ptr<API_Manager> manager;
    std::shared_ptr<ams::iface::irmel::Control> control;
    std::string instance;
};

struct ams_mel_session {
    std::shared_ptr<SessionState> state;
};
