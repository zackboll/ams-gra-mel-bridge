#include "internal/ir_track_task.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL: %s\n", #x); std::exit(1); } } while (false)
int main()
{
    using namespace ams_mel::internal;
    ams_mel_ir_track_task_v1 input{};
    for (std::size_t i = 0; i < AMS_MEL_IR_TRACK_TASK_CAPACITY; ++i) {
        input.track_parameters.targets[i] = {
            i == 254 ? INT32_MIN : i == 253 ? INT32_MAX : -static_cast<std::int32_t>(i % 17),
            static_cast<double>(i) + 0.125, -static_cast<double>(i) - 0.25,
            static_cast<double>(i) + 0.375, -static_cast<double>(i) - 0.5};
    }
    for (auto time : std::array<std::int64_t, 5>{INT64_MIN, INT64_MAX, -1, 0, 1}) {
        for (auto id : std::array<std::uint32_t, 3>{0, 0x80000000U, UINT32_MAX}) {
            for (std::uint32_t action = 0; action < 4; ++action) {
                for (auto count : std::array<std::uint32_t, 4>{0, 1, 254, 255}) {
                    input.command_id = id;
                    input.track_parameters.count = count;
                    input.track_parameters.duration_ns = time;
                    input.task_info = {time, id, action, UINT32_MAX, 0x80000001U};
                    IRSTTrackTask task;
                    CHECK(convert_track_task(input, task));
                    CHECK(matches_track_task(input, task));
                    for (std::size_t i = 0; i < 255; ++i) {
                        auto& source = input.track_parameters.targets[i];
                        const auto target = task.getParams().getTargetData()[i];
                        CHECK(target.getTargetID() == source.target_id);
                        CHECK(target.getTargetLocation().az == source.location_azimuth_rad);
                        CHECK(target.getTargetLocation().el == source.location_elevation_rad);
                        CHECK(target.getTargetRate().az == source.rate_azimuth_rad_per_second);
                        CHECK(target.getTargetRate().el == source.rate_elevation_rad_per_second);
                    }
                }
            }
        }
    }
    /* Each component, at each stored index, independently exercises binary64
     * patterns, including signed zeros, infinities, subnormals and NaN payloads.
     * Deliberate single-field changes must fail the getter verification. */
    for (auto& source : input.track_parameters.targets) {
        for (auto field : {&source.location_azimuth_rad, &source.location_elevation_rad,
                           &source.rate_azimuth_rad_per_second, &source.rate_elevation_rad_per_second}) {
            const auto saved = *field;
            for (auto pattern : std::array<std::uint64_t, 8>{0, 0x8000000000000000ULL,
                    1, 0x8000000000000001ULL, 0x7ff0000000000000ULL,
                    0xfff0000000000000ULL, 0x7ff8123456789abcULL, 0x7ff0123456789abcULL}) {
                *field = std::bit_cast<double>(pattern);
                IRSTTrackTask task;
                CHECK(convert_track_task(input, task));
                *field = std::bit_cast<double>(pattern ^ 1U);
                CHECK(!matches_track_task(input, task));
            }
            *field = saved;
        }
    }
    IRSTTrackTask task;
    for (auto count : std::array<std::uint32_t, 3>{256, 0x80000000U, UINT32_MAX}) {
        input.track_parameters.count = count;
        CHECK(!convert_track_task(input, task));
    }
    input.track_parameters.count = 255;
    input.task_info.action = 4;
    CHECK(!convert_track_task(input, task));
    return 0;
}
