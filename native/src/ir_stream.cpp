#include <ams_mel/abi.h>
#include "internal.hpp"

#include <irmel/library/image/ImageChannel.h>
#include <irmel/library/irmel-types/FrameHeader.h>
#include <irmel/library/irmel-types/ImageListener.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace ams::iface;
using BufferFactory = std::shared_ptr<irmel::Buffer> (*)(
    std::string_view, std::shared_ptr<API_Manager>);

void diagnostic(std::string_view text, char *out, std::size_t capacity,
                std::size_t *required) noexcept
{
    if (required != nullptr) {
        *required = text.size() + 1U;
    }
    if (out != nullptr && capacity != 0U) {
        const std::size_t copied = std::min(text.size(), capacity - 1U);
        std::memcpy(out, text.data(), copied);
        out[copied] = '\0';
    }
}

bool valid_utf8(std::string_view value) noexcept
{
    std::size_t index = 0;
    while (index < value.size()) {
        const auto lead = static_cast<unsigned char>(value[index]);
        if (lead == 0U) return false;
        if (lead <= 0x7fU) { ++index; continue; }
        std::size_t trailing{};
        std::uint32_t point{};
        if (lead >= 0xc2U && lead <= 0xdfU) { trailing = 1U; point = lead & 0x1fU; }
        else if (lead >= 0xe0U && lead <= 0xefU) { trailing = 2U; point = lead & 0x0fU; }
        else if (lead >= 0xf0U && lead <= 0xf4U) { trailing = 3U; point = lead & 0x07U; }
        else return false;
        if (index + trailing >= value.size()) return false;
        for (std::size_t offset = 1; offset <= trailing; ++offset) {
            const auto byte = static_cast<unsigned char>(value[index + offset]);
            if ((byte & 0xc0U) != 0x80U) return false;
            point = (point << 6U) | (byte & 0x3fU);
        }
        if ((trailing == 2U && point < 0x800U) ||
            (trailing == 3U && point < 0x10000U) || point > 0x10ffffU ||
            (point >= 0xd800U && point <= 0xdfffU)) return false;
        index += trailing + 1U;
    }
    return true;
}

bool valid_view(const ams_mel_string_view_v1& value) noexcept
{
    if (value.data == nullptr) return value.size == 0U;
    return valid_utf8({value.data, value.size});
}

std::string copy_view(const ams_mel_string_view_v1& value)
{
    return value.size == 0U ? std::string{} : std::string{value.data, value.size};
}

mel::UCI_ID convert_id(const ams_mel_uci_id_v1& value)
{
    std::array<std::uint8_t, mel::UUID_SIZE> uuid{};
    std::copy(std::begin(value.uuid), std::end(value.uuid), uuid.begin());
    return {uuid, copy_view(value.descriptive_label)};
}

void increment(std::uint64_t& value) noexcept
{
    if (value != std::numeric_limits<std::uint64_t>::max()) {
        ++value;
    }
}

struct QueuedFrame {
    ams_mel_ir_frame_v1 metadata{};
    std::vector<std::uint8_t> pixels;
};

struct CallbackState {
    struct HostRange {
        std::uintptr_t base;
        std::size_t size;
    };
    std::mutex mutex;
    std::condition_variable ready;
    std::deque<QueuedFrame> queue;
    std::size_t capacity{};
    bool accepting{false};
    bool stopped{true};
    ams_mel_ir_stream_counters_v1 counters{};
    std::vector<HostRange> registered_ranges;

