#include "internal/ir_scheduling_update_track_list.hpp"

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
using View = ams_mel_ir_update_track_list_response_v1;
using Event = ams_mel_ir_scheduling_update_track_list_event;
using Stream = ams_mel_ir_scheduling_update_track_list_metadata;

static_assert(std::is_same_v<decltype(std::declval<const UpdateTrackListResponse&>().getCommandID()),
                             std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<const UpdateTrackListResponse&>().getTrackListResponse()),
                             const TrackListResponse&>);
static_assert(std::is_same_v<std::underlying_type_t<TrackListResponse>, std::uint32_t>);
static_assert(static_cast<std::uint32_t>(TrackListResponse::None) == AMS_MEL_IR_TRACK_LIST_RESPONSE_NONE);
static_assert(static_cast<std::uint32_t>(TrackListResponse::Rejected) == AMS_MEL_IR_TRACK_LIST_RESPONSE_REJECTED);
static_assert(static_cast<std::uint32_t>(TrackListResponse::RejectedExpired) == AMS_MEL_IR_TRACK_LIST_RESPONSE_REJECTED_EXPIRED);
static_assert(static_cast<std::uint32_t>(TrackListResponse::Accepted) == AMS_MEL_IR_TRACK_LIST_RESPONSE_ACCEPTED);
static_assert(static_cast<std::uint32_t>(ChannelMetadataCapabilityType::UpdateTrackListResponse) ==
              AMS_MEL_IR_METADATA_UPDATE_TRACK_LIST_RESPONSE);
static_assert(AMS_MEL_IR_METADATA_UPDATE_TRACK_LIST_RESPONSE == 23U);
static_assert(std::is_standard_layout_v<View> && std::is_trivially_copyable_v<View>);
static_assert(std::is_same_v<decltype(View::command_id), std::uint32_t> && sizeof(View::command_id) == 4);
static_assert(std::is_same_v<decltype(View::response), ams_mel_ir_track_list_response_t> &&
              std::is_same_v<ams_mel_ir_track_list_response_t, std::uint32_t> && sizeof(View::response) == 4);
static_assert(sizeof(View) == 8 && alignof(View) == 4 &&
              offsetof(View, command_id) == 0 && offsetof(View, response) == 4);

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

bool valid(TrackListResponse response) noexcept
{
    switch (response) {
    case TrackListResponse::None:
    case TrackListResponse::Rejected:
    case TrackListResponse::RejectedExpired:
    case TrackListResponse::Accepted: return true;
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

struct ams_mel_ir_scheduling_update_track_list_event {
    View view;
};

struct UpdateTrackListState {
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

    void receive(const UpdateTrackListResponse *input) noexcept
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
                const auto command_id = input->getCommandID();
                const auto response = input->getTrackListResponse(); // copy, not a retained reference
                well_formed = valid(response);
                if (well_formed && slot) {
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
                    if (fail_next_snapshot.exchange(false)) throw std::bad_alloc{};
#endif
                    event = std::make_unique<Event>(View{command_id, static_cast<std::uint32_t>(response)});
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

struct ams_mel_ir_scheduling_update_track_list_metadata {
    std::shared_ptr<UpdateTrackListState> state;
};

namespace ams_mel::internal {
void scheduling_update_track_list_stopped(const std::shared_ptr<UpdateTrackListState>& state) noexcept
{ if (state) state->transition(Lifecycle::Stopped); }

void scheduling_update_track_list_failed(const std::shared_ptr<UpdateTrackListState>& state) noexcept
{ if (state) state->transition(Lifecycle::Failed); }

void scheduling_update_track_list_drained(const std::shared_ptr<UpdateTrackListState>& state) noexcept
{
    if (!state) return;
    scheduling_update_track_list_stopped(state);
    std::unique_lock lock{state->mutex};
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    if (state->reading) observe(drain_waiting);
#endif
    state->copies_done.wait(lock, [&] { return !state->reading; });
}
} // namespace ams_mel::internal

extern "C" ams_mel_status_t ams_mel_ir_scheduling_update_track_list_open(
    ams_mel_ir_scheduling *scheduling, std::size_t queue_capacity, Stream **output,
    ams_mel_ir_return_t *registration, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!scheduling || !queue_capacity || !output || *output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    std::unique_ptr<Stream> owner;
    std::function<void(Channel&, const UpdateTrackListResponse *const)> callback;
    try {
        owner = std::make_unique<Stream>();
        owner->state = std::make_shared<UpdateTrackListState>();
        owner->state->capacity = queue_capacity;
        callback = [state = owner->state](Channel&, const UpdateTrackListResponse *value) noexcept {
            const auto keep = state;
            keep->receive(value);
        };
    } catch (...) {
        diagnostic("could not allocate UpdateTrackListResponse stream", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    std::shared_ptr<void> graph; // outlives the local provider channel reference
    std::shared_ptr<SchedulingChannel> channel;
    if (!ams_mel::internal::claim_scheduling_update_track_list(
            *scheduling, owner->state, channel, graph)) {
        diagnostic("Scheduling owner is ineligible or UpdateTrackListResponse already attempted",
                   out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    Return result{};
    try {
        result = channel->registerMetadataCallback(std::move(callback));
    } catch (...) {
        owner->state->transition(Lifecycle::Inactive);
        diagnostic("provider exception during UpdateTrackListResponse registration", out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
    ams_mel_ir_return_t mapped{};
    const bool known = map_return(result, mapped);
    if (known && registration) *registration = mapped;
    if (!known || mapped != AMS_MEL_IR_RETURN_SUCCESS) {
        owner->state->transition(Lifecycle::Inactive);
        diagnostic(known ? "provider refused UpdateTrackListResponse registration" :
                           "unknown UpdateTrackListResponse registration result", out, capacity, required);
        return AMS_MEL_PROVIDER_FAILED;
    }
    *output = owner.release();
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_scheduling_update_track_list_receive(
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
            diagnostic("Scheduling cleanup failed; UpdateTrackListResponse stream failed", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        return state.lifecycle == Lifecycle::Active ? AMS_MEL_TIMEOUT : AMS_MEL_STREAM_STOPPED;
    } catch (...) {
        diagnostic("UpdateTrackListResponse receive failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_scheduling_update_track_list_get_counters(
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

extern "C" ams_mel_status_t ams_mel_ir_scheduling_update_track_list_close(
    Stream **metadata, char *out, std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!metadata || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    std::unique_ptr<Stream> owner{std::exchange(*metadata, nullptr)};
    if (owner) owner->state->transition(Lifecycle::Inactive);
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_scheduling_update_track_list_event_view(
    const Event *event, const View **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!event || !output || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    *output = &event->view;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_scheduling_update_track_list_event_close(
    Event **event, char *out, std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!event || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    delete std::exchange(*event, nullptr);
    return AMS_MEL_OK;
}

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
extern "C" __attribute__((visibility("default"))) int ams_mel_test_update_track_list_probe(
    Stream *stream, unsigned operation) noexcept
{
    if (operation == 0U) { fail_next_snapshot = true; return 1; }
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
