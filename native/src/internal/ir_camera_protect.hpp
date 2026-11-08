#pragma once

/* IRMEL-ADA-004 private C2 CameraProtectCmd / CameraProtectCmdResp support.
 *
 * The request engine is the established request architecture, unchanged:
 * Session CompletionPermit, a C2 lifetime claim taken under the C2 lock, an
 * unlocked provider send, a detached completion worker that is the only
 * future::get() caller, and the allocation-free emergency retention path. The
 * C2 adapter (lifecycle rule + channel copy + CommonRequestClaim + typed send)
 * lives in ir_c2.cpp beside the private ChannelState.
 *
 * The opt-in CameraProtectCmdResp owner follows the proven IRMEL-ADA-002 NUC
 * shape: provider callbacks only touch CameraProtectState through the
 * shared_ptr captured by the registered closure, count themselves in flight
 * under the state mutex, and C2 physical cleanup waits for them to return
 * after the provider channel has been released. */

#include "../internal.hpp"
#include "common_channel_requests.hpp"
#include "completion_probe.hpp"

#include <ams_mel/abi.h>
#include <irmel/library/c2/CameraProtectCmd.h>
#include <irmel/library/irmel-types/Channel.h>
#include <mel/library/CommonMEL.h>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <string>

struct CameraProtectCompletion {
    std::mutex mutex;
    std::condition_variable ready;
    ams_mel_common::CompletionKind kind{ams_mel_common::CompletionKind::Pending};
    ams_mel_ir_camera_protect_result_v1 result{};
    std::string message;
    ams_mel_common::CommonRequestClaim claim;
};

/* Member order is the lifetime proof: the probe owner is destroyed last,
 * after the future, the completion graph and the admission permit. */
struct CameraProtectWorkerInput {
    AMS_MEL_PROBE_OWNER(CameraProtect)
    CompletionPermit admission;
    std::shared_ptr<CameraProtectCompletion> completion;
    ams::iface::mel::RequestFor<ams::iface::irmel::CameraProtectCmdResp> future;
    std::shared_ptr<CameraProtectWorkerInput> emergency_self;
    CameraProtectWorkerInput *emergency_next{};
    std::atomic<bool> emergency_retained{};
    std::atomic<unsigned> launch_state{};
};

struct ams_mel_ir_camera_protect_request {
    std::shared_ptr<CameraProtectCompletion> state;
};

namespace ams_mel_camera_protect {

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
    ams::iface::mel::RequestFor<ams::iface::irmel::CameraProtectCmdResp> (*send)(
        const std::shared_ptr<void>& channel, ams::iface::irmel::CameraProtectCmd command);
    const char *ineligible;
};

/* Validates protect_flag in {0, 1} and copies every field. Any other value
 * returns false and nothing is constructed for sending. */
bool convert_command(const ams_mel_ir_camera_protect_command_v1& input,
                     ams::iface::irmel::CameraProtectCmd& output) noexcept;

/* Copies every CameraProtectCmdResp field. Returns false (and leaves output
 * unspecified) for a CmdResp value outside the three pinned alternatives. */
bool convert_response(const ams::iface::irmel::CameraProtectCmdResp& input,
                      ams_mel_ir_camera_protect_response_v1& output) noexcept;

ams_mel_status_t submit(const Family& family, const void *context,
                        const ams_mel_ir_camera_protect_command_v1 *command,
                        ams_mel_ir_camera_protect_request **output, char *out,
                        std::size_t capacity, std::size_t *required) noexcept;

void diagnostic(const char *text, char *out, std::size_t capacity,
                std::size_t *required) noexcept;

} // namespace ams_mel_camera_protect

enum class CameraProtectLifecycle { Active, Inactive, Stopped, Failed };

struct CameraProtectState {
    std::mutex mutex;
    std::condition_variable ready;
    std::condition_variable callbacks_done;
    std::deque<ams_mel_ir_camera_protect_response_v1> queue;
    std::size_t capacity{};
    ams_mel_ir_metadata_counters_v1 counters{};
    CameraProtectLifecycle lifecycle{CameraProtectLifecycle::Active};
    /* Bridge callbacks currently inside receive(); guarded by mutex. */
    std::size_t callbacks{};

    /* Provider callback entry point. Never throws. */
    void receive(const ams::iface::irmel::CameraProtectCmdResp *input) noexcept;
    /* Active -> Inactive (public close, failed registration, logical close). */
    void deactivate() noexcept;
    /* Uncertain provider ownership; Receive reports PROVIDER_FAILED. */
    void fail() noexcept;
    /* After the C2 provider channel is released: wait for in-flight bridge
     * callbacks, then stop. Must not be called from inside a callback. */
    void drain_and_stop() noexcept;
};

struct ams_mel_ir_camera_protect_metadata { std::shared_ptr<CameraProtectState> state; };
struct ams_mel_ir_camera_protect_event { ams_mel_ir_camera_protect_response_v1 view{}; };
