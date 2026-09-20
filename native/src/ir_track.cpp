#include <ams_mel/abi.h>
#include "internal.hpp"
#include "internal/ir_channel.hpp"

#include <irmel/library/irmel-types/CandidateObjectMessage.h>
#include <irmel/library/irmel-types/CandidateObjectPreProcMessage.h>
#include <irmel/library/irmel-types/ChannelMetadataCapabilityType.h>
#include <irmel/library/irmel-types/RequestSystemTrackData.h>
#include <irmel/library/irmel-types/SystemTrackDataResponse.h>
#include <irmel/library/track/IRSTTrackReport.h>
#include <irmel/library/track/TrackChannel.h>
#include <irmel/library/track/TrackDataUpdate.h>
#include <irmel/library/track/TrackStatus.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
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

bool valid_view(const ams_mel_string_view_v1& value) noexcept
{ return value.data ? valid_utf8({value.data, value.size}) : value.size == 0U; }

std::string copy_view(const ams_mel_string_view_v1& value)
{ return value.size ? std::string{value.data, value.size} : std::string{}; }

mel::UCI_ID convert_id(const ams_mel_uci_id_v1& value)
{
    std::array<std::uint8_t, mel::UUID_SIZE> uuid{};
    std::copy(std::begin(value.uuid), std::end(value.uuid), uuid.begin());
    return {uuid, copy_view(value.descriptive_label)};
}

void increment(std::uint64_t& value) noexcept { if (value != UINT64_MAX) ++value; }

/* The nine published MEL ErrorCode values, mapped through the existing shared
 * AMS_MEL_ERROR_* representation. Anything else is a malformed provider
 * outcome. */
ams_mel_error_code_t map_error(mel::ErrorCode code, bool& known) noexcept
{
    known = true;
    switch (code) {
    case mel::ErrorCode::None: return AMS_MEL_ERROR_NONE;
    case mel::ErrorCode::InvalidId: return AMS_MEL_ERROR_INVALID_ID;
    case mel::ErrorCode::InvalidState: return AMS_MEL_ERROR_INVALID_STATE;
    case mel::ErrorCode::InvalidParameters: return AMS_MEL_ERROR_INVALID_PARAMETERS;
    case mel::ErrorCode::InsufficientPermissions:
        return AMS_MEL_ERROR_INSUFFICIENT_PERMISSIONS;
    case mel::ErrorCode::InsufficientResources:
        return AMS_MEL_ERROR_INSUFFICIENT_RESOURCES;
    case mel::ErrorCode::InsufficientLocalResources:
        return AMS_MEL_ERROR_INSUFFICIENT_LOCAL_RESOURCES;
    case mel::ErrorCode::InsufficientRemoteResources:
        return AMS_MEL_ERROR_INSUFFICIENT_REMOTE_RESOURCES;
    case mel::ErrorCode::Unsupported: return AMS_MEL_ERROR_UNSUPPORTED;
    }
    known = false;
    return AMS_MEL_ERROR_NONE;
}

/* Upstream TrackStatus is exactly Create=0, Update=1, Predict=2, Delete=3 and
 * declares no MaxExclusive value, so anything above Delete is rejected. */
bool convert_track_status(ams_mel_ir_track_status_t value,
                          irmel::TrackStatus& status) noexcept
{
    if (value > AMS_MEL_IR_TRACK_STATUS_DELETE) return false;
    status = static_cast<irmel::TrackStatus>(value);
    return true;
}

mel::Directional convert_directional(const ams_mel_ir_directional_v1& value) noexcept
{ return {value.x, value.y, value.z}; }

/* Builds the complete upstream TrackDataUpdate through the published setters
 * only; no provider object layout is assumed. Every required setter is called
 * exactly once with the corresponding C field, including all 21 covariance
 * terms. Times stay in epoch seconds and no value is clamped or normalized. */
irmel::TrackDataUpdate build_update(const ams_mel_ir_track_data_update_v1& input,
                                    irmel::TrackStatus status)
{
    irmel::TrackDataUpdate value;
    value.setPlatformId(input.platform_id);
    value.setCapabilityUUID(convert_id(input.capability_uuid));
    value.setActivityUUID(convert_id(input.activity_uuid));
    value.setTrackId(input.track_id);
    value.setEntityUUID(convert_id(input.entity_uuid));
    value.setTrackStatus(status);
    value.setTimeOfValidity(input.time_of_validity_seconds);
    value.setTimeOfLastUpdate(input.time_of_last_update_seconds);
    value.setTrackPosition(convert_directional(input.track_position_ecef));
    value.setTrackVelocity(convert_directional(input.track_velocity_ecef));

    const ams_mel_ir_track_covariance_v1& c = input.covariance;
    value.setTrackCovarianceXX(c.xx);
    value.setTrackCovarianceXY(c.xy);
    value.setTrackCovarianceXZ(c.xz);
    value.setTrackCovarianceXVx(c.x_vx);
    value.setTrackCovarianceXVy(c.x_vy);
    value.setTrackCovarianceXVz(c.x_vz);
    value.setTrackCovarianceYY(c.yy);
    value.setTrackCovarianceYZ(c.yz);
    value.setTrackCovarianceYVx(c.y_vx);
    value.setTrackCovarianceYVy(c.y_vy);
    value.setTrackCovarianceYVz(c.y_vz);
    value.setTrackCovarianceZZ(c.zz);
    value.setTrackCovarianceZVx(c.z_vx);
    value.setTrackCovarianceZVy(c.z_vy);
    value.setTrackCovarianceZVz(c.z_vz);
    value.setTrackCovarianceVxVx(c.vx_vx);
    value.setTrackCovarianceVxVy(c.vx_vy);
    value.setTrackCovarianceVxVz(c.vx_vz);
    value.setTrackCovarianceVyVy(c.vy_vy);
    value.setTrackCovarianceVyVz(c.vy_vz);
    value.setTrackCovarianceVzVz(c.vz_vz);

    value.setManeuverProbability(input.maneuver_probability);
    value.setTrackQuality(input.track_quality);
    return value;
}

/* The repository's established uint8_t representation for published bool
 * values: exactly 0 or 1 is accepted and anything else is malformed input. */
bool convert_bool(std::uint8_t value, bool& result) noexcept
{
    if (value > 1U) return false;
    result = value != 0U;
    return true;
}

AzEl convert_az_el(const ams_mel_ir_az_el_v1& value) noexcept
{
    AzEl result;
    result.az = value.azimuth_rad;
    result.el = value.elevation_rad;
    return result;
}

/* Builds the complete upstream SystemTrackDataResponse (@Optional) through the
 * published setters only; no provider object layout is assumed. Every published
 * setter is called exactly once with the corresponding C field. The system time
 * stays signed nanoseconds and no range, rate, error, or angle value is clamped
 * or normalized, because the upstream setters perform no such validation. */
irmel::SystemTrackDataResponse build_response(
    const ams_mel_ir_system_track_data_response_v1& input, bool az_el_valid,
    bool range_valid)
{
    irmel::SystemTrackDataResponse value;
    value.setCommandID(input.command_id);
    value.setSystemTime(std::chrono::nanoseconds{input.system_time_ns});
    value.setRequestId(input.request_id);
    value.setTrackId(input.track_id);
    value.setRange(input.range_m);
    value.setRangeRate(input.range_rate_mps);
    value.setRangeError(input.range_error_m);
    value.setRangeRateError(input.range_rate_error_mps);
    value.setAzElValid(az_el_valid);
    value.setInertialAzEl(convert_az_el(input.inertial_az_el));
    value.setAzElError(convert_az_el(input.az_el_error));
    value.setRangeValid(range_valid);
    return value;
}

/* Complete IRSTTrackReport copy. Every upstream getter is read exactly once and
 * every floating-point value is copied verbatim: no clamping, no normalization,
 * and no narrowing. Upstream IrstTrackState and IrstTrackMode declare no
 * MaxExclusive value, so anything above Dropped or Stare is malformed. */
bool copy_track_report(const irmel::IRSTTrackReport& source,
                       ams_mel_ir_track_report_v1& destination) noexcept
{
    const auto state = static_cast<std::uint32_t>(source.getState());
    const auto mode = static_cast<std::uint32_t>(source.getMode());
    if (state > AMS_MEL_IR_TRACK_STATE_DROPPED) return false;
    if (mode > AMS_MEL_IR_TRACK_MODE_STARE) return false;

    destination.system_time_ns = source.getSystemTime().count();
    destination.activity_id = source.getActivityId();

    const auto& measured = source.getMeasuredNed();
    destination.measured_ned.north = measured.getNorth();
    destination.measured_ned.east = measured.getEast();
    destination.measured_ned.down = measured.getDown();
    destination.measured_intensity = source.getMeasuredIntensity();
    destination.measured_snr = source.getMeasuredSnr();

    const auto& filtered = source.getFilteredNed();
    destination.filtered_ned.north = filtered.getNorth();
    destination.filtered_ned.east = filtered.getEast();
    destination.filtered_ned.down = filtered.getDown();
    destination.filtered_intensity = source.getFilteredIntensity();
    destination.filtered_snr = source.getFilteredSnr();

    destination.range_m = source.getRange();
    destination.range_error_m = source.getRangeError();
    destination.spatial_extent_rad = source.getSpatialExtent();
    destination.track_quality = source.getTrackQuality();
    destination.clutter = source.getClutter();

    destination.age_ns = source.getAge().count();

    destination.state = state;
    destination.mode = mode;
    return true;
}

