#pragma once

#include "../internal.hpp"
#include "completion_probe.hpp"

#include <ams_mel/abi.h>
#include <irmel/library/irmel-types/Channel.h>

#include <atomic>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <utility>

struct CommsMetadataState;

namespace ams_mel_common {

class CommonRequestClaim;
/* An idle view holds no provider or Session owner. Typed adapters live only in
 * their respective family translation units. The view is exactly one weak
 * family state plus static function pointers; it never strongly owns family
 * state, a Session, the provider channel, Control, or the provider library. */
class CommonChannelAccess {
public:
    using AdmissionFunction = std::shared_ptr<CompletionAdmission> (*)(const std::shared_ptr<void>&) noexcept;
    using ClaimFunction = bool (*)(const std::shared_ptr<void>&,
        std::shared_ptr<ams::iface::irmel::Channel>&, CommonRequestClaim&);
    /* Invoked only with a temporarily locked, live family state. It applies
     * the family's own typed capability lifecycle, locking, and status
     * mapping, and returns an independent capability snapshot. */
    using CapabilityFunction = ams_mel_status_t (*)(const std::shared_ptr<void>&,
        ams_mel_ir_channel_capability **, char *, std::size_t, std::size_t *) noexcept;
    using MetadataFunction = bool (*)(const std::shared_ptr<void>&,
        const std::shared_ptr<CommsMetadataState>&);
    using DisableFunction = ams_mel_status_t (*)(const std::shared_ptr<void>&,
        ams_mel_ir_return_t *, char *, std::size_t, std::size_t *) noexcept;
    using BufferFunction = ams_mel_status_t (*)(const std::shared_ptr<void>&,
        ams_mel_ir_buffer&, bool, ams_mel_ir_return_t *) noexcept;
    /* Optional family-local one-shot publication, called while a temporary
     * claim retains the graph. C2 deliberately has no adapter here: its common
     * callback belongs exclusively to the existing C2 metadata queue. */
    CommonChannelAccess(std::weak_ptr<void> state, AdmissionFunction admission,
                        ClaimFunction claim, CapabilityFunction capability,
                        MetadataFunction metadata = nullptr,
                        DisableFunction disable = nullptr,
                        BufferFunction buffer = nullptr) noexcept
        : state_{std::move(state)}, admission_{admission}, claim_{claim},
          capability_{capability}, metadata_{metadata}, disable_{disable}, buffer_{buffer} {}
    ams_mel_status_t buffer(const std::shared_ptr<void>& state,
        ams_mel_ir_buffer& object, bool removing, ams_mel_ir_return_t *result) const noexcept
    { return buffer_ ? buffer_(state, object, removing, result) : AMS_MEL_PROVIDER_FAILED; }
    ams_mel_status_t disable(const std::shared_ptr<void>& state,
        ams_mel_ir_return_t *result, char *out, std::size_t capacity,
        std::size_t *required) const noexcept
    { return disable_ ? disable_(state, result, out, capacity, required) : AMS_MEL_PROVIDER_FAILED; }
    bool expired() const noexcept { return state_.expired(); }
    std::shared_ptr<void> lock() const noexcept { return state_.lock(); }
    std::shared_ptr<CompletionAdmission> admission(const std::shared_ptr<void>& state) const noexcept
    { return admission_(state); }
    bool claim(const std::shared_ptr<void>& state,
               std::shared_ptr<ams::iface::irmel::Channel>& channel,
               CommonRequestClaim& request) const { return claim_(state, channel, request); }
    ams_mel_status_t capability(const std::shared_ptr<void>& state,
        ams_mel_ir_channel_capability **output, char *out, std::size_t capacity,
        std::size_t *required) const noexcept
    { return capability_(state, output, out, capacity, required); }
    bool metadata(const std::shared_ptr<void>& state,
                  const std::shared_ptr<CommsMetadataState>& queue) const
    { return metadata_ && metadata_(state, queue); }
private:
    std::weak_ptr<void> state_;
    AdmissionFunction admission_;
    ClaimFunction claim_;
    CapabilityFunction capability_;
    MetadataFunction metadata_;
    DisableFunction disable_;
    BufferFunction buffer_{};
};
static_assert(std::is_nothrow_move_constructible_v<CommonChannelAccess>);

/* Construction is allocation-free after family accounting has been reserved.
 * The finish adapter must not throw. A failed cleanup may retain the graph
 * itself; dropping the claim never implies that provider teardown succeeded. */
class CommonRequestClaim {
public:
    using FinishFunction = bool (*)(const std::shared_ptr<void>&) noexcept;

    CommonRequestClaim() noexcept = default;
    CommonRequestClaim(std::shared_ptr<void> state, FinishFunction finish,
                       const char *failure_message) noexcept
        : state_{std::move(state)}, finish_{finish}, failure_message_{failure_message} {}
    CommonRequestClaim(const CommonRequestClaim&) = delete;
    CommonRequestClaim& operator=(const CommonRequestClaim&) = delete;
    CommonRequestClaim(CommonRequestClaim&& other) noexcept
        : state_{std::move(other.state_)}, finish_{std::exchange(other.finish_, nullptr)},
          failure_message_{std::exchange(other.failure_message_, nullptr)} {}
    CommonRequestClaim& operator=(CommonRequestClaim&& other) noexcept
    {
        if (this != &other) {
            (void)finish();
            state_ = std::move(other.state_);
            finish_ = std::exchange(other.finish_, nullptr);
            failure_message_ = std::exchange(other.failure_message_, nullptr);
        }
        return *this;
    }
    ~CommonRequestClaim() noexcept { (void)finish(); }

