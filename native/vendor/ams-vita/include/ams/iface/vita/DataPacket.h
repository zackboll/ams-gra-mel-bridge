#pragma once

#include "ams/iface/vita/Primitives.h"
#include "ams/iface/vita/DataPacketBase.h"
#include <cstdint>
#include <cstddef>
#include <optional>
#include <arpa/inet.h>
#include <span>

namespace ams {
namespace iface {
namespace vita {

namespace data_trailer_detail {
constexpr uint32_t SAMPLE_FRAME_ENABLES_MASK = 0x3U << 22;
constexpr uint32_t SAMPLE_FRAME_INDICATORS_MASK = 0x3U << 10;
constexpr uint32_t ALLOWED_MASK = SAMPLE_FRAME_ENABLES_MASK | SAMPLE_FRAME_INDICATORS_MASK;
constexpr uint32_t DEFAULT_WORD = 0x3U << 22;
} // namespace data_trailer_detail

class DataTrailerView {
private:
    uint32_t word;

public:
    explicit DataTrailerView(uint32_t trailer_word) : word(ntohl(trailer_word)) {}

    bool isValidTrailer() const {
        return (word & ~data_trailer_detail::ALLOWED_MASK) == 0U;
    }

    uint8_t getSampleFrameIndicatorEnables() const { return (word >> 22) & 0x3; }
    uint8_t getSampleFrameIndicators() const { return (word >> 10) & 0x3; }
};

class DataTrailerBuilder {
private:
    uint32_t word = data_trailer_detail::DEFAULT_WORD;

public:
    DataTrailerBuilder() = default;

    void setSampleFrameIndicatorEnables(uint8_t v) {
        word = (word & ~data_trailer_detail::SAMPLE_FRAME_ENABLES_MASK) |
               ((static_cast<uint32_t>(v) & 0x3U) << 22);
    }
    void setSampleFrameIndicators(uint8_t v) {
        word = (word & ~data_trailer_detail::SAMPLE_FRAME_INDICATORS_MASK) |
               ((static_cast<uint32_t>(v) & 0x3U) << 10);
    }

    uint32_t build() const { return htonl(word); }
};

class DataPacketView : public DataPacketBaseView {
private:
    const uint32_t* buffer;
    size_t packet_words;

public:
    static inline size_t getPrologueWords() { return 7; } // 28 bytes = 7 words

    explicit DataPacketView(std::span<const uint32_t> raw_buffer) {
        if (raw_buffer.size() < getPrologueWords()) {
            buffer = nullptr;
            packet_words = 0;
            return;
        }

        uint32_t word1 = ntohl(raw_buffer[0]);
        // Enforce AMS VITA 49.2 tailoring constraints
        // Packet Type: 1 (Signal Data with Stream ID)
        if (((word1 & masks::PKT_TYPE_MASK) >> masks::PKT_TYPE_SHIFT) != 1U) {
            buffer = nullptr;
            packet_words = 0;
            return;
        }
        // C-Bit must be 1
        if ((word1 & masks::CBIT_MASK) == 0) {
            buffer = nullptr;
            packet_words = 0;
            return;
        }
        // TSI must be 3 (Other)
        if (((word1 & masks::TSI_MASK) >> masks::TSI_SHIFT) != 3U) {
            buffer = nullptr;
            packet_words = 0;
            return;
        }
        // TSF must be 2 (Picoseconds)
        if (((word1 & masks::TSF_MASK) >> masks::TSF_SHIFT) != 2U) {
            buffer = nullptr;
            packet_words = 0;
            return;
        }
        // tBit (Trailer) must be 1 per AMS GRA tailoring
        if ((word1 & masks::DATA_TRAILER_MASK) == 0) {
            buffer = nullptr;
            packet_words = 0;
            return;
        }
        // AMS data packets are V49.0-compatible signal time data packets; Nd0 and S bits must be zero.
        if ((word1 & (masks::ND0_MASK | masks::SBIT_MASK)) != 0U) {
            buffer = nullptr;
            packet_words = 0;
            return;
        }

        // Validate Fixed Metadata Class ID
        if (ntohl(raw_buffer[2]) != 0xAAAAAA) { // OUI
            buffer = nullptr;
            packet_words = 0;
            return;
        }
        if (!isAmsClassIdCodes(ntohl(raw_buffer[3]), 0x0DU)) { // Info & Pkt Class
            buffer = nullptr;
            packet_words = 0;
            return;
        }

        uint32_t pkt_size = word1 & masks::PKT_SIZE_MASK;
        
        if (pkt_size < getPrologueWords() + 1 || raw_buffer.size() < pkt_size) {
            buffer = nullptr;
            packet_words = 0;
            return;
        }
        DataTrailerView trailer(raw_buffer[pkt_size - 1]);
        if (!trailer.isValidTrailer()) {
            buffer = nullptr;
            packet_words = 0;
            return;
        }

        buffer = raw_buffer.data();
        packet_words = pkt_size;
    }

    inline bool isValid() const { return buffer != nullptr; }

    inline uint32_t getWord1() const { return buffer ? ntohl(buffer[0]) : 0; }
    inline uint32_t getStreamId() const { return buffer ? ntohl(buffer[1]) : 0; }
    inline uint32_t getClassIdOui() const { return buffer ? ntohl(buffer[2]) : 0; }
    inline uint32_t getClassIdCodes() const { return buffer ? ntohl(buffer[3]) : 0; }
    inline uint32_t getTimestampInt() const { return buffer ? ntohl(buffer[4]) : 0; }
    inline uint32_t getTimestampFracHigh() const { return buffer ? ntohl(buffer[5]) : 0; }
    inline uint32_t getTimestampFracLow() const { return buffer ? ntohl(buffer[6]) : 0; }

