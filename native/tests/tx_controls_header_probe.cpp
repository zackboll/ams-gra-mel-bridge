#include <rfmel/jobs/JobInterval.h>
#include <type_traits>
#include <limits>
namespace tx_controls_contract {
namespace r=ams::iface::rfmel;
using T=r::TransmitEvent; using J=r::JobEvent;
#define SIGN(name,type) static_assert(std::is_same_v<decltype(&T::name),void(J::*)(type)>);
SIGN(setPolarization,const std::vector<r::StokesVector>&)
SIGN(setPolarizationBeemSteerCorrectionEnabled,bool)
SIGN(setPhaseOffset,r::Angle)
SIGN(setExecutionType,r::ExecutionType)
SIGN(setEventTerminationType,J::EventTerminationType)
SIGN(setAllowDelayStart,bool)
SIGN(setIterationHoldCount,size_t)
SIGN(setIterationTerminationCount,size_t)
SIGN(setChannelizationEnabled,bool)
static_assert(std::is_same_v<r::Angle,double>);
static_assert(static_cast<int>(r::ExecutionType::Normal)==0 && static_cast<int>(r::ExecutionType::Conditional)==1);
static_assert(static_cast<int>(J::EventTerminationType::InhibitEvent)==0 && static_cast<int>(J::EventTerminationType::CancelEvent)==1);
static_assert(std::is_same_v<decltype(std::declval<const T&>().getPolarization()),const std::vector<r::StokesVector>&>);
#define GET(name,type) static_assert(std::is_same_v<decltype(std::declval<const T&>().name()),type>);
GET(getPolarizationBeemSteerCorrection,bool)
GET(getPhaseOffset,double)
GET(getExecutionType,r::ExecutionType)
GET(getEventTerminationType,J::EventTerminationType)
GET(getAllowDelayStart,bool)
GET(getIterationHoldCount,size_t)
GET(getIterationTerminationCount,size_t)
GET(getChannelizationEnabled,bool)
int tx_controls_header_probe(){T t;
if(!t.getPolarization().empty() || t.getPolarizationBeemSteerCorrection() || t.getPhaseOffset()!=0 || t.getExecutionType()!=r::ExecutionType::Normal || t.getEventTerminationType()!=J::EventTerminationType::InhibitEvent || t.getAllowDelayStart() || t.getIterationHoldCount()!=0 || t.getIterationTerminationCount()!=0 || t.getChannelizationEnabled()) return 1;
t.setPolarization({{1,2,3,4},{5,6,7,8}});t.setPolarizationBeemSteerCorrectionEnabled(true);t.setPhaseOffset(-2.5);t.setExecutionType(r::ExecutionType::Conditional);t.setEventTerminationType(J::EventTerminationType::CancelEvent);t.setAllowDelayStart(true);t.setIterationHoldCount(SIZE_MAX);t.setIterationTerminationCount(1);t.setChannelizationEnabled(true);
T copy=t;t.setPolarization({});t.setPhaseOffset(9);if(copy.getPolarization().size()!=2 || copy.getPhaseOffset()!=-2.5 || !copy.getPolarizationBeemSteerCorrection() || copy.getExecutionType()!=r::ExecutionType::Conditional || copy.getEventTerminationType()!=J::EventTerminationType::CancelEvent || !copy.getAllowDelayStart() || copy.getIterationHoldCount()!=SIZE_MAX || copy.getIterationTerminationCount()!=1 || !copy.getChannelizationEnabled())return 2;
return 0;}

}
