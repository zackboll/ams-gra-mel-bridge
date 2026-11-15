#pragma once

#include "common_channel_requests.hpp"

namespace ams_mel_common {
/* Synchronous lifecycle call only; no request engine or persistent ownership.
 * Each adapter supplies its own mutex, lifecycle and existing finish claim. */
template<class State, class Lifecycle, class Finish, class Retain>
ams_mel_status_t disable_attached(const std::shared_ptr<State>& state,
    std::mutex& mutex, Lifecycle& lifecycle, Lifecycle attached, Lifecycle enabled,
    Lifecycle disabling, Lifecycle failed, bool eligible, bool& obligation,
    bool& disable_failed, std::size_t& requests,
    const std::shared_ptr<ams::iface::irmel::Channel>& provider,
    Finish finish, Retain retain, ams_mel_ir_return_t *result) noexcept
{
    std::shared_ptr<ams::iface::irmel::Channel> channel;
    try {
        {
            std::lock_guard lock{mutex};
            if (!eligible || !provider) return AMS_MEL_PROVIDER_FAILED;
            if (lifecycle == attached) return AMS_MEL_OK;
            if (lifecycle != enabled) return AMS_MEL_PROVIDER_FAILED;
            channel = provider;
            ++requests;
            lifecycle = disabling;
            // Consume this cycle's attempt before provider entry, even on failure.
            // Detach, not a silent second disable(), establishes cleanup safety.
            obligation = false;
        }
        ams_mel_status_t status = AMS_MEL_OK;
        try {
            const auto returned = channel->disable();
            *result = static_cast<ams_mel_ir_return_t>(returned);
            if (returned != ams::iface::irmel::Return::Success)
                status = AMS_MEL_PROVIDER_FAILED;
        } catch (...) { status = AMS_MEL_PROVIDER_EXCEPTION; }
        {
            std::lock_guard lock{mutex};
            disable_failed = status != AMS_MEL_OK;
            // Logical Close is terminal and always wins publication.
            if (lifecycle == disabling)
                lifecycle = disable_failed ? failed : attached;
        }
        channel.reset();
        if (!finish(state) && status == AMS_MEL_OK) status = AMS_MEL_PROVIDER_FAILED;
        return status;
    } catch (...) {
        // A lock/publication failure after admission cannot prove safe cleanup.
        retain(state);
        return AMS_MEL_INTERNAL_ERROR;
    }
}
} // namespace ams_mel_common
