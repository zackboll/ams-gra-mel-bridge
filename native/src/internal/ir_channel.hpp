#pragma once

#include <ams_mel/abi.h>

#include <irmel/library/irmel-types/Channel.h>

#include <cstddef>

namespace ams_mel::internal {

ams_mel_status_t snapshot_capability(
    const ams::iface::irmel::Channel& channel,
    ams_mel_ir_channel_capability **output,
    char *diagnostic,
    std::size_t diagnostic_capacity,
    std::size_t *diagnostic_required) noexcept;

} // namespace ams_mel::internal
