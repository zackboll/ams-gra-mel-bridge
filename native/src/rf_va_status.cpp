/* Reference argument is intentionally unnamed and never inspected. */
#include "internal/rf_va_status.hpp"
#include "internal/provider_common.hpp"
#include <atomic>
#include <chrono>
#include <limits>
#include <utility>
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
#include <cerrno>
#include <unistd.h>
namespace {
std::atomic<int> notify_fd{-1}, release_fd{-1};
std::atomic<bool> hold{};
std::atomic<ams_mel::internal::VaSignalState *> last_state{};
void before_signal_lock() noexcept
{
    if (!hold.exchange(false)) return;
    char byte = 'h';
    ssize_t result;
    do { result = write(notify_fd.load(), &byte, 1); } while (result < 0 && errno == EINTR);
    do { result = read(release_fd.load(), &byte, 1); } while (result < 0 && errno == EINTR);
}
}
extern "C" __attribute__((visibility("default"))) void ams_mel_test_va_signal_hold(int notify, int release) noexcept
{ notify_fd.store(notify); release_fd.store(release); hold.store(true); }
extern "C" __attribute__((visibility("default"))) unsigned ams_mel_test_va_signal_waiters(
    const ams_mel_rf_va_status_subscription *owner) noexcept
{ std::lock_guard lock{owner->state->mutex}; return owner->state->waiters; }
extern "C" __attribute__((visibility("default"))) void ams_mel_test_va_signal_preset(
    ams_mel_rf_va_status_subscription *owner, std::uint64_t value) noexcept
{
    std::lock_guard lock{owner->state->mutex};
    owner->state->statistics = {value, value, value, value, 0, 0};
}
extern "C" __attribute__((visibility("default"))) void ams_mel_test_va_signal_last(
    ams_mel_rf_va_status_subscription_statistics_v1 *output) noexcept
{
    auto *state = last_state.load();
    std::lock_guard lock{state->mutex};
    *output = state->statistics;
    output->pending = state->pending ? 1U : 0U;
    output->stopped = state->stopped ? 1U : 0U;
}
#endif
using namespace ams_mel::internal;
namespace {
void increment(std::uint64_t& value) noexcept
{ if (value != std::numeric_limits<std::uint64_t>::max()) ++value; }
bool bad_diag(const char *buffer, std::size_t capacity) noexcept
{ return !buffer && capacity != 0; }
}
namespace ams_mel::internal {
std::unique_ptr<PermanentVaCallable> prepare_va_callable(
    const std::shared_ptr<VaSignalState>& state, std::shared_ptr<SharedLibrary> library)
{
    auto shell = std::make_unique<PermanentVaCallable>();
    shell->library = std::move(library);
    shell->state = state;
    shell->callback = [state](ams::iface::rfmel::BaseVirtualAperture&) noexcept {
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
        before_signal_lock();
#endif
        try {
            std::lock_guard lock{state->mutex};
            increment(state->statistics.callback_entries);
            if (state->stopped) { increment(state->statistics.callbacks_after_stop); return; }
            if (state->pending) increment(state->statistics.callbacks_coalesced);
            state->pending = true;
            state->changed.notify_all();
        } catch (...) { /* No exception may cross the provider callback boundary. */ }
    };
    return shell;
}
void retain_va_callable(PermanentVaCallable *shell) noexcept
{
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    last_state.store(shell->state.get());
#endif
    static std::atomic<PermanentVaCallable *> root{};
    auto *head = root.load(std::memory_order_relaxed);
    do { shell->next = head; }
    while (!root.compare_exchange_weak(head, shell, std::memory_order_release,
                                       std::memory_order_relaxed));
}
void stop_va_signal(const std::shared_ptr<VaSignalState>& state) noexcept
{
    if (!state) return;
    try {
        std::lock_guard lock{state->mutex};
        state->stopped = true;
        state->pending = false;
        state->changed.notify_all();
    } catch (...) {}
}
void remove_va_callback(ams::iface::rfmel::BaseVirtualAperture& provider, VaRegistration& registration) noexcept
{
    stop_va_signal(registration.state);
    if (!registration.key_known || registration.removal_attempted) return;
    registration.removal_attempted = true; // Consumed even if diagnostic construction fails.
    try { provider.removeStatusCallback(registration.key); }
    catch (...) {
        registration.removal_status = translate_provider_exception(
            "VA callback removal exception", "unknown VA callback removal exception",
            registration.diagnostic.data(), registration.diagnostic.size(), nullptr);
    }
}
}
extern "C" ams_mel_status_t ams_mel_rf_va_status_subscription_wait(
    ams_mel_rf_va_status_subscription *owner, std::uint32_t timeout_ms,
    char *diagnostic, std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!owner || !owner->state || bad_diag(diagnostic, capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try {
        auto& state = *owner->state;
        std::unique_lock lock{state.mutex};
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
        ++state.waiters;
        struct Exit { unsigned& count; ~Exit() { --count; } } exit{state.waiters};
#endif
        if (!state.changed.wait_for(lock, std::chrono::milliseconds{timeout_ms},
                                   [&] { return state.stopped || state.pending; })) return AMS_MEL_TIMEOUT;
        if (state.stopped) return AMS_MEL_STREAM_STOPPED;
        state.pending = false;
        increment(state.statistics.notifications_delivered);
        return AMS_MEL_OK;
    } catch (...) { return AMS_MEL_INTERNAL_ERROR; }
}
extern "C" ams_mel_status_t ams_mel_rf_va_status_subscription_get_statistics(
    const ams_mel_rf_va_status_subscription *owner,
    ams_mel_rf_va_status_subscription_statistics_v1 *output,
    char *diagnostic, std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!owner || !owner->state || !output || bad_diag(diagnostic, capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::lock_guard lock{owner->state->mutex};
        auto value = owner->state->statistics;
        value.pending = owner->state->pending ? 1U : 0U;
        value.stopped = owner->state->stopped ? 1U : 0U;
        *output = value;
        return AMS_MEL_OK;
    } catch (...) { return AMS_MEL_INTERNAL_ERROR; }
}
extern "C" ams_mel_status_t ams_mel_rf_va_status_subscription_close(
    ams_mel_rf_va_status_subscription **owner,
    char *diagnostic, std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!owner || bad_diag(diagnostic, capacity)) return AMS_MEL_INVALID_ARGUMENT;
    auto *owned = std::exchange(*owner, nullptr);
    if (owned) { stop_va_signal(owned->state); delete owned; }
    return AMS_MEL_OK;
}
