#define _POSIX_C_SOURCE 200809L

#include <ams_mel/abi.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <unistd.h>

#ifndef AMS_MEL_TEST_MOCK_PROVIDER
#error "mock provider path is required"
#endif

namespace {

char lifetime_directory[] = "/tmp/ams-mel-diagnostic-allocation-XXXXXX";
char lifetime_log[sizeof lifetime_directory + sizeof "/lifetime.log"];

void cleanup_lifetime_log()
{
    (void)std::remove(lifetime_log);
    (void)rmdir(lifetime_directory);
}

bool setup_lifetime_log()
{
    return mkdtemp(lifetime_directory) != nullptr &&
           std::snprintf(lifetime_log, sizeof lifetime_log, "%s/lifetime.log",
                         lifetime_directory) >= 0 &&
           setenv("AMS_MEL_TEST_LIFETIME_LOG", lifetime_log, 1) == 0 &&
           std::atexit(cleanup_lifetime_log) == 0;
}

} // namespace

void *operator new(std::size_t size)
{
    if (std::getenv("AMS_MEL_TEST_FAIL_ALLOCATION") != nullptr) {
        throw std::bad_alloc{};
    }
    if (void *const memory = std::malloc(size)) {
        return memory;
    }
    throw std::bad_alloc{};
}

void *operator new[](std::size_t size)
{
    return ::operator new(size);
}

void operator delete(void *memory) noexcept
{
    std::free(memory);
}

void operator delete[](void *memory) noexcept
{
    ::operator delete(memory);
}

void operator delete(void *memory, std::size_t) noexcept
{
    ::operator delete(memory);
}

void operator delete[](void *memory, std::size_t) noexcept
{
    ::operator delete(memory);
}

int main()
{
    if (!setup_lifetime_log()) {
        cleanup_lifetime_log();
        return EXIT_FAILURE;
    }
    ams_mel_session *session = nullptr;
    char diagnostic[128]{};
    std::size_t required = 0;
    if (ams_mel_session_open(
            AMS_MEL_TEST_MOCK_PROVIDER, "fail-diagnostic-allocation", "",
            &session, diagnostic, sizeof diagnostic, &required) !=
            AMS_MEL_PROVIDER_EXCEPTION ||
        session != nullptr ||
        std::strcmp(diagnostic,
                    "provider exception text intentionally longer than "
                    "small-string optimization") != 0 ||
        required != std::strlen(diagnostic) + 1U) {
        return EXIT_FAILURE;
    }

    if (ams_mel_session_open(
            AMS_MEL_TEST_MOCK_PROVIDER, "fail-diagnostic-allocation", "",
            &session, nullptr, 0, nullptr) != AMS_MEL_PROVIDER_EXCEPTION ||
        session != nullptr) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
