#pragma once

#include "../internal.hpp"

#include <irmel/library/image/ImageChannel.h>

#include <atomic>
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
 * handle to this state so future private asynchronous work can retain it.
 * requests/cleanup_started/public_owner_closed support deferred physical
 * teardown while an asynchronous Navigation request future is outstanding;
 * see Task 027B. emergency_* fields provide an allocation-free fail-safe
 * retention path when a deferred detach cannot be established, mirroring
 * the C2 ChannelState::retain_failed pattern. */
struct ImageStreamState {
    std::shared_ptr<SessionState> session;
    std::shared_ptr<CallbackState> callback;
    std::shared_ptr<Listener> listener;
    std::shared_ptr<ams::iface::irmel::Channel> channel;
    std::shared_ptr<ams::iface::irmel::ImageChannel> image_channel;
    std::shared_ptr<ImageMetadataState> image_metadata;
    bool image_metadata_attempted{false};
    BufferFactory buffer_factory{};
    std::string instance;
    std::size_t buffer_count{};
    std::size_t buffer_size{};
    std::vector<std::vector<std::uint8_t>> storage;
    std::vector<std::shared_ptr<ams::iface::irmel::Buffer>> buffers;
    bool enable_attempted{false};

    /* Guarded by callback->mutex, matching ordinary Image lifecycle state. */
    std::size_t requests{};
    bool cleanup_started{false};
    bool public_owner_closed{false};

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

/* Shared physical-teardown machinery used by both ordinary Stop/Close and
 * final Navigation request completion. Pass deferred=false from public
 * Stop/Close and deferred=true from final request completion: a deferred call
 * performs teardown only when a logical Stop/Close already began, and only a
 * deferred failure after the public owner is gone uses allocation-free
 * emergency retention. Defined in ir_stream.cpp. */
bool image_stream_cleanup(const std::shared_ptr<ImageStreamState>& state,
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
