#include <ams_mel/abi.h>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <new>

namespace { long fail_after = -1; }
void *operator new(std::size_t size)
{
    if (fail_after == 0) throw std::bad_alloc{};
    if (fail_after > 0) --fail_after;
    if (void *memory = std::malloc(size ? size : 1U)) return memory;
    throw std::bad_alloc{};
}
void *operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void *memory) noexcept { std::free(memory); }
void operator delete[](void *memory) noexcept { ::operator delete(memory); }
void operator delete(void *memory, std::size_t) noexcept { ::operator delete(memory); }
void operator delete[](void *memory, std::size_t) noexcept { ::operator delete(memory); }
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%d: %s\n", __LINE__, #x); return EXIT_FAILURE; } } while (0)
int main()
{
    ams_mel_session *session = nullptr;
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "foundation-normal", "", &session, nullptr, 0, nullptr) == AMS_MEL_OK);
    bool succeeded = false;
    for (long allocation = 0; allocation < 100; ++allocation) {
        ams_mel_ir_buffer *buffer = nullptr;
        std::uint32_t result{};
        char diagnostic[128]{};
        fail_after = allocation;
        const auto status = ams_mel_ir_buffer_open(session, 32, INT64_MIN, &buffer, &result, diagnostic, sizeof diagnostic, nullptr);
        fail_after = -1;
        if (status == AMS_MEL_OK) {
            CHECK(buffer && result == 0);
            CHECK(ams_mel_ir_buffer_close(&buffer, nullptr, 0, nullptr) == AMS_MEL_OK);
            succeeded = true;
            break;
        }
        CHECK(status == AMS_MEL_INTERNAL_ERROR && !buffer && diagnostic[0]);
    }
    CHECK(succeeded);
    CHECK(ams_mel_session_close(&session, nullptr, 0, nullptr) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}
