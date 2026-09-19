#include <ams_mel/abi.h>
#include "internal/ir_stream.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace ams::iface;

void diagnostic(std::string_view text, char *out, std::size_t capacity,
                std::size_t *required) noexcept
{
    if (required) *required = text.size() + 1U;
    if (out && capacity) {
        const std::size_t copied = std::min(text.size(), capacity - 1U);
        std::memcpy(out, text.data(), copied);
        out[copied] = '\0';
    }
}

void increment(std::uint64_t& value) noexcept
{
    if (value != std::numeric_limits<std::uint64_t>::max()) ++value;
}

enum class MetadataLifecycle { Active, Inactive, Stopped };

struct EventData {
    ams_mel_ir_image_metadata_event_v1 view{};
    std::vector<ams_mel_ir_bad_pixel_v1> pixels;

    void bind() noexcept
    {
        view.bad_pixel_list.pixels = {pixels.data(), pixels.size()};
    }
};

std::unique_ptr<EventData> copy_bad_pixels(const irmel::BadPixelList *input)
{
    if (!input) return {};
    auto event = std::make_unique<EventData>();
    event->view.kind = AMS_MEL_IR_IMAGE_METADATA_BAD_PIXEL_LIST;
    event->view.bad_pixel_list.reported_size = input->getSize();
    event->view.bad_pixel_list.reported_count = input->getBadPixelCount();
    const auto& source = input->getBadPixelList();
    event->pixels.reserve(source.size());
    for (const auto& pixel : source) {
        const auto reason = static_cast<std::uint32_t>(pixel.getReason());
        if (reason != AMS_MEL_IR_BAD_PIXEL_REASON_UNKNOWN) return {};
        event->pixels.push_back({pixel.getRow(), pixel.getCol(), reason});
    }
    event->bind();
    return event;
}

ams_mel_ir_az_el_v1 az_el(const AzEl& value) noexcept { return {value.az, value.el}; }
ams_mel_euler_v1 euler(const mel::Euler& value) noexcept
{ return {value.getRoll(), value.getPitch(), value.getYaw()}; }

std::unique_ptr<EventData> copy_line_of_sight_report(const irmel::LineOfSightReport *input)
{
    if (!input) return {};
    auto event = std::make_unique<EventData>();
    event->view.kind = AMS_MEL_IR_IMAGE_METADATA_LINE_OF_SIGHT_REPORT;
    auto& output = event->view.line_of_sight_report;
    output.system_time_ns = input->getSystemTime().count();
    output.pointing_angle = az_el(input->getPointingAngle());
    output.pointing_angle_rates = az_el(input->getPointingAngleRates());
    output.at_speed = input->getAtSpeed() ? UINT8_C(1) : UINT8_C(0);
    output.in_tolerance = input->getInTolerance() ? UINT8_C(1) : UINT8_C(0);
    output.platform_attitude = euler(input->getPlatformAttitude());
    output.validity_flag_bitfield = input->getValidityFlagBitfield();
    output.image_rotation_rad = input->getImageRotation();
    return event;
}

std::unique_ptr<EventData> copy_line_of_sight_euler(const irmel::LineOfSightEuler *input)
{
    if (!input) return {};
    auto event = std::make_unique<EventData>();
    event->view.kind = AMS_MEL_IR_IMAGE_METADATA_LINE_OF_SIGHT_EULER;
    auto& output = event->view.line_of_sight_euler;
    output.system_time_ns = input->getSystemTime().count();
    output.attitude = euler(input->getAttitude());
    output.attitude_rates = euler(input->getAttitudeRates());
    return event;
}

std::unique_ptr<EventData> copy_navigation_response(const irmel::NavigationReportResp *input)
{
    if (!input) return {};
    auto event = std::make_unique<EventData>();
    event->view.kind = AMS_MEL_IR_IMAGE_METADATA_NAVIGATION_RESPONSE;
    auto& output = event->view.navigation_response;
    output.system_time_ns = input->getSystemTime().count();
    output.command_id = input->getCommandID();
    output.request_id = input->getReqId();
    return event;
}
} // namespace

