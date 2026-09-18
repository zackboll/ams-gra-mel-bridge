#include <ams_mel/abi.h>
#include "internal.hpp"
#include "internal/ir_channel.hpp"

#include <irmel/library/health-status/HealthStatusChannel.h>

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
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>
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
        std::size_t trailing{}; std::uint32_t point{};
        if (lead >= 0xc2U && lead <= 0xdfU) { trailing=1U; point=lead&0x1fU; }
        else if (lead >= 0xe0U && lead <= 0xefU) { trailing=2U; point=lead&0x0fU; }
        else if (lead >= 0xf0U && lead <= 0xf4U) { trailing=3U; point=lead&0x07U; }
        else return false;
        if (index + trailing >= value.size()) return false;
        for (std::size_t offset=1; offset<=trailing; ++offset) {
            const auto byte=static_cast<unsigned char>(value[index+offset]);
            if ((byte&0xc0U)!=0x80U) return false;
            point=(point<<6U)|(byte&0x3fU);
        }
        if ((trailing==2U&&point<0x800U)||(trailing==3U&&point<0x10000U)||
            point>0x10ffffU||(point>=0xd800U&&point<=0xdfffU)) return false;
        index += trailing + 1U;
    }
    return true;
}

void diagnostic(std::string_view text, char *out, std::size_t capacity,
                std::size_t *required) noexcept
{
    if (required) *required=text.size()+1U;
    if (out&&capacity) {
        std::size_t copied=std::min(text.size(),capacity-1U);
        while (copied&&!valid_utf8(text.substr(0U,copied))) --copied;
        std::memcpy(out,text.data(),copied); out[copied]='\0';
    }
}

bool valid_view(const ams_mel_string_view_v1& value) noexcept
{ return value.data ? valid_utf8({value.data,value.size}) : value.size==0U; }
std::string copy_view(const ams_mel_string_view_v1& value)
{ return value.size ? std::string{value.data,value.size} : std::string{}; }
mel::UCI_ID convert_id(const ams_mel_uci_id_v1& value)
{
    std::array<std::uint8_t,mel::UUID_SIZE> uuid{};
    std::copy(std::begin(value.uuid),std::end(value.uuid),uuid.begin());
    return {uuid,copy_view(value.descriptive_label)};
}
void increment(std::uint64_t& value) noexcept { if(value!=UINT64_MAX)++value; }

struct OwnedID { std::array<std::uint8_t,16> uuid{}; std::string label; };
struct OwnedLocation { double x{},y{},z{}; std::string key,system; };
struct OwnedComponent {
    OwnedID id; std::uint32_t state{},temperature_state{}; double temperature{};
    std::string installation_key,installation_system;
    OwnedLocation location; double orientation[3]{},boresight[3]{};
};
struct OwnedBitActive { OwnedID id; std::int64_t completion{}; double percent{}; };
struct OwnedBitItem { std::string name; std::uint32_t result{}; std::string reason; };
struct OwnedBitCompleted { OwnedID id; std::int64_t time{}; std::uint32_t result{}; std::string reason; std::vector<OwnedBitItem> items; };
struct OwnedFaultData { std::string key,value,format,units; };
struct OwnedAmbiguity { std::vector<OwnedID> tests,components; };
struct OwnedFault { OwnedID id; std::uint32_t severity{},state{}; std::vector<OwnedFaultData> data; std::int64_t time{}; std::string code,description; std::vector<OwnedID> components; std::vector<OwnedAmbiguity> groups; };
struct OwnedCSCI { std::string csci; std::uint32_t mode{}; ams_mel_ir_version_v1 version{}; std::uint32_t criticality{},failure{},bit_report{},connected{}; };
struct OwnedPair { std::string name,value; };
struct OwnedArtifact { OwnedID component,associated; };
struct OwnedSecurityEvent { std::uint32_t kind{},category{}; std::string details; OwnedID subsystem,service,mdf; };

struct EventData {
    ams_mel_ir_health_metadata_event_v1 view{};
    std::string state_description,mode_description;
    std::string model,serial,software,bootloader,hardware;
    std::vector<OwnedComponent> components;
    std::vector<ams_mel_mfa_component_v1> component_views;
    std::vector<OwnedBitActive> active;
    std::vector<OwnedBitCompleted> completed;
    std::vector<OwnedFault> faults;
    std::vector<ams_mel_active_bit_v1> active_views;
    std::vector<ams_mel_completed_bit_v1> completed_views;
    std::vector<std::vector<ams_mel_completed_bit_item_v1>> item_views;
    std::vector<ams_mel_fault_v1> fault_views;
    std::vector<std::vector<ams_mel_fault_data_v1>> fault_data_views;
    std::vector<std::vector<ams_mel_uci_id_v1>> fault_component_views;
    std::vector<std::vector<ams_mel_fault_ambiguity_group_v1>> group_views;
    std::vector<std::vector<std::vector<ams_mel_uci_id_v1>>> group_test_views,group_component_views;
    std::vector<ams_mel_ir_subsystem_dep_info_v1> subsystems;
    std::vector<OwnedCSCI> csci;
    std::vector<ams_mel_ir_subsystem_csci_info_v1> csci_views;
    std::vector<OwnedPair> pairs;
    std::vector<ams_mel_name_value_pair_v1> pair_views;
    OwnedID security_event_id,security_subsystem_id;
    std::vector<OwnedArtifact> artifacts;
    std::vector<ams_mel_security_artifact_v1> artifact_views;
    OwnedSecurityEvent security_event;
};

