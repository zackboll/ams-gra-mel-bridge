#pragma once

// Test-only, process-independent sink: no DSO-owned stream or static owner.
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

namespace ams_mel_test_timeline {
static inline std::int64_t now() noexcept
{
    timespec value{};
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) return -1;
    return static_cast<std::int64_t>(value.tv_sec) * 1000000000 + value.tv_nsec;
}
static inline void record(const char *event, int family = -1) noexcept
{
    const char *path = std::getenv("AMS_MEL_TEST_TIMELINE");
    if (!path) return;
    const auto timestamp = now();
    if (timestamp < 0) return;
    char line[192];
    const int size = std::snprintf(line, sizeof line, "%lld %s %d\n",
        static_cast<long long>(timestamp), event, family);
    if (size <= 0 || static_cast<unsigned>(size) >= sizeof line) return;
    const int fd = open(path, O_WRONLY | O_APPEND | O_CLOEXEC);
    if (fd < 0) return;
    // One append per event. Missing/partial evidence is rejected by the reader.
    ssize_t written;
    do { written = write(fd, line, static_cast<std::size_t>(size)); }
    while (written < 0 && errno == EINTR);
    (void)close(fd);
}
}
