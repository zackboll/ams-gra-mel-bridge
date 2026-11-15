#pragma once

#include <ams_mel/abi.h>
#include <irmel/library/irmel-types/ChannelCommsTestRep.h>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

/* Queue-only ownership: no provider pointers, deleters or library references.
 * A callback owns this state; the typed graph drains callbacks after channel
 * destruction and retains the provider library throughout that boundary. */
struct CommsMetadataState {
    explicit CommsMetadataState(std::size_t capacity) : queue(capacity) {}
    std::mutex mutex;
    std::condition_variable ready;
    std::vector<ams_mel_ir_channel_comms_test_report_v1> queue;
    std::size_t head{}, size{};
    ams_mel_ir_metadata_counters_v1 counters{};
    bool active{true}, failed{};
    std::atomic<std::size_t> callbacks{};
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    std::atomic<void (*)()> test_hook{};
#endif
    void receive(const ams::iface::irmel::ChannelCommsTestRep *) noexcept;
    void deactivate(bool failure = false) noexcept;
    void drain() noexcept;
};

struct ams_mel_ir_channel_comms_metadata {
    std::shared_ptr<CommsMetadataState> state;
};
