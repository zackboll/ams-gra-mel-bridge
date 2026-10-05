/* Ordered provider-created RX/TX commands and one C2 child claim per async Job. */
#include <ams_mel/abi.h>
#include "internal/provider_common.hpp"
#include "internal/rf_va_job_parent.hpp"
#include "internal/rf_job_interval_status.hpp"
#include <rfmel/c2/ElementGroupCommand.h>
#include <rfmel/c2/CancelStatus.h>
#include <rfmel/c2/JobDetail.h>
#include <rfmel/c2/JobRequest.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace mel = ams::iface::mel;
namespace rfmel = ams::iface::rfmel;
using namespace ams_mel::internal;
namespace {
using Future = mel::RequestFor<rfmel::JobDetail>;
enum class Kind { Pending, Succeeded, ProviderFailed, ProviderException, InternalError };
struct Completion {
    std::mutex mutex;
    std::condition_variable changed;
    Kind kind{Kind::Pending};
    ams_mel_error_code_t code{AMS_MEL_ERROR_NONE};
    std::string message;
    std::shared_ptr<rfmel::JobDetail> job;
    std::shared_ptr<rfmel::VirtualAperture> va;
    RfC2ChildClaim claim;
    bool claimed{};
    bool abandoned{};
    bool claim_failed{};
    ams_mel_status_t claim_status{AMS_MEL_OK};
    std::string claim_message;
    std::shared_ptr<Completion> emergency_self;
};
struct WorkerInput {
    std::shared_ptr<Completion> completion;
    Future future;
    std::shared_ptr<WorkerInput> emergency_self;
    WorkerInput *next{};
    std::atomic<bool> retained{};
    std::atomic<unsigned> launch_state{};
};
void retain(const std::shared_ptr<WorkerInput>& input) noexcept
{
    static std::atomic<WorkerInput *> root{};
    bool expected = false;
    if (!input->retained.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return;
    input->emergency_self = input;
    WorkerInput *head = root.load(std::memory_order_relaxed);
    do { input->next = head; }
    while (!root.compare_exchange_weak(head, input.get(), std::memory_order_release,
                                       std::memory_order_relaxed));
}
bool bad_diag(const char *out, std::size_t capacity) noexcept { return !out && capacity != 0U; }
bool span_ok(std::size_t size, const void *data, std::size_t element) noexcept
{ return (!size || data) && size <= std::numeric_limits<std::size_t>::max() / element; }
bool valid_view(ams_mel_string_view_v1 view) noexcept
{ return (view.data || !view.size) && view.size < std::numeric_limits<std::size_t>::max() &&
         valid_utf8(view.data ? std::string_view{view.data, view.size} : std::string_view{}); }
std::string copy_view(ams_mel_string_view_v1 view)
{ return view.data ? std::string{view.data, view.size} : std::string{}; }
bool failpoint(const char *name) noexcept
{
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    const char *value = std::getenv("AMS_MEL_TEST_RF_JOB_FAILURE");
    return value && std::strcmp(value, name) == 0;
#else
    (void)name;
    return false;
#endif
}
ams_mel_status_t status(Kind kind) noexcept
{
    switch (kind) {
    case Kind::Pending: return AMS_MEL_TIMEOUT;
    case Kind::Succeeded: return AMS_MEL_OK;
    case Kind::ProviderFailed: return AMS_MEL_PROVIDER_FAILED;
    case Kind::ProviderException: return AMS_MEL_PROVIDER_EXCEPTION;
    case Kind::InternalError: return AMS_MEL_INTERNAL_ERROR;
    }
    return AMS_MEL_INTERNAL_ERROR;
}
bool map_error(mel::ErrorCode value, ams_mel_error_code_t& code) noexcept
{
    switch (value) {
    case mel::ErrorCode::None: code = AMS_MEL_ERROR_NONE; return true;
    case mel::ErrorCode::InvalidId: code = AMS_MEL_ERROR_INVALID_ID; return true;
    case mel::ErrorCode::InvalidState: code = AMS_MEL_ERROR_INVALID_STATE; return true;
    case mel::ErrorCode::InvalidParameters: code = AMS_MEL_ERROR_INVALID_PARAMETERS; return true;
    case mel::ErrorCode::InsufficientPermissions: code = AMS_MEL_ERROR_INSUFFICIENT_PERMISSIONS; return true;
    case mel::ErrorCode::InsufficientResources: code = AMS_MEL_ERROR_INSUFFICIENT_RESOURCES; return true;
    case mel::ErrorCode::InsufficientLocalResources: code = AMS_MEL_ERROR_INSUFFICIENT_LOCAL_RESOURCES; return true;
    case mel::ErrorCode::InsufficientRemoteResources: code = AMS_MEL_ERROR_INSUFFICIENT_REMOTE_RESOURCES; return true;
    case mel::ErrorCode::Unsupported: code = AMS_MEL_ERROR_UNSUPPORTED; return true;
    }
    return false;
}
std::string exception_text(std::string_view fallback, std::string_view unknown)
{
    try { throw; }
    catch (const std::exception& error) {
        const char *what = error.what();
        return std::string{what && valid_utf8(what) ? std::string_view{what} : fallback};
    } catch (...) { return std::string{unknown}; }
}
struct Outcome {
    Kind kind{Kind::InternalError};
    ams_mel_error_code_t code{AMS_MEL_ERROR_NONE};
    std::string message;
    std::shared_ptr<rfmel::JobDetail> job;
};
/* The sole production future.get() for RF Job requests. */
void settle(WorkerInput& input, Outcome& output) noexcept
{
    std::optional<mel::ErrorOr<std::shared_ptr<rfmel::JobDetail>>> result;
    try { result.emplace(input.future.get()); }
    catch (const std::bad_alloc&) { return; }
    catch (...) {
        output.kind = Kind::ProviderException;
        try { output.message = exception_text("provider Job future exception", "unknown provider Job future exception"); }
        catch (...) {}
        return;
    }
    try {
        if (!*result) {
            const auto& error = result->getError();
            output.kind = Kind::ProviderFailed;
            if (!map_error(error.getCode(), output.code)) output.message = "unknown MEL error code";
            else {
                const auto& text = error.getDescription();
                output.message = valid_utf8(text) ? text : "invalid provider Job error description";
            }
        } else {
            output.job = std::move(result->get());
            if (!output.job) {
                output.kind = Kind::ProviderFailed;
                output.message = "null successful JobDetail";
            } else output.kind = Kind::Succeeded;
        }
    } catch (...) {
        output.kind = Kind::InternalError;
        output.job.reset();
        output.message.clear();
    }
}
void run_worker(std::shared_ptr<WorkerInput> input) noexcept
{
    unsigned launch = input->launch_state.load(std::memory_order_acquire);
    while (launch == 0U) {
        std::this_thread::yield();
        launch = input->launch_state.load(std::memory_order_acquire);
    }
    if (launch == 2U) return;
    try {
        Outcome outcome;
        settle(*input, outcome);
        input->future = Future{};
        auto& completion = *input->completion;
        std::shared_ptr<rfmel::VirtualAperture> released_va;
        RfC2ChildClaim released_claim;
        if (outcome.kind != Kind::Succeeded) {
            std::lock_guard lock{completion.mutex};
            released_va = std::move(completion.va);
            released_claim = std::move(completion.claim);
        }
        released_va.reset();
        const auto shutdown = released_claim.release();
        if (shutdown.status != AMS_MEL_OK) {
            outcome.kind = shutdown.status == AMS_MEL_INTERNAL_ERROR ? Kind::InternalError : Kind::ProviderException;
            outcome.message = shutdown.message;
        }
        std::shared_ptr<rfmel::JobDetail> abandoned_job;
        {
            std::unique_lock lock{completion.mutex};
            completion.kind = outcome.kind;
            completion.code = outcome.code;
            completion.message.swap(outcome.message);
            if (outcome.kind == Kind::Succeeded) completion.job = std::move(outcome.job);
            completion.changed.notify_all();
            if (outcome.kind == Kind::Succeeded) {
                completion.changed.wait(lock, [&] {
                    return completion.claimed || completion.abandoned || completion.claim_failed;
                });
                if (!completion.claimed && !completion.claim_failed) {
                    abandoned_job = std::move(completion.job);
                    released_va = std::move(completion.va);
                    released_claim = std::move(completion.claim);
                }
            }
        }
        abandoned_job.reset();
        released_va.reset();
        (void)released_claim.release();
    } catch (...) { retain(input); }
}
} // namespace

struct ams_mel_rf_job_request { std::shared_ptr<Completion> completion; };
namespace {
static_assert(static_cast<int>(rfmel::JobStatus::None) == AMS_MEL_RF_JOB_STATUS_NONE);
static_assert(static_cast<int>(rfmel::JobStatus::InProgress) == AMS_MEL_RF_JOB_STATUS_IN_PROGRESS);
static_assert(static_cast<int>(rfmel::JobStatus::Complete) == AMS_MEL_RF_JOB_STATUS_COMPLETE);
static_assert(static_cast<int>(rfmel::JobStatus::FailedInvalidID) == AMS_MEL_RF_JOB_STATUS_FAILED_INVALID_ID);
static_assert(static_cast<int>(rfmel::JobStatus::FailedInterrupted) == AMS_MEL_RF_JOB_STATUS_FAILED_INTERRUPTED);
static_assert(static_cast<int>(rfmel::JobStatus::FailedInvalidState) == AMS_MEL_RF_JOB_STATUS_FAILED_INVALID_STATE);
static_assert(static_cast<int>(rfmel::CancelError::None) == AMS_MEL_RF_CANCEL_ERROR_NONE);

struct JobState {
    std::shared_ptr<IntervalStatusState> interval_status;
    bool status_registration_attempted{};
    bool status_registration_returned{};
    std::shared_ptr<rfmel::JobDetail> detail;
    std::shared_ptr<rfmel::VirtualAperture> va;
    RfC2ChildClaim claim;
    std::vector<std::uint32_t> streams;
    ams_mel_rf_job_info_v1 info{};
    std::mutex mutex;
    std::condition_variable changed;
    bool finalize_attempted{};
    bool finalize_running{};
    ams_mel_status_t finalize_result{AMS_MEL_OK};
    std::string finalize_message;
    bool terminal{};
    ams_mel_status_t terminal_result{AMS_MEL_OK};
    ams_mel_rf_job_status_t published_status{};
    std::string terminal_message;
    bool cancel_attempted{};
    bool cancel_running{};
    ams_mel_status_t cancel_result{AMS_MEL_OK};
    ams_mel_rf_job_cancel_result_v1 cancelled{};
    std::string cancel_message;
    /* Explicit Close releases the claim itself so a deferred shutdown error
     * can be returned to that caller. Worker-only cleanup uses this fallback. */
    ~JobState() { stop_interval_status(interval_status); detail.reset(); va.reset(); (void)claim.release(); }
};
struct FinalizeInput {
    std::shared_ptr<JobState> state;
    std::future<rfmel::JobStatus> future;
    std::shared_ptr<FinalizeInput> emergency_self;
    FinalizeInput *next{};
    std::atomic<bool> retained{};
    std::atomic<unsigned> launch_state{};
};
void retain_finalize(const std::shared_ptr<FinalizeInput>& input) noexcept
{
    static std::atomic<FinalizeInput *> root{};
    bool expected = false;
    if (!input->retained.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return;
    input->emergency_self = input;
    auto *head = root.load(std::memory_order_relaxed);
    do { input->next = head; }
    while (!root.compare_exchange_weak(head, input.get(), std::memory_order_release,
                                       std::memory_order_relaxed));
}
void finalize_worker(std::shared_ptr<FinalizeInput> input) noexcept
{
    unsigned launch = input->launch_state.load(std::memory_order_acquire);
    while (launch == 0U) {
        std::this_thread::yield();
        launch = input->launch_state.load(std::memory_order_acquire);
    }
    if (launch == 2U) return;
    ams_mel_status_t result = AMS_MEL_OK;
    ams_mel_rf_job_status_t value{};
    std::string message;
    try {
        /* Sole production Job-finalize future.get(): never on the C caller. */
        const auto published = input->future.get();
        if (published != rfmel::JobStatus::None && published != rfmel::JobStatus::InProgress &&
            published != rfmel::JobStatus::Complete && published != rfmel::JobStatus::FailedInvalidID &&
            published != rfmel::JobStatus::FailedInterrupted &&
            published != rfmel::JobStatus::FailedInvalidState) {
            result = AMS_MEL_PROVIDER_FAILED;
            message = "unknown provider JobStatus";
        } else value = static_cast<ams_mel_rf_job_status_t>(published);
    } catch (const std::bad_alloc&) {
        result = AMS_MEL_INTERNAL_ERROR;
        try { message = "Job finalize completion allocation failed"; } catch (...) {}
    } catch (...) {
        result = AMS_MEL_PROVIDER_EXCEPTION;
        try { message = exception_text("provider Job finalize future exception",
                                       "unknown provider Job finalize future exception"); } catch (...) {}
    }
    /* Release the consumed provider future before publishing terminal status.
     * Do not destroy provider objects under a live future. */
    input->future = {};
    try {
        std::lock_guard lock{input->state->mutex};
        input->state->terminal_result = result;
        input->state->published_status = value;
        input->state->terminal_message.swap(message);
        input->state->terminal = true;
        input->state->changed.notify_all();
    } catch (...) { retain_finalize(input); }
}
} // namespace
struct ams_mel_rf_job {
    std::shared_ptr<JobState> state;
};

extern "C" ams_mel_status_t ams_mel_rf_job_interval_status_open(
    ams_mel_rf_job *job, const ams_mel_rf_job_interval_status_options_v1 *options,
    ams_mel_rf_job_interval_status **output, char *diagnostic,
    std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!job || !job->state || !job->state->detail || !options || !output || *output ||
        bad_diag(diagnostic, capacity) || !valid_interval_status_options(*options))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        const auto state = job->state;
        std::unique_lock lock{state->mutex};
        if (state->finalize_attempted || state->cancel_attempted || state->status_registration_attempted) {
            write_diagnostic("Job status registration unavailable or already attempted", diagnostic, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        auto owner = std::make_unique<ams_mel_rf_job_interval_status>();
        auto library = state->claim.library_pin();
        if (!library) return AMS_MEL_PROVIDER_FAILED;
        auto registration = prepare_interval_status(*options, std::move(library));
        owner->state = registration->state;
        state->interval_status = registration->state;
        state->status_registration_attempted = true;
        auto *permanent = registration.release();
        /* Retain before exposure: also safe for synchronous/reentrant callbacks. */
        retain_interval_status(permanent);
        lock.unlock();
        try { state->detail->registerJobIntervalStatusCallback(permanent->callback); }
        catch (...) {
            stop_interval_status(permanent->state);
            return translate_provider_exception("provider Job status registration exception",
                "unknown provider Job status registration exception", diagnostic, capacity, required);
        }
        lock.lock();
        state->status_registration_returned = true;
        *output = owner.release();
        return AMS_MEL_OK;
    } catch (...) {
        write_diagnostic("Job status registration preparation failed", diagnostic, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

namespace {
using Fs = ams::util::math::Femtoseconds;
static_assert(!std::numeric_limits<Fs>::is_specialized,
              "Review the pinned duration-sentinel contract");
static_assert(std::numeric_limits<Fs>::max().count() == INT64_C(0),
              "Unexpected numeric_limits<Femtoseconds> behavior");
static_assert(rfmel::JobInterval::ContinueFromPrevious.count() == INT64_C(0),
              "Pinned RF MEL ContinueFromPrevious changed; review compatibility");
static_assert(rfmel::JobInterval::ContinueFromPrevious.count() ==
              AMS_MEL_RF_JOB_INTERVAL_CONTINUE_FROM_PREVIOUS_FS,
              "C continuation constant must exactly match pinned RF MEL");
bool fits_count(std::uint64_t value) noexcept
{ return value <= std::numeric_limits<std::size_t>::max(); }
template<class Operation>
ams_mel_status_t interval_command(ams_mel_rf_job *job, bool require_unfinalized,
    Operation operation, char *diagnostic, std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!job || !job->state || !job->state->detail || bad_diag(diagnostic, capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        const auto state = job->state;
        {
            std::lock_guard lock{state->mutex};
            if (state->cancel_attempted || (require_unfinalized && state->finalize_attempted)) {
                write_diagnostic(state->cancel_attempted ? "full Job Cancel already attempted" :
                                 "Job Finalize already attempted", diagnostic, capacity, required);
                return AMS_MEL_PROVIDER_FAILED;
            }
        }
        operation(*state->detail); // Provider runs unlocked, including during future publication.
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        write_diagnostic("Job interval allocation failed", diagnostic, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (const std::exception& error) {
        write_exception_diagnostic(error, "provider Job interval exception", diagnostic, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    } catch (...) {
        write_diagnostic("unknown provider Job interval exception", diagnostic, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}
template<class Span>
ams_mel_status_t add_rx_intervals(
    ams_mel_rf_job *job, Span inputs,
    char *diagnostic, std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!job || !job->state || !job->state->detail || bad_diag(diagnostic, capacity) ||
        !span_ok(inputs.size, inputs.data, sizeof(*inputs.data))) return AMS_MEL_INVALID_ARGUMENT;
    const auto input_at = [&](std::size_t i) -> const ams_mel_rf_job_interval_config_v1& {
        if constexpr (std::is_same_v<Span, ams_mel_rf_job_interval_config_span_v1>) return inputs.data[i];
        else return inputs.data[i].interval;
    };
    const auto mode_at = [&](std::size_t i) -> std::uint32_t {
        if constexpr (std::is_same_v<Span, ams_mel_rf_job_interval_config_span_v1>) {
            (void)i; return AMS_MEL_RF_INTERVAL_STATUS_NEVER;
        } else return inputs.data[i].status_enable;
    };
    bool enabled = false;
    for (std::size_t i = 0; i < inputs.size; ++i) {
        const auto& input = input_at(i);
        if (mode_at(i) > AMS_MEL_RF_INTERVAL_STATUS_ON_EXCEPTION) return AMS_MEL_INVALID_ARGUMENT;
        enabled = enabled || mode_at(i) != AMS_MEL_RF_INTERVAL_STATUS_NEVER;
        if (input.phase_coherence_with_prior > 1U || !fits_count(input.sequence_repeat_count) ||
            !fits_count(input.iterations_per_signal) ||
            !span_ok(input.receive_events.size, input.receive_events.data, sizeof(*input.receive_events.data)))
            return AMS_MEL_INVALID_ARGUMENT;
        for (std::size_t e = 0; e < input.receive_events.size; ++e) {
            const auto& event = input.receive_events.data[e];
            if (!valid_view(event.element_group_label) || !fits_count(event.agc_processing_iterations) ||
                !fits_count(event.ignored_post_agc_iterations)) return AMS_MEL_INVALID_ARGUMENT;
        }
    }
    try {
        if (enabled) {
            std::lock_guard lock{job->state->mutex};
            if (!job->state->status_registration_returned ||
                !usable_interval_status(job->state->interval_status)) {
                write_diagnostic("status-enabled intervals require usable bridge registration", diagnostic, capacity, required);
                return AMS_MEL_PROVIDER_FAILED;
            }
        }
        std::vector<rfmel::JobInterval> intervals;
        intervals.reserve(inputs.size);
        for (std::size_t i = 0; i < inputs.size; ++i) {
            const auto& input = input_at(i);
            std::vector<rfmel::ReceiveEvent> events;
            events.reserve(input.receive_events.size);
            for (std::size_t e = 0; e < input.receive_events.size; ++e) {
                const auto& config = input.receive_events.data[e];
                rfmel::ReceiveEvent event;
                event.setEventID(config.event_id);
                event.setElementGroupLabel(copy_view(config.element_group_label));
                event.setStart(Fs{config.start_femtoseconds});
                event.setDuration(Fs{config.duration_femtoseconds});
                event.setCenterFrequency(config.center_frequency_hz);
                event.setSampleFrequency(config.sample_frequency_hz);
                event.setNumIterationProcessingAGC(static_cast<std::size_t>(config.agc_processing_iterations));
                event.setNumIterationIgnoredPostAGC(static_cast<std::size_t>(config.ignored_post_agc_iterations));
                event.setMaxExtensionDuration(Fs{config.max_extension_femtoseconds});
                events.push_back(std::move(event));
            }
            rfmel::Sequence sequence;
            sequence.setDuration(Fs{input.sequence_duration_femtoseconds});
            sequence.setRxEvents(events);
            rfmel::JobInterval interval;
            interval.setIntervalStart(Fs{input.interval_start_femtoseconds});
            interval.setIntervalID(input.interval_id);
            interval.setIntervalStartingGap(Fs{input.interval_starting_gap_femtoseconds});
            interval.setSequence(sequence);
            interval.setSequenceRepeatCount(static_cast<std::size_t>(input.sequence_repeat_count));
            interval.setCalDuration(Fs{input.calibration_duration_femtoseconds});
            interval.setIntervalEndingGap(Fs{input.interval_ending_gap_femtoseconds});
            interval.setPhaseCoherenceWithPrior(input.phase_coherence_with_prior != 0U);
            interval.setIterationsPerSignal(static_cast<std::size_t>(input.iterations_per_signal));
            interval.setMaxDataRateBps(input.max_data_rate_bps);
            interval.setMaxSampleRateHZ(input.max_sample_rate_hz);
            interval.setJobDetailsId(input.job_details_id);
            interval.setJobIntervalStatusEnable(static_cast<rfmel::JobIntervalStatusEnable>(mode_at(i)));
            intervals.push_back(std::move(interval));
        }
        return interval_command(job, true, [&](rfmel::JobDetail& detail) { detail.addJobIntervals(intervals); },
                                diagnostic, capacity, required);
    } catch (const std::bad_alloc&) {
        write_diagnostic("Job interval construction allocation failed", diagnostic, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        write_diagnostic("Job interval construction exception", diagnostic, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}
}
extern "C" ams_mel_status_t ams_mel_rf_job_add_rx_intervals(
    ams_mel_rf_job *job, ams_mel_rf_job_interval_config_span_v1 inputs,
    char *diagnostic, std::size_t capacity, std::size_t *required) noexcept
{ return add_rx_intervals(job, inputs, diagnostic, capacity, required); }
extern "C" ams_mel_status_t ams_mel_rf_job_add_rx_intervals_v2(
    ams_mel_rf_job *job, ams_mel_rf_job_interval_config_span_v2 inputs,
    char *diagnostic, std::size_t capacity, std::size_t *required) noexcept
{ return add_rx_intervals(job, inputs, diagnostic, capacity, required); }
extern "C" ams_mel_status_t ams_mel_rf_job_flush(
    ams_mel_rf_job *job, char *diagnostic, std::size_t capacity, std::size_t *required) noexcept
{
    return interval_command(job, true, [](rfmel::JobDetail& detail) { detail.flush(); },
                            diagnostic, capacity, required);
}
extern "C" ams_mel_status_t ams_mel_rf_job_extend_event(
    ams_mel_rf_job *job, std::uint32_t interval_id, std::uint32_t event_id,
    std::int64_t added_duration_femtoseconds, char *diagnostic,
    std::size_t capacity, std::size_t *required) noexcept
{
    return interval_command(job, false, [=](rfmel::JobDetail& detail) {
        detail.extendJobEvent(interval_id, event_id, Fs{added_duration_femtoseconds});
    }, diagnostic, capacity, required);
}
extern "C" ams_mel_status_t ams_mel_rf_job_cancel_remaining_intervals(
    ams_mel_rf_job *job, char *diagnostic, std::size_t capacity, std::size_t *required) noexcept
{
    return interval_command(job, false, [](rfmel::JobDetail& detail) { detail.cancelRemainingJobIntervals(); },
                            diagnostic, capacity, required);
}

namespace {
/* All profiles use this sole post-validation pipeline. The builder's command
 * locals and request unwind before the explicit parent release on every failure.
 * No allocation needed to publish/retain the async owner follows requestJob. */
template<class Build>
ams_mel_status_t submit_prepared(ams_mel_rf_virtual_aperture *va, Build&& build,
    ams_mel_rf_job_request **out_request, char *diagnostic,
    std::size_t capacity, std::size_t *required) noexcept
{
    std::shared_ptr<Completion> completion;
    std::shared_ptr<WorkerInput> input;
    std::unique_ptr<ams_mel_rf_job_request> owner;
    std::unique_ptr<std::thread> worker;
    try {
        completion = std::make_shared<Completion>();
        input = std::make_shared<WorkerInput>();
        input->completion = completion;
        owner = std::make_unique<ams_mel_rf_job_request>();
        owner->completion = completion;
        worker = std::make_unique<std::thread>();
    } catch (...) {
        write_diagnostic("Job request preparation failed", diagnostic, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    if (!rf_va_acquire_job_parent(va, completion->va, completion->claim)) {
        write_diagnostic("VirtualAperture job parent unavailable", diagnostic, capacity, required);
        return AMS_MEL_PROVIDER_FAILED;
    }
    bool available = false;
    try {
        rfmel::JobRequest request;
        available = build(*completion->va, request);
        if (available) input->future = completion->va->requestJob(request);
    } catch (...) {
        const auto result = translate_provider_exception("provider Job submission exception",
            "unknown provider Job submission exception", diagnostic, capacity, required);
        completion->va.reset();
        (void)completion->claim.release();
        return result;
    }
    if (!available || !input->future.valid()) {
        input->future = Future{};
        completion->va.reset();
        (void)completion->claim.release();
        write_diagnostic(available ? "invalid JobDetail future" : "provider RX element group unavailable",
                         diagnostic, capacity, required);
        return AMS_MEL_PROVIDER_FAILED;
    }
    input->emergency_self = input;
    try {
        if (failpoint("post-provider-allocation")) throw std::bad_alloc{};
        if (failpoint("worker-launch"))
            throw std::system_error{std::make_error_code(std::errc::resource_unavailable_try_again)};
        *worker = std::thread{[input]() noexcept { run_worker(input); }};
        worker->detach();
        if (failpoint("publication")) throw std::bad_alloc{};
        *out_request = owner.release();
        input->emergency_self.reset();
        input->launch_state.store(1U, std::memory_order_release);
        return AMS_MEL_OK;
    } catch (...) {
        retain(input);
        if (worker->joinable()) (void)worker.release();
        input->launch_state.store(2U, std::memory_order_release);
        write_diagnostic("Job worker/publication failed; future and parent retained",
                         diagnostic, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}
struct PreparedPipe {
    std::string label;
    std::set<rfmel::EndpointID> endpoints;
};
struct PreparedGroup {
    std::string label;
    double duty;
    std::vector<rfmel::FrequencyRange> frequencies;
    std::vector<PreparedPipe> pipes;
    std::vector<rfmel::PointingType> pointings;
    rfmel::Mode mode{rfmel::Mode::RX};
    rfmel::TxPowerLevel power{};
};
bool canonical(ams_mel_rf_utc_time_v1 time) noexcept
{ return time.fractional_femtoseconds >= 0 && time.fractional_femtoseconds < INT64_C(1000000000000000); }
bool valid_pointing(const ams_mel_rf_pointing_v1& value) noexcept
{
    switch (value.kind) {
    case AMS_MEL_RF_POINTING_ECEF: return canonical(value.ecef.time_of_validity);
    case AMS_MEL_RF_POINTING_LLA: return canonical(value.lla.time_of_validity);
    case AMS_MEL_RF_POINTING_PLATFORM_RELATIVE:
    case AMS_MEL_RF_POINTING_FACE_RELATIVE:
    case AMS_MEL_RF_POINTING_BASELINE_RELATIVE: return true;
    default: return false;
    }
}
ams::util::math::UTCTime pointing_time(ams_mel_rf_utc_time_v1 value)
{
    return {std::chrono::seconds{value.seconds}, Fs{value.fractional_femtoseconds}};
}
rfmel::PointingType prepare_pointing(const ams_mel_rf_pointing_v1& value)
{
    // Called only after tag/active UTC validation. Every c_vector component is
    // assigned explicitly: its default constructor does not initialize data_.
    switch (value.kind) {
    case AMS_MEL_RF_POINTING_ECEF: {
        EcefPoint location;
        location[0] = value.ecef.location_m.x;
        location[1] = value.ecef.location_m.y;
        location[2] = value.ecef.location_m.z;
        EcefVelocity velocity;
        velocity[0] = value.ecef.velocity_mps.x;
        velocity[1] = value.ecef.velocity_mps.y;
        velocity[2] = value.ecef.velocity_mps.z;
        rfmel::ECEFPointing point;
        point.setLocation(location);
        point.setVelocity(velocity);
        point.setTimeOfValidity(pointing_time(value.ecef.time_of_validity));
        return rfmel::PointingType{point};
    }
    case AMS_MEL_RF_POINTING_LLA: {
        const auto& input = value.lla;
        NedVelocity velocity;
        velocity[0] = input.velocity_north_mps;
        velocity[1] = input.velocity_east_mps;
        velocity[2] = input.velocity_down_mps;
        rfmel::LLAPointing point;
        point.setLocation(LLAPoint{input.latitude_rad, input.longitude_rad, input.altitude_m});
        point.setVelocity(velocity);
        point.setTimeOfValidity(pointing_time(input.time_of_validity));
        return rfmel::PointingType{point};
    }
    case AMS_MEL_RF_POINTING_PLATFORM_RELATIVE:
        return rfmel::PointingType{rfmel::PlatformRelativePointing{AzEl{
            value.platform_relative.azimuth_rad, value.platform_relative.elevation_rad}}};
    case AMS_MEL_RF_POINTING_FACE_RELATIVE:
        return rfmel::PointingType{rfmel::FaceRelativePointing{AzEl{
            value.face_relative.azimuth_rad, value.face_relative.elevation_rad}}};
    default:
        return rfmel::PointingType{rfmel::BaselineRelativePointing{value.baseline_relative_conic_rad}};
    }
}
template<class T, class Span>
std::vector<T> copy_span(Span span)
{ return span.size ? std::vector<T>{span.data, span.data + span.size} : std::vector<T>{}; }
} // namespace

extern "C" ams_mel_status_t ams_mel_rf_virtual_aperture_submit_job(
    ams_mel_rf_virtual_aperture *va, const ams_mel_rf_job_request_config_v1 *config,
    ams_mel_rf_job_request **out_request, char *diagnostic,
    std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (bad_diag(diagnostic, capacity) || !va || !config || !out_request || *out_request)
        return AMS_MEL_INVALID_ARGUMENT;
    const auto& group = config->rx_group;
    if (!valid_view(group.label) || !valid_view(group.data_pipe_label) ||
        !std::isfinite(group.desired_duty_factor) || group.desired_duty_factor <= 0.0 ||
        group.desired_duty_factor > 1.0 || config->is_interruptable > 1U ||
        !span_ok(group.expected_center_frequencies.size, group.expected_center_frequencies.data,
                 sizeof(ams_mel_rf_frequency_range_v1)) ||
        !span_ok(group.endpoint_ids.size, group.endpoint_ids.data, sizeof(std::uint64_t)) ||
        !span_ok(config->instance_selection.size, config->instance_selection.data,
                 sizeof(std::uint32_t))) return AMS_MEL_INVALID_ARGUMENT;
    std::string label, pipe;
    std::vector<rfmel::FrequencyRange> frequencies;
    std::set<rfmel::EndpointID> endpoints;
    std::vector<rfmel::VirtualApertureInstanceID> instances;
    try {
        label = copy_view(group.label);
        pipe = copy_view(group.data_pipe_label);
        frequencies.reserve(group.expected_center_frequencies.size);
        for (std::size_t i = 0; i < group.expected_center_frequencies.size; ++i) {
            const auto& range = group.expected_center_frequencies.data[i];
            if (!std::isfinite(range.min_hz) || !std::isfinite(range.max_hz) ||
                range.min_hz > range.max_hz) return AMS_MEL_INVALID_ARGUMENT;
            frequencies.emplace_back(range.min_hz, range.max_hz);
        }
        for (std::size_t i = 0; i < group.endpoint_ids.size; ++i)
            if (!endpoints.insert(group.endpoint_ids.data[i]).second) return AMS_MEL_INVALID_ARGUMENT;
        if (config->instance_selection.size)
            instances.assign(config->instance_selection.data,
                             config->instance_selection.data + config->instance_selection.size);
    } catch (...) {
        write_diagnostic("Job request preparation failed", diagnostic, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    return submit_prepared(va, [&](rfmel::VirtualAperture& provider, rfmel::JobRequest& request) {
        auto command = provider.createElementGroupCommand(label);
        if (!command || command->getMode() != rfmel::Mode::RX) return false;
        command->setDesiredDutyFactor(group.desired_duty_factor);
        for (const auto& range : frequencies) command->addExpectedCenterFrequencies(range);
        if (!endpoints.empty()) command->addEndpointIDs(endpoints, pipe);
        request.setRequestId(config->request_id);
        request.setPriority(config->priority);
        request.setPrecedenceWithinPriority(config->precedence_within_priority);
        request.setInstanceSelection(instances);
        request.setIsInterruptable(config->is_interruptable == 1U);
        request.addElementGroup(command);
        return true;
    }, out_request, diagnostic, capacity, required);
}

namespace {
template<class Config>
ams_mel_status_t submit_extended_job(
    ams_mel_rf_virtual_aperture *va, const Config *config,
    ams_mel_rf_job_request **out_request, char *diagnostic,
    std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (bad_diag(diagnostic, capacity) || !va || !config || !out_request || *out_request)
        return AMS_MEL_INVALID_ARGUMENT;
    const auto& value = *config;
    const auto inputs = [&] {
        if constexpr (std::is_same_v<Config, ams_mel_rf_job_request_config_v4>) return value.element_groups;
        else return value.rx_groups;
    }();
    if (value.is_interruptable > 1U || !canonical(value.min_start_time) ||
        !canonical(value.max_complete_time) || !inputs.size ||
        !span_ok(inputs.size, inputs.data, sizeof(*inputs.data)) ||
        !span_ok(value.instance_selection.size, value.instance_selection.data, sizeof(std::uint32_t)) ||
        !span_ok(value.capability_id.size, value.capability_id.data, sizeof(std::uint8_t)) ||
        !span_ok(value.activity_id.size, value.activity_id.data, sizeof(std::uint8_t)) ||
        !span_ok(value.tx_power_mode_ids.size, value.tx_power_mode_ids.data, sizeof(std::uint32_t)))
        return AMS_MEL_INVALID_ARGUMENT;
    std::vector<PreparedGroup> groups;
    std::vector<rfmel::VirtualApertureInstanceID> instances;
    std::vector<std::uint8_t> capability, activity;
    std::set<rfmel::TxPowerModeID> modes;
    std::optional<rfmel::PointingType> estimated;
    try {
        if constexpr (!std::is_same_v<Config, ams_mel_rf_job_request_config_v2>) {
            if (failpoint("pointing-preparation")) throw std::bad_alloc{};
            if (value.has_estimated_stab_point > 1U) return AMS_MEL_INVALID_ARGUMENT;
            if (value.has_estimated_stab_point) {
                if (!valid_pointing(value.estimated_stab_point)) return AMS_MEL_INVALID_ARGUMENT;
                estimated.emplace(prepare_pointing(value.estimated_stab_point));
            }
        }
        groups.reserve(inputs.size);
        for (std::size_t i = 0; i < inputs.size; ++i) {
            const auto& envelope = inputs.data[i];
            if constexpr (std::is_same_v<Config, ams_mel_rf_job_request_config_v4>) {
                if (envelope.mode != AMS_MEL_RF_ELEMENT_GROUP_MODE_RX &&
                    envelope.mode != AMS_MEL_RF_ELEMENT_GROUP_MODE_TX) return AMS_MEL_INVALID_ARGUMENT;
                if (envelope.mode == AMS_MEL_RF_ELEMENT_GROUP_MODE_TX) {
                    const auto& tx = envelope.tx;
                    if (!valid_view(tx.label) || !std::isfinite(tx.desired_duty_factor) ||
                        tx.desired_duty_factor <= 0.0 || tx.desired_duty_factor > 1.0 ||
                        !span_ok(tx.expected_center_frequencies.size, tx.expected_center_frequencies.data,
                                 sizeof(*tx.expected_center_frequencies.data))) return AMS_MEL_INVALID_ARGUMENT;
                    PreparedGroup prepared{copy_view(tx.label), tx.desired_duty_factor, {}, {}, {},
                                           rfmel::Mode::TX, tx.tx_power_level};
                    prepared.frequencies.reserve(tx.expected_center_frequencies.size);
                    for (std::size_t f = 0; f < tx.expected_center_frequencies.size; ++f) {
                        const auto& range = tx.expected_center_frequencies.data[f];
                        if (!std::isfinite(range.min_hz) || !std::isfinite(range.max_hz) ||
                            range.min_hz > range.max_hz) return AMS_MEL_INVALID_ARGUMENT;
                        prepared.frequencies.emplace_back(range.min_hz, range.max_hz);
                    }
                    groups.push_back(std::move(prepared));
                    continue; // Never access the inactive RX payload.
                }
            }
            const auto& input = [&]() -> const auto& {
                if constexpr (std::is_same_v<Config, ams_mel_rf_job_request_config_v4>) return envelope.rx;
                else return envelope;
            }();
            const auto& group = [&]() -> const ams_mel_rf_rx_element_group_config_v2& {
                if constexpr (!std::is_same_v<Config, ams_mel_rf_job_request_config_v2>) return input.group;
                else return input;
            }();
            if (!valid_view(group.label) || !std::isfinite(group.desired_duty_factor) ||
                group.desired_duty_factor <= 0.0 || group.desired_duty_factor > 1.0 ||
                !span_ok(group.expected_center_frequencies.size, group.expected_center_frequencies.data,
                         sizeof(*group.expected_center_frequencies.data)) ||
                !span_ok(group.data_pipe_endpoint_configs.size, group.data_pipe_endpoint_configs.data,
                         sizeof(*group.data_pipe_endpoint_configs.data))) return AMS_MEL_INVALID_ARGUMENT;
            PreparedGroup prepared{copy_view(group.label), group.desired_duty_factor, {}, {}, {}};
            prepared.frequencies.reserve(group.expected_center_frequencies.size);
            for (std::size_t f = 0; f < group.expected_center_frequencies.size; ++f) {
                const auto& range = group.expected_center_frequencies.data[f];
                if (!std::isfinite(range.min_hz) || !std::isfinite(range.max_hz) ||
                    range.min_hz > range.max_hz) return AMS_MEL_INVALID_ARGUMENT;
                prepared.frequencies.emplace_back(range.min_hz, range.max_hz);
            }
            prepared.pipes.reserve(group.data_pipe_endpoint_configs.size);
            for (std::size_t p = 0; p < group.data_pipe_endpoint_configs.size; ++p) {
                const auto& pipe = group.data_pipe_endpoint_configs.data[p];
                if (!valid_view(pipe.data_pipe_label) || !pipe.endpoint_ids.size ||
                    !span_ok(pipe.endpoint_ids.size, pipe.endpoint_ids.data, sizeof(std::uint64_t)))
                    return AMS_MEL_INVALID_ARGUMENT;
                PreparedPipe association{copy_view(pipe.data_pipe_label), {}};
                for (std::size_t e = 0; e < pipe.endpoint_ids.size; ++e)
                    if (!association.endpoints.insert(pipe.endpoint_ids.data[e]).second)
                        return AMS_MEL_INVALID_ARGUMENT;
                prepared.pipes.push_back(std::move(association));
            }
            if constexpr (!std::is_same_v<Config, ams_mel_rf_job_request_config_v2>) {
                const auto points = input.expected_pointing_angles;
                if (!span_ok(points.size, points.data, sizeof(*points.data))) return AMS_MEL_INVALID_ARGUMENT;
                prepared.pointings.reserve(points.size);
                for (std::size_t p = 0; p < points.size; ++p) {
                    if (!valid_pointing(points.data[p])) return AMS_MEL_INVALID_ARGUMENT;
                    prepared.pointings.push_back(prepare_pointing(points.data[p]));
                }
            }
            groups.push_back(std::move(prepared));
        }
        instances = copy_span<rfmel::VirtualApertureInstanceID>(value.instance_selection);
        capability = copy_span<std::uint8_t>(value.capability_id);
        activity = copy_span<std::uint8_t>(value.activity_id);
        for (std::size_t i = 0; i < value.tx_power_mode_ids.size; ++i)
            modes.insert(value.tx_power_mode_ids.data[i]);
    } catch (...) {
        write_diagnostic("Job request preparation failed", diagnostic, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    return submit_prepared(va, [&](rfmel::VirtualAperture& provider, rfmel::JobRequest& request) {
        using ams::util::math::UTCTime;
        using ams::util::math::Femtoseconds;
        request.setPriority(value.priority);
        request.setPrecedenceWithinPriority(value.precedence_within_priority);
        request.setMinStartTime(UTCTime{std::chrono::seconds{value.min_start_time.seconds},
                                      Femtoseconds{value.min_start_time.fractional_femtoseconds}});
        request.setMaxCompleteTime(UTCTime{std::chrono::seconds{value.max_complete_time.seconds},
                                         Femtoseconds{value.max_complete_time.fractional_femtoseconds}});
        request.setDuration(Femtoseconds{value.duration_femtoseconds});
        request.setCapabilityId(capability);
        request.setActivityId(activity);
        request.setRequestId(value.request_id);
        request.setInstanceSelection(instances);
        request.setIsInterruptable(value.is_interruptable == 1U);
        request.setTxPowerModeIDs(modes);
        request.setLookAheadTime(Femtoseconds{value.lookahead_femtoseconds});
        if (estimated) request.setEstimatedStabPoint(*estimated);
        for (const auto& group : groups) {
            auto command = provider.createElementGroupCommand(group.label);
            if (!command || command->getMode() != group.mode) return false;
            command->setDesiredDutyFactor(group.duty);
            if (group.mode == rfmel::Mode::TX) command->setTxPower(group.power);
            for (const auto& range : group.frequencies) command->addExpectedCenterFrequencies(range);
            if (group.mode == rfmel::Mode::RX) {
                for (const auto& pipe : group.pipes) command->addEndpointIDs(pipe.endpoints, pipe.label);
                for (const auto& point : group.pointings) command->addExpectedPointingAngle(point);
            }
            request.addElementGroup(command);
        }
        return true;
    }, out_request, diagnostic, capacity, required);
}
} // namespace

extern "C" ams_mel_status_t ams_mel_rf_virtual_aperture_submit_job_v2(
    ams_mel_rf_virtual_aperture *va, const ams_mel_rf_job_request_config_v2 *config,
    ams_mel_rf_job_request **out_request, char *diagnostic,
    std::size_t capacity, std::size_t *required) noexcept
{
    return submit_extended_job(va, config, out_request, diagnostic, capacity, required);
}
extern "C" ams_mel_status_t ams_mel_rf_virtual_aperture_submit_job_v3(
    ams_mel_rf_virtual_aperture *va, const ams_mel_rf_job_request_config_v3 *config,
    ams_mel_rf_job_request **out_request, char *diagnostic,
    std::size_t capacity, std::size_t *required) noexcept
{
    return submit_extended_job(va, config, out_request, diagnostic, capacity, required);
}

extern "C" ams_mel_status_t ams_mel_rf_virtual_aperture_submit_job_v4(
    ams_mel_rf_virtual_aperture *va, const ams_mel_rf_job_request_config_v4 *config,
    ams_mel_rf_job_request **out_request, char *diagnostic,
    std::size_t capacity, std::size_t *required) noexcept
{
    return submit_extended_job(va, config, out_request, diagnostic, capacity, required);
}

extern "C" ams_mel_status_t ams_mel_rf_job_request_wait(
    const ams_mel_rf_job_request *request, std::uint32_t timeout_ms,
    ams_mel_rf_job_result_v1 *result, char *diagnostic,
    std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!request || !request->completion || !result || bad_diag(diagnostic, capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::unique_lock lock{request->completion->mutex};
        if (request->completion->kind == Kind::Pending && timeout_ms)
            request->completion->changed.wait_for(lock, std::chrono::milliseconds{timeout_ms},
                [&] { return request->completion->kind != Kind::Pending; });
        if (request->completion->kind == Kind::Pending) return AMS_MEL_TIMEOUT;
        result->error_code = request->completion->code;
        write_diagnostic(request->completion->message, diagnostic, capacity, required);
        return status(request->completion->kind);
    } catch (...) { return AMS_MEL_INTERNAL_ERROR; }
}

extern "C" ams_mel_status_t ams_mel_rf_job_request_claim(
    ams_mel_rf_job_request *request, ams_mel_rf_job **out_job,
    char *diagnostic, std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!request || !request->completion || !out_job || *out_job || bad_diag(diagnostic, capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    auto& completion = *request->completion;
    std::shared_ptr<rfmel::JobDetail> failed_job;
    std::shared_ptr<rfmel::VirtualAperture> failed_va;
    RfC2ChildClaim failed_claim;
    ams_mel_status_t failure{AMS_MEL_OK};
    std::string failure_message;
    try {
        std::unique_lock lock{completion.mutex};
        if (completion.kind != Kind::Succeeded) {
            write_diagnostic(completion.message, diagnostic, capacity, required);
            return status(completion.kind);
        }
        if (completion.claim_failed) {
            write_diagnostic(completion.claim_message, diagnostic, capacity, required);
            return completion.claim_status;
        }
        if (completion.claimed) {
            write_diagnostic("JobDetail already claimed", diagnostic, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        std::unique_ptr<ams_mel_rf_job> owner;
        bool getter_started = false;
        try {
            if (failpoint("job-owner")) throw std::bad_alloc{};
            owner = std::make_unique<ams_mel_rf_job>();
            owner->state = std::make_shared<JobState>();
            auto& state = *owner->state;
            getter_started = true;
            const auto time = completion.job->actualStartTime();
            state.info.actual_start_seconds = time.getIntegralSeconds().count();
            state.info.actual_start_femtoseconds = time.getFractionalFemtoseconds().count();
            state.info.total_job_duration_femtoseconds = completion.job->totalJobDuration().count();
            state.info.va_instance_id = completion.job->getVAInstanceID();
            state.info.va_definition_id = completion.job->getVADefinitionID();
            state.info.job_details_id = completion.job->getJobDetailsID();
            state.streams = completion.job->getRxStreamIDs(0);
            state.info.job_request_id = completion.job->getJobRequestId();
            state.info.lookahead_femtoseconds = completion.job->getLookAheadTime().count();
            if (failpoint("job-snapshot")) throw std::bad_alloc{};
            state.info.rx_stream_ids = {state.streams.empty() ? nullptr : state.streams.data(), state.streams.size()};
            state.detail = std::move(completion.job);
            state.va = std::move(completion.va);
            state.claim = std::move(completion.claim);
            completion.claimed = true;
            completion.changed.notify_all();
            *out_job = owner.release();
            return AMS_MEL_OK;
        } catch (const std::bad_alloc&) {
            failure = AMS_MEL_INTERNAL_ERROR;
            try { failure_message = "Job snapshot allocation failed"; } catch (...) {}
        } catch (...) {
            failure = AMS_MEL_PROVIDER_EXCEPTION;
            try { failure_message = exception_text("Job getter exception", "unknown Job getter exception"); }
            catch (...) {}
        }
        if (getter_started) {
            completion.claim_failed = true;
            completion.claim_status = failure;
            completion.claim_message.swap(failure_message);
            failed_job = std::move(completion.job);
            failed_va = std::move(completion.va);
            failed_claim = std::move(completion.claim);
            completion.changed.notify_all();
        }
    } catch (...) { return AMS_MEL_INTERNAL_ERROR; }
    failed_job.reset();
    failed_va.reset();
    const auto shutdown = failed_claim.release();
    if (shutdown.status != AMS_MEL_OK) {
        std::lock_guard lock{completion.mutex};
        completion.claim_failed = true;
        completion.claim_status = shutdown.status;
        completion.claim_message = shutdown.message;
        write_diagnostic(completion.claim_message, diagnostic, capacity, required);
        return shutdown.status;
    }
    if (failure == AMS_MEL_OK) {
        write_diagnostic("Job owner allocation failed before getter", diagnostic, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    write_diagnostic(completion.claim_message, diagnostic, capacity, required);
    return failure;
}

extern "C" ams_mel_status_t ams_mel_rf_job_request_close(
    ams_mel_rf_job_request **request, char *diagnostic,
    std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!request || bad_diag(diagnostic, capacity)) return AMS_MEL_INVALID_ARGUMENT;
    auto owned = std::exchange(*request, nullptr);
    if (!owned) return AMS_MEL_OK;
    auto completion = std::move(owned->completion);
    delete owned;
    if (!completion) return AMS_MEL_OK;
    try {
        std::lock_guard lock{completion->mutex};
        completion->abandoned = true;
        completion->changed.notify_all();
    } catch (...) {
        completion->emergency_self = completion;
        return AMS_MEL_INTERNAL_ERROR;
    }
    return AMS_MEL_OK;
}
extern "C" ams_mel_status_t ams_mel_rf_job_view(
    const ams_mel_rf_job *job, const ams_mel_rf_job_info_v1 **out_info,
    char *diagnostic, std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!job || !job->state || !job->state->detail || !out_info || *out_info || bad_diag(diagnostic, capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    *out_info = &job->state->info;
    return AMS_MEL_OK;
}
extern "C" ams_mel_status_t ams_mel_rf_job_finalize(
    ams_mel_rf_job *job, char *diagnostic, std::size_t capacity,
    std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!job || !job->state || !job->state->detail || bad_diag(diagnostic, capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    auto state = job->state;
    try {
        std::unique_lock lock{state->mutex};
        if (state->finalize_attempted) {
            state->changed.wait(lock, [&] { return !state->finalize_running; });
            write_diagnostic(state->finalize_message, diagnostic, capacity, required);
            return state->finalize_result;
        }
        if (state->cancel_attempted) {
            write_diagnostic("cancellation was already attempted", diagnostic, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        auto input = std::make_shared<FinalizeInput>();
        input->state = state;
        /* Every bridge allocation needed to publish a post-provider launch
         * failure is completed before invoking finalize(). */
        std::string message;
        message.reserve(256);
        state->finalize_attempted = true;
        state->finalize_running = true;
        lock.unlock();
        ams_mel_status_t result = AMS_MEL_OK;
        try {
            input->future = state->detail->finalize();
            if (!input->future.valid()) {
                result = AMS_MEL_PROVIDER_FAILED;
                message = "provider Job finalize returned invalid future";
            } else {
                input->emergency_self = input;
                try {
                    if (failpoint("finalize-worker-launch"))
                        throw std::system_error(std::make_error_code(std::errc::resource_unavailable_try_again));
                    std::thread{[input] { finalize_worker(input); }}.detach();
                    input->emergency_self.reset();
                    input->launch_state.store(1U, std::memory_order_release);
                } catch (...) {
                    retain_finalize(input);
                    input->launch_state.store(2U, std::memory_order_release);
                    result = AMS_MEL_INTERNAL_ERROR;
                    message = "Job finalize worker launch failed; future and provider graph retained";
                }
            }
        } catch (...) {
            result = AMS_MEL_PROVIDER_EXCEPTION;
            try { message = exception_text("provider Job finalize exception",
                                           "unknown provider Job finalize exception"); } catch (...) {}
        }
        lock.lock();
        state->finalize_result = result;
        state->finalize_message.swap(message);
        state->finalize_running = false;
        state->changed.notify_all();
        write_diagnostic(state->finalize_message, diagnostic, capacity, required);
        return result;
    } catch (...) {
        write_diagnostic("Job finalize bridge failure", diagnostic, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}
extern "C" ams_mel_status_t ams_mel_rf_job_wait_status(
    const ams_mel_rf_job *job, std::uint32_t timeout_ms,
    ams_mel_rf_job_status_t *out_status, char *diagnostic,
    std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!job || !job->state || !out_status || bad_diag(diagnostic, capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        auto& state = *job->state;
        std::unique_lock lock{state.mutex};
        if (!state.finalize_attempted) {
            write_diagnostic("Job finalize has not been attempted", diagnostic, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        if (!state.finalize_running && state.finalize_result != AMS_MEL_OK) {
            write_diagnostic(state.finalize_message, diagnostic, capacity, required);
            return state.finalize_result;
        }
        if (!state.changed.wait_for(lock, std::chrono::milliseconds{timeout_ms},
                                    [&] { return state.terminal || (!state.finalize_running && state.finalize_result != AMS_MEL_OK); }))
            return AMS_MEL_TIMEOUT;
        if (!state.terminal) {
            write_diagnostic(state.finalize_message, diagnostic, capacity, required);
            return state.finalize_result;
        }
        write_diagnostic(state.terminal_message, diagnostic, capacity, required);
        if (state.terminal_result == AMS_MEL_OK) *out_status = state.published_status;
        return state.terminal_result;
    } catch (...) { return AMS_MEL_INTERNAL_ERROR; }
}
extern "C" ams_mel_status_t ams_mel_rf_job_cancel(
    ams_mel_rf_job *job, ams_mel_rf_job_cancel_result_v1 *out_result,
    char *diagnostic, std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!job || !job->state || !job->state->detail || !out_result || bad_diag(diagnostic, capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    auto state = job->state;
    try {
        std::unique_lock lock{state->mutex};
        if (state->cancel_attempted) {
            state->changed.wait(lock, [&] { return !state->cancel_running; });
        } else {
            std::string message;
            message.reserve(256);
            state->cancel_attempted = true;
            state->cancel_running = true;
            state->changed.wait(lock, [&] { return !state->finalize_running; });
            lock.unlock(); /* Provider may fulfill the finalize future here. */
            ams_mel_status_t result = AMS_MEL_OK;
            ams_mel_rf_job_cancel_result_v1 cancelled{};
            try {
                const auto answer = state->detail->cancelJob();
                if (answer.getError() != rfmel::CancelError::None) {
                    result = AMS_MEL_PROVIDER_FAILED;
                    message = "unknown provider CancelError";
                } else {
                    cancelled.cancelled = static_cast<bool>(answer) ? 1U : 0U;
                    cancelled.error_code = AMS_MEL_RF_CANCEL_ERROR_NONE;
                }
            } catch (...) {
                result = AMS_MEL_PROVIDER_EXCEPTION;
                try { message = exception_text("provider Job cancel exception",
                                               "unknown provider Job cancel exception"); } catch (...) {}
            }
            lock.lock();
            state->cancel_result = result;
            state->cancelled = cancelled;
            state->cancel_message.swap(message);
            state->cancel_running = false;
            state->changed.notify_all();
        }
        write_diagnostic(state->cancel_message, diagnostic, capacity, required);
        if (state->cancel_result == AMS_MEL_OK) *out_result = state->cancelled;
        return state->cancel_result;
    } catch (...) { return AMS_MEL_INTERNAL_ERROR; }
}
extern "C" ams_mel_status_t ams_mel_rf_job_close(
    ams_mel_rf_job **job, char *diagnostic,
    std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!job || bad_diag(diagnostic, capacity)) return AMS_MEL_INVALID_ARGUMENT;
    auto owned = std::exchange(*job, nullptr);
    if (!owned) return AMS_MEL_OK;
    auto state = std::move(owned->state);
    delete owned;
    if (!state) return AMS_MEL_OK;
    stop_interval_status(state->interval_status);
    {
        std::lock_guard lock{state->mutex};
        /* Terminal publication happens only after releasing the consumed
         * future. A pending worker keeps the graph; Close never waits. */
        if (state->finalize_attempted && !state->terminal &&
            (state->finalize_running || state->finalize_result == AMS_MEL_OK ||
             (state->finalize_result == AMS_MEL_INTERNAL_ERROR &&
              state->finalize_message.find("retained") != std::string::npos)))
            return AMS_MEL_OK;
    }
    state->detail.reset();
    state->va.reset();
    const auto shutdown = state->claim.release();
    write_diagnostic(shutdown.message, diagnostic, capacity, required);
    return shutdown.status;
}
