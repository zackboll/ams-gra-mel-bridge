#include "internal/ir_scheduling_schedule_created.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <functional>
#include <limits>
#include <mutex>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
using namespace ams::iface::irmel;
using Activity = ams_mel_ir_activity_record_v1;
using View = ams_mel_ir_sched_created_rep_v1;
using Activities = std::vector<Activity>;
using Duration = std::chrono::nanoseconds;

static_assert(std::is_same_v<decltype(std::declval<const SchedCreatedRep&>().getActivity()),
                             const std::vector<ActivityRecord>&>);
static_assert(std::is_same_v<decltype(std::declval<const SchedCreatedRep&>().getCommandID()),
                             std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<const SchedCreatedRep&>().getScanId()),
                             std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<const SchedCreatedRep&>().getActivityCount()),
                             std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<const SchedCreatedRep&>().getEstDuration()),
                             Duration>);
static_assert(std::is_same_v<decltype(std::declval<const ActivityRecord&>().getActivityId()),
                             std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<const ActivityRecord&>().getTrackId()),
                             std::int32_t>);
static_assert(std::is_same_v<decltype(std::declval<const ActivityRecord&>().getTaskId()),
                             const Task&>);
static_assert(std::is_same_v<decltype(std::declval<const ActivityRecord&>().getDuration()),
                             Duration>);
static_assert(std::numeric_limits<Duration::rep>::digits == 63 &&
              std::numeric_limits<Duration::rep>::is_signed);
static_assert(std::is_same_v<std::underlying_type_t<Task>, ams_mel_ir_task_t>);
static_assert(static_cast<ams_mel_ir_task_t>(Task::Reserved6) == AMS_MEL_IR_TASK_RESERVED_6);
static_assert(static_cast<ams_mel_ir_task_t>(Task::NumTaskTypes) == 17U);
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);

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
    if (value < std::numeric_limits<std::uint64_t>::max()) ++value;
}

enum class Lifecycle { Active, Inactive, Stopped, Failed };

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
std::atomic<bool> fail_next_copy{};
std::mutex probe_mutex;
std::condition_variable probe_ready;
bool park_copy{}, copy_parked{}, receive_waiting{}, drain_waiting{};

void observe(bool& flag)
{
    std::lock_guard lock{probe_mutex};
    flag = true;
    probe_ready.notify_all();
}
#endif
} // namespace

/* The object (not its vector buffer) moves between owners via unique_ptr.
 * Once published, neither the buffer nor the view is modified. */
struct ams_mel_ir_scheduling_schedule_created_event {
    View view{};
    Activities activities;
};

namespace {
using Event = ams_mel_ir_scheduling_schedule_created_event;

bool valid(const std::vector<ActivityRecord>& activities)
{
    const auto size = activities.size();
    /* max_size also accounts for the vector implementation's difference type.
     * The event and vector are allocated separately; no header-size addition. */
    return size <= std::numeric_limits<std::size_t>::max() / sizeof(Activity) &&
           size <= Activities{}.max_size() &&
           std::all_of(activities.begin(), activities.end(), [](const auto& activity) {
               return static_cast<ams_mel_ir_task_t>(activity.getTaskId()) <=
                      AMS_MEL_IR_TASK_RESERVED_6;
           });
}

std::unique_ptr<Event> snapshot(const SchedCreatedRep& input,
                                const std::vector<ActivityRecord>& activities)
{
    auto event = std::make_unique<Event>();
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    {
        std::unique_lock lock{probe_mutex};
        if (park_copy && !copy_parked) {
            copy_parked = true;
            probe_ready.notify_all();
            probe_ready.wait(lock, [] { return !park_copy; });
        }
    }
    if (fail_next_copy.exchange(false)) throw std::bad_alloc{};
#endif
    event->activities.resize(activities.size());
    for (std::size_t i = 0; i < activities.size(); ++i) {
        const auto& source = activities[i];
        auto& target = event->activities[i];
        target.activity_id = source.getActivityId();
        target.task_id = static_cast<ams_mel_ir_task_t>(source.getTaskId());
        target.track_id = source.getTrackId();
        const auto position = source.getPosition();
        std::memcpy(&target.position.range_m, &position.range, sizeof(double));
        std::memcpy(&target.position.azimuth_rad, &position.az, sizeof(double));
        std::memcpy(&target.position.elevation_rad, &position.el, sizeof(double));
        std::memcpy(&target.position.range_rate_mps, &position.rangerate, sizeof(double));
        target.duration_ns = source.getDuration().count();
    }
    event->view = {input.getCommandID(), input.getScanId(), input.getEstDuration().count(),
                   input.getActivityCount(),
                   {activities.empty() ? nullptr : event->activities.data(), activities.size()}};
    return event;
}
} // namespace

struct ScheduleCreatedState {
    // This leaf mutex is never held while executing provider code or copying payloads.
    std::mutex mutex;
    std::condition_variable ready, copies_done;
    std::deque<std::unique_ptr<Event>> queue;
    std::size_t capacity{}, readers{}, reserved{};
    ams_mel_ir_metadata_counters_v1 counters{};
    Lifecycle lifecycle{Lifecycle::Active};

