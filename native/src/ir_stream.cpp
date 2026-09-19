#include <ams_mel/abi.h>
#include "internal.hpp"
#include "internal/ir_channel.hpp"
#include "internal/ir_stream.hpp"

#include <irmel/library/image/ImageChannel.h>
#include <irmel/library/irmel-types/FrameHeader.h>
#include <irmel/library/irmel-types/ImageListener.h>

#include <algorithm>
#include <array>
#include <atomic>
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
    ams_mel_ir_frame_snapshot_v1 view{};
    std::string location_key;
    std::string location_system_name;
    std::vector<std::uint32_t> flags;
    std::vector<ams_mel_ir_sensor_inertial_state_v1> inertial;
    std::vector<ams_mel_ir_sensor_nav_state_v1> nav;
    std::vector<std::uint8_t> pixels;

    void bind() noexcept
    {
        view.contributing_sensor.location.key = {location_key.data(), location_key.size()};
        view.contributing_sensor.location.system_name =
            {location_system_name.data(), location_system_name.size()};
        view.image_flags = {flags.data(), flags.size()};
        view.sensor_inertial_states = {inertial.data(), inertial.size()};
        view.sensor_nav_states = {nav.data(), nav.size()};
        view.pixels = {pixels.data(), pixels.size()};
    }
};

ams_mel_ir_directional_v1 directional(const irmel::IR_Directional& value) noexcept
{ return {value.getX(), value.getY(), value.getZ()}; }
ams_mel_ir_quaternion_v1 quaternion(const irmel::Quaternion& value) noexcept
{ return {value.getQuaternionX(), value.getQuaternionY(), value.getQuaternionZ(), value.getQuaternionW()}; }
ams_mel_ir_nav_error_v1 nav_error(const irmel::NavError& value) noexcept
{ return {value.getX(), value.getY(), value.getZ(), value.getW()}; }
ams_mel_euler_v1 euler(const mel::Euler& value) noexcept
{ return {value.getRoll(), value.getPitch(), value.getYaw()}; }

template<typename EulerGetter, typename QuaternionGetter>
ams_mel_ir_orientation_v1 orientation(EulerGetter get_euler,
                                      QuaternionGetter get_quaternion)
{
    try {
        return {AMS_MEL_IR_ORIENTATION_EULER, euler(get_euler()), {}};
    } catch (const std::bad_variant_access&) {
        return {AMS_MEL_IR_ORIENTATION_QUATERNION, {}, quaternion(get_quaternion())};
    }
}

