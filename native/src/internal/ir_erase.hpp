#pragma once

/* IRMEL-ADA-007: same C2 claim/permit/worker ownership as the other C2 requests.
 * The observer never owns provider execution. No metadata callback is defined. */

#include "../internal.hpp"
#include "common_channel_requests.hpp"
#include "completion_probe.hpp"

#include <ams_mel/abi.h>
#include <irmel/library/c2/EraseCommand.h>
#include <irmel/library/irmel-types/Channel.h>
#include <mel/library/CommonMEL.h>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>

struct EraseCompletion {
    std::mutex mutex;
    std::condition_variable ready;
    ams_mel_common::CompletionKind kind{ams_mel_common::CompletionKind::Pending};
    ams_mel_ir_erase_result_v1 result{};
    std::string message;
    ams_mel_common::CommonRequestClaim claim;
};

/* Member order is the lifetime proof: the probe owner is destroyed last,
 * after the future, the completion graph and the admission permit. */
struct EraseWorkerInput {
    AMS_MEL_PROBE_OWNER(Erase)
    CompletionPermit admission;
    std::shared_ptr<EraseCompletion> completion;
    ams::iface::mel::RequestFor<ams::iface::irmel::EraseCommandType> future;
    std::shared_ptr<EraseWorkerInput> emergency_self;
    EraseWorkerInput *emergency_next{};
    std::atomic<bool> emergency_retained{};
    std::atomic<unsigned> launch_state{};
};

struct ams_mel_ir_erase_request {
    std::shared_ptr<EraseCompletion> state;
};

namespace ams_mel_erase {

/* Erased family adapter, identical in contract to the IRMEL-ADA-003 camera
 * adapter. claim runs once, before provider code, and must either refuse
 * without side effects or (under the C2 lock) validate Enabled, copy the
 * typed channel, account one request and publish a CommonRequestClaim whose
 * finish function releases exactly that request. send performs the typed
 * provider call with no bridge lock held. */
struct Family {
    std::shared_ptr<CompletionAdmission> (*admission)(const void *context) noexcept;
    bool (*claim)(const void *context, std::shared_ptr<void>& channel,
                  ams_mel_common::CommonRequestClaim& claim);
    ams::iface::mel::RequestFor<ams::iface::irmel::EraseCommandType> (*send)(
        const std::shared_ptr<void>& channel, ams::iface::irmel::EraseCommand command);
    const char *ineligible;
};

/* Reject unknown alternatives before provider send. */
bool convert_command(const ams_mel_ir_erase_command_v1& input,
                     ams::iface::irmel::EraseCommand& output) noexcept;
bool convert_response(const ams::iface::irmel::EraseCommandType& input,
                      ams_mel_ir_erase_type_t& output) noexcept;

ams_mel_status_t submit(const Family& family, const void *context,
                        const ams_mel_ir_erase_command_v1 *command,
                        ams_mel_ir_erase_request **output, char *out,
                        std::size_t capacity, std::size_t *required) noexcept;

void diagnostic(const char *text, char *out, std::size_t capacity,
                std::size_t *required) noexcept;

} // namespace ams_mel_erase
