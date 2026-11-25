#pragma once
#include <ams_mel/abi.h>
#include <string.h>

/* Rotate independently distinctive representations through every binary64
 * position. Use integer bits throughout, including quiet and signaling NaNs. */
static inline double performance_double(unsigned index)
{
    static const uint64_t bits[] = {
        UINT64_C(0), UINT64_C(0x8000000000000000), UINT64_C(0x3ff123456789abcd),
        UINT64_C(0xc004123456789abc), UINT64_C(1), UINT64_C(0x8000000000000067),
        UINT64_C(0x7ff0000000000000), UINT64_C(0xfff0000000000000),
        UINT64_C(0x7ff8000000001234), UINT64_C(0xfff8000000054321),
        UINT64_C(0x7ff0000000005678), UINT64_C(0x4009000000000000),
        UINT64_C(0x0010000000000000), UINT64_C(0xc032000000000000)
    };
    double value;
    memcpy(&value, &bits[index % 14U], sizeof value);
    return value;
}

static inline uint32_t performance_unsigned(unsigned index)
{
    static const uint32_t values[] = {0, UINT32_MAX, 0x80000000U, 17, 0x80000027U,
                                      103, 0xf0000003U, 0x7fffffffU, 8001, 123456789};
    return values[index % 10U];
}

static inline ams_mel_ir_scan_param_v1 performance_param(unsigned fixture, unsigned side)
{
    const unsigned f = fixture + 7U * side, u = fixture + 3U * side;
    ams_mel_ir_scan_param_v1 value = {
        (fixture + side) % 2U, performance_double(f), performance_double(f + 1U),
        (fixture / 2U + side) % 2U, (fixture / 4U + side) % 2U,
        performance_double(f + 2U), performance_double(f + 3U),
        {performance_unsigned(u), performance_unsigned(u + 1U), performance_unsigned(u + 2U)},
        performance_unsigned(u + 3U), performance_double(f + 4U),
        performance_double(f + 5U), performance_double(f + 6U), performance_unsigned(u + 4U),
        performance_unsigned(u + 5U), performance_unsigned(u + 6U), performance_unsigned(u + 7U),
        (fixture + 2U * side) % 4U
    };
    return value;
}