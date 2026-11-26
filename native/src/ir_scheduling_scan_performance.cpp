#include "internal/ir_scheduling_scan_performance.hpp"
#include "internal/ir_scan_param.hpp"

#include <irmel/library/irmel-types/ChannelMetadataCapabilityType.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <deque>
#include <functional>
#include <limits>
#include <mutex>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {
using namespace ams::iface::irmel;
using View = ams_mel_ir_scan_performance_report_v1;
using Event = ams_mel_ir_scheduling_scan_performance_event;
using Stream = ams_mel_ir_scheduling_scan_performance_metadata;

using Callback = std::function<void(Channel&, const ScanPerformanceReport *const)>;
using Registration = Return (SchedulingChannel::*)(Callback);
static_assert(std::is_same_v<decltype(static_cast<Registration>(
    &SchedulingChannel::registerMetadataCallback)), Registration>);
static_assert(std::is_same_v<decltype(std::declval<const ScanPerformanceReport&>().getCommanded()), ScanParam>);
static_assert(std::is_same_v<decltype(std::declval<const ScanPerformanceReport&>().getAchieved()), ScanParam>);
static_assert(static_cast<std::uint32_t>(ChannelMetadataCapabilityType::ScanPerformanceReport) ==
              AMS_MEL_IR_METADATA_SCAN_PERFORMANCE_REPORT);
static_assert(AMS_MEL_IR_METADATA_SCAN_PERFORMANCE_REPORT == 27U);
static_assert(std::is_standard_layout_v<View> && std::is_trivially_copyable_v<View>);
static_assert(std::is_same_v<decltype(View::commanded), ams_mel_ir_scan_param_v1>);
static_assert(std::is_same_v<decltype(View::achieved), ams_mel_ir_scan_param_v1>);
static_assert(sizeof(View) == 2 * sizeof(ams_mel_ir_scan_param_v1) &&
              alignof(View) == alignof(ams_mel_ir_scan_param_v1) &&
              offsetof(View, commanded) == 0 &&
              offsetof(View, achieved) == sizeof(ams_mel_ir_scan_param_v1));

void diagnostic(std::string_view text, char *out, std::size_t capacity,
                std::size_t *required) noexcept
{
    if (required) *required = text.size() + 1U;
    if (out && capacity) {
        const auto copied = std::min(text.size(), capacity - 1U);
        std::memcpy(out, text.data(), copied);
        out[copied] = '\0';
    }
}

bool map_return(Return input, ams_mel_ir_return_t& output) noexcept
{
    switch (input) {
    case Return::Success: output = AMS_MEL_IR_RETURN_SUCCESS; return true;
    case Return::BadPointer: output = AMS_MEL_IR_RETURN_BAD_POINTER; return true;
    case Return::Fail: output = AMS_MEL_IR_RETURN_FAIL; return true;
    case Return::NotSupported: output = AMS_MEL_IR_RETURN_NOT_SUPPORTED; return true;
    case Return::NotImplemented: output = AMS_MEL_IR_RETURN_NOT_IMPLEMENTED; return true;
    }
    return false;
}

void increment(std::uint64_t& value) noexcept
{
    if (value != std::numeric_limits<std::uint64_t>::max()) ++value;
}

enum class Lifecycle { Active, Inactive, Stopped, Failed };

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
std::atomic<bool> fail_next_snapshot{};
std::atomic<unsigned> corrupt_next_view{}, events_closed{};
std::mutex probe_mutex;
std::condition_variable probe_ready;
bool park_copy{}, copy_parked{}, receive_waiting{}, drain_waiting{}, overlapping_callback{};

void observe(bool& flag)
{
    std::lock_guard lock{probe_mutex};
    flag = true;
    probe_ready.notify_all();
}
#endif
} // namespace

struct ams_mel_ir_scheduling_scan_performance_event {
    View view;
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    mutable View corrupt_view;
#endif

    explicit ams_mel_ir_scheduling_scan_performance_event(const View& snapshot) noexcept : view{snapshot} {}
};

struct ScanPerformanceReportState {
    // Serialize callback admission and publication, not lifecycle or Receive.
    // An overlapping callback cannot publish ahead of an admitted payload.
    std::mutex callbacks;
    std::mutex mutex;
    std::condition_variable ready, copies_done;
    std::deque<std::unique_ptr<Event>> queue;
    std::size_t capacity{};
    bool reading{};
    ams_mel_ir_metadata_counters_v1 counters{};
    Lifecycle lifecycle{Lifecycle::Active};

