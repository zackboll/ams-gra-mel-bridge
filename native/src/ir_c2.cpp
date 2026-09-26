#include <ams_mel/abi.h>
#include "internal.hpp"
#include "internal/ir_channel.hpp"
#include "internal/completion_probe.hpp"

#include <irmel/library/c2/C2Channel.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <future>
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
{
    return value.data ? valid_utf8({value.data, value.size}) : value.size == 0U;
}

std::string copy_view(const ams_mel_string_view_v1& value)
{ return value.size ? std::string{value.data, value.size} : std::string{}; }

mel::UCI_ID convert_id(const ams_mel_uci_id_v1& value)
{
    std::array<std::uint8_t, mel::UUID_SIZE> uuid{};
    std::copy(std::begin(value.uuid), std::end(value.uuid), uuid.begin());
    return {uuid, copy_view(value.descriptive_label)};
}

bool has_c2(const irmel::ChannelCapability& capability)
{
    const auto& types = capability.getChannelTypes();
    return std::find(types.begin(), types.end(), irmel::ChannelType::CommandAndControl) !=
           types.end();
}

ams_mel_error_code_t map_error(mel::ErrorCode code, bool& known) noexcept
{
    known = true;
    switch (code) {
    case mel::ErrorCode::None: return AMS_MEL_ERROR_NONE;
    case mel::ErrorCode::InvalidId: return AMS_MEL_ERROR_INVALID_ID;
    case mel::ErrorCode::InvalidState: return AMS_MEL_ERROR_INVALID_STATE;
    case mel::ErrorCode::InvalidParameters: return AMS_MEL_ERROR_INVALID_PARAMETERS;
    case mel::ErrorCode::InsufficientPermissions: return AMS_MEL_ERROR_INSUFFICIENT_PERMISSIONS;
    case mel::ErrorCode::InsufficientResources: return AMS_MEL_ERROR_INSUFFICIENT_RESOURCES;
    case mel::ErrorCode::InsufficientLocalResources: return AMS_MEL_ERROR_INSUFFICIENT_LOCAL_RESOURCES;
    case mel::ErrorCode::InsufficientRemoteResources: return AMS_MEL_ERROR_INSUFFICIENT_REMOTE_RESOURCES;
    case mel::ErrorCode::Unsupported: return AMS_MEL_ERROR_UNSUPPORTED;
    }
    known = false;
    return AMS_MEL_ERROR_NONE;
}

bool map_mode(irmel::MFA_Mode mode, ams_mel_ir_mfa_mode_t& value) noexcept
{
    switch (mode) {
    case irmel::MFA_Mode::Unused: value = AMS_MEL_IR_MFA_MODE_UNUSED; return true;
    case irmel::MFA_Mode::TaskSched: value = AMS_MEL_IR_MFA_MODE_TASK_SCHED; return true;
    case irmel::MFA_Mode::ScanVolumeSched: value = AMS_MEL_IR_MFA_MODE_SCAN_VOLUME_SCHED; return true;
    case irmel::MFA_Mode::ScanBarSched: value = AMS_MEL_IR_MFA_MODE_SCAN_BAR_SCHED; return true;
    }
    return false;
}

bool convert_state(ams_mel_ir_mfa_state_t value, mel::MFA_State& state) noexcept
{
    if (value >= AMS_MEL_IR_MFA_STATE_MAX_EXCLUSIVE) return false;
    state = static_cast<mel::MFA_State>(value);
    return true;
}

bool convert_mode(ams_mel_ir_mfa_mode_t value, irmel::MFA_Mode& mode) noexcept
{
    if (value > AMS_MEL_IR_MFA_MODE_SCAN_BAR_SCHED) return false;
    mode = static_cast<irmel::MFA_Mode>(value);
    return true;
}

bool valid_scan(const ams_mel_ir_scan_param_v1& value) noexcept
{
    return value.elevation_defined_with_range_and_altitude <= 1U &&
           value.center_frame_ref_el <= AMS_MEL_IR_COORD_FRAME_AIRCRAFT &&
           value.center_frame_ref_az <= AMS_MEL_IR_COORD_FRAME_AIRCRAFT &&
           value.degradation_method <= AMS_MEL_IR_DEGRADATION_REVISIT;
}

irmel::ScanParam convert_scan(const ams_mel_ir_scan_param_v1& value)
{
    irmel::ScanType type;
    type.setContinuousScan(value.scan_type.continuous_scan);
    type.setReturning(value.scan_type.returning);
    type.setAgileScan(value.scan_type.agile_scan);
    return {value.elevation_defined_with_range_and_altitude != 0U,
            AzEl{value.center_az_rad, value.center_el_rad},
            static_cast<irmel::CoordFrameRef>(value.center_frame_ref_el),
            static_cast<irmel::CoordFrameRef>(value.center_frame_ref_az),
            value.scan_width_rad, value.scan_height_rad, std::move(type),
            value.scan_id, value.scan_rate_rad_per_second,
            value.preferred_revisit_interval_seconds,
            value.required_revisit_interval_seconds,
            value.max_range_of_interest_m, value.min_range_of_interest_m,
            value.elevation_scan_center_altitude_m,
            value.elevation_scan_center_range_m,
            static_cast<irmel::DegradationMethod>(value.degradation_method)};
}

bool valid_span(const ams_mel_u32_span_v1& span) noexcept
{ return span.data != nullptr || span.size == 0U; }

bool valid_span(const ams_mel_string_view_span_v1& span) noexcept
{
    if (!span.data && span.size != 0U) return false;
    for (std::size_t index = 0; index < span.size; ++index)
        if (!valid_view(span.data[index])) return false;
    return true;
}

bool map_return(irmel::Return input, ams_mel_ir_return_t& value) noexcept
{
    switch (input) {
    case irmel::Return::Success: value = AMS_MEL_IR_RETURN_SUCCESS; return true;
    case irmel::Return::BadPointer: value = AMS_MEL_IR_RETURN_BAD_POINTER; return true;
    case irmel::Return::Fail: value = AMS_MEL_IR_RETURN_FAIL; return true;
    case irmel::Return::NotSupported: value = AMS_MEL_IR_RETURN_NOT_SUPPORTED; return true;
    case irmel::Return::NotImplemented: value = AMS_MEL_IR_RETURN_NOT_IMPLEMENTED; return true;
    }
    return false;
}

void increment(std::uint64_t& value) noexcept
{ if (value != UINT64_MAX) ++value; }

struct OwnedID { std::array<std::uint8_t, 16> uuid{}; std::string label; };
struct OwnedBitType { OwnedID id; std::uint32_t interface{}; std::vector<std::string> names; std::vector<OwnedID> components; std::int64_t duration{}; };
struct OwnedActive { OwnedID id; std::int64_t completion{}; double percent{}; };
struct OwnedItem { std::string name; std::uint32_t result{}; std::string reason; };
struct OwnedCompleted { OwnedID id; std::int64_t time{}; std::uint32_t result{}; std::string reason; std::vector<OwnedItem> items; };
struct OwnedFaultData { std::string key, value, format, units; };
struct OwnedAmbiguity { std::vector<OwnedID> tests, components; };
struct OwnedFault { OwnedID id; std::uint32_t severity{}, state{}; std::vector<OwnedFaultData> data; std::int64_t time{}; std::string code, description; std::vector<OwnedID> components; std::vector<OwnedAmbiguity> groups; };

struct EventData {
    ams_mel_ir_c2_metadata_event_v1 view{};
    std::string command_description;
    std::vector<OwnedBitType> bit_types;
    std::vector<OwnedActive> active;
    std::vector<OwnedCompleted> completed;
    std::vector<OwnedFault> faults;
    std::vector<ams_mel_bit_type_v1> bit_type_views;
    std::vector<std::vector<ams_mel_string_view_v1>> name_views;
    std::vector<std::vector<ams_mel_uci_id_v1>> bit_component_views;
    std::vector<ams_mel_active_bit_v1> active_views;
    std::vector<ams_mel_completed_bit_v1> completed_views;
    std::vector<std::vector<ams_mel_completed_bit_item_v1>> item_views;
    std::vector<ams_mel_fault_v1> fault_views;
    std::vector<std::vector<ams_mel_fault_data_v1>> fault_data_views;
    std::vector<std::vector<ams_mel_uci_id_v1>> fault_component_views;
    std::vector<std::vector<ams_mel_fault_ambiguity_group_v1>> group_views;
    std::vector<std::vector<std::vector<ams_mel_uci_id_v1>>> group_test_views, group_component_views;
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
    output.uuid = input.getUUID(); output.label = input.getDescriptiveLabel(); return true;
}