ams_mel_string_view_v1 string_view(const std::string& value) noexcept
{ return {value.empty()?nullptr:value.data(),value.size()}; }
ams_mel_uci_id_v1 id_view(const OwnedID& id) noexcept
{
    ams_mel_uci_id_v1 result{}; std::copy(id.uuid.begin(),id.uuid.end(),result.uuid);
    result.descriptive_label=string_view(id.label); return result;
}
bool copy_id(const mel::UCI_ID& input,OwnedID& output)
{ if(!valid_utf8(input.getDescriptiveLabel()))return false; output.uuid=input.getUUID();output.label=input.getDescriptiveLabel();return true; }
bool copy_location(const mel::ComponentLocation& input,OwnedLocation& output)
{
    output={input.getOffsetX(),input.getOffsetY(),input.getOffsetZ(),
        input.getLocationId().getKey(),input.getLocationId().getSystemName()};
    return valid_utf8(output.key)&&valid_utf8(output.system);
}
ams_mel_component_location_v1 location_view(const OwnedLocation& value) noexcept
{ return {value.x,value.y,value.z,string_view(value.key),string_view(value.system)}; }
bool valid_result(mel::BIT_Result value) noexcept
{ return static_cast<std::uint32_t>(value)<static_cast<std::uint32_t>(mel::BIT_Result::MaxExclusive); }

void build_bit_views(EventData& event)
{
    event.active_views.resize(event.active.size());
    for(std::size_t i=0;i<event.active.size();++i)event.active_views[i]={id_view(event.active[i].id),event.active[i].completion,event.active[i].percent};
    event.completed_views.resize(event.completed.size());event.item_views.resize(event.completed.size());
    for(std::size_t i=0;i<event.completed.size();++i){auto& items=event.item_views[i];for(const auto& item:event.completed[i].items)items.push_back({string_view(item.name),item.result,string_view(item.reason)});const auto& source=event.completed[i];event.completed_views[i]={id_view(source.id),source.time,source.result,string_view(source.reason),{items.data(),items.size()}};}
    event.fault_views.resize(event.faults.size());event.fault_data_views.resize(event.faults.size());event.fault_component_views.resize(event.faults.size());event.group_views.resize(event.faults.size());event.group_test_views.resize(event.faults.size());event.group_component_views.resize(event.faults.size());
    for(std::size_t i=0;i<event.faults.size();++i){const auto& source=event.faults[i];auto& data=event.fault_data_views[i];for(const auto& value:source.data)data.push_back({string_view(value.key),string_view(value.value),string_view(value.format),string_view(value.units)});auto& components=event.fault_component_views[i];for(const auto& value:source.components)components.push_back(id_view(value));event.group_test_views[i].resize(source.groups.size());event.group_component_views[i].resize(source.groups.size());auto& groups=event.group_views[i];for(std::size_t j=0;j<source.groups.size();++j){for(const auto& value:source.groups[j].tests)event.group_test_views[i][j].push_back(id_view(value));for(const auto& value:source.groups[j].components)event.group_component_views[i][j].push_back(id_view(value));groups.push_back({{event.group_test_views[i][j].data(),event.group_test_views[i][j].size()},{event.group_component_views[i][j].data(),event.group_component_views[i][j].size()}});}event.fault_views[i]={id_view(source.id),source.severity,source.state,{data.data(),data.size()},source.time,string_view(source.code),string_view(source.description),{components.data(),components.size()},{groups.data(),groups.size()}};}
    event.view.bit_status={{event.active_views.data(),event.active_views.size()},{event.completed_views.data(),event.completed_views.size()},{event.fault_views.data(),event.fault_views.size()}};
}