    void receive(const ScanPerformanceReport *input) noexcept
    {
        bool admitted = false, slot = false;
        // Keep callback serialization through exception cleanup as well.
        std::unique_lock serial{callbacks, std::defer_lock};
        try {
            {
                std::lock_guard lock{mutex};
                increment(counters.events_received);
                if (lifecycle != Lifecycle::Active) return;
            }
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
            if (!serial.try_lock()) {
                observe(overlapping_callback);
                serial.lock();
            }
#else
            serial.lock();
#endif
            {
                std::lock_guard lock{mutex};
                // Stop or public close can win while this callback awaits admission.
                if (lifecycle != Lifecycle::Active) return;
                reading = admitted = true;
                slot = queue.size() < capacity;
            }

            bool well_formed = false;
            std::unique_ptr<Event> event;
            if (input) {
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
                {
                    std::unique_lock lock{probe_mutex};
                    if (park_copy && !copy_parked) {
                        copy_parked = true;
                        probe_ready.notify_all();
                        probe_ready.wait(lock, [] { return !park_copy; });
                    }
                }
#endif
                // Both getter results are independent, by-value ScanParams. Even
                // with a full queue, classify malformed reports before dropping.
                using namespace ams_mel::internal;
                const View snapshot{snapshot_scan(input->getCommanded()),
                                    snapshot_scan(input->getAchieved())};
                well_formed = valid_scan(snapshot.commanded) && valid_scan(snapshot.achieved);
                if (well_formed && slot) {
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
                    if (fail_next_snapshot.exchange(false)) throw std::bad_alloc{};
#endif
                    event = std::make_unique<Event>(snapshot);
                }
            }

            std::lock_guard lock{mutex};
            if (lifecycle == Lifecycle::Active) {
                if (!well_formed) increment(counters.malformed_or_unsupported);
                else if (!slot) increment(counters.events_dropped_queue_full);
                else {
                    queue.push_back(std::move(event));
                    ready.notify_one();
                }
            }
            finish();
        } catch (...) {
            // Snapshot and queue-node allocation failures affect only this event.
            if (admitted) {
                try {
                    std::lock_guard lock{mutex};
                    if (lifecycle == Lifecycle::Active) increment(counters.malformed_or_unsupported);
                    finish();
                } catch (...) {}
            }
        }
    }

    void finish() noexcept // mutex held; no further payload reads
    {
        reading = false;
        copies_done.notify_all();
    }

    void transition(Lifecycle next) noexcept
    {
        try {
            std::lock_guard lock{mutex};
            if (lifecycle == Lifecycle::Active || next == Lifecycle::Inactive ||
                (lifecycle == Lifecycle::Stopped && next == Lifecycle::Failed))
                lifecycle = next;
            if (next == Lifecycle::Inactive) queue.clear();
            ready.notify_all();
        } catch (...) {}
    }
};

struct ams_mel_ir_scheduling_scan_performance_metadata {
    std::shared_ptr<ScanPerformanceReportState> state;
};

namespace ams_mel::internal {
void scheduling_scan_performance_stopped(const std::shared_ptr<ScanPerformanceReportState>& state) noexcept
{ if (state) state->transition(Lifecycle::Stopped); }

void scheduling_scan_performance_failed(const std::shared_ptr<ScanPerformanceReportState>& state) noexcept
{ if (state) state->transition(Lifecycle::Failed); }

void scheduling_scan_performance_drained(const std::shared_ptr<ScanPerformanceReportState>& state) noexcept
{
    if (!state) return;
    scheduling_scan_performance_stopped(state);
    std::unique_lock lock{state->mutex};
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    if (state->reading) observe(drain_waiting);
#endif
    state->copies_done.wait(lock, [&] { return !state->reading; });
}
} // namespace ams_mel::internal

