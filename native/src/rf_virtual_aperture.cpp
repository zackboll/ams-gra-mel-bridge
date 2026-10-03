/* One C2 child claim follows a VA future into its single claimed VA. */
#include <ams_mel/abi.h>
#include "internal/provider_common.hpp"
#include "internal/rf_c2.hpp"
#include "internal/rf_va_job_parent.hpp"
#include "internal/rf_va_status.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <cstdlib>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <stdexcept>
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
using Future = mel::RequestFor<rfmel::VirtualAperture>;
enum class Kind { Pending, Succeeded, ProviderFailed, ProviderException, InternalError };
struct Completion {
    std::mutex mutex;
    std::condition_variable changed;
    Kind kind{Kind::Pending};
    ams_mel_error_code_t code{AMS_MEL_ERROR_NONE};
    std::string message;
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
bool bad_diag(const char *out, std::size_t capacity) noexcept
{ return !out && capacity != 0U; }
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
bool map_error(mel::ErrorCode value, ams_mel_error_code_t& output) noexcept
{
    switch (value) {
    case mel::ErrorCode::None: output = AMS_MEL_ERROR_NONE; return true;
    case mel::ErrorCode::InvalidId: output = AMS_MEL_ERROR_INVALID_ID; return true;
    case mel::ErrorCode::InvalidState: output = AMS_MEL_ERROR_INVALID_STATE; return true;
    case mel::ErrorCode::InvalidParameters: output = AMS_MEL_ERROR_INVALID_PARAMETERS; return true;
    case mel::ErrorCode::InsufficientPermissions: output = AMS_MEL_ERROR_INSUFFICIENT_PERMISSIONS; return true;
    case mel::ErrorCode::InsufficientResources: output = AMS_MEL_ERROR_INSUFFICIENT_RESOURCES; return true;
    case mel::ErrorCode::InsufficientLocalResources: output = AMS_MEL_ERROR_INSUFFICIENT_LOCAL_RESOURCES; return true;
    case mel::ErrorCode::InsufficientRemoteResources: output = AMS_MEL_ERROR_INSUFFICIENT_REMOTE_RESOURCES; return true;
    case mel::ErrorCode::Unsupported: output = AMS_MEL_ERROR_UNSUPPORTED; return true;
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
    std::shared_ptr<rfmel::VirtualAperture> va;
};
/* The sole production future.get() for RF VA requests. */
void settle(WorkerInput& input, Outcome& output) noexcept
{
    std::optional<mel::ErrorOr<std::shared_ptr<rfmel::VirtualAperture>>> result;
    try { result.emplace(input.future.get()); }
    catch (const std::bad_alloc&) { return; }
    catch (...) {
        output.kind = Kind::ProviderException;
        try { output.message = exception_text("provider VA future exception", "unknown provider VA future exception"); }
        catch (...) {}
        return;
    }
    try {
        if (!*result) {
            const mel::Error& error = result->getError();
            output.kind = Kind::ProviderFailed;
            if (!map_error(error.getCode(), output.code)) {
                output.message = "unknown MEL error code";
            } else {
                const std::string& text = error.getDescription();
                output.message = valid_utf8(text) ? text : "invalid provider VA error description";
            }
        } else {
            output.va = std::move(result->get());
            if (!output.va) {
                output.kind = Kind::ProviderFailed;
                output.message = "null successful VirtualAperture";
            } else output.kind = Kind::Succeeded;
        }
    } catch (...) {
        output.kind = Kind::InternalError;
        output.va.reset();
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
    if (launch == 2U) return; /* input and future are already retained */
    try {
        Outcome result;
        settle(*input, result);
        input->future = Future{}; /* release provider future before child claim */
        auto& completion = *input->completion;
        RfC2ChildClaim released;
        if (result.kind != Kind::Succeeded) {
            std::lock_guard lock{completion.mutex};
            released = std::move(completion.claim);
        }
        const C2ShutdownOutcome shutdown = released.release();
        if (shutdown.status != AMS_MEL_OK) {
            result.kind = shutdown.status == AMS_MEL_INTERNAL_ERROR ? Kind::InternalError : Kind::ProviderException;
            result.message = shutdown.message;
        }
        std::shared_ptr<rfmel::VirtualAperture> abandoned;
        {
            std::unique_lock lock{completion.mutex};
            completion.kind = result.kind;
            completion.code = result.code;
            completion.message.swap(result.message);
            if (result.kind == Kind::Succeeded) {
                completion.va = std::move(result.va);
            }
            completion.changed.notify_all();
            if (result.kind == Kind::Succeeded) {
                completion.changed.wait(lock, [&] { return completion.claimed || completion.abandoned || completion.claim_failed; });
                if (!completion.claimed && !completion.claim_failed) {
                    abandoned = std::move(completion.va);
                    released = std::move(completion.claim);
                }
            }
        }
        abandoned.reset(); /* destroy VA before deferred shutdown */
        (void)released.release();
    } catch (...) { retain(input); }
}
bool valid_view(ams_mel_string_view_v1 view) noexcept
{ return (view.data || !view.size) && view.size < std::numeric_limits<std::size_t>::max() &&
         valid_utf8(view.data ? std::string_view{view.data, view.size} : std::string_view{}); }
std::string copy_view(ams_mel_string_view_v1 view)
{ return view.data ? std::string{view.data, view.size} : std::string{}; }
bool span_ok(std::size_t count, const void *data, std::size_t element) noexcept
{ return (!count || data) && count <= std::numeric_limits<std::size_t>::max() / element; }
bool failpoint(const char *name) noexcept
{
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    const char *value = std::getenv("AMS_MEL_TEST_RF_VA_FAILURE");
    return value && std::strcmp(value, name) == 0;
#else
    (void)name;
    return false;
#endif
}
} // namespace

struct ams_mel_rf_virtual_aperture_request {
    std::shared_ptr<Completion> completion;
};
struct ams_mel_rf_virtual_aperture {
    std::shared_ptr<rfmel::VirtualAperture> va;
    RfC2ChildClaim claim;
    std::vector<std::uint32_t> ids;
    std::vector<std::string> labels;
    std::vector<ams_mel_string_view_v1> views;
    ams_mel_rf_virtual_aperture_info_v1 info{};
    std::unique_ptr<VaRegistration> registration;
};
struct ams_mel_rf_va_instance_list {
    std::vector<std::uint32_t> ids;
};
struct ams_mel_rf_va_instance_status_report {
    std::vector<std::vector<std::uint32_t>> statuses;
    std::vector<ams_mel_rf_va_local_function_status_v1> groups;
    ams_mel_rf_va_instance_status_report_v1 view{};
};

namespace {
static_assert(static_cast<unsigned>(rfmel::VirtualApertureStatus::None) == AMS_MEL_RF_VA_STATUS_NONE);
static_assert(static_cast<unsigned>(rfmel::VirtualApertureStatus::Operational) == AMS_MEL_RF_VA_STATUS_OPERATIONAL);
static_assert(static_cast<unsigned>(rfmel::VirtualApertureStatus::Degraded) == AMS_MEL_RF_VA_STATUS_DEGRADED);
static_assert(static_cast<unsigned>(rfmel::VirtualApertureStatus::Failed) == AMS_MEL_RF_VA_STATUS_FAILED);
bool decode_va_status(rfmel::VirtualApertureStatus value, std::uint32_t& output) noexcept
{
    switch (value) {
    case rfmel::VirtualApertureStatus::None: output = AMS_MEL_RF_VA_STATUS_NONE; return true;
    case rfmel::VirtualApertureStatus::Operational: output = AMS_MEL_RF_VA_STATUS_OPERATIONAL; return true;
    case rfmel::VirtualApertureStatus::Degraded: output = AMS_MEL_RF_VA_STATUS_DEGRADED; return true;
    case rfmel::VirtualApertureStatus::Failed: output = AMS_MEL_RF_VA_STATUS_FAILED; return true;
    }
    return false;
}
ams_mel_status_t unknown_va_status(char *diagnostic, std::size_t capacity,
                                  std::size_t *required) noexcept
{
    write_diagnostic("unknown VirtualApertureStatus", diagnostic, capacity, required);
    return AMS_MEL_PROVIDER_FAILED;
}
template<class Query>
ams_mel_status_t query_va_status(const ams_mel_rf_virtual_aperture *va,
    std::uint32_t *output, char *diagnostic, std::size_t capacity,
    std::size_t *required, Query query) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!va || !va->va || !output || bad_diag(diagnostic, capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::uint32_t value{};
        if (!decode_va_status(query(*va->va), value)) return unknown_va_status(diagnostic, capacity, required);
        *output = value;
        return AMS_MEL_OK;
    } catch (...) {
        return translate_provider_exception("VA query exception", "unknown VA query exception", diagnostic, capacity, required);
    }
}
template<class Query>
ams_mel_status_t query_va_list(const ams_mel_rf_virtual_aperture *va,
    ams_mel_rf_va_instance_list **output, char *diagnostic, std::size_t capacity,
    std::size_t *required, Query query) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!va || !va->va || !output || *output || bad_diag(diagnostic, capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try {
        const auto value = query(*va->va);
        if (failpoint("query-list-owner")) throw std::bad_alloc{};
        auto owner = std::make_unique<ams_mel_rf_va_instance_list>();
        if (failpoint("query-list-copy")) throw std::bad_alloc{};
        owner->ids.assign(value.begin(), value.end());
        *output = owner.release();
        return AMS_MEL_OK;
    } catch (...) {
        return translate_provider_exception("VA list query exception", "unknown VA list query exception", diagnostic, capacity, required);
    }
}
} // namespace

extern "C" ams_mel_status_t ams_mel_rf_virtual_aperture_get_id(
    const ams_mel_rf_virtual_aperture *va, std::uint32_t *output, char *diagnostic,
    std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!va || !va->va || !output || bad_diag(diagnostic, capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try { const auto value = va->va->getID(); *output = value; return AMS_MEL_OK; }
    catch (...) { return translate_provider_exception("VA ID query exception", "unknown VA ID query exception", diagnostic, capacity, required); }
}
extern "C" ams_mel_status_t ams_mel_rf_virtual_aperture_get_status(
    const ams_mel_rf_virtual_aperture *va, ams_mel_rf_virtual_aperture_status_t *output,
    char *diagnostic, std::size_t capacity, std::size_t *required) noexcept
{
    return query_va_status(va, output, diagnostic, capacity, required,
                          [](const auto& provider) { return provider.getStatus(); });
}
extern "C" ams_mel_status_t ams_mel_rf_virtual_aperture_get_instance_status(
    const ams_mel_rf_virtual_aperture *va, std::uint32_t id,
    ams_mel_rf_virtual_aperture_status_t *output, char *diagnostic,
    std::size_t capacity, std::size_t *required) noexcept
{
    return query_va_status(va, output, diagnostic, capacity, required,
                          [id](const auto& provider) { return provider.getInstanceStatus(id); });
}
extern "C" ams_mel_status_t ams_mel_rf_virtual_aperture_get_all_instances(
    const ams_mel_rf_virtual_aperture *va, ams_mel_rf_va_instance_list **output,
    char *diagnostic, std::size_t capacity, std::size_t *required) noexcept
{
    return query_va_list(va, output, diagnostic, capacity, required,
                        [](const auto& provider) { return provider.getAllInstances(); });
}
extern "C" ams_mel_status_t ams_mel_rf_virtual_aperture_get_instances(
    const ams_mel_rf_virtual_aperture *va, std::uint32_t face,
    ams_mel_rf_va_instance_list **output, char *diagnostic,
    std::size_t capacity, std::size_t *required) noexcept
{
    return query_va_list(va, output, diagnostic, capacity, required,
                        [face](const auto& provider) { return provider.getInstances(face); });
}
extern "C" ams_mel_status_t ams_mel_rf_va_instance_list_view(
    const ams_mel_rf_va_instance_list *owner, ams_mel_u32_span_v1 *output,
    char *diagnostic, std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!owner || !output || bad_diag(diagnostic, capacity)) return AMS_MEL_INVALID_ARGUMENT;
    *output = {owner->ids.empty() ? nullptr : owner->ids.data(), owner->ids.size()};
    return AMS_MEL_OK;
}
extern "C" ams_mel_status_t ams_mel_rf_va_instance_list_close(
    ams_mel_rf_va_instance_list **owner, char *diagnostic,
    std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!owner || bad_diag(diagnostic, capacity)) return AMS_MEL_INVALID_ARGUMENT;
    delete std::exchange(*owner, nullptr);
    return AMS_MEL_OK;
}
extern "C" ams_mel_status_t ams_mel_rf_virtual_aperture_get_instance_status_report(
    const ams_mel_rf_virtual_aperture *va, std::uint32_t id,
    ams_mel_rf_va_instance_status_report **output, char *diagnostic,
    std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!va || !va->va || !output || *output || bad_diag(diagnostic, capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try {
        const auto value = va->va->getInstanceStatusReport(id);
        const auto returned_id = value.getVAInstanceID();
        const auto top_status = value.getStatus();
        const auto functions = value.getLFStatus(); // BY VALUE, exactly once.
        std::uint32_t decoded{};
        if (!decode_va_status(top_status, decoded)) return unknown_va_status(diagnostic, capacity, required);
        if (failpoint("query-report-owner")) throw std::bad_alloc{};
        auto owner = std::make_unique<ams_mel_rf_va_instance_status_report>();
        owner->view.va_instance_id = returned_id;
        owner->view.status = decoded;
        owner->statuses.resize(functions.size());
        owner->groups.resize(functions.size());
        std::size_t index = 0;
        for (const auto& [key, statuses] : functions) {
            if (index == 1 && failpoint("query-report-nested")) throw std::bad_alloc{};
            owner->groups[index].local_function_type_id = key;
            auto& copied = owner->statuses[index];
            copied.reserve(statuses.size());
            for (const auto item : statuses) {
                if (!decode_va_status(item, decoded)) return unknown_va_status(diagnostic, capacity, required);
                copied.push_back(decoded);
            }
            ++index;
        }
        // All backing arrays are final-sized before publishing any pointers.
        for (std::size_t i = 0; i < owner->groups.size(); ++i) {
            const auto& copied = owner->statuses[i];
            owner->groups[i].statuses = {copied.empty() ? nullptr : copied.data(), copied.size()};
        }
        owner->view.local_functions = {owner->groups.empty() ? nullptr : owner->groups.data(), owner->groups.size()};
        *output = owner.release();
        return AMS_MEL_OK;
    } catch (...) {
        return translate_provider_exception("VA report query exception", "unknown VA report query exception", diagnostic, capacity, required);
    }
}
extern "C" ams_mel_status_t ams_mel_rf_va_instance_status_report_view(
    const ams_mel_rf_va_instance_status_report *owner,
    const ams_mel_rf_va_instance_status_report_v1 **output, char *diagnostic,
    std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!owner || !output || *output || bad_diag(diagnostic, capacity)) return AMS_MEL_INVALID_ARGUMENT;
    *output = &owner->view;
    return AMS_MEL_OK;
}
extern "C" ams_mel_status_t ams_mel_rf_va_instance_status_report_close(
    ams_mel_rf_va_instance_status_report **owner, char *diagnostic,
    std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!owner || bad_diag(diagnostic, capacity)) return AMS_MEL_INVALID_ARGUMENT;
    delete std::exchange(*owner, nullptr);
    return AMS_MEL_OK;
}