bool copy_bit(const mel::BIT_Status& input,EventData& event)
{
    for(const auto& source:input.getActiveBITs()){OwnedBitActive value;if(!copy_id(source.getBitID(),value.id))return false;value.completion=source.getEstimatedCompletionTime().count();value.percent=source.getEstimatedPercentComplete();event.active.push_back(std::move(value));}
    for(const auto& source:input.getCompletedBITs()){if(!valid_result(source.getResult())||!valid_utf8(source.getFailReason()))return false;OwnedBitCompleted value;if(!copy_id(source.getBitID(),value.id))return false;value.time=source.getTimeTag().count();value.result=static_cast<std::uint32_t>(source.getResult());value.reason=source.getFailReason();for(const auto& item:source.getBitItem()){if(!valid_result(item.getResult())||!valid_utf8(item.getBitItemName())||!valid_utf8(item.getFailReason()))return false;value.items.push_back({item.getBitItemName(),static_cast<std::uint32_t>(item.getResult()),item.getFailReason()});}event.completed.push_back(std::move(value));}
    for(const auto& source:input.getFaults()){const auto severity=static_cast<std::uint32_t>(source.getSeverity()),state=static_cast<std::uint32_t>(source.getState());if(severity>=static_cast<std::uint32_t>(mel::FaultSeverity::MaxExclusive)||state>=static_cast<std::uint32_t>(mel::FaultState::MaxExclusive)||!valid_utf8(source.getFaultCode())||!valid_utf8(source.getFaultDescription()))return false;OwnedFault value;if(!copy_id(source.getFaultID(),value.id))return false;value.severity=severity;value.state=state;value.time=source.getDetectionTime().count();value.code=source.getFaultCode();value.description=source.getFaultDescription();for(const auto& data:source.getFaultData()){if(!valid_utf8(data.getKey())||!valid_utf8(data.getValue())||!valid_utf8(data.getFormat())||!valid_utf8(data.getUnits()))return false;value.data.push_back({data.getKey(),data.getValue(),data.getFormat(),data.getUnits()});}for(const auto& id:source.getComponentID()){OwnedID copied;if(!copy_id(id,copied))return false;value.components.push_back(std::move(copied));}for(const auto& group:source.getAmbiguityGroup()){OwnedAmbiguity copied;for(const auto& id:group.getDiagnosticTestID()){OwnedID item;if(!copy_id(id,item))return false;copied.tests.push_back(std::move(item));}for(const auto& id:group.getComponentID()){OwnedID item;if(!copy_id(id,item))return false;copied.components.push_back(std::move(item));}value.groups.push_back(std::move(copied));}event.faults.push_back(std::move(value));}
    build_bit_views(event);return true;
}

std::unique_ptr<EventData> copy_mfa_status(const mel::MFA_Status *input)
{
    if (!input) return {};
    const auto state=static_cast<std::uint32_t>(input->getMFAState());
    const auto transition=static_cast<std::uint32_t>(input->getStateTransitionStatus());
    if(state>=AMS_MEL_IR_MFA_STATE_MAX_EXCLUSIVE||transition>AMS_MEL_STATE_TRANSITION_TRANSITIONING||!valid_utf8(input->getMFAStateDescription())||!valid_utf8(input->getMFAModeDescription()))return {};
    const auto& about=input->getAbout();if(!valid_utf8(about.getModel())||!valid_utf8(about.getSerialNumber())||!valid_utf8(about.getSoftwareVersion())||!valid_utf8(about.getBootloaderSoftwareVersion())||!valid_utf8(about.getHardwareVersion()))return {};
    auto event=std::make_unique<EventData>();event->view.kind=AMS_MEL_IR_HEALTH_METADATA_MFA_STATUS;event->state_description=input->getMFAStateDescription();event->mode_description=input->getMFAModeDescription();event->model=about.getModel();event->serial=about.getSerialNumber();event->software=about.getSoftwareVersion();event->bootloader=about.getBootloaderSoftwareVersion();event->hardware=about.getHardwareVersion();
    for(const auto& source:input->getMFAComponents()){const auto component_state=static_cast<std::uint32_t>(source.getComponentState()),temperature_state=static_cast<std::uint32_t>(source.getTemperature().getTemperatureState());if(component_state>AMS_MEL_COMPONENT_STATE_FAULTED||temperature_state>AMS_MEL_TEMPERATURE_STATE_OVER_TEMP_SHUTDOWN)return {};OwnedComponent value;if(!copy_id(source.getComponentId(),value.id))return {};value.state=component_state;value.temperature=source.getTemperature().getTemperature();value.temperature_state=temperature_state;value.installation_key=source.getInstallationLocationId().getKey();value.installation_system=source.getInstallationLocationId().getSystemName();if(!valid_utf8(value.installation_key)||!valid_utf8(value.installation_system)||!copy_location(source.getInstallationDetails().getLocation(),value.location))return {};const auto& orientation=source.getInstallationDetails().getOrientation();const auto& boresight=source.getInstallationDetails().getBoresight();value.orientation[0]=orientation.getRoll();value.orientation[1]=orientation.getPitch();value.orientation[2]=orientation.getYaw();value.boresight[0]=boresight.getRoll();value.boresight[1]=boresight.getPitch();value.boresight[2]=boresight.getYaw();event->components.push_back(std::move(value));}
    for(const auto& value:event->components)event->component_views.push_back({id_view(value.id),value.state,{value.temperature,value.temperature_state},{string_view(value.installation_key),string_view(value.installation_system)},{location_view(value.location),{value.orientation[0],value.orientation[1],value.orientation[2]},{value.boresight[0],value.boresight[1],value.boresight[2]}}});
    event->view.mfa_status={state,string_view(event->state_description),string_view(event->mode_description),transition,{string_view(event->model),string_view(event->serial),string_view(event->software),string_view(event->bootloader),string_view(event->hardware)},{event->component_views.data(),event->component_views.size()}};return event;
}

