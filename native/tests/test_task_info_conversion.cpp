#include "internal/ir_task_info.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL: %s\n", #x); std::exit(1); } } while (false)
int main()
{
    using namespace ams_mel::internal;
    for (const auto time : std::array<std::int64_t, 4>{INT64_MIN, INT64_MAX, 0, -987654321}) {
        for (std::uint32_t action = 0; action != 4; ++action) {
            const ams_mel_ir_task_info_v1 input{time, 0xf1234567U, action, UINT32_MAX, 0x80000001U};
            TaskInfo result;
            CHECK(convert_task_info(input, result));
            CHECK(result.getSystemTime().count() == time);
            CHECK(result.getReqId() == 0xf1234567U);
            CHECK(static_cast<std::uint32_t>(result.getAction()) == action);
            CHECK(result.getPrecedenceWithinPriority() == UINT32_MAX);
            CHECK(result.getPriority() == 0x80000001U);
        }
    }
    TaskInfo result;
    CHECK(!convert_task_info({0, 0, 4, 0, 0}, result));
    CHECK(!convert_task_info({0, 0, UINT32_MAX, 0, 0}, result));
    return 0;
}
