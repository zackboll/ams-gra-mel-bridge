#pragma once

#include "../internal.hpp"

#include <irmel/library/image/ImageChannel.h>

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

/* Private stream owner shared by the frame and Image metadata verticals.  The
 * ImageChannel is attached exactly once in ir_stream.cpp. */
struct ams_mel_ir_stream {
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
};

void image_metadata_stream_stopped(const std::shared_ptr<ImageMetadataState>& state) noexcept;
