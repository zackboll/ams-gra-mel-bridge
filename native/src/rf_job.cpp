/* One provider-created RX command and one C2 child claim per asynchronous Job. */
#include <ams_mel/abi.h>
#include "internal/provider_common.hpp"
#include "internal/rf_va_job_parent.hpp"
#include <rfmel/c2/ElementGroupCommand.h>
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
struct ams_mel_rf_job {
    std::shared_ptr<rfmel::JobDetail> detail;
    std::shared_ptr<rfmel::VirtualAperture> va;
    RfC2ChildClaim claim;
    std::vector<std::uint32_t> streams;
    ams_mel_rf_job_info_v1 info{};
};

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
    std::shared_ptr<Completion> completion;
    std::shared_ptr<WorkerInput> input;
    std::unique_ptr<ams_mel_rf_job_request> owner;
    std::unique_ptr<std::thread> worker;
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
    /* Synchronous failures must destroy the command before dropping VA/C2. */
    std::shared_ptr<rfmel::ElementGroupCommand> command;
    try {
        command = completion->va->createElementGroupCommand(label);
        if (!command || command->getMode() != rfmel::Mode::RX) {
            write_diagnostic("provider RX element group unavailable", diagnostic, capacity, required);
            command.reset();
            completion->va.reset();
            (void)completion->claim.release();
            return AMS_MEL_PROVIDER_FAILED;
        }
        command->setDesiredDutyFactor(group.desired_duty_factor);
        for (const auto& range : frequencies) command->addExpectedCenterFrequencies(range);
        if (!endpoints.empty()) command->addEndpointIDs(endpoints, pipe);
        rfmel::JobRequest request;
        request.setRequestId(config->request_id);
        request.setPriority(config->priority);
        request.setPrecedenceWithinPriority(config->precedence_within_priority);
        request.setInstanceSelection(instances);
        request.setIsInterruptable(config->is_interruptable == 1U);
        request.addElementGroup(command);
        input->future = completion->va->requestJob(request);
    } catch (...) {
        const auto result = translate_provider_exception("provider Job submission exception",
            "unknown provider Job submission exception", diagnostic, capacity, required);
        command.reset();
        completion->va.reset();
        (void)completion->claim.release();
        return result;
    }
    command.reset();
    if (!input->future.valid()) {
        input->future = Future{};
        completion->va.reset();
        (void)completion->claim.release();
        write_diagnostic("invalid JobDetail future", diagnostic, capacity, required);
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
            getter_started = true;
            const auto time = completion.job->actualStartTime();
            owner->info.actual_start_seconds = time.getIntegralSeconds().count();
            owner->info.actual_start_femtoseconds = time.getFractionalFemtoseconds().count();
            owner->info.total_job_duration_femtoseconds = completion.job->totalJobDuration().count();
            owner->info.va_instance_id = completion.job->getVAInstanceID();
            owner->info.va_definition_id = completion.job->getVADefinitionID();
            owner->info.job_details_id = completion.job->getJobDetailsID();
            owner->streams = completion.job->getRxStreamIDs(0);
            owner->info.job_request_id = completion.job->getJobRequestId();
            owner->info.lookahead_femtoseconds = completion.job->getLookAheadTime().count();
            if (failpoint("job-snapshot")) throw std::bad_alloc{};
            owner->info.rx_stream_ids = {owner->streams.empty() ? nullptr : owner->streams.data(), owner->streams.size()};
            owner->detail = std::move(completion.job);
            owner->va = std::move(completion.va);
            owner->claim = std::move(completion.claim);
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
    if (!job || !job->detail || !out_info || *out_info || bad_diag(diagnostic, capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    *out_info = &job->info;
    return AMS_MEL_OK;
}
extern "C" ams_mel_status_t ams_mel_rf_job_close(
    ams_mel_rf_job **job, char *diagnostic,
    std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!job || bad_diag(diagnostic, capacity)) return AMS_MEL_INVALID_ARGUMENT;
    auto owned = std::exchange(*job, nullptr);
    if (!owned) return AMS_MEL_OK;
    auto claim = std::move(owned->claim);
    owned->detail.reset();
    owned->va.reset();
    delete owned;
    const auto shutdown = claim.release();
    write_diagnostic(shutdown.message, diagnostic, capacity, required);
    return shutdown.status;
}