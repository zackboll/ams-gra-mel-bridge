#include "internal/ir_scan_task.hpp"
#include "scan_performance_fixture.h"
#include <array>
#include <cstdio>
#include <cstdlib>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL: %s\n", #x); std::exit(1); } } while (false)
int main()
{
    using namespace ams_mel::internal;
    for (unsigned fixture = 0; fixture < 14; ++fixture) {
        for (unsigned side = 0; side < 2; ++side) {
            const auto input = performance_param(fixture, side);
            const auto upstream = convert_scan(input);
            CHECK(matches_scan(input, upstream));
            const auto received = snapshot_scan(upstream);
            CHECK(valid_scan(received));
            CHECK(matches_scan(received, upstream));
            CHECK(matches_scan(input, convert_scan(received)));
        }
    }
    for (const auto time : std::array<std::int64_t, 4>{INT64_MIN, INT64_MAX, 0, -987654321}) {
        for (const auto id : std::array<std::uint32_t, 3>{0, 0x80000000U, UINT32_MAX}) {
            for (std::uint32_t action = 0; action != 4; ++action) {
                for (std::uint32_t el = 0; el != 2; ++el) {
                    for (std::uint32_t az = 0; az != 2; ++az) {
                        for (std::uint32_t degradation = 0; degradation != 4; ++degradation) {
                            for (std::uint32_t range = 0; range != 2; ++range) {
                                ams_mel_ir_scan_task_v1 input{
                                    {range, -0.0, 1.23456789012345, el, az, -2.34567890123456,
                                     0.0, {id, UINT32_MAX, 0x80000001U}, id,
                                     -3.45678901234567, 4.56789012345678, -5.67890123456789,
                                     id, UINT32_MAX, 0x80000001U, id, degradation},
                                    {time, id, action, UINT32_MAX, 0x80000001U}, id};
                                IRSTScanTask task;
                                CHECK(convert_scan_task(input, task));
                                CHECK(matches_scan_task(input, task));
                                /* C2 and Scheduling use precisely the same conversion. */
                                CHECK(matches_scan(input.scan_parameters, convert_scan(input.scan_parameters)));
                                /* Every double independently carries negative zero. */
                                for (auto field : {&input.scan_parameters.center_az_rad,
                                     &input.scan_parameters.center_el_rad, &input.scan_parameters.scan_width_rad,
                                     &input.scan_parameters.scan_height_rad, &input.scan_parameters.scan_rate_rad_per_second,
                                     &input.scan_parameters.preferred_revisit_interval_seconds,
                                     &input.scan_parameters.required_revisit_interval_seconds}) {
                                    const auto saved = *field;
                                    *field = -0.0;
                                    CHECK(convert_scan_task(input, task));
                                    CHECK(matches_scan_task(input, task));
                                    *field = saved;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    IRSTScanTask task;
    for (const auto invalid : {2U, UINT32_MAX}) {
        ams_mel_ir_scan_task_v1 input{};
        input.scan_parameters.center_frame_ref_el = invalid;
        CHECK(!convert_scan_task(input, task));
        input.scan_parameters.center_frame_ref_el = 0;
        input.scan_parameters.center_frame_ref_az = invalid;
        CHECK(!convert_scan_task(input, task));
        input.scan_parameters.center_frame_ref_az = 0;
        input.scan_parameters.elevation_defined_with_range_and_altitude = invalid;
        CHECK(!convert_scan_task(input, task));
    }
    for (const auto invalid : {4U, UINT32_MAX}) {
        ams_mel_ir_scan_task_v1 input{};
        input.scan_parameters.degradation_method = invalid;
        CHECK(!convert_scan_task(input, task));
        input.scan_parameters.degradation_method = 0;
        input.task_info.action = invalid;
        CHECK(!convert_scan_task(input, task));
    }
    return 0;
}