    inline uint8_t getPacketType() const { return (getWord1() & masks::PKT_TYPE_MASK) >> masks::PKT_TYPE_SHIFT; }
    inline bool getCBit() const { return (getWord1() & masks::CBIT_MASK) != 0; }
    inline uint8_t getTsi() const { return (getWord1() & masks::TSI_MASK) >> masks::TSI_SHIFT; }
    inline uint8_t getTsf() const { return (getWord1() & masks::TSF_MASK) >> masks::TSF_SHIFT; }
    inline uint8_t getPacketCount() const { return (getWord1() & masks::PKT_COUNT_MASK) >> masks::PKT_COUNT_SHIFT; }
    inline uint16_t getPacketSize() const { return static_cast<uint16_t>(packet_words); }
    
    inline bool hasTrailer() const { return (getWord1() & masks::DATA_TRAILER_MASK) != 0; }
    
    inline std::optional<DataTrailerView> getTrailerView() const {
        if (!hasTrailer() || packet_words < getPrologueWords() + 1) return std::nullopt;
        DataTrailerView dtv(buffer[packet_words - 1]);
        if (!dtv.isValidTrailer()) return std::nullopt;
        return dtv;
    }

    // Returns packet-order payload words without copying. Callers that need
    // host-order sample words must convert each word explicitly.
    inline std::span<const uint32_t> getPayload() const {
        size_t payload_start = getPrologueWords();
        size_t payload_end = packet_words - (hasTrailer() ? 1 : 0);
        if (payload_end > payload_start) {
            return {buffer + payload_start, payload_end - payload_start};
        }
        return {};
    }
};

class DataPacketBuilder : public DataPacketBaseBuilder {
private:
    uint32_t* buffer;
    size_t max_words;

    uint32_t word1 = 0;
    uint32_t stream_id = 0;
    uint32_t class_id_oui = 0xAAAAAA;
    uint32_t class_id_codes = makeAmsClassIdCodes(InfoClassCodeType::TxCommBaseSet, 0x0DU);
    uint32_t ts_int = 0;
    uint32_t ts_frac_hi = 0;
    uint32_t ts_frac_lo = 0;

    std::span<const uint32_t> payload;
    
    bool has_trailer = false;
    DataTrailerBuilder trailer_builder;

public:
    explicit DataPacketBuilder(uint32_t* target_buffer, size_t max_words_in) 
        : buffer(target_buffer), max_words(max_words_in) {}

    void setStreamId(uint32_t val) { stream_id = val; }
    void setTimestampInt(uint32_t val) { ts_int = val; }
    void setTimestampFracHigh(uint32_t val) { ts_frac_hi = val; }
    void setTimestampFracLow(uint32_t val) { ts_frac_lo = val; }
    void setInfoClassType(InfoClassCodeType val) { class_id_codes = makeAmsClassIdCodes(val, 0x0DU); }

    void setPacketCount(uint8_t count) { word1 = (word1 & ~masks::PKT_COUNT_MASK) | ((static_cast<uint32_t>(count) << masks::PKT_COUNT_SHIFT) & masks::PKT_COUNT_MASK); }

    // Payload words must already be in packet/network byte order. The builder
    // copies them directly into the packet without byte-order conversion.
    void setPayload(std::span<const uint32_t> payload_data) {
        payload = payload_data;
    }

    void setTrailer(const DataTrailerBuilder& builder) {
        has_trailer = true;
        trailer_builder = builder;
        word1 |= masks::DATA_TRAILER_MASK;
    }

    size_t finalize() {
        if (!has_trailer) {
            has_trailer = true;
            word1 |= masks::DATA_TRAILER_MASK;
        }

        size_t offset = 7;
        size_t trailer_words = has_trailer ? 1 : 0;
        
        if (max_words < offset + trailer_words) return 0;

        // Prevent integer overflow and check bounds against max_words
        if (payload.size() > max_words - offset - trailer_words) return 0;
        
        size_t required_words = offset + payload.size() + trailer_words;
        if (required_words > 65535U) return 0;

        // Force ams_vita_49-2_tailoring word1 prologue constraints
        word1 &= ~masks::PKT_TYPE_MASK;
        word1 |= (1U << masks::PKT_TYPE_SHIFT); // 0001
        word1 |= masks::CBIT_MASK; // 1
        word1 &= ~(masks::ND0_MASK | masks::SBIT_MASK); // .notV49p0Packet = 0, .sBit = 0
        word1 |= masks::TSI_MASK; // 11
        word1 &= ~masks::TSF_MASK;
        word1 |= (2 << masks::TSF_SHIFT); // 10
        word1 |= masks::DATA_TRAILER_MASK; // 1 (Trailer mandatory)

        buffer[0] = htonl((word1 & ~masks::PKT_SIZE_MASK) | (required_words & masks::PKT_SIZE_MASK));
        buffer[1] = htonl(stream_id);
        buffer[2] = htonl(class_id_oui);
        buffer[3] = htonl(class_id_codes);
        buffer[4] = htonl(ts_int);
        buffer[5] = htonl(ts_frac_hi);
        buffer[6] = htonl(ts_frac_lo);

        for (uint32_t p : payload) {
            buffer[offset++] = p; // Raw data payload, written directly as constructed by caller
        }

        if (has_trailer) {
            buffer[offset++] = trailer_builder.build();
        }

        return offset;
    }
};

} // namespace vita
} // namespace iface
} // namespace ams
