/* Distinct RF C2 factory and parent-first owner. */
#include <ams_mel/abi.h>
#include "internal/provider_common.hpp"
#include "internal/rf_c2.hpp"
#include "internal/shared_library.hpp"

#include <rfmel/c2/C2MEL.h>
#include <rfmel/factory/RFCreateFunctions.h>

#include <atomic>
#include <memory>
#include <limits>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>

namespace rfmel = ams::iface::rfmel;
using namespace ams_mel::internal;

namespace {
using C2Factory = std::shared_ptr<rfmel::C2MEL> (*)(std::string_view);
static_assert(std::is_same_v<C2Factory, rfmel::fnC2MEL>);

bool bad_diagnostic(const char *buffer, std::size_t capacity) noexcept
{ return buffer == nullptr && capacity != 0U; }
} // namespace

namespace ams_mel::internal {
void retain_c2_forever(const std::shared_ptr<RfC2State>& state) noexcept
{
    static std::atomic<RfC2State *> root{};
    bool expected = false;
    if (!state->retained.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        return;
    state->emergency_self = state;
    RfC2State *head = root.load(std::memory_order_relaxed);
    do { state->emergency_next = head; }
    while (!root.compare_exchange_weak(head, state.get(), std::memory_order_release,
                                       std::memory_order_relaxed));
}

C2ShutdownOutcome finish_c2_shutdown(const std::shared_ptr<RfC2State>& state) noexcept
{
    C2ShutdownOutcome outcome;
    try { state->c2->shutdown(); }
    catch (...) {
        retain_c2_forever(state);
        outcome.status = translate_provider_exception(
            "provider shutdown exception", "unknown provider shutdown exception", nullptr, 0, nullptr);
        try { throw; }
        catch (const std::exception& error) {
            const char *what = error.what();
            try { outcome.message = what && valid_utf8(what) ? what : "provider shutdown exception"; }
            catch (...) {}
        } catch (...) {
            try { outcome.message = "unknown provider shutdown exception"; } catch (...) {}
        }
        return outcome;
    }
    state->c2.reset();
    state->library.reset();
    return outcome;
}

bool RfC2ChildClaim::acquire(const std::shared_ptr<RfC2State>& parent, RfC2ChildClaim& out) noexcept
{
    if (!parent || out.state_) return false;
    try {
        std::lock_guard lock{parent->mutex};
        if (parent->close_requested || parent->shutdown_started ||
            parent->children == std::numeric_limits<std::size_t>::max()) return false;
        ++parent->children;
        out.state_ = parent;
        return true;
    } catch (...) { return false; }
}

bool RfC2ChildClaim::acquire_sibling(RfC2ChildClaim& output) const noexcept
{ return acquire(state_, output); }

C2ShutdownOutcome RfC2ChildClaim::release() noexcept
{
    auto parent = std::move(state_);
    if (!parent) return {};
    bool shutdown = false;
    try {
        std::lock_guard lock{parent->mutex};
        --parent->children;
        if (!parent->children && parent->close_requested && !parent->shutdown_started) {
            parent->shutdown_started = true;
            shutdown = true;
        }
    } catch (...) {
        retain_c2_forever(parent);
        return {AMS_MEL_INTERNAL_ERROR, {}};
    }
    return shutdown ? finish_c2_shutdown(parent) : C2ShutdownOutcome{};
}
} // namespace ams_mel::internal

extern "C" ams_mel_status_t ams_mel_rf_c2_open(
    const char *library_path, const char *configuration, ams_mel_rf_c2 **out_c2,
    char *diagnostic, std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!library_path || !configuration || !out_c2 || *out_c2 ||
        bad_diagnostic(diagnostic, capacity)) {
        write_diagnostic("invalid argument", diagnostic, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    std::shared_ptr<SharedLibrary> library;
    try { library = std::make_shared<SharedLibrary>(library_path); }
    catch (const std::bad_alloc&) {
        write_diagnostic("allocation failed", diagnostic, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (const std::exception& error) {
        write_exception_diagnostic(error, "library load failed", diagnostic, capacity, required);
        return AMS_MEL_LIBRARY_LOAD_FAILED;
    } catch (...) {
        write_diagnostic("unknown library-load exception", diagnostic, capacity, required);
        return AMS_MEL_LIBRARY_LOAD_FAILED;
    }
    C2Factory factory{};
    try { factory = library->symbol<C2Factory>("createC2MEL"); }
    catch (const std::bad_alloc&) {
        write_diagnostic("allocation failed", diagnostic, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (const std::exception& error) {
        write_exception_diagnostic(error, "symbol resolution failed", diagnostic, capacity, required);
        return AMS_MEL_SYMBOL_NOT_FOUND;
    } catch (...) {
        write_diagnostic("unknown symbol-resolution exception", diagnostic, capacity, required);
        return AMS_MEL_SYMBOL_NOT_FOUND;
    }
    std::shared_ptr<RfC2State> state;
    std::unique_ptr<ams_mel_rf_c2> owner;
    try {
        state = std::make_shared<RfC2State>();
        owner = std::make_unique<ams_mel_rf_c2>();
    } catch (...) {
        write_diagnostic("allocation failed", diagnostic, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    /* The local C2 is destroyed before the local library on failure. */
    std::shared_ptr<rfmel::C2MEL> c2;
    try { c2 = factory(std::string_view{configuration}); }
    catch (...) {
        return translate_provider_exception("provider factory exception",
                                            "unknown provider factory exception",
                                            diagnostic, capacity, required);
    }
    if (!c2) {
        write_diagnostic("createC2MEL returned null", diagnostic, capacity, required);
        return AMS_MEL_FACTORY_FAILED;
    }
    state->library = std::move(library);
    state->c2 = std::move(c2);
    owner->state = std::move(state);
    *out_c2 = owner.release();
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_rf_c2_close(
    ams_mel_rf_c2 **c2, char *diagnostic, std::size_t capacity,
    std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!c2 || bad_diagnostic(diagnostic, capacity)) {
        write_diagnostic("invalid argument", diagnostic, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    ams_mel_rf_c2 *owned = std::exchange(*c2, nullptr);
    if (!owned) return AMS_MEL_OK;
    auto state = std::move(owned->state);
    delete owned;
    if (!state || !state->c2) return AMS_MEL_OK;
    bool shutdown = false;
    try {
        std::lock_guard lock{state->mutex};
        state->close_requested = true;
        if (state->children == 0U && !state->shutdown_started) {
            state->shutdown_started = true;
            shutdown = true;
        }
    } catch (...) {
        retain_c2_forever(state);
        write_diagnostic("C2 close lock failed; provider retained", diagnostic, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    if (!shutdown) return AMS_MEL_OK;
    const auto outcome = finish_c2_shutdown(state);
    write_diagnostic(outcome.message, diagnostic, capacity, required);
    return outcome.status;
}