#pragma once

/* IRMEL-ADA-010 private opt-in Image LineOfSightQuaternion reception.
 *
 * One independently registered owner per Image stream, never registered by
 * the four-kind Image metadata owner, the NUC_TempData owner, the
 * CameraCommandResp owner or the OpticalDistortionMap owner. Provider
 * callbacks only ever touch LosQuaternionState through the shared_ptr captured
 * by the registered closure, so a late callback after the public owner, the
 * stream or the Session is closed remains memory-safe. Every non-null payload
 * is copied by value before the provider callback returns; no provider-owned
 * object or borrowed pointer survives callback return. */

#include <ams_mel/abi.h>

#include <irmel/library/irmel-types/LineOfSightQuaternion.h>

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>

/* Event-owned storage: one fixed-size C record, no indirection. */
struct LosQuaternionEventData {
    ams_mel_ir_line_of_sight_quaternion_v1 view{};
};

enum class LosQuaternionLifecycle { Active, Inactive, Stopped, Failed };

struct LosQuaternionState {
    std::mutex mutex;
    std::condition_variable ready;
    std::condition_variable callbacks_done;
    std::deque<std::unique_ptr<LosQuaternionEventData>> queue;
    std::size_t capacity{};
    ams_mel_ir_metadata_counters_v1 counters{};
    LosQuaternionLifecycle lifecycle{LosQuaternionLifecycle::Active};
    /* Bridge callbacks currently inside receive(); guarded by mutex. */
    std::size_t callbacks{};

    /* Provider callback entry point. Never throws. */
    void receive(const ams::iface::irmel::LineOfSightQuaternion *input) noexcept;
    /* Active -> Inactive (public close or failed registration). */
    void deactivate() noexcept;
    /* Active -> Stopped (owning Image stream logically stopped). */
    void stop() noexcept;
    /* Uncertain provider ownership after failed detach; Receive drains, then
     * reports PROVIDER_FAILED. */
    void fail() noexcept;
    /* After provider channel destruction: wait for bridge callbacks already in
     * flight, then stop. Never called from inside a quaternion callback. */
    void drain_and_stop() noexcept;
};

struct ams_mel_ir_image_los_quaternion_metadata { std::shared_ptr<LosQuaternionState> state; };
struct ams_mel_ir_image_los_quaternion_event { std::unique_ptr<LosQuaternionEventData> data; };

namespace ams_mel::internal {

/* Copies every published getter exactly once, in upstream order, by plain
 * assignment. The payload has no collections or enumerations, so no
 * structural validation exists: a non-unit, zero or non-finite quaternion is
 * copied, never rejected. Throws std::bad_alloc on allocation failure, in
 * which case nothing has been published. */
std::unique_ptr<LosQuaternionEventData> copy_line_of_sight_quaternion(
    const ams::iface::irmel::LineOfSightQuaternion& input);

} // namespace ams_mel::internal
