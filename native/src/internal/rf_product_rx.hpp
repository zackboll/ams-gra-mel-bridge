#pragma once

/* Private RF DataMEL owner state shared by rf_data.cpp (Task 033B) and
 * rf_product_rx.cpp (Task 033D). Nothing here crosses the C boundary.
 *
 *     ams_mel_rf_data ----------------------------+
 *     RfChildClaim (create request / endpoint) ---+--shared_ptr--> RfDataState
 *                                                   library : shared_ptr<SharedLibrary>
 *                                                   data    : shared_ptr<rfmel::DataMEL>
 *                                                   children, close_requested
 *
 * DataMEL::shutdown() runs exactly once: synchronously in ams_mel_rf_data_close
 * when no child exists, otherwise deferred to the release of the final child.
 * After a successful shutdown the DataMEL is destroyed before this state drops
 * its SharedLibrary reference. The DSO is unloaded only when the LAST
 * SharedLibrary reference drops; a permanent callback registration (Task 033D)
 * holds its own reference, so a DSO that ever received a data-ready callback
 * registration is never unloaded. */

#include <ams_mel/abi.h>
#include "shared_library.hpp"

#include <rfmel/data/DataMEL.h>

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>

namespace ams_mel::internal {

struct RfDataState {
    RfDataState() = default;
    RfDataState(const RfDataState&) = delete;
    RfDataState& operator=(const RfDataState&) = delete;
    /* Never calls shutdown(). Destroys the DataMEL before dropping this
     * state's library reference, independently of member order. */
    ~RfDataState()
    {
        data.reset();
        library.reset();
    }

    std::shared_ptr<SharedLibrary> library;
    std::shared_ptr<ams::iface::rfmel::DataMEL> data;

    /* Parent/child lifecycle. Guarded by mutex. */
    std::mutex mutex;
    std::size_t children{};
    bool close_requested{};
    bool shutdown_started{};

    /* Allocation-free permanent retention after a throwing shutdown(). */
    std::shared_ptr<RfDataState> emergency_self;
    RfDataState *emergency_next{};
    std::atomic<bool> emergency_retained{};
};

/* Outcome of the one-and-only deferred or synchronous shutdown attempt. */
struct RfShutdownOutcome {
    ams_mel_status_t status{AMS_MEL_OK};
    std::string message;
};

/* shutdown() exactly once, then DataMEL destruction, then this state's
 * library reference is dropped. On a throwing shutdown the COMPLETE state is
 * retained for the process lifetime (no retry, no destruction, no unload).
 * The caller must have claimed shutdown_started. Never throws. */
RfShutdownOutcome rf_finish_shutdown(const std::shared_ptr<RfDataState>& state) noexcept;

/* Move-only ownership of exactly one parent child count. One admitted create
 * operation holds exactly one claim for its whole life: pending future,
 * cached-but-unclaimed endpoint, and claimed endpoint all transfer the SAME
 * claim; it is never double-counted. Releasing the final child of a closed
 * parent performs the deferred shutdown on the releasing thread. */
class RfChildClaim {
public:
    RfChildClaim() = default;
    RfChildClaim(const RfChildClaim&) = delete;
    RfChildClaim& operator=(const RfChildClaim&) = delete;
    RfChildClaim(RfChildClaim&& other) noexcept : state_{std::move(other.state_)} {}
    RfChildClaim& operator=(RfChildClaim&& other) noexcept
    {
        if (this != &other) {
            release_detached();
            state_ = std::move(other.state_);
        }
        return *this;
    }
    /* A claim dropped without an explicit release has no public caller to
     * report to: a throwing deferred shutdown is retained, never retried. */
    ~RfChildClaim() { release_detached(); }

    /* Adds one child. Fails (false) only if the parent is already closing,
     * which cannot happen while its public owner exists, or if locking the
     * parent fails. Never throws. */
    static bool acquire(const std::shared_ptr<RfDataState>& state,
                        RfChildClaim& output) noexcept;

    /* Explicit release on a public caller. Returns the deferred shutdown
     * outcome when this was the final child of a closed parent, else OK. */
    RfShutdownOutcome release() noexcept;
    void release_detached() noexcept { (void)release(); }

    [[nodiscard]] bool held() const noexcept { return static_cast<bool>(state_); }
    [[nodiscard]] const std::shared_ptr<RfDataState>& state() const noexcept
    { return state_; }

private:
    std::shared_ptr<RfDataState> state_;
};

} // namespace ams_mel::internal

/* The RF Data public owner. Defined here so rf_product_rx.cpp can submit
 * through it; never exposed through C. */
struct ams_mel_rf_data {
    std::shared_ptr<ams_mel::internal::RfDataState> state;
};
