#include "internal/ir_prioritized_track_list.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (false)

int main()
{
    using namespace ams_mel_prioritized_tracks;
    ams_mel_ir_track_data_v1 track{};
    ams_mel_ir_prioritized_track_list_cmd_v1 input{INT64_MIN, INT64_MAX, UINT32_MAX, 0, {&track, 1}};
    PrioritizedTrackListCmd command;
    for (std::uint32_t task = 0; task <= AMS_MEL_IR_TASK_RESERVED_6; ++task) {
        track.task = task;
        for (auto member : {&track.position, &track.velocity}) {
            for (auto field : {&member->range_m, &member->azimuth_rad, &member->elevation_rad, &member->range_rate_mps}) {
                for (auto pattern : std::array<std::uint64_t, 8>{0, 0x8000000000000000ULL,
                         1, 0x8000000000000001ULL, 0x7ff0000000000000ULL, 0xfff0000000000000ULL,
                         0x7ff8123456789abcULL, 0x7ff0123456789abcULL}) {
                    *field = std::bit_cast<double>(pattern);
                    CHECK(convert(input, command) == AMS_MEL_OK);
                    CHECK(command.getTrackCount() == 0 && command.getTrackData().size() == 1);
                    *field = std::bit_cast<double>(pattern ^ 1U);
                    CHECK(!matches(track, command.getTrackData().front()));
                }
                *field = 0.0;
            }
        }
    }
    track.task = 17;
    CHECK(convert(input, command) == AMS_MEL_INVALID_ARGUMENT);
    track.task = 16;
    try {
        (void)convert(input, command, true);
        CHECK(false);
    } catch (const std::bad_alloc&) {}
    return 0;
}
