#pragma once

/* IRMEL-ADA-003 private CameraCommand / CameraCommandResp support.
 *
 * One completion engine serves the two independent upstream sends
 * (C2Channel::send(CameraCommand) and ImageChannel::send(CameraCommand)). The
 * engine is the established request architecture: Session CompletionPermit,
 * a family lifetime claim taken under the family lock, an unlocked provider
 * send, a detached completion worker that is the only future::get() caller,
 * and the allocation-free emergency retention path. Each family supplies only
 * a claim adapter (lifecycle rule + channel copy + CommonRequestClaim) and a
 * typed send adapter; the C2 adapter lives in ir_c2.cpp beside its private
 * ChannelState, the Image adapter beside the shared ImageStreamState. */

#include "../internal.hpp"
#include "common_channel_requests.hpp"
#include "completion_probe.hpp"

#include <ams_mel/abi.h>
#include <irmel/library/irmel-types/CameraCommand.h>
#include <irmel/library/irmel-types/CameraCommandResp.h>
#include <irmel/library/irmel-types/Channel.h>
#include <mel/library/CommonMEL.h>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <string>

struct CameraCompletion {
    std::mutex mutex;
    std::condition_variable ready;
    ams_mel_common::CompletionKind kind{ams_mel_common::CompletionKind::Pending};
    ams_mel_ir_camera_command_result_v1 result{};
    std::string message;
    ams_mel_common::CommonRequestClaim claim;
};

/* Member order is the lifetime proof: the probe owner is destroyed last,
 * after the future, the completion graph and the admission permit. */
struct CameraWorkerInput {
    AMS_MEL_PROBE_OWNER(Camera)
    CompletionPermit admission;
    std::shared_ptr<CameraCompletion> completion;
    ams::iface::mel::RequestFor<ams::iface::irmel::CameraCommandResp> future;
    std::shared_ptr<CameraWorkerInput> emergency_self;
    CameraWorkerInput *emergency_next{};
    std::atomic<bool> emergency_retained{};
    std::atomic<unsigned> launch_state{};
};

struct ams_mel_ir_camera_command_request {
    std::shared_ptr<CameraCompletion> state;
};

namespace ams_mel_camera {

/* Erased family adapter. claim runs once, before provider code, and must
 * either refuse without side effects or (under the family lock) validate the
 * lifecycle, copy the typed channel, account one request and publish a
 * CommonRequestClaim whose finish function releases exactly that request.
 * It may throw only before accounting. send performs the typed provider call
 * with no bridge lock held. */
struct CameraFamily {
    std::shared_ptr<CompletionAdmission> (*admission)(const void *context) noexcept;
    bool (*claim)(const void *context, std::shared_ptr<void>& channel,
                  ams_mel_common::CommonRequestClaim& claim);
    ams::iface::mel::RequestFor<ams::iface::irmel::CameraCommandResp> (*send)(
        const std::shared_ptr<void>& channel, ams::iface::irmel::CameraCommand command);
    const char *ineligible;
};

/* Validates the exact supported domains (Staring/Scanning, both ScanDir
 * values) and copies every field. Reserved13 and unknown values return false
 * and nothing is constructed for sending. */
bool convert_command(const ams_mel_ir_camera_command_v1& input,
                     ams::iface::irmel::CameraCommand& output) noexcept;

/* Shared submission engine used by both public submit exports. */
ams_mel_status_t submit(const CameraFamily& family, const void *context,
                        const ams_mel_ir_camera_command_v1 *command,
                        ams_mel_ir_camera_command_request **output, char *out,
                        std::size_t capacity, std::size_t *required) noexcept;

void diagnostic(const char *text, char *out, std::size_t capacity,
                std::size_t *required) noexcept;

} // namespace ams_mel_camera

/* Opt-in Image CameraCommandResp owner state. Provider callbacks only touch
 * this through the shared_ptr captured by the registered closure, so a late
 * callback after owner, stream or Session closure remains memory-safe. */
enum class CameraResponseLifecycle { Active, Inactive, Stopped };

struct CameraResponseState {
    std::mutex mutex;
    std::condition_variable ready;
    std::deque<ams_mel_ir_camera_command_response_v1> queue;
    std::size_t capacity{};
    ams_mel_ir_metadata_counters_v1 counters{};
    CameraResponseLifecycle lifecycle{CameraResponseLifecycle::Active};

    /* Provider callback entry point. Never throws. */
    void receive(const ams::iface::irmel::CameraCommandResp *input) noexcept;
    /* Active -> Inactive (public close or failed registration). */
    void deactivate() noexcept;
    /* Active -> Stopped (owning Image stream stopped/closed). */
    void stop() noexcept;
};

struct ams_mel_ir_camera_response_metadata { std::shared_ptr<CameraResponseState> state; };
struct ams_mel_ir_camera_response_event { ams_mel_ir_camera_command_response_v1 view{}; };