/* Verbatim copy of the four published RequestSystemTrackData getters. Upstream
 * declares no enum, no optional field, and no range, so there is nothing to
 * validate and nothing to reject: any value the provider supplies is a
 * well-formed request. getSystemTime() is std::chrono::nanoseconds, whose rep
 * is signed, and count() is preserved without any unit conversion. */
void copy_request_system_track_data(
    const irmel::RequestSystemTrackData& source,
    ams_mel_ir_request_system_track_data_v1& destination) noexcept
{
    destination.system_time_ns = source.getSystemTime().count();
    destination.command_id = source.getCommandID();
    destination.request_id = source.getRequestId();
    destination.track_id = source.getTrackId();
}

/* The native event owns every byte its view references. The two vectors carry
 * the variable-size CandidateObjectMessage storage; the view's span pointers
 * are bound to them only after both vectors are fully built and will never be
 * grown again. std::vector storage is heap allocated, so moving the owning
 * unique_ptr through the queue and into the public event owner never
 * invalidates those pointers. No provider-owned pointer, provider STL storage,
 * or callback-stack address is ever referenced by the view. */
struct EventData {
    /* v3 is stored, never v1 or v2: both are frozen ABI records and each is
     * nested at offset 0 of the next, so one allocation serves all three
     * views. The v1 view returns &view.base.base, the v2 view returns
     * &view.base, and the v3 view returns &view. */
    ams_mel_ir_track_metadata_event_v3 view{};
    std::vector<ams_mel_ir_hot_region_v1> hot_regions;
    std::vector<ams_mel_ir_candidate_object_v1> candidate_objects;
    /* Task 029G PreProc storage. It is a separate vector from
     * candidate_objects because the two kinds are never both populated and
     * their element types differ. */
    std::vector<ams_mel_ir_candidate_object_preproc_v1> candidate_preprocs;
};

/* Complete verbatim copy of the published HotRegion getters. Upstream declares
 * exactly INVALID/FLARE/SOLAR/MASK and no MaxExclusive value, so a
 * representation outside 0..3 is malformed. */
bool copy_hot_region(const irmel::HotRegion& source,
                     ams_mel_ir_hot_region_v1& destination) noexcept
{
    const auto type = static_cast<std::uint32_t>(source.getType());
    if (type > AMS_MEL_IR_HOT_REGION_MASK) return false;
    destination.type = type;
    destination.size = source.getSize();
    destination.top = source.getTop();
    destination.left = source.getLeft();
    destination.right = source.getRight();
    destination.bottom = source.getBottom();
    return true;
}

/* Reuses the one canonical XYZ shape; no value is clamped or normalized. */
ams_mel_ir_directional_v1 copy_directional(const irmel::IR_Directional& value) noexcept
{ return {value.getX(), value.getY(), value.getZ()}; }

ams_mel_ir_quaternion_v1 copy_quaternion(const irmel::Quaternion& value) noexcept
{
    return {value.getQuaternionX(), value.getQuaternionY(),
            value.getQuaternionZ(), value.getQuaternionW()};
}

/* Complete SensorInertialState copy through the canonical shared ABI record.
 * Every published getter is read exactly once and no quaternion is
 * renormalized: the upstream setters perform no such validation. */
void copy_inertial_state(const irmel::SensorInertialState& source,
                         ams_mel_ir_sensor_inertial_state_v1& destination) noexcept
{
    destination.system_time_ns = source.getSystemTime().count();
    destination.q_xyzw = copy_quaternion(source.getQ_xyzw());
    destination.q_ecef_xyzw = copy_quaternion(source.getQECEF_xyzw());
    destination.sensor_position = copy_directional(source.getSensorPosition());
    destination.sensor_velocity = copy_directional(source.getSensorVelocity());
    const auto& uncertainties = source.getUncertainties();
    destination.uncertainties.sensor_uncertainties =
        uncertainties.getSensorUncertainties();
    destination.uncertainties.platform_uncertainties =
        uncertainties.getPlatformUncertainties();
}

/* Complete verbatim copy of the published CandidateObject getters. Upstream
 * declares no enum and no constrained field here, so nothing can be malformed;
 * every floating-point value is copied without clamping or normalization and
 * the sensor-relative unit vector is not renormalized. */
void copy_candidate_object(const irmel::CandidateObject& source,
                           ams_mel_ir_candidate_object_v1& destination) noexcept
{
    destination.system_time_ns = source.getSystemTime().count();
    destination.detection_category = source.getDetectionCategory();
    destination.sensor_index = source.getSensorIndex();
    const auto& subpixel = source.getSubpixel();
    destination.subpixel.row = subpixel.getRow();
    destination.subpixel.column = subpixel.getCol();
    destination.intensity = source.getIntensity();
    destination.sensor_relative_unit = copy_directional(source.getSenRelUnit());
    destination.signal_to_interference_ratio = source.getSignalToInterferenceRatio();
    destination.signal_to_noise_ratio = source.getSignalToNoiseRatio();
}

/* Complete deep copy of CandidateObjectMessage into event-owned storage.
 *
 * numberOfCOs is the MEANINGFUL PREFIX LENGTH of the fixed 900-entry upstream
 * array. The pinned headers publish numberOfCOs as the only count and publish
 * no separate array-length validity flag, so exactly
 * candidateObjects[0 .. numberOfCOs-1] is exposed and trailing storage slots
 * are neither read nor converted. A numberOfCOs above MAX_CANDIDATE_OBJECTS is
 * malformed, because it cannot describe a prefix of the published array.
 *
 * The HotRegion vector has no published fixed maximum, so the complete vector
 * is copied in its published order. An invalid HotRegion enum makes the whole
 * message malformed. */
/* The one CandidateObjectHeader scalar copy, shared by both candidate message
 * kinds. numberOfCOs is copied verbatim; no consistency rule against any
 * container length is applied here. */
void copy_candidate_header(const irmel::CandidateObjectHeader& source,
                           ams_mel_ir_candidate_object_header_v1& destination) noexcept
{
    destination.number_of_cos = source.getNumberOfCOs();
    destination.stack_frame_index = source.getStackFrameIndex();
    destination.cfar = source.getCFAR();
    destination.validity_flag_bitfield = source.getValidityFlagBitField();
    destination.tov_utc_ns = source.getTOVutcNanoseconds().count();
}

/* The one HotRegion vector copy, shared by both candidate message kinds. The
 * upstream vector has no published fixed maximum, so the COMPLETE vector is
 * copied in its published order; an invalid enum makes the whole message
 * malformed. The destination span is bound by the caller, only after the
 * vector is final. */
bool copy_hot_regions(const std::vector<irmel::HotRegion>& regions,
                      std::vector<ams_mel_ir_hot_region_v1>& destination)
{
    destination.resize(regions.size());
    for (std::size_t index = 0; index < regions.size(); ++index)
        if (!copy_hot_region(regions[index], destination[index])) return false;
    return true;
}

/* Binds a span to event-owned vector storage. The storage is heap owned, so a
 * later move of the owning EventData keeps the pointer valid; an empty vector
 * keeps a NULL data pointer and a zero size. */
template <typename Span, typename Vector>
void bind_span(Span& span, const Vector& storage) noexcept
{
    span.data = storage.empty() ? nullptr : storage.data();
    span.size = storage.size();
}

bool copy_candidate_object_message(const irmel::CandidateObjectMessage& source,
                                   EventData& event)
{
    const auto& header = source.getHeader();
    const auto count = header.getNumberOfCOs();
    if (static_cast<std::uint32_t>(count) > AMS_MEL_IR_MAX_CANDIDATE_OBJECTS)
        return false;

    if (!copy_hot_regions(header.getHotRegions(), event.hot_regions)) return false;

    const auto& objects = source.getCandidateObjects();
    event.candidate_objects.resize(count);
    for (std::size_t index = 0; index < static_cast<std::size_t>(count); ++index)
        copy_candidate_object(objects[index], event.candidate_objects[index]);

    auto& view = event.view.base.candidate_object_message;
    copy_candidate_header(header, view.header);
    copy_inertial_state(source.getInertialState(), view.inertial_state);

    /* Bound only after both vectors are final; the storage is heap owned, so a
     * later move of this EventData keeps these pointers valid. */
    bind_span(view.hot_regions, event.hot_regions);
    bind_span(view.candidate_objects, event.candidate_objects);
    return true;
}

/* Complete verbatim copy of the published CandidateObjectPreProc getters.
 * Every one of the seventeen getters is read exactly once.
 *
 * Nothing is validated here because upstream constrains nothing: no enum is
 * published, candidateObjectQuality is documented as "0 to 1" but its setter
 * enforces no such range, the sensor-relative unit vector is not renormalized,
 * the nested quaternions are not normalized, detectionCategory is not decoded,
 * and the two sigma values are not reinterpreted.
 *
 * The 3 by 3 background patch is copied ROW-MAJOR, one element at a time: the
 * upstream std::array object itself is never memcpy'd into C storage. */
