#include "internal/common_channel_requests.hpp"
#include "internal/provider_common.hpp"

#include <chrono>
#include <limits>
#include <utility>
#include <type_traits>

namespace {
using ams_mel::internal::write_diagnostic;
void increment(std::uint64_t& value) noexcept
{ if (value != std::numeric_limits<std::uint64_t>::max()) ++value; }
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
std::mutex probe_mutex;
std::condition_variable probe_ready;
bool park{}, entered{}, draining{}, waiting{}, poison_next{};
#endif
}

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
/* Test facade only. No production exports/state or provider-pointer registry.
 * 0 park next callback; 1 await entry; 2 release; 3 await drain;
 * 4 await Receive; 5 reset observations; 6 poison next callback;
 * 7 preset counters at UINT64_MAX-1. All waits are bounded. */
extern "C" __attribute__((visibility("default"))) int ams_mel_test_comms_metadata(
    unsigned operation, const ams_mel_ir_channel_comms_metadata *owner) noexcept
{
    try {
        if (operation == 7U) {
            if (!owner) return 0;
            std::lock_guard lock{owner->state->mutex};
            const auto value = UINT64_MAX - 1U;
            owner->state->counters = {value, value, value};
            return 1;
        }
        std::unique_lock lock{probe_mutex};
        switch (operation) {
        case 0: park = true; entered = false; draining = false; return 1;
        case 1: return probe_ready.wait_for(lock, std::chrono::seconds{10}, [] { return entered; });
        case 2: park = false; probe_ready.notify_all(); return 1;
        case 3: return probe_ready.wait_for(lock, std::chrono::seconds{10}, [] { return draining; });
        case 4: return probe_ready.wait_for(lock, std::chrono::seconds{10}, [] { return waiting; });
        case 5: waiting = false; draining = false; entered = false; return 1;
        case 6: poison_next = true; return 1;
        default: return 0;
        }
    } catch (...) { return 0; }
}
#endif

namespace ams_mel_common {
static_assert(std::is_same_v<decltype(std::declval<ams::iface::irmel::ChannelCommsTestRep>().getCommandID()), std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<ams::iface::irmel::ChannelCommsTestRep>().getRequestID()), std::uint32_t>);

void CommsMetadataState::receive(const ams::iface::irmel::ChannelCommsTestRep *input) noexcept
{
    callbacks.fetch_add(1U, std::memory_order_acq_rel);
    struct Guard {
        CommsMetadataState& state;
        ~Guard() { if (state.callbacks.fetch_sub(1U, std::memory_order_acq_rel) == 1U)
            state.callbacks.notify_all(); }
    } guard{*this};
    try {
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
        {
            std::unique_lock probe{probe_mutex};
            if (park) {
                entered = true; probe_ready.notify_all();
                probe_ready.wait(probe, [] { return !park; });
            }
            if (std::exchange(poison_next, false)) { probe.unlock(); fail(); return; }
        }
#endif
        /* Copy while the provider's borrowed payload is valid; the queue never
         * allocates or calls provider code. This lock linearizes acceptance. */
        std::lock_guard lock{mutex};
        increment(counters.events_received);
        if (!input) { increment(counters.malformed_or_unsupported); return; }
        const ams_mel_ir_channel_comms_test_report_v1 report{
            input->getCommandID(), input->getRequestID()};
        if (!active || poisoned.load(std::memory_order_acquire)) return;
        if (size == ring.size()) { increment(counters.events_dropped_queue_full); return; }
        ring[(head + size) % ring.size()] = report;
        ++size;
        ready.notify_one();
    } catch (...) { fail(); }
}
void CommsMetadataState::deactivate() noexcept
{
    try { std::lock_guard lock{mutex}; active = false; ready.notify_all(); }
    catch (...) { fail(); }
}
void CommsMetadataState::fail() noexcept
{
    try { std::lock_guard lock{mutex}; poisoned.store(true, std::memory_order_release); }
    catch (...) { poisoned.store(true, std::memory_order_release); }
    ready.notify_all();
}
void CommsMetadataState::drain() noexcept
{
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    try { std::lock_guard probe{probe_mutex}; draining = true; probe_ready.notify_all(); }
    catch (...) { fail(); }
#endif
    auto count = callbacks.load(std::memory_order_acquire);
    while (count) { callbacks.wait(count, std::memory_order_acquire);
        count = callbacks.load(std::memory_order_acquire); }
}

