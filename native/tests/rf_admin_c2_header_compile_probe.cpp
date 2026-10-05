// Task 034B1/034B2A: declaration-only probe of the published Admin/C2/VA/JobDetail roots.
// No provider object is constructed and no job operation is invoked.
#include <rfmel/factory/RFCreateFunctions.h>
#include <rfmel/admin/AdminMEL.h>
#include <rfmel/admin/UCI_Control.h>
#include <rfmel/admin/StatusControl.h>
#include <rfmel/c2/C2MEL.h>
#include <rfmel/c2/VirtualAperture.h>
#include <rfmel/c2/JobDetail.h>
#include <rfmel/mfa/PhysicalData.h>

#include <memory>
#include <string_view>
#include <type_traits>
#include <cstdint>
#include <chrono>
#include <limits>
#include "../include/ams_mel/abi.h"

namespace rfmel = ams::iface::rfmel;
// E6: actual pinned member signatures include size_t group IDs, not labels.
using VA = rfmel::VirtualAperture;
using JR = rfmel::JobRequest;
using Pointing = rfmel::PointingType;
static_assert(std::is_same_v<Pointing, std::variant<rfmel::ECEFPointing, rfmel::LLAPointing,
    rfmel::PlatformRelativePointing, rfmel::FaceRelativePointing, rfmel::BaselineRelativePointing>>);
static_assert(std::variant_size_v<Pointing> == 5);
static_assert(std::is_same_v<std::variant_alternative_t<0, Pointing>, rfmel::ECEFPointing>);
static_assert(std::is_same_v<std::variant_alternative_t<1, Pointing>, rfmel::LLAPointing>);
static_assert(std::is_same_v<std::variant_alternative_t<2, Pointing>, rfmel::PlatformRelativePointing>);
static_assert(std::is_same_v<std::variant_alternative_t<3, Pointing>, rfmel::FaceRelativePointing>);
static_assert(std::is_same_v<std::variant_alternative_t<4, Pointing>, rfmel::BaselineRelativePointing>);
static_assert(std::is_same_v<EcefPoint, boost::numeric::ublas::c_vector<double, 3>>);
static_assert(std::is_same_v<EcefVelocity, EcefPoint> && std::is_same_v<NedVelocity, EcefPoint>);
static_assert(std::is_same_v<decltype(&LLAPoint::getLatitude), double (LLAPoint::*)() const>);
static_assert(std::is_same_v<decltype(&LLAPoint::getLongitude), double (LLAPoint::*)() const>);
static_assert(std::is_same_v<decltype(&LLAPoint::getAltitude), double (LLAPoint::*)() const>);
static_assert(std::is_same_v<decltype(&LLAPoint::setLatitude), void (LLAPoint::*)(double)>);
static_assert(std::is_same_v<decltype(&LLAPoint::setLongitude), void (LLAPoint::*)(double)>);
static_assert(std::is_same_v<decltype(&LLAPoint::setAltitude), void (LLAPoint::*)(double)>);
static_assert(std::is_same_v<decltype(AzEl::az), double> && std::is_same_v<decltype(AzEl::el), double>);
static_assert(std::is_same_v<decltype(&JR::setEstimatedStabPoint), void (JR::*)(const Pointing&)>);
using EGC = rfmel::ElementGroupCommand;
static_assert(std::is_same_v<rfmel::TxPowerLevel, uint32_t>);
static_assert(std::is_same_v<std::underlying_type_t<rfmel::Mode>, uint8_t>);
static_assert(static_cast<uint8_t>(rfmel::Mode::RX) == 0);
static_assert(static_cast<uint8_t>(rfmel::Mode::TX) == 1);
static_assert(std::is_same_v<decltype(&EGC::setTxPower), void (EGC::*)(rfmel::TxPowerLevel)>);
static_assert(std::is_same_v<decltype(&EGC::getTxPower), rfmel::TxPowerLevel (EGC::*)() const>);
static_assert(std::is_same_v<decltype(&EGC::setDesiredDutyFactor), void (EGC::*)(rfmel::DutyFactor)>);
static_assert(std::is_same_v<decltype(&EGC::getDesiredDutyFactor), rfmel::DutyFactor (EGC::*)() const>);
static_assert(std::is_same_v<decltype(&EGC::addExpectedCenterFrequencies), void (EGC::*)(rfmel::FrequencyRange)>);
static_assert(std::is_same_v<decltype(&JR::getElementGroups), const rfmel::ElementGroupCommandList& (JR::*)() const>);
static_assert(std::is_same_v<decltype(&JR::getRxElementGroups), const rfmel::ElementGroupCommandList (JR::*)() const>);
static_assert(std::is_same_v<decltype(&JR::getTxElementGroups), const rfmel::ElementGroupCommandList (JR::*)() const>);
static_assert(std::is_same_v<decltype(std::declval<const rfmel::ElementGroupCommandList&>()[0]),
    const std::shared_ptr<EGC>&>);
