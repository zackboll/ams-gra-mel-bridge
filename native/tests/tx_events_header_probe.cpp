#include <rfmel/jobs/JobInterval.h>
#include <type_traits>
#include <limits>
#include <iostream>
namespace tx_contract {
namespace r = ams::iface::rfmel;
using T=r::TransmitEvent; using J=r::JobEvent; using S=r::Sequence; using F=ams::util::math::Femtoseconds;
static_assert(std::is_same_v<r::JobEventID, uint32_t>);
static_assert(std::is_same_v<r::Frequency, double>);
static_assert(std::is_same_v<F::rep, int64_t>);
static_assert(std::is_same_v<decltype(&S::setTxEvents),void(S::*)(const std::vector<T>&)>);
static_assert(std::is_same_v<decltype(&S::setRxEvents),void(S::*)(const std::vector<r::ReceiveEvent>&)>);
static_assert(std::is_same_v<decltype(static_cast<const std::vector<T>&(S::*)()const>(&S::getTxEvents)),const std::vector<T>&(S::*)()const>);
static_assert(std::is_same_v<decltype(static_cast<std::vector<T>&(S::*)()>(&S::getTxEvents)),std::vector<T>&(S::*)()>);
static_assert(std::is_same_v<decltype(&T::setTxAtten_dB),void(T::*)(double)>);
static_assert(std::is_same_v<decltype(&T::setRiseDuration),void(T::*)(F)>);
static_assert(std::is_same_v<decltype(&T::setFallDuration),void(T::*)(F)>);
static_assert(std::is_same_v<decltype(&T::setApplicableTxElementGroups),void(T::*)(const std::vector<size_t>&)>);
static_assert(std::is_same_v<decltype(&T::setEventID),void(J::*)(uint32_t)>);
static_assert(std::is_same_v<decltype(&T::setElementGroupLabel),void(J::*)(const std::string&)>);
static_assert(std::is_same_v<decltype(&T::setStart),void(J::*)(F)>);
static_assert(std::is_same_v<decltype(&T::setDuration),void(J::*)(F)>);
static_assert(std::is_same_v<decltype(&T::setCenterFrequency),void(J::*)(double)>);
static_assert(std::is_same_v<decltype(&T::setStabPointIndex),void(J::*)(size_t)>);
static_assert(std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>);
static_assert(std::numeric_limits<size_t>::digits<=64);
int tx_events_header_probe() {
 T t;
 if(t.getModIndex()!=std::vector<int>{-1} || !t.getWeights().empty() || t.getDirection()!=J::Direction::Transmit || t.getTxAtten_dB()!=0 || t.getRiseDuration().count()!=0 || t.getFallDuration().count()!=0 || !t.getApplicableTxElementGroups().empty()) return 1;
 t.setEventID(UINT32_MAX); t.setElementGroupLabel("duplicate"); t.setStart(F{INT64_MIN}); t.setDuration(F{INT64_MAX}); t.setCenterFrequency(-0.0); t.setStabPointIndex(SIZE_MAX); t.setTxAtten_dB(-12.5); t.setRiseDuration(F{INT64_MAX}); t.setFallDuration(F{INT64_MIN});
 std::vector<size_t> groups{0,SIZE_MAX,0}; t.setApplicableTxElementGroups(groups); groups.clear();
 std::vector<T> events{t,t}; S s; s.setTxEvents(events); events.clear(); t.setElementGroupLabel("changed");
 const S copy=s;
 if(copy.getTxEvents().size()!=2 || !copy.getRxEvents().empty()) return 2;
 for(const auto& x:copy.getTxEvents()) if(x.getEventID()!=UINT32_MAX || x.getElementGroupLabel()!="duplicate" || x.getStart().count()!=INT64_MIN || x.getDuration().count()!=INT64_MAX || x.getStabPointIndex()!=SIZE_MAX || x.getTxAtten_dB()!=-12.5 || x.getRiseDuration().count()!=INT64_MAX || x.getFallDuration().count()!=INT64_MIN || x.getApplicableTxElementGroups()!=std::vector<size_t>{0,SIZE_MAX,0} || x.getModIndex()!=std::vector<int>{-1} || !x.getWeights().empty()) return 3;
 s.setRxEvents(std::vector<r::ReceiveEvent>{r::ReceiveEvent{}}); if(s.getTxEvents().size()!=2 || s.getRxEvents().size()!=1) return 4;
 s.setTxEvents({}); if(!s.getTxEvents().empty() || s.getRxEvents().size()!=1 || copy.getTxEvents().size()!=2) return 5;
 std::cout<<"Pinned TX signatures, defaults, scalar/vector copies, and independent RX/TX collections: passed\n";
 return 0;
}
}