    void image(const irmel::FrameHeader& header,
               const std::shared_ptr<irmel::Buffer>& buffer) noexcept
    {
        struct Releaser {
            std::shared_ptr<irmel::Buffer> value;
            ~Releaser()
            {
                if (value) {
                    try { (void)value->release(); } catch (...) {}
                }
            }
        } release{buffer};

        try {
            {
                std::lock_guard lock{mutex};
                increment(counters.frames_received);
            }
            if (!buffer || header.getWidth() == 0U || header.getHeight() == 0U ||
                header.getBitsPerPixel() != 8U || header.getNumBands() != 1U ||
                header.getFormat() != irmel::PixelFormat::Mono ||
                static_cast<std::uint32_t>(header.getImageType()) > 1U ||
                static_cast<std::uint32_t>(header.getImageFlip()) > 3U) {
                std::lock_guard lock{mutex};
                increment(counters.malformed_or_unsupported_frames);
                return;
            }
            for (const auto flag : header.getFlags()) {
                if (static_cast<std::uint32_t>(flag) > 3U) {
                    std::lock_guard lock{mutex};
                    increment(counters.malformed_or_unsupported_frames);
                    return;
                }
            }
            const std::size_t width = header.getWidth();
            const std::size_t height = header.getHeight();
            if (height > std::numeric_limits<std::size_t>::max() / width) {
                std::lock_guard lock{mutex};
                increment(counters.malformed_or_unsupported_frames);
                return;
            }
            const std::size_t bytes = width * height;
            void *const base_pointer = buffer->getBufferAddress();
            void *const image_pointer = buffer->getImageAddress();
            const std::int64_t signed_size = buffer->getSize();
            if (base_pointer == nullptr || image_pointer == nullptr || signed_size < 0) {
                std::lock_guard lock{mutex};
                increment(counters.malformed_or_unsupported_frames);
                return;
            }
            const auto base = reinterpret_cast<std::uintptr_t>(base_pointer);
            const auto image = reinterpret_cast<std::uintptr_t>(image_pointer);
            const auto size = static_cast<std::uint64_t>(signed_size);
            bool registered = false;
            {
                std::lock_guard lock{mutex};
                for (const auto& range : registered_ranges) {
                    if (range.base == base && size <= range.size) {
                        registered = true;
                        break;
                    }
                }
            }
            if (size > std::numeric_limits<std::uintptr_t>::max() - base ||
                !registered || image < base || image > base + size ||
                bytes > base + size - image) {
                std::lock_guard lock{mutex};
                increment(counters.malformed_or_unsupported_frames);
                return;
            }

            QueuedFrame frame;
            frame.pixels.resize(bytes);
            std::memcpy(frame.pixels.data(), image_pointer, bytes);
            frame.metadata.system_time_ns = header.getSystemTime().count();
            frame.metadata.integration_time_ns = header.getIntegrationTime().count();
            frame.metadata.width = header.getWidth();
            frame.metadata.height = header.getHeight();
            frame.metadata.bits_per_pixel = header.getBitsPerPixel();
            frame.metadata.number_of_bands = header.getNumBands();
            frame.metadata.horizontal_fov_rad = header.getHorizontalFieldOfView();
            frame.metadata.vertical_fov_rad = header.getVerticalFieldOfView();
            frame.metadata.pixel_format = AMS_MEL_IR_PIXEL_MONO;
            frame.metadata.frame_id = header.getFrameID();
            frame.metadata.subframe_id = header.getSubframeID();
            frame.metadata.subframe_total = header.getSubframeTotal();
            frame.metadata.image_type = static_cast<std::uint32_t>(header.getImageType());
            frame.metadata.image_flip = static_cast<std::uint32_t>(header.getImageFlip());
            for (const auto flag : header.getFlags()) {
                const auto bit = static_cast<std::uint32_t>(flag);
                if (bit < 32U) frame.metadata.image_flags |= UINT32_C(1) << bit;
            }
            frame.metadata.dither_row = header.getDitherRow();
            frame.metadata.dither_column = header.getDitherCol();
            frame.metadata.row_offset = header.getRowOffset();
            frame.metadata.column_offset = header.getColumnOffset();
            frame.metadata.band_index = header.getBandIndex();

            std::lock_guard lock{mutex};
            if (!accepting) return;
            if (queue.size() >= capacity) {
                increment(counters.frames_dropped_queue_full);
                return;
            }
            queue.push_back(std::move(frame));
            ready.notify_one();
        } catch (...) {
            std::lock_guard lock{mutex};
            increment(counters.malformed_or_unsupported_frames);
        }
    }
};

class Listener final : public irmel::ImageListener {
public:
    explicit Listener(std::shared_ptr<CallbackState> state) : state_{std::move(state)} {}
    void onImage(const irmel::Channel&, const irmel::FrameHeader& header,
                 std::shared_ptr<irmel::Buffer> buffer) override
    {
        state_->image(header, buffer);
    }
private:
    std::shared_ptr<CallbackState> state_;
};
} // namespace

struct ams_mel_ir_stream {
    std::shared_ptr<SessionState> session;
    std::shared_ptr<CallbackState> callback;
    std::shared_ptr<Listener> listener;
    std::shared_ptr<ams::iface::irmel::Channel> channel;
    std::shared_ptr<ams::iface::irmel::ImageChannel> image_channel;
    BufferFactory buffer_factory{};
    std::string instance;
    std::size_t buffer_count{};
    std::size_t buffer_size{};
    std::vector<std::vector<std::uint8_t>> storage;
    std::vector<std::shared_ptr<ams::iface::irmel::Buffer>> buffers;
    bool started{false};
    bool attached{false};
};

