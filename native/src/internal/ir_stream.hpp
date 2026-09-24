#pragma once

#include "../internal.hpp"

#include <irmel/library/image/ImageChannel.h>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct CallbackState;
class Listener;
struct ImageMetadataState;

using BufferFactory = std::shared_ptr<ams::iface::irmel::Buffer> (*)
    (std::string_view, std::shared_ptr<API_Manager>);

/* Shared private Image resource graph. The public stream is only an opaque
 * handle to this state so private asynchronous work can retain it.
 * The requests, cleanup, and public_owner_closed fields support deferred
 * physical teardown while an asynchronous Navigation request is outstanding;
 * see
 * Task 027B and docs/corrective-image-navigation-close-race.md. emergency_*
 * fields provide an allocation-free fail-safe retention path when a deferred
 * detach cannot be established, mirroring the C2 ChannelState::retain_failed
 * pattern.
 *
 * Teardown synchronization invariant (corrective task):
 *
 *   CallbackState::mutex is the single teardown lock. Every read and every
     *   write of requests, callback->retained_frames,
     *   callback->release_obligations, cleanup_in_progress, cleanup_complete,
 *   cleanup_ok, public_owner_closed, enable_attempted, channel and
 *   image_channel that can race between an application Stop/Close thread, a
 *   frame-lease close thread, the provider callback thread, and the adapter's
 *   own Navigation completion thread happens while that mutex is held.
 *
 *   Task 030B extends the deferred-teardown precondition. Physical teardown
 *   may run only when
 *
 *       requests == 0  AND  callback->retained_frames == 0
 *                      AND  callback->release_obligations == 0
 *                      AND  !callback->uncertain_release
 *
 *   The final term is the PR #42 second-review corrective backstop. It is a
 *   lock-free atomic published the instant ANY release for this stream is
 *   uncertain, including a callback-side rejection, so teardown stays blocked
 *   even if the retained_frames publication under this mutex could not be
 *   performed at all.
 *
 *   because a queued frame or a live frame snapshot still holds a provider
 *   Buffer checked out of the provider's pool, and that Buffer's memory, its
 *   vtable, and its registered host byte range must all stay alive until
 *   Buffer::release() has been called. Buffer::release() is a provider call
 *   and therefore never runs under CallbackState::mutex: ownership of the
 *   retained Buffer is moved out under the lock, release() runs unlocked, and
 *   the count transition is published under the lock afterwards.
 *
 *   Provider calls (disable/detachChannel/channel destruction) never run
 *   under that mutex. A cleanup owner claims cleanup_in_progress under the
 *   lock, copies the provider shared_ptrs into local owners, releases the
 *   lock, performs the provider work on the local owners, then re-acquires
 *   the lock to publish cleanup_complete/cleanup_ok and notify cleanup_done.
 *   A concurrent Stop/Close waits on cleanup_done rather than inspecting the
 *   channel members or spinning, and adopts the published cleanup result
 *   instead of an earlier, now stale, Stop status. */
struct ImageStreamState {
    std::shared_ptr<SessionState> session;
    std::shared_ptr<CallbackState> callback;
    std::shared_ptr<Listener> listener;
    std::shared_ptr<ImageMetadataState> image_metadata;
    bool image_metadata_attempted{false};
    BufferFactory buffer_factory{};
    std::string instance;
    std::size_t buffer_count{};
    std::size_t buffer_size{};
    std::vector<std::vector<std::uint8_t>> storage;
    std::vector<std::shared_ptr<ams::iface::irmel::Buffer>> buffers;
    /* Task 030B fail-safe retention, diagnostic record only.
     *
     * A provider buffer whose release() failed or threw has uncertain provider
     * ownership, so neither the Buffer object nor its registered host byte
     * range may be destroyed, and the retained-buffer count is deliberately
     * never decremented for it, which permanently blocks physical teardown.
     *
     * The AUTHORITATIVE owner of such a buffer is NOT this vector. It is the
     * frame's dedicated slot in CallbackState::retention_slots, preallocated
     * from the configured buffer_count in ams_mel_ir_stream_start() before
     * channel->enable() and therefore before any provider callback can run.
     * Moving the exact callback shared_ptr into that existing empty slot is
     * allocation-free and cannot throw. This vector is an additional record
     * kept for ordinary diagnostics and is written only after that slot is
     * published, so a failed push_back here cannot affect safety (PR #42
     * second-review corrective). Guarded by callback->mutex. */
    std::vector<std::shared_ptr<ams::iface::irmel::Buffer>> retained_failed_buffers;

