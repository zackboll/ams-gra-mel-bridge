#include "internal/ir_stare_task.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL: %s\n", #x); std::exit(1); } } while (false)
int main()
{
    using namespace ams_mel::internal;
    for (const auto time : std::array<std::int64_t, 4>{INT64_MIN, INT64_MAX, 0, -987654321}) {
        for (const auto id : std::array<std::uint32_t, 3>{0, 0x80000000U, UINT32_MAX}) {
            for (std::uint32_t action = 0; action != 4; ++action) {
                for (std::uint32_t el = 0; el != 2; ++el) {
                    for (std::uint32_t az = 0; az != 2; ++az) {
                        const ams_mel_ir_stare_task_v1 input{-0.0, 1.23456789012345,
                            time, el, az, id, {time, id, action, UINT32_MAX, 0x80000001U}};
                        IRSTStareTask task;
                        CHECK(convert_stare(input, task));
                        CHECK(matches_stare(input, task));
                        CHECK(std::bit_cast<std::uint64_t>(task.getParams().getCenter().az) ==
                              std::bit_cast<std::uint64_t>(-0.0));
                    }
                }
            }
        }
    }
    IRSTStareTask task;
    ams_mel_ir_stare_task_v1 invalid{};
    invalid.azimuth_frame = 2;
    CHECK(!convert_stare(invalid, task));
    invalid.azimuth_frame = 0;
    invalid.elevation_frame = UINT32_MAX;
    CHECK(!convert_stare(invalid, task));
    invalid.elevation_frame = 0;
    invalid.task_info.action = 4;
    CHECK(!convert_stare(invalid, task));
    return 0;
}