std::unique_ptr<EventData> copy_bit_status(const mel::BIT_Status *input)
{ if(!input)return {};auto event=std::make_unique<EventData>();event->view.kind=AMS_MEL_IR_HEALTH_METADATA_BIT_STATUS;return copy_bit(*input,*event)?std::move(event):nullptr; }

std::unique_ptr<EventData> copy_subsystem(const irmel::SubsystemStatusResp *input)
{
    if (!input) return {};
    const auto failure=static_cast<std::uint32_t>(input->getFailureLevel());
    if(failure>AMS_MEL_IR_FAILURE_NOT_PRESENT)return {};
    auto event=std::make_unique<EventData>();event->view.kind=AMS_MEL_IR_HEALTH_METADATA_SUBSYSTEM_STATUS;
    for(const auto& source:input->getSubsystems()){const auto value=static_cast<std::uint32_t>(source.getFailureLevel());if(value>AMS_MEL_IR_FAILURE_NOT_PRESENT)return {};event->subsystems.push_back({source.getSubsystemId(),source.getCriticality(),value});}
    for(const auto& source:input->getCSCI()){const auto mode=static_cast<std::uint32_t>(source.getMode()),value=static_cast<std::uint32_t>(source.getFailureLevel());if(mode>AMS_MEL_IR_CSCI_MODE_NOISE||value>AMS_MEL_IR_FAILURE_NOT_PRESENT||!valid_utf8(source.getCsci()))return {};const auto version=source.getVersion();event->csci.push_back({source.getCsci(),mode,{version.getSource(),version.getMajorRevision(),version.getMinorRevision(),version.getEngineeringRevision()},source.getCriticality(),value,source.getBIT_report(),source.getConnectionEstablished()?1U:0U});}
    for(const auto& value:event->csci)
        event->csci_views.push_back({string_view(value.csci),value.mode,value.version,value.criticality,value.failure,value.bit_report,value.connected});
    event->view.subsystem_status={input->getSubsystemId(),input->getCriticality(),input->getStatusSeqNum(),failure,input->getSubsystemCount(),{event->subsystems.data(),event->subsystems.size()},input->getCsciCount(),{event->csci_views.data(),event->csci_views.size()}};
    return event;
}

template<class T> std::unique_ptr<EventData> copy_pairs(const T *input,std::uint32_t kind)
{
    if (!input) return {};
    auto event=std::make_unique<EventData>();event->view.kind=kind;
    for(const auto& source:input->getStatus()){if(!valid_utf8(source.getName())||!valid_utf8(source.getValue()))return {};event->pairs.push_back({source.getName(),source.getValue()});}
    for(const auto& value:event->pairs)event->pair_views.push_back({string_view(value.name),string_view(value.value)});
    const ams_mel_name_value_pair_span_v1 span{event->pair_views.data(),event->pair_views.size()};
    if(kind==AMS_MEL_IR_HEALTH_METADATA_DISCRETE_STATUS)event->view.discrete_status=span;else event->view.mfa_status_detailed=span;
    return event;
}

template<class T> bool copy_security_common(const T& source,OwnedSecurityEvent& output,std::uint32_t kind,std::uint32_t max_category)
{
    const auto category=static_cast<std::uint32_t>(source.getCategory());if(category>=max_category||!valid_utf8(source.getDetails()))return false;output.kind=kind;output.category=category;output.details=source.getDetails();return true;
}

std::unique_ptr<EventData> copy_security(const mel::MFA_SecurityAuditRecord *input)
{
    if (!input) return {};
    const auto outcome=static_cast<std::uint32_t>(input->getOutcome());
    const auto severity=static_cast<std::uint32_t>(input->getSeverity());
    if(outcome>AMS_MEL_SECURITY_OUTCOME_SUCCESS||severity>AMS_MEL_SECURITY_SEVERITY_WARNING)return {};
    auto event=std::make_unique<EventData>();event->view.kind=AMS_MEL_IR_HEALTH_METADATA_SECURITY_AUDIT;if(!copy_id(input->getSecurityEventID(),event->security_event_id)||!copy_id(input->getSubsystemID(),event->security_subsystem_id))return {};
    for(const auto& source:input->getSecurityArtifacts()){OwnedArtifact value;if(!copy_id(source.getComponentID(),value.component)||!copy_id(source.getAssociatedID(),value.associated))return {};event->artifacts.push_back(std::move(value));}
    bool valid=std::visit([&](const auto& source)->bool{using T=std::decay_t<decltype(source)>;if constexpr(std::is_same_v<T,std::monostate>){event->security_event.kind=AMS_MEL_SECURITY_EVENT_NONE;return true;}else if constexpr(std::is_same_v<T,mel::SecurityAuthenticationType>){if(!copy_security_common(source,event->security_event,AMS_MEL_SECURITY_EVENT_AUTHENTICATION,8U))return false;return copy_id(source.getSubsystemID(),event->security_event.subsystem)&&copy_id(source.getServiceID(),event->security_event.service)&&copy_id(source.getMDF_ID(),event->security_event.mdf);}else if constexpr(std::is_same_v<T,mel::SecurityIntegrityType>){if(!copy_security_common(source,event->security_event,AMS_MEL_SECURITY_EVENT_INTEGRITY,7U))return false;return copy_id(source.getMDF_ID(),event->security_event.mdf);}else if constexpr(std::is_same_v<T,mel::SecurityFileManagementType>){if(!copy_security_common(source,event->security_event,AMS_MEL_SECURITY_EVENT_FILE_MANAGEMENT,5U))return false;return copy_id(source.getMDF_ID(),event->security_event.mdf);}else if constexpr(std::is_same_v<T,mel::SecurityKeyManagementType>)return copy_security_common(source,event->security_event,AMS_MEL_SECURITY_EVENT_KEY_MANAGEMENT,7U);else if constexpr(std::is_same_v<T,mel::SecuritySystemType>)return copy_security_common(source,event->security_event,AMS_MEL_SECURITY_EVENT_SYSTEM,5U);else return copy_security_common(source,event->security_event,AMS_MEL_SECURITY_EVENT_SANITIZATION,4U);},input->getEventType());if(!valid)return {};
    for(const auto& value:event->artifacts)
        event->artifact_views.push_back({id_view(value.component),id_view(value.associated)});
    event->view.security_audit={id_view(event->security_event_id),input->getEventTimestamp().count(),id_view(event->security_subsystem_id),{event->artifact_views.data(),event->artifact_views.size()},{event->security_event.kind,event->security_event.category,string_view(event->security_event.details),id_view(event->security_event.subsystem),id_view(event->security_event.service),id_view(event->security_event.mdf)},outcome,severity};
    return event;
}