struct ImageMetadataState {
    std::mutex mutex;
    std::condition_variable ready;
    std::deque<std::unique_ptr<EventData>> queue;
    std::size_t capacity{};
    ams_mel_ir_metadata_counters_v1 counters{};
    MetadataLifecycle lifecycle{MetadataLifecycle::Active};

    template<typename Input, typename Copy>
    void callback(const Input *input, Copy copy) noexcept
    {
        try {
            {
                std::lock_guard lock{mutex};
                increment(counters.events_received);
                if (lifecycle != MetadataLifecycle::Active) return;
            }
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
            if (const char *value = std::getenv("AMS_MEL_TEST_IMAGE_CALLBACK_FAILURE");
                value && std::strcmp(value, "allocation") == 0) {
                (void)unsetenv("AMS_MEL_TEST_IMAGE_CALLBACK_FAILURE");
                throw std::bad_alloc{};
            }
#endif
            auto event = copy(input);
            std::lock_guard lock{mutex};
            if (lifecycle != MetadataLifecycle::Active) return;
            if (!event) {
                increment(counters.malformed_or_unsupported);
                return;
            }
            if (queue.size() >= capacity) {
                increment(counters.events_dropped_queue_full);
                return;
            }
            queue.push_back(std::move(event));
            ready.notify_one();
        } catch (...) {
            try {
                std::lock_guard lock{mutex};
                increment(counters.malformed_or_unsupported);
            } catch (...) {
                /* No exception crosses the provider callback boundary. */
            }
        }
    }

    void bad_pixels(const irmel::BadPixelList *input) noexcept
    { callback(input, copy_bad_pixels); }
    void line_of_sight_report(const irmel::LineOfSightReport *input) noexcept
    { callback(input, copy_line_of_sight_report); }
    void line_of_sight_euler(const irmel::LineOfSightEuler *input) noexcept
    { callback(input, copy_line_of_sight_euler); }
    void navigation_response(const irmel::NavigationReportResp *input) noexcept
    { callback(input, copy_navigation_response); }
};

struct ams_mel_ir_image_metadata { std::shared_ptr<ImageMetadataState> state; };
struct ams_mel_ir_image_metadata_event { std::unique_ptr<EventData> data; };

void image_metadata_stream_stopped(const std::shared_ptr<ImageMetadataState>& state) noexcept
{
    if (!state) return;
    try {
        std::lock_guard lock{state->mutex};
        if (state->lifecycle == MetadataLifecycle::Active) state->lifecycle = MetadataLifecycle::Stopped;
        state->ready.notify_all();
    } catch (...) {
        /* Stream teardown must remain noexcept. */
    }
}