    /* Teardown-participating state. All of the following is guarded by
     * callback->mutex; see the invariant above. channel/image_channel are
     * shared_ptr objects that the Navigation completion thread may reset
     * concurrently with an application Stop/Close, so they must never be read
     * or modified outside that lock. */
    std::shared_ptr<ams::iface::irmel::Channel> channel;
    std::shared_ptr<ams::iface::irmel::ImageChannel> image_channel;
    bool enable_attempted{false};
    std::size_t requests{};
    bool public_owner_closed{false};
    /* Exactly one caller may own physical teardown at a time. */
    bool cleanup_in_progress{false};
    /* A terminal cleanup attempt has published its outcome in cleanup_ok.
     * Terminal means detachChannel succeeded, so provider ownership of the
     * channel is proven released and no retry is possible or needed. */
    bool cleanup_complete{false};
    bool cleanup_ok{false};
    /* A cleanup attempt failed. When channel is still set the failure was a
     * detach failure with uncertain provider ownership and a later public
     * Close must retry it. */
    bool cleanup_failed{false};
    /* Signalled whenever cleanup ownership is released, so Stop/Close can
     * block on a concurrent cleanup instead of polling or racing. */
    std::condition_variable cleanup_done;

    std::shared_ptr<ImageStreamState> emergency_self;
    ImageStreamState *emergency_next{};
    std::atomic<bool> emergency_retained{};
};

struct ams_mel_ir_stream {
    std::shared_ptr<ImageStreamState> state;
};

void image_metadata_stream_stopped(const std::shared_ptr<ImageMetadataState>& state) noexcept;
bool claim_image_metadata(
    ImageStreamState& stream,
    const std::shared_ptr<ImageMetadataState>& state,
    std::shared_ptr<ams::iface::irmel::ImageChannel>& image_channel) noexcept;

/* Outcome of an image_stream_cleanup attempt, so callers can distinguish
 * "nothing to do" from "teardown ran and succeeded/failed". */
enum class ImageCleanupOutcome {
    /* No physical teardown was required or permitted at this point: requests
     * are still outstanding, the channel is already gone, or a deferred call
     * arrived while the stream is still logically usable. */
    NotRequired,
    /* Physical teardown ran to completion (channel detached and released). */
    Succeeded,
    /* Physical teardown was attempted and failed; the provider graph stays
     * retained and the public owner, if any, must be kept for a retry. */
    Failed
};

/* Shared physical-teardown machinery used by both ordinary Stop/Close and
 * final Navigation request completion. Pass deferred=false from public
 * Stop/Close and deferred=true from final request completion: a deferred call
 * performs teardown only when a logical Stop/Close already began, and only a
 * deferred failure after the public owner is gone uses allocation-free
 * emergency retention.
 *
 * The call synchronizes with any cleanup already owned by another thread: it
 * blocks on ImageStreamState::cleanup_done and reports that cleanup's
 * published outcome rather than inspecting channel concurrently or returning
 * a stale success. Never called with the frame callback mutex held.
 * Defined in ir_stream.cpp. */
ImageCleanupOutcome image_stream_cleanup(const std::shared_ptr<ImageStreamState>& state,
                                         bool deferred) noexcept;
void image_stream_retain_failed(const std::shared_ptr<ImageStreamState>& state) noexcept;

/* Validates the stream is logically Attached or Running, retrieves a copy of
 * the shared ImageChannel, and increments the request count, all under the
 * frame callback mutex. Returns false (leaving requests unchanged) when the
 * stream is not in a submittable lifecycle state. Defined in ir_stream.cpp. */
bool claim_navigation_submission(
    ImageStreamState& stream,
    std::shared_ptr<ams::iface::irmel::ImageChannel>& image_channel) noexcept;

/* Decrements the request count under the frame callback mutex. Used when a
 * submission attempt fails after claim_navigation_submission succeeded but
 * before a provider future exists, and by final Navigation request
 * completion. Defined in ir_stream.cpp. */
void release_navigation_submission(ImageStreamState& stream) noexcept;