enum class MetadataLifecycle { Active,Inactive,Stopped,Failed };
struct MetadataState {
    std::mutex mutex;std::condition_variable ready,callbacks_done;std::deque<std::unique_ptr<EventData>> queue;std::size_t capacity{};ams_mel_ir_metadata_counters_v1 counters{};MetadataLifecycle lifecycle{MetadataLifecycle::Active};std::atomic<std::size_t> callbacks{};
    void fail() noexcept {try{std::lock_guard lock{mutex};lifecycle=MetadataLifecycle::Failed;ready.notify_all();}catch(...){}}
};
struct CallbackGuard {std::shared_ptr<MetadataState> state;explicit CallbackGuard(std::shared_ptr<MetadataState> value)noexcept:state(std::move(value)){state->callbacks.fetch_add(1,std::memory_order_acq_rel);}~CallbackGuard()noexcept{if(state->callbacks.fetch_sub(1,std::memory_order_acq_rel)==1)state->callbacks_done.notify_all();}};
template<class Build> void callback(const std::shared_ptr<MetadataState>& state,Build build) noexcept
{
    CallbackGuard guard{state};try{
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
        if(const char* base=std::getenv("AMS_MEL_TEST_HEALTH_CALLBACK_BARRIER")) {
            if(const char* log=std::getenv("AMS_MEL_TEST_LIFETIME_LOG"))std::ofstream{log,std::ios::app}<<"health_callback_entered\n";
            std::ofstream{std::string{base}+".entered"}<<"entered\n";
            while(!std::ifstream{std::string{base}+".release"}.good())std::this_thread::yield();
        }
#endif
        {std::lock_guard lock{state->mutex};increment(state->counters.events_received);if(state->lifecycle!=MetadataLifecycle::Active)return;}
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
        if(const char* value=std::getenv("AMS_MEL_TEST_HEALTH_CALLBACK_FAILURE");value&&std::strcmp(value,"allocation")==0)throw std::bad_alloc{};
#endif
        auto event=build();std::lock_guard lock{state->mutex};if(state->lifecycle!=MetadataLifecycle::Active)return;if(!event){increment(state->counters.malformed_or_unsupported);return;}if(state->queue.size()>=state->capacity){increment(state->counters.events_dropped_queue_full);return;}state->queue.push_back(std::move(event));state->ready.notify_one();}catch(...){state->fail();}
}

enum class Lifecycle { Attached,Enabled,Failed,Closed };
struct HealthState {
    std::mutex mutex;std::shared_ptr<SessionState> session;std::shared_ptr<irmel::Channel> channel;std::shared_ptr<irmel::HealthStatusChannel> health;std::shared_ptr<MetadataState> metadata;Lifecycle lifecycle{Lifecycle::Attached};bool enable_attempted{},metadata_attempted{},cleanup_started{};std::shared_ptr<HealthState> emergency_self;HealthState* emergency_next{};std::atomic<bool> retained{};
};
void retain_failed(const std::shared_ptr<HealthState>& state) noexcept
{
    static std::atomic<HealthState*> head{};bool expected=false;if(!state->retained.compare_exchange_strong(expected,true))return;state->emergency_self=state;auto* current=head.load(std::memory_order_relaxed);do{state->emergency_next=current;}while(!head.compare_exchange_weak(current,state.get(),std::memory_order_release,std::memory_order_relaxed));
}
bool has_health(const irmel::ChannelCapability& capability)
{const auto& types=capability.getChannelTypes();return std::find(types.begin(),types.end(),irmel::ChannelType::HealthAndStatus)!=types.end();}

