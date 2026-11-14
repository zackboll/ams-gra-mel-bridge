#include "internal/channel_comms_metadata.hpp"

#include <chrono>
#include <utility>

namespace {
void increment(std::uint64_t& value) noexcept
{ if (value != UINT64_MAX) ++value; }
}

void CommsMetadataState::receive(
    const ams::iface::irmel::ChannelCommsTestRep *report) noexcept
{
    callbacks.fetch_add(1, std::memory_order_acq_rel);
    try {
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
        if (auto hook = test_hook.exchange(nullptr)) hook();
#endif
        std::lock_guard lock{mutex};
        increment(counters.events_received);
        if (!report) increment(counters.malformed_or_unsupported);
        else {
            const ams_mel_ir_channel_comms_test_report_v1 copy{
                report->getCommandID(), report->getRequestID()};
            if (active) {
                if (size == queue.size()) increment(counters.events_dropped_queue_full);
                else {
                    queue[(head + size) % queue.size()] = copy;
                    ++size;
                    ready.notify_one();
                }
            }
        }
    } catch (...) { deactivate(true); }
    callbacks.fetch_sub(1, std::memory_order_acq_rel);
    callbacks.notify_all();
}

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
extern "C" __attribute__((visibility("default"))) int ams_mel_test_comms_metadata_probe(
    ams_mel_ir_channel_comms_metadata *owner, unsigned operation, void (*hook)()) noexcept
{
    if (!owner || !owner->state) return 0;
    try {
        if (operation == 0) {
            std::lock_guard lock{owner->state->mutex};
            owner->state->counters = {UINT64_MAX - 1, UINT64_MAX - 1, UINT64_MAX - 1};
        } else if (operation == 1) owner->state->test_hook.store(hook);
        else return 0;
        return 1;
    } catch (...) { return 0; }
}
#endif

void CommsMetadataState::deactivate(bool failure) noexcept
{
    try {
        std::lock_guard lock{mutex};
        active = false;
        failed = failed || failure;
        ready.notify_all();
    } catch (...) {}
}

void CommsMetadataState::drain() noexcept
{
    deactivate();
    auto count = callbacks.load(std::memory_order_acquire);
    while (count) {
        callbacks.wait(count, std::memory_order_acquire);
        count = callbacks.load(std::memory_order_acquire);
    }
}

extern "C" ams_mel_status_t ams_mel_ir_channel_comms_metadata_receive(
    const ams_mel_ir_channel_comms_metadata *owner, std::uint32_t timeout_ms,
    ams_mel_ir_channel_comms_test_report_v1 *report, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    if (required) *required = 1;
    if (out && capacity) *out = '\0';
    if (!owner || !owner->state || !report || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        auto& state = *owner->state;
        std::unique_lock lock{state.mutex};
        if (!state.size && state.active && timeout_ms)
            state.ready.wait_for(lock, std::chrono::milliseconds{timeout_ms}, [&] {
                return state.size || !state.active;
            });
        if (state.size) {
            *report = state.queue[state.head];
            state.head = (state.head + 1) % state.queue.size();
            --state.size;
            return AMS_MEL_OK;
        }
        if (state.failed) return AMS_MEL_PROVIDER_FAILED;
        return state.active ? AMS_MEL_TIMEOUT : AMS_MEL_STREAM_STOPPED;
    } catch (...) { return AMS_MEL_INTERNAL_ERROR; }
}

extern "C" ams_mel_status_t ams_mel_ir_channel_comms_metadata_get_counters(
    const ams_mel_ir_channel_comms_metadata *owner,
    ams_mel_ir_metadata_counters_v1 *output, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    if (required) *required = 1;
    if (out && capacity) *out = '\0';
    if (!owner || !owner->state || !output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::lock_guard lock{owner->state->mutex};
        *output = owner->state->counters;
        return AMS_MEL_OK;
    } catch (...) { return AMS_MEL_INTERNAL_ERROR; }
}

extern "C" ams_mel_status_t ams_mel_ir_channel_comms_metadata_close(
    ams_mel_ir_channel_comms_metadata **owner, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    if (required) *required = 1;
    if (out && capacity) *out = '\0';
    if (!owner || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    auto *value = std::exchange(*owner, nullptr);
    if (value) value->state->deactivate();
    delete value;
    return AMS_MEL_OK;
}