QueuedFrame copy_frame(const irmel::FrameHeader& header, const std::uint8_t *pixels,
                       std::size_t pixel_count)
{
#ifdef AMS_MEL_ENABLE_TEST_FAILPOINTS
    if (const char *failure = std::getenv("AMS_MEL_TEST_FRAME_COPY_FAILURE");
        failure != nullptr && std::strcmp(failure, "allocation") == 0) {
        (void)unsetenv("AMS_MEL_TEST_FRAME_COPY_FAILURE");
        throw std::bad_alloc{};
    }
#endif
    QueuedFrame frame;
    const auto image_type = static_cast<std::uint32_t>(header.getImageType());
    const auto image_flip = static_cast<std::uint32_t>(header.getImageFlip());
    if (image_type > 2U || image_flip > 3U) throw std::runtime_error("invalid image enum");
    frame.view.system_time_ns = header.getSystemTime().count();
    frame.view.integration_time_ns = header.getIntegrationTime().count();
    frame.view.width = header.getWidth(); frame.view.height = header.getHeight();
    frame.view.bits_per_pixel = header.getBitsPerPixel(); frame.view.number_of_bands = header.getNumBands();
    frame.view.horizontal_fov_rad = header.getHorizontalFieldOfView();
    frame.view.vertical_fov_rad = header.getVerticalFieldOfView();
    const auto& sensor = header.getFace(); const auto& location = sensor.first;
    frame.view.contributing_sensor.location.offset_x_m = location.getOffsetX();
    frame.view.contributing_sensor.location.offset_y_m = location.getOffsetY();
    frame.view.contributing_sensor.location.offset_z_m = location.getOffsetZ();
    frame.location_key = location.getLocationId().getKey();
    frame.location_system_name = location.getLocationId().getSystemName();
    if (!valid_utf8(frame.location_key) || !valid_utf8(frame.location_system_name))
        throw std::runtime_error("invalid contributing sensor string");
    frame.view.contributing_sensor.sensor_id = sensor.second;
    frame.view.pixel_format = static_cast<std::uint32_t>(header.getFormat());
    frame.view.frame_id = header.getFrameID(); frame.view.subframe_id = header.getSubframeID();
    frame.view.subframe_total = header.getSubframeTotal(); frame.view.image_type = image_type;
    frame.view.image_flip = image_flip;
    for (const auto flag : header.getFlags()) {
        const auto value = static_cast<std::uint32_t>(flag);
        if (value > 3U) throw std::runtime_error("invalid image flag");
        frame.flags.push_back(value);
    }
    frame.view.dither_row = header.getDitherRow(); frame.view.dither_column = header.getDitherCol();
    frame.view.row_offset = header.getRowOffset(); frame.view.column_offset = header.getColumnOffset();
    for (const auto& state : header.getSensorInertialState()) {
        frame.inertial.push_back({state.getSystemTime().count(), quaternion(state.getQ_xyzw()),
            quaternion(state.getQECEF_xyzw()), directional(state.getSensorPosition()),
            directional(state.getSensorVelocity()), {state.getUncertainties().getSensorUncertainties(),
            state.getUncertainties().getPlatformUncertainties()}});
    }
    for (const auto& state : header.getSensorNavState()) {
        const auto coordinate = static_cast<std::uint32_t>(state.getCoordinateSystem());
        if (coordinate > 3U) throw std::runtime_error("invalid coordinate system");
        frame.nav.push_back({directional(state.getPosition()), nav_error(state.getPositionError()),
            directional(state.getVelocity()), nav_error(state.getVelocityError()),
            directional(state.getAccel()), nav_error(state.getAccelError()),
            orientation([&state]() -> const mel::Euler& { return state.getEulerOrientation(); },
                        [&state]() -> const irmel::Quaternion& { return state.getQuaternionOrientation(); }),
            nav_error(state.getOrientationError()),
            orientation([&state]() -> const mel::Euler& { return state.getEulerOrientationVel(); },
                        [&state]() -> const irmel::Quaternion& { return state.getQuaternionOrientationVel(); }),
            nav_error(state.getOrientationVelError()),
            orientation([&state]() -> const mel::Euler& { return state.getEulerOrientationAccel(); },
                        [&state]() -> const irmel::Quaternion& { return state.getQuaternionOrientationAccel(); }),
            nav_error(state.getOrientationAccelError()), coordinate});
    }
    frame.view.band_index = header.getBandIndex();
    frame.pixels.assign(pixels, pixels + pixel_count);
    frame.bind();
    return frame;
}

} // namespace

using namespace ams::iface;

enum class Lifecycle {
    Attached,
    Starting,
    Running,
    Stopping,
    Stopped,
    Failed
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
    Lifecycle lifecycle{Lifecycle::Attached};
    ams_mel_ir_stream_counters_v1 counters{};
    std::vector<HostRange> registered_ranges;
    std::atomic<std::size_t> callbacks_in_flight{0U};

    void fail() noexcept
    {
        try {
            std::lock_guard lock{mutex};
            accepting = false;
            lifecycle = Lifecycle::Failed;
            ready.notify_all();
        } catch (...) {
            /* No exception may cross the provider callback boundary. */
        }
    }