void copy_candidate_preproc(
    const irmel::CandidateObjectPreProc& source,
    ams_mel_ir_candidate_object_preproc_v1& destination) noexcept
{
    destination.system_time_ns = source.getSystemTime().count();
    destination.detection_category = source.getDetectionCategory();
    destination.sensor_index = source.getSensorIndex();
    const auto& subpixel = source.getSubpixel();
    destination.subpixel.row = subpixel.getRow();
    destination.subpixel.column = subpixel.getCol();
    destination.intensity = source.getIntensity();
    destination.sensor_relative_unit = copy_directional(source.getSenRelUnit());
    destination.signal_to_interference_ratio = source.getSignalToInterferenceRatio();
    destination.signal_to_noise_ratio = source.getSignalToNoiseRatio();

    const auto& background = source.getCandidateObjectWithBackground();
    for (std::size_t row = 0; row < AMS_MEL_IR_CANDIDATE_BACKGROUND_SIDE; ++row)
        for (std::size_t column = 0;
             column < AMS_MEL_IR_CANDIDATE_BACKGROUND_SIDE; ++column)
            destination.candidate_object_with_background
                .samples[row * AMS_MEL_IR_CANDIDATE_BACKGROUND_SIDE + column] =
                background[row][column];

    destination.clutter = source.getClutter();
    destination.candidate_object_quality = source.getCandidateObjectQuality();
    destination.sir_delta = source.getSirDelta();
    /* The PreProc's OWN nested inertial state, distinct from the message-level
     * one; the same canonical copy is reused. */
    copy_inertial_state(source.getInertialState(), destination.inertial_state);
    /* Upstream bool, normalized to exactly 0 or 1. */
    destination.edge = source.getEdge() ? UINT8_C(1) : UINT8_C(0);
    destination.az_sigma = source.getAzSigma();
    destination.el_sigma = source.getElSigma();
    destination.background_normalizer = source.getBackgroundNormalizer();
}

/* Complete deep copy of CandidateObjectPreProcMessage into event-owned
 * storage.
 *
 * Unlike CandidateObjectMessage, the PreProc container is a
 * std::vector<CandidateObjectPreProc>, which already carries its own explicit
 * size. The pinned headers publish NO invariant requiring
 * numberOfCOs == candidateObjectPreProcs.size(): CandidateObjectHeader is a
 * standalone class shared by both message types, its numberOfCOs has no
 * documented relationship to this vector, and CandidateObjectPreProcMessage
 * validates nothing in setCandidateObjectPreProcs. So numberOfCOs is copied
 * verbatim into the header, the COMPLETE vector is copied using its actual
 * size, no truncation to numberOfCOs happens, and a mismatch is NOT treated as
 * malformed. Both values are preserved and the consumer decides.
 *
 * The message getters return BY VALUE, including the vector, so each is called
 * exactly once and its local result is retained for the whole copy.
 *
 * The HotRegion vector has no published fixed maximum, so the complete vector
 * is copied in its published order. An invalid HotRegion enum makes the whole
 * message malformed, exactly as for CandidateObjectMessage. */
bool copy_candidate_preproc_message(
    const irmel::CandidateObjectPreProcMessage& source, EventData& event)
{
    /* Retained locals: these getters return values, not references. */
    const irmel::CandidateObjectHeader header = source.getCandidateObjectHeader();
    const irmel::SensorInertialState inertial = source.getSensorInertialState();
    const std::vector<irmel::CandidateObjectPreProc> preprocs =
        source.getCandidateObjectPreProcs();

    if (!copy_hot_regions(header.getHotRegions(), event.hot_regions)) return false;

    event.candidate_preprocs.resize(preprocs.size());
    for (std::size_t index = 0; index < preprocs.size(); ++index)
        copy_candidate_preproc(preprocs[index], event.candidate_preprocs[index]);

    auto& view = event.view.candidate_object_preproc_message;
    copy_candidate_header(header, view.header);
    copy_inertial_state(inertial, view.inertial_state);

    /* Bound only after both vectors are final and will never grow again. */
    bind_span(view.hot_regions, event.hot_regions);
    bind_span(view.candidate_object_preprocs, event.candidate_preprocs);
    return true;
}

enum class MetadataLifecycle { Active, Inactive, Stopped, Failed };

/* Follows the proven Instrumentation metadata shape: a bounded DROP-INCOMING
 * FIFO of owned events, saturating counters, and an explicit in-flight callback
 * count whose drain is the quiescence proof. */
struct MetadataState {
    std::mutex mutex;
    std::condition_variable ready, callbacks_done;
    std::deque<std::unique_ptr<EventData>> queue;
    std::size_t capacity{};
    ams_mel_ir_metadata_counters_v1 counters{};
    MetadataLifecycle lifecycle{MetadataLifecycle::Active};
    std::atomic<std::uint64_t> callbacks_in_flight{};

    void fail() noexcept
    {
        try {
            std::lock_guard lock{mutex};
            lifecycle = MetadataLifecycle::Failed;
            ready.notify_all();
        } catch (...) {
            /* No exception may cross the provider callback boundary. */
        }
    }
};

/* Every provider callback increments this count before touching callback state
 * and decrements it on return, including on an exceptional path. */
struct CallbackGuard {
    std::shared_ptr<MetadataState> state;
    explicit CallbackGuard(std::shared_ptr<MetadataState> value) noexcept
        : state(std::move(value))
    { state->callbacks_in_flight.fetch_add(1, std::memory_order_acq_rel); }
    ~CallbackGuard() noexcept
    {
        if (state->callbacks_in_flight.fetch_sub(1, std::memory_order_acq_rel) == 1)
            state->callbacks_done.notify_all();
    }
};

/* Shared provider callback boundary for every implemented Track metadata kind.
 * It never calls into Ada, never lets an exception escape into provider code,
 * and always accounts for the event. A null payload is malformed. The queue
 * drops the INCOMING event when full so the earliest events survive. The
 * builder returns false for a malformed payload; every implemented kind shares
 * one queue, one capacity, and one counter set.
 *
 * The builder receives the whole EventData rather than only the fixed view, so
 * a kind with variable-size storage can populate the event-owned vectors and
 * bind its spans before the event is ever published. */
template <typename Payload, typename Builder>
void metadata_callback_impl(const std::shared_ptr<MetadataState>& state,
                            const Payload *value, Builder build) noexcept
{
    try {
        CallbackGuard guard{state};
        {
            std::lock_guard lock{state->mutex};
            increment(state->counters.events_received);
            if (state->lifecycle != MetadataLifecycle::Active) return;
        }
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
        if (const char *failure = std::getenv("AMS_MEL_TEST_TRACK_CALLBACK_FAILURE");
            failure && std::strcmp(failure, "allocation") == 0)
            throw std::bad_alloc{};
#endif
        std::unique_ptr<EventData> event;
        if (value) {
            auto owned = std::make_unique<EventData>();
            if (build(*value, *owned)) event = std::move(owned);
        }
        std::lock_guard lock{state->mutex};
        if (state->lifecycle != MetadataLifecycle::Active) return;
        if (!event) { increment(state->counters.malformed_or_unsupported); return; }
        if (state->queue.size() >= state->capacity) {
            increment(state->counters.events_dropped_queue_full);
            return;
        }
        state->queue.push_back(std::move(event));
        state->ready.notify_one();
    } catch (...) { state->fail(); }
}

/* Provider callback boundary for the @RequiredIfTrack IRSTTrackReport. */
void metadata_callback(const std::shared_ptr<MetadataState>& state,
                       const irmel::IRSTTrackReport *value) noexcept
{
    metadata_callback_impl(state, value,
        [](const irmel::IRSTTrackReport& report, EventData& event) noexcept {
            event.view.base.base.kind = AMS_MEL_IR_TRACK_METADATA_IRST_TRACK_REPORT;
            return copy_track_report(report, event.view.base.base.track_report);
        });
}

/* Provider callback boundary for the @RequiredIfDetectCandidateObjects
 * CandidateObjectMessage. This is inbound metadata: upstream declares no
 * send(CandidateObjectMessage) and no RequestFor<CandidateObjectMessage>, so no
 * request handle, completion, or worker is ever created here. A null payload, a
 * numberOfCOs above 900, and an invalid HotRegion enum are each malformed and
 * enqueue nothing. An allocation failure inside the builder is caught by the
 * shared boundary and never crosses back into provider code. */
void metadata_callback(const std::shared_ptr<MetadataState>& state,
                       const irmel::CandidateObjectMessage *value) noexcept
{
    metadata_callback_impl(state, value,
        [](const irmel::CandidateObjectMessage& message, EventData& event) {
            event.view.base.base.kind =
                AMS_MEL_IR_TRACK_METADATA_CANDIDATE_OBJECT_MESSAGE;
            return copy_candidate_object_message(message, event);
        });
}

/* Provider callback boundary for the @Optional CandidateObjectPreProcMessage.
 * This is inbound metadata: upstream declares no
 * send(CandidateObjectPreProcMessage) and no
 * RequestFor<CandidateObjectPreProcMessage>, so no request handle, completion,
 * or worker is ever created here. A null payload and an invalid HotRegion enum
 * are each malformed and enqueue nothing; a header count that differs from the
 * PreProc vector size is NOT malformed, because no such invariant is
 * published. An allocation failure inside the builder is caught by the shared
 * boundary and never crosses back into provider code. */