static_assert(std::is_same_v<decltype(&EGC::addExpectedPointingAngle), void (EGC::*)(const Pointing&)>);
static_assert(std::is_same_v<decltype(&EGC::getExpectedPointingAngles), std::vector<Pointing> (EGC::*)()>);
using UTC = ams::util::math::UTCTime;
using Femto = ams::util::math::Femtoseconds;
static_assert(std::is_same_v<std::chrono::seconds::rep, int64_t>);
static_assert(std::is_signed_v<std::chrono::seconds::rep> && sizeof(std::chrono::seconds::rep) == 8);
static_assert(std::is_same_v<Femto::rep, int64_t> && std::is_signed_v<Femto::rep>);
static_assert(std::femto::den == INT64_C(1000000000000000));
static_assert(std::is_same_v<rfmel::Priority, uint32_t>);
static_assert(std::is_same_v<rfmel::PrecedenceWithinPriority, uint32_t>);
static_assert(std::is_same_v<rfmel::EndpointID, uint64_t>);
static_assert(std::is_same_v<decltype(&JR::setPriority), void (JR::*)(uint32_t)>);
static_assert(std::is_same_v<decltype(&JR::setPrecedenceWithinPriority), void (JR::*)(uint32_t)>);
static_assert(std::is_same_v<decltype(&JR::setMinStartTime), void (JR::*)(UTC)>);
static_assert(std::is_same_v<decltype(&JR::setMaxCompleteTime), void (JR::*)(UTC)>);
static_assert(std::is_same_v<decltype(&JR::setDuration), void (JR::*)(Femto)>);
static_assert(std::is_same_v<decltype(static_cast<void (JR::*)(std::shared_ptr<rfmel::ElementGroupCommand>)>(&JR::addElementGroup)), void (JR::*)(std::shared_ptr<rfmel::ElementGroupCommand>)>);
static_assert(std::is_same_v<decltype(&JR::setCapabilityId), void (JR::*)(const std::vector<uint8_t>&)>);
static_assert(std::is_same_v<decltype(&JR::setActivityId), void (JR::*)(const std::vector<uint8_t>&)>);
static_assert(std::is_same_v<decltype(&JR::setRequestId), void (JR::*)(uint32_t)>);
static_assert(std::is_same_v<decltype(&JR::setInstanceSelection), void (JR::*)(const std::vector<uint32_t>&)>);
static_assert(std::is_same_v<decltype(&JR::setIsInterruptable), void (JR::*)(bool)>);
static_assert(std::is_same_v<decltype(&JR::setTxPowerModeIDs), void (JR::*)(const std::set<uint32_t>&)>);
static_assert(std::is_same_v<decltype(&JR::setLookAheadTime), void (JR::*)(Femto)>);
static_assert(std::is_same_v<decltype(&JR::getPriority), uint32_t (JR::*)() const>);
static_assert(std::is_same_v<decltype(&JR::getPrecedenceWithinPriority), uint32_t (JR::*)() const>);
static_assert(std::is_same_v<decltype(&JR::getMinStartTime), UTC (JR::*)() const>);
static_assert(std::is_same_v<decltype(&JR::getMaxCompleteTime), UTC (JR::*)() const>);
static_assert(std::is_same_v<decltype(&JR::getDuration), Femto (JR::*)() const>);
static_assert(std::is_same_v<decltype(&JR::getElementGroups), const rfmel::ElementGroupCommandList& (JR::*)() const>);
static_assert(std::is_same_v<decltype(&JR::getCapabilityId), std::vector<uint8_t> (JR::*)() const>);
static_assert(std::is_same_v<decltype(&JR::getActivityId), std::vector<uint8_t> (JR::*)() const>);
static_assert(std::is_same_v<decltype(&JR::getRequestId), uint32_t (JR::*)() const>);
static_assert(std::is_same_v<decltype(static_cast<const std::vector<uint32_t>& (JR::*)() const>(&JR::getInstanceSelection)), const std::vector<uint32_t>& (JR::*)() const>);
static_assert(std::is_same_v<decltype(&JR::getIsInterruptable), bool (JR::*)() const>);
static_assert(std::is_same_v<decltype(&JR::getTxPowerModeIDs), std::set<uint32_t> (JR::*)() const>);
static_assert(std::is_same_v<decltype(&JR::getLookAheadTime), const Femto (JR::*)() const>);
static_assert(std::is_same_v<decltype(static_cast<std::shared_ptr<rfmel::ElementGroupCommand> (VA::*)(rfmel::ElementGroupLabel)>(&VA::createElementGroupCommand)), std::shared_ptr<rfmel::ElementGroupCommand> (VA::*)(rfmel::ElementGroupLabel)>);
static_assert(std::is_same_v<decltype(&rfmel::ElementGroupCommand::addEndpointIDs), void (rfmel::ElementGroupCommand::*)(const std::set<uint64_t>&, rfmel::DataPipeLabel)>);
static_assert(std::is_same_v<rfmel::TxPowerModeID, uint32_t>);
static_assert(std::is_same_v<rfmel::VirtualApertureInstanceID, uint32_t>);
static_assert(std::is_same_v<rfmel::WeightType, std::size_t>);
static_assert(std::is_same_v<rfmel::AnglePair, std::pair<double, double>>);
static_assert(std::is_same_v<rfmel::AnglePair::first_type, double>);
static_assert(std::is_same_v<rfmel::AnglePair::second_type, double>);
static_assert(std::is_same_v<decltype(&VA::getTxRadiatedPower), double (VA::*)(std::size_t, uint32_t, double, std::size_t, double, std::pair<double, double>, uint32_t) const>);
static_assert(std::is_same_v<decltype(&VA::getTxPeakRadiatedPower), double (VA::*)(std::size_t, uint32_t, double, double, uint32_t) const>);
static_assert(std::is_same_v<decltype(&VA::getTxApertureGain), double (VA::*)(std::size_t, uint32_t, std::size_t, double, std::pair<double, double>, uint32_t) const>);
static_assert(std::is_same_v<decltype(&VA::getMaxTxAttenuation), double (VA::*)(std::size_t, uint32_t, uint32_t) const>);
using LFCatalog = std::map<rfmel::LocalFunctionTypeID, std::size_t>;
static_assert(std::is_same_v<decltype(&rfmel::VirtualAperture::isCachedWaveformSupported), bool (rfmel::VirtualAperture::*)() const>);
static_assert(std::is_same_v<decltype(&rfmel::VirtualAperture::dynamicWeightsSupported), bool (rfmel::VirtualAperture::*)() const>);
static_assert(std::is_same_v<decltype(&rfmel::VirtualAperture::getLocalFunctions), LFCatalog (rfmel::VirtualAperture::*)() const>);
static_assert(std::is_same_v<LFCatalog::mapped_type, std::size_t>);
static_assert(std::is_same_v<decltype(&rfmel::VirtualAperture::getLocalFunctionStatus), std::vector<rfmel::VirtualApertureStatus> (rfmel::VirtualAperture::*)(rfmel::VirtualApertureInstanceID, rfmel::LocalFunctionTypeID) const>);
// 034E3: exact pinned by-value containers and const virtual getter signatures.
using Descriptor = rfmel::ElementGroupDescriptor;
using Pipe = rfmel::DataPipe;
using PipeMap = std::map<rfmel::DataPipeLabel, std::shared_ptr<Pipe>>;
// E5: exact VA-level getter and set mutations; no expanded vendor dependency.
using ConnectionMap = std::unordered_map<rfmel::ElementGroupLabel, PipeMap>;
static_assert(std::is_same_v<decltype(&rfmel::VirtualAperture::getDataPipes), rfmel::ElementGroupConnections (rfmel::VirtualAperture::*)()>);
static_assert(std::is_same_v<decltype(std::declval<const rfmel::ElementGroupConnections&>().begin()), ConnectionMap::const_iterator>);
static_assert(std::is_same_v<decltype(std::declval<rfmel::ElementGroupConnections&>().begin()), ConnectionMap::iterator>);
static_assert(std::is_same_v<decltype(*std::declval<const rfmel::ElementGroupConnections&>().begin()), const ConnectionMap::value_type&>);
static_assert(std::is_same_v<decltype(&Pipe::associateEndpoint), bool (Pipe::*)(rfmel::EndpointID)>);
static_assert(std::is_same_v<decltype(&Pipe::associateEndpoints), bool (Pipe::*)(const std::set<rfmel::EndpointID>&)>);
using DescriptorMap = std::unordered_map<rfmel::ElementGroupLabel, std::shared_ptr<Descriptor>>;
static_assert(std::is_same_v<std::underlying_type_t<rfmel::Mode>, uint8_t>);
static_assert(static_cast<unsigned>(rfmel::Mode::RX) == AMS_MEL_RF_ELEMENT_GROUP_MODE_RX);
static_assert(static_cast<unsigned>(rfmel::Mode::TX) == AMS_MEL_RF_ELEMENT_GROUP_MODE_TX);
static_assert(std::is_same_v<rfmel::DutyFactor, double>);
static_assert(std::is_same_v<rfmel::EndpointID, uint64_t>);
static_assert(std::is_same_v<rfmel::ElementGroupLabel, std::string>);
static_assert(std::is_same_v<rfmel::DataPipeLabel, std::string>);
static_assert(std::is_same_v<decltype(&rfmel::VirtualAperture::getElementGroups), rfmel::ElementGroupDescriptorLookupMap (rfmel::VirtualAperture::*)() const>);
static_assert(std::is_same_v<decltype(std::declval<const rfmel::ElementGroupDescriptorLookupMap&>().begin()), DescriptorMap::const_iterator>);
static_assert(std::is_same_v<decltype(&Descriptor::getElementGroupLabel), rfmel::ElementGroupLabel (Descriptor::*)() const>);
static_assert(std::is_same_v<decltype(&Descriptor::getMode), rfmel::Mode (Descriptor::*)() const>);
static_assert(std::is_same_v<decltype(&Descriptor::getMaxRfBandwidth), double (Descriptor::*)() const>);
static_assert(std::is_same_v<decltype(&Descriptor::getMaxSampleRate), double (Descriptor::*)() const>);
static_assert(std::is_same_v<decltype(&Descriptor::getMaxDataRate), double (Descriptor::*)() const>);
static_assert(std::is_same_v<decltype(&Descriptor::getMaxDutyFactor), double (Descriptor::*)() const>);
static_assert(std::is_same_v<decltype(&Descriptor::getDataPipes), PipeMap (Descriptor::*)() const>);
static_assert(std::is_same_v<decltype(&Pipe::getLabel), const rfmel::DataPipeLabel (Pipe::*)() const>);
static_assert(std::is_same_v<decltype(&Pipe::getAssociatedEndpoints), std::set<uint64_t> (Pipe::*)() const>);
// Task 034E1: all six exact live methods, distinct set/vector getters, and
// concrete report getters. In particular getLFStatus is a VALUE, not reference.
using BaseVA = rfmel::BaseVirtualAperture;
using VaCallback = std::function<void(BaseVA&)>;
static_assert(std::is_same_v<decltype(&BaseVA::addStatusCallback), std::size_t (BaseVA::*)(const VaCallback&)>);
static_assert(std::is_same_v<decltype(&BaseVA::removeStatusCallback), void (BaseVA::*)(std::size_t)>);
using VAReport = const rfmel::VirtualApertureInstanceStatusReport&;
using LFStatuses = std::map<rfmel::LocalFunctionTypeID, std::vector<rfmel::VirtualApertureStatus>>;
static_assert(std::is_same_v<rfmel::VirtualApertureDefinitionID, uint32_t>);
static_assert(std::is_same_v<rfmel::VirtualApertureInstanceID, uint32_t>);
static_assert(std::is_same_v<rfmel::FaceID, uint32_t>);
static_assert(std::is_same_v<rfmel::LocalFunctionTypeID, uint32_t>);
static_assert(std::is_same_v<decltype(&BaseVA::getID), uint32_t (BaseVA::*)() const>);
static_assert(std::is_same_v<decltype(&BaseVA::getStatus), rfmel::VirtualApertureStatus (BaseVA::*)() const>);
static_assert(std::is_same_v<decltype(&BaseVA::getInstanceStatus), rfmel::VirtualApertureStatus (BaseVA::*)(uint32_t) const>);
static_assert(std::is_same_v<decltype(&BaseVA::getAllInstances), std::vector<uint32_t> (BaseVA::*)() const>);
static_assert(std::is_same_v<decltype(&BaseVA::getInstances), std::vector<uint32_t> (BaseVA::*)(uint32_t) const>);
static_assert(std::is_same_v<decltype(&BaseVA::getInstanceStatusReport), rfmel::VirtualApertureInstanceStatusReport (BaseVA::*)(uint32_t) const>);
static_assert(std::is_same_v<decltype(std::declval<const rfmel::VirtualAperture&>().getVAInstanceIDs()), std::set<uint32_t>>);
static_assert(std::is_same_v<decltype(std::declval<VAReport>().getVAInstanceID()), uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<VAReport>().getStatus()), rfmel::VirtualApertureStatus>);
static_assert(std::is_same_v<decltype(std::declval<VAReport>().getLFStatus()), LFStatuses>);
static_assert(static_cast<unsigned>(rfmel::VirtualApertureStatus::None) == AMS_MEL_RF_VA_STATUS_NONE);
static_assert(static_cast<unsigned>(rfmel::VirtualApertureStatus::Operational) == AMS_MEL_RF_VA_STATUS_OPERATIONAL);
static_assert(static_cast<unsigned>(rfmel::VirtualApertureStatus::Degraded) == AMS_MEL_RF_VA_STATUS_DEGRADED);
static_assert(static_cast<unsigned>(rfmel::VirtualApertureStatus::Failed) == AMS_MEL_RF_VA_STATUS_FAILED);
using Fs = ams::util::math::Femtoseconds;
static_assert(std::is_same_v<Fs::rep, std::int64_t>);
static_assert(!std::numeric_limits<Fs>::is_specialized);
static_assert(std::numeric_limits<Fs>::max().count() == 0);
static_assert(rfmel::JobInterval::ContinueFromPrevious.count() == 0);
static_assert(Fs::max().count() == INT64_MAX);
static_assert(rfmel::JobInterval::ContinueFromPrevious.count() != Fs::max().count());
static_assert(rfmel::JobInterval::ContinueFromPrevious.count() ==
              AMS_MEL_RF_JOB_INTERVAL_CONTINUE_FROM_PREVIOUS_FS);