bool cleanup(const std::shared_ptr<HealthState>& state,bool retain_orphan)
{
    std::shared_ptr<irmel::Channel> channel;std::shared_ptr<MetadataState> metadata;bool disable=false;
    {std::lock_guard lock{state->mutex};if(state->cleanup_started)return !state->channel;state->cleanup_started=true;channel=state->channel;metadata=state->metadata;disable=state->enable_attempted;}
    if(metadata){std::lock_guard lock{metadata->mutex};if(metadata->lifecycle==MetadataLifecycle::Active)metadata->lifecycle=MetadataLifecycle::Inactive;}
    bool ok=true;if(disable&&channel){try{if(channel->disable()!=irmel::Return::Success)ok=false;}catch(...){ok=false;}}
    bool detached=!channel;if(channel){try{detached=state->session->control->detachChannel(channel)==irmel::Return::Success;}catch(...){detached=false;}}
    if(!detached){{std::lock_guard lock{state->mutex};state->cleanup_started=false;}if(metadata)metadata->fail();if(retain_orphan)retain_failed(state);return false;}
    {std::lock_guard lock{state->mutex};state->health.reset();state->channel.reset();state->lifecycle=Lifecycle::Closed;state->enable_attempted=false;}channel.reset();
    if(metadata){std::unique_lock lock{metadata->mutex};metadata->callbacks_done.wait(lock,[&]{return metadata->callbacks.load(std::memory_order_acquire)==0U;});if(metadata->lifecycle!=MetadataLifecycle::Failed)metadata->lifecycle=MetadataLifecycle::Stopped;metadata->ready.notify_all();}return ok;
}
} // namespace

struct ams_mel_ir_health { std::shared_ptr<HealthState> state; };
struct ams_mel_ir_health_metadata { std::shared_ptr<MetadataState> state;std::weak_ptr<HealthState> channel; };
struct ams_mel_ir_health_metadata_event { std::unique_ptr<EventData> data; };

extern "C" ams_mel_status_t ams_mel_ir_health_open(ams_mel_session* session,const ams_mel_ir_health_config_v1* config,ams_mel_ir_health** output,char* out,std::size_t capacity,std::size_t* required) noexcept
{
    diagnostic("",out,capacity,required);if(!session||!session->state||!config||!output||*output||(!out&&capacity)||config->channel_type!=AMS_MEL_IR_CHANNEL_HEALTH_AND_STATUS||!valid_view(config->channel_id.descriptive_label)||!valid_view(config->platform_id.descriptive_label)||!valid_view(config->sensor_location.key)||!valid_view(config->sensor_location.system_name))return AMS_MEL_INVALID_ARGUMENT;std::shared_ptr<HealthState> state;
    try{state=std::make_shared<HealthState>();state->session=session->state;mel::ForeignKey key{copy_view(config->sensor_location.key),copy_view(config->sensor_location.system_name)};mel::ComponentLocation location{config->sensor_location.offset_x_m,config->sensor_location.offset_y_m,config->sensor_location.offset_z_m,key};irmel::Config upstream{convert_id(config->channel_id),irmel::ChannelType::HealthAndStatus,convert_id(config->platform_id),std::move(location),{},false,false};state->channel=state->session->control->attachChannel(upstream);if(!state->channel){diagnostic("attachChannel returned null",out,capacity,required);return AMS_MEL_FACTORY_FAILED;}state->health=std::dynamic_pointer_cast<irmel::HealthStatusChannel>(state->channel);if(!state->health||!has_health(state->channel->getCapabilities())){bool detached=false;try{detached=state->session->control->detachChannel(state->channel)==irmel::Return::Success;}catch(...){}if(!detached)retain_failed(state);diagnostic(detached?"attached channel is not compatible HealthAndStatus":"incompatible HealthAndStatus channel and detach failed",out,capacity,required);return detached?AMS_MEL_INITIALIZATION_FAILED:AMS_MEL_PROVIDER_FAILED;}auto owner=std::make_unique<ams_mel_ir_health>();owner->state=std::move(state);*output=owner.release();return AMS_MEL_OK;}catch(const std::bad_alloc&){if(state&&state->channel)retain_failed(state);diagnostic("allocation failed",out,capacity,required);return AMS_MEL_INTERNAL_ERROR;}catch(...){if(state&&state->channel)retain_failed(state);diagnostic("provider exception during Health open",out,capacity,required);return AMS_MEL_PROVIDER_EXCEPTION;}
}