void metadata_callback(const std::shared_ptr<MetadataState>& state,
                       const irmel::CandidateObjectPreProcMessage *value) noexcept
{
    metadata_callback_impl(state, value,
        [](const irmel::CandidateObjectPreProcMessage& message, EventData& event) {
            event.view.base.base.kind =
                AMS_MEL_IR_TRACK_METADATA_CANDIDATE_OBJECT_PREPROC_MESSAGE;
            return copy_candidate_preproc_message(message, event);
        });
}

/* Provider callback boundary for the @Optional RequestSystemTrackData. The
 * copy cannot fail because upstream declares no enum or constrained field, so
 * only a null payload is malformed. */
void metadata_callback(const std::shared_ptr<MetadataState>& state,
                       const irmel::RequestSystemTrackData *value) noexcept
{
    metadata_callback_impl(state, value,
        [](const irmel::RequestSystemTrackData& request, EventData& event) noexcept {
            event.view.base.base.kind =
                AMS_MEL_IR_TRACK_METADATA_REQUEST_SYSTEM_TRACK_DATA;
            copy_request_system_track_data(
                request, event.view.base.base.request_system_track_data);
            return true;
        });
}

enum class Lifecycle { Attached, Enabled, Failed, Closed };

/* Track channel ownership foundation, extended only as far as the
 * @RequiredIfTrack IRSTTrackReport callback and the @RequiredIfTrackUpdate
 * TrackDataUpdate send require. The existing metadata ownership is unchanged;
 * only the pending-request count is added. */
struct TrackState {
    std::mutex mutex;
    std::shared_ptr<SessionState> session;
    std::shared_ptr<irmel::Channel> channel;
    std::shared_ptr<irmel::TrackChannel> track;
    /* Callback-accessible state belongs to the channel, not to the public
     * metadata wrapper: upstream declares no unregister operation. */
    std::shared_ptr<MetadataState> metadata;
    Lifecycle lifecycle{Lifecycle::Attached};
    /* The single pending-request accounting domain, shared by the
     * @RequiredIfTrackUpdate TrackDataUpdate requests and the @Optional
     * SystemTrackDataResponse requests. There is deliberately no second
     * counter: while this total is non-zero, physical provider teardown is
     * deferred to the final request completion of either family. */
    std::size_t requests{};
    bool enable_attempted{};
    bool metadata_attempted{};
    /* Whether the channel advertised ChannelMetadataCapabilityType::
     * CandidateObjectMessage at Open. Private state only: no new public API
     * exposes it. When false, the @RequiredIfDetectCandidateObjects callback is
     * not registered at all, exactly as upstream documents for a metadata type
     * absent from the advertised set. */
    bool candidate_objects_advertised{};
    bool cleanup_started{};
    /* Allocation-free permanent retention root for graphs whose detach
     * ownership could not be proven. */
    std::shared_ptr<TrackState> emergency_self;
    TrackState *emergency_next{};
    std::atomic<bool> emergency_retained{};
};

void retain_failed(const std::shared_ptr<TrackState>& state) noexcept
{
    static std::atomic<TrackState *> head{};
    bool expected = false;
    if (!state->emergency_retained.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) return;
    state->emergency_self = state;
    auto *current = head.load(std::memory_order_relaxed);
    do { state->emergency_next = current; }
    while (!head.compare_exchange_weak(current, state.get(),
        std::memory_order_release, std::memory_order_relaxed));
}

bool has_track(const irmel::ChannelCapability& capability)
{
    const auto& types = capability.getChannelTypes();
    return std::find(types.begin(), types.end(), irmel::ChannelType::IRSTTrack) !=
           types.end();
}

/* Upstream documents that registerMetadataCallback for a type absent from the
 * advertised channelMetadataCapabilities set is expected to return
 * Return::NotSupported, so the advertised set -- not a guess -- decides whether
 * the CandidateObjectMessage callback is registered at all. */
bool has_candidate_objects(const irmel::ChannelCapability& capability)
{
    const auto& types = capability.getChannelMetadataCapabilities();
    return types.find(irmel::ChannelMetadataCapabilityType::CandidateObjectMessage) !=
           types.end();
}

/* Returns false when the provider disable failed; leaves state->channel
 * non-null when detach could not be proven. */
bool cleanup(const std::shared_ptr<TrackState>& state, bool retain_orphan)
{
    std::shared_ptr<irmel::Channel> channel;
    std::shared_ptr<MetadataState> metadata;
    bool disable = false;
    {
        std::lock_guard lock{state->mutex};
        if (state->cleanup_started) return !state->channel;
        /* Physical teardown never runs while a request of either family still
         * owns the provider graph; the final completion performs it instead. */
        if (state->requests != 0U) return true;
        state->cleanup_started = true;
        channel = state->channel;
        metadata = state->metadata;
        disable = state->enable_attempted;
    }
    /* Public consumption stops before provider teardown begins, and any waiting
     * receiver is woken immediately. */
    if (metadata) {
        std::lock_guard lock{metadata->mutex};
        if (metadata->lifecycle == MetadataLifecycle::Active)
            metadata->lifecycle = MetadataLifecycle::Inactive;
        metadata->ready.notify_all();
    }
    bool ok = true;
    if (disable && channel) {
        try { if (channel->disable() != irmel::Return::Success) ok = false; }
        catch (...) { ok = false; }
    }
    /* A failed disable never proves ownership safety, so detach is attempted
     * regardless of the disable outcome. */
    bool detached = !channel;
    if (channel) {
        try {
            detached = state->session->control->detachChannel(channel) ==
                       irmel::Return::Success;
        } catch (...) { detached = false; }
    }
    if (!detached) {
        /* The complete callback/provider graph stays alive so a later Close can
         * retry; the metadata is failed rather than stopped because quiescence
         * was never established. */
        if (metadata) {
            std::lock_guard lock{metadata->mutex};
            metadata->lifecycle = MetadataLifecycle::Failed;
            metadata->ready.notify_all();
        }
        {
            std::lock_guard lock{state->mutex};
            state->cleanup_started = false;
        }
        if (retain_orphan) retain_failed(state);
        return false;
    }
    {
        std::lock_guard lock{state->mutex};
        state->track.reset();
        state->channel.reset();
        state->enable_attempted = false;
        state->lifecycle = Lifecycle::Closed;
    }
    /* Destroying the provider channel while this state still holds the session
     * graph keeps provider code loaded until the whole graph is released. This
     * destruction is the provider callback-quiescence boundary. */
    channel.reset();
    /* Only after provider channel destruction is waiting for the in-flight
     * callback count to drain meaningful. */
    if (metadata) {
        std::unique_lock lock{metadata->mutex};
        metadata->callbacks_done.wait(lock, [&] {
            return metadata->callbacks_in_flight.load(std::memory_order_acquire) == 0U; });
        if (metadata->lifecycle != MetadataLifecycle::Failed)
            metadata->lifecycle = MetadataLifecycle::Stopped;
        metadata->ready.notify_all();
    }
    return ok;
}

enum class CompletionKind {
    Pending, Success, Rejected, ProviderException, ProviderFailure, InternalError
};

/* Terminal outcome of one Track command request. Both the @RequiredIfTrackUpdate
 * TrackDataUpdate family and the @Optional SystemTrackDataResponse family use
 * this one completion engine, because both upstream operations return
 * RequestFor<CommandStatus>. The terminal value is stored as the neutral
 * CommandStatus/error-code pair rather than as either public result record, so
 * no public result layout is assumed to equal the other.
 *
 * It never retains a raw ams_mel_ir_track *: the TrackState graph keeps this
 * request alive independently of the public owners. reason_text is the
 * request-owned immutable cache that the published
 * status.reason_description points into, so no provider-owned memory is ever
 * exposed and repeated Wait calls return identical storage. */
struct Completion {
    std::mutex mutex;
    std::condition_variable ready;
    CompletionKind kind{CompletionKind::Pending};
    ams_mel_ir_command_status_v1 status{};
    ams_mel_error_code_t error_code{};
    std::string reason_text;
    std::string message;
    std::shared_ptr<TrackState> channel;
};

struct WorkerInput {
    std::shared_ptr<Completion> completion;
    mel::RequestFor<irmel::CommandStatus> future;
    std::shared_ptr<WorkerInput> emergency_self;
    WorkerInput *emergency_next{};
    std::atomic<bool> emergency_retained{};
    std::atomic<unsigned> launch_state{};
};

void arm_worker(const std::shared_ptr<WorkerInput>& input) noexcept
{ input->emergency_self = input; }

/* Allocation-free permanent retention of a worker whose future/provider graph
 * could not be handed to a running worker safely. */
void retain_worker(const std::shared_ptr<WorkerInput>& input) noexcept
{
    static std::atomic<WorkerInput *> retained{};
    bool expected = false;
    if (!input->emergency_retained.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) return;
    WorkerInput *head = retained.load(std::memory_order_relaxed);
    do { input->emergency_next = head; }
    while (!retained.compare_exchange_weak(
        head, input.get(), std::memory_order_release, std::memory_order_relaxed));
}

/* Decrements the pending request count; if this was the final request of an
 * already logically closed Track, performs the deferred physical cleanup.
 * Never called with the TrackState mutex held. */
bool finish_request(const std::shared_ptr<TrackState>& state)
{
    bool close = false;
    {
        std::lock_guard lock{state->mutex};
        if (state->requests) --state->requests;
        close = state->requests == 0U && state->lifecycle == Lifecycle::Closed;
    }
    return !close || cleanup(state, true);
}

