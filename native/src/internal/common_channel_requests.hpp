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

namespace ams_mel_common {

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
} // namespace ams_mel_common

struct ams_mel_ir_return_request {
    std::shared_ptr<ams_mel_common::ReturnCompletion> state;
};
struct ams_mel_ir_channel_comms_request {
    std::shared_ptr<ams_mel_common::CommsCompletion> state;
};