extern "C" ams_mel_status_t ams_mel_ir_health_enable(ams_mel_ir_health* health,char* out,std::size_t capacity,std::size_t* required) noexcept
{diagnostic("",out,capacity,required);if(!health||!health->state||(!out&&capacity))return AMS_MEL_INVALID_ARGUMENT;try{std::lock_guard lock{health->state->mutex};if(health->state->lifecycle==Lifecycle::Enabled)return AMS_MEL_OK;if(health->state->lifecycle!=Lifecycle::Attached)return AMS_MEL_PROVIDER_FAILED;health->state->enable_attempted=true;if(health->state->channel->enable()!=irmel::Return::Success){health->state->lifecycle=Lifecycle::Failed;diagnostic("Health enable failed",out,capacity,required);return AMS_MEL_PROVIDER_FAILED;}health->state->lifecycle=Lifecycle::Enabled;return AMS_MEL_OK;}catch(...){health->state->lifecycle=Lifecycle::Failed;diagnostic("provider exception during Health enable",out,capacity,required);return AMS_MEL_PROVIDER_EXCEPTION;}}

extern "C" ams_mel_status_t ams_mel_ir_health_get_capabilities(ams_mel_ir_health* health,ams_mel_ir_channel_capability** output,char* out,std::size_t capacity,std::size_t* required) noexcept
{diagnostic("",out,capacity,required);if(!health||!health->state||!output||*output||(!out&&capacity))return AMS_MEL_INVALID_ARGUMENT;try{std::lock_guard lock{health->state->mutex};if(health->state->lifecycle!=Lifecycle::Attached&&health->state->lifecycle!=Lifecycle::Enabled)return AMS_MEL_PROVIDER_FAILED;return ams_mel::internal::snapshot_capability(*health->state->health,output,out,capacity,required);}catch(...){diagnostic("Health capability query failed",out,capacity,required);return AMS_MEL_PROVIDER_EXCEPTION;}}

extern "C" ams_mel_status_t ams_mel_ir_health_metadata_open(ams_mel_ir_health* health,std::size_t queue_capacity,ams_mel_ir_health_metadata** output,char* out,std::size_t capacity,std::size_t* required) noexcept
{
    diagnostic("",out,capacity,required);if(!health||!health->state||!queue_capacity||!output||*output||(!out&&capacity))return AMS_MEL_INVALID_ARGUMENT;try{std::shared_ptr<MetadataState> state;{std::lock_guard lock{health->state->mutex};if((health->state->lifecycle!=Lifecycle::Attached&&health->state->lifecycle!=Lifecycle::Enabled)||health->state->metadata_attempted)return AMS_MEL_INVALID_ARGUMENT;health->state->metadata_attempted=true;state=std::make_shared<MetadataState>();state->capacity=queue_capacity;health->state->metadata=state;}auto& channel=*health->state->health;
        auto one=channel.registerMetadataCallback(std::function<void(irmel::Channel&,const mel::MFA_Status*const)>{[state](irmel::Channel&,const mel::MFA_Status* value)noexcept{callback(state,[&]{return copy_mfa_status(value);});}});
        auto two=one==irmel::Return::Success?channel.registerMetadataCallback(std::function<void(irmel::Channel&,const mel::BIT_Status*const)>{[state](irmel::Channel&,const mel::BIT_Status* value)noexcept{callback(state,[&]{return copy_bit_status(value);});}}):irmel::Return::Fail;
        auto three=two==irmel::Return::Success?channel.registerMetadataCallback(std::function<void(irmel::Channel&,const irmel::SubsystemStatusResp*const)>{[state](irmel::Channel&,const irmel::SubsystemStatusResp* value)noexcept{callback(state,[&]{return copy_subsystem(value);});}}):irmel::Return::Fail;
        auto four=three==irmel::Return::Success?channel.registerMetadataCallback(std::function<void(irmel::Channel&,const mel::DiscreteStatus*const)>{[state](irmel::Channel&,const mel::DiscreteStatus* value)noexcept{callback(state,[&]{return copy_pairs(value,AMS_MEL_IR_HEALTH_METADATA_DISCRETE_STATUS);});}}):irmel::Return::Fail;
        auto five=four==irmel::Return::Success?channel.registerMetadataCallback(std::function<void(irmel::Channel&,const mel::MFA_SecurityAuditRecord*const)>{[state](irmel::Channel&,const mel::MFA_SecurityAuditRecord* value)noexcept{callback(state,[&]{return copy_security(value);});}}):irmel::Return::Fail;
        auto six=five==irmel::Return::Success?channel.registerMetadataCallback(std::function<void(irmel::Channel&,const mel::MFA_StatusDetailed*const)>{[state](irmel::Channel&,const mel::MFA_StatusDetailed* value)noexcept{callback(state,[&]{return copy_pairs(value,AMS_MEL_IR_HEALTH_METADATA_MFA_STATUS_DETAILED);});}}):irmel::Return::Fail;
        if(one!=irmel::Return::Success||two!=irmel::Return::Success||three!=irmel::Return::Success||four!=irmel::Return::Success||five!=irmel::Return::Success||six!=irmel::Return::Success){std::lock_guard lock{state->mutex};state->lifecycle=MetadataLifecycle::Inactive;diagnostic("Health metadata callback registration failed",out,capacity,required);return AMS_MEL_PROVIDER_FAILED;}auto owner=std::make_unique<ams_mel_ir_health_metadata>();owner->state=state;owner->channel=health->state;*output=owner.release();return AMS_MEL_OK;}catch(const std::bad_alloc&){diagnostic("Health metadata allocation failed",out,capacity,required);return AMS_MEL_INTERNAL_ERROR;}catch(...){diagnostic("provider exception during Health metadata registration",out,capacity,required);return AMS_MEL_PROVIDER_EXCEPTION;}
}

