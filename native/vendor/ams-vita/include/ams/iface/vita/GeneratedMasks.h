#pragma once

#include <cstdint>

namespace ams::iface::vita::masks {
    // CIF Enables
    constexpr uint32_t CIF0_CIF7_ENABLE = 1U << 7;
    constexpr uint32_t CIF0_CIF4_ENABLE = 1U << 4;
    constexpr uint32_t CIF0_CIF3_ENABLE = 1U << 3;
    constexpr uint32_t CIF0_CIF2_ENABLE = 1U << 2;
    constexpr uint32_t CIF0_CIF1_ENABLE = 1U << 1;

    constexpr uint32_t CIF0_BANDWIDTH_MASK            = 1U << 29;
    constexpr uint32_t CIF0_RFREFFREQ_MASK            = 1U << 27;
    constexpr uint32_t CIF0_GAIN_MASK                 = 1U << 23;
    constexpr uint32_t CIF0_SAMPLERATE_MASK           = 1U << 21;
    constexpr uint32_t CIF0_DATAFORMAT_MASK           = 1U << 15;
    constexpr uint32_t CONTROLSCHEDULEREQUESTPACKET_CIF0_B_MASK = 0xA8A0801E;
    constexpr uint32_t CONTROLSCHEDULEREQUESTPACKET_CIF0_N_MASK = 0x575F7FE1;
    constexpr uint32_t CIF1_POLARIZATION_MASK         = 1U << 30;
    constexpr uint32_t CIF1_POINTING3D_MASK           = 1U << 29;
    constexpr uint32_t CIF1_POINTING3DSTRUCT_MASK     = 1U << 28;
    constexpr uint32_t CIF1_BEAMWIDTH_MASK            = 1U << 25;
    constexpr uint32_t CONTROLSCHEDULEREQUESTPACKET_CIF1_B_MASK = 0x62000000;
    constexpr uint32_t CONTROLSCHEDULEREQUESTPACKET_CIF1_N_MASK = 0x8DFFFFFF;
    constexpr uint32_t CIF2_CITEDMSGID_MASK           = 1U << 26;
    constexpr uint32_t CIF2_INFOSOURCE_MASK           = 1U << 21;
    constexpr uint32_t CIF2_MODEID_MASK               = 1U << 8;
    constexpr uint32_t CIF2_FUNCPRIORITYID_MASK       = 1U << 6;
    constexpr uint32_t CONTROLSCHEDULEREQUESTPACKET_CIF2_B_MASK = 0x00000040;
    constexpr uint32_t CONTROLSCHEDULEREQUESTPACKET_CIF2_N_MASK = 0xFBDFFEBF;
    constexpr uint32_t CIF3_DWELL_MASK                = 1U << 21;
    constexpr uint32_t CIF3_JITTER_MASK               = 1U << 20;
    constexpr uint32_t CONTROLSCHEDULEREQUESTPACKET_CIF3_B_MASK = 0x00200000;
    constexpr uint32_t CONTROLSCHEDULEREQUESTPACKET_CIF3_N_MASK = 0xFFCFFFFF;
    constexpr uint32_t CIF4_RFFIGUREOFMERIT_MASK      = 1U << 29;
    constexpr uint32_t CIF4_ADDRESSGROUPINDEX_MASK    = 1U << 24;
    constexpr uint32_t CIF4_TXDIGITALINPUTPOWER_MASK  = 1U << 21;
    constexpr uint32_t CONTROLSCHEDULEREQUESTPACKET_CIF4_B_MASK = 0x21200000;
    constexpr uint32_t CONTROLSCHEDULEREQUESTPACKET_CIF4_N_MASK = 0xDEDFFFFF;
    constexpr uint32_t CONTROLSCHEDULEREQUESTPACKET_CIF7_B_MASK = 0x00000000;
    constexpr uint32_t CONTROLSCHEDULEREQUESTPACKET_CIF7_N_MASK = 0xFFFFFFFF;
    constexpr uint32_t SCHEDULEACKACKRPACKET_CIF0_B_MASK = 0x00000014;
    constexpr uint32_t SCHEDULEACKACKRPACKET_CIF0_N_MASK = 0xFFFFFFEB;
    constexpr uint32_t SCHEDULEACKACKRPACKET_CIF1_B_MASK = 0x00000000;
    constexpr uint32_t SCHEDULEACKACKRPACKET_CIF1_N_MASK = 0xFFFFFFFF;
    constexpr uint32_t CIF2_CITEDSID_MASK             = 1U << 30;
    constexpr uint32_t SCHEDULEACKACKRPACKET_CIF2_B_MASK = 0x40000000;
    constexpr uint32_t SCHEDULEACKACKRPACKET_CIF2_N_MASK = 0xBFFFFFFF;
    constexpr uint32_t SCHEDULEACKACKRPACKET_CIF3_B_MASK = 0x00000000;
    constexpr uint32_t SCHEDULEACKACKRPACKET_CIF3_N_MASK = 0xFFFFFFFF;
    constexpr uint32_t CIF4_REJECTREASON_MASK         = 1U << 26;
    constexpr uint32_t SCHEDULEACKACKRPACKET_CIF4_B_MASK = 0x05000000;
    constexpr uint32_t SCHEDULEACKACKRPACKET_CIF4_N_MASK = 0xFAFFFFFF;
    constexpr uint32_t SCHEDULEACKACKRPACKET_CIF7_B_MASK = 0x00000000;
    constexpr uint32_t SCHEDULEACKACKRPACKET_CIF7_N_MASK = 0xFFFFFFFF;
    constexpr uint32_t EXTENSIONDATACONTEXTPACKET_CIF0_B_MASK = 0xA8A0801E;
    constexpr uint32_t EXTENSIONDATACONTEXTPACKET_CIF0_N_MASK = 0x575F7FE1;
    constexpr uint32_t CIF1_PHASEOFFSET_MASK          = 1U << 31;
    constexpr uint32_t EXTENSIONDATACONTEXTPACKET_CIF1_B_MASK = 0x62000000;
    constexpr uint32_t EXTENSIONDATACONTEXTPACKET_CIF1_N_MASK = 0x1DFFFFFF;
    constexpr uint32_t EXTENSIONDATACONTEXTPACKET_CIF2_B_MASK = 0x00000040;
    constexpr uint32_t EXTENSIONDATACONTEXTPACKET_CIF2_N_MASK = 0xFFFFFFBF;
    constexpr uint32_t EXTENSIONDATACONTEXTPACKET_CIF3_B_MASK = 0x00200000;
    constexpr uint32_t EXTENSIONDATACONTEXTPACKET_CIF3_N_MASK = 0xFFDFFFFF;
    constexpr uint32_t CIF4_EARLYSTARTTIME_MASK       = 1U << 28;
    constexpr uint32_t CIF4_LATESTARTTIME_MASK        = 1U << 27;
    constexpr uint32_t CIF4_MAXDATAPACKETDWELL_MASK   = 1U << 25;
    constexpr uint32_t EXTENSIONDATACONTEXTPACKET_CIF4_B_MASK = 0x21000000;
    constexpr uint32_t EXTENSIONDATACONTEXTPACKET_CIF4_N_MASK = 0xC0DFFFFF;
    constexpr uint32_t EXTENSIONDATACONTEXTPACKET_CIF7_B_MASK = 0x00000000;
    constexpr uint32_t EXTENSIONDATACONTEXTPACKET_CIF7_N_MASK = 0xFFFFFFFF;
} // namespace ams::iface::vita::masks