// Task 034C3: exact pinned const TxPowerModeData getter/representation surface.
using TxMode = const rfmel::TxPowerModeData&;
static_assert(std::is_same_v<rfmel::TxPowerModeID, std::uint32_t>);
static_assert(std::is_same_v<rfmel::TxPowerLevel, std::uint32_t>);
static_assert(std::is_same_v<rfmel::DutyFactor, double>);
static_assert(std::is_same_v<rfmel::Frequency, double>);
static_assert(std::is_same_v<std::chrono::nanoseconds::rep, std::int64_t>);
static_assert(std::is_integral_v<std::chrono::nanoseconds::rep>);
static_assert(std::is_signed_v<std::chrono::nanoseconds::rep>);
static_assert(std::is_same_v<decltype(std::declval<TxMode>().getTxPowerModeID()), std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<TxMode>().getIsLinearOperation()), bool>);
static_assert(std::is_same_v<decltype(std::declval<TxMode>().getTxPowerLevel()), std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<TxMode>().getTxFrequencyRanges(0)),
                             const std::vector<rfmel::FrequencyRange>&>);
static_assert(std::is_same_v<decltype(std::declval<TxMode>().getMaxTxDutyFactor()), double>);
static_assert(std::is_same_v<decltype(std::declval<TxMode>().getMaxTxPulseWidth()), std::chrono::nanoseconds>);
static_assert(std::is_same_v<decltype(std::declval<TxMode>().getMaxTxAtten()), double>);
static_assert(std::is_same_v<decltype(std::declval<TxMode>().getTxAttenStepSize()), double>);