extern "C" ams_mel_status_t ams_mel_ir_health_metadata_receive(ams_mel_ir_health_metadata* metadata,std::uint32_t timeout_ms,ams_mel_ir_health_metadata_event** output,char* out,std::size_t capacity,std::size_t* required) noexcept
{diagnostic("",out,capacity,required);if(!metadata||!metadata->state||!output||*output||(!out&&capacity))return AMS_MEL_INVALID_ARGUMENT;try{std::unique_lock lock{metadata->state->mutex};if(metadata->state->queue.empty()&&metadata->state->lifecycle==MetadataLifecycle::Active&&timeout_ms)metadata->state->ready.wait_for(lock,std::chrono::milliseconds{timeout_ms},[&]{return !metadata->state->queue.empty()||metadata->state->lifecycle!=MetadataLifecycle::Active;});if(!metadata->state->queue.empty()){auto owner=std::make_unique<ams_mel_ir_health_metadata_event>();owner->data=std::move(metadata->state->queue.front());metadata->state->queue.pop_front();*output=owner.release();return AMS_MEL_OK;}if(metadata->state->lifecycle==MetadataLifecycle::Failed)return AMS_MEL_PROVIDER_FAILED;if(metadata->state->lifecycle!=MetadataLifecycle::Active)return AMS_MEL_STREAM_STOPPED;return AMS_MEL_TIMEOUT;}catch(...){diagnostic("Health metadata receive failed",out,capacity,required);return AMS_MEL_INTERNAL_ERROR;}}

extern "C" ams_mel_status_t ams_mel_ir_health_metadata_get_counters(const ams_mel_ir_health_metadata* metadata,ams_mel_ir_metadata_counters_v1* output,char* out,std::size_t capacity,std::size_t* required) noexcept
{diagnostic("",out,capacity,required);if(!metadata||!metadata->state||!output||(!out&&capacity))return AMS_MEL_INVALID_ARGUMENT;try{std::lock_guard lock{metadata->state->mutex};*output=metadata->state->counters;return AMS_MEL_OK;}catch(...){return AMS_MEL_INTERNAL_ERROR;}}

extern "C" ams_mel_status_t ams_mel_ir_health_metadata_close(ams_mel_ir_health_metadata** metadata,char* out,std::size_t capacity,std::size_t* required) noexcept
{diagnostic("",out,capacity,required);if(!metadata||(!out&&capacity))return AMS_MEL_INVALID_ARGUMENT;try{auto* owner=std::exchange(*metadata,nullptr);if(owner){std::lock_guard lock{owner->state->mutex};if(owner->state->lifecycle==MetadataLifecycle::Active)owner->state->lifecycle=MetadataLifecycle::Inactive;owner->state->ready.notify_all();}delete owner;return AMS_MEL_OK;}catch(...){return AMS_MEL_INTERNAL_ERROR;}}

extern "C" ams_mel_status_t ams_mel_ir_health_metadata_event_view(const ams_mel_ir_health_metadata_event* event,const ams_mel_ir_health_metadata_event_v1** output,char* out,std::size_t capacity,std::size_t* required) noexcept
{diagnostic("",out,capacity,required);if(!event||!event->data||!output||(!out&&capacity))return AMS_MEL_INVALID_ARGUMENT;*output=&event->data->view;return AMS_MEL_OK;}
extern "C" ams_mel_status_t ams_mel_ir_health_metadata_event_close(ams_mel_ir_health_metadata_event** event,char* out,std::size_t capacity,std::size_t* required) noexcept
{diagnostic("",out,capacity,required);if(!event||(!out&&capacity))return AMS_MEL_INVALID_ARGUMENT;try{delete std::exchange(*event,nullptr);return AMS_MEL_OK;}catch(...){return AMS_MEL_INTERNAL_ERROR;}}

extern "C" ams_mel_status_t ams_mel_ir_health_close(ams_mel_ir_health** health,char* out,std::size_t capacity,std::size_t* required) noexcept
{diagnostic("",out,capacity,required);if(!health||(!out&&capacity))return AMS_MEL_INVALID_ARGUMENT;auto* owner=*health;if(!owner)return AMS_MEL_OK;try{auto state=owner->state;{std::lock_guard lock{state->mutex};state->lifecycle=Lifecycle::Closed;}const bool ok=cleanup(state,false);if(state->channel){diagnostic("Health detach failed; provider state retained",out,capacity,required);return AMS_MEL_PROVIDER_FAILED;}*health=nullptr;delete owner;if(!ok){diagnostic("Health disable failed",out,capacity,required);return AMS_MEL_PROVIDER_FAILED;}return AMS_MEL_OK;}catch(...){diagnostic("Health close failed",out,capacity,required);return AMS_MEL_INTERNAL_ERROR;}}