extern "C" ams_mel_status_t ams_mel_ir_stream_open(
    const ams_mel_session *session, const ams_mel_ir_stream_config_v1 *config,
    ams_mel_ir_stream **out_stream, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!session || !session->state || !config || !out_stream || *out_stream ||
        (!out && capacity != 0U) ||
        config->channel_type != AMS_MEL_IR_CHANNEL_IRST_IMAGE ||
        config->buffer_count == 0U || config->buffer_size == 0U ||
        config->queue_capacity == 0U ||
        !valid_view(config->channel_id.descriptive_label) ||
        !valid_view(config->platform_id.descriptive_label) ||
        !valid_view(config->sensor_location.key) ||
        !valid_view(config->sensor_location.system_name)) {
        diagnostic("invalid IR stream configuration", out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    try {
        auto stream = std::make_unique<ams_mel_ir_stream>();
        stream->session = session->state;
        stream->callback = std::make_shared<CallbackState>();
        stream->callback->capacity = config->queue_capacity;
        stream->listener = std::make_shared<Listener>(stream->callback);
        stream->buffer_count = config->buffer_count;
        stream->buffer_size = config->buffer_size;
        stream->instance = stream->session->instance;
        stream->buffer_factory = stream->session->library->symbol<BufferFactory>("getBuffer");
        mel::ForeignKey key{copy_view(config->sensor_location.key),
                            copy_view(config->sensor_location.system_name)};
        mel::ComponentLocation location{config->sensor_location.offset_x_m,
            config->sensor_location.offset_y_m,
            config->sensor_location.offset_z_m, key};
        irmel::Config upstream{convert_id(config->channel_id),
            irmel::ChannelType::IRSTImage, convert_id(config->platform_id),
            std::move(location), stream->listener, false, false};
        stream->channel = stream->session->control->attachChannel(upstream);
        if (!stream->channel) {
            diagnostic("attachChannel returned null", out, capacity, required);
            return AMS_MEL_FACTORY_FAILED;
        }
        stream->image_channel =
            std::dynamic_pointer_cast<irmel::ImageChannel>(stream->channel);
        if (!stream->image_channel) {
            (void)stream->session->control->detachChannel(stream->channel);
            diagnostic("attached channel is not ImageChannel", out, capacity, required);
            return AMS_MEL_INITIALIZATION_FAILED;
        }
        stream->attached = true;
        *out_stream = stream.release();
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        diagnostic("allocation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        diagnostic("provider exception during stream open", out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_stream_start(
    ams_mel_ir_stream *stream, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!stream || (!out && capacity != 0U)) return AMS_MEL_INVALID_ARGUMENT;
    if (stream->started) return AMS_MEL_OK;
    if (!stream->attached || !stream->channel) return AMS_MEL_STREAM_STOPPED;
    try {
        stream->storage.reserve(stream->buffer_count);
        stream->buffers.reserve(stream->buffer_count);
        for (std::size_t i = 0; i < stream->buffer_count; ++i) {
            auto buffer = stream->buffer_factory(stream->instance, stream->session->manager);
            if (!buffer) throw std::runtime_error("getBuffer returned null");
            stream->storage.emplace_back(stream->buffer_size);
            if (buffer->init(stream->storage.back().data(), stream->buffer_size,
                             static_cast<std::int64_t>(i)) != irmel::Return::Success ||
                stream->channel->registerBuffer(buffer) != irmel::Return::Success) {
                throw std::runtime_error("buffer initialization or registration failed");
            }
            stream->buffers.push_back(std::move(buffer));
        }
        {
            std::lock_guard lock{stream->callback->mutex};
            stream->callback->registered_ranges.clear();
            stream->callback->registered_ranges.reserve(stream->storage.size());
            for (const auto& bytes : stream->storage) {
                stream->callback->registered_ranges.push_back(
                    {reinterpret_cast<std::uintptr_t>(bytes.data()), bytes.size()});
            }
            stream->callback->accepting = true;
            stream->callback->stopped = false;
        }
        stream->started = true;
        if (stream->channel->enable() != irmel::Return::Success) {
            throw std::runtime_error("channel enable failed");
        }
        return AMS_MEL_OK;
    } catch (...) {
        {
            std::lock_guard lock{stream->callback->mutex};
            stream->callback->accepting = false;
            stream->callback->stopped = true;
        }
        bool disabled = false;
        try { disabled = stream->channel->disable() == irmel::Return::Success; }
        catch (...) { disabled = false; }
        if (!disabled) {
            diagnostic("stream start failed and disable did not quiesce", out,
                       capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        bool unregistered = true;
        for (auto it = stream->buffers.rbegin(); it != stream->buffers.rend(); ++it) {
            try {
                unregistered &= stream->channel->unregisterBuffer(*it) ==
                    irmel::Return::Success;
            } catch (...) { unregistered = false; }
        }
        if (!unregistered) {
            diagnostic("stream start rollback could not unregister buffers", out,
                       capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        stream->buffers.clear();
        stream->storage.clear();
        stream->callback->registered_ranges.clear();
        stream->started = false;
        diagnostic("provider failure during stream start", out, capacity, required);
        return AMS_MEL_PROVIDER_FAILED;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_stream_receive(
    ams_mel_ir_stream *stream, std::uint32_t timeout_ms,
    ams_mel_ir_frame_v1 *frame, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!stream || !frame || (!frame->pixels && frame->pixel_capacity != 0U) ||
        (!out && capacity != 0U)) return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::unique_lock lock{stream->callback->mutex};
        if (stream->callback->queue.empty() && !stream->callback->stopped) {
            (void)stream->callback->ready.wait_for(lock,
                std::chrono::milliseconds{timeout_ms}, [&] {
                    return !stream->callback->queue.empty() || stream->callback->stopped;
                });
        }
        if (stream->callback->queue.empty()) {
            return stream->callback->stopped ? AMS_MEL_STREAM_STOPPED : AMS_MEL_TIMEOUT;
        }
        const QueuedFrame& queued = stream->callback->queue.front();
        frame->pixel_required = queued.pixels.size();
        if (!frame->pixels || frame->pixel_capacity < queued.pixels.size()) {
            return AMS_MEL_BUFFER_TOO_SMALL;
        }
        std::uint8_t *pixels = frame->pixels;
        const std::size_t pixel_capacity = frame->pixel_capacity;
        *frame = queued.metadata;
        frame->pixels = pixels;
        frame->pixel_capacity = pixel_capacity;
        frame->pixel_required = queued.pixels.size();
        std::memcpy(pixels, queued.pixels.data(), queued.pixels.size());
        stream->callback->queue.pop_front();
        return AMS_MEL_OK;
    } catch (...) {
        diagnostic("receive failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_stream_get_counters(
    const ams_mel_ir_stream *stream, ams_mel_ir_stream_counters_v1 *counters,
    char *out, std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!stream || !counters || (!out && capacity != 0U)) return AMS_MEL_INVALID_ARGUMENT;
    std::lock_guard lock{stream->callback->mutex};
    *counters = stream->callback->counters;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_stream_stop(
    ams_mel_ir_stream *stream, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!stream || (!out && capacity != 0U)) return AMS_MEL_INVALID_ARGUMENT;
    {
        std::lock_guard lock{stream->callback->mutex};
        stream->callback->accepting = false;
    }
    if (stream->started) {
        bool disabled = false;
        try { disabled = stream->channel->disable() == irmel::Return::Success; }
        catch (...) { disabled = false; }
        if (!disabled) {
            diagnostic("channel disable failed; resources retained", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        std::vector<std::shared_ptr<irmel::Buffer>> retained;
        for (auto it = stream->buffers.rbegin(); it != stream->buffers.rend(); ++it) {
            bool removed = false;
            try { removed = stream->channel->unregisterBuffer(*it) == irmel::Return::Success; }
            catch (...) { removed = false; }
            if (!removed) retained.push_back(*it);
        }
        if (!retained.empty()) {
            stream->buffers.assign(retained.rbegin(), retained.rend());
            diagnostic("buffer unregister failed; resources retained", out,
                       capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        stream->started = false;
        stream->buffers.clear();
        stream->storage.clear();
        {
            std::lock_guard lock{stream->callback->mutex};
            stream->callback->registered_ranges.clear();
        }
    }
    if (stream->attached) {
        bool detached = false;
        try { detached = stream->session->control->detachChannel(stream->channel) ==
            irmel::Return::Success; } catch (...) { detached = false; }
        if (!detached) {
            diagnostic("channel detach failed; resources retained", out,
                       capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        stream->attached = false;
        stream->image_channel.reset();
        stream->channel.reset();
    }
    {
        std::lock_guard lock{stream->callback->mutex};
        stream->callback->stopped = true;
        stream->callback->ready.notify_all();
    }
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_stream_close(
    ams_mel_ir_stream **stream, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!stream || (!out && capacity != 0U)) return AMS_MEL_INVALID_ARGUMENT;
    ams_mel_ir_stream *owned = *stream;
    if (!owned) return AMS_MEL_OK;
    const auto status = ams_mel_ir_stream_stop(owned, out, capacity, required);
    if (status != AMS_MEL_OK) return status;
    *stream = nullptr;
    delete owned;
    return status;
}