extern "C" ams_mel_status_t ams_mel_ir_scheduling_scan_performance_open(
    ams_mel_ir_scheduling *scheduling, std::size_t queue_capacity, Stream **output,
    ams_mel_ir_return_t *registration, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!scheduling || !queue_capacity || !output || *output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    std::unique_ptr<Stream> owner;
    Callback callback;
    try {
        owner = std::make_unique<Stream>();
        owner->state = std::make_shared<ScanPerformanceReportState>();
        owner->state->capacity = queue_capacity;
        callback = [state = owner->state](Channel&, const ScanPerformanceReport *value) noexcept {
            const auto keep = state;
            keep->receive(value);
        };
    } catch (...) {
        diagnostic("could not allocate ScanPerformanceReport stream", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    std::shared_ptr<void> graph; // outlives the local provider channel reference
    std::shared_ptr<SchedulingChannel> channel;
    if (!ams_mel::internal::claim_scheduling_scan_performance(
            *scheduling, owner->state, channel, graph)) {
        diagnostic("Scheduling owner is ineligible or ScanPerformanceReport already attempted",
                   out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    Return result{};
    try {
        result = channel->registerMetadataCallback(std::move(callback));
    } catch (...) {
        owner->state->transition(Lifecycle::Inactive);
        diagnostic("provider exception during ScanPerformanceReport registration", out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
    ams_mel_ir_return_t mapped{};
    const bool known = map_return(result, mapped);
    if (known && registration) *registration = mapped;
    if (!known || mapped != AMS_MEL_IR_RETURN_SUCCESS) {
        owner->state->transition(Lifecycle::Inactive);
        diagnostic(known ? "provider refused ScanPerformanceReport registration" :
                           "unknown ScanPerformanceReport registration result", out, capacity, required);
        return AMS_MEL_PROVIDER_FAILED;
    }
    *output = owner.release();
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_scheduling_scan_performance_receive(
    Stream *metadata, std::uint32_t timeout_ms, Event **output, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!metadata || !output || *output || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try {
        auto& state = *metadata->state;
        std::unique_lock lock{state.mutex};
        if (state.queue.empty() && state.lifecycle == Lifecycle::Active && timeout_ms) {
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
            observe(receive_waiting);
#endif
            state.ready.wait_for(lock, std::chrono::milliseconds{timeout_ms}, [&] {
                return !state.queue.empty() || state.lifecycle != Lifecycle::Active;
            });
        }
        if (!state.queue.empty()) {
            *output = state.queue.front().release();
            state.queue.pop_front();
            return AMS_MEL_OK;
        }
        if (state.lifecycle == Lifecycle::Failed) {
            diagnostic("Scheduling cleanup failed; ScanPerformanceReport stream failed", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        return state.lifecycle == Lifecycle::Active ? AMS_MEL_TIMEOUT : AMS_MEL_STREAM_STOPPED;
    } catch (...) {
        diagnostic("ScanPerformanceReport receive failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_scheduling_scan_performance_get_counters(
    const Stream *metadata, ams_mel_ir_metadata_counters_v1 *output, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!metadata || !output || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::lock_guard lock{metadata->state->mutex};
        *output = metadata->state->counters;
        return AMS_MEL_OK;
    } catch (...) {
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_scheduling_scan_performance_close(
    Stream **metadata, char *out, std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!metadata || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    std::unique_ptr<Stream> owner{std::exchange(*metadata, nullptr)};
    if (owner) owner->state->transition(Lifecycle::Inactive);
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_scheduling_scan_performance_event_view(
    const Event *event, const View **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!event || !output || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    if (const auto corruption = corrupt_next_view.exchange(0)) {
        event->corrupt_view = event->view;
        auto& value = corruption <= 4 ? event->corrupt_view.commanded : event->corrupt_view.achieved;
        switch ((corruption - 1U) % 4U) {
        case 0: value.elevation_defined_with_range_and_altitude = 2U; break;
        case 1: value.center_frame_ref_el = UINT32_MAX; break;
        case 2: value.center_frame_ref_az = 2U; break;
        case 3: value.degradation_method = UINT32_MAX; break;
        }
        *output = &event->corrupt_view;
        return AMS_MEL_OK;
    }
#endif
    *output = &event->view;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_scheduling_scan_performance_event_close(
    Event **event, char *out, std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!event || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    if (*event) ++events_closed;
#endif
    delete std::exchange(*event, nullptr);
    return AMS_MEL_OK;
}

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
extern "C" __attribute__((visibility("default"))) int ams_mel_test_scan_performance_probe(
    Stream *stream, unsigned operation) noexcept
{
    if (operation == 0U) { fail_next_snapshot = true; return 1; }
    if (operation >= 8U && operation < 16U) { corrupt_next_view = operation - 7U; return 1; }
    if (operation == 16U) return static_cast<int>(events_closed.load());
    if (operation == 1U && stream) {
        std::lock_guard lock{stream->state->mutex};
        const auto limit = std::numeric_limits<std::uint64_t>::max() - 1U;
        stream->state->counters = {limit, limit, limit};
        return 1;
    }
    std::unique_lock lock{probe_mutex};
    switch (operation) {
    case 2U: park_copy = true; copy_parked = false; return 1;
    case 3U: probe_ready.wait(lock, [] { return copy_parked; }); return 1;
    case 4U: park_copy = false; probe_ready.notify_all(); return 1;
    case 5U: probe_ready.wait(lock, [] { return receive_waiting; }); return 1;
    case 6U: probe_ready.wait(lock, [] { return drain_waiting; }); return 1;
    case 7U: probe_ready.wait(lock, [] { return overlapping_callback; }); return 1;
    default: return 0;
    }
}
#endif