    bool finish() noexcept
    {
        if (!state_) return true;
        auto state = std::move(state_);
        const auto function = std::exchange(finish_, nullptr);
        failure_message_ = nullptr;
        return function(state);
    }
    bool empty() const noexcept { return !state_; }
    const char *failure_message() const noexcept { return failure_message_; }

private:
    std::shared_ptr<void> state_;
    FinishFunction finish_{};
    const char *failure_message_{};
};

static_assert(!std::is_copy_constructible_v<CommonRequestClaim>);
static_assert(!std::is_copy_assignable_v<CommonRequestClaim>);
static_assert(std::is_nothrow_move_constructible_v<CommonRequestClaim>);
static_assert(std::is_nothrow_move_assignable_v<CommonRequestClaim>);

enum class CompletionKind { Pending, Success, Rejected, ProviderException, ProviderFailure, InternalError };

struct ReturnCompletion {
    std::mutex mutex;
    std::condition_variable ready;
    CompletionKind kind{CompletionKind::Pending};
    ams_mel_ir_return_result_v1 result{};
    std::string message;
    CommonRequestClaim claim;
};

struct ReturnWorkerInput {
    AMS_MEL_PROBE_OWNER(Return)
    CompletionPermit admission;
    std::shared_ptr<ReturnCompletion> completion;
    ams::iface::mel::RequestFor<ams::iface::irmel::Return> future;
    std::shared_ptr<ReturnWorkerInput> emergency_self;
    ReturnWorkerInput *emergency_next{};
    std::atomic<bool> emergency_retained{};
    std::atomic<unsigned> launch_state{};
};

struct CommsCompletion {
    std::mutex mutex;
    std::condition_variable ready;
    CompletionKind kind{CompletionKind::Pending};
    ams_mel_ir_channel_comms_test_result_v1 result{};
    std::string message;
    CommonRequestClaim claim;
};

struct CommsWorkerInput {
    AMS_MEL_PROBE_OWNER(Comms)
    CompletionPermit admission;
    std::shared_ptr<CommsCompletion> completion;
    ams::iface::mel::RequestFor<ams::iface::irmel::ChannelCommsTestRep> future;
    std::shared_ptr<CommsWorkerInput> emergency_self;
    CommsWorkerInput *emergency_next{};
    std::atomic<bool> emergency_retained{};
    std::atomic<unsigned> launch_state{};
};

void arm_return_worker(const std::shared_ptr<ReturnWorkerInput>&) noexcept;
void retain_return_worker(const std::shared_ptr<ReturnWorkerInput>&) noexcept;
void run_return_worker(const std::shared_ptr<ReturnWorkerInput>&) noexcept;
void arm_comms_worker(const std::shared_ptr<CommsWorkerInput>&) noexcept;
void retain_comms_worker(const std::shared_ptr<CommsWorkerInput>&) noexcept;
void run_comms_worker(const std::shared_ptr<CommsWorkerInput>&) noexcept;
CommonChannelAccess common_from_c2(const ams_mel_ir_c2 *);
CommonChannelAccess common_from_stream(const ams_mel_ir_stream *);
CommonChannelAccess common_from_health(const ams_mel_ir_health *);
CommonChannelAccess common_from_instrumentation(const ams_mel_ir_instrumentation *);
CommonChannelAccess common_from_track(const ams_mel_ir_track *);
/* IRMEL-ADA-016: family-local lifetime adapter, not a second request engine. */
CommonChannelAccess common_from_stacked(const ams_mel_ir_stacked *);
/* IRMEL-ADA-011: Scheduling adapter; defined in ir_scheduling.cpp. */
CommonChannelAccess common_from_scheduling(const ams_mel_ir_scheduling *);
/* Typed commands share the common Return engine, but supply their own
 * lifecycle adapter and published-interface dispatch. Context is borrowed only
 * during this synchronous submission; no worker retains it. */
using ReturnSend = ams::iface::mel::RequestFor<ams::iface::irmel::Return> (*)(
    ams::iface::irmel::Channel&, const void *);
ams_mel_status_t submit_return(const CommonChannelAccess&, ReturnSend, const void *,
    ams_mel_ir_return_request **, char *, std::size_t, std::size_t *) noexcept;
ams_mel_status_t submit_common_keepalive(const CommonChannelAccess&,
    ams_mel_ir_return_request **, char *, std::size_t, std::size_t *) noexcept;
ams_mel_status_t submit_common_comms(const CommonChannelAccess&,
    const ams_mel_ir_channel_comms_test_request_v1 *,
    ams_mel_ir_channel_comms_request **, char *, std::size_t, std::size_t *) noexcept;
} // namespace ams_mel_common

struct ams_mel_ir_return_request {
    std::shared_ptr<ams_mel_common::ReturnCompletion> state;
};
struct ams_mel_ir_channel_comms_request {
    std::shared_ptr<ams_mel_common::CommsCompletion> state;
};
