#pragma once

/* IRMEL-ADA-009 private opt-in Image OpticalDistortionMap reception.
 *
 * One independently registered owner per Image stream, never registered by
 * the four-kind Image metadata owner, the NUC_TempData owner or the
 * CameraCommandResp owner. Provider callbacks only ever touch OdmState through
 * the shared_ptr captured by the registered closure, so a late callback after
 * the public owner, the stream or the Session is closed remains memory-safe.
 * Every valid map is deep-copied before the provider callback returns; no
 * provider-owned object or borrowed pointer survives callback return. */

#include <ams_mel/abi.h>

#include <irmel/library/irmel-types/OpticalDistortionMap.h>

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

/* Event-owned storage. view.frd/view.row_col point into the two vectors, which
 * are sized once before view is bound and never resized afterwards. */
struct OdmEventData {
    ams_mel_ir_optical_distortion_map_v1 view{};
    std::vector<ams_mel_ir_forward_right_down_v1> frd;
    std::vector<ams_mel_ir_row_col_v1> row_col;
};

enum class OdmLifecycle { Active, Inactive, Stopped, Failed };

struct OdmState {
    std::mutex mutex;
    std::condition_variable ready;
    std::condition_variable callbacks_done;
    std::deque<std::unique_ptr<OdmEventData>> queue;
    std::size_t capacity{};
    ams_mel_ir_metadata_counters_v1 counters{};
    OdmLifecycle lifecycle{OdmLifecycle::Active};
    /* Bridge callbacks currently inside receive(); guarded by mutex. */
    std::size_t callbacks{};

    /* Provider callback entry point. Never throws. */
    void receive(const ams::iface::irmel::OpticalDistortionMap *input) noexcept;
    /* Active -> Inactive (public close or failed registration). */
    void deactivate() noexcept;
    /* Active -> Stopped (owning Image stream logically stopped). */
    void stop() noexcept;
    /* Uncertain provider ownership after failed detach; Receive drains, then
     * reports PROVIDER_FAILED. */
    void fail() noexcept;
    /* After provider channel destruction: wait for bridge callbacks already in
     * flight, then stop. Never called from inside an ODM callback. */
    void drain_and_stop() noexcept;
};

struct ams_mel_ir_image_odm_metadata { std::shared_ptr<OdmState> state; };
struct ams_mel_ir_image_odm_event { std::unique_ptr<OdmEventData> data; };

namespace ams_mel::internal {

/* Shared allocation-free validation used by both the full-queue
 * classification and the copy path: each vector count must fit INT32_MAX and
 * the checked byte size of its C element. No relationship between the two
 * vector lengths, or between them and the scalar dimensions, is required. */
bool valid_optical_distortion_map(
    const ams::iface::irmel::OpticalDistortionMap& input) noexcept;

/* Validates exactly once, then deep-copies every published getter. Returns
 * nullptr for an invalid map; throws std::bad_alloc on allocation failure, in
 * which case nothing has been published. */
std::unique_ptr<OdmEventData> copy_optical_distortion_map(
    const ams::iface::irmel::OpticalDistortionMap& input);

} // namespace ams_mel::internal
