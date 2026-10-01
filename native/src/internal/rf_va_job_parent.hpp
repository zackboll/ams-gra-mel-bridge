#pragma once
#include "rf_c2.hpp"
#include <rfmel/c2/VirtualAperture.h>
#include <memory>

/* Private, externally serialized with Close on this public VA owner. */
bool rf_va_acquire_job_parent(
    const ams_mel_rf_virtual_aperture *owner,
    std::shared_ptr<ams::iface::rfmel::VirtualAperture>& provider,
    ams_mel::internal::RfC2ChildClaim& claim) noexcept;