#pragma once

#include <ams_mel/abi.h>
#include <irmel/library/scheduling/SchedulingChannel.h>
#include <memory>

struct ScanPerformanceReportState;

namespace ams_mel::internal {

// Called without the Scheduling mutex. Drain stops new payload reads and
// waits for admitted reads before physical provider teardown.
void scheduling_scan_performance_stopped(const std::shared_ptr<ScanPerformanceReportState>&) noexcept;
void scheduling_scan_performance_drained(const std::shared_ptr<ScanPerformanceReportState>&) noexcept;
void scheduling_scan_performance_failed(const std::shared_ptr<ScanPerformanceReportState>&) noexcept;

bool claim_scheduling_scan_performance(
    ams_mel_ir_scheduling&, const std::shared_ptr<ScanPerformanceReportState>&,
    std::shared_ptr<ams::iface::irmel::SchedulingChannel>&, std::shared_ptr<void>&) noexcept;

} // namespace ams_mel::internal
