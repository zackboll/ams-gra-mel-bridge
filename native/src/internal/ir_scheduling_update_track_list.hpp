#pragma once

#include <ams_mel/abi.h>
#include <irmel/library/scheduling/SchedulingChannel.h>
#include <memory>

struct UpdateTrackListState;

namespace ams_mel::internal {

// Called without the Scheduling mutex. Drain stops new payload reads and
// waits for admitted reads before physical provider teardown.
void scheduling_update_track_list_stopped(const std::shared_ptr<UpdateTrackListState>&) noexcept;
void scheduling_update_track_list_drained(const std::shared_ptr<UpdateTrackListState>&) noexcept;
void scheduling_update_track_list_failed(const std::shared_ptr<UpdateTrackListState>&) noexcept;

bool claim_scheduling_update_track_list(
    ams_mel_ir_scheduling&, const std::shared_ptr<UpdateTrackListState>&,
    std::shared_ptr<ams::iface::irmel::SchedulingChannel>&, std::shared_ptr<void>&) noexcept;

} // namespace ams_mel::internal
