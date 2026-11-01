#include <ams_mel/abi.h>
#include "internal/ir_nuc.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <type_traits>
#include <utility>
#include <variant>

namespace {
using namespace ams::iface;
using ams_mel::internal::nuc_diagnostic;

static_assert(static_cast<std::uint32_t>(irmel::TempPointType::NUC_Point) ==
              AMS_MEL_IR_TEMP_POINT_NUC_POINT);
static_assert(static_cast<std::uint32_t>(irmel::TempPointType::MFA_Estimate) ==
              AMS_MEL_IR_TEMP_POINT_MFA_ESTIMATE);
static_assert(static_cast<std::uint32_t>(irmel::TempPointType::Other) ==
              AMS_MEL_IR_TEMP_POINT_OTHER);
static_assert(static_cast<std::uint32_t>(irmel::BandType::UV_Vacuum) ==
              AMS_MEL_IR_BAND_UV_VACUUM);
static_assert(std::variant_size_v<irmel::TempPoint::PixelValue> == 3U);
static_assert(std::is_same_v<std::variant_alternative_t<0, irmel::TempPoint::PixelValue>,
                             std::uint8_t>);
static_assert(std::is_same_v<std::variant_alternative_t<1, irmel::TempPoint::PixelValue>,
                             std::uint16_t>);
static_assert(std::is_same_v<std::variant_alternative_t<2, irmel::TempPoint::PixelValue>,
                             std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<irmel::TempPoint>().getTimestamp()),
                             std::chrono::nanoseconds>);

void increment(std::uint64_t& value) noexcept
{
    if (value != std::numeric_limits<std::uint64_t>::max()) ++value;
}

bool copy_pixel(const irmel::TempPoint::PixelValue& input,
                ams_mel_ir_pixel_value_v1& output) noexcept
{
    if (input.valueless_by_exception()) return false;
    switch (input.index()) {
    case 0U:
        output = {AMS_MEL_IR_PIXEL_VALUE_UINT8, *std::get_if<0>(&input)};
        return true;
    case 1U:
        output = {AMS_MEL_IR_PIXEL_VALUE_UINT16, *std::get_if<1>(&input)};
        return true;
    case 2U:
        output = {AMS_MEL_IR_PIXEL_VALUE_UINT32, *std::get_if<2>(&input)};
        return true;
    default:
        return false;
    }
}
} // namespace

namespace ams_mel::internal {
std::unique_ptr<NucEventData> copy_nuc_temp_data(const irmel::NUC_TempData *input)
{
    if (!input) return {};
    auto event = std::make_unique<NucEventData>();
    const auto source = input->getTempPoints();
    event->points.reserve(source.size());
    for (const auto& point : source) {
        ams_mel_ir_temp_point_v1 copied{};
        copied.timestamp_ns = point.getTimestamp().count();
        const auto band = point.getFrameBand();
        copied.frame_band.type = static_cast<std::uint32_t>(band.getBandType());
        if (copied.frame_band.type > AMS_MEL_IR_BAND_UV_VACUUM) return {};
        copied.frame_band.min_wavelength_m = band.getMinWavelength();
        copied.frame_band.max_wavelength_m = band.getMaxWavelength();
        if (!copy_pixel(point.getPixelValue(), copied.pixel_value)) return {};
        copied.temperature_k = point.getTemperature();
        copied.point_type = static_cast<std::uint32_t>(point.getPointType());
        if (copied.point_type > AMS_MEL_IR_TEMP_POINT_OTHER) return {};
        event->points.push_back(copied);
    }
    event->view.temp_points = {event->points.empty() ? nullptr : event->points.data(),
                               event->points.size()};
    return event;
}
} // namespace ams_mel::internal

