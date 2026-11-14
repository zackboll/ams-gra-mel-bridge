#pragma once

#include <ams_mel/abi.h>
#include <irmel/library/irmel-types/Channel.h>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

namespace ams_mel_common {
/* No provider graph or family back-reference: both the family and provider
 * closure own this state, independently of the public consumption handle. */
struct CommsMetadataState {
    explicit CommsMetadataState(std::size_t capacity) : ring(capacity) {}
    std::mutex mutex;
    std::condition_variable ready;
    std::vector<ams_mel_ir_channel_comms_test_report_v1> ring;
    std::size_t head{}, size{};
    ams_mel_ir_metadata_counters_v1 counters{};
    bool active{true};
    std::atomic<bool> poisoned{};
    std::atomic<std::size_t> callbacks{};
    void receive(const ams::iface::irmel::ChannelCommsTestRep *) noexcept;
    void deactivate() noexcept;
    void fail() noexcept;
    void drain() noexcept;
};
class CommonChannelAccess;
ams_mel_status_t open_comms_metadata(const CommonChannelAccess&, std::size_t,
    ams_mel_ir_channel_comms_metadata **, char *, std::size_t, std::size_t *) noexcept;
} // namespace ams_mel_common

struct ams_mel_ir_channel_comms_metadata {
    std::shared_ptr<ams_mel_common::CommsMetadataState> state;
};
