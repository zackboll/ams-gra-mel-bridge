#include <ams_mel/abi.h>
#include "internal/ir_channel.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace ams::iface;

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

void diagnostic(std::string_view text, char *out, std::size_t capacity,
                std::size_t *required) noexcept
{
    if (required) *required = text.size() + 1U;
    if (out && capacity) {
        std::size_t copied = std::min(text.size(), capacity - 1U);
        while (copied && !valid_utf8(text.substr(0U, copied))) --copied;
        std::memcpy(out, text.data(), copied);
        out[copied] = '\0';
    }
}

struct OwnedID { std::array<std::uint8_t, 16> uuid{}; std::string label; };
struct OwnedLocation { double x{}, y{}, z{}; std::string key, system; };
struct OwnedImageBand {
    std::uint32_t index{};
    std::vector<ams_mel_ir_band_info_v1> bands;
};
struct CapabilityData {
    ams_mel_ir_channel_capability_v1 view{};
    OwnedID channel_id, platform_id;
    OwnedLocation location;
    std::vector<std::uint32_t> sensors, channels, metadata, nav_frames;
    std::vector<OwnedImageBand> image_bands;
    std::vector<ams_mel_ir_image_band_v1> image_band_views;
};

ams_mel_string_view_v1 string_view(const std::string& value) noexcept
{ return {value.empty() ? nullptr : value.data(), value.size()}; }

ams_mel_uci_id_v1 id_view(const OwnedID& id) noexcept
{
    ams_mel_uci_id_v1 result{};
    std::copy(id.uuid.begin(), id.uuid.end(), result.uuid);
    result.descriptive_label = string_view(id.label);
    return result;
}

bool copy_id(const mel::UCI_ID& input, OwnedID& output)
{
    if (!valid_utf8(input.getDescriptiveLabel())) return false;
    output.uuid = input.getUUID();
    output.label = input.getDescriptiveLabel();
    return true;
}

bool copy_capability(const irmel::ChannelCapability& input, CapabilityData& output)
{
    if (!copy_id(input.getChanID(), output.channel_id) ||
        !copy_id(input.getPlatform(), output.platform_id)) return false;
    const auto& location = input.getSensorLocation();
    output.location = {location.getOffsetX(), location.getOffsetY(),
        location.getOffsetZ(), location.getLocationId().getKey(),
        location.getLocationId().getSystemName()};
    if (!valid_utf8(output.location.key) || !valid_utf8(output.location.system)) return false;
    const auto pixel = static_cast<std::uint32_t>(input.getFormat());
    if (pixel > AMS_MEL_IR_PIXEL_BAYER) return false;
    for (const auto value : input.getSensorTypes()) {
        const auto raw = static_cast<std::uint32_t>(value);
        if (raw >= static_cast<std::uint32_t>(irmel::SensorType::MAXEXCLUSIVE)) return false;
        output.sensors.push_back(raw);
    }
    for (const auto value : input.getChannelTypes()) {
        const auto raw = static_cast<std::uint32_t>(value);
        if (raw > AMS_MEL_IR_CHANNEL_RESERVED_2) return false;
        output.channels.push_back(raw);
    }
    for (const auto value : input.getChannelMetadataCapabilities()) {
        const auto raw = static_cast<std::uint32_t>(value);
        if (raw > AMS_MEL_IR_METADATA_RESERVED_10) return false;
        output.metadata.push_back(raw);
    }
    for (const auto& [index, values] : input.getImageBands()) {
        OwnedImageBand copied;
        copied.index = index;
        for (const auto& value : values) {
            const auto type = static_cast<std::uint32_t>(value.getBandType());
            if (type > AMS_MEL_IR_BAND_UV_VACUUM) return false;
            copied.bands.push_back({type, value.getMinWavelength(), value.getMaxWavelength()});
        }
        output.image_bands.push_back(std::move(copied));
    }
    for (const auto value : input.getNavFrames()) {
        const auto raw = static_cast<std::uint32_t>(value);
        if (raw > AMS_MEL_IR_COORDINATE_NED_SENSOR) return false;
        output.nav_frames.push_back(raw);
    }
    output.image_band_views.reserve(output.image_bands.size());
    for (const auto& value : output.image_bands)
        output.image_band_views.push_back({value.index, {value.bands.data(), value.bands.size()}});
    output.view = {id_view(output.channel_id), input.getHeight(), input.getWidth(),
        input.getBitDepth(), input.getRowPitch(), input.getBufferSize(), input.getImageSize(),
        input.getNumberOfBands(), pixel, {output.sensors.data(), output.sensors.size()},
        id_view(output.platform_id), {output.location.x, output.location.y, output.location.z,
            string_view(output.location.key), string_view(output.location.system)},
        {output.channels.data(), output.channels.size()}, input.getTaskScheduleDepth(),
        input.getOdcAvail() ? 1U : 0U, input.getNucAvail() ? 1U : 0U,
        {output.metadata.data(), output.metadata.size()},
        {output.image_band_views.data(), output.image_band_views.size()},
        {output.nav_frames.data(), output.nav_frames.size()}};
    return true;
}
} // namespace

struct ams_mel_ir_channel_capability { std::unique_ptr<CapabilityData> data; };

ams_mel_status_t ams_mel::internal::snapshot_capability(
    const irmel::Channel& channel, ams_mel_ir_channel_capability **output,
    char *out, std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!output || *output || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try {
        auto owner = std::make_unique<ams_mel_ir_channel_capability>();
        owner->data = std::make_unique<CapabilityData>();
        const auto value = channel.getCapabilities();
        if (!copy_capability(value, *owner->data)) {
            diagnostic("provider returned malformed ChannelCapability", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        *output = owner.release();
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        diagnostic("capability snapshot allocation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (const std::exception& error) {
        const char *what = error.what();
        const std::string_view text = what ? std::string_view{what} : std::string_view{};
        diagnostic(!text.empty() && valid_utf8(text) ? text : "provider capability exception",
                   out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    } catch (...) {
        diagnostic("unknown provider capability exception", out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_channel_capability_view(
    const ams_mel_ir_channel_capability *capability,
    const ams_mel_ir_channel_capability_v1 **output, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!capability || !capability->data || !output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    *output = &capability->data->view;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_channel_capability_close(
    ams_mel_ir_channel_capability **capability, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!capability || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try { delete std::exchange(*capability, nullptr); return AMS_MEL_OK; }
    catch (...) { return AMS_MEL_INTERNAL_ERROR; }
}
