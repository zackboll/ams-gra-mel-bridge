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
    return rfmel::JobInterval{}.getIntervalStart() == rfmel::JobInterval::ContinueFromPrevious ? 0 : 1;
}
