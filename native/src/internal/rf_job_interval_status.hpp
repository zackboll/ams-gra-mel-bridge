#pragma once
#include <ams_mel/abi.h>
#include "shared_library.hpp"
#include <rfmel/c2/JobIntervalStatus.h>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

struct ams_mel_rf_job_interval_status_event {
    ams_mel_rf_job_interval_status_v1 view{};
    std::vector<ams_mel_rf_job_event_log_entry_v1> logs;
    std::vector<std::uint8_t> activity;
};
namespace ams_mel::internal {
struct IntervalStatusState {
    std::mutex mutex;
    std::condition_variable ready;
    bool stopped{};
    ams_mel_rf_job_interval_status_options_v1 options{};
    ams_mel_rf_job_interval_status_counters_v1 counters{};
    /* Fixed-size ring: publication and permanent retention allocate nothing. */
    std::vector<std::unique_ptr<ams_mel_rf_job_interval_status_event>> queue;
    std::size_t head{}, size{};
};
struct PermanentIntervalStatusRegistration {
    std::shared_ptr<SharedLibrary> library;
    std::shared_ptr<IntervalStatusState> state;
    std::function<void(ams::iface::rfmel::JobIntervalStatus)> callback;
    PermanentIntervalStatusRegistration *next{};
};
bool valid_interval_status_options(const ams_mel_rf_job_interval_status_options_v1&) noexcept;
std::unique_ptr<PermanentIntervalStatusRegistration> prepare_interval_status(
    const ams_mel_rf_job_interval_status_options_v1&, std::shared_ptr<SharedLibrary>);
void retain_interval_status(PermanentIntervalStatusRegistration *) noexcept;
void stop_interval_status(const std::shared_ptr<IntervalStatusState>&) noexcept;
bool usable_interval_status(const std::shared_ptr<IntervalStatusState>&);
}
struct ams_mel_rf_job_interval_status {
    std::shared_ptr<ams_mel::internal::IntervalStatusState> state;
};