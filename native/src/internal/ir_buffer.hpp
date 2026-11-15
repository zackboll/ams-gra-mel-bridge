#pragma once

#include "../internal.hpp"
#include <irmel/library/irmel-types/Buffer.h>
#include <mutex>
#include <vector>

enum class BufferRegistration { Unregistered, Registering, Registered, Unregistering, OwnershipUncertain };

/* Reverse destruction: provider, fixed storage, Session/DSO. An unresolved
 * registration retains the concrete family graph even if Ada finalizes. */
struct ams_mel_ir_buffer {
    std::shared_ptr<SessionState> session;
    std::vector<std::uint8_t> storage;
    std::shared_ptr<ams::iface::irmel::Buffer> provider;
    std::int64_t context{};
    std::mutex mutex;
    BufferRegistration registration{BufferRegistration::Unregistered};
    std::shared_ptr<void> channel_owner;
    bool logically_unregistered{};
    bool (*reconcile)(ams_mel_ir_buffer&) noexcept{};
};

/* Called with Buffer mutex held. No provider call, retry or inferred unregister.
 * The family reconciler only removes an already successful unregister. */
inline bool buffer_accessible(ams_mel_ir_buffer& buffer) noexcept
{
    if (buffer.registration == BufferRegistration::Unregistered) return true;
    return buffer.logically_unregistered && buffer.provider.use_count() == 1 &&
           buffer.reconcile && buffer.reconcile(buffer);
}