void NucState::receive(const irmel::NUC_TempData *input) noexcept
{
    bool counted = false;
    try {
        bool active;
        {
            std::lock_guard lock{mutex};
            ++callbacks;
            counted = true;
            increment(counters.events_received);
            active = lifecycle == NucLifecycle::Active;
        }
        std::unique_ptr<NucEventData> event;
        if (active) {
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
            if (const char *value = std::getenv("AMS_MEL_TEST_NUC_CALLBACK_FAILURE");
                value && std::strcmp(value, "allocation") == 0) {
                (void)unsetenv("AMS_MEL_TEST_NUC_CALLBACK_FAILURE");
                throw std::bad_alloc{};
            }
#endif
            event = ams_mel::internal::copy_nuc_temp_data(input);
        }
        std::lock_guard lock{mutex};
        if (active && lifecycle == NucLifecycle::Active) {
            if (!event) increment(counters.malformed_or_unsupported);
            else if (queue.size() >= capacity) increment(counters.events_dropped_queue_full);
            else {
                queue.push_back(std::move(event));
                ready.notify_one();
            }
        }
        if (--callbacks == 0U) callbacks_done.notify_all();
    } catch (...) {
        /* No exception crosses the provider callback boundary. A copying or
         * allocation failure rejects this event only. */
        try {
            std::lock_guard lock{mutex};
            increment(counters.malformed_or_unsupported);
            if (counted && --callbacks == 0U) callbacks_done.notify_all();
        } catch (...) {
        }
    }
}

void NucState::deactivate() noexcept
{
    try {
        std::lock_guard lock{mutex};
        if (lifecycle == NucLifecycle::Active) lifecycle = NucLifecycle::Inactive;
        ready.notify_all();
    } catch (...) {
    }
}

void NucState::stop() noexcept
{
    try {
        std::lock_guard lock{mutex};
        if (lifecycle == NucLifecycle::Active) lifecycle = NucLifecycle::Stopped;
        ready.notify_all();
    } catch (...) {
    }
}

void NucState::fail() noexcept
{
    try {
        std::lock_guard lock{mutex};
        lifecycle = NucLifecycle::Failed;
        ready.notify_all();
    } catch (...) {
    }
}

void NucState::drain_and_stop() noexcept
{
    try {
        std::unique_lock lock{mutex};
        callbacks_done.wait(lock, [this] { return callbacks == 0U; });
        if (lifecycle == NucLifecycle::Active) lifecycle = NucLifecycle::Stopped;
        ready.notify_all();
    } catch (...) {
    }
}

extern "C" ams_mel_status_t ams_mel_ir_nuc_receive(
    ams_mel_ir_nuc_metadata *metadata, std::uint32_t timeout_ms,
    ams_mel_ir_nuc_event **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    nuc_diagnostic("", out, capacity, required);
    if (!metadata || !metadata->state || !output || *output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        auto& state = *metadata->state;
        std::unique_lock lock{state.mutex};
        if (state.queue.empty() && state.lifecycle == NucLifecycle::Active && timeout_ms)
            state.ready.wait_for(lock, std::chrono::milliseconds{timeout_ms}, [&] {
                return !state.queue.empty() || state.lifecycle != NucLifecycle::Active;
            });
        if (!state.queue.empty()) {
            auto owner = std::make_unique<ams_mel_ir_nuc_event>();
            owner->data = std::move(state.queue.front());
            state.queue.pop_front();
            *output = owner.release();
            return AMS_MEL_OK;
        }
        if (state.lifecycle == NucLifecycle::Failed) {
            nuc_diagnostic("NUC_TempData channel cleanup failed", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        return state.lifecycle == NucLifecycle::Active ? AMS_MEL_TIMEOUT : AMS_MEL_STREAM_STOPPED;
    } catch (...) {
        nuc_diagnostic("NUC_TempData metadata receive failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_nuc_get_counters(
    const ams_mel_ir_nuc_metadata *metadata, ams_mel_ir_metadata_counters_v1 *output,
    char *out, std::size_t capacity, std::size_t *required) noexcept
{
    nuc_diagnostic("", out, capacity, required);
    if (!metadata || !metadata->state || !output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::lock_guard lock{metadata->state->mutex};
        *output = metadata->state->counters;
        return AMS_MEL_OK;
    } catch (...) {
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_nuc_close(
    ams_mel_ir_nuc_metadata **metadata, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    nuc_diagnostic("", out, capacity, required);
    if (!metadata || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    auto *owner = std::exchange(*metadata, nullptr);
    if (owner && owner->state) owner->state->deactivate();
    delete owner;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_nuc_event_view(
    const ams_mel_ir_nuc_event *event, const ams_mel_ir_nuc_temp_data_event_v1 **output,
    char *out, std::size_t capacity, std::size_t *required) noexcept
{
    nuc_diagnostic("", out, capacity, required);
    if (!event || !event->data || !output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    *output = &event->data->view;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_nuc_event_close(
    ams_mel_ir_nuc_event **event, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    nuc_diagnostic("", out, capacity, required);
    if (!event || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    delete std::exchange(*event, nullptr);
    return AMS_MEL_OK;
}
