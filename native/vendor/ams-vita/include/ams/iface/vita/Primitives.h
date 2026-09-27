#pragma once

#include <cstdint>
#include <cstddef>
#include <optional>
#include <arpa/inet.h>
#include <span>
#include <iterator>

namespace ams {
namespace iface {
namespace vita {


namespace masks {
    // Header (Word 1) Masks
    constexpr uint32_t PKT_TYPE_MASK   = 0xF0000000;
    constexpr uint32_t PKT_TYPE_SHIFT  = 28;
    constexpr uint32_t CBIT_MASK       = 0x08000000;
    
    // Bit 26 Masks (Packet-type specific)
    constexpr uint32_t DATA_TRAILER_MASK = 0x04000000;
    constexpr uint32_t CTX_TSM_MASK      = 0x04000000;
    constexpr uint32_t CMD_ACK_MASK      = 0x04000000;

    // Bit 25 Masks (Packet-type specific)
    constexpr uint32_t ND0_MASK          = 0x02000000;
    constexpr uint32_t CMD_CANC_MASK     = 0x02000000;

    // Bit 24 Masks (Packet-type specific)
    constexpr uint32_t SBIT_MASK         = 0x01000000;
    
    constexpr uint32_t TSI_MASK        = 0x00C00000;
    constexpr uint32_t TSI_SHIFT       = 22;
    constexpr uint32_t TSF_MASK        = 0x00300000;
    constexpr uint32_t TSF_SHIFT       = 20;
    constexpr uint32_t PKT_COUNT_MASK  = 0x000F0000;
    constexpr uint32_t PKT_COUNT_SHIFT = 16;
    constexpr uint32_t PKT_SIZE_MASK   = 0x0000FFFF;

    // Data Address Structure Header Masks
    constexpr uint32_t DAS_WORD2_RECORD_SIZE_MASK  = 0x00FFF000;
    constexpr uint32_t DAS_WORD2_RECORD_SIZE_SHIFT = 12;
    constexpr uint32_t DAS_WORD2_NUM_RECORDS_MASK  = 0x00000FFF;
    
    constexpr uint32_t DAS_WORD3_HAS_SID_MASK      = 0x80000000;
    constexpr uint32_t DAS_WORD3_HAS_ADDRESS_MASK  = 0x40000000;

    // Pointing3d Structure Header Masks
    constexpr uint32_t P3D_WORD2_HEADER_SIZE_MASK  = 0xFF000000;
    constexpr uint32_t P3D_WORD2_HEADER_SIZE_SHIFT = 24;
    constexpr uint32_t P3D_WORD2_RECORD_SIZE_MASK  = 0x00FFF000;
    constexpr uint32_t P3D_WORD2_RECORD_SIZE_SHIFT = 12;
    constexpr uint32_t P3D_WORD2_NUM_RECORDS_MASK  = 0x00000FFF;
    
    constexpr uint32_t P3D_WORD3_HAS_IRB_MASK      = 0x80000000;
    constexpr uint32_t P3D_WORD3_HAS_VECTOR_MASK   = 0x40000000;

    // CAM (Control/Ack Mode) Masks
    constexpr uint32_t CAM_CE_MASK          = 0x80000000;
    constexpr uint32_t CAM_IE_MASK          = 0x40000000;
    constexpr uint32_t CAM_CR_MASK          = 0x20000000;
    constexpr uint32_t CAM_IR_MASK          = 0x10000000;
    constexpr uint32_t CAM_P_MASK           = 0x08000000;
    constexpr uint32_t CAM_W_MASK           = 0x04000000;
    constexpr uint32_t CAM_ER_MASK          = 0x02000000;
    constexpr uint32_t CAM_ACTION_MASK      = 0x01800000;
    constexpr uint32_t CAM_ACTION_SHIFT     = 23;
    constexpr uint32_t CAM_NACK_MASK        = 0x00400000;
    constexpr uint32_t CAM_REQV_MASK        = 0x00100000;
    constexpr uint32_t CAM_REQX_MASK        = 0x00080000;
    constexpr uint32_t CAM_REQS_MASK        = 0x00040000;
    constexpr uint32_t CAM_REQW_MASK        = 0x00020000;
    constexpr uint32_t CAM_REQER_MASK       = 0x00010000;
    constexpr uint32_t CAM_REQR_MASK        = 0x00008000;
    constexpr uint32_t CAM_TIMING_CTRL_MASK = 0x00007000;
    constexpr uint32_t CAM_TIMING_CTRL_SHIFT= 12;
    constexpr uint32_t CAM_ACK_BITS_MASK    = 0x00000F00;
    constexpr uint32_t CAM_ACK_BITS_SHIFT   = 8;
    constexpr uint32_t CAM_SCH_REQ_TYP_MASK = 0x000000F0;
    constexpr uint32_t CAM_SCH_REQ_TYP_SHIFT= 4;
    constexpr uint32_t CAM_REQ_STAT_CH_MASK = 0x00000008;

