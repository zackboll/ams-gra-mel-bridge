#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <arpa/inet.h>
#include <span>
#include <array>
#include "ams/iface/vita/GeneratedMasks.h"
#include "ams/iface/vita/Primitives.h"

namespace ams::iface::vita {

class DataPacketBaseView {
protected:
    uint32_t cif0 = 0;
    const uint32_t* mapGeneratedFields(const uint32_t* current, [[maybe_unused]] const uint32_t* end) {
        return current;
    }

public:
};

class DataPacketBaseBuilder {
protected:
    size_t writeGeneratedFields([[maybe_unused]] uint32_t* buffer, size_t offset, [[maybe_unused]] size_t max_words) const {
        return offset;
    }

public:
};

} // namespace ams::iface::vita