static_assert(std::is_same_v<rfmel::fnAdminMEL,
                             std::shared_ptr<rfmel::AdminMEL> (*)(std::string_view)>);
static_assert(std::is_same_v<rfmel::fnC2MEL,
                             std::shared_ptr<rfmel::C2MEL> (*)(std::string_view)>);
static_assert(std::is_abstract_v<rfmel::AdminMEL>);
static_assert(std::is_abstract_v<rfmel::C2MEL>);
static_assert(std::is_abstract_v<rfmel::VirtualAperture>);
static_assert(std::is_abstract_v<rfmel::JobDetail>);

// Check the complete const getter surface consumed by the PhysicalData snapshot.
using Physical = const rfmel::PhysicalData&;
using Installation = decltype(std::declval<Physical>().getInstallationDetails());
using Location = decltype(std::declval<Installation>().getLocation());
using Key = decltype(std::declval<Location>().getLocationId());
using Orientation = decltype(std::declval<Installation>().getOrientation());
using Boresight = decltype(std::declval<Installation>().getBoresight());
static_assert(std::is_same_v<decltype(std::declval<Physical>().getAntennaHeight()), double>);
static_assert(std::is_same_v<decltype(std::declval<Physical>().getAntennaWidth()), double>);
static_assert(std::is_same_v<decltype(std::declval<Physical>().getLatticeAngle()), double>);
static_assert(std::is_same_v<decltype(std::declval<Location>().getOffsetX()), double>);
static_assert(std::is_same_v<decltype(std::declval<Location>().getOffsetY()), double>);
static_assert(std::is_same_v<decltype(std::declval<Location>().getOffsetZ()), double>);
static_assert(std::is_same_v<decltype(std::declval<Key>().getKey()), const std::string&>);
static_assert(std::is_same_v<decltype(std::declval<Key>().getSystemName()), const std::string&>);
static_assert(std::is_same_v<decltype(std::declval<Orientation>().getRoll()), double>);
static_assert(std::is_same_v<decltype(std::declval<Orientation>().getPitch()), double>);
static_assert(std::is_same_v<decltype(std::declval<Orientation>().getYaw()), double>);
static_assert(std::is_same_v<decltype(std::declval<Boresight>().getRoll()), double>);
static_assert(std::is_same_v<decltype(std::declval<Boresight>().getPitch()), double>);
static_assert(std::is_same_v<decltype(std::declval<Boresight>().getYaw()), double>);