/* Validates a provider CommandStatus completely. Upstream CommandState tops out
 * at Cancelled and CannotComply at Alignment_Maneuver; neither declares a
 * MaxExclusive value, so anything above is malformed. The description is copied
 * into request-owned storage only after it is proven valid UTF-8 without an
 * embedded NUL. */
bool copy_command_status(const irmel::CommandStatus& source,
                         ams_mel_ir_command_status_v1& destination,
                         std::string& owned_text)
{
    const auto state = static_cast<std::uint32_t>(source.getState());
    const auto reason = static_cast<std::uint32_t>(source.getReasonID());
    if (state > AMS_MEL_IR_COMMAND_CANCELLED) return false;
    if (reason > AMS_MEL_IR_CANNOT_COMPLY_ALIGNMENT_MANEUVER) return false;
    const std::string& description = source.getReasonDescription();
    if (!valid_utf8(description)) return false;
    owned_text = description;
    destination.command_id = source.getCommandID();
    destination.state = state;
    destination.reason_id = reason;
    return true;
}

/* The single future::get() caller. No other thread may call it. */
void complete(const std::shared_ptr<Completion>& state,
              mel::RequestFor<irmel::CommandStatus>& future)
{
    CompletionKind kind = CompletionKind::ProviderException;
    ams_mel_ir_command_status_v1 status{};
    ams_mel_error_code_t error_code{};
    std::string reason_text;
    std::string message;
    try {
        auto outcome = future.get();
        if (outcome) {
            const auto& value = outcome.get();
            if (!value) {
                kind = CompletionKind::ProviderFailure;
                message = "provider returned null successful CommandStatus";
            } else if (!copy_command_status(*value, status, reason_text)) {
                kind = CompletionKind::ProviderFailure;
                message = "provider returned malformed Track CommandStatus";
            } else {
                /* A CommandStatus whose own state is Rejected is still a
                 * successful future outcome and stays AMS_MEL_OK. */
                kind = CompletionKind::Success;
            }
        } else {
            const mel::Error& error = outcome.getError();
            bool known = false;
            error_code = map_error(error.getCode(), known);
            if (!known) {
                kind = CompletionKind::ProviderFailure;
                message = "provider returned unknown MEL error code";
            } else {
                kind = CompletionKind::Rejected;
                const std::string& description = error.getDescription();
                message = valid_utf8(description) ? description :
                    "provider rejection description was invalid UTF-8 or contained NUL";
            }
        }
    } catch (const std::bad_alloc&) {
        kind = CompletionKind::InternalError;
        message.clear();
        reason_text.clear();
    } catch (const std::exception& error) {
        kind = CompletionKind::ProviderException;
        const char *what = error.what();
        const std::string_view text = what ? std::string_view{what} : std::string_view{};
        try { message = !text.empty() && valid_utf8(text) ? text : "provider future exception"; }
        catch (...) { message.clear(); }
    } catch (...) {
        kind = CompletionKind::ProviderException;
        try { message = "unknown provider future exception"; } catch (...) {}
    }
    auto channel = state->channel;
    if (!finish_request(channel)) {
        /* Deferred detach could not be proven: the complete graph stays
         * retained permanently and the terminal result fails closed. */
        kind = CompletionKind::ProviderFailure;
        status = {};
        error_code = AMS_MEL_ERROR_NONE;
        try { message = "deferred Track cleanup failed"; }
        catch (...) { message.clear(); }
        reason_text.clear();
    }
    {
        std::lock_guard lock{state->mutex};
        state->kind = kind;
        state->status = status;
        state->error_code = error_code;
        state->reason_text = std::move(reason_text);
        /* The public view always points into request-owned cached storage. */
        state->status.reason_description.data =
            state->reason_text.empty() ? nullptr : state->reason_text.data();
        state->status.reason_description.size = state->reason_text.size();
        state->message = std::move(message);
        state->channel.reset();
    }
    channel.reset();
    state->ready.notify_all();
}

void run_worker(const std::shared_ptr<WorkerInput>& input) noexcept
{
    while (input->launch_state.load(std::memory_order_acquire) == 0U)
        std::this_thread::yield();
    try {
        complete(input->completion, input->future);
    } catch (...) {
        /* A mutex/system failure must neither escape the detached thread nor
         * destroy an unaccounted future/provider graph. */
        arm_worker(input);
        retain_worker(input);
    }
}

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
enum class SubmitFailpoint { None, Allocation, WorkerLaunch };

/* One shared post-send failpoint mechanism for every Track submit family. Each
 * family names its own environment variable, so a SystemTrackDataResponse
 * failpoint can never weaken or perturb the existing TrackDataUpdate
 * post-send coverage. */
SubmitFailpoint submit_failpoint(const char *variable) noexcept
{
    const char *value = std::getenv(variable);
    if (value && std::strcmp(value, "allocation") == 0) return SubmitFailpoint::Allocation;
    if (value && std::strcmp(value, "worker-launch") == 0) return SubmitFailpoint::WorkerLaunch;
    return SubmitFailpoint::None;
}
#endif
} // namespace

struct ams_mel_ir_track { std::shared_ptr<TrackState> state; };
struct ams_mel_ir_track_update_request { std::shared_ptr<Completion> state; };
/* A deliberately distinct public owner for the @Optional
 * SystemTrackDataResponse family. The TrackDataUpdate request type is never
 * exposed under this semantic name; only the private Completion engine is
 * shared, because both upstream operations return RequestFor<CommandStatus>. */
struct ams_mel_ir_track_system_response_request {
    std::shared_ptr<Completion> state;
};

