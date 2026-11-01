#pragma once

/* IRMEL-ADA-002 private NUC_TempData reception shared by the Image and Health
 * channel families. One copied-event queue implementation, two independent
 * registrations. Provider callbacks only ever touch NucState through the
 * shared_ptr captured by the registered closure, so a late callback after the
 * public owner, the channel or the stream is closed remains memory-safe. */

#include <ams_mel/abi.h>

#include <irmel/library/irmel-types/Channel.h>
#include <irmel/library/irmel-types/NUC_TempData.h>

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <new>
#include <string_view>
#include <vector>

struct NucEventData {
    ams_mel_ir_nuc_temp_data_event_v1 view{};
    std::vector<ams_mel_ir_temp_point_v1> points;
};

enum class NucLifecycle { Active, Inactive, Stopped, Failed };

struct NucState {
    std::mutex mutex;
    std::condition_variable ready;
    std::condition_variable callbacks_done;
    std::deque<std::unique_ptr<NucEventData>> queue;
    std::size_t capacity{};
    ams_mel_ir_metadata_counters_v1 counters{};
    NucLifecycle lifecycle{NucLifecycle::Active};
    /* Bridge callbacks currently inside receive(); guarded by mutex. */
    std::size_t callbacks{};

    /* Provider callback entry point. Never throws. */
    void receive(const ams::iface::irmel::NUC_TempData *input) noexcept;
    /* Active -> Inactive (public close or failed registration). */
    void deactivate() noexcept;
    /* Active -> Stopped (owning channel/stream stopped). */
    void stop() noexcept;
    /* Uncertain provider ownership; Receive reports PROVIDER_FAILED. */
    void fail() noexcept;
    /* After channel destruction: wait for in-flight bridge callbacks, then
     * stop. Must not be called from inside a NUC callback. */
    void drain_and_stop() noexcept;
};

struct ams_mel_ir_nuc_metadata { std::shared_ptr<NucState> state; };
struct ams_mel_ir_nuc_event { std::unique_ptr<NucEventData> data; };

namespace ams_mel::internal {

/* Copies every publicly readable field; nullptr for null input, an unknown
 * BandType/TempPointType value or a valueless PixelValue. May throw
 * std::bad_alloc. */
std::unique_ptr<NucEventData> copy_nuc_temp_data(
    const ams::iface::irmel::NUC_TempData *input);

inline void nuc_diagnostic(std::string_view text, char *out, std::size_t capacity,
                           std::size_t *required) noexcept
{
    if (required) *required = text.size() + 1U;
    if (out && capacity) {
        const std::size_t copied = std::min(text.size(), capacity - 1U);
        std::memcpy(out, text.data(), copied);
        out[copied] = '\0';
    }
}

inline bool nuc_map_return(ams::iface::irmel::Return input,
                           ams_mel_ir_return_t& value) noexcept
{
    using ams::iface::irmel::Return;
    switch (input) {
    case Return::Success: value = AMS_MEL_IR_RETURN_SUCCESS; return true;
    case Return::BadPointer: value = AMS_MEL_IR_RETURN_BAD_POINTER; return true;
    case Return::Fail: value = AMS_MEL_IR_RETURN_FAIL; return true;
    case Return::NotSupported: value = AMS_MEL_IR_RETURN_NOT_SUPPORTED; return true;
    case Return::NotImplemented: value = AMS_MEL_IR_RETURN_NOT_IMPLEMENTED; return true;
    }
    return false;
}

/* Registers the actual upstream NUC_TempData overload on an already claimed
 * state and, only on Success, publishes a public owner. The caller has
 * recorded the one-shot attempt. Provider code is entered without any bridge
 * lock held, so synchronous delivery during registration is safe. */
template<class ChannelType>
ams_mel_status_t open_nuc_owner(ChannelType& channel, const std::shared_ptr<NucState>& state,
                                ams_mel_ir_nuc_metadata **output,
                                ams_mel_ir_return_t *registration, char *out,
                                std::size_t capacity, std::size_t *required) noexcept
{
    namespace irmel = ams::iface::irmel;
    std::function<void(irmel::Channel&, const irmel::NUC_TempData *const)> callback;
    std::unique_ptr<ams_mel_ir_nuc_metadata> owner;
    try {
        callback = [state](irmel::Channel&, const irmel::NUC_TempData *const value) noexcept {
            state->receive(value);
        };
        owner = std::make_unique<ams_mel_ir_nuc_metadata>();
        owner->state = state;
    } catch (...) {
        state->deactivate();
        nuc_diagnostic("NUC_TempData metadata allocation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    irmel::Return result{};
    try {
        result = channel.registerMetadataCallback(std::move(callback));
    } catch (...) {
        state->deactivate();
        nuc_diagnostic("provider exception during NUC_TempData registration", out, capacity,
                       required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
    ams_mel_ir_return_t mapped = AMS_MEL_IR_RETURN_FAIL;
    const bool known = nuc_map_return(result, mapped);
    if (known && registration) *registration = mapped;
    if (!known || mapped != AMS_MEL_IR_RETURN_SUCCESS) {
        state->deactivate();
        nuc_diagnostic(known && mapped == AMS_MEL_IR_RETURN_NOT_SUPPORTED ?
                           "provider does not support optional NUC_TempData callback" :
                           "NUC_TempData callback registration failed",
                       out, capacity, required);
        return AMS_MEL_PROVIDER_FAILED;
    }
    *output = owner.release();
    return AMS_MEL_OK;
}

} // namespace ams_mel::internal
