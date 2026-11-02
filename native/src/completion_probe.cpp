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

/* IR-CAMERA-CI-002 FinalOwner notification handshake; *observed always
 * receives owner_notifications[family].
 * op 0: bounded wait until all created owners have returned from their
 *       dedicated notification (not quiescence of the shared notification).
 * op 1: strict wait. Arms the family's single waiter slot and fails if the
 *       target is already reached (the waiter must block first). Success
 *       requires a wake-up that returns before the deadline with the target
 *       reached; a timeout fails even if the count arrived meanwhile.
 *       Only FinalOwner notifies this family's dedicated condition variable.
 *       Its generation advances after notify_all returns, under this mutex;
 *       spurious wakes before that cannot succeed. No shared probe activity
 *       or other family can wake this condition variable.
 * op 2: bounded wait until op 1 is armed. Armed is set and the waiter blocks
 *       under one hold of observation_mutex, so observing it proves the
 *       waiter is blocked, or is re-checking after a non-qualifying wake-up. */
extern "C" __attribute__((visibility("default"))) int ams_mel_test_completion_owner_notify(
    unsigned family, unsigned operation, std::uint64_t target, std::uint64_t *observed) noexcept
{
    using namespace ams_mel_test_completion;
    if (family >= Count || operation > 2U || !observed) return 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{15};
    std::unique_lock lock{observation_mutex};
    bool ready = false;
    if (operation == 0U) {
        ready = observation_ready.wait_until(lock, deadline, [=] {
            return owner_notifications[family] == owners_created[family].load();
        });
    } else if (operation == 2U) {
        ready = observation_ready.wait_until(lock, deadline,
                                             [=] { return owner_waiter_armed[family]; });
    } else if (!owner_waiter_armed[family] && owner_notifications[family] < target) {
        owner_waiter_armed[family] = true;
        observation_ready.notify_all();
        ready = true;
        while (owner_notifications[family] < target) {
            if (observation.owner_ready[family].wait_until(lock, deadline) == std::cv_status::timeout ||
                std::chrono::steady_clock::now() >= deadline) {
                ready = false;
                break;
            }
        }
        owner_waiter_armed[family] = false;
    }
    *observed = owner_notifications[family];
    return ready ? 1 : 0;
}
#endif
