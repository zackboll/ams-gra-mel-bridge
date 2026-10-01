#pragma once
#include "shared_library.hpp"
#include <ams_mel/abi.h>
#include <rfmel/c2/C2MEL.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace ams_mel::internal {
struct RfC2State {
    std::shared_ptr<SharedLibrary> library;
    std::shared_ptr<ams::iface::rfmel::C2MEL> c2;
    std::mutex mutex;
    std::size_t children{};
    bool close_requested{};
    bool shutdown_started{};
    std::shared_ptr<RfC2State> emergency_self;
    RfC2State *emergency_next{};
    std::atomic<bool> retained{};
    ~RfC2State() { c2.reset(); library.reset(); }
};
struct C2ShutdownOutcome {
    ams_mel_status_t status{AMS_MEL_OK};
    std::string message;
};
void retain_c2_forever(const std::shared_ptr<RfC2State>&) noexcept;
C2ShutdownOutcome finish_c2_shutdown(const std::shared_ptr<RfC2State>&) noexcept;
class RfC2ChildClaim {
public:
    RfC2ChildClaim() = default;
    RfC2ChildClaim(const RfC2ChildClaim&) = delete;
    RfC2ChildClaim& operator=(const RfC2ChildClaim&) = delete;
    RfC2ChildClaim(RfC2ChildClaim&& other) noexcept : state_{std::move(other.state_)} {}
    RfC2ChildClaim& operator=(RfC2ChildClaim&& other) noexcept
    {
        if (this != &other) { (void)release(); state_ = std::move(other.state_); }
        return *this;
    }
    ~RfC2ChildClaim() { (void)release(); }
    static bool acquire(const std::shared_ptr<RfC2State>&, RfC2ChildClaim&) noexcept;
    /* Count a new sibling without transferring the existing VA's claim. */
    bool acquire_sibling(RfC2ChildClaim& output) const noexcept;
    C2ShutdownOutcome release() noexcept;
private:
    std::shared_ptr<RfC2State> state_;
};
} // namespace ams_mel::internal

struct ams_mel_rf_c2 {
    std::shared_ptr<ams_mel::internal::RfC2State> state;
};