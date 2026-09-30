/* Task 034B1B1: distinct RF C2 factory owner. No child is published yet;
 * 034B1B2 will add child accounting to this private state. */
#include <ams_mel/abi.h>
#include "internal/provider_common.hpp"
#include "internal/shared_library.hpp"

#include <rfmel/c2/C2MEL.h>
#include <rfmel/factory/RFCreateFunctions.h>

#include <atomic>
#include <memory>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>

namespace rfmel = ams::iface::rfmel;
using namespace ams_mel::internal;

namespace {
using C2Factory = std::shared_ptr<rfmel::C2MEL> (*)(std::string_view);
static_assert(std::is_same_v<C2Factory, rfmel::fnC2MEL>);

struct RfC2State {
    std::shared_ptr<SharedLibrary> library;
    std::shared_ptr<rfmel::C2MEL> c2;
    std::shared_ptr<RfC2State> emergency_self;
    RfC2State *emergency_next{};
    std::atomic<bool> retained{};
    ~RfC2State() { c2.reset(); library.reset(); }
};

void retain_forever(const std::shared_ptr<RfC2State>& state) noexcept
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

bool bad_diagnostic(const char *buffer, std::size_t capacity) noexcept
{ return buffer == nullptr && capacity != 0U; }
} // namespace

struct ams_mel_rf_c2 {
    std::shared_ptr<RfC2State> state;
};

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
    try { state->c2->shutdown(); }
    catch (...) {
        retain_forever(state);
        return translate_provider_exception("provider shutdown exception",
                                            "unknown provider shutdown exception",
                                            diagnostic, capacity, required);
    }
    state->c2.reset();
    state->library.reset();
    return AMS_MEL_OK;
}