#include "internal/completion_probe.hpp"

#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
#include <chrono>
/* op 0 parks a family, 1 waits for post-get/pre-cleanup arrivals, 2 releases,
 * 3 snapshots resource and safety totals. No production symbol is emitted. */
extern "C" __attribute__((visibility("default"))) int ams_mel_test_completion_boundary(
    unsigned family, unsigned operation, std::uint64_t target, std::uint64_t *values) noexcept
{
    using namespace ams_mel_test_completion;
    if (family >= Count || operation > 3U) return 0;
    std::unique_lock lock{observation_mutex};
    if (operation == 0U) parked[family] = true;
    if (operation == 1U && !observation_ready.wait_for(lock, std::chrono::seconds{15},
            [=] { return consumed[family] >= target; })) return 0;
    if (operation == 2U) { parked[family] = false; observation_ready.notify_all(); }
    if (values) {
        values[0] = consumed[family]; values[1] = graphs[family];
        values[2] = owners_created[family].load();
        values[3] = owners_destroyed[family].load();
    }
    return 1;
}
extern "C" __attribute__((visibility("default"))) int ams_mel_test_completion_snapshot(
    unsigned family, std::uint64_t *values) noexcept
{
    if (!values || family >= ams_mel_test_completion::Count) return 0;
    const auto& c = ams_mel_test_completion::counters[family];
    values[0] = c.started.load(); values[1] = c.active.load();
    values[2] = c.peak.load(); values[3] = c.completed.load();
    values[4] = c.get_entries.load(); values[5] = c.get_completions.load();
    return 1;
}

extern "C" __attribute__((visibility("default"))) int ams_mel_test_completion_wait(
    unsigned family, unsigned field, std::uint64_t target) noexcept
{
    if (family >= ams_mel_test_completion::Count || field > 5U) return 0;
    auto& c = ams_mel_test_completion::counters[family];
    const std::atomic<std::uint64_t> *values[] = {
        &c.started, &c.active, &c.peak, &c.completed, &c.get_entries, &c.get_completions};
    std::unique_lock lock{ams_mel_test_completion::observation_mutex};
    return ams_mel_test_completion::observation_ready.wait_for(
        lock, std::chrono::seconds{15}, [&] { return values[field]->load() >= target; });
}
extern "C" __attribute__((visibility("default"))) int ams_mel_test_completion_owner(
    unsigned family, std::uint64_t target, std::uint64_t *observed) noexcept
{
    if (family >= ams_mel_test_completion::Count || !observed) return 0;
    std::unique_lock lock{ams_mel_test_completion::observation_mutex};
    const bool ready = ams_mel_test_completion::observation_ready.wait_for(
        lock, std::chrono::seconds{15}, [&] {
            return ams_mel_test_completion::owners_destroyed[family].load(
                std::memory_order_acquire) >= target;
        });
    *observed = ams_mel_test_completion::owners_destroyed[family].load(
        std::memory_order_acquire);
    return ready ? 1 : 0;
}
#endif