bool valid_result(mel::BIT_Result value) noexcept
{ return static_cast<std::uint32_t>(value) < static_cast<std::uint32_t>(mel::BIT_Result::MaxExclusive); }

void build_views(EventData& event)
{
    if (event.view.kind == AMS_MEL_IR_C2_METADATA_CHANNEL_COMMS_TEST) return;
    if (event.view.kind == AMS_MEL_IR_C2_METADATA_COMMAND_STATUS) {
        event.view.command_status.reason_description = string_view(event.command_description);
        return;
    }
    if (event.view.kind == AMS_MEL_IR_C2_METADATA_BIT_CONFIGURATION) {
        event.bit_type_views.resize(event.bit_types.size());
        event.name_views.resize(event.bit_types.size()); event.bit_component_views.resize(event.bit_types.size());
        for (std::size_t i=0; i<event.bit_types.size(); ++i) {
            const auto& source=event.bit_types[i]; auto& names=event.name_views[i]; auto& ids=event.bit_component_views[i];
            for (const auto& value:source.names) names.push_back(string_view(value));
            for (const auto& value:source.components) ids.push_back(id_view(value));
            event.bit_type_views[i]={id_view(source.id), source.interface, {names.data(),names.size()}, {ids.data(),ids.size()}, source.duration};
        }
        event.view.bit_configuration.bit_types={event.bit_type_views.data(),event.bit_type_views.size()}; return;
    }
    event.active_views.resize(event.active.size());
    for (std::size_t i=0;i<event.active.size();++i) event.active_views[i]={id_view(event.active[i].id),event.active[i].completion,event.active[i].percent};
    event.completed_views.resize(event.completed.size()); event.item_views.resize(event.completed.size());
    for (std::size_t i=0;i<event.completed.size();++i) {
        auto& items=event.item_views[i]; for(const auto& item:event.completed[i].items) items.push_back({string_view(item.name),item.result,string_view(item.reason)});
        const auto& source=event.completed[i]; event.completed_views[i]={id_view(source.id),source.time,source.result,string_view(source.reason),{items.data(),items.size()}};
    }
    event.fault_views.resize(event.faults.size()); event.fault_data_views.resize(event.faults.size()); event.fault_component_views.resize(event.faults.size());
    event.group_views.resize(event.faults.size()); event.group_test_views.resize(event.faults.size()); event.group_component_views.resize(event.faults.size());
    for(std::size_t i=0;i<event.faults.size();++i){ const auto& source=event.faults[i]; auto& data=event.fault_data_views[i];
        for(const auto& value:source.data)data.push_back({string_view(value.key),string_view(value.value),string_view(value.format),string_view(value.units)});
        auto& components=event.fault_component_views[i]; for(const auto& value:source.components)components.push_back(id_view(value));
        event.group_test_views[i].resize(source.groups.size()); event.group_component_views[i].resize(source.groups.size()); auto& groups=event.group_views[i];
        for(std::size_t j=0;j<source.groups.size();++j){for(const auto& value:source.groups[j].tests)event.group_test_views[i][j].push_back(id_view(value)); for(const auto& value:source.groups[j].components)event.group_component_views[i][j].push_back(id_view(value)); groups.push_back({{event.group_test_views[i][j].data(),event.group_test_views[i][j].size()},{event.group_component_views[i][j].data(),event.group_component_views[i][j].size()}});}
        event.fault_views[i]={id_view(source.id),source.severity,source.state,{data.data(),data.size()},source.time,string_view(source.code),string_view(source.description),{components.data(),components.size()},{groups.data(),groups.size()}};
    }
    event.view.bit_status={{event.active_views.data(),event.active_views.size()},{event.completed_views.data(),event.completed_views.size()},{event.fault_views.data(),event.fault_views.size()}};
}

std::unique_ptr<EventData> copy_comms_test(const irmel::ChannelCommsTestRep *value)
{
    if (!value) return {};
    auto event = std::make_unique<EventData>();
    event->view.kind = AMS_MEL_IR_C2_METADATA_CHANNEL_COMMS_TEST;
    event->view.channel_comms_test = {value->getCommandID(), value->getRequestID()};
    return event;
}