    void image(const irmel::FrameHeader& header,
               const std::shared_ptr<irmel::Buffer>& buffer) noexcept
    {
        struct Releaser {
            CallbackState& state;
            std::shared_ptr<irmel::Buffer> value;
            ~Releaser() noexcept
            {
                if (value) {
                    try {
                        if (value->release() != irmel::Return::Success) state.fail();
                    } catch (...) {
                        state.fail();
                    }
                }
            }
        } release{*this, buffer};

        try {
            {
                std::lock_guard lock{mutex};
                increment(counters.frames_received);
            }
            if (!buffer || header.getWidth() == 0U || header.getHeight() == 0U ||
                header.getBitsPerPixel() != 8U || header.getNumBands() != 1U ||
                header.getFormat() != irmel::PixelFormat::Mono ||
                static_cast<std::uint32_t>(header.getImageType()) > 2U ||
                static_cast<std::uint32_t>(header.getImageFlip()) > 3U) {
                std::lock_guard lock{mutex};
                increment(counters.malformed_or_unsupported_frames);
                return;
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

            QueuedFrame frame = copy_frame(header,
                static_cast<const std::uint8_t *>(image_pointer), bytes);

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

bool claim_image_metadata(
    ImageStreamState& stream,
    const std::shared_ptr<ImageMetadataState>& state,
    std::shared_ptr<irmel::ImageChannel>& image_channel) noexcept
{
    std::lock_guard lock{stream.callback->mutex};
    if (!stream.image_channel || stream.callback->lifecycle == Lifecycle::Stopping ||
        stream.callback->lifecycle == Lifecycle::Stopped ||
        stream.callback->lifecycle == Lifecycle::Failed ||
        stream.image_metadata_attempted)
        return false;
    stream.image_metadata_attempted = true;
    stream.image_metadata = state;
    image_channel = stream.image_channel;
    return true;
}

class Listener final : public irmel::ImageListener {
public:
    explicit Listener(std::shared_ptr<CallbackState> state) : state_{std::move(state)} {}
    void onImage(const irmel::Channel&, const irmel::FrameHeader& header,
                 std::shared_ptr<irmel::Buffer> buffer) override
    {
        struct CallbackEntry {
            CallbackState& state;
            explicit CallbackEntry(CallbackState& value) noexcept : state{value}
            { state.callbacks_in_flight.fetch_add(1U, std::memory_order_acq_rel); }
            ~CallbackEntry() noexcept
            {
                state.callbacks_in_flight.fetch_sub(1U, std::memory_order_acq_rel);
                state.callbacks_in_flight.notify_all();
            }
        } entry{*state_};
        state_->image(header, buffer);
    }
private:
    std::shared_ptr<CallbackState> state_;
};

struct ams_mel_ir_frame_snapshot {
    QueuedFrame frame;
};

namespace {
bool supported(const irmel::ChannelCapability& capability) noexcept
{
    return capability.getFormat() == irmel::PixelFormat::Mono &&
           capability.getBitDepth() == 8U && capability.getNumberOfBands() == 1U;
}

ams_mel_status_t teardown(ImageStreamState& stream, bool failed, char *out,
                          std::size_t capacity, std::size_t *required) noexcept
{
    try {
        {
            std::lock_guard lock{stream.callback->mutex};
            stream.callback->accepting = false;
            if (stream.callback->lifecycle != Lifecycle::Failed)
                stream.callback->lifecycle = Lifecycle::Stopping;
        }

        if (stream.enable_attempted && stream.channel) {
            try {
                if (stream.channel->disable() != irmel::Return::Success) failed = true;
            } catch (...) {
                failed = true;
            }
        }

        if (stream.channel) {
            bool detached = false;
            try {
                detached = stream.session->control->detachChannel(stream.channel) ==
                           irmel::Return::Success;
            } catch (...) {
                failed = true;
            }
            if (!detached) {
                std::lock_guard lock{stream.callback->mutex};
                stream.callback->lifecycle = Lifecycle::Failed;
                stream.callback->ready.notify_all();
                diagnostic("channel detach failed; callback resources retained", out,
                           capacity, required);
                return AMS_MEL_PROVIDER_FAILED;
            }

            /* disable() is not a quiescence boundary. Provider channel
             * destruction occurs while every callback-accessible object and
             * host byte remains retained by stream. */
            stream.image_channel.reset();
            stream.channel.reset();
        }

        image_metadata_stream_stopped(stream.image_metadata);

        std::size_t in_flight = stream.callback->callbacks_in_flight.load(
            std::memory_order_acquire);
        while (in_flight != 0U) {
            stream.callback->callbacks_in_flight.wait(
                in_flight, std::memory_order_acquire);
            in_flight = stream.callback->callbacks_in_flight.load(
                std::memory_order_acquire);
        }
        {
            std::lock_guard lock{stream.callback->mutex};
            failed = failed || stream.callback->lifecycle == Lifecycle::Failed;
            stream.callback->registered_ranges.clear();
        }
        stream.buffers.clear();
        stream.storage.clear();
        stream.enable_attempted = false;
        {
            std::lock_guard lock{stream.callback->mutex};
            stream.callback->lifecycle = failed ? Lifecycle::Failed : Lifecycle::Stopped;
            stream.callback->ready.notify_all();
        }
        if (failed) {
            diagnostic("provider stream operation or cleanup failed", out,
                       capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        return AMS_MEL_OK;
    } catch (...) {
        try {
            std::lock_guard lock{stream.callback->mutex};
            stream.callback->accepting = false;
            stream.callback->lifecycle = Lifecycle::Failed;
            stream.callback->ready.notify_all();
        } catch (...) {}
        diagnostic("stream teardown failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}
} // namespace

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
        config->buffer_size > static_cast<std::size_t>(
            std::numeric_limits<std::int64_t>::max()) ||
        config->buffer_count - 1U > static_cast<std::size_t>(
            std::numeric_limits<std::int64_t>::max()) ||
        !valid_view(config->channel_id.descriptive_label) ||
        !valid_view(config->platform_id.descriptive_label) ||
        !valid_view(config->sensor_location.key) ||
        !valid_view(config->sensor_location.system_name)) {
        diagnostic("invalid IR stream configuration", out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    try {
        auto stream = std::make_unique<ams_mel_ir_stream>();
        auto state = std::make_shared<ImageStreamState>();
        stream->state = state;
        state->session = session->state;
        state->callback = std::make_shared<CallbackState>();
        state->callback->capacity = config->queue_capacity;
        state->listener = std::make_shared<Listener>(state->callback);
        state->buffer_count = config->buffer_count;
        state->buffer_size = config->buffer_size;
        state->instance = state->session->instance;
        state->buffer_factory = state->session->library->symbol<BufferFactory>("getBuffer");
        mel::ForeignKey key{copy_view(config->sensor_location.key),
                            copy_view(config->sensor_location.system_name)};
        mel::ComponentLocation location{config->sensor_location.offset_x_m,
            config->sensor_location.offset_y_m,
            config->sensor_location.offset_z_m, key};
        irmel::Config upstream{convert_id(config->channel_id),
            irmel::ChannelType::IRSTImage, convert_id(config->platform_id),
            std::move(location), state->listener, false, false};
        state->channel = state->session->control->attachChannel(upstream);
        if (!state->channel) {
            diagnostic("attachChannel returned null", out, capacity, required);
            return AMS_MEL_FACTORY_FAILED;
        }
        state->image_channel =
            std::dynamic_pointer_cast<irmel::ImageChannel>(state->channel);
        bool compatible = false;
        try {
            compatible = state->image_channel &&
                         supported(state->channel->getCapabilities());
        } catch (...) {
            bool detached = false;
            try {
                detached = state->session->control->detachChannel(state->channel) ==
                           irmel::Return::Success;
            } catch (...) {}
            if (!detached) {
                diagnostic("capability query and detach failed", out, capacity, required);
                (void)stream.release();
                return AMS_MEL_PROVIDER_FAILED;
            }
            state->image_channel.reset();
            state->channel.reset();
            throw;
        }
        if (!compatible) {
            bool detached = false;
            try {
                detached = state->session->control->detachChannel(state->channel) ==
                           irmel::Return::Success;
            } catch (...) {}
            if (detached) {
            state->image_channel.reset();
            state->channel.reset();
            } else {
                /* No owner may be destroyed while provider access can remain. */
                (void)stream.release();
            }
            diagnostic(detached ? "provider does not advertise Mono8 single-band capability" :
                       "incompatible channel and detach failed", out, capacity, required);
            return detached ? AMS_MEL_INITIALIZATION_FAILED : AMS_MEL_PROVIDER_FAILED;
        }
        stream->state = std::move(state);
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
    try {
        {
            std::lock_guard lock{stream->state->callback->mutex};
            switch (stream->state->callback->lifecycle) {
            case Lifecycle::Running: return AMS_MEL_OK;
            case Lifecycle::Stopped: return AMS_MEL_STREAM_STOPPED;
            case Lifecycle::Failed: return AMS_MEL_PROVIDER_FAILED;
            case Lifecycle::Starting:
            case Lifecycle::Stopping: return AMS_MEL_PROVIDER_FAILED;
            case Lifecycle::Attached: break;
            }
            stream->state->callback->lifecycle = Lifecycle::Starting;
        }
    } catch (...) {
        diagnostic("stream state transition failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    try {
        auto& state = *stream->state;
        state.storage.reserve(state.buffer_count);
        state.buffers.reserve(state.buffer_count);
        for (std::size_t i = 0; i < state.buffer_count; ++i) {
            auto buffer = state.buffer_factory(state.instance, state.session->manager);
            if (!buffer) throw std::runtime_error("getBuffer returned null");
            state.storage.emplace_back(state.buffer_size);
            if (buffer->init(state.storage.back().data(), state.buffer_size,
                             static_cast<std::int64_t>(i)) != irmel::Return::Success ||
                state.channel->registerBuffer(buffer) != irmel::Return::Success) {
                throw std::runtime_error("buffer initialization or registration failed");
            }
            state.buffers.push_back(std::move(buffer));
        }
        {
            std::lock_guard lock{state.callback->mutex};
            state.callback->registered_ranges.clear();
            state.callback->registered_ranges.reserve(state.storage.size());
            for (const auto& bytes : state.storage) {
                state.callback->registered_ranges.push_back(
                    {reinterpret_cast<std::uintptr_t>(bytes.data()), bytes.size()});
            }
            state.callback->accepting = true;
        }
        state.enable_attempted = true;
        if (state.channel->enable() != irmel::Return::Success) {
            throw std::runtime_error("channel enable failed");
        }
        bool callback_failed = false;
        {
            std::lock_guard lock{state.callback->mutex};
            callback_failed = state.callback->lifecycle == Lifecycle::Failed;
            if (!callback_failed) state.callback->lifecycle = Lifecycle::Running;
        }
        if (callback_failed) return teardown(state, true, out, capacity, required);
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        stream->state->callback->fail();
        (void)teardown(*stream->state, true, nullptr, 0U, nullptr);
        diagnostic("allocation failed during stream start", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        stream->state->callback->fail();
        (void)teardown(*stream->state, true, nullptr, 0U, nullptr);
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
        const auto& state = *stream->state;
        std::unique_lock lock{state.callback->mutex};
        const auto terminal = [&state] {
            return state.callback->lifecycle == Lifecycle::Stopped ||
                   state.callback->lifecycle == Lifecycle::Failed;
        };
        if (state.callback->queue.empty() && !terminal()) {
            (void)state.callback->ready.wait_for(lock,
                std::chrono::milliseconds{timeout_ms}, [&] {
                    return !state.callback->queue.empty() || terminal();
                });
        }
        if (state.callback->queue.empty()) {
            if (state.callback->lifecycle == Lifecycle::Failed)
                return AMS_MEL_PROVIDER_FAILED;
            return state.callback->lifecycle == Lifecycle::Stopped ?
                   AMS_MEL_STREAM_STOPPED : AMS_MEL_TIMEOUT;
        }
        const QueuedFrame& queued = state.callback->queue.front();
        frame->pixel_required = queued.pixels.size();
        if (!frame->pixels || frame->pixel_capacity < queued.pixels.size()) {
            return AMS_MEL_BUFFER_TOO_SMALL;
        }
        std::uint8_t *pixels = frame->pixels;
        const std::size_t pixel_capacity = frame->pixel_capacity;
        const auto& metadata = queued.view;
        frame->system_time_ns = metadata.system_time_ns;
        frame->integration_time_ns = metadata.integration_time_ns;
        frame->width = metadata.width; frame->height = metadata.height;
        frame->bits_per_pixel = metadata.bits_per_pixel;
        frame->number_of_bands = metadata.number_of_bands;
        frame->horizontal_fov_rad = metadata.horizontal_fov_rad;
        frame->vertical_fov_rad = metadata.vertical_fov_rad;
        frame->pixel_format = metadata.pixel_format;
        frame->frame_id = metadata.frame_id; frame->subframe_id = metadata.subframe_id;
        frame->subframe_total = metadata.subframe_total; frame->image_type = metadata.image_type;
        frame->image_flip = metadata.image_flip; frame->image_flags = 0U;
        for (const auto flag : queued.flags) frame->image_flags |= UINT32_C(1) << flag;
        frame->dither_row = metadata.dither_row; frame->dither_column = metadata.dither_column;
        frame->row_offset = metadata.row_offset; frame->column_offset = metadata.column_offset;
        frame->band_index = metadata.band_index;
        frame->pixels = pixels;
        frame->pixel_capacity = pixel_capacity;
        frame->pixel_required = queued.pixels.size();
        std::memcpy(pixels, queued.pixels.data(), queued.pixels.size());
        state.callback->queue.pop_front();
        return AMS_MEL_OK;
    } catch (...) {
        diagnostic("receive failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_stream_receive_snapshot(
    ams_mel_ir_stream *stream, std::uint32_t timeout_ms,
    ams_mel_ir_frame_snapshot **snapshot, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!stream || !snapshot || *snapshot || (!out && capacity != 0U))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        const auto& state = *stream->state;
        std::unique_lock lock{state.callback->mutex};
        const auto terminal = [&state] { return state.callback->lifecycle == Lifecycle::Stopped ||
            state.callback->lifecycle == Lifecycle::Failed; };
        if (state.callback->queue.empty() && !terminal()) {
            (void)state.callback->ready.wait_for(lock, std::chrono::milliseconds{timeout_ms}, [&] {
                return !state.callback->queue.empty() || terminal(); });
        }
        if (state.callback->queue.empty()) {
            if (state.callback->lifecycle == Lifecycle::Failed) return AMS_MEL_PROVIDER_FAILED;
            return state.callback->lifecycle == Lifecycle::Stopped ? AMS_MEL_STREAM_STOPPED : AMS_MEL_TIMEOUT;
        }
        auto owner = std::make_unique<ams_mel_ir_frame_snapshot>();
        owner->frame = std::move(state.callback->queue.front());
        state.callback->queue.pop_front();
        owner->frame.bind();
        *snapshot = owner.release();
        return AMS_MEL_OK;
    } catch (...) {
        diagnostic("snapshot receive failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_frame_snapshot_view(
    const ams_mel_ir_frame_snapshot *snapshot, const ams_mel_ir_frame_snapshot_v1 **view,
    char *out, std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!snapshot || !view || (!out && capacity != 0U)) return AMS_MEL_INVALID_ARGUMENT;
    *view = &snapshot->frame.view;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_frame_snapshot_close(
    ams_mel_ir_frame_snapshot **snapshot, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!snapshot || (!out && capacity != 0U)) return AMS_MEL_INVALID_ARGUMENT;
    delete *snapshot; *snapshot = nullptr;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_stream_get_counters(
    const ams_mel_ir_stream *stream, ams_mel_ir_stream_counters_v1 *counters,
    char *out, std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    try {
        if (!stream || !counters || (!out && capacity != 0U))
            return AMS_MEL_INVALID_ARGUMENT;
        std::lock_guard lock{stream->state->callback->mutex};
        *counters = stream->state->callback->counters;
        return AMS_MEL_OK;
    } catch (...) {
        diagnostic("counter query failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_stream_get_capabilities(
    ams_mel_ir_stream *stream, ams_mel_ir_channel_capability **output, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!stream || !output || *output || (!out && capacity != 0U))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::shared_ptr<irmel::ImageChannel> image_channel;
        {
            std::lock_guard lock{stream->state->callback->mutex};
            if (!stream->state->image_channel ||
                stream->state->callback->lifecycle == Lifecycle::Stopping ||
                stream->state->callback->lifecycle == Lifecycle::Stopped ||
                stream->state->callback->lifecycle == Lifecycle::Failed)
                return AMS_MEL_PROVIDER_FAILED;
            image_channel = stream->state->image_channel;
        }
        return ams_mel::internal::snapshot_capability(*image_channel, output,
                                                       out, capacity, required);
    } catch (...) {
        diagnostic("Image capability query failed", out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_stream_stop(
    ams_mel_ir_stream *stream, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    try {
        if (!stream || (!out && capacity != 0U)) return AMS_MEL_INVALID_ARGUMENT;
        bool failed;
        {
            std::lock_guard lock{stream->state->callback->mutex};
            if (stream->state->callback->lifecycle == Lifecycle::Stopped) return AMS_MEL_OK;
            failed = stream->state->callback->lifecycle == Lifecycle::Failed;
        }
        if (!stream->state->channel) return failed ? AMS_MEL_PROVIDER_FAILED : AMS_MEL_OK;
        return teardown(*stream->state, failed, out, capacity, required);
    } catch (const std::bad_alloc&) {
        diagnostic("allocation failed during stream stop", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        diagnostic("stream stop failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_stream_close(
    ams_mel_ir_stream **stream, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    try {
        if (!stream || (!out && capacity != 0U)) return AMS_MEL_INVALID_ARGUMENT;
        ams_mel_ir_stream *owned = *stream;
        if (!owned) return AMS_MEL_OK;
        const auto status = ams_mel_ir_stream_stop(owned, out, capacity, required);
        if (owned->state->channel) return status;
        *stream = nullptr;
        delete owned;
        return status;
    } catch (const std::bad_alloc&) {
        diagnostic("allocation failed during stream close", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        diagnostic("stream close failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}
