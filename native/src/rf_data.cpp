/* Task 033B RF DataMEL foundation (parent lifecycle extended by Task 033D).
 *
 * RF owner graph (deliberately independent of the IR Session graph):
 *
 *     ams_mel_rf_data ---------+
 *     RfChildClaim (033D) -----+--shared_ptr--> RfDataState
 *                                                 library : shared_ptr<SharedLibrary>
 *                                                 data    : shared_ptr<rfmel::DataMEL>
 *
 * The DataMEL is always destroyed before RfDataState drops its SharedLibrary
 * reference. With no data-ready callback ever registered that reference is the
 * last one, so behavior is exactly Task 033B's: shutdown, destroy DataMEL,
 * unload DSO. A Task 033D permanent callback registration holds an additional
 * SharedLibrary reference for the process lifetime.
 * An ams_mel_rf_mfa_info snapshot owns plain copies only and references no
 * RfDataState, provider object, or provider memory. */
#include <ams_mel/abi.h>
#include "internal/provider_common.hpp"
#include "internal/rf_product_rx.hpp"
#include "internal/shared_library.hpp"

#include <rfmel/data/DataMEL.h>
#include <rfmel/factory/RFCreateFunctions.h>
#include <rfmel/mfa/RFMFAInfo.h>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <set>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace rfmel = ams::iface::rfmel;
using namespace ams_mel::internal;

namespace {

using Femtoseconds = ams::util::math::Femtoseconds;
static_assert(std::is_same_v<Femtoseconds::rep, std::int64_t>);
static_assert(std::is_integral_v<Femtoseconds::rep> && std::is_signed_v<Femtoseconds::rep>);
static_assert(sizeof(Femtoseconds::rep) == sizeof(std::int64_t));

/* The exact pinned rfmel::fnDataMEL type: C linkage, C++ signature. It is
 * called only from this translation unit and never exposed through C. */
using RfDataFactory =
    std::shared_ptr<rfmel::DataMEL> (*)(std::string_view);
static_assert(std::is_same_v<RfDataFactory, rfmel::fnDataMEL>);

/* shutdown() threw, so the DataMEL has no proven shutdown boundary: it may not
 * be destroyed and its DSO may not be unloaded. Keep the graph for the process
 * lifetime through an intrusive root. The self reference reuses the existing
 * control block and the list link is a raw pointer, so nothing allocates. */
void retain_forever(const std::shared_ptr<RfDataState>& state) noexcept
{
    static std::atomic<RfDataState *> retained{};
    bool expected = false;
    if (!state->emergency_retained.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) return;
    state->emergency_self = state;
    RfDataState *head = retained.load(std::memory_order_relaxed);
    do { state->emergency_next = head; }
    while (!retained.compare_exchange_weak(
        head, state.get(), std::memory_order_release, std::memory_order_relaxed));
}

enum class RfAllocationFailure { None, Open, SnapshotOwner, SnapshotRange };

/* Test-build-only deterministic allocation failpoints; compiled out of
 * production builds. */
RfAllocationFailure rf_allocation_failure() noexcept
{
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    const char *value = std::getenv("AMS_MEL_TEST_RF_ALLOCATION_FAILURE");
    if (value == nullptr) return RfAllocationFailure::None;
    if (std::strcmp(value, "open") == 0) return RfAllocationFailure::Open;
    if (std::strcmp(value, "snapshot-owner") == 0)
        return RfAllocationFailure::SnapshotOwner;
    if (std::strcmp(value, "snapshot-range") == 0)
        return RfAllocationFailure::SnapshotRange;
#endif
    return RfAllocationFailure::None;
}

/* size_t -> uint64_t without truncation. Always representable on LP64/ILP32;
 * the check exists for a platform whose size_t is wider than 64 bits. */
bool to_u64(std::size_t value, std::uint64_t& output) noexcept
{
    if constexpr (std::numeric_limits<std::size_t>::max() >
                  std::numeric_limits<std::uint64_t>::max()) {
        if (value > static_cast<std::size_t>(
                        std::numeric_limits<std::uint64_t>::max()))
            return false;
    }
    output = static_cast<std::uint64_t>(value);
    return true;
}

std::uint32_t raw_format(rfmel::JobDataFormat value) noexcept
{
    /* Modular integral conversion preserves the raw bits of any provider
     * value, including values upstream does not publish yet. */
    return static_cast<std::uint32_t>(
        static_cast<std::underlying_type_t<rfmel::JobDataFormat>>(value));
}

std::vector<ams_mel_rf_frequency_range_v1> copy_ranges(
    const std::vector<rfmel::FrequencyRange>& input)
{
    std::vector<ams_mel_rf_frequency_range_v1> output;
    output.reserve(input.size());
    for (const auto& range : input)
        output.push_back({range.getMinFrequency(), range.getMaxFrequency()});
    return output;
}

ams_mel_rf_frequency_range_span_v1 range_span(
    const std::vector<ams_mel_rf_frequency_range_v1>& storage) noexcept
{
    return {storage.empty() ? nullptr : storage.data(), storage.size()};
}

struct FaceRanges {
    std::vector<ams_mel_rf_frequency_range_v1> rx, tx, sample;
};

bool invalid_diagnostic(const char *diagnostic, std::size_t capacity) noexcept
{
    return diagnostic == nullptr && capacity != 0U;
}

} // namespace

