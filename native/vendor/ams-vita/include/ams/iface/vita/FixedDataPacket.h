#pragma once

#include "ams/iface/vita/DataPacket.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace ams {
namespace iface {
namespace vita {

// FixedDataPacket owns packet-order storage for AMS VITA signal data packets.
// The template parameter is storage capacity in bytes. The serialized packet
// length remains the packet-size field read by DataPacketView.
template <std::size_t Bytes>
class FixedDataPacket {
public:
    static_assert(Bytes % sizeof(uint32_t) == 0U, "FixedDataPacket capacity must be word-aligned");

    static constexpr std::size_t byte_capacity = Bytes;
    static constexpr std::size_t word_capacity = Bytes / sizeof(uint32_t);

    uint32_t* data() { return words_.data(); }
    const uint32_t* data() const { return words_.data(); }

    std::span<uint32_t> words() { return words_; }
    std::span<const uint32_t> words() const { return words_; }

    std::span<std::byte> bytes() { return std::as_writable_bytes(words()); }
    std::span<const std::byte> bytes() const { return std::as_bytes(words()); }

    DataPacketBuilder builder() { return DataPacketBuilder(words_.data(), words_.size()); }
    DataPacketView view() const { return DataPacketView(words()); }

private:
    std::array<uint32_t, word_capacity> words_{};
};

} // namespace vita
} // namespace iface
} // namespace ams