extern "C" ams_mel_status_t ams_mel_ir_image_metadata_open(
    ams_mel_ir_stream *stream, std::size_t queue_capacity,
    ams_mel_ir_image_metadata **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!stream || !stream->state || !queue_capacity || !output || *output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    std::shared_ptr<ImageMetadataState> state;
    try {
        std::shared_ptr<irmel::ImageChannel> channel;
        state = std::make_shared<ImageMetadataState>();
        state->capacity = queue_capacity;
        if (!claim_image_metadata(*stream->state, state, channel))
            return AMS_MEL_INVALID_ARGUMENT;
        const auto bad_pixels = channel->registerMetadataCallback(
            [state](irmel::Channel&, const irmel::BadPixelList *const value) { state->bad_pixels(value); });
        if (bad_pixels != irmel::Return::Success) {
            std::lock_guard lock{state->mutex};
            state->lifecycle = MetadataLifecycle::Inactive;
            diagnostic("BadPixelList callback registration failed", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        const auto report = channel->registerMetadataCallback(
            [state](irmel::Channel&, const irmel::LineOfSightReport *const value) {
                state->line_of_sight_report(value);
            });
        if (report != irmel::Return::Success) {
            std::lock_guard lock{state->mutex};
            state->lifecycle = MetadataLifecycle::Inactive;
            diagnostic("LineOfSightReport callback registration failed", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        const auto euler_callback = channel->registerMetadataCallback(
            [state](irmel::Channel&, const irmel::LineOfSightEuler *const value) {
                state->line_of_sight_euler(value);
            });
        if (euler_callback != irmel::Return::Success) {
            std::lock_guard lock{state->mutex};
            state->lifecycle = MetadataLifecycle::Inactive;
            diagnostic("LineOfSightEuler callback registration failed", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        const auto navigation_response = channel->registerMetadataCallback(
            [state](irmel::Channel&, const irmel::NavigationReportResp *const value) {
                state->navigation_response(value);
            });
        if (navigation_response != irmel::Return::Success) {
            std::lock_guard lock{state->mutex};
            state->lifecycle = MetadataLifecycle::Inactive;
            diagnostic("NavigationReportResp callback registration failed", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        auto owner = std::make_unique<ams_mel_ir_image_metadata>();
        owner->state = std::move(state);
        *output = owner.release();
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        image_metadata_stream_stopped(state);
        diagnostic("Image metadata allocation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        if (state) {
            std::lock_guard lock{state->mutex};
            state->lifecycle = MetadataLifecycle::Inactive;
        }
        diagnostic("Image metadata callback registration exception", out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_image_metadata_receive(
    ams_mel_ir_image_metadata *metadata, std::uint32_t timeout_ms,
    ams_mel_ir_image_metadata_event **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!metadata || !metadata->state || !output || *output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::unique_lock lock{metadata->state->mutex};
        if (metadata->state->queue.empty() && metadata->state->lifecycle == MetadataLifecycle::Active && timeout_ms) {
            metadata->state->ready.wait_for(lock, std::chrono::milliseconds{timeout_ms}, [&] {
                return !metadata->state->queue.empty() ||
                       metadata->state->lifecycle != MetadataLifecycle::Active;
            });
        }
        if (!metadata->state->queue.empty()) {
            auto owner = std::make_unique<ams_mel_ir_image_metadata_event>();
            owner->data = std::move(metadata->state->queue.front());
            metadata->state->queue.pop_front();
            *output = owner.release();
            return AMS_MEL_OK;
        }
        return metadata->state->lifecycle == MetadataLifecycle::Active ?
               AMS_MEL_TIMEOUT : AMS_MEL_STREAM_STOPPED;
    } catch (...) {
        diagnostic("BadPixelList metadata receive failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_image_metadata_get_counters(
    const ams_mel_ir_image_metadata *metadata, ams_mel_ir_metadata_counters_v1 *output,
    char *out, std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!metadata || !metadata->state || !output || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try { std::lock_guard lock{metadata->state->mutex}; *output = metadata->state->counters; return AMS_MEL_OK; }
    catch (...) { return AMS_MEL_INTERNAL_ERROR; }
}

extern "C" ams_mel_status_t ams_mel_ir_image_metadata_close(
    ams_mel_ir_image_metadata **metadata, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!metadata || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try {
        auto *owner = std::exchange(*metadata, nullptr);
        if (owner) {
            std::lock_guard lock{owner->state->mutex};
            if (owner->state->lifecycle == MetadataLifecycle::Active)
                owner->state->lifecycle = MetadataLifecycle::Inactive;
            owner->state->ready.notify_all();
        }
        delete owner;
        return AMS_MEL_OK;
    } catch (...) { return AMS_MEL_INTERNAL_ERROR; }
}

extern "C" ams_mel_status_t ams_mel_ir_image_metadata_event_view(
    const ams_mel_ir_image_metadata_event *event,
    const ams_mel_ir_image_metadata_event_v1 **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!event || !event->data || !output || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    *output = &event->data->view;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_image_metadata_event_close(
    ams_mel_ir_image_metadata_event **event, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!event || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try { delete std::exchange(*event, nullptr); return AMS_MEL_OK; }
    catch (...) { return AMS_MEL_INTERNAL_ERROR; }
}