    constexpr uint32_t CAM_ACKV_MASK        = 0x00100000;
    constexpr uint32_t CAM_ACKX_MASK        = 0x00080000;
    constexpr uint32_t CAM_ACKS_MASK        = 0x00040000;
    constexpr uint32_t CAM_ACKW_MASK        = 0x00020000;
    constexpr uint32_t CAM_ACKER_MASK       = 0x00010000;
    constexpr uint32_t CAM_ACKR_MASK        = 0x00008000;
    constexpr uint32_t CAM_ACKP_MASK        = 0x00000800;
    constexpr uint32_t CAM_SCHX_MASK        = 0x00000400;
} // namespace masks

enum class InfoClassCodeType : uint8_t {
    TxCommBaseSet = 0x04,
    RxCommBaseSet = 0x05
};

inline uint32_t makeAmsClassIdCodes(InfoClassCodeType info_class_type, uint8_t packet_class_type) {
    return (static_cast<uint32_t>(info_class_type) << 24) |
           (static_cast<uint32_t>(packet_class_type) << 8) |
           0x05U;
}

inline bool isAmsClassIdCodes(uint32_t codes, uint8_t packet_class_type) {
    const uint32_t info_class_type = (codes >> 24) & 0xFFU;
    const uint32_t info_class_version = (codes >> 16) & 0xFFU;
    const uint32_t packet_type = (codes >> 8) & 0xFFU;
    const uint32_t packet_version = codes & 0xFFU;

    return (info_class_type == static_cast<uint32_t>(InfoClassCodeType::TxCommBaseSet) ||
            info_class_type == static_cast<uint32_t>(InfoClassCodeType::RxCommBaseSet)) &&
           info_class_version == 0U &&
           packet_type == static_cast<uint32_t>(packet_class_type) &&
           packet_version == 0x05U;
}

enum class DataFormat : uint64_t {
    Complex16BitSigned = 0x200003CF00000000ULL,
    Complex8BitSigned = 0xA00001C700000000ULL
};

inline bool isAmsDataFormat(uint64_t value) {
    return value == static_cast<uint64_t>(DataFormat::Complex16BitSigned) ||
           value == static_cast<uint64_t>(DataFormat::Complex8BitSigned);
}

inline std::optional<DataFormat> parseAmsDataFormat(uint64_t value) {
    if (!isAmsDataFormat(value)) {
        return std::nullopt;
    }
    return static_cast<DataFormat>(value);
}

enum class AckXErrorSubject : uint8_t {
    NoCorrespondingPayloadField,
    Bandwidth,
    RfRefFreq,
    Gain,
    SampleRate,
    DataFormat,
    Polarization,
    Pointing3d,
    BeamWidth,
    FuncPriorityId,
    Dwell,
    RfFigureOfMerit,
    AddressGroupIndex,
    TxDigitalInputPower
};

namespace ackx_error_flags {
    constexpr uint32_t NotExecuted = 1U << 31;
    constexpr uint32_t DeviceFailure = 1U << 30;
    constexpr uint32_t ErroneousField = 1U << 29;
    constexpr uint32_t OutOfRange = 1U << 28;
    constexpr uint32_t UnsupportedPrecision = 1U << 27;
    constexpr uint32_t InvalidValue = 1U << 26;
    constexpr uint32_t BadTimestamp = 1U << 25;
    constexpr uint32_t HazardousPowerLevels = 1U << 24;
    constexpr uint32_t Distortion = 1U << 23;
    constexpr uint32_t InBandPowerCompliance = 1U << 22;
    constexpr uint32_t OutOfBandPowerCompliance = 1U << 21;
    constexpr uint32_t CoSiteInterference = 1U << 20;
    constexpr uint32_t RegionalInterference = 1U << 19;
} // namespace ackx_error_flags

struct AckXErrorPayload {
    AckXErrorSubject subject;
    uint32_t error_flags;
    uint16_t error_enum_index;
};

struct DataAddressRecord {
    std::optional<uint32_t> sid;
    uint32_t mfp_address_index;
};

struct Pointing3dRecord {
    std::optional<uint32_t> index_ref_beam;
    uint32_t pointing3d;
};

class DataAddressStructureView {
private:
    const uint32_t* ptr;
    size_t num_records;
    bool has_sid;
    bool valid;

public:
    explicit DataAddressStructureView(std::span<const uint32_t> data) : ptr(nullptr), num_records(0), has_sid(false), valid(false) {
        if (data.size() < 3) {
            return;
        }

        const uint32_t total_words = ntohl(data[0]);
        const uint32_t word2 = ntohl(data[1]);
        const uint32_t word3 = ntohl(data[2]);
        const uint32_t header_size = (word2 >> 24) & 0xFFU;
        const uint32_t words_per_record = (word2 & masks::DAS_WORD2_RECORD_SIZE_MASK) >> masks::DAS_WORD2_RECORD_SIZE_SHIFT;
        const uint32_t records = word2 & masks::DAS_WORD2_NUM_RECORDS_MASK;

        if (total_words != data.size() || header_size != 0U || records == 0U) {
            return;
        }
        if (words_per_record != 1U && words_per_record != 2U) {
            return;
        }
        if ((word3 & masks::DAS_WORD3_HAS_ADDRESS_MASK) == 0U || (word3 & 0x3FFF'FFFFU) != 0U) {
            return;
        }

        const bool sid_present = (word3 & masks::DAS_WORD3_HAS_SID_MASK) != 0U;
        if ((sid_present && words_per_record != 2U) || (!sid_present && words_per_record != 1U)) {
            return;
        }

        const size_t expected_words = 3U + (static_cast<size_t>(records) * static_cast<size_t>(words_per_record));
        if (expected_words != data.size()) {
            return;
        }

        ptr = data.data();
        num_records = records;
        has_sid = sid_present;
        valid = true;
    }

    inline bool isValid() const { return valid; }

    class Iterator {
    private:
        const uint32_t* payload;
        size_t index;
        bool has_sid;

    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = DataAddressRecord;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = DataAddressRecord;

        Iterator(const uint32_t* p, bool sid_present, size_t idx)
            : payload(p), index(idx), has_sid(sid_present) {}

        bool operator!=(const Iterator& other) const { return index != other.index; }
        
        Iterator& operator++() {
            payload += has_sid ? 2 : 1;
            index++;
            return *this;
        }

        DataAddressRecord operator*() const {
            DataAddressRecord rec;
            const uint32_t* current = payload;
            if (has_sid) {
                rec.sid = ntohl(*current++);
            }
            rec.mfp_address_index = ntohl(*current);
            return rec;
        }
    };

    Iterator begin() const { return Iterator(ptr ? ptr + 3 : nullptr, has_sid, 0); }
    Iterator end() const { return Iterator(ptr ? ptr + 3 + (num_records * (has_sid ? 2 : 1)) : nullptr, has_sid, num_records); }
    size_t size() const { return num_records; }
};

class Pointing3dStructureView {
private:
    const uint32_t* ptr;
    size_t num_records;
    bool has_irb;
    size_t header_size;
    std::optional<uint32_t> global_irb;
    bool valid;

public:
    explicit Pointing3dStructureView(std::span<const uint32_t> data) : ptr(nullptr), num_records(0), has_irb(false), header_size(3), valid(false) {
        if (data.size() < 3) {
            return;
        }

        const uint32_t total_words = ntohl(data[0]);
        const uint32_t word2 = ntohl(data[1]);
        const uint32_t word3 = ntohl(data[2]);
        const uint32_t hdr_size = (word2 & masks::P3D_WORD2_HEADER_SIZE_MASK) >> masks::P3D_WORD2_HEADER_SIZE_SHIFT;
        const uint32_t words_per_record = (word2 & masks::P3D_WORD2_RECORD_SIZE_MASK) >> masks::P3D_WORD2_RECORD_SIZE_SHIFT;
        const uint32_t records = word2 & masks::P3D_WORD2_NUM_RECORDS_MASK;
        const bool irb_present = (word3 & masks::P3D_WORD3_HAS_IRB_MASK) != 0U;

        if (total_words != data.size() || records == 0U || (hdr_size != 3U && hdr_size != 4U)) {
            return;
        }
        if ((word3 & masks::P3D_WORD3_HAS_VECTOR_MASK) == 0U || (word3 & 0x3FFF'FFFFU) != 0U) {
            return;
        }
        if ((irb_present && words_per_record != 2U) || (!irb_present && words_per_record != 1U)) {
            return;
        }

        const size_t expected_words = static_cast<size_t>(hdr_size) + (static_cast<size_t>(records) * static_cast<size_t>(words_per_record));
        if (expected_words != data.size()) {
            return;
        }

        ptr = data.data();
        num_records = records;
        has_irb = irb_present;
        header_size = hdr_size;
        if (header_size == 4U) {
            global_irb = ntohl(ptr[3]);
        }
        valid = true;
    }

    inline bool isValid() const { return valid; }

    std::optional<uint32_t> getGlobalIndexRefBeam() const { return global_irb; }

    class Iterator {
    private:
        const uint32_t* payload;
        size_t index;
        bool has_irb;

    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = Pointing3dRecord;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = Pointing3dRecord;

        Iterator(const uint32_t* p, bool irb_present, size_t idx)
            : payload(p), index(idx), has_irb(irb_present) {}

        bool operator!=(const Iterator& other) const { return index != other.index; }
        
        Iterator& operator++() {
            payload += has_irb ? 2 : 1;
            index++;
            return *this;
        }

        Pointing3dRecord operator*() const {
            Pointing3dRecord rec;
            const uint32_t* current = payload;
            if (has_irb) {
                rec.index_ref_beam = ntohl(*current++);
            }
            rec.pointing3d = ntohl(*current);
            return rec;
        }
    };

    Iterator begin() const { return Iterator(ptr ? ptr + header_size : nullptr, has_irb, 0); }
    Iterator end() const { return Iterator(ptr ? ptr + header_size + (num_records * (has_irb ? 2 : 1)) : nullptr, has_irb, num_records); }
    size_t size() const { return num_records; }
};


} // namespace vita
} // namespace iface
} // namespace ams