static_assert(std::is_same_v<decltype(std::declval<rfmel::JobInterval&>().setIntervalStart(std::declval<Fs>())), void>);
static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<const rfmel::JobInterval&>().getIntervalStart())>, std::remove_cvref_t<Fs>>);
static_assert(std::is_same_v<decltype(std::declval<rfmel::JobInterval&>().setIntervalID(std::declval<uint32_t>())), void>);
static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<const rfmel::JobInterval&>().getIntervalID())>, std::remove_cvref_t<uint32_t>>);
static_assert(std::is_same_v<decltype(std::declval<rfmel::JobInterval&>().setIntervalStartingGap(std::declval<Fs>())), void>);
static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<const rfmel::JobInterval&>().getIntervalStartingGap())>, std::remove_cvref_t<Fs>>);
static_assert(std::is_same_v<decltype(std::declval<rfmel::JobInterval&>().setSequence(std::declval<const rfmel::Sequence&>())), void>);
static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<const rfmel::JobInterval&>().getSequence())>, std::remove_cvref_t<const rfmel::Sequence&>>);
static_assert(std::is_same_v<decltype(std::declval<rfmel::JobInterval&>().setSequenceRepeatCount(std::declval<size_t>())), void>);
static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<const rfmel::JobInterval&>().getSequenceRepeatCount())>, std::remove_cvref_t<size_t>>);
static_assert(std::is_same_v<decltype(std::declval<rfmel::JobInterval&>().setCalDuration(std::declval<Fs>())), void>);
static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<const rfmel::JobInterval&>().getCalDuration())>, std::remove_cvref_t<Fs>>);
static_assert(std::is_same_v<decltype(std::declval<rfmel::JobInterval&>().setIntervalEndingGap(std::declval<Fs>())), void>);
static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<const rfmel::JobInterval&>().getIntervalEndingGap())>, std::remove_cvref_t<Fs>>);
static_assert(std::is_same_v<decltype(std::declval<rfmel::JobInterval&>().setPhaseCoherenceWithPrior(std::declval<bool>())), void>);
static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<const rfmel::JobInterval&>().getPhaseCoherenceWithPrior())>, std::remove_cvref_t<bool>>);
static_assert(std::is_same_v<decltype(std::declval<rfmel::JobInterval&>().setIterationsPerSignal(std::declval<size_t>())), void>);
static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<const rfmel::JobInterval&>().getIterationsPerSignal())>, std::remove_cvref_t<size_t>>);
static_assert(std::is_same_v<decltype(std::declval<rfmel::JobInterval&>().setMaxDataRateBps(std::declval<double>())), void>);
static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<const rfmel::JobInterval&>().getMaxDataRateBps())>, std::remove_cvref_t<double>>);
static_assert(std::is_same_v<decltype(std::declval<rfmel::JobInterval&>().setMaxSampleRateHZ(std::declval<rfmel::Frequency>())), void>);
static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<const rfmel::JobInterval&>().getMaxSampleRateHZ())>, std::remove_cvref_t<rfmel::Frequency>>);
static_assert(std::is_same_v<decltype(std::declval<rfmel::JobInterval&>().setJobDetailsId(std::declval<uint32_t>())), void>);
static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<const rfmel::JobInterval&>().getJobDetailsID())>, std::remove_cvref_t<uint32_t>>);
static_assert(std::is_same_v<decltype(std::declval<rfmel::Sequence&>().setDuration(std::declval<Fs>())), void>);
static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<const rfmel::Sequence&>().getDuration())>, std::remove_cvref_t<Fs>>);
static_assert(std::is_same_v<decltype(std::declval<rfmel::Sequence&>().setRxEvents(std::declval<const std::vector<rfmel::ReceiveEvent>&>())), void>);
static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<const rfmel::Sequence&>().getRxEvents())>, std::remove_cvref_t<const std::vector<rfmel::ReceiveEvent>&>>);
static_assert(std::is_same_v<decltype(std::declval<rfmel::ReceiveEvent&>().setEventID(std::declval<rfmel::JobEventID>())), void>);
static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<const rfmel::ReceiveEvent&>().getEventID())>, std::remove_cvref_t<rfmel::JobEventID>>);
static_assert(std::is_same_v<decltype(std::declval<rfmel::ReceiveEvent&>().setElementGroupLabel(std::declval<const rfmel::ElementGroupLabel&>())), void>);
static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<const rfmel::ReceiveEvent&>().getElementGroupLabel())>, std::remove_cvref_t<const rfmel::ElementGroupLabel&>>);
static_assert(std::is_same_v<decltype(std::declval<rfmel::ReceiveEvent&>().setStart(std::declval<Fs>())), void>);
static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<const rfmel::ReceiveEvent&>().getStart())>, std::remove_cvref_t<Fs>>);
static_assert(std::is_same_v<decltype(std::declval<rfmel::ReceiveEvent&>().setDuration(std::declval<Fs>())), void>);
static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<const rfmel::ReceiveEvent&>().getDuration())>, std::remove_cvref_t<Fs>>);
static_assert(std::is_same_v<decltype(std::declval<rfmel::ReceiveEvent&>().setCenterFrequency(std::declval<rfmel::Frequency>())), void>);
static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<const rfmel::ReceiveEvent&>().getCenterFrequency())>, std::remove_cvref_t<rfmel::Frequency>>);
static_assert(std::is_same_v<decltype(std::declval<rfmel::ReceiveEvent&>().setSampleFrequency(std::declval<rfmel::Frequency>())), void>);
static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<const rfmel::ReceiveEvent&>().getSampleFrequency())>, std::remove_cvref_t<rfmel::Frequency>>);
static_assert(std::is_same_v<decltype(std::declval<rfmel::ReceiveEvent&>().setNumIterationProcessingAGC(std::declval<size_t>())), void>);
static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<const rfmel::ReceiveEvent&>().getNumIterationProcessingAGC())>, std::remove_cvref_t<size_t>>);
static_assert(std::is_same_v<decltype(std::declval<rfmel::ReceiveEvent&>().setNumIterationIgnoredPostAGC(std::declval<size_t>())), void>);
static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<const rfmel::ReceiveEvent&>().getNumIterationIgnoredPostAGC())>, std::remove_cvref_t<size_t>>);
static_assert(std::is_same_v<decltype(std::declval<rfmel::ReceiveEvent&>().setMaxExtensionDuration(std::declval<Fs>())), void>);
static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<const rfmel::ReceiveEvent&>().getMaxExtensionDuration())>, std::remove_cvref_t<Fs>>);

