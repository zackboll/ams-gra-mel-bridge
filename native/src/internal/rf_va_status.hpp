#pragma once
#include <ams_mel/abi.h>
#include "shared_library.hpp"
#include <rfmel/virtualaperture/BaseVirtualAperture.h>
#include <array>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>

namespace ams_mel::internal {
struct VaSignalState {
    std::mutex mutex;
    std::condition_variable changed;
    bool stopped{}, pending{};
    ams_mel_rf_va_status_subscription_statistics_v1 statistics{};
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    unsigned waiters{};
#endif
};
/* Owned only by the public VA. The observer never borrows that owner. */
struct VaRegistration {
    std::shared_ptr<VaSignalState> state;
    bool key_known{}, removal_attempted{};
    std::size_t key{};
    ams_mel_status_t removal_status{AMS_MEL_OK};
    std::array<char, 512> diagnostic{};
};
/* Process-lifetime shell: no provider object, claim, or registration control. */
struct PermanentVaCallable {
    std::shared_ptr<SharedLibrary> library;
    std::shared_ptr<VaSignalState> state;
    std::function<void(ams::iface::rfmel::BaseVirtualAperture&)> callback;
    PermanentVaCallable *next{};
};
std::unique_ptr<PermanentVaCallable> prepare_va_callable(
    const std::shared_ptr<VaSignalState>&, std::shared_ptr<SharedLibrary>);
void retain_va_callable(PermanentVaCallable *) noexcept;
void stop_va_signal(const std::shared_ptr<VaSignalState>&) noexcept;
void remove_va_callback(ams::iface::rfmel::BaseVirtualAperture&, VaRegistration&) noexcept;
}
struct ams_mel_rf_va_status_subscription {
    std::shared_ptr<ams_mel::internal::VaSignalState> state;
};