bool rf_va_acquire_job_parent(const ams_mel_rf_virtual_aperture *owner,
    std::shared_ptr<rfmel::VirtualAperture>& provider, RfC2ChildClaim& claim) noexcept
{
    if (!owner || !owner->va || provider || !owner->claim.acquire_sibling(claim)) return false;
    provider = owner->va;
    return true;
}

extern "C" ams_mel_status_t ams_mel_rf_c2_submit_virtual_aperture(
    ams_mel_rf_c2 *c2, const ams_mel_rf_virtual_aperture_config_v1 *config,
    ams_mel_rf_virtual_aperture_request **out_request, char *diagnostic,
    std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (bad_diag(diagnostic, capacity) || !c2 || !c2->state || !config ||
        !out_request || *out_request ||
        !span_ok(config->local_function_info.size, config->local_function_info.data,
                 sizeof(ams_mel_string_view_v1)) ||
        !span_ok(config->capability_ids.size, config->capability_ids.data,
                 sizeof(ams_mel_uci_id_v1)) || !valid_view(config->va_definition_file_info))
        return AMS_MEL_INVALID_ARGUMENT;
    std::vector<std::string> functions;
    std::vector<mel::UCI_ID> capabilities;
    std::string file_info;
    std::shared_ptr<Completion> completion;
    std::shared_ptr<WorkerInput> input;
    std::unique_ptr<ams_mel_rf_virtual_aperture_request> owner;
    std::unique_ptr<std::thread> worker;
    try {
        functions.reserve(config->local_function_info.size);
        for (std::size_t i = 0; i < config->local_function_info.size; ++i) {
            auto value = config->local_function_info.data[i];
            if (!valid_view(value)) return AMS_MEL_INVALID_ARGUMENT;
            functions.push_back(copy_view(value));
        }
        file_info = copy_view(config->va_definition_file_info);
        capabilities.reserve(config->capability_ids.size);
        for (std::size_t i = 0; i < config->capability_ids.size; ++i) {
            const auto& item = config->capability_ids.data[i];
            if (!valid_view(item.descriptive_label)) return AMS_MEL_INVALID_ARGUMENT;
            std::array<std::uint8_t, mel::UUID_SIZE> bytes{};
            std::copy(std::begin(item.uuid), std::end(item.uuid), bytes.begin());
            capabilities.emplace_back(bytes, copy_view(item.descriptive_label));
        }
        completion = std::make_shared<Completion>();
        input = std::make_shared<WorkerInput>();
        input->completion = completion;
        owner = std::make_unique<ams_mel_rf_virtual_aperture_request>();
        owner->completion = completion;
        worker = std::make_unique<std::thread>();
    } catch (...) {
        write_diagnostic("VA request preparation failed", diagnostic, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    if (!RfC2ChildClaim::acquire(c2->state, completion->claim)) {
        write_diagnostic("C2 closing", diagnostic, capacity, required);
        return AMS_MEL_PROVIDER_FAILED;
    }
    try {
        input->future = c2->state->c2->requestVirtualAperture(
            config->va_definition_id, config->priority, functions, file_info, capabilities);
    } catch (...) {
        (void)completion->claim.release();
        return translate_provider_exception("provider VA submission exception",
                                            "unknown provider VA submission exception",
                                            diagnostic, capacity, required);
    }
    if (!input->future.valid()) {
        (void)completion->claim.release();
        write_diagnostic("invalid VirtualAperture future", diagnostic, capacity, required);
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
        write_diagnostic("VA worker/publication failed; future and C2 retained",
                         diagnostic, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_rf_virtual_aperture_request_wait(
    const ams_mel_rf_virtual_aperture_request *request, std::uint32_t timeout_ms,
    ams_mel_rf_virtual_aperture_result_v1 *result, char *diagnostic,
    std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!request || !request->completion || !result || bad_diag(diagnostic, capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::unique_lock lock{request->completion->mutex};
        if (request->completion->kind == Kind::Pending && timeout_ms != 0U)
            request->completion->changed.wait_for(lock, std::chrono::milliseconds{timeout_ms},
                [&] { return request->completion->kind != Kind::Pending; });
        if (request->completion->kind == Kind::Pending) return AMS_MEL_TIMEOUT;
        result->error_code = request->completion->code;
        write_diagnostic(request->completion->message, diagnostic, capacity, required);
        return status(request->completion->kind);
    } catch (...) { return AMS_MEL_INTERNAL_ERROR; }
}

extern "C" ams_mel_status_t ams_mel_rf_virtual_aperture_request_claim(
    ams_mel_rf_virtual_aperture_request *request, ams_mel_rf_virtual_aperture **out_va,
    char *diagnostic, std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!request || !request->completion || !out_va || *out_va || bad_diag(diagnostic, capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    auto& completion = *request->completion;
    std::shared_ptr<rfmel::VirtualAperture> failed_va;
    RfC2ChildClaim failed_claim;
    ams_mel_status_t failure = AMS_MEL_OK;
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
            write_diagnostic("VirtualAperture already claimed", diagnostic, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        std::unique_ptr<ams_mel_rf_virtual_aperture> owner;
        bool getter_started = false;
        try {
            if (failpoint("va-owner")) throw std::bad_alloc{};
            owner = std::make_unique<ams_mel_rf_virtual_aperture>();
            getter_started = true;
            auto ids = completion.va->getVAInstanceIDs();
            owner->ids.assign(ids.begin(), ids.end());
            owner->labels = completion.va->getElementGroupLabels();
            owner->info.is_single_group = completion.va->isSingleGroup() ? 1U : 0U;
            if (failpoint("va-snapshot")) throw std::bad_alloc{};
            owner->views.reserve(owner->labels.size());
            for (const auto& label : owner->labels) {
                if (!valid_utf8(label)) throw std::runtime_error("invalid VA label UTF-8 or NUL");
                owner->views.push_back({label.data(), label.size()});
            }
            owner->info.va_instance_ids = {owner->ids.empty() ? nullptr : owner->ids.data(), owner->ids.size()};
            owner->info.element_group_labels = {owner->views.empty() ? nullptr : owner->views.data(), owner->views.size()};
            owner->va = std::move(completion.va);
            owner->claim = std::move(completion.claim);
            completion.claimed = true;
            completion.changed.notify_all();
            *out_va = owner.release();
            return AMS_MEL_OK;
        } catch (const std::bad_alloc&) {
            failure = AMS_MEL_INTERNAL_ERROR;
            try { failure_message = "VA snapshot allocation failed"; } catch (...) {}
        } catch (...) {
            failure = AMS_MEL_PROVIDER_EXCEPTION;
            try { failure_message = exception_text("VA getter exception", "unknown VA getter exception"); }
            catch (...) {}
        }
        if (getter_started) {
            completion.claim_failed = true;
            completion.claim_status = failure;
            completion.claim_message.swap(failure_message);
            failed_va = std::move(completion.va);
            failed_claim = std::move(completion.claim);
            completion.changed.notify_all();
        }
    } catch (...) { return AMS_MEL_INTERNAL_ERROR; }
    failed_va.reset();
    auto shutdown = failed_claim.release();
    if (shutdown.status != AMS_MEL_OK) {
        std::lock_guard lock{completion.mutex};
        completion.claim_failed = true;
        completion.claim_status = shutdown.status;
        completion.kind = shutdown.status == AMS_MEL_INTERNAL_ERROR ? Kind::InternalError : Kind::ProviderException;
        completion.message.swap(shutdown.message);
        try { completion.claim_message = completion.message; }
        catch (...) { completion.claim_message.clear(); }
        write_diagnostic(completion.claim_message, diagnostic, capacity, required);
        return shutdown.status;
    }
    if (!completion.claim_failed) {
        write_diagnostic("VA owner allocation failed before getter", diagnostic, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    write_diagnostic(completion.claim_message, diagnostic, capacity, required);
    return completion.claim_status;
}

extern "C" ams_mel_status_t ams_mel_rf_virtual_aperture_request_close(
    ams_mel_rf_virtual_aperture_request **request, char *diagnostic,
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
        /* A failed lock cannot prove abandonment. Keep the complete graph. */
        completion->emergency_self = completion;
        return AMS_MEL_INTERNAL_ERROR;
    }
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_rf_virtual_aperture_view(
    const ams_mel_rf_virtual_aperture *va,
    const ams_mel_rf_virtual_aperture_info_v1 **out_info, char *diagnostic,
    std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!va || !va->va || !out_info || *out_info || bad_diag(diagnostic, capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    *out_info = &va->info;
    return AMS_MEL_OK;
}
extern "C" ams_mel_status_t ams_mel_rf_virtual_aperture_close(
    ams_mel_rf_virtual_aperture **va, char *diagnostic,
    std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!va || bad_diag(diagnostic, capacity)) return AMS_MEL_INVALID_ARGUMENT;
    auto owned = std::exchange(*va, nullptr);
    if (!owned) return AMS_MEL_OK;
    auto registration = std::move(owned->registration);
    if (registration) remove_va_callback(*owned->va, *registration);
    auto claim = std::move(owned->claim);
    delete owned; /* VA destroyed while parent and DSO still held by claim */
    const auto outcome = claim.release();
    if (registration && registration->removal_status != AMS_MEL_OK) {
        if (outcome.status != AMS_MEL_OK) {
            /* Fixed storage after cleanup; neither allocation nor truncation can
             * prevent provider destruction or cause a second removal attempt. */
            std::array<char, 1024> combined{};
            write_diagnostic(outcome.message, combined.data(), 512, nullptr);
            auto length = std::strlen(combined.data());
            constexpr std::string_view separator{"; removal: "};
            std::memcpy(combined.data() + length, separator.data(), separator.size());
            length += separator.size();
            write_diagnostic(registration->diagnostic.data(), combined.data() + length,
                             combined.size() - length, nullptr);
            write_diagnostic(combined.data(), diagnostic, capacity, required);
            return outcome.status;
        }
        write_diagnostic(registration->diagnostic.data(), diagnostic, capacity, required);
        return registration->removal_status;
    }
    write_diagnostic(outcome.message, diagnostic, capacity, required);
    return outcome.status;
}

extern "C" ams_mel_status_t ams_mel_rf_va_status_subscription_open(
    ams_mel_rf_virtual_aperture *va, ams_mel_rf_va_status_subscription **output,
    char *diagnostic, std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!va || !va->va || !output || *output || bad_diag(diagnostic, capacity)) return AMS_MEL_INVALID_ARGUMENT;
    if (va->registration) {
        write_diagnostic("VA subscription registration already attempted", diagnostic, capacity, required);
        return AMS_MEL_PROVIDER_FAILED;
    }
    try {
        if (failpoint("subscription-owner")) throw std::bad_alloc{};
        auto owner = std::make_unique<ams_mel_rf_va_status_subscription>();
        if (failpoint("subscription-state")) throw std::bad_alloc{};
        owner->state = std::make_shared<VaSignalState>();
        if (failpoint("subscription-control")) throw std::bad_alloc{};
        auto registration = std::make_unique<VaRegistration>();
        registration->state = owner->state;
        if (failpoint("subscription-callable")) throw std::bad_alloc{};
        auto shell = prepare_va_callable(owner->state, va->claim.library_pin());
        if (failpoint("subscription-retention")) throw std::bad_alloc{};
        /* Every allocation precedes exposure. Nonallocating retention consumes
         * the attempt before provider code; synchronous callbacks are supported. */
        va->registration = std::move(registration);
        auto *stable = shell.release();
        retain_va_callable(stable);
        try {
            va->registration->key = va->va->addStatusCallback(stable->callback);
            va->registration->key_known = true;
        } catch (...) { stop_va_signal(owner->state); throw; }
        *output = owner.release();
        return AMS_MEL_OK;
    } catch (...) {
        return translate_provider_exception("VA callback registration exception",
            "unknown VA callback registration exception", diagnostic, capacity, required);
    }
}
extern "C" ams_mel_status_t ams_mel_rf_va_status_subscription_unsubscribe(
    ams_mel_rf_virtual_aperture *va, ams_mel_rf_va_status_subscription *owner,
    char *diagnostic, std::size_t capacity, std::size_t *required) noexcept
{
    clear_diagnostic(diagnostic, capacity, required);
    if (!va || !va->va || !owner || !owner->state || !va->registration ||
        va->registration->state != owner->state || bad_diag(diagnostic, capacity)) return AMS_MEL_INVALID_ARGUMENT;
    remove_va_callback(*va->va, *va->registration);
    write_diagnostic(va->registration->diagnostic.data(), diagnostic, capacity, required);
    return va->registration->removal_status;
}
