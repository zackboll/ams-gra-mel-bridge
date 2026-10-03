/* By-value payloads need no provider graph or callback drain on logical stop. */
#include "internal/rf_job_interval_status.hpp"
#include "internal/provider_common.hpp"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
#include <cerrno>
#include <unistd.h>
namespace {
std::atomic<unsigned> hold_stage{};
std::atomic<int> hold_notify{-1}, hold_release{-1};
void test_hold(unsigned stage) noexcept
{
    unsigned expected = stage;
    if (!hold_stage.compare_exchange_strong(expected, 0)) return;
    char byte = 'h';
    ssize_t result;
    do { result = write(hold_notify.load(), &byte, 1); } while (result < 0 && errno == EINTR);
    do { result = read(hold_release.load(), &byte, 1); } while (result < 0 && errno == EINTR);
}
}
extern "C" __attribute__((visibility("default"))) void ams_mel_test_rf_status_hold(
    unsigned stage, int notify, int release) noexcept
{ hold_notify.store(notify); hold_release.store(release); hold_stage.store(stage); }
#endif

using namespace ams_mel::internal;
namespace rfmel = ams::iface::rfmel;
namespace {
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::None) == AMS_MEL_RF_INTERVAL_COMPLETION_NONE);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::ReadyForNextJobInterval) == AMS_MEL_RF_INTERVAL_COMPLETION_READY_FOR_NEXT_JOB_INTERVAL);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInterrupted) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INTERRUPTED);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidTxEventSpatialData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_TX_EVENT_SPATIAL_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidTxEventSignalData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_TX_EVENT_SIGNAL_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidTxEventTemporalData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_TX_EVENT_TEMPORAL_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidTxEventIdentifierData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_TX_EVENT_IDENTIFIER_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidRxEventSpatialData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_RX_EVENT_SPATIAL_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidRxEventSignalData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_RX_EVENT_SIGNAL_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidRxEventTemporalData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_RX_EVENT_TEMPORAL_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidRxEventIdentifierData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_RX_EVENT_IDENTIFIER_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidSequenceTemporalData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_SEQUENCE_TEMPORAL_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidJobIntervalSpatialData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_INTERVAL_SPATIAL_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidJobIntervalEventSignalData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_INTERVAL_EVENT_SIGNAL_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidJobIntervalEventTemporalData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_INTERVAL_EVENT_TEMPORAL_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidJobIntervalEventIdentifierData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_INTERVAL_EVENT_IDENTIFIER_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidJobTemporalData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_TEMPORAL_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidJobIdentifierData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_IDENTIFIER_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::Completed) == AMS_MEL_RF_INTERVAL_COMPLETION_COMPLETED);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::Cancelled) == AMS_MEL_RF_INTERVAL_COMPLETION_CANCELLED);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::lateControls) == AMS_MEL_RF_INTERVAL_COMPLETION_LATE_CONTROLS);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::invalidControls) == AMS_MEL_RF_INTERVAL_COMPLETION_INVALID_CONTROLS);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::antennaFovError) == AMS_MEL_RF_INTERVAL_COMPLETION_ANTENNA_FOV_ERROR);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::TransmitRfInhibited) == AMS_MEL_RF_INTERVAL_COMPLETION_TRANSMIT_RF_INHIBITED);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::Started) == AMS_MEL_RF_INTERVAL_COMPLETION_STARTED);
static_assert(static_cast<unsigned>(rfmel::JobEventLogTriggerType::None) == AMS_MEL_RF_LOG_TRIGGER_NONE);
static_assert(static_cast<unsigned>(rfmel::JobEventLogTriggerType::eventExtended) == AMS_MEL_RF_LOG_TRIGGER_EVENT_EXTENDED);
static_assert(static_cast<unsigned>(rfmel::JobEventLogTriggerType::eventTriggered) == AMS_MEL_RF_LOG_TRIGGER_EVENT_TRIGGERED);
static_assert(static_cast<unsigned>(rfmel::JobEventLogTriggerType::eventResumed) == AMS_MEL_RF_LOG_TRIGGER_EVENT_RESUMED);
static_assert(static_cast<unsigned>(rfmel::JobEventLogTriggerType::eventCancelled) == AMS_MEL_RF_LOG_TRIGGER_EVENT_CANCELLED);
static_assert(static_cast<unsigned>(rfmel::JobEventLogTriggerType::eventInhibited) == AMS_MEL_RF_LOG_TRIGGER_EVENT_INHIBITED);
static_assert(static_cast<unsigned>(rfmel::JobEventLogTriggerType::eventDelayedStart) == AMS_MEL_RF_LOG_TRIGGER_EVENT_DELAYED_START);
static_assert(static_cast<unsigned>(rfmel::JobEventLogTriggerType::eventTypeNotSupported) == AMS_MEL_RF_LOG_TRIGGER_EVENT_TYPE_NOT_SUPPORTED);
static_assert(static_cast<unsigned>(rfmel::JobIntervalStatusEnable::Never) == AMS_MEL_RF_INTERVAL_STATUS_NEVER);
static_assert(static_cast<unsigned>(rfmel::JobIntervalStatusEnable::Always) == AMS_MEL_RF_INTERVAL_STATUS_ALWAYS);
static_assert(static_cast<unsigned>(rfmel::JobIntervalStatusEnable::OnException) == AMS_MEL_RF_INTERVAL_STATUS_ON_EXCEPTION);
void increment(std::uint64_t& value) noexcept
{ if (value != UINT64_MAX) ++value; }
bool bad_diag(const char *data, std::size_t size) noexcept { return !data && size; }
bool failpoint(const char *name) noexcept
{
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    const auto *value = std::getenv("AMS_MEL_TEST_RF_STATUS_FAILURE");
    return value && std::strcmp(value, name) == 0;
#else
    (void)name; return false;
#endif
}
enum class Outcome { Valid, Malformed, Oversize, Allocation };
void receive_callback(IntervalStatusState& state, const rfmel::JobIntervalStatus& payload) noexcept
{
    try {
        {
            std::lock_guard lock{state.mutex};
            increment(state.counters.callback_entries);
            if (state.stopped) { increment(state.counters.callbacks_after_close); return; }
        }
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
        test_hold(1);
#endif
        Outcome outcome = Outcome::Valid;
        std::unique_ptr<ams_mel_rf_job_interval_status_event> event;
        const auto completion = static_cast<std::uint32_t>(payload.getJobIntervalCompletionStatus());
        const auto& log = payload.getJobEventLog();
        const auto& activity = payload.getActivityId();
        if (completion > AMS_MEL_RF_INTERVAL_COMPLETION_STARTED) outcome = Outcome::Malformed;
        else if (log.size() > state.options.max_event_log_entries ||
                 activity.size() > state.options.max_activity_id_bytes) outcome = Outcome::Oversize;
        else {
            for (const auto& [id, value] : log) {
                (void)id;
                if (static_cast<std::uint32_t>(value.getJobEventLogTrigger()) >
                    AMS_MEL_RF_LOG_TRIGGER_EVENT_TYPE_NOT_SUPPORTED) {
                    outcome = Outcome::Malformed; break;
                }
            }
        }
        if (outcome == Outcome::Valid) {
            try {
                if (failpoint("event-allocation")) throw std::bad_alloc{};
                event = std::make_unique<ams_mel_rf_job_interval_status_event>();
                event->logs.reserve(log.size());
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
                test_hold(2);
#endif
                for (const auto& [id, value] : log) {
                    /* Pinned time getter is non-const. Copy, never const_cast. */
                    auto mutable_value = value;
                    const auto& time = mutable_value.getJobEventLogTime();
                    event->logs.push_back({id,
                        static_cast<std::uint32_t>(value.getJobEventLogTrigger()),
                        time.getIntegralSeconds().count(), time.getFractionalFemtoseconds().count()});
                }
                if (failpoint("activity-allocation")) throw std::bad_alloc{};
                event->activity = activity;
                event->view = {payload.getJobIntervalID(), completion,
                    {event->logs.data(), event->logs.size()},
                    {event->activity.data(), event->activity.size()}};
            } catch (...) { outcome = Outcome::Allocation; event.reset(); }
        }
        std::lock_guard lock{state.mutex};
        /* Close wins even if the callback was already copying. */
        if (state.stopped) increment(state.counters.callbacks_after_close);
        else if (outcome == Outcome::Malformed) increment(state.counters.malformed_drops);
        else if (outcome == Outcome::Oversize) increment(state.counters.oversize_drops);
        else if (outcome == Outcome::Allocation) increment(state.counters.allocation_failures);
        else if (state.size == state.queue.size()) increment(state.counters.queue_full_drops);
        else {
            const auto tail = (state.head + state.size) % state.queue.size();
            state.queue[tail] = std::move(event);
            ++state.size;
            increment(state.counters.events_queued);
            state.ready.notify_all();
        }
    } catch (...) {
        /* Contains bridge-body exceptions, not provider by-value construction
         * before entry into this body. */
    }
}
}
namespace ams_mel::internal {
bool valid_interval_status_options(const ams_mel_rf_job_interval_status_options_v1& options) noexcept
{
    const auto maximum = std::numeric_limits<std::size_t>::max();
    const auto entry = sizeof(ams_mel_rf_job_event_log_entry_v1);
    if (!options.queue_capacity ||
        options.queue_capacity > std::vector<std::unique_ptr<ams_mel_rf_job_interval_status_event>>{}.max_size() ||
        options.queue_capacity > maximum / 2 ||
        options.max_event_log_entries > std::vector<ams_mel_rf_job_event_log_entry_v1>{}.max_size() ||
        options.max_activity_id_bytes > std::vector<std::uint8_t>{}.max_size() ||
        options.max_event_log_entries > maximum / entry) return false;
    const auto log_bytes = options.max_event_log_entries * entry;
    if (options.max_activity_id_bytes > maximum - log_bytes) return false;
    const auto payload = log_bytes + options.max_activity_id_bytes;
    const auto shell = sizeof(ams_mel_rf_job_interval_status_event) + sizeof(void *);
    return payload <= maximum - shell && options.queue_capacity <= maximum / (payload + shell);
}
std::unique_ptr<PermanentIntervalStatusRegistration> prepare_interval_status(
    const ams_mel_rf_job_interval_status_options_v1& options, std::shared_ptr<SharedLibrary> library)
{
    if (failpoint("pre-registration")) throw std::bad_alloc{};
    auto registration = std::make_unique<PermanentIntervalStatusRegistration>();
    registration->library = std::move(library);
    registration->state = std::make_shared<IntervalStatusState>();
    registration->state->options = options;
    registration->state->queue.resize(options.queue_capacity);
    auto *const state = registration->state.get();
    registration->callback = [state](rfmel::JobIntervalStatus payload) noexcept {
        receive_callback(*state, payload);
    };
    return registration;
}
void retain_interval_status(PermanentIntervalStatusRegistration *registration) noexcept
{
    static std::atomic<PermanentIntervalStatusRegistration *> root{};
    auto *head = root.load(std::memory_order_relaxed);
    do { registration->next = head; }
    while (!root.compare_exchange_weak(head, registration, std::memory_order_release,
                                      std::memory_order_relaxed));
}
void stop_interval_status(const std::shared_ptr<IntervalStatusState>& state) noexcept
{
    if (!state) return;
    try {
        std::lock_guard lock{state->mutex};
        state->stopped = true;
        for (auto& event : state->queue) event.reset();
        state->size = 0;
        state->ready.notify_all();
    } catch (...) {}
}
bool usable_interval_status(const std::shared_ptr<IntervalStatusState>& state)
{
    if (!state) return false;
    std::lock_guard lock{state->mutex};
    return !state->stopped;
}
}
extern "C" ams_mel_status_t ams_mel_rf_job_interval_status_receive(
    ams_mel_rf_job_interval_status *stream, std::uint32_t timeout_ms,
    ams_mel_rf_job_interval_status_event **output, char *diagnostic,
    std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!stream || !stream->state || !output || *output || bad_diag(diagnostic, capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        auto& state = *stream->state;
        std::unique_lock lock{state.mutex};
        if (!state.ready.wait_for(lock, std::chrono::milliseconds{timeout_ms},
                                 [&] { return state.stopped || state.size; })) return AMS_MEL_TIMEOUT;
        if (state.stopped) return AMS_MEL_STREAM_STOPPED;
        *output = state.queue[state.head].release();
        state.head = (state.head + 1) % state.queue.size();
        --state.size;
        increment(state.counters.events_delivered);
        return AMS_MEL_OK;
    } catch (...) { return AMS_MEL_INTERNAL_ERROR; }
}
extern "C" ams_mel_status_t ams_mel_rf_job_interval_status_get_counters(
    const ams_mel_rf_job_interval_status *stream,
    ams_mel_rf_job_interval_status_counters_v1 *output, char *diagnostic,
    std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!stream || !stream->state || !output || bad_diag(diagnostic, capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::lock_guard lock{stream->state->mutex};
        *output = stream->state->counters;
        return AMS_MEL_OK;
    } catch (...) { return AMS_MEL_INTERNAL_ERROR; }
}
extern "C" ams_mel_status_t ams_mel_rf_job_interval_status_close(
    ams_mel_rf_job_interval_status **stream, char *diagnostic,
    std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!stream || bad_diag(diagnostic, capacity)) return AMS_MEL_INVALID_ARGUMENT;
    auto *owned = std::exchange(*stream, nullptr);
    if (owned) { stop_interval_status(owned->state); delete owned; }
    return AMS_MEL_OK;
}
extern "C" ams_mel_status_t ams_mel_rf_job_interval_status_event_view(
    const ams_mel_rf_job_interval_status_event *event,
    const ams_mel_rf_job_interval_status_v1 **output, char *diagnostic,
    std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!event || !output || *output || bad_diag(diagnostic, capacity)) return AMS_MEL_INVALID_ARGUMENT;
    *output = &event->view;
    return AMS_MEL_OK;
}
extern "C" ams_mel_status_t ams_mel_rf_job_interval_status_event_close(
    ams_mel_rf_job_interval_status_event **event, char *diagnostic,
    std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!event || bad_diag(diagnostic, capacity)) return AMS_MEL_INVALID_ARGUMENT;
    delete std::exchange(*event, nullptr);
    return AMS_MEL_OK;
}