enum class MetadataLifecycle { Active, Inactive, Stopped, Failed };
struct MetadataState {
    std::mutex mutex; std::condition_variable ready, callbacks_done;
    std::deque<std::unique_ptr<EventData>> queue; std::size_t capacity{};
    ams_mel_ir_c2_metadata_counters_v1 counters{}; MetadataLifecycle lifecycle{MetadataLifecycle::Active};
    std::atomic<std::uint64_t> callbacks{};

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

struct CallbackGuard { std::shared_ptr<MetadataState> state; explicit CallbackGuard(std::shared_ptr<MetadataState> value) noexcept:state(std::move(value)){state->callbacks.fetch_add(1,std::memory_order_acq_rel);} ~CallbackGuard() noexcept {if(state->callbacks.fetch_sub(1,std::memory_order_acq_rel)==1)state->callbacks_done.notify_all();} };

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
void metadata_callback_test_barrier()
{
    const char *base = std::getenv("AMS_MEL_TEST_METADATA_CALLBACK_BARRIER");
    if (!base) return;
    const std::string entered = std::string{base} + ".entered";
    const std::string release = std::string{base} + ".release";
    { std::ofstream marker{entered}; marker << "entered\n"; }
    /* The provider-side test waits at most five seconds to observe entry. Keep
     * this watchdog longer so equal deadlines cannot race under heavy load. */
    for (unsigned attempt = 0; attempt < 15000U; ++attempt) {
        if (std::ifstream{release}.good()) return;
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    throw std::runtime_error("metadata callback test barrier timed out");
}
#endif

template<class Build> void metadata_callback(const std::shared_ptr<MetadataState>& state, Build build) noexcept
{
    try {
        CallbackGuard guard{state};
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
        metadata_callback_test_barrier();
#endif
        { std::lock_guard lock{state->mutex}; increment(state->counters.events_received); if(state->lifecycle!=MetadataLifecycle::Active)return; }
        auto event=build();
        std::lock_guard lock{state->mutex}; if(state->lifecycle!=MetadataLifecycle::Active)return;
        if(!event){increment(state->counters.malformed_or_unsupported);return;}
        if(state->queue.size()>=state->capacity){increment(state->counters.events_dropped_queue_full);return;}
        state->queue.push_back(std::move(event)); state->ready.notify_one();
    } catch (...) { state->fail(); }
}

std::unique_ptr<EventData> copy_command_status(const irmel::CommandStatus *input)
{
    if(!input)return {};
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
    if (const char *value = std::getenv("AMS_MEL_TEST_METADATA_CALLBACK_FAILURE");
        value && std::strcmp(value, "command-allocation") == 0)
        throw std::bad_alloc{};
#endif
    const auto state=static_cast<std::uint32_t>(input->getState()), reason=static_cast<std::uint32_t>(input->getReasonID());
    if(state>AMS_MEL_IR_COMMAND_CANCELLED || reason>AMS_MEL_IR_CANNOT_COMPLY_ALIGNMENT_MANEUVER || !valid_utf8(input->getReasonDescription()))return {};
    auto event=std::make_unique<EventData>(); event->view.kind=AMS_MEL_IR_C2_METADATA_COMMAND_STATUS;
    event->view.command_status={input->getCommandID(),state,reason,{}}; event->command_description=input->getReasonDescription(); build_views(*event); return event;
}
std::unique_ptr<EventData> copy_bit_configuration(const mel::BIT_Configuration *input)
{
    if (!input) return {};
    auto event = std::make_unique<EventData>();
    event->view.kind = AMS_MEL_IR_C2_METADATA_BIT_CONFIGURATION;
    for(const auto& source:input->getBit()){
        const auto interface=static_cast<std::uint32_t>(source.getAcceptedInterface()); if(interface>=static_cast<std::uint32_t>(mel::BIT_ControlInterface::MaxExclusive))return {};
        OwnedBitType value; if(!copy_id(source.getBitID(),value.id))return {}; value.interface=interface; value.duration=source.getExpectedDuration().count();
        for(const auto& text:source.getBitItemName()){if(!valid_utf8(text))return {};value.names.push_back(text);} for(const auto& id:source.getSubsystemComponentID()){OwnedID copied;if(!copy_id(id,copied))return {};value.components.push_back(std::move(copied));} event->bit_types.push_back(std::move(value));
    } build_views(*event); return event;
}
std::unique_ptr<EventData> copy_bit_status(const mel::BIT_Status *input)
{
    if (!input) return {};
    auto event = std::make_unique<EventData>();
    event->view.kind = AMS_MEL_IR_C2_METADATA_BIT_STATUS;
    for(const auto& source:input->getActiveBITs()){OwnedActive value;if(!copy_id(source.getBitID(),value.id))return {};value.completion=source.getEstimatedCompletionTime().count();value.percent=source.getEstimatedPercentComplete();event->active.push_back(std::move(value));}
    for(const auto& source:input->getCompletedBITs()){if(!valid_result(source.getResult())||!valid_utf8(source.getFailReason()))return {};OwnedCompleted value;if(!copy_id(source.getBitID(),value.id))return {};value.time=source.getTimeTag().count();value.result=static_cast<std::uint32_t>(source.getResult());value.reason=source.getFailReason();for(const auto& item:source.getBitItem()){if(!valid_result(item.getResult())||!valid_utf8(item.getBitItemName())||!valid_utf8(item.getFailReason()))return {};value.items.push_back({item.getBitItemName(),static_cast<std::uint32_t>(item.getResult()),item.getFailReason()});}event->completed.push_back(std::move(value));}
    for(const auto& source:input->getFaults()){
        const auto severity=static_cast<std::uint32_t>(source.getSeverity()), state=static_cast<std::uint32_t>(source.getState()); if(severity>=static_cast<std::uint32_t>(mel::FaultSeverity::MaxExclusive)||state>=static_cast<std::uint32_t>(mel::FaultState::MaxExclusive)||!valid_utf8(source.getFaultCode())||!valid_utf8(source.getFaultDescription()))return {};
        OwnedFault value;if(!copy_id(source.getFaultID(),value.id))return {};value.severity=severity;value.state=state;value.time=source.getDetectionTime().count();value.code=source.getFaultCode();value.description=source.getFaultDescription();
        for(const auto& data:source.getFaultData()){if(!valid_utf8(data.getKey())||!valid_utf8(data.getValue())||!valid_utf8(data.getFormat())||!valid_utf8(data.getUnits()))return {};value.data.push_back({data.getKey(),data.getValue(),data.getFormat(),data.getUnits()});}
        for(const auto& id:source.getComponentID()){OwnedID copied;if(!copy_id(id,copied))return {};value.components.push_back(std::move(copied));}
        for(const auto& group:source.getAmbiguityGroup()){OwnedAmbiguity copied;for(const auto& id:group.getDiagnosticTestID()){OwnedID item;if(!copy_id(id,item))return {};copied.tests.push_back(std::move(item));}for(const auto& id:group.getComponentID()){OwnedID item;if(!copy_id(id,item))return {};copied.components.push_back(std::move(item));}value.groups.push_back(std::move(copied));}event->faults.push_back(std::move(value));
    } build_views(*event); return event;
}

enum class C2Lifecycle { Attached, Enabled, Failed, Closed };

struct ChannelState {
    std::mutex mutex;
    std::shared_ptr<SessionState> session;
    std::shared_ptr<irmel::Channel> channel;
    std::shared_ptr<irmel::C2Channel> c2;
    C2Lifecycle lifecycle{C2Lifecycle::Attached};
    bool enabled{};
    bool enable_attempted{};
    std::size_t requests{};
    bool cleanup_started{};
    std::shared_ptr<ChannelState> emergency_self;
    ChannelState *emergency_next{};
    std::atomic<bool> emergency_retained{};
    std::shared_ptr<struct MetadataState> metadata;
    bool metadata_attempted{};
    bool comms_metadata_attempted{};
};

/* A failed deferred detach cannot safely destroy its graph. Keep it for process
 * lifetime rather than unload provider code that may still own the channel.
 * The intrusive root and pre-existing shared_ptr cycle require no allocation. */
void retain_failed(const std::shared_ptr<ChannelState>& state) noexcept
{
    static std::atomic<ChannelState *> retained{};
    bool expected = false;
    if (!state->emergency_retained.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) return;
    state->emergency_self = state;
    ChannelState *head = retained.load(std::memory_order_relaxed);
    do { state->emergency_next = head; }
    while (!retained.compare_exchange_weak(
        head, state.get(), std::memory_order_release, std::memory_order_relaxed));
}

bool cleanup(const std::shared_ptr<ChannelState>& state,
             bool retain_if_orphaned)
{
    std::shared_ptr<irmel::Channel> channel;
    std::shared_ptr<MetadataState> metadata;
    bool disable = false;
    {
        std::lock_guard lock{state->mutex};
        if (state->cleanup_started || state->requests != 0U) return true;
        state->cleanup_started = true;
        channel = state->channel;
        metadata = state->metadata;
        disable = state->enable_attempted;
    }
    if (metadata) {
        std::lock_guard lock{metadata->mutex};
        if (metadata->lifecycle == MetadataLifecycle::Active)
            metadata->lifecycle = MetadataLifecycle::Inactive;
    }
    bool ok = true;
    if (channel && disable) {
        try { if (channel->disable() != irmel::Return::Success) ok = false; }
        catch (...) { ok = false; }
    }
    bool detached = !channel;
    if (channel) {
        try {
            detached = state->session->control->detachChannel(channel) ==
                       irmel::Return::Success;
        } catch (...) { detached = false; }
    }
    if (!detached) {
        if (metadata) {
            std::lock_guard lock{metadata->mutex};
            metadata->lifecycle = MetadataLifecycle::Failed;
            metadata->ready.notify_all();
        }
        {
            std::lock_guard lock{state->mutex};
            state->cleanup_started = false;
        }
        if (retain_if_orphaned) retain_failed(state);
        return false;
    }
    {
        std::lock_guard lock{state->mutex};
        state->c2.reset();
        state->channel.reset();
        state->enabled = false;
        state->enable_attempted = false;
        state->lifecycle = C2Lifecycle::Closed;
    }
    channel.reset();
    if (metadata) {
        std::unique_lock lock{metadata->mutex};
        metadata->callbacks_done.wait(lock, [&]{ return metadata->callbacks.load(std::memory_order_acquire) == 0U; });
        if (metadata->lifecycle != MetadataLifecycle::Failed) metadata->lifecycle = MetadataLifecycle::Stopped;
        metadata->ready.notify_all();
    }
    return ok;
}

enum class CompletionKind { Pending, Success, Rejected, ProviderException, ProviderFailure, InternalError };

struct Completion {
    std::mutex mutex;
    std::condition_variable ready;
    CompletionKind kind{CompletionKind::Pending};
    ams_mel_ir_mode_result_v1 result{};
    std::string message;
    std::shared_ptr<ChannelState> channel;
};

struct WorkerInput {
    AMS_MEL_PROBE_OWNER(Mode)
    CompletionPermit admission;
    std::shared_ptr<Completion> completion;
    mel::RequestFor<irmel::MFA_Mode> future;
    std::shared_ptr<WorkerInput> emergency_self;
    WorkerInput *emergency_next{};
    std::atomic<bool> emergency_retained{};
    std::atomic<unsigned> launch_state{};
};

void arm_worker(const std::shared_ptr<WorkerInput>& input) noexcept
{
    input->emergency_self = input;
}

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

enum class SubmissionRequirement { Enabled, AttachedOrEnabled };

/* The lifecycle lock linearizes Close against submission. A successful claim
 * reserves physical lifetime before unlocking: later Close defers cleanup.
 * If Close wins first, no provider call is made. Never hold this lock across
 * provider code, including synchronous metadata callbacks. */
bool claim_c2_submission(const std::shared_ptr<ChannelState>& state,
                         SubmissionRequirement requirement,
                         std::shared_ptr<irmel::C2Channel>& channel)
{
    std::lock_guard lock{state->mutex};
    if (state->lifecycle != C2Lifecycle::Enabled &&
        !(requirement == SubmissionRequirement::AttachedOrEnabled &&
          state->lifecycle == C2Lifecycle::Attached)) return false;
    channel = state->c2;
    ++state->requests;
    return true;
}

bool finish_channel(const std::shared_ptr<ChannelState>& channel)
{
    bool close = false;
    {
        std::lock_guard lock{channel->mutex};
        if (channel->requests) --channel->requests;
        close = channel->requests == 0U && channel->lifecycle == C2Lifecycle::Closed;
    }
    return !close || cleanup(channel, true);
}

void complete(const std::shared_ptr<Completion>& state,
              mel::RequestFor<irmel::MFA_Mode>& future)
{
    CompletionKind kind = CompletionKind::ProviderException;
    ams_mel_ir_mode_result_v1 result{};
    std::string message;
    try {
        auto outcome = AMS_MEL_PROBE_GET(Mode, future.get());
        if (outcome) {
            const auto& value = outcome.get();
            if (!value) {
                kind = CompletionKind::ProviderFailure;
                message = "provider returned null successful mode result";
            } else if (!map_mode(*value, result.mode)) {
                kind = CompletionKind::ProviderFailure;
                message = "provider returned unknown MFA mode";
            } else {
                kind = CompletionKind::Success;
            }
        } else {
            const mel::Error& error = outcome.getError();
            bool known = false;
            result.error_code = map_error(error.getCode(), known);
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
    AMS_MEL_PROBE_BOUNDARY(Mode);
    auto channel = state->channel;
    const bool cleanup_ok = finish_channel(channel);
    if (!cleanup_ok) {
        kind = CompletionKind::ProviderFailure;
        try { message = "deferred C2 cleanup failed"; }
        catch (...) { message.clear(); }
    }
    {
        std::lock_guard lock{state->mutex};
        state->kind = kind;
        state->result = result;
        state->message = std::move(message);
        state->channel.reset();
        AMS_MEL_PROBE_GRAPH(Mode, channel);
    }
    channel.reset();
    state->ready.notify_all();
}

void run_worker(const std::shared_ptr<WorkerInput>& input) noexcept
{
    while (input->launch_state.load(std::memory_order_acquire) == 0U)
        std::this_thread::yield();
    AMS_MEL_PROBE_WORKER(Mode);
    try {
        complete(input->completion, input->future); AMS_MEL_PROBE_RETURN(Mode);
    } catch (...) {
        /* A mutex/system failure must neither escape the detached thread nor
         * destroy an unaccounted future/provider graph. */
        arm_worker(input);
        retain_worker(input);
    }
}

struct ReturnCompletion {
    std::mutex mutex;
    std::condition_variable ready;
    CompletionKind kind{CompletionKind::Pending};
    ams_mel_ir_return_result_v1 result{};
    std::string message;
    std::shared_ptr<ChannelState> channel;
};

struct ReturnWorkerInput {
    AMS_MEL_PROBE_OWNER(Return)
    CompletionPermit admission;
    std::shared_ptr<ReturnCompletion> completion;
    mel::RequestFor<irmel::Return> future;
    std::shared_ptr<ReturnWorkerInput> emergency_self;
    ReturnWorkerInput *emergency_next{};
    std::atomic<bool> emergency_retained{};
    std::atomic<unsigned> launch_state{};
};

void arm_return_worker(const std::shared_ptr<ReturnWorkerInput>& input) noexcept
{
    input->emergency_self = input;
}

void retain_return_worker(const std::shared_ptr<ReturnWorkerInput>& input) noexcept
{
    static std::atomic<ReturnWorkerInput *> retained{};
    bool expected = false;
    if (!input->emergency_retained.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) return;
    ReturnWorkerInput *head = retained.load(std::memory_order_relaxed);
    do { input->emergency_next = head; }
    while (!retained.compare_exchange_weak(
        head, input.get(), std::memory_order_release, std::memory_order_relaxed));
}

void complete_return(const std::shared_ptr<ReturnCompletion>& state,
                     mel::RequestFor<irmel::Return>& future)
{
    CompletionKind kind = CompletionKind::ProviderException;
    ams_mel_ir_return_result_v1 result{};
    std::string message;
    try {
        auto outcome = AMS_MEL_PROBE_GET(Return, future.get());
        if (outcome) {
            const auto& value = outcome.get();
            if (!value) {
                kind = CompletionKind::ProviderFailure;
                message = "provider returned null successful Return result";
            } else if (!map_return(*value, result.value)) {
                kind = CompletionKind::ProviderFailure;
                message = "provider returned unknown IR Return value";
            } else {
                kind = CompletionKind::Success;
            }
        } else {
            const mel::Error& error = outcome.getError();
            bool known = false;
            result.error_code = map_error(error.getCode(), known);
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
    AMS_MEL_PROBE_BOUNDARY(Return);
    auto channel = state->channel;
    if (!finish_channel(channel)) {
        kind = CompletionKind::ProviderFailure;
        try { message = "deferred C2 cleanup failed"; }
        catch (...) { message.clear(); }
    }
    {
        std::lock_guard lock{state->mutex};
        state->kind = kind;
        state->result = result;
        state->message = std::move(message);
        state->channel.reset();
        AMS_MEL_PROBE_GRAPH(Return, channel);
    }
    channel.reset();
    state->ready.notify_all();
}

void run_return_worker(const std::shared_ptr<ReturnWorkerInput>& input) noexcept
{
    while (input->launch_state.load(std::memory_order_acquire) == 0U)
        std::this_thread::yield();
    AMS_MEL_PROBE_WORKER(Return);
    try {
        complete_return(input->completion, input->future); AMS_MEL_PROBE_RETURN(Return);
    } catch (...) {
        arm_return_worker(input);
        retain_return_worker(input);
    }
}

struct CommsCompletion {
    std::mutex mutex;
    std::condition_variable ready;
    CompletionKind kind{CompletionKind::Pending};
    ams_mel_ir_channel_comms_test_result_v1 result{};
    std::string message;
    std::shared_ptr<ChannelState> channel;
};
struct CommsWorkerInput {
    AMS_MEL_PROBE_OWNER(Comms)
    CompletionPermit admission;
    std::shared_ptr<CommsCompletion> completion;
    mel::RequestFor<irmel::ChannelCommsTestRep> future;
    std::shared_ptr<CommsWorkerInput> emergency_self;
    CommsWorkerInput *emergency_next{};
    std::atomic<bool> emergency_retained{};
    std::atomic<unsigned> launch_state{};
};
void arm_comms_worker(const std::shared_ptr<CommsWorkerInput>& input) noexcept
{ input->emergency_self = input; }
void retain_comms_worker(const std::shared_ptr<CommsWorkerInput>& input) noexcept
{
    static std::atomic<CommsWorkerInput *> retained{};
    bool expected = false;
    if (!input->emergency_retained.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) return;
    CommsWorkerInput *head = retained.load(std::memory_order_relaxed);
    do { input->emergency_next = head; }
    while (!retained.compare_exchange_weak(
        head, input.get(), std::memory_order_release, std::memory_order_relaxed));
}
void complete_comms(const std::shared_ptr<CommsCompletion>& state,
                    mel::RequestFor<irmel::ChannelCommsTestRep>& future)
{
    CompletionKind kind = CompletionKind::ProviderException;
    ams_mel_ir_channel_comms_test_result_v1 result{};
    std::string message;
    try {
        auto outcome = AMS_MEL_PROBE_GET(Comms, future.get());
        if (outcome) {
            const auto& value = outcome.get();
            if (!value) {
                kind = CompletionKind::ProviderFailure;
                message = "provider returned null successful CommsTest result";
            } else {
                result.command_id = value->getCommandID();
                result.request_id = value->getRequestID();
                result.error_code = AMS_MEL_ERROR_NONE;
                kind = CompletionKind::Success;
            }
        } else {
            const mel::Error& error = outcome.getError();
            bool known = false;
            result.error_code = map_error(error.getCode(), known);
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
    AMS_MEL_PROBE_BOUNDARY(Comms);
    auto channel = state->channel;
    if (!finish_channel(channel)) {
        kind = CompletionKind::ProviderFailure;
        try { message = "deferred C2 cleanup failed"; } catch (...) { message.clear(); }
    }
    {
        std::lock_guard lock{state->mutex};
        state->kind = kind; state->result = result;
        state->message = std::move(message); state->channel.reset();
        AMS_MEL_PROBE_GRAPH(Comms, channel);
    }
    channel.reset(); state->ready.notify_all();
}
void run_comms_worker(const std::shared_ptr<CommsWorkerInput>& input) noexcept
{
    while (input->launch_state.load(std::memory_order_acquire) == 0U)
        std::this_thread::yield();
    AMS_MEL_PROBE_WORKER(Comms);
    try { complete_comms(input->completion, input->future); AMS_MEL_PROBE_RETURN(Comms); }
    catch (...) { arm_comms_worker(input); retain_comms_worker(input); }
}

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
enum class SubmitFailpoint { None, Allocation, WorkerLaunch };

SubmitFailpoint submit_failpoint() noexcept
{
    const char *value = std::getenv("AMS_MEL_TEST_C2_POST_SEND_FAILURE");
    if (value && std::strcmp(value, "allocation") == 0)
        return SubmitFailpoint::Allocation;
    if (value && std::strcmp(value, "worker-launch") == 0)
        return SubmitFailpoint::WorkerLaunch;
    return SubmitFailpoint::None;
}
#endif
} // namespace

struct ams_mel_ir_c2 { std::shared_ptr<ChannelState> state; };
struct ams_mel_ir_mode_request { std::shared_ptr<Completion> state; };
struct ams_mel_ir_return_request { std::shared_ptr<ReturnCompletion> state; };
struct ams_mel_ir_channel_comms_request { std::shared_ptr<CommsCompletion> state; };
struct ams_mel_ir_c2_metadata {
    std::shared_ptr<MetadataState> state;
    std::weak_ptr<ChannelState> channel;
};
struct ams_mel_ir_c2_metadata_event { std::unique_ptr<EventData> data; };

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
/* Test-owned state observation survives public wrapper Close. It deliberately
 * keeps the Session alive until operation 2; it owns no extra C2Channel. */
extern "C" __attribute__((visibility("default"))) int ams_mel_test_c2_submission(
    ams_mel_ir_c2 *c2, unsigned operation, void **token, std::size_t *requests) noexcept
{
    try {
        if (operation == 0U) {
            *token = new std::shared_ptr<ChannelState>{c2->state};
            return 1;
        }
        auto *owner = static_cast<std::shared_ptr<ChannelState> *>(*token);
        if (operation == 2U) { delete owner; *token = nullptr; return 1; }
        if (operation != 1U || !owner) return 0;
        std::unique_lock lock{(*owner)->mutex, std::try_to_lock};
        if (!lock.owns_lock()) return 0;
        *requests = (*owner)->requests;
        return 1;
    } catch (...) { return 0; }
}
#endif

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
extern "C" __attribute__((visibility("default"))) int ams_mel_test_c2_requests(
    const ams_mel_ir_c2 *owner, std::size_t *requests) noexcept
{
    try {
        if (!owner || !requests) return 0;
        std::lock_guard lock{owner->state->mutex};
        *requests = owner->state->requests;
        return 1;
    } catch (...) { return 0; }
}
#endif

namespace {
ams_mel_status_t submit_mode_command(
    ams_mel_ir_c2 *c2, irmel::ModeCmd command,
    ams_mel_ir_mode_request **out_request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    const auto state = c2->state;
    std::shared_ptr<Completion> completion;
    std::shared_ptr<WorkerInput> input;
    std::unique_ptr<ams_mel_ir_mode_request> owner;
    std::unique_ptr<std::thread> worker;
    try {
        completion = std::make_shared<Completion>();
        completion->channel = state;
        input = std::make_shared<WorkerInput>();
        input->completion = completion;
        owner = std::make_unique<ams_mel_ir_mode_request>();
        owner->state = completion;
        worker = std::make_unique<std::thread>();
    } catch (...) {
        diagnostic("allocation failed before provider send", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    std::shared_ptr<irmel::C2Channel> channel;
    try {
        if (!acquire_completion_permit(state->session->admission, input->admission)) {
            diagnostic("async request limit reached", out, capacity, required);
            return AMS_MEL_RESOURCE_EXHAUSTED;
        }
        if (!claim_c2_submission(state, SubmissionRequirement::Enabled, channel)) {
            diagnostic("C2 channel is not enabled", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
    } catch (...) {
        diagnostic("C2 submission lock failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    try {
        try {
            input->future = channel->send(std::move(command));
            arm_worker(input);
        } catch (const std::exception& error) {
            channel.reset();
            (void)finish_channel(state);
            const char *what = error.what();
            const std::string_view text = what ? std::string_view{what} : std::string_view{};
            diagnostic(!text.empty() && valid_utf8(text) ? text : "provider send exception", out, capacity, required);
            return AMS_MEL_PROVIDER_EXCEPTION;
        } catch (...) {
            channel.reset();
            (void)finish_channel(state);
            diagnostic("unknown provider send exception", out, capacity, required);
            return AMS_MEL_PROVIDER_EXCEPTION;
        }
        /* Only the accounted graph may own the channel when cleanup runs. */
        channel.reset();
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
        const SubmitFailpoint failpoint = submit_failpoint();
        if (failpoint == SubmitFailpoint::Allocation) throw std::bad_alloc{};
        if (failpoint == SubmitFailpoint::WorkerLaunch)
            throw std::system_error{std::make_error_code(std::errc::resource_unavailable_try_again)};
#endif
        *worker = std::thread{[input] { run_worker(input); }};
        worker->detach();
        input->emergency_self.reset();
        input->launch_state.store(1U, std::memory_order_release);
        *out_request = owner.release();
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
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

template<typename Send>
ams_mel_status_t submit_return_operation(
    ams_mel_ir_c2 *c2, Send send, bool require_enabled,
    ams_mel_ir_return_request **out_request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    const auto state = c2->state;
    std::shared_ptr<ReturnCompletion> completion;
    std::shared_ptr<ReturnWorkerInput> input;
    std::unique_ptr<ams_mel_ir_return_request> owner;
    std::unique_ptr<std::thread> worker;
    try {
        completion = std::make_shared<ReturnCompletion>();
        completion->channel = state;
        input = std::make_shared<ReturnWorkerInput>();
        input->completion = completion;
        owner = std::make_unique<ams_mel_ir_return_request>();
        owner->state = completion;
        worker = std::make_unique<std::thread>();
    } catch (...) {
        diagnostic("allocation failed before provider send", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    std::shared_ptr<irmel::C2Channel> channel;
    try {
        if (!acquire_completion_permit(state->session->admission, input->admission)) {
            diagnostic("async request limit reached", out, capacity, required);
            return AMS_MEL_RESOURCE_EXHAUSTED;
        }
        if (!claim_c2_submission(state, require_enabled ? SubmissionRequirement::Enabled : SubmissionRequirement::AttachedOrEnabled, channel)) {
            diagnostic(require_enabled ? "C2 channel is not enabled" : "C2 channel is not available", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
    } catch (...) {
        diagnostic("C2 submission lock failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    try {
        try {
            input->future = send(*channel);
            arm_return_worker(input);
        } catch (const std::exception& error) {
            channel.reset();
            (void)finish_channel(state);
            const char *what = error.what();
            const std::string_view text = what ? std::string_view{what} : std::string_view{};
            diagnostic(!text.empty() && valid_utf8(text) ? text : "provider send exception", out, capacity, required);
            return AMS_MEL_PROVIDER_EXCEPTION;
        } catch (...) {
            channel.reset();
            (void)finish_channel(state);
            diagnostic("unknown provider send exception", out, capacity, required);
            return AMS_MEL_PROVIDER_EXCEPTION;
        }
        /* Only the accounted graph may own the channel when cleanup runs. */
        channel.reset();
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
        const SubmitFailpoint failpoint = submit_failpoint();
        if (failpoint == SubmitFailpoint::Allocation) throw std::bad_alloc{};
        if (failpoint == SubmitFailpoint::WorkerLaunch)
            throw std::system_error{std::make_error_code(std::errc::resource_unavailable_try_again)};
#endif
        *worker = std::thread{[input] { run_return_worker(input); }};
        worker->detach();
        input->emergency_self.reset();
        input->launch_state.store(1U, std::memory_order_release);
        *out_request = owner.release();
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        retain_return_worker(input);
        diagnostic("facade allocation failed after provider send", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        retain_return_worker(input);
        if (worker && worker->joinable()) (void)worker.release();
        input->launch_state.store(2U, std::memory_order_release);
        diagnostic("facade worker launch failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

template<typename Command>
ams_mel_status_t submit_return_command(
    ams_mel_ir_c2 *c2, Command command,
    ams_mel_ir_return_request **out_request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    return submit_return_operation(c2,
        [command = std::move(command)](irmel::C2Channel& channel) mutable {
            return channel.send(std::move(command));
        }, true, out_request, out, capacity, required);
}

ams_mel_status_t submit_comms_operation(
    ams_mel_ir_c2 *c2, irmel::ChannelCommsTestReq command,
    ams_mel_ir_channel_comms_request **out_request, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    const auto state = c2->state;
    std::shared_ptr<CommsCompletion> completion;
    std::shared_ptr<CommsWorkerInput> input;
    std::unique_ptr<ams_mel_ir_channel_comms_request> owner;
    std::unique_ptr<std::thread> worker;
    try {
        completion = std::make_shared<CommsCompletion>(); completion->channel = state;
        input = std::make_shared<CommsWorkerInput>(); input->completion = completion;
        owner = std::make_unique<ams_mel_ir_channel_comms_request>(); owner->state = completion;
        worker = std::make_unique<std::thread>();
    } catch (...) {
        diagnostic("allocation failed before provider send", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    std::shared_ptr<irmel::C2Channel> channel;
    try {
        if (!acquire_completion_permit(state->session->admission, input->admission)) {
            diagnostic("async request limit reached", out, capacity, required);
            return AMS_MEL_RESOURCE_EXHAUSTED;
        }
        if (!claim_c2_submission(state, SubmissionRequirement::AttachedOrEnabled, channel)) {
            diagnostic("C2 channel is not available", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
    } catch (...) {
        diagnostic("C2 submission lock failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    try {
        try {
            input->future = channel->send(std::move(command));
            arm_comms_worker(input);
        } catch (const std::exception& error) {
            channel.reset();
            (void)finish_channel(state);
            const char *what = error.what();
            const std::string_view text = what ? std::string_view{what} : std::string_view{};
            diagnostic(!text.empty() && valid_utf8(text) ? text : "provider send exception",
                       out, capacity, required);
            return AMS_MEL_PROVIDER_EXCEPTION;
        } catch (...) {
            channel.reset();
            (void)finish_channel(state);
            diagnostic("unknown provider send exception", out, capacity, required);
            return AMS_MEL_PROVIDER_EXCEPTION;
        }
        /* Only the accounted graph may own the channel when cleanup runs. */
        channel.reset();
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
        const SubmitFailpoint failpoint = submit_failpoint();
        if (failpoint == SubmitFailpoint::Allocation) throw std::bad_alloc{};
        if (failpoint == SubmitFailpoint::WorkerLaunch)
            throw std::system_error{std::make_error_code(std::errc::resource_unavailable_try_again)};
#endif
        *worker = std::thread{[input] { run_comms_worker(input); }}; worker->detach();
        input->emergency_self.reset(); input->launch_state.store(1U, std::memory_order_release);
        *out_request = owner.release(); return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        retain_comms_worker(input);
        diagnostic("facade allocation failed after provider send", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        retain_comms_worker(input);
        if (worker && worker->joinable()) (void)worker.release();
        input->launch_state.store(2U, std::memory_order_release);
        diagnostic("facade worker launch failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}
} // namespace

extern "C" ams_mel_status_t ams_mel_ir_c2_open(
    const ams_mel_session *session, const ams_mel_ir_c2_config_v1 *config,
    ams_mel_ir_c2 **out_c2, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!session || !session->state || !config || !out_c2 || *out_c2 ||
        (!out && capacity) ||
        config->channel_type != AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL ||
        !valid_view(config->channel_id.descriptive_label) ||
        !valid_view(config->platform_id.descriptive_label) ||
        !valid_view(config->sensor_location.key) ||
        !valid_view(config->sensor_location.system_name)) {
        diagnostic("invalid C2 configuration", out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    std::shared_ptr<ChannelState> state;
    try {
        bool advertised = false;
        for (const auto& capability : session->state->control->getCapabilities())
            if (has_c2(capability)) { advertised = true; break; }
        if (!advertised) {
            diagnostic("provider does not advertise CommandAndControl", out, capacity, required);
            return AMS_MEL_INITIALIZATION_FAILED;
        }
        state = std::make_shared<ChannelState>();
        state->session = session->state;
        mel::ForeignKey key{copy_view(config->sensor_location.key),
                            copy_view(config->sensor_location.system_name)};
        mel::ComponentLocation location{config->sensor_location.offset_x_m,
            config->sensor_location.offset_y_m, config->sensor_location.offset_z_m,
            key};
        irmel::Config upstream{convert_id(config->channel_id),
            irmel::ChannelType::CommandAndControl, convert_id(config->platform_id),
            std::move(location), {}, false, false};
        state->channel = state->session->control->attachChannel(upstream);
        if (!state->channel) {
            diagnostic("attachChannel returned null", out, capacity, required);
            return AMS_MEL_FACTORY_FAILED;
        }
        state->c2 = std::dynamic_pointer_cast<irmel::C2Channel>(state->channel);
        bool compatible = state->c2 && has_c2(state->channel->getCapabilities());
        if (!compatible) {
            bool detached = false;
            try { detached = state->session->control->detachChannel(state->channel) == irmel::Return::Success; }
            catch (...) {}
            if (!detached) retain_failed(state);
            diagnostic(detached ? "attached channel is not compatible C2" :
                       "incompatible C2 channel and detach failed", out, capacity, required);
            return detached ? AMS_MEL_INITIALIZATION_FAILED : AMS_MEL_PROVIDER_FAILED;
        }
        auto owner = std::make_unique<ams_mel_ir_c2>();
        owner->state = std::move(state);
        *out_c2 = owner.release();
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        if (state && state->channel) {
            bool detached = false;
            try { detached = state->session->control->detachChannel(state->channel) == irmel::Return::Success; }
            catch (...) {}
            if (!detached) retain_failed(state);
        }
        diagnostic("allocation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        if (state && state->channel) {
            bool detached = false;
            try { detached = state->session->control->detachChannel(state->channel) == irmel::Return::Success; }
            catch (...) {}
            if (!detached) {
                retain_failed(state);
                diagnostic("C2 open exception and detach failed", out, capacity, required);
                return AMS_MEL_PROVIDER_FAILED;
            }
        }
        diagnostic("provider exception during C2 open", out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_c2_enable(
    ams_mel_ir_c2 *c2, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!c2 || !c2->state || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::lock_guard lock{c2->state->mutex};
        if (c2->state->lifecycle == C2Lifecycle::Enabled) return AMS_MEL_OK;
        if (c2->state->lifecycle != C2Lifecycle::Attached) {
            diagnostic("C2 channel is not attachable", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        c2->state->enable_attempted = true;
        if (c2->state->channel->enable() != irmel::Return::Success) {
            c2->state->lifecycle = C2Lifecycle::Failed;
            diagnostic("C2 enable failed", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        c2->state->lifecycle = C2Lifecycle::Enabled;
        c2->state->enabled = true;
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        diagnostic("allocation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        c2->state->lifecycle = C2Lifecycle::Failed;
        diagnostic("provider exception during C2 enable", out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_c2_submit_operate(
    ams_mel_ir_c2 *c2, std::uint32_t command_id,
    ams_mel_ir_mode_request **out_request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!c2 || !c2->state || !out_request || *out_request || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    irmel::ModeCmd command;
    command.setCommandID(command_id);
    command.setState(mel::MFA_State::Operate);
    command.setMode(irmel::MFA_Mode::TaskSched);
    return submit_mode_command(c2, std::move(command), out_request, out, capacity, required);
}

extern "C" ams_mel_status_t ams_mel_ir_c2_submit_mode(
    ams_mel_ir_c2 *c2, const ams_mel_ir_mode_command_v1 *input,
    ams_mel_ir_mode_request **out_request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    mel::MFA_State state{};
    irmel::MFA_Mode mode{};
    if (!c2 || !c2->state || !input || !out_request || *out_request ||
        (!out && capacity) || !convert_state(input->state, state) ||
        !convert_mode(input->mode, mode) || !valid_scan(input->scan_parameters)) {
        diagnostic("invalid ModeCmd input", out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    try {
        irmel::ModeCmd command;
        command.setCommandID(input->command_id);
        command.setState(state);
        command.setMode(mode);
        command.setScanParameters(convert_scan(input->scan_parameters));
        return submit_mode_command(c2, std::move(command), out_request, out, capacity, required);
    } catch (...) {
        diagnostic("ModeCmd preparation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_mode_request_wait(
    const ams_mel_ir_mode_request *request, std::uint32_t timeout_ms,
    ams_mel_ir_mode_result_v1 *result, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!request || !request->state || !result || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::unique_lock lock{request->state->mutex};
        if (request->state->kind == CompletionKind::Pending &&
            !request->state->ready.wait_for(lock, std::chrono::milliseconds{timeout_ms},
                [&] { return request->state->kind != CompletionKind::Pending; }))
            return AMS_MEL_TIMEOUT;
        *result = request->state->result;
        diagnostic(request->state->message, out, capacity, required);
        switch (request->state->kind) {
        case CompletionKind::Success: return AMS_MEL_OK;
        case CompletionKind::Rejected: return AMS_MEL_COMMAND_REJECTED;
        case CompletionKind::ProviderException: return AMS_MEL_PROVIDER_EXCEPTION;
        case CompletionKind::ProviderFailure: return AMS_MEL_PROVIDER_FAILED;
        case CompletionKind::InternalError: return AMS_MEL_INTERNAL_ERROR;
        case CompletionKind::Pending: return AMS_MEL_TIMEOUT;
        }
    } catch (...) {
        diagnostic("mode request wait failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    return AMS_MEL_INTERNAL_ERROR;
}

extern "C" ams_mel_status_t ams_mel_ir_mode_request_close(
    ams_mel_ir_mode_request **request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!request || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try { delete std::exchange(*request, nullptr); return AMS_MEL_OK; }
    catch (...) { diagnostic("request close failed", out, capacity, required); return AMS_MEL_INTERNAL_ERROR; }
}

extern "C" ams_mel_status_t ams_mel_ir_c2_submit_bit_noop(
    ams_mel_ir_c2 *c2, std::uint32_t command_id,
    ams_mel_ir_return_request **out_request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!c2 || !c2->state || !out_request || *out_request || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    irmel::BIT_Command command;
    command.setCommandID(command_id);
    return submit_return_command(c2, std::move(command), out_request, out, capacity, required);
}

extern "C" ams_mel_status_t ams_mel_ir_c2_submit_bit(
    ams_mel_ir_c2 *c2, const ams_mel_ir_bit_command_v1 *input,
    ams_mel_ir_return_request **out_request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!c2 || !c2->state || !input || !out_request || *out_request || (!out && capacity) ||
        !valid_span(input->initiate_bit_ids) || !valid_span(input->cancel_bit_ids) ||
        !valid_span(input->clear_fault_codes)) {
        diagnostic("invalid BIT command input", out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    try {
        std::vector<std::uint32_t> initiate;
        std::vector<std::uint32_t> cancel;
        if (input->initiate_bit_ids.size != 0U)
            initiate.assign(input->initiate_bit_ids.data,
                input->initiate_bit_ids.data + input->initiate_bit_ids.size);
        if (input->cancel_bit_ids.size != 0U)
            cancel.assign(input->cancel_bit_ids.data,
                input->cancel_bit_ids.data + input->cancel_bit_ids.size);
        std::vector<std::string> faults;
        faults.reserve(input->clear_fault_codes.size);
        for (std::size_t index = 0; index < input->clear_fault_codes.size; ++index)
            faults.push_back(copy_view(input->clear_fault_codes.data[index]));
        irmel::BIT_Command command{input->command_id, std::move(initiate),
                                   std::move(cancel), std::move(faults)};
        return submit_return_command(c2, std::move(command), out_request, out, capacity, required);
    } catch (...) {
        diagnostic("BIT command preparation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_c2_submit_config_set(
    ams_mel_ir_c2 *c2, const ams_mel_ir_config_set_command_v1 *input,
    ams_mel_ir_return_request **out_request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!c2 || !c2->state || !input || !out_request || *out_request || (!out && capacity) ||
        !valid_view(input->config)) {
        diagnostic("invalid ConfigSet command input", out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    try {
        irmel::ConfigSetCommand command;
        command.setCommandID(input->command_id);
        command.setSystemTime(std::chrono::nanoseconds{input->system_time_ns});
        command.setConfig(copy_view(input->config));
        return submit_return_command(c2, std::move(command), out_request, out, capacity, required);
    } catch (...) {
        diagnostic("ConfigSet command preparation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_c2_send_keepalive(
    ams_mel_ir_c2 *c2, ams_mel_ir_return_request **out_request, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!c2 || !c2->state || !out_request || *out_request || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    return submit_return_operation(c2,
        [](irmel::C2Channel& channel) { return channel.sendKeepAliveRep(); },
        false, out_request, out, capacity, required);
}

extern "C" ams_mel_status_t ams_mel_ir_c2_submit_comms_test(
    ams_mel_ir_c2 *c2, const ams_mel_ir_channel_comms_test_request_v1 *request,
    ams_mel_ir_channel_comms_request **out_request, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!c2 || !c2->state || !request || !out_request || *out_request || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        irmel::ChannelCommsTestReq command;
        command.setCommandID(request->command_id);
        command.setChannelID(request->channel_id);
        command.setRequestID(request->request_id);
        return submit_comms_operation(c2, std::move(command), out_request,
                                      out, capacity, required);
    } catch (...) {
        diagnostic("CommsTest command preparation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_channel_comms_request_wait(
    const ams_mel_ir_channel_comms_request *request, std::uint32_t timeout_ms,
    ams_mel_ir_channel_comms_test_result_v1 *result, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!request || !request->state || !result || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::unique_lock lock{request->state->mutex};
        if (request->state->kind == CompletionKind::Pending &&
            !request->state->ready.wait_for(lock, std::chrono::milliseconds{timeout_ms},
                [&] { return request->state->kind != CompletionKind::Pending; }))
            return AMS_MEL_TIMEOUT;
        *result = request->state->result;
        diagnostic(request->state->message, out, capacity, required);
        switch (request->state->kind) {
        case CompletionKind::Success: return AMS_MEL_OK;
        case CompletionKind::Rejected: return AMS_MEL_COMMAND_REJECTED;
        case CompletionKind::ProviderException: return AMS_MEL_PROVIDER_EXCEPTION;
        case CompletionKind::ProviderFailure: return AMS_MEL_PROVIDER_FAILED;
        case CompletionKind::InternalError: return AMS_MEL_INTERNAL_ERROR;
        case CompletionKind::Pending: return AMS_MEL_TIMEOUT;
        }
    } catch (...) {
        diagnostic("CommsTest request wait failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    return AMS_MEL_INTERNAL_ERROR;
}

extern "C" ams_mel_status_t ams_mel_ir_channel_comms_request_close(
    ams_mel_ir_channel_comms_request **request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!request || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try { delete std::exchange(*request, nullptr); return AMS_MEL_OK; }
    catch (...) { return AMS_MEL_INTERNAL_ERROR; }
}

extern "C" ams_mel_status_t ams_mel_ir_c2_get_capabilities(
    ams_mel_ir_c2 *c2, ams_mel_ir_channel_capability **output, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!c2 || !c2->state || !output || *output || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::lock_guard lock{c2->state->mutex};
        if (c2->state->lifecycle != C2Lifecycle::Attached &&
            c2->state->lifecycle != C2Lifecycle::Enabled) {
            diagnostic("C2 channel is not available", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        return ams_mel::internal::snapshot_capability(
            *c2->state->c2, output, out, capacity, required);
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

extern "C" ams_mel_status_t ams_mel_ir_c2_metadata_open(
    ams_mel_ir_c2 *c2, std::size_t queue_capacity, ams_mel_ir_c2_metadata **output,
    char *out, std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("",out,capacity,required);
    if(!c2||!c2->state||!queue_capacity||!output||*output||(!out&&capacity))return AMS_MEL_INVALID_ARGUMENT;
    std::shared_ptr<MetadataState> state;
    try {
        std::lock_guard channel_lock{c2->state->mutex};
        if(c2->state->metadata_attempted||c2->state->lifecycle==C2Lifecycle::Closed)return AMS_MEL_INVALID_ARGUMENT;
        c2->state->metadata_attempted=true; state=std::make_shared<MetadataState>(); state->capacity=queue_capacity; c2->state->metadata=state;
        auto first=c2->state->c2->registerMetadataCallback(std::function<void(irmel::Channel&,const mel::BIT_Configuration*const)>{[state](irmel::Channel&,const mel::BIT_Configuration* value) noexcept {metadata_callback(state,[&]{return copy_bit_configuration(value);});}});
        if(first!=irmel::Return::Success)throw std::runtime_error("BIT_Configuration callback registration failed");
        auto second=c2->state->c2->registerMetadataCallback(std::function<void(irmel::Channel&,const irmel::CommandStatus*const)>{[state](irmel::Channel&,const irmel::CommandStatus* value) noexcept {metadata_callback(state,[&]{return copy_command_status(value);});}});
        if(second!=irmel::Return::Success)throw std::runtime_error("CommandStatus callback registration failed");
        auto third=c2->state->c2->registerMetadataCallback(std::function<void(irmel::Channel&,const mel::BIT_Status*const)>{[state](irmel::Channel&,const mel::BIT_Status* value) noexcept {metadata_callback(state,[&]{return copy_bit_status(value);});}});
        if(third!=irmel::Return::Success)throw std::runtime_error("BIT_Status callback registration failed");
        auto owner=std::make_unique<ams_mel_ir_c2_metadata>();owner->state=state;
        owner->channel=c2->state;*output=owner.release();return AMS_MEL_OK;
    } catch(const std::bad_alloc&){if(state){std::lock_guard lock{state->mutex};state->lifecycle=MetadataLifecycle::Failed;state->ready.notify_all();}diagnostic("metadata allocation failed",out,capacity,required);return AMS_MEL_INTERNAL_ERROR;}
      catch(const std::exception& error){if(state){std::lock_guard lock{state->mutex};state->lifecycle=MetadataLifecycle::Inactive;state->ready.notify_all();}diagnostic(error.what(),out,capacity,required);return AMS_MEL_PROVIDER_FAILED;}
      catch(...){if(state){std::lock_guard lock{state->mutex};state->lifecycle=MetadataLifecycle::Inactive;state->ready.notify_all();}diagnostic("metadata registration failed",out,capacity,required);return AMS_MEL_PROVIDER_EXCEPTION;}
}

extern "C" ams_mel_status_t ams_mel_ir_c2_metadata_register_comms_test(
    ams_mel_ir_c2_metadata *metadata, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!metadata || !metadata->state || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    auto channel = metadata->channel.lock();
    if (!channel) return AMS_MEL_PROVIDER_FAILED;
    try {
        std::lock_guard lock{channel->mutex};
        if (channel->comms_metadata_attempted) {
            diagnostic("CommsTest metadata registration already attempted", out, capacity, required);
            return AMS_MEL_INVALID_ARGUMENT;
        }
        if (channel->lifecycle != C2Lifecycle::Attached &&
            channel->lifecycle != C2Lifecycle::Enabled) return AMS_MEL_PROVIDER_FAILED;
        channel->comms_metadata_attempted = true;
        auto state = metadata->state;
        const auto result = channel->c2->registerMetadataCallback(
            std::function<void(irmel::Channel&, const irmel::ChannelCommsTestRep *const)>{
                [state](irmel::Channel&, const irmel::ChannelCommsTestRep *value) noexcept {
                    metadata_callback(state, [&] { return copy_comms_test(value); });
                }});
        if (result != irmel::Return::Success) {
            diagnostic("ChannelCommsTest callback registration failed", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        return AMS_MEL_OK;
    } catch (const std::exception& error) {
        const char *what = error.what();
        const std::string_view text = what ? std::string_view{what} : std::string_view{};
        diagnostic(!text.empty() && valid_utf8(text) ? text : "metadata registration exception",
                   out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    } catch (...) {
        diagnostic("unknown metadata registration exception", out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_c2_metadata_receive(ams_mel_ir_c2_metadata *metadata,std::uint32_t timeout_ms,ams_mel_ir_c2_metadata_event **output,char *out,std::size_t capacity,std::size_t *required) noexcept
{
    diagnostic("",out,capacity,required);if(!metadata||!metadata->state||!output||*output||(!out&&capacity))return AMS_MEL_INVALID_ARGUMENT;
    try{std::unique_lock lock{metadata->state->mutex};if(metadata->state->queue.empty()&&metadata->state->lifecycle==MetadataLifecycle::Active&&!metadata->state->ready.wait_for(lock,std::chrono::milliseconds{timeout_ms},[&]{return !metadata->state->queue.empty()||metadata->state->lifecycle!=MetadataLifecycle::Active;}))return AMS_MEL_TIMEOUT;
        if(!metadata->state->queue.empty()){auto owner=std::make_unique<ams_mel_ir_c2_metadata_event>();owner->data=std::move(metadata->state->queue.front());metadata->state->queue.pop_front();*output=owner.release();return AMS_MEL_OK;}
        return metadata->state->lifecycle==MetadataLifecycle::Failed?AMS_MEL_PROVIDER_FAILED:(metadata->state->lifecycle==MetadataLifecycle::Active?AMS_MEL_TIMEOUT:AMS_MEL_STREAM_STOPPED);
    }catch(...){diagnostic("metadata receive failed",out,capacity,required);return AMS_MEL_INTERNAL_ERROR;}
}
extern "C" ams_mel_status_t ams_mel_ir_c2_metadata_get_counters(const ams_mel_ir_c2_metadata *metadata,ams_mel_ir_c2_metadata_counters_v1 *output,char *out,std::size_t capacity,std::size_t *required) noexcept
{diagnostic("",out,capacity,required);if(!metadata||!metadata->state||!output||(!out&&capacity))return AMS_MEL_INVALID_ARGUMENT;try{std::lock_guard lock{metadata->state->mutex};*output=metadata->state->counters;return AMS_MEL_OK;}catch(...){return AMS_MEL_INTERNAL_ERROR;}}
extern "C" ams_mel_status_t ams_mel_ir_c2_metadata_close(ams_mel_ir_c2_metadata **metadata,char *out,std::size_t capacity,std::size_t *required) noexcept
{diagnostic("",out,capacity,required);if(!metadata||(!out&&capacity))return AMS_MEL_INVALID_ARGUMENT;try{auto owner=std::exchange(*metadata,nullptr);if(owner){std::lock_guard lock{owner->state->mutex};if(owner->state->lifecycle==MetadataLifecycle::Active)owner->state->lifecycle=MetadataLifecycle::Inactive;owner->state->ready.notify_all();delete owner;}return AMS_MEL_OK;}catch(...){return AMS_MEL_INTERNAL_ERROR;}}
extern "C" ams_mel_status_t ams_mel_ir_c2_metadata_event_view(const ams_mel_ir_c2_metadata_event *event,const ams_mel_ir_c2_metadata_event_v1 **output,char *out,std::size_t capacity,std::size_t *required) noexcept
{diagnostic("",out,capacity,required);if(!event||!event->data||!output||(!out&&capacity))return AMS_MEL_INVALID_ARGUMENT;*output=&event->data->view;return AMS_MEL_OK;}
extern "C" ams_mel_status_t ams_mel_ir_c2_metadata_event_close(ams_mel_ir_c2_metadata_event **event,char *out,std::size_t capacity,std::size_t *required) noexcept
{diagnostic("",out,capacity,required);if(!event||(!out&&capacity))return AMS_MEL_INVALID_ARGUMENT;try{delete std::exchange(*event,nullptr);return AMS_MEL_OK;}catch(...){return AMS_MEL_INTERNAL_ERROR;}}

extern "C" ams_mel_status_t ams_mel_ir_return_request_wait(
    const ams_mel_ir_return_request *request, std::uint32_t timeout_ms,
    ams_mel_ir_return_result_v1 *result, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!request || !request->state || !result || (!out && capacity))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::unique_lock lock{request->state->mutex};
        if (request->state->kind == CompletionKind::Pending &&
            !request->state->ready.wait_for(lock, std::chrono::milliseconds{timeout_ms},
                [&] { return request->state->kind != CompletionKind::Pending; }))
            return AMS_MEL_TIMEOUT;
        *result = request->state->result;
        diagnostic(request->state->message, out, capacity, required);
        switch (request->state->kind) {
        case CompletionKind::Success: return AMS_MEL_OK;
        case CompletionKind::Rejected: return AMS_MEL_COMMAND_REJECTED;
        case CompletionKind::ProviderException: return AMS_MEL_PROVIDER_EXCEPTION;
        case CompletionKind::ProviderFailure: return AMS_MEL_PROVIDER_FAILED;
        case CompletionKind::InternalError: return AMS_MEL_INTERNAL_ERROR;
        case CompletionKind::Pending: return AMS_MEL_TIMEOUT;
        }
    } catch (...) {
        diagnostic("return request wait failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    return AMS_MEL_INTERNAL_ERROR;
}

extern "C" ams_mel_status_t ams_mel_ir_return_request_close(
    ams_mel_ir_return_request **request, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!request || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    try { delete std::exchange(*request, nullptr); return AMS_MEL_OK; }
    catch (...) { diagnostic("request close failed", out, capacity, required); return AMS_MEL_INTERNAL_ERROR; }
}

extern "C" ams_mel_status_t ams_mel_ir_c2_close(
    ams_mel_ir_c2 **c2, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!c2 || (!out && capacity)) return AMS_MEL_INVALID_ARGUMENT;
    ams_mel_ir_c2 *owner = *c2;
    if (!owner) return AMS_MEL_OK;
    try {
        auto state = owner->state;
        bool now = false;
        {
            std::lock_guard lock{state->mutex};
            state->lifecycle = C2Lifecycle::Closed;
            now = state->requests == 0U;
        }
        bool ok = !now || cleanup(state, false);
        if (now && state->channel) {
            diagnostic("C2 detach failed; provider state retained", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        *c2 = nullptr;
        delete owner;
        if (!ok) {
            diagnostic("C2 disable failed", out, capacity, required);
            return AMS_MEL_PROVIDER_FAILED;
        }
        return AMS_MEL_OK;
    } catch (...) {
        diagnostic("C2 close failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}
