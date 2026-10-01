/* Task 034B1A: RF AdminMEL is independent of DataMEL. No children are
 * published. Shutdown failure permanently retains the complete provider graph
 * through a pre-existing shared_ptr control block, without allocating. */
#include <ams_mel/abi.h>
#include "internal/provider_common.hpp"
#include "internal/shared_library.hpp"

#include <rfmel/admin/AdminMEL.h>
#include <rfmel/admin/StatusControl.h>
#include <rfmel/factory/RFCreateFunctions.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>

namespace mel = ams::iface::mel;
namespace rfmel = ams::iface::rfmel;
using namespace ams_mel::internal;

namespace {
using AdminFactory = std::shared_ptr<rfmel::AdminMEL> (*)(std::string_view);
static_assert(std::is_same_v<AdminFactory, rfmel::fnAdminMEL>);
static_assert(std::is_same_v<std::underlying_type_t<mel::MFA_State>, std::uint32_t>);
#define CHECK_STATE(name, upstream) \
    static_assert(static_cast<std::uint32_t>(mel::MFA_State::upstream) == AMS_MEL_IR_MFA_STATE_##name)
CHECK_STATE(NOT_SET, NotSet);
CHECK_STATE(UNKNOWN, Unknown);
CHECK_STATE(NOT_INSTALLED, Not_Installed);
CHECK_STATE(OFF, Off);
CHECK_STATE(PRE_INITIALIZATION, Pre_initialization);
CHECK_STATE(INITIALIZATION, Initialization);
CHECK_STATE(STANDBY, Standby);
CHECK_STATE(OPERATE, Operate);
CHECK_STATE(OPERATE_RX_ONLY, OperateRxOnly);
CHECK_STATE(OPERATE_TX_ONLY, OperateTxOnly);
CHECK_STATE(MAINTENANCE, Maintenance);
CHECK_STATE(CALIBRATION, Calibration);
CHECK_STATE(INITIATED_BIT, Initiated_BIT);
CHECK_STATE(SHUTDOWN, Shutdown);
CHECK_STATE(DEGRADED, Degraded);
#undef CHECK_STATE
static_assert(AMS_MEL_IR_MFA_STATE_MAX_EXCLUSIVE == AMS_MEL_IR_MFA_STATE_DEGRADED + 1U);

struct AdminState {
    std::shared_ptr<SharedLibrary> library;
    std::shared_ptr<rfmel::AdminMEL> admin;
    std::shared_ptr<AdminState> emergency_self;
    AdminState *emergency_next{};
    std::atomic<bool> retained{};
    ~AdminState() { admin.reset(); library.reset(); }
};

void retain_forever(const std::shared_ptr<AdminState>& state) noexcept
{
    static std::atomic<AdminState *> retained{};
    bool expected = false;
    if (!state->retained.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        return;
    state->emergency_self = state;
    AdminState *head = retained.load(std::memory_order_relaxed);
    do { state->emergency_next = head; }
    while (!retained.compare_exchange_weak(head, state.get(), std::memory_order_release,
                                           std::memory_order_relaxed));
}

bool invalid_diagnostic(const char *buffer, std::size_t capacity) noexcept
{
    return buffer == nullptr && capacity != 0U;
}
} // namespace

struct ams_mel_rf_admin {
    std::shared_ptr<AdminState> state;
};

extern "C" ams_mel_status_t ams_mel_rf_admin_open(
    const char *library_path, const char *configuration, ams_mel_rf_admin **out_admin,
    char *diagnostic, std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (library_path == nullptr || configuration == nullptr || out_admin == nullptr ||
        *out_admin != nullptr || invalid_diagnostic(diagnostic, capacity)) {
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
    AdminFactory factory{};
    try { factory = library->symbol<AdminFactory>("createAdminMEL"); }
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
    std::shared_ptr<AdminState> state;
    std::unique_ptr<ams_mel_rf_admin> owner;
    try {
        state = std::make_shared<AdminState>();
        owner = std::make_unique<ams_mel_rf_admin>();
    } catch (...) {
        write_diagnostic("allocation failed", diagnostic, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    /* Local admin is destroyed before local library on all failure paths. */
    std::shared_ptr<rfmel::AdminMEL> admin;
    try { admin = factory(std::string_view{configuration}); }
    catch (...) {
        return translate_provider_exception("provider factory exception",
                                            "unknown provider factory exception",
                                            diagnostic, capacity, required);
    }
    if (!admin) {
        write_diagnostic("createAdminMEL returned null", diagnostic, capacity, required);
        return AMS_MEL_FACTORY_FAILED;
    }
    state->library = std::move(library);
    state->admin = std::move(admin);
    owner->state = std::move(state);
    *out_admin = owner.release();
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_rf_admin_command_state(
    ams_mel_rf_admin *admin, ams_mel_rf_mfa_state_t state, std::uint32_t *accepted,
    char *diagnostic, std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (admin == nullptr || !admin->state || !admin->state->admin || accepted == nullptr ||
        state >= AMS_MEL_IR_MFA_STATE_MAX_EXCLUSIVE ||
        invalid_diagnostic(diagnostic, capacity)) {
        write_diagnostic("invalid argument", diagnostic, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    try {
        auto uci = admin->state->admin->getUCIControl();
        if (!uci) {
            write_diagnostic("AdminMEL::getUCIControl returned null", diagnostic, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        auto status = uci->getStatusControl();
        if (!status) {
            write_diagnostic("UCI_Control::getStatusControl returned null", diagnostic, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        *accepted = status->commandState(static_cast<mel::MFA_State>(state)) ? 1U : 0U;
        return AMS_MEL_OK;
    } catch (...) {
        return translate_provider_exception("provider commandState exception",
                                            "unknown provider commandState exception",
                                            diagnostic, capacity, required);
    }
}

extern "C" ams_mel_status_t ams_mel_rf_admin_close(
    ams_mel_rf_admin **admin, char *diagnostic, std::size_t capacity,
    std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (admin == nullptr || invalid_diagnostic(diagnostic, capacity)) {
        write_diagnostic("invalid argument", diagnostic, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    ams_mel_rf_admin *owned = std::exchange(*admin, nullptr);
    if (!owned) return AMS_MEL_OK;
    auto state = std::move(owned->state);
    delete owned;
    if (!state || !state->admin) return AMS_MEL_OK;
    try { state->admin->shutdown(); }
    catch (...) {
        retain_forever(state);
        return translate_provider_exception("provider shutdown exception",
                                            "unknown provider shutdown exception",
                                            diagnostic, capacity, required);
    }
    state->admin.reset();
    state->library.reset();
    return AMS_MEL_OK;
}