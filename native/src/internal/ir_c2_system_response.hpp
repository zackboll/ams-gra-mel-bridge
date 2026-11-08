#pragma once

/* IRMEL-ADA-008: C2Channel::send(SystemTrackDataResponse) request engine. Same
 * C2 claim/permit/worker ownership as the other C2 requests (Erase pattern).
 * It owns no Track state and never calls the Track overload; only the pure
 * value conversion in system_track_data.hpp is shared with Track. */

#include "../internal.hpp"
#include "common_channel_requests.hpp"
#include "completion_probe.hpp"

#include <ams_mel/abi.h>
#include <irmel/library/irmel-types/Channel.h>
#include <irmel/library/irmel-types/CommandStatus.h>
#include <irmel/library/irmel-types/SystemTrackDataResponse.h>
#include <mel/library/CommonMEL.h>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>

struct C2SystemResponseCompletion {
    std::mutex mutex;
    std::condition_variable ready;
    ams_mel_common::CompletionKind kind{ams_mel_common::CompletionKind::Pending};
    ams_mel_ir_c2_system_response_result_v1 result{};
    std::string reason_text;
    std::string message;
    ams_mel_common::CommonRequestClaim claim;
};

/* Member order is the lifetime proof: the probe owner is destroyed last,
 * after the future, the completion graph and the admission permit. */
struct C2SystemResponseWorkerInput {
    AMS_MEL_PROBE_OWNER(C2SystemResponse)
    CompletionPermit admission;
    std::shared_ptr<C2SystemResponseCompletion> completion;
    ams::iface::mel::RequestFor<ams::iface::irmel::CommandStatus> future;
    std::shared_ptr<C2SystemResponseWorkerInput> emergency_self;
    C2SystemResponseWorkerInput *emergency_next{};
    std::atomic<bool> emergency_retained{};
    std::atomic<unsigned> launch_state{};
};

struct ams_mel_ir_c2_system_response_request {
    std::shared_ptr<C2SystemResponseCompletion> state;
};

namespace ams_mel_c2_system_response {

/* Erased C2 family adapter with the IRMEL-ADA-007 contract. claim runs once,
 * before provider code, and must either refuse without side effects or (under
 * the C2 lock) validate Enabled, copy the typed C2 channel, account one C2
 * request and publish a CommonRequestClaim whose finish function releases
 * exactly that request. send performs the typed provider call unlocked. */
struct Family {
    std::shared_ptr<CompletionAdmission> (*admission)(const void *context) noexcept;
    bool (*claim)(const void *context, std::shared_ptr<void>& channel,
                  ams_mel_common::CommonRequestClaim& claim);
    ams::iface::mel::RequestFor<ams::iface::irmel::CommandStatus> (*send)(
        const std::shared_ptr<void>& channel,
        ams::iface::irmel::SystemTrackDataResponse response);
    const char *ineligible;
};

ams_mel_status_t submit(const Family& family, const void *context,
                        const ams_mel_ir_system_track_data_response_v1 *response,
                        ams_mel_ir_c2_system_response_request **output, char *out,
                        std::size_t capacity, std::size_t *required) noexcept;

void diagnostic(const char *text, char *out, std::size_t capacity,
                std::size_t *required) noexcept;

} // namespace ams_mel_c2_system_response
