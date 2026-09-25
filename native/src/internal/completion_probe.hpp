#pragma once

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
#include "../../tests/lifecycle_timeline.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <condition_variable>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <chrono>
#include <cstdio>

namespace ams_mel_test_completion {
enum Family : unsigned { Mode, Return, Comms, Navigation, Instrumentation, Track, Count };
struct Counters {
    std::atomic<std::uint64_t> started{}, active{}, peak{}, completed{};
    std::atomic<std::uint64_t> get_entries{}, get_completions{};
};
inline std::array<Counters, Count> counters{};
inline std::array<std::atomic<std::uint64_t>, Count> owners_destroyed{};
inline std::mutex observation_mutex;
inline std::condition_variable observation_ready;
/* Per-thread evidence belongs to one healthy completion invocation. */
inline thread_local unsigned safety_mask{};
inline std::array<bool, Count> parked{};
inline std::array<std::uint64_t, Count> consumed{};
inline std::array<std::uint64_t, Count> graphs{};
inline std::array<std::atomic<std::uint64_t>, Count> owners_created{};
inline void event(Family family, const char *name) noexcept
{
    ams_mel_test_timeline::record(name, static_cast<int>(family));
    if (const char *path = std::getenv("AMS_MEL_TEST_LIFETIME_LOG")) {
        if (FILE *file = std::fopen(path, "a")) {
            std::fprintf(file, "%s_%u\n", name, static_cast<unsigned>(family));
            std::fclose(file);
        } else std::abort();
    }
}
inline void boundary(Family family, bool invalid) noexcept
{
    if (!invalid) std::abort();
    event(family, "future_consumed");
    event(family, "provider_result_scope_exited");
    safety_mask |= 3U;
    std::unique_lock lock{observation_mutex};
    ++consumed[family];
    observation_ready.notify_all();
    if (!observation_ready.wait_for(lock, std::chrono::seconds{15},
            [=] { return !parked[family]; })) std::abort();
}
inline void graph_released(Family family, bool empty) noexcept
{
    if (!empty) std::abort();
    event(family, "completion_graph_released");
    safety_mask |= 4U;
    std::lock_guard lock{observation_mutex};
    ++graphs[family];
}
inline void start(Family family) noexcept
{
    auto& c = counters[family];
    c.started.fetch_add(1, std::memory_order_relaxed);
    const auto active = c.active.fetch_add(1, std::memory_order_relaxed) + 1;
    auto peak = c.peak.load(std::memory_order_relaxed);
    while (peak < active && !c.peak.compare_exchange_weak(
        peak, active, std::memory_order_relaxed)) {}
    { std::lock_guard lock{observation_mutex}; }
    observation_ready.notify_all();
}
inline void get_entry(Family family) noexcept
{
    counters[family].get_entries.fetch_add(1, std::memory_order_relaxed);
    { std::lock_guard lock{observation_mutex}; }
    observation_ready.notify_all();
}
inline void get_done(Family family) noexcept
{ counters[family].get_completions.fetch_add(1, std::memory_order_relaxed); }
inline void finish(Family family) noexcept
{
    counters[family].active.fetch_sub(1, std::memory_order_relaxed);
    counters[family].completed.fetch_add(1, std::memory_order_release);
    { std::lock_guard lock{observation_mutex}; }
    observation_ready.notify_all();
}
struct Worker {
    Family family;
    explicit Worker(Family value) noexcept : family{value} { start(family); }
    ~Worker() { finish(family); }
};
/* Declare this as the FIRST WorkerInput field: it is destroyed last, after
 * the future and Completion. The submitting thread drops its input reference
 * after launch; only the captured thread owner remains in normal operation. */
struct FinalOwner {
    Family family;
    bool healthy_finished{};
    explicit FinalOwner(Family value) noexcept : family{value}
    { ++owners_created[family]; }
    ~FinalOwner()
    {
        if (healthy_finished) event(family, "safe_owner_destroyed");
        event(family, "completion_owner_destroyed");
        owners_destroyed[family].fetch_add(1, std::memory_order_release);
        { std::lock_guard lock{observation_mutex}; }
        observation_ready.notify_all();
    }
};
} // namespace ams_mel_test_completion
#define AMS_MEL_PROBE_OWNER(family) \
    ams_mel_test_completion::FinalOwner final_owner{ams_mel_test_completion::family};
#define AMS_MEL_PROBE_WORKER(family) \
    if (input->emergency_self || input->emergency_retained.load() || \
        !input->future.valid()) std::abort(); \
    ams_mel_test_completion::safety_mask = 0U; \
    const ams_mel_test_completion::Worker completion_probe_worker{ams_mel_test_completion::family}
#define AMS_MEL_PROBE_BOUNDARY(family) \
    ams_mel_test_completion::boundary(ams_mel_test_completion::family, !future.valid())
#define AMS_MEL_PROBE_GRAPH(family, field) \
    ams_mel_test_completion::graph_released(ams_mel_test_completion::family, !state->field)
#define AMS_MEL_PROBE_RETURN(family) \
    if (ams_mel_test_completion::safety_mask != 7U || input->future.valid() || \
        input->emergency_self || input->emergency_retained.load()) std::abort(); \
    input->final_owner.healthy_finished = true; \
    ams_mel_test_completion::event(ams_mel_test_completion::family, "worker_return_boundary")
#define AMS_MEL_PROBE_GET(family, expression) ([&]() { \
    ams_mel_test_completion::get_entry(ams_mel_test_completion::family); \
    try { auto value = (expression); \
          ams_mel_test_completion::get_done(ams_mel_test_completion::family); \
          ams_mel_test_completion::event(ams_mel_test_completion::family, "get_returned"); \
          return value; \
    } catch (...) { ams_mel_test_completion::get_done(ams_mel_test_completion::family); \
        ams_mel_test_completion::event(ams_mel_test_completion::family, "get_threw"); throw; } \
}())
#else
#define AMS_MEL_PROBE_OWNER(family)
#define AMS_MEL_PROBE_WORKER(family)
#define AMS_MEL_PROBE_BOUNDARY(family)
#define AMS_MEL_PROBE_GRAPH(family, field)
#define AMS_MEL_PROBE_RETURN(family)
#define AMS_MEL_PROBE_GET(family, expression) (expression)
#endif
