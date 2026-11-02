#pragma once

/* IRMEL-ADA-005 private C2 CalibrationConfigurationCmd / CalibrationConfiguration support.
 *
 * The request engine is the established request architecture, unchanged:
 * Session CompletionPermit, a C2 lifetime claim taken under the C2 lock, an
 * unlocked provider send, a detached completion worker that is the only
 * future::get() caller, and the allocation-free emergency retention path. The
 * C2 adapter (lifecycle rule + channel copy + CommonRequestClaim + typed send)
 * lives in ir_c2.cpp beside the private ChannelState.
 *
 * The opt-in CalibrationConfiguration owner follows the proven IRMEL-ADA-002 NUC
 * shape: provider callbacks only touch CalibrationConfigurationState through the
 * shared_ptr captured by the registered closure, count themselves in flight
 * under the state mutex, and C2 physical cleanup waits for them to return
 * after the provider channel has been released. */

#include "../internal.hpp"
#include "common_channel_requests.hpp"
#include "completion_probe.hpp"

#include <ams_mel/abi.h>
#include <irmel/library/c2/CalibrationConfigurationCmd.h>
#include <irmel/library/irmel-types/Channel.h>
#include <mel/library/CommonMEL.h>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ams_mel_calibration_configuration { struct Snapshot; }

struct CalibrationConfigurationCompletion {
    std::mutex mutex;
    std::condition_variable ready;
    ams_mel_common::CompletionKind kind{ams_mel_common::CompletionKind::Pending};
    ams_mel_ir_calibration_configuration_result_v1 result{};
    std::shared_ptr<const ams_mel_calibration_configuration::Snapshot> snapshot;
    std::string message;
    ams_mel_common::CommonRequestClaim claim;
};

/* Member order is the lifetime proof: the probe owner is destroyed last,
 * after the future, the completion graph and the admission permit. */
struct CalibrationConfigurationWorkerInput {
    AMS_MEL_PROBE_OWNER(CalibrationConfiguration)
    CompletionPermit admission;
    std::shared_ptr<CalibrationConfigurationCompletion> completion;
    ams::iface::mel::RequestFor<ams::iface::mel::CalibrationConfiguration> future;
    std::shared_ptr<CalibrationConfigurationWorkerInput> emergency_self;
    CalibrationConfigurationWorkerInput *emergency_next{};
    std::atomic<bool> emergency_retained{};
    std::atomic<unsigned> launch_state{};
};

struct ams_mel_ir_calibration_configuration_request {
    std::shared_ptr<CalibrationConfigurationCompletion> state;
};

namespace ams_mel_calibration_configuration {

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
    ams::iface::mel::RequestFor<ams::iface::mel::CalibrationConfiguration> (*send)(
        const std::shared_ptr<void>& channel, ams::iface::irmel::CalibrationConfigurationCmd command);
    const char *ineligible;
};

bool convert_command(const ams_mel_ir_calibration_configuration_command_v1& input,
                     ams::iface::irmel::CalibrationConfigurationCmd& output) noexcept;

struct Snapshot {
    ams::iface::mel::CalibrationConfiguration values;
    struct Nested {
        std::vector<ams_mel_string_view_v1> names;
        std::vector<ams_mel_uci_id_v1> components, capabilities;
    };
    std::vector<Nested> nested;
    std::vector<ams_mel_ir_subsystem_calibration_v1> entries;
    ams_mel_ir_calibration_configuration_v1 view{};
};
std::shared_ptr<const Snapshot> copy_configuration(
    const ams::iface::mel::CalibrationConfiguration& input);

ams_mel_status_t submit(const Family& family, const void *context,
                        const ams_mel_ir_calibration_configuration_command_v1 *command,
                        ams_mel_ir_calibration_configuration_request **output, char *out,
                        std::size_t capacity, std::size_t *required) noexcept;

void diagnostic(const char *text, char *out, std::size_t capacity,
                std::size_t *required) noexcept;

} // namespace ams_mel_calibration_configuration

enum class CalibrationConfigurationLifecycle { Active, Inactive, Stopped, Failed };

struct CalibrationConfigurationState {
    std::mutex mutex;
    std::condition_variable ready;
    std::condition_variable callbacks_done;
    std::deque<std::shared_ptr<const ams_mel_calibration_configuration::Snapshot>> queue;
    std::size_t capacity{};
    ams_mel_ir_metadata_counters_v1 counters{};
    CalibrationConfigurationLifecycle lifecycle{CalibrationConfigurationLifecycle::Active};
    /* Bridge callbacks currently inside receive(); guarded by mutex. */
    std::size_t callbacks{};

    /* Provider callback entry point. Never throws. */
    void receive(const ams::iface::mel::CalibrationConfiguration *input) noexcept;
    /* Active -> Inactive (public close, failed registration, logical close). */
    void deactivate() noexcept;
    /* Uncertain provider ownership; Receive reports PROVIDER_FAILED. */
    void fail() noexcept;
    /* After the C2 provider channel is released: wait for in-flight bridge
     * callbacks, then stop. Must not be called from inside a callback. */
    void drain_and_stop() noexcept;
};

struct ams_mel_ir_calibration_configuration_metadata { std::shared_ptr<CalibrationConfigurationState> state; };
struct ams_mel_ir_calibration_configuration_event {
    std::shared_ptr<const ams_mel_calibration_configuration::Snapshot> snapshot;
};