namespace ams_mel::internal {

RfShutdownOutcome rf_finish_shutdown(const std::shared_ptr<RfDataState>& state) noexcept
{
    RfShutdownOutcome outcome;
    if (!state->data) return outcome;
    try {
        state->data->shutdown();
    } catch (...) {
        /* No retry, no DataMEL destruction, no DSO unload. Retention is
         * published before any diagnostic text is produced. */
        retain_forever(state);
        outcome.status = AMS_MEL_PROVIDER_EXCEPTION;
        try {
            throw;
        } catch (const std::exception& error) {
            const char *const what = error.what();
            const std::string_view text = what == nullptr ? std::string_view{}
                                                          : std::string_view{what};
            try {
                outcome.message = !text.empty() && valid_utf8(text)
                    ? std::string{text} : std::string{"provider shutdown exception"};
            } catch (...) { outcome.message.clear(); }
        } catch (...) {
            try { outcome.message = "unknown provider shutdown exception"; }
            catch (...) { outcome.message.clear(); }
        }
        return outcome;
    }
    /* shutdown() returned: destroy the DataMEL (its provider control block and
     * deleter run while the DSO is still loaded), then drop this state's
     * library reference. Destructors are not expected to throw; were one to,
     * this noexcept function terminates rather than unloading code under a
     * live graph. */
    state->data.reset();
    state->library.reset();
    return outcome;
}

bool RfChildClaim::acquire(const std::shared_ptr<RfDataState>& state,
                           RfChildClaim& output) noexcept
{
    if (!state || output.held()) return false;
    try {
        std::lock_guard lock{state->mutex};
        if (state->close_requested || state->shutdown_started) return false;
        ++state->children;
    } catch (...) {
        return false;
    }
    output.state_ = state;
    return true;
}

RfShutdownOutcome RfChildClaim::release() noexcept
{
    std::shared_ptr<RfDataState> state = std::move(state_);
    if (!state) return {};
    bool now = false;
    try {
        std::lock_guard lock{state->mutex};
        --state->children;
        if (state->children == 0U && state->close_requested && !state->shutdown_started) {
            state->shutdown_started = true;
            now = true;
        }
    } catch (...) {
        /* The child count could not be decremented, so the graph can never
         * reach a proven shutdown boundary: retain it permanently. */
        retain_forever(state);
        return {AMS_MEL_INTERNAL_ERROR, {}};
    }
    if (!now) return {};
    return rf_finish_shutdown(state);
}

} // namespace ams_mel::internal

/* Owns every byte reachable from `view`. The view is wired exactly once after
 * all vectors are final, and nothing is mutated afterwards. */
struct ams_mel_rf_mfa_info {
    ams_mel_rf_mfa_info_v1 view{};
    std::vector<std::uint32_t> formats;
    std::vector<ams_mel_rf_face_info_v1> faces;
    std::vector<FaceRanges> ranges;
};