ams_mel_status_t open_comms_metadata(const CommonChannelAccess& access, std::size_t count,
    ams_mel_ir_channel_comms_metadata **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    std::shared_ptr<CommsMetadataState> queue;
    try {
        auto family = access.lock();
        if (!family) {
            write_diagnostic("Channel view expired", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        /* All bridge allocations precede the one-shot claim. */
        queue = std::make_shared<CommsMetadataState>(count);
        auto owner = std::make_unique<ams_mel_ir_channel_comms_metadata>();
        owner->state = queue;
        std::function<void(ams::iface::irmel::Channel&,
            const ams::iface::irmel::ChannelCommsTestRep *const)> callback{
            [queue](ams::iface::irmel::Channel&,
                    const ams::iface::irmel::ChannelCommsTestRep *report) noexcept {
                queue->receive(report);
            }};
        CommonRequestClaim claim;
        std::shared_ptr<ams::iface::irmel::Channel> channel;
        auto status = access.claim_comms(family, queue, channel, claim);
        if (status != AMS_MEL_OK) {
            write_diagnostic(status == AMS_MEL_INITIALIZATION_FAILED ?
                "CommsTest callback registration unsupported for this family" :
                "CommsTest callback already attempted or channel not eligible",
                out, capacity, required);
            return status;
        }
        /* The family owns queue before provider entry. A retained callback on
         * failure remains valid until channel destruction; no public owner is
         * published. Drop the temporary channel BEFORE finishing accounting. */
        try {
            const auto result = channel->registerMetadataCallback(std::move(callback));
            if (result != ams::iface::irmel::Return::Success) {
                status = AMS_MEL_PROVIDER_FAILED;
                write_diagnostic("ChannelCommsTest callback registration failed", out, capacity, required);
            }
        } catch (const std::exception& error) {
            status = AMS_MEL_PROVIDER_EXCEPTION;
            ams_mel::internal::write_exception_diagnostic(error, "provider registration exception",
                                                         out, capacity, required);
        } catch (...) {
            status = AMS_MEL_PROVIDER_EXCEPTION;
            write_diagnostic("unknown provider registration exception", out, capacity, required);
        }
        if (status != AMS_MEL_OK) queue->deactivate();
        channel.reset();
        if (!claim.finish()) {
            queue->fail();
            if (status == AMS_MEL_OK) status = AMS_MEL_PROVIDER_FAILED;
        }
        if (status == AMS_MEL_OK) *output = owner.release();
        return status;
    } catch (...) {
        if (queue) queue->deactivate();
        write_diagnostic("CommsTest metadata allocation or claim failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}
} // namespace ams_mel_common

extern "C" ams_mel_status_t ams_mel_ir_channel_comms_metadata_receive(
    const ams_mel_ir_channel_comms_metadata *owner, std::uint32_t timeout_ms,
    ams_mel_ir_channel_comms_test_report_v1 *report, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    write_diagnostic("", out, capacity, required);
    if (!owner || !owner->state || !report || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try {
        auto state = owner->state;
        std::unique_lock lock{state->mutex};
        const auto ready = [&] { return state->size || !state->active || state->poisoned.load(); };
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
        if (!ready() && timeout_ms) {
            std::lock_guard probe{probe_mutex}; waiting = true; probe_ready.notify_all();
        }
#endif
        if (!ready() && timeout_ms) (void)state->ready.wait_for(lock,
            std::chrono::milliseconds{timeout_ms}, ready);
        if (state->size) {
            *report = state->ring[state->head];
            state->head = (state->head + 1U) % state->ring.size();
            --state->size;
            return AMS_MEL_OK;
        }
        if (state->poisoned.load()) return AMS_MEL_PROVIDER_FAILED;
        return state->active ? AMS_MEL_TIMEOUT : AMS_MEL_STREAM_STOPPED;
    } catch (...) { return AMS_MEL_INTERNAL_ERROR; }
}
extern "C" ams_mel_status_t ams_mel_ir_channel_comms_metadata_get_counters(
    const ams_mel_ir_channel_comms_metadata *owner, ams_mel_ir_metadata_counters_v1 *output,
    char *out, std::size_t capacity, std::size_t *required) noexcept
{
    write_diagnostic("", out, capacity, required);
    if (!owner || !owner->state || !output || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try { std::lock_guard lock{owner->state->mutex}; *output = owner->state->counters; return AMS_MEL_OK; }
    catch (...) { return AMS_MEL_INTERNAL_ERROR; }
}
extern "C" ams_mel_status_t ams_mel_ir_channel_comms_metadata_close(
    ams_mel_ir_channel_comms_metadata **owner, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    write_diagnostic("", out, capacity, required);
    if (!owner || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    auto *value = std::exchange(*owner, nullptr);
    if (value) value->state->deactivate();
    delete value;
    return AMS_MEL_OK;
}
