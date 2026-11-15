#include <ams_mel/abi.h>
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <new>

/* Injection belongs to the calling thread. Completion workers are independent
 * and must neither race this countdown nor receive the intended Receive failure. */
namespace { thread_local long fail_after = -1; }
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
#define ARGS nullptr, 0, nullptr
extern "C" int ams_mel_test_completion_owner_notify(unsigned, unsigned,
                                                   std::uint64_t, std::uint64_t *) noexcept;
int main()
{
    ams_mel_session *session = nullptr;
    ams_mel_ir_stream *image = nullptr;
    ams_mel_ir_image_control_config_v1 config{};
    config.channel_type = AMS_MEL_IR_CHANNEL_IRST_IMAGE;
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "checkout021-normal", "", &session, ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_control_open(session, &config, &image, ARGS) == AMS_MEL_OK);
    bool succeeded = false;
    for (long allocation = 0; allocation < 100; ++allocation) {
        ams_mel_ir_image_checkout *receiver = nullptr;
        fail_after = allocation;
        const auto status = ams_mel_ir_image_checkout_open(image, 2, 3, &receiver, ARGS);
        fail_after = -1;
        if (status == AMS_MEL_OK) {
            CHECK(receiver);
            ams_mel_ir_buffer *buffer = nullptr;
            ams_mel_ir_channel *view = nullptr;
            ams_mel_ir_return_request *request = nullptr;
            ams_mel_ir_image_checkout_lease *lease = nullptr;
            std::uint32_t result;
            CHECK(ams_mel_ir_channel_from_stream(image, &view, ARGS) == AMS_MEL_OK);
            CHECK(ams_mel_ir_buffer_open(session, 16, -20, &buffer, &result, ARGS) == AMS_MEL_OK);
            CHECK(ams_mel_ir_channel_register_buffer(view, buffer, &result, ARGS) == AMS_MEL_OK);
            CHECK(ams_mel_ir_stream_start(image, ARGS) == AMS_MEL_OK);
            CHECK(ams_mel_ir_channel_send_keepalive(view, &request, ARGS) == AMS_MEL_OK);
            fail_after = 0;
            const auto receive_status = ams_mel_ir_image_checkout_receive(receiver, 0, &lease, ARGS);
            fail_after = -1;
            CHECK(receive_status == AMS_MEL_INTERNAL_ERROR && !lease);
            CHECK(ams_mel_ir_image_checkout_receive(receiver, 0, &lease, ARGS) == AMS_MEL_OK && lease);
            CHECK(ams_mel_ir_image_checkout_lease_close(&lease, &result, ARGS) == AMS_MEL_OK);
            CHECK(ams_mel_ir_return_request_close(&request, ARGS) == AMS_MEL_OK);
            std::uint64_t observed{};
            CHECK(ams_mel_test_completion_owner_notify(1U, 0U, 1U, &observed) == 1 && observed == 1U);
            CHECK(ams_mel_ir_channel_disable(view, &result, ARGS) == AMS_MEL_OK);
            CHECK(ams_mel_ir_channel_unregister_buffer(view, buffer, &result, ARGS) == AMS_MEL_OK);
            CHECK(ams_mel_ir_buffer_close(&buffer, ARGS) == AMS_MEL_OK);
            CHECK(ams_mel_ir_channel_close(&view, ARGS) == AMS_MEL_OK);
            CHECK(ams_mel_ir_image_checkout_close(&receiver, ARGS) == AMS_MEL_OK);
            succeeded = true;
            break;
        }
        CHECK(status == AMS_MEL_INTERNAL_ERROR && !receiver);
    }
    CHECK(succeeded);
    CHECK(ams_mel_ir_stream_close(&image, ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, ARGS) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}