extern "C" ams_mel_status_t ams_mel_rf_data_open(
    const char *library_path, const char *configuration,
    ams_mel_rf_data **out_data, char *diagnostic,
    std::size_t diagnostic_capacity, std::size_t *diagnostic_required) noexcept
{
    clear_diagnostic(diagnostic, diagnostic_capacity, diagnostic_required);
    if (library_path == nullptr || configuration == nullptr ||
        out_data == nullptr || *out_data != nullptr ||
        invalid_diagnostic(diagnostic, diagnostic_capacity)) {
        write_diagnostic("invalid argument", diagnostic, diagnostic_capacity,
                         diagnostic_required);
        return AMS_MEL_INVALID_ARGUMENT;
    }

    /* Shared so a Task 033D permanent callback registration can hold its own
     * reference; with no registration this is the only reference. */
    std::shared_ptr<SharedLibrary> library;
    try {
        library = std::make_shared<SharedLibrary>(library_path);
    } catch (const std::bad_alloc&) {
        write_diagnostic("allocation failed", diagnostic, diagnostic_capacity,
                         diagnostic_required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (const std::exception& error) {
        write_exception_diagnostic(error, "library load failed", diagnostic,
                                   diagnostic_capacity, diagnostic_required);
        return AMS_MEL_LIBRARY_LOAD_FAILED;
    } catch (...) {
        write_diagnostic("unknown library-load exception", diagnostic,
                         diagnostic_capacity, diagnostic_required);
        return AMS_MEL_LIBRARY_LOAD_FAILED;
    }

    RfDataFactory factory{};
    try {
        factory = library->symbol<RfDataFactory>("createDataMEL");
    } catch (const std::bad_alloc&) {
        write_diagnostic("allocation failed", diagnostic, diagnostic_capacity,
                         diagnostic_required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (const std::exception& error) {
        write_exception_diagnostic(error, "symbol resolution failed", diagnostic,
                                   diagnostic_capacity, diagnostic_required);
        return AMS_MEL_SYMBOL_NOT_FOUND;
    } catch (...) {
        write_diagnostic("unknown symbol-resolution exception", diagnostic,
                         diagnostic_capacity, diagnostic_required);
        return AMS_MEL_SYMBOL_NOT_FOUND;
    }

    /* Allocate every façade object BEFORE the provider object exists, so no
     * failure can occur between a successful factory call and publication. */
    std::shared_ptr<RfDataState> state;
    std::unique_ptr<ams_mel_rf_data> owner;
    try {
        if (rf_allocation_failure() == RfAllocationFailure::Open)
            throw std::bad_alloc{};
        state = std::make_shared<RfDataState>();
        owner = std::make_unique<ams_mel_rf_data>();
    } catch (...) {
        write_diagnostic("allocation failed", diagnostic, diagnostic_capacity,
                         diagnostic_required);
        return AMS_MEL_INTERNAL_ERROR;
    }

    /* Declared after `library`: on every early return below the DataMEL (if
     * any) is destroyed before the DSO is unloaded. A factory exception
     * object is destroyed when its handler exits, also before `library`. */
    std::shared_ptr<rfmel::DataMEL> data;
    try {
        data = factory(std::string_view{configuration});
    } catch (...) {
        return translate_provider_exception(
            "provider factory exception", "unknown provider factory exception",
            diagnostic, diagnostic_capacity, diagnostic_required);
    }
    if (!data) {
        write_diagnostic("createDataMEL returned null", diagnostic,
                         diagnostic_capacity, diagnostic_required);
        return AMS_MEL_FACTORY_FAILED;
    }

    state->library = std::move(library);
    state->data = std::move(data);
    owner->state = std::move(state);
    *out_data = owner.release();
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_rf_data_get_provider_version(
    const ams_mel_rf_data *data, ams_mel_provider_version_v1 *out_version,
    char *diagnostic, std::size_t diagnostic_capacity,
    std::size_t *diagnostic_required) noexcept
{
    clear_diagnostic(diagnostic, diagnostic_capacity, diagnostic_required);
    if (data == nullptr || !data->state || !data->state->data ||
        invalid_diagnostic(diagnostic, diagnostic_capacity) ||
        !valid_provider_version_output(out_version)) {
        write_diagnostic("invalid argument", diagnostic, diagnostic_capacity,
                         diagnostic_required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    try {
        const ams::iface::mel::VersionInfo value =
            data->state->data->getVersionInfo();
        return publish_provider_version(value, out_version, diagnostic,
                                        diagnostic_capacity,
                                        diagnostic_required);
    } catch (...) {
        return translate_provider_exception(
            "provider exception", "unknown provider exception", diagnostic,
            diagnostic_capacity, diagnostic_required);
    }
}

extern "C" ams_mel_status_t ams_mel_rf_data_quantize_duration(
    const ams_mel_rf_data *data, std::int64_t unquantized_femtoseconds,
    std::int64_t *out_quantized_femtoseconds, char *diagnostic,
    std::size_t diagnostic_capacity, std::size_t *diagnostic_required) noexcept
{
    clear_diagnostic(diagnostic, diagnostic_capacity, diagnostic_required);
    if (data == nullptr || !data->state || !data->state->data ||
        out_quantized_femtoseconds == nullptr ||
        invalid_diagnostic(diagnostic, diagnostic_capacity)) {
        write_diagnostic("invalid argument", diagnostic, diagnostic_capacity,
                         diagnostic_required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    try {
        const rfmel::RFMFAInfo& info = data->state->data->getRFMFAInfo();
        const Femtoseconds result = info.quantizeDuration(Femtoseconds{unquantized_femtoseconds});
        *out_quantized_femtoseconds = result.count();
        return AMS_MEL_OK;
    } catch (...) {
        return translate_provider_exception(
            "provider quantizeDuration exception", "unknown provider quantizeDuration exception",
            diagnostic, diagnostic_capacity, diagnostic_required);
    }
}

extern "C" ams_mel_status_t ams_mel_rf_data_get_mfa_info(
    const ams_mel_rf_data *data, ams_mel_rf_mfa_info **out_info,
    char *diagnostic, std::size_t diagnostic_capacity,
    std::size_t *diagnostic_required) noexcept
{
    clear_diagnostic(diagnostic, diagnostic_capacity, diagnostic_required);
    if (data == nullptr || !data->state || !data->state->data ||
        out_info == nullptr || *out_info != nullptr ||
        invalid_diagnostic(diagnostic, diagnostic_capacity)) {
        write_diagnostic("invalid argument", diagnostic, diagnostic_capacity,
                         diagnostic_required);
        return AMS_MEL_INVALID_ARGUMENT;
    }

    try {
        const auto failure = rf_allocation_failure();
        if (failure == RfAllocationFailure::SnapshotOwner) throw std::bad_alloc{};
        auto owner = std::make_unique<ams_mel_rf_mfa_info>();
        /* A provider-owned reference; it never escapes this call. */
        const rfmel::RFMFAInfo& info = data->state->data->getRFMFAInfo();

        std::uint64_t reported_num_faces{};
        if (!to_u64(info.getNumFaces(), reported_num_faces)) {
            write_diagnostic("provider getNumFaces value exceeds uint64_t",
                             diagnostic, diagnostic_capacity, diagnostic_required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        const bool open_additions = info.containsOpenAdditions();
        const std::int64_t resolution = info.schedulerResolution().count();
        std::uint64_t max_context_bytes{};
        if (!to_u64(info.getMaxNumUserDefinedContextBytes(), max_context_bytes)) {
            write_diagnostic(
                "provider getMaxNumUserDefinedContextBytes value exceeds uint64_t",
                diagnostic, diagnostic_capacity, diagnostic_required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        const std::set<rfmel::JobDataFormat> formats = info.getSupportedDataFormats();
        owner->formats.reserve(formats.size());
        for (const auto format : formats) owner->formats.push_back(raw_format(format));

        const std::set<rfmel::FaceID> face_ids = info.getFaceIDs();
        owner->faces.reserve(face_ids.size());
        owner->ranges.reserve(face_ids.size());
        for (const rfmel::FaceID face : face_ids) {
            ams_mel_rf_face_info_v1 record{};
            record.face_id = face;
            record.supports_receive = info.supportsReceive(face) ? 1U : 0U;
            record.supports_transmit = info.supportsTransmit(face) ? 1U : 0U;
            record.requires_endpoint_association =
                info.requiresEndpointAssociation(face) ? 1U : 0U;
            record.agc_processing_time_fs = info.getAGCProcessingTime(face).count();
            record.min_job_request_lead_time_fs = info.minJobRequestLeadTime(face).count();
            record.max_job_request_lead_time_fs = info.maxJobRequestLeadTime(face).count();
            record.min_job_detail_lead_time_fs = info.minJobDetailLeadTime(face).count();
            record.tx_rx_switching_time_fs = info.txRxSwitchingTime(face).count();
            record.rx_tx_switching_time_fs = info.rxTxSwitchingTime(face).count();
            record.tx_tx_switching_time_fs = info.txTxSwitchingTime(face).count();
            record.rx_rx_switching_time_fs = info.rxRxSwitchingTime(face).count();
            FaceRanges copied;
            copied.rx = copy_ranges(info.getRxFrequencyRanges(face));
            /* Deterministic nested OOM after range data was already copied. */
            if (failure == RfAllocationFailure::SnapshotRange && !owner->ranges.empty())
                throw std::bad_alloc{};
            copied.tx = copy_ranges(info.getTxFrequencyRanges(face));
            copied.sample = copy_ranges(info.getSampleFrequencyRange(face));
            owner->faces.push_back(record);
            owner->ranges.push_back(std::move(copied));
        }

        /* Every backing vector is final (capacity reserved, no later
         * insertion): wire the spans exactly once, then publish. */
        for (std::size_t index = 0; index < owner->faces.size(); ++index) {
            auto& record = owner->faces[index];
            const auto& ranges = owner->ranges[index];
            record.rx_frequency_ranges = range_span(ranges.rx);
            record.tx_frequency_ranges = range_span(ranges.tx);
            record.sample_frequency_ranges = range_span(ranges.sample);
        }
        owner->view.reported_num_faces = reported_num_faces;
        owner->view.contains_open_additions = open_additions ? 1U : 0U;
        owner->view.scheduler_resolution_fs = resolution;
        owner->view.max_user_defined_context_bytes = max_context_bytes;
        owner->view.supported_data_formats = {
            owner->formats.empty() ? nullptr : owner->formats.data(),
            owner->formats.size()};
        owner->view.faces = {owner->faces.empty() ? nullptr : owner->faces.data(),
                             owner->faces.size()};
        *out_info = owner.release();
        return AMS_MEL_OK;
    } catch (...) {
        return translate_provider_exception(
            "provider RFMFAInfo exception", "unknown provider RFMFAInfo exception",
            diagnostic, diagnostic_capacity, diagnostic_required);
    }
}

extern "C" ams_mel_status_t ams_mel_rf_mfa_info_view(
    const ams_mel_rf_mfa_info *info, const ams_mel_rf_mfa_info_v1 **out_view,
    char *diagnostic, std::size_t diagnostic_capacity,
    std::size_t *diagnostic_required) noexcept
{
    clear_diagnostic(diagnostic, diagnostic_capacity, diagnostic_required);
    if (info == nullptr || out_view == nullptr ||
        invalid_diagnostic(diagnostic, diagnostic_capacity)) {
        write_diagnostic("invalid argument", diagnostic, diagnostic_capacity,
                         diagnostic_required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    *out_view = &info->view;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_rf_mfa_info_close(
    ams_mel_rf_mfa_info **info, char *diagnostic,
    std::size_t diagnostic_capacity, std::size_t *diagnostic_required) noexcept
{
    clear_diagnostic(diagnostic, diagnostic_capacity, diagnostic_required);
    if (info == nullptr || invalid_diagnostic(diagnostic, diagnostic_capacity)) {
        write_diagnostic("invalid argument", diagnostic, diagnostic_capacity,
                         diagnostic_required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    /* Owns only façade vectors of trivial records; runs no provider code. */
    delete std::exchange(*info, nullptr);
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_rf_data_close(
    ams_mel_rf_data **data, char *diagnostic,
    std::size_t diagnostic_capacity, std::size_t *diagnostic_required) noexcept
{
    clear_diagnostic(diagnostic, diagnostic_capacity, diagnostic_required);
    if (data == nullptr || invalid_diagnostic(diagnostic, diagnostic_capacity)) {
        write_diagnostic("invalid argument", diagnostic, diagnostic_capacity,
                         diagnostic_required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    /* Logical Close: consume the public owner before any provider call, so no
     * further provider operation can be initiated through it. */
    ams_mel_rf_data *const owned = std::exchange(*data, nullptr);
    if (owned == nullptr) return AMS_MEL_OK;
    std::shared_ptr<RfDataState> state = std::move(owned->state);
    delete owned;
    if (!state || !state->data) return AMS_MEL_OK;

    /* Task 033D parent-first lifetime. With live create-request/endpoint
     * children, Close only records the request: the final child release runs
     * the single deferred shutdown. Without children this is exactly 033B. */
    bool now = false;
    try {
        std::lock_guard lock{state->mutex};
        state->close_requested = true;
        if (state->children == 0U && !state->shutdown_started) {
            state->shutdown_started = true;
            now = true;
        }
    } catch (...) {
        /* The lifecycle could not be recorded, so no shutdown boundary can
         * ever be proven: retain the complete graph. */
        retain_forever(state);
        write_diagnostic("RF Data close lifecycle failure; provider graph retained",
                         diagnostic, diagnostic_capacity, diagnostic_required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    if (!now) return AMS_MEL_OK;

    const RfShutdownOutcome outcome = rf_finish_shutdown(state);
    if (outcome.status != AMS_MEL_OK) {
        write_diagnostic(outcome.message.empty()
                             ? std::string_view{"unknown provider shutdown exception"}
                             : std::string_view{outcome.message},
                         diagnostic, diagnostic_capacity, diagnostic_required);
    }
    return outcome.status;
}
