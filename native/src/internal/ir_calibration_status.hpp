#pragma once

/* IRMEL-ADA-006 private C2 CalibrationStatusCmd / CalibrationStatus support.
 *
 * The request engine is the established request architecture, unchanged:
 * Session CompletionPermit, a C2 lifetime claim taken under the C2 lock, an
 * unlocked provider send, a detached completion worker that is the only
 * future::get() caller, and the allocation-free emergency retention path. The
 * C2 adapter (lifecycle rule + channel copy + CommonRequestClaim + typed send)
 * lives in ir_c2.cpp beside the private ChannelState.
 *
 * The opt-in CalibrationStatus owner follows the IRMEL-ADA-005 shape: provider
 * callbacks only touch CalibrationStatusState through the shared_ptr captured
 * by the registered closure, count themselves in flight under the state mutex,
 * and C2 physical cleanup waits for them to return after the provider channel
 * has been released. It is independent of the CalibrationConfiguration owner. */

#include "../internal.hpp"
#include "common_channel_requests.hpp"
#include "completion_probe.hpp"

#include <ams_mel/abi.h>
#include <irmel/library/c2/CalibrationStatusCmd.h>
#include <irmel/library/irmel-types/Channel.h>
#include <mel/library/CalibrationStatus.h>
#include <mel/library/CommonMEL.h>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ams_mel_calibration_status { struct Snapshot; }

struct CalibrationStatusCompletion {
    std::mutex mutex;
    std::condition_variable ready;
    ams_mel_common::CompletionKind kind{ams_mel_common::CompletionKind::Pending};
    ams_mel_ir_calibration_status_result_v1 result{};
    std::shared_ptr<const ams_mel_calibration_status::Snapshot> snapshot;
    std::string message;
    ams_mel_common::CommonRequestClaim claim;
};

/* Member order is the lifetime proof: the probe owner is destroyed last,
 * after the future, the completion graph and the admission permit. */
struct CalibrationStatusWorkerInput {
    AMS_MEL_PROBE_OWNER(CalibrationStatus)
    CompletionPermit admission;
    std::shared_ptr<CalibrationStatusCompletion> completion;
    ams::iface::mel::RequestFor<ams::iface::mel::CalibrationStatus> future;
    std::shared_ptr<CalibrationStatusWorkerInput> emergency_self;
    CalibrationStatusWorkerInput *emergency_next{};
    std::atomic<bool> emergency_retained{};
    std::atomic<unsigned> launch_state{};
};

struct ams_mel_ir_calibration_status_request {
    std::shared_ptr<CalibrationStatusCompletion> state;
};

namespace ams_mel_calibration_status {

/* Erased family adapter, identical in contract to the IRMEL-ADA-005 adapter.
 * claim runs once, before provider code, and must either refuse without side
 * effects or (under the C2 lock) validate Enabled, copy the typed channel,
 * account one request and publish a CommonRequestClaim whose finish function
 * releases exactly that request. send performs the typed provider call with no
 * bridge lock held. */
struct Family {
    std::shared_ptr<CompletionAdmission> (*admission)(const void *context) noexcept;
    bool (*claim)(const void *context, std::shared_ptr<void>& channel,
                  ams_mel_common::CommonRequestClaim& claim);
    ams::iface::mel::RequestFor<ams::iface::mel::CalibrationStatus> (*send)(
        const std::shared_ptr<void>& channel, ams::iface::irmel::CalibrationStatusCmd command);
    const char *ineligible;
};

bool convert_command(const ams_mel_ir_calibration_status_command_v1& input,
                     ams::iface::irmel::CalibrationStatusCmd& output) noexcept;

/* Immutable deep copy. values owns every string; the C views below point into
 * values and into the view vectors, which are fully sized before any view is
 * published and never reallocated afterwards. */
struct Snapshot {
    ams::iface::mel::CalibrationStatus values;
    std::vector<ams_mel_uci_id_v1> active;
    std::vector<std::vector<ams_mel_ir_completed_calibration_item_v1>> items;
    std::vector<ams_mel_ir_completed_calibration_v1> completed;
    ams_mel_ir_calibration_status_v1 view{};
};
/* Returns null for invalid provider data (never remapped). May throw
 * std::bad_alloc; callers report that distinctly. */
std::shared_ptr<const Snapshot> copy_status(const ams::iface::mel::CalibrationStatus& input);

ams_mel_status_t submit(const Family& family, const void *context,
                        const ams_mel_ir_calibration_status_command_v1 *command,
                        ams_mel_ir_calibration_status_request **output, char *out,
                        std::size_t capacity, std::size_t *required) noexcept;

void diagnostic(const char *text, char *out, std::size_t capacity,
                std::size_t *required) noexcept;

} // namespace ams_mel_calibration_status

enum class CalibrationStatusLifecycle { Active, Inactive, Stopped, Failed };

struct CalibrationStatusState {
    std::mutex mutex;
    std::condition_variable ready;
    std::condition_variable callbacks_done;
    std::deque<std::shared_ptr<const ams_mel_calibration_status::Snapshot>> queue;
    std::size_t capacity{};
    ams_mel_ir_metadata_counters_v1 counters{};
    CalibrationStatusLifecycle lifecycle{CalibrationStatusLifecycle::Active};
    /* Bridge callbacks currently inside receive(); guarded by mutex. */
    std::size_t callbacks{};

    /* Provider callback entry point. Never throws. */
    void receive(const ams::iface::mel::CalibrationStatus *input) noexcept;
    /* Active -> Inactive (public close, failed registration, logical close). */
    void deactivate() noexcept;
    /* Uncertain provider ownership; Receive reports PROVIDER_FAILED. */
    void fail() noexcept;
    /* After the C2 provider channel is released: wait for in-flight bridge
     * callbacks, then stop. Must not be called from inside a callback. */
    void drain_and_stop() noexcept;
};

struct ams_mel_ir_calibration_status_metadata { std::shared_ptr<CalibrationStatusState> state; };
struct ams_mel_ir_calibration_status_event {
    std::shared_ptr<const ams_mel_calibration_status::Snapshot> snapshot;
};