namespace {
/* The one Wait implementation shared by both Track request families. It fills
 * the caller's neutral CommandStatus/error-code pair, which each public Wait
 * then stores into its own distinct result record. */
ams_mel_status_t wait_completion(const std::shared_ptr<Completion>& state,
                                 std::uint32_t timeout_ms,
                                 ams_mel_ir_command_status_v1& status,
                                 ams_mel_error_code_t& error_code, char *out,
                                 std::size_t capacity, std::size_t *required,
                                 const char *failure_text) noexcept
{
    try {
        std::unique_lock lock{state->mutex};
        /* A finite timeout means only "not ready yet"; the pending request and
         * the caller's result record are left untouched. */
        if (state->kind == CompletionKind::Pending &&
            !state->ready.wait_for(lock, std::chrono::milliseconds{timeout_ms},
                [&] { return state->kind != CompletionKind::Pending; }))
            return AMS_MEL_TIMEOUT;
        status = state->status;
        error_code = state->error_code;
        diagnostic(state->message, out, capacity, required);
        switch (state->kind) {
        case CompletionKind::Success: return AMS_MEL_OK;
        case CompletionKind::Rejected: return AMS_MEL_COMMAND_REJECTED;
        case CompletionKind::ProviderException: return AMS_MEL_PROVIDER_EXCEPTION;
        case CompletionKind::ProviderFailure: return AMS_MEL_PROVIDER_FAILED;
        case CompletionKind::InternalError: return AMS_MEL_INTERNAL_ERROR;
        case CompletionKind::Pending: return AMS_MEL_TIMEOUT;
        }
    } catch (...) {
        diagnostic(failure_text, out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    return AMS_MEL_INTERNAL_ERROR;
}
} // namespace
/* Public consumption wrapper only. It owns the adapter-owned MetadataState and
 * nothing else: no Track, session, or provider-library ownership. Deleting it
 * never unregisters the provider callback and never destroys the
 * callback-accessible state, and keeping it open never keeps provider code
 * loaded. */
struct ams_mel_ir_track_metadata {
    std::shared_ptr<MetadataState> state;
};
struct ams_mel_ir_track_metadata_event { std::unique_ptr<EventData> data; };

extern "C" ams_mel_status_t ams_mel_ir_track_open(
    const ams_mel_session *session, const ams_mel_ir_track_config_v1 *config,
    ams_mel_ir_track **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!session || !session->state || !config || !output || *output ||
        (!out && capacity) ||
        config->channel_type != AMS_MEL_IR_CHANNEL_IRST_TRACK ||
        !valid_view(config->channel_id.descriptive_label) ||
        !valid_view(config->platform_id.descriptive_label) ||
        !valid_view(config->sensor_location.key) ||
        !valid_view(config->sensor_location.system_name)) {
        diagnostic("invalid Track configuration", out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    std::shared_ptr<TrackState> state;
    try {
        state = std::make_shared<TrackState>();
        state->session = session->state;
        mel::ForeignKey key{copy_view(config->sensor_location.key),
                            copy_view(config->sensor_location.system_name)};
        mel::ComponentLocation location{config->sensor_location.offset_x_m,
            config->sensor_location.offset_y_m, config->sensor_location.offset_z_m,
            key};
        irmel::Config upstream{convert_id(config->channel_id),
            irmel::ChannelType::IRSTTrack, convert_id(config->platform_id),
            std::move(location), {}, false, false};
        state->channel = state->session->control->attachChannel(upstream);
        if (!state->channel) {
            diagnostic("attachChannel returned null", out, capacity, required);
            return AMS_MEL_FACTORY_FAILED;
        }
        state->track = std::dynamic_pointer_cast<irmel::TrackChannel>(state->channel);
        bool compatible = static_cast<bool>(state->track);
        const char *reason = "attached channel is not a TrackChannel";
        if (compatible) {
            /* One capability query serves both the channel-type check and the
             * CandidateObjectMessage advertisement record; no extra provider
             * call is introduced. */
            const auto capability = state->channel->getCapabilities();
            compatible = has_track(capability);
            if (!compatible) reason = "Track capability omits IRSTTrack";
            else state->candidate_objects_advertised = has_candidate_objects(capability);
        }
        if (!compatible) {
            bool detached = false;
            try {
                detached = state->session->control->detachChannel(state->channel) ==
                           irmel::Return::Success;
            } catch (...) {}
            if (!detached) retain_failed(state);
            diagnostic(detached ? reason :
                       "incompatible Track channel and detach failed",
                       out, capacity, required);
            return detached ? AMS_MEL_INITIALIZATION_FAILED : AMS_MEL_PROVIDER_FAILED;
        }
        auto owner = std::make_unique<ams_mel_ir_track>();
        owner->state = std::move(state);
        *output = owner.release();
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        if (state && state->channel) retain_failed(state);
        diagnostic("allocation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        /* Includes a throwing getCapabilities. The provider graph is never
         * destructed while detach ownership remains uncertain. */
        if (state && state->channel) {
            bool detached = false;
            try {
                detached = state->session->control->detachChannel(state->channel) ==
                           irmel::Return::Success;
            } catch (...) {}
            if (!detached) {
                retain_failed(state);
                diagnostic("provider exception during Track open and detach failed",
                           out, capacity, required);
                return AMS_MEL_PROVIDER_FAILED;
            }
            {
                std::lock_guard lock{state->mutex};
                state->track.reset();
                state->channel.reset();
                state->lifecycle = Lifecycle::Closed;
            }
            diagnostic("provider exception during Track open",
                       out, capacity, required);
            return AMS_MEL_INITIALIZATION_FAILED;
        }
        diagnostic("provider exception during Track open", out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_track_enable(
    ams_mel_ir_track *track, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!track || !track->state || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    auto state = track->state;
    try {
        std::lock_guard lock{state->mutex};
        if (state->lifecycle == Lifecycle::Enabled) return AMS_MEL_OK;
        if (state->lifecycle != Lifecycle::Attached) {
            diagnostic("Track channel is not attached", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        state->enable_attempted = true;
        if (state->channel->enable() != irmel::Return::Success) {
            state->lifecycle = Lifecycle::Failed;
            diagnostic("Track enable failed", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        state->lifecycle = Lifecycle::Enabled;
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        diagnostic("allocation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        try {
            std::lock_guard lock{state->mutex};
            state->lifecycle = Lifecycle::Failed;
        } catch (...) {}
        diagnostic("provider exception during Track enable", out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_track_get_capabilities(
    ams_mel_ir_track *track, ams_mel_ir_channel_capability **output, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!track || !track->state || !output || *output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::lock_guard lock{track->state->mutex};
        if (track->state->lifecycle != Lifecycle::Attached &&
            track->state->lifecycle != Lifecycle::Enabled)
            return AMS_MEL_PROVIDER_FAILED;
        /* Reuses the one shared native ChannelCapability snapshot. */
        return ams_mel::internal::snapshot_capability(
            *track->state->track, output, out, capacity, required);
    } catch (...) {
        diagnostic("Track capability query failed", out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_track_metadata_open(
    ams_mel_ir_track *track, std::size_t queue_capacity,
    ams_mel_ir_track_metadata **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!track || !track->state || !queue_capacity || !output || *output ||
        (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    std::shared_ptr<MetadataState> state;
    std::shared_ptr<irmel::TrackChannel> channel;
    bool candidate_advertised = false;
    try {
        /* Publish and retain the callback state first, then release the
         * lifecycle lock, then register. The provider may invoke the
         * IRSTTrackReport callback synchronously from inside
         * registerMetadataCallback, so TrackState::mutex is never held across
         * the registration call. */
        {
            std::lock_guard lock{track->state->mutex};
            /* Registration is one-shot: upstream has no unregister. */
            if (track->state->metadata_attempted ||
                (track->state->lifecycle != Lifecycle::Attached &&
                 track->state->lifecycle != Lifecycle::Enabled))
                return AMS_MEL_INVALID_ARGUMENT;
            track->state->metadata_attempted = true;
            state = std::make_shared<MetadataState>();
            state->capacity = queue_capacity;
            track->state->metadata = state;
            channel = track->state->track;
            candidate_advertised = track->state->candidate_objects_advertised;
        }
        auto owner = std::make_unique<ams_mel_ir_track_metadata>();
        /* Only MetadataState is owned here. The provider graph stays owned by
         * TrackState and by the retained callback closure. */
        owner->state = state;
        const auto result = channel->registerMetadataCallback(
            std::function<void(irmel::Channel&, const irmel::IRSTTrackReport *const)>{
                [state](irmel::Channel&,
                        const irmel::IRSTTrackReport *value) noexcept {
                    metadata_callback(state, value);
                }});
        if (result != irmel::Return::Success) {
            /* No owner escapes, but the callback state stays retained by the
             * Track channel because registration may have partially taken. */
            std::lock_guard lock{state->mutex};
            state->lifecycle = MetadataLifecycle::Inactive;
            state->ready.notify_all();
            diagnostic("IRSTTrackReport callback registration failed",
                       out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        /* The @RequiredIfDetectCandidateObjects CandidateObjectMessage callback
         * is registered second and ONLY when the channel advertised
         * ChannelMetadataCapabilityType::CandidateObjectMessage. An
         * unadvertised channel is skipped entirely, so every existing
         * non-candidate deployment keeps its exact current behavior.
         *
         * When the capability IS advertised the registration FAILS CLOSED on
         * anything other than Success, including NotSupported. Unlike the
         * @Optional RequestSystemTrackData refusal, a NotSupported here
         * contradicts the channel's own advertisement: the channel promised the
         * metadata type, so Metadata Open must not return success while
         * promising an event path the adapter cannot actually receive.
         * BadPointer, NotImplemented, and any value added by a future upstream
         * revision fail closed for the same reason.
         *
         * TrackState::mutex is again not held: the provider may deliver a
         * CandidateObjectMessage synchronously from inside this call. */
        if (candidate_advertised) {
            const auto candidate_result = channel->registerMetadataCallback(
                std::function<void(irmel::Channel&,
                                   const irmel::CandidateObjectMessage *const)>{
                    [state](irmel::Channel&,
                            const irmel::CandidateObjectMessage *value) noexcept {
                        metadata_callback(state, value);
                    }});
            if (candidate_result != irmel::Return::Success) {
                /* Fail closed. No public owner escapes; `owner` is still the
                 * unique_ptr and is destroyed on this path. The already
                 * registered @RequiredIfTrack callback and its retained state
                 * stay live until Track teardown because upstream provides no
                 * unregister operation, no unregister is attempted, and the
                 * one-shot rule stays established so a retry cannot
                 * double-register. */
                std::lock_guard lock{state->mutex};
                state->lifecycle = MetadataLifecycle::Inactive;
                state->ready.notify_all();
                diagnostic("CandidateObjectMessage callback registration failed",
                           out, capacity, required);
                return AMS_MEL_PROVIDER_FAILED;
            }
        }
        /* The @Optional RequestSystemTrackData request shares the same queue.
         * The registration again happens without TrackState::mutex held because
         * the provider may deliver synchronously from inside it.
         *
         * Upstream documents three distinct answers, and they are NOT
         * interchangeable:
         *
         *   Success      registration took; the optional kind is active.
         *   NotSupported the provider does not implement this @Optional
         *                callback. Non-fatal: pinned Squall is exactly such a
         *                provider, the @RequiredIfTrack report callback is
         *                already live and must keep working, and only the
         *                reception of this optional kind is lost.
         *   Fail         a callback is ALREADY REGISTERED for this datatype on
         *                this channel. That is a genuine conflict, not an
         *                optional refusal: some other subscriber owns this
         *                datatype and our closure may never be invoked, so
         *                silently returning OK would promise deliveries the
         *                bridge cannot make.
         *
         * Anything else -- BadPointer, NotImplemented, or a value added by a
         * future upstream revision -- is likewise not a documented refusal, so
         * it fails closed rather than silently claiming success. */
        const auto request_result = channel->registerMetadataCallback(
            std::function<void(irmel::Channel&,
                               const irmel::RequestSystemTrackData *const)>{
                [state](irmel::Channel&,
                        const irmel::RequestSystemTrackData *value) noexcept {
                    metadata_callback(state, value);
                }});
        if (request_result != irmel::Return::Success &&
            request_result != irmel::Return::NotSupported) {
            /* Fail closed. No public owner escapes; `owner` is still the
             * unique_ptr and is destroyed on this path. The already-registered
             * @RequiredIfTrack callback and its retained state stay live until
             * Track teardown because upstream provides no unregister
             * operation, and the one-shot rule stays established so a retry
             * cannot double-register. */
            std::lock_guard lock{state->mutex};
            state->lifecycle = MetadataLifecycle::Inactive;
            state->ready.notify_all();
            diagnostic("RequestSystemTrackData callback registration failed",
                       out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        /* The @Optional CandidateObjectPreProcMessage shares the same queue and
         * is registered last. TrackState::mutex is again not held: the provider
         * may deliver a PreProc message synchronously from inside this call.
         *
         * This follows the established @Optional policy used for
         * RequestSystemTrackData, NOT the advertised-capability policy used for
         * CandidateObjectMessage. The callback itself is annotated @Optional
         * and is documented as intended for IR MFAs that use
         * CandidateObjectPreProc, so:
         *
         *   Success      registration took; the optional kind is active.
         *   NotSupported the provider does not implement this @Optional
         *                callback. NON-FATAL: metadata open continues, the
         *                already-live required report callback keeps working,
         *                and only the reception of this optional kind is lost.
         *                It is deliberately NOT made fatal merely because
         *                ChannelMetadataCapabilityType happens to name this
         *                type; the callback's own annotation governs.
         *   Fail         a callback is ALREADY REGISTERED for this datatype on
         *                this channel. That is a genuine conflict, not an
         *                optional refusal: another subscriber owns the
         *                datatype and our closure may never be invoked, so
         *                silently claiming success would be incorrect.
         *
         * BadPointer, NotImplemented, and any value added by a future upstream
         * revision are likewise not documented refusals and fail closed. */
        const auto preproc_result = channel->registerMetadataCallback(
            std::function<void(irmel::Channel&,
                               const irmel::CandidateObjectPreProcMessage *const)>{
                [state](irmel::Channel&,
                        const irmel::CandidateObjectPreProcMessage *value) noexcept {
                    metadata_callback(state, value);
                }});
        if (preproc_result != irmel::Return::Success &&
            preproc_result != irmel::Return::NotSupported) {
            /* Fail closed. No public owner escapes; `owner` is still the
             * unique_ptr and is destroyed on this path. Every already
             * registered callback and its retained state stay live until Track
             * teardown because upstream provides no unregister operation, no
             * unregister is attempted, and the one-shot rule stays established
             * so a retry cannot double-register. */
            std::lock_guard lock{state->mutex};
            state->lifecycle = MetadataLifecycle::Inactive;
            state->ready.notify_all();
            diagnostic("CandidateObjectPreProcMessage callback registration failed",
                       out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        *output = owner.release();
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        if (state) {
            std::lock_guard lock{state->mutex};
            state->lifecycle = MetadataLifecycle::Failed;
            state->ready.notify_all();
        }
        diagnostic("Track metadata allocation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        if (state) {
            std::lock_guard lock{state->mutex};
            state->lifecycle = MetadataLifecycle::Inactive;
            state->ready.notify_all();
        }
        diagnostic("provider exception during Track metadata registration",
                   out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_track_metadata_receive(
    ams_mel_ir_track_metadata *metadata, std::uint32_t timeout_ms,
    ams_mel_ir_track_metadata_event **output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!metadata || !metadata->state || !output || *output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::unique_lock lock{metadata->state->mutex};
        if (metadata->state->queue.empty() &&
            metadata->state->lifecycle == MetadataLifecycle::Active && timeout_ms)
            metadata->state->ready.wait_for(lock, std::chrono::milliseconds{timeout_ms},
                [&] { return !metadata->state->queue.empty() ||
                             metadata->state->lifecycle != MetadataLifecycle::Active; });
        /* A queued event is always delivered first, whatever the lifecycle. */
        if (!metadata->state->queue.empty()) {
            auto owner = std::make_unique<ams_mel_ir_track_metadata_event>();
            owner->data = std::move(metadata->state->queue.front());
            metadata->state->queue.pop_front();
            *output = owner.release();
            return AMS_MEL_OK;
        }
        if (metadata->state->lifecycle == MetadataLifecycle::Failed)
            return AMS_MEL_PROVIDER_FAILED;
        if (metadata->state->lifecycle != MetadataLifecycle::Active)
            return AMS_MEL_STREAM_STOPPED;
        return AMS_MEL_TIMEOUT;
    } catch (...) {
        diagnostic("Track metadata receive failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_track_metadata_get_counters(
    const ams_mel_ir_track_metadata *metadata,
    ams_mel_ir_metadata_counters_v1 *output, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!metadata || !metadata->state || !output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::lock_guard lock{metadata->state->mutex};
        *output = metadata->state->counters;
        return AMS_MEL_OK;
    } catch (...) { return AMS_MEL_INTERNAL_ERROR; }
}

extern "C" ams_mel_status_t ams_mel_ir_track_metadata_close(
    ams_mel_ir_track_metadata **metadata, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!metadata || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try {
        auto *owner = std::exchange(*metadata, nullptr);
        if (owner) {
            {
                /* Deactivates public consumption only. The retained provider
                 * callback keeps using channel-owned state until provider
                 * channel destruction; this call never blocks on it. */
                std::lock_guard lock{owner->state->mutex};
                if (owner->state->lifecycle == MetadataLifecycle::Active)
                    owner->state->lifecycle = MetadataLifecycle::Inactive;
                owner->state->ready.notify_all();
            }
            delete owner;
        }
        return AMS_MEL_OK;
    } catch (...) { return AMS_MEL_INTERNAL_ERROR; }
}

/* Unchanged v1 contract. This operation's output type stays the frozen v1
 * record, so a consumer compiled before CandidateObjectMessage existed keeps
 * working without recompilation. It deliberately does NOT gain a larger output
 * contract: a CandidateObjectMessage event is still delivered here, and
 * base.kind still reports kind 3, but the candidate payload is reachable only
 * through ams_mel_ir_track_metadata_event_view_v2. */
extern "C" ams_mel_status_t ams_mel_ir_track_metadata_event_view(
    const ams_mel_ir_track_metadata_event *event,
    const ams_mel_ir_track_metadata_event_v1 **output, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!event || !event->data || !output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    *output = &event->data->view.base.base;
    return AMS_MEL_OK;
}

/* Unchanged v2 contract. This operation's output type stays the now-frozen v2
 * record, so a consumer compiled before CandidateObjectPreProcMessage existed
 * keeps working without recompilation. A PreProc event is still delivered
 * here, and base.kind still reports kind 4, but the PreProc payload is
 * reachable only through ams_mel_ir_track_metadata_event_view_v3. No
 * provider-owned pointer escapes: both spans inside candidate_object_message
 * address the event's own vectors. */
extern "C" ams_mel_status_t ams_mel_ir_track_metadata_event_view_v2(
    const ams_mel_ir_track_metadata_event *event,
    const ams_mel_ir_track_metadata_event_v2 **output, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!event || !event->data || !output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    *output = &event->data->view.base;
    return AMS_MEL_OK;
}

/* v3 view over the same adapter-owned event storage, with identical ownership,
 * validity, and diagnostic rules as the v1 and v2 views. No provider-owned
 * pointer escapes: both spans inside candidate_object_preproc_message address
 * the event's own vectors, which stay valid until event close, including after
 * the provider library is unloaded. */
extern "C" ams_mel_status_t ams_mel_ir_track_metadata_event_view_v3(
    const ams_mel_ir_track_metadata_event *event,
    const ams_mel_ir_track_metadata_event_v3 **output, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!event || !event->data || !output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    *output = &event->data->view;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_track_metadata_event_close(
    ams_mel_ir_track_metadata_event **event, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!event || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try { delete std::exchange(*event, nullptr); return AMS_MEL_OK; }
    catch (...) { return AMS_MEL_INTERNAL_ERROR; }
}


extern "C" ams_mel_status_t ams_mel_ir_track_submit_update(
    ams_mel_ir_track *track, const ams_mel_ir_track_data_update_v1 *update,
    ams_mel_ir_track_update_request **out_request, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    irmel::TrackStatus status{};
    if (!track || !track->state || !update || !out_request || *out_request ||
        (!out && capacity) ||
        !convert_track_status(update->track_status, status) ||
        !valid_view(update->capability_uuid.descriptive_label) ||
        !valid_view(update->activity_uuid.descriptive_label) ||
        !valid_view(update->entity_uuid.descriptive_label)) {
        diagnostic("invalid TrackDataUpdate input", out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }

    /* Everything the request needs to own the returned future is allocated
     * before the provider send. */
    std::shared_ptr<Completion> completion;
    std::shared_ptr<WorkerInput> input;
    std::unique_ptr<ams_mel_ir_track_update_request> owner;
    std::unique_ptr<std::thread> worker;
    try {
        completion = std::make_shared<Completion>();
        completion->channel = track->state;
        input = std::make_shared<WorkerInput>();
        input->completion = completion;
        owner = std::make_unique<ams_mel_ir_track_update_request>();
        owner->state = completion;
        worker = std::make_unique<std::thread>();
    } catch (...) {
        diagnostic("allocation failed before provider send", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }

    /* Every borrowed C input, including all three UCI_ID labels, is converted
     * and copied here; nothing borrowed outlives this call. */
    irmel::TrackDataUpdate upstream;
    try {
        upstream = build_update(*update, status);
    } catch (...) {
        diagnostic("TrackDataUpdate preparation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }

    /* Validate Enabled, copy the shared TrackChannel locally, and account the
     * request under the lifecycle mutex, then release it: provider send() may
     * synchronously invoke the IRSTTrackReport metadata callback, which
     * independently locks the metadata mutex. */
    std::shared_ptr<irmel::TrackChannel> channel;
    try {
        std::lock_guard lock{track->state->mutex};
        if (track->state->lifecycle != Lifecycle::Enabled) {
            diagnostic("Track channel is not enabled", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        channel = track->state->track;
        ++track->state->requests;
    } catch (...) {
        diagnostic("Track submission lock failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }

    auto state = track->state;
    try {
        try {
            input->future = channel->send(std::move(upstream));
            arm_worker(input);
        } catch (const std::exception& error) {
            (void)finish_request(state);
            const char *what = error.what();
            const std::string_view text = what ? std::string_view{what} : std::string_view{};
            diagnostic(!text.empty() && valid_utf8(text) ? text : "provider send exception",
                       out, capacity, required);
            return AMS_MEL_PROVIDER_EXCEPTION;
        } catch (...) {
            (void)finish_request(state);
            diagnostic("unknown provider send exception", out, capacity, required);
            return AMS_MEL_PROVIDER_EXCEPTION;
        }
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
        const SubmitFailpoint failpoint =
            submit_failpoint("AMS_MEL_TEST_TRACK_UPDATE_POST_SEND_FAILURE");
        if (failpoint == SubmitFailpoint::Allocation) throw std::bad_alloc{};
        if (failpoint == SubmitFailpoint::WorkerLaunch)
            throw std::system_error{
                std::make_error_code(std::errc::resource_unavailable_try_again)};
#endif
        *worker = std::thread{[input] { run_worker(input); }};
        worker->detach();
        input->emergency_self.reset();
        input->launch_state.store(1U, std::memory_order_release);
        *out_request = owner.release();
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        /* The provider send already returned a future. It is retained
         * permanently rather than destroyed with uncertain ownership. */
        retain_worker(input);
        diagnostic("facade allocation failed after provider send", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        retain_worker(input);
        if (worker && worker->joinable()) (void)worker.release();
        input->launch_state.store(2U, std::memory_order_release);
        diagnostic("facade worker launch failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_track_update_request_wait(
    const ams_mel_ir_track_update_request *request, std::uint32_t timeout_ms,
    ams_mel_ir_track_update_result_v1 *result, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!request || !request->state || !result || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    ams_mel_ir_command_status_v1 status{};
    ams_mel_error_code_t error_code{};
    const ams_mel_status_t code = wait_completion(
        request->state, timeout_ms, status, error_code, out, capacity, required,
        "Track update request wait failed");
    /* A timeout leaves the caller's record untouched. */
    if (code == AMS_MEL_TIMEOUT) return code;
    result->status = status;
    result->error_code = error_code;
    return code;
}

extern "C" ams_mel_status_t ams_mel_ir_track_update_request_close(
    ams_mel_ir_track_update_request **request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!request || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    /* Drops the public owner only. Pending provider work, the future, the
     * TrackState graph, and the provider library all survive. */
    try { delete std::exchange(*request, nullptr); return AMS_MEL_OK; }
    catch (...) { return AMS_MEL_INTERNAL_ERROR; }
}

extern "C" ams_mel_status_t ams_mel_ir_track_submit_system_track_data_response(
    ams_mel_ir_track *track,
    const ams_mel_ir_system_track_data_response_v1 *response,
    ams_mel_ir_track_system_response_request **out_request, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    bool az_el_valid = false;
    bool range_valid = false;
    if (!track || !track->state || !response || !out_request || *out_request ||
        (!out && capacity) ||
        !convert_bool(response->az_el_valid, az_el_valid) ||
        !convert_bool(response->range_valid, range_valid)) {
        diagnostic("invalid SystemTrackDataResponse input", out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }

    /* Everything the request needs to own the returned future is allocated
     * before the provider send. */
    std::shared_ptr<Completion> completion;
    std::shared_ptr<WorkerInput> input;
    std::unique_ptr<ams_mel_ir_track_system_response_request> owner;
    std::unique_ptr<std::thread> worker;
    try {
        completion = std::make_shared<Completion>();
        completion->channel = track->state;
        input = std::make_shared<WorkerInput>();
        input->completion = completion;
        owner = std::make_unique<ams_mel_ir_track_system_response_request>();
        owner->state = completion;
        worker = std::make_unique<std::thread>();
    } catch (...) {
        diagnostic("allocation failed before provider send", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }

    /* The complete response is converted and copied here; nothing borrowed
     * outlives this call. */
    irmel::SystemTrackDataResponse upstream;
    try {
        upstream = build_response(*response, az_el_valid, range_valid);
    } catch (...) {
        diagnostic("SystemTrackDataResponse preparation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }

    /* Validate Enabled, copy the shared TrackChannel locally, and account the
     * request under the lifecycle mutex, then release it: provider send() may
     * synchronously invoke the IRSTTrackReport metadata callback, which
     * independently locks the metadata mutex. The count incremented here is the
     * one shared Track request domain, not a second counter. */
    std::shared_ptr<irmel::TrackChannel> channel;
    try {
        std::lock_guard lock{track->state->mutex};
        if (track->state->lifecycle != Lifecycle::Enabled) {
            diagnostic("Track channel is not enabled", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        channel = track->state->track;
        ++track->state->requests;
    } catch (...) {
        diagnostic("Track submission lock failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }

    auto state = track->state;
    try {
        try {
            input->future = channel->send(std::move(upstream));
            arm_worker(input);
        } catch (const std::exception& error) {
            (void)finish_request(state);
            const char *what = error.what();
            const std::string_view text = what ? std::string_view{what} : std::string_view{};
            diagnostic(!text.empty() && valid_utf8(text) ? text : "provider send exception",
                       out, capacity, required);
            return AMS_MEL_PROVIDER_EXCEPTION;
        } catch (...) {
            (void)finish_request(state);
            diagnostic("unknown provider send exception", out, capacity, required);
            return AMS_MEL_PROVIDER_EXCEPTION;
        }
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
        /* The System-response-specific post-send failpoint. It uses the same
         * shared mechanism and the same fail-safe retention path as the
         * TrackDataUpdate failpoint, under its own variable. */
        const SubmitFailpoint failpoint =
            submit_failpoint("AMS_MEL_TEST_TRACK_RESPONSE_POST_SEND_FAILURE");
        if (failpoint == SubmitFailpoint::Allocation) throw std::bad_alloc{};
        if (failpoint == SubmitFailpoint::WorkerLaunch)
            throw std::system_error{
                std::make_error_code(std::errc::resource_unavailable_try_again)};
#endif
        *worker = std::thread{[input] { run_worker(input); }};
        worker->detach();
        input->emergency_self.reset();
        input->launch_state.store(1U, std::memory_order_release);
        *out_request = owner.release();
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        /* The provider send already returned a future. It is retained
         * permanently rather than destroyed with uncertain ownership. */
        retain_worker(input);
        diagnostic("facade allocation failed after provider send", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        retain_worker(input);
        if (worker && worker->joinable()) (void)worker.release();
        input->launch_state.store(2U, std::memory_order_release);
        diagnostic("facade worker launch failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_track_system_response_request_wait(
    const ams_mel_ir_track_system_response_request *request,
    std::uint32_t timeout_ms,
    ams_mel_ir_track_system_response_result_v1 *result, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!request || !request->state || !result || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    ams_mel_ir_command_status_v1 status{};
    ams_mel_error_code_t error_code{};
    const ams_mel_status_t code = wait_completion(
        request->state, timeout_ms, status, error_code, out, capacity, required,
        "Track system response request wait failed");
    /* A timeout leaves the caller's record untouched. */
    if (code == AMS_MEL_TIMEOUT) return code;
    result->status = status;
    result->error_code = error_code;
    return code;
}

extern "C" ams_mel_status_t ams_mel_ir_track_system_response_request_close(
    ams_mel_ir_track_system_response_request **request, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!request || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    /* Drops the public owner only. Pending provider work, the future, the
     * TrackState graph, and the provider library all survive. */
    try { delete std::exchange(*request, nullptr); return AMS_MEL_OK; }
    catch (...) { return AMS_MEL_INTERNAL_ERROR; }
}

extern "C" ams_mel_status_t ams_mel_ir_track_close(
    ams_mel_ir_track **track, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!track || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    auto *owner = *track;
    if (!owner) return AMS_MEL_OK;
    try {
        auto state = owner->state;
        std::shared_ptr<MetadataState> metadata;
        bool now = false;
        {
            std::lock_guard lock{state->mutex};
            state->lifecycle = Lifecycle::Closed;
            metadata = state->metadata;
            /* With requests of either family still pending, the requests own
             * the graph and the final completion of the shared request count
             * performs the physical teardown. */
            now = state->requests == 0U;
        }
        /* Deactivate public metadata consumption and wake receivers before any
         * provider teardown begins, including when teardown is deferred. */
        if (metadata) {
            std::lock_guard lock{metadata->mutex};
            if (metadata->lifecycle == MetadataLifecycle::Active)
                metadata->lifecycle = MetadataLifecycle::Inactive;
            metadata->ready.notify_all();
        }
        const bool ok = !now || cleanup(state, false);
        if (now && state->channel) {
            /* Detach could not be proven: the complete graph and the caller's
             * owner are retained so a later Close can retry. */
            diagnostic("Track detach failed; provider state retained",
                       out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        *track = nullptr;
        delete owner;
        if (!ok) {
            diagnostic("Track disable failed", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        return AMS_MEL_OK;
    } catch (...) {
        diagnostic("Track close failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}