static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::None) == AMS_MEL_RF_INTERVAL_COMPLETION_NONE);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::ReadyForNextJobInterval) == AMS_MEL_RF_INTERVAL_COMPLETION_READY_FOR_NEXT_JOB_INTERVAL);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInterrupted) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INTERRUPTED);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidTxEventSpatialData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_TX_EVENT_SPATIAL_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidTxEventSignalData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_TX_EVENT_SIGNAL_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidTxEventTemporalData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_TX_EVENT_TEMPORAL_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidTxEventIdentifierData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_TX_EVENT_IDENTIFIER_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidRxEventSpatialData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_RX_EVENT_SPATIAL_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidRxEventSignalData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_RX_EVENT_SIGNAL_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidRxEventTemporalData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_RX_EVENT_TEMPORAL_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidRxEventIdentifierData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_RX_EVENT_IDENTIFIER_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidSequenceTemporalData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_SEQUENCE_TEMPORAL_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidJobIntervalSpatialData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_INTERVAL_SPATIAL_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidJobIntervalEventSignalData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_INTERVAL_EVENT_SIGNAL_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidJobIntervalEventTemporalData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_INTERVAL_EVENT_TEMPORAL_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidJobIntervalEventIdentifierData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_INTERVAL_EVENT_IDENTIFIER_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidJobTemporalData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_TEMPORAL_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::FailedInvalidJobIdentifierData) == AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_IDENTIFIER_DATA);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::Completed) == AMS_MEL_RF_INTERVAL_COMPLETION_COMPLETED);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::Cancelled) == AMS_MEL_RF_INTERVAL_COMPLETION_CANCELLED);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::lateControls) == AMS_MEL_RF_INTERVAL_COMPLETION_LATE_CONTROLS);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::invalidControls) == AMS_MEL_RF_INTERVAL_COMPLETION_INVALID_CONTROLS);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::antennaFovError) == AMS_MEL_RF_INTERVAL_COMPLETION_ANTENNA_FOV_ERROR);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::TransmitRfInhibited) == AMS_MEL_RF_INTERVAL_COMPLETION_TRANSMIT_RF_INHIBITED);
static_assert(static_cast<unsigned>(rfmel::JobIntervalCompletionStatus::Started) == AMS_MEL_RF_INTERVAL_COMPLETION_STARTED);
static_assert(static_cast<unsigned>(rfmel::JobEventLogTriggerType::None) == AMS_MEL_RF_LOG_TRIGGER_NONE);
static_assert(static_cast<unsigned>(rfmel::JobEventLogTriggerType::eventExtended) == AMS_MEL_RF_LOG_TRIGGER_EVENT_EXTENDED);
static_assert(static_cast<unsigned>(rfmel::JobEventLogTriggerType::eventTriggered) == AMS_MEL_RF_LOG_TRIGGER_EVENT_TRIGGERED);
static_assert(static_cast<unsigned>(rfmel::JobEventLogTriggerType::eventResumed) == AMS_MEL_RF_LOG_TRIGGER_EVENT_RESUMED);
static_assert(static_cast<unsigned>(rfmel::JobEventLogTriggerType::eventCancelled) == AMS_MEL_RF_LOG_TRIGGER_EVENT_CANCELLED);
static_assert(static_cast<unsigned>(rfmel::JobEventLogTriggerType::eventInhibited) == AMS_MEL_RF_LOG_TRIGGER_EVENT_INHIBITED);
static_assert(static_cast<unsigned>(rfmel::JobEventLogTriggerType::eventDelayedStart) == AMS_MEL_RF_LOG_TRIGGER_EVENT_DELAYED_START);
static_assert(static_cast<unsigned>(rfmel::JobEventLogTriggerType::eventTypeNotSupported) == AMS_MEL_RF_LOG_TRIGGER_EVENT_TYPE_NOT_SUPPORTED);
static_assert(static_cast<unsigned>(rfmel::JobIntervalStatusEnable::Never) == AMS_MEL_RF_INTERVAL_STATUS_NEVER);
static_assert(static_cast<unsigned>(rfmel::JobIntervalStatusEnable::Always) == AMS_MEL_RF_INTERVAL_STATUS_ALWAYS);
static_assert(static_cast<unsigned>(rfmel::JobIntervalStatusEnable::OnException) == AMS_MEL_RF_INTERVAL_STATUS_ON_EXCEPTION);
using StatusCallback = std::function<void(rfmel::JobIntervalStatus)>;
static_assert(std::is_same_v<decltype(&rfmel::JobDetail::registerJobIntervalStatusCallback), void (rfmel::JobDetail::*)(const StatusCallback&)>);
static_assert(std::is_same_v<decltype(std::declval<const rfmel::JobIntervalStatus&>().getJobIntervalID()), const uint32_t&>);
static_assert(std::is_same_v<decltype(std::declval<const rfmel::JobIntervalStatus&>().getJobIntervalCompletionStatus()), const rfmel::JobIntervalCompletionStatus&>);
static_assert(std::is_same_v<decltype(std::declval<const rfmel::JobIntervalStatus&>().getJobEventLog()), const std::map<rfmel::JobEventID, rfmel::JobEventLogInfo>&>);
static_assert(std::is_same_v<decltype(std::declval<const rfmel::JobIntervalStatus&>().getActivityId()), const std::vector<uint8_t>&>);
static_assert(std::is_same_v<decltype(std::declval<const rfmel::JobEventLogInfo&>().getJobEventLogTrigger()), const rfmel::JobEventLogTriggerType&>);
static_assert(std::is_same_v<decltype(std::declval<rfmel::JobEventLogInfo&>().getJobEventLogTime()), const ams::util::math::UTCTime&>);
static_assert(std::is_same_v<std::chrono::seconds::rep, int64_t>);
static_assert(std::is_same_v<rfmel::JobEventID, uint32_t>);
static_assert(std::is_same_v<decltype(&rfmel::JobDetail::extendJobEvent),
                             void (rfmel::JobDetail::*)(uint32_t, rfmel::JobEventID, Fs)>);
static_assert(std::is_same_v<decltype(&rfmel::JobInterval::setJobIntervalStatusEnable), void (rfmel::JobInterval::*)(rfmel::JobIntervalStatusEnable)>);
int rf_admin_c2_header_compile_probe()
{
    for (const auto seconds : {INT64_MIN, INT64_MAX}) {
        const UTC time{std::chrono::seconds{seconds}, Femto{999999999999999}};
        if (time.getIntegralSeconds().count() != seconds ||
            time.getFractionalFemtoseconds().count() != 999999999999999) return 1;
    }
    return rfmel::JobInterval{}.getIntervalStart() == rfmel::JobInterval::ContinueFromPrevious ? 0 : 1;
}
