#pragma once

#include "ir_buffer.hpp"
#include "common_channel_requests.hpp"

namespace ams_mel_common {
/* Fields protected by the family's lifecycle mutex, not request accounting. */
struct BufferRegistrations {
    std::size_t count{};
    std::size_t uncertain{};
    bool operation{};
};

template<class State, class Lifecycle, class Safe>
ams_mel_status_t buffer_registration(const std::shared_ptr<State>& state,
    std::mutex& mutex, Lifecycle& lifecycle, Lifecycle attached,
    bool eligible, BufferRegistrations& registrations,
    const std::shared_ptr<ams::iface::irmel::Channel>& provider,
    ams_mel_ir_buffer& buffer, bool removing, ams_mel_ir_return_t *result,
    bool (*reconcile)(ams_mel_ir_buffer&) noexcept, Safe safe) noexcept
{
    std::shared_ptr<ams::iface::irmel::Channel> channel;
    try {
        {
            std::scoped_lock lock{mutex, buffer.mutex};
            if (!eligible || !provider || lifecycle != attached || registrations.operation ||
                state->session != buffer.session || !safe()) return AMS_MEL_PROVIDER_FAILED;
            if (removing) {
                if (buffer.channel_owner.get() != state.get() || buffer.logically_unregistered ||
                    (buffer.registration != BufferRegistration::Registered &&
                     buffer.registration != BufferRegistration::OwnershipUncertain))
                    return AMS_MEL_PROVIDER_FAILED;
                if (buffer.registration == BufferRegistration::OwnershipUncertain)
                    --registrations.uncertain;
            } else {
                if (registrations.uncertain || buffer.registration != BufferRegistration::Unregistered)
                    return AMS_MEL_PROVIDER_FAILED;
                buffer.channel_owner = state;
                buffer.reconcile = reconcile;
                ++registrations.count;
            }
            registrations.operation = true;
            buffer.registration = removing ? BufferRegistration::Unregistering : BufferRegistration::Registering;
            channel = provider;
        }
        ams_mel_status_t status = AMS_MEL_OK;
        try {
            const auto returned = removing ? channel->unregisterBuffer(buffer.provider) :
                                            channel->registerBuffer(buffer.provider);
            *result = static_cast<ams_mel_ir_return_t>(returned);
            if (returned != ams::iface::irmel::Return::Success) status = AMS_MEL_PROVIDER_FAILED;
        } catch (...) { status = AMS_MEL_PROVIDER_EXCEPTION; }
        {
            std::scoped_lock lock{mutex, buffer.mutex};
            registrations.operation = false;
            buffer.registration = status == AMS_MEL_OK && !removing ?
                BufferRegistration::Registered : BufferRegistration::OwnershipUncertain;
            buffer.logically_unregistered = removing && status == AMS_MEL_OK;
            if (buffer.registration == BufferRegistration::OwnershipUncertain)
                ++registrations.uncertain;
            if (buffer.logically_unregistered && buffer.provider.use_count() == 1 && safe()) {
                --registrations.count;
                --registrations.uncertain;
                buffer.registration = BufferRegistration::Unregistered;
                buffer.logically_unregistered = false;
                buffer.reconcile = nullptr;
                buffer.channel_owner.reset();
            }
        }
        return status;
    } catch (...) {
        /* No ownership rollback after possible provider entry. The public
         * Buffer remains allocated, including its strong family/Session root. */
        return AMS_MEL_INTERNAL_ERROR;
    }
}

/* Buffer lock is held. The caller supplies the family-specific safety gate. */
template<class Safe>
bool reconcile_buffer(ams_mel_ir_buffer& buffer, std::mutex& mutex,
    BufferRegistrations& registrations, Safe safe) noexcept
{
    try {
        std::lock_guard lock{mutex};
        if (registrations.operation || !safe()) return false;
        --registrations.count;
        --registrations.uncertain;
        buffer.registration = BufferRegistration::Unregistered;
        buffer.logically_unregistered = false;
        buffer.reconcile = nullptr;
        buffer.channel_owner.reset();
        return true;
    } catch (...) { return false; }
}
} // namespace ams_mel_common