    void receive(const SchedCreatedRep *input) noexcept
    {
        bool admitted = false, slot = false;
        try {
            {
                std::lock_guard lock{mutex};
                increment(counters.events_received);
                if (lifecycle != Lifecycle::Active) return;
                ++readers;
                admitted = true;
                // Reserve at arrival: concurrent callbacks cannot allocate
                // snapshots beyond the configured event capacity.
                slot = reserved < capacity - queue.size();
                if (slot) ++reserved;
            }
            bool well_formed = false;
            std::unique_ptr<Event> event;
            if (input) {
                const auto& activities = input->getActivity();
                well_formed = valid(activities);
                if (well_formed && slot) event = snapshot(*input, activities);
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
            finish(slot);
        } catch (...) {
            // Includes failure to allocate the event, activity buffer or queue node.
            if (admitted) {
                try {
                    std::lock_guard lock{mutex};
                    if (lifecycle == Lifecycle::Active)
                        increment(counters.malformed_or_unsupported);
                    finish(slot);
                } catch (...) {}
            }
        }
    }

    void finish(bool slot) noexcept // mutex held; no more provider payload access
    {
        if (slot) --reserved;
        if (--readers == 0U) copies_done.notify_all();
    }

    void transition(Lifecycle next) noexcept
    {
        try {
            std::lock_guard lock{mutex};
            if (lifecycle == Lifecycle::Active ||
                (lifecycle == Lifecycle::Stopped && next == Lifecycle::Failed))
                lifecycle = next;
            if (next == Lifecycle::Inactive) queue.clear();
            ready.notify_all();
        } catch (...) {}
    }
};

struct ams_mel_ir_scheduling_schedule_created_metadata {
    std::shared_ptr<ScheduleCreatedState> state;
};

namespace ams_mel::internal {
void scheduling_schedule_created_stopped(const std::shared_ptr<ScheduleCreatedState>& state) noexcept
{ if (state) state->transition(Lifecycle::Stopped); }

void scheduling_schedule_created_failed(const std::shared_ptr<ScheduleCreatedState>& state) noexcept
{ if (state) state->transition(Lifecycle::Failed); }

void scheduling_schedule_created_drained(const std::shared_ptr<ScheduleCreatedState>& state) noexcept
{
    if (!state) return;
    scheduling_schedule_created_stopped(state);
    std::unique_lock lock{state->mutex};
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    if (state->readers) observe(drain_waiting);
#endif
    state->copies_done.wait(lock, [&] { return state->readers == 0U; });
}
} // namespace ams_mel::internal

extern "C" ams_mel_status_t ams_mel_ir_scheduling_schedule_created_open(
    ams_mel_ir_scheduling *scheduling, std::size_t queue_capacity,
    ams_mel_ir_scheduling_schedule_created_metadata **output,
    ams_mel_ir_return_t *registration, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!scheduling || !queue_capacity || !output || *output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    std::unique_ptr<ams_mel_ir_scheduling_schedule_created_metadata> owner;
    std::function<void(Channel&, const SchedCreatedRep *const)> callback;
    try {
        owner = std::make_unique<ams_mel_ir_scheduling_schedule_created_metadata>();
        owner->state = std::make_shared<ScheduleCreatedState>();
        owner->state->capacity = queue_capacity;
        callback = [state = owner->state](Channel&, const SchedCreatedRep *value) noexcept {
            const auto keep = state;
            keep->receive(value);
        };
    } catch (...) {
        diagnostic("could not allocate SchedCreatedRep stream", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    std::shared_ptr<void> graph; // destroyed after the provider channel reference
    std::shared_ptr<SchedulingChannel> channel;
    if (!ams_mel::internal::claim_scheduling_schedule_created(
            *scheduling, owner->state, channel, graph)) {
        diagnostic("Scheduling owner is ineligible or SchedCreatedRep already attempted", out,
                   capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    Return result{};
    try {
        result = channel->registerMetadataCallback(std::move(callback));
    } catch (...) {
        owner->state->transition(Lifecycle::Inactive);
        diagnostic("provider exception during SchedCreatedRep registration", out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
    ams_mel_ir_return_t mapped{};
    const bool known = map_return(result, mapped);
    if (known && registration) *registration = mapped;
    if (!known || mapped != AMS_MEL_IR_RETURN_SUCCESS) {
        owner->state->transition(Lifecycle::Inactive);
        diagnostic(known ? "provider refused SchedCreatedRep registration" :
                           "unknown SchedCreatedRep registration result", out, capacity, required);
        return AMS_MEL_PROVIDER_FAILED;
    }
    *output = owner.release();
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_scheduling_schedule_created_receive(
    ams_mel_ir_scheduling_schedule_created_metadata *metadata, std::uint32_t timeout_ms,
    Event **output, char *out, std::size_t capacity, std::size_t *required) noexcept
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
            diagnostic("Scheduling cleanup failed; SchedCreatedRep stream failed", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        return state.lifecycle == Lifecycle::Active ? AMS_MEL_TIMEOUT : AMS_MEL_STREAM_STOPPED;
    } catch (...) {
        diagnostic("SchedCreatedRep receive failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_scheduling_schedule_created_get_counters(
    const ams_mel_ir_scheduling_schedule_created_metadata *metadata,
    ams_mel_ir_metadata_counters_v1 *output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
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

extern "C" ams_mel_status_t ams_mel_ir_scheduling_schedule_created_close(
    ams_mel_ir_scheduling_schedule_created_metadata **metadata, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!metadata || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    auto *owner = std::exchange(*metadata, nullptr);
    if (owner) owner->state->transition(Lifecycle::Inactive);
    delete owner;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_scheduling_schedule_created_event_view(
    const Event *event, const View **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!event || !output || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    *output = &event->view;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_scheduling_schedule_created_event_close(
    Event **event, char *out, std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!event || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    delete std::exchange(*event, nullptr);
    return AMS_MEL_OK;
}

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
extern "C" __attribute__((visibility("default"))) int ams_mel_test_schedule_created_probe(
    ams_mel_ir_scheduling_schedule_created_metadata *stream, unsigned operation) noexcept
{
    if (operation == 0U) { fail_next_copy = true; return 1; }
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
    default: return 0;
    }
}
#endif
