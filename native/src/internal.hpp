#pragma once

#include <irmel/library/irmel-types/Control.h>

#include <dlfcn.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

struct CompletionAdmission {
    explicit CompletionAdmission(std::uint32_t limit) noexcept : maximum{limit} {}
    const std::uint32_t maximum;
    std::atomic<std::uint32_t> active{0};
};

/* A slot survives public owner closure and provider unload. Member destruction
 * in WorkerInput releases it only after the future and Completion are gone. */
class CompletionPermit {
public:
    CompletionPermit() noexcept = default;
    explicit CompletionPermit(std::shared_ptr<CompletionAdmission> admission) noexcept
        : admission_{std::move(admission)} {}
    CompletionPermit(const CompletionPermit&) = delete;
    CompletionPermit& operator=(const CompletionPermit&) = delete;
    CompletionPermit(CompletionPermit&& other) noexcept
        : admission_{std::move(other.admission_)} {}
    CompletionPermit& operator=(CompletionPermit&& other) noexcept
    {
        if (this != &other) {
            release();
            admission_ = std::move(other.admission_);
        }
        return *this;
    }
    ~CompletionPermit() noexcept { release(); }

private:
    void release() noexcept
    {
        if (admission_) {
            admission_->active.fetch_sub(1, std::memory_order_acq_rel);
            admission_.reset();
        }
    }
    std::shared_ptr<CompletionAdmission> admission_;
};

inline bool acquire_completion_permit(
    const std::shared_ptr<CompletionAdmission>& admission,
    CompletionPermit& permit) noexcept
{
    if (!admission) return true;
    auto count = admission->active.load(std::memory_order_relaxed);
    while (count < admission->maximum) {
        if (admission->active.compare_exchange_weak(count, count + 1,
                std::memory_order_acq_rel, std::memory_order_relaxed)) {
            permit = CompletionPermit{admission};
            return true;
        }
    }
    return false;
}

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
    std::shared_ptr<CompletionAdmission> admission;
    std::unique_ptr<SharedLibrary> library;
    std::shared_ptr<API_Manager> manager;
    std::shared_ptr<ams::iface::irmel::Control> control;
    std::string instance;
};

struct ams_mel_session {
    std::shared_ptr<SessionState> state;
};
