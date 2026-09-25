#include <ams_mel/abi.h>
#include "../tests/lifecycle_timeline.hpp"
#include <map>
#include <sstream>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <array>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>
#include <dlfcn.h>

extern "C" int ams_mel_test_completion_snapshot(unsigned, std::uint64_t *);
extern "C" int ams_mel_test_completion_wait(unsigned, unsigned, std::uint64_t);
extern "C" int ams_mel_test_completion_owner(unsigned, std::uint64_t, std::uint64_t *);
extern "C" int ams_mel_test_completion_boundary(unsigned, unsigned, std::uint64_t, std::uint64_t *);
using Gate = int (*)(unsigned, std::uint64_t, std::uint64_t *, std::uint64_t *);
using Clock = std::chrono::steady_clock;
struct OsState { long threads{-1}, rss{-1}, virtual_kb{-1}; };
OsState os_state();
double ms(Clock::time_point start, Clock::time_point end);

struct Timeline {
    char path[64] = "/tmp/ams-031a-timeline-XXXXXX";
    bool complete{};
    bool open() {
        const int fd = mkstemp(path);
        if (fd < 0) return false;
        close(fd);
        return setenv("AMS_MEL_TEST_TIMELINE", path, 1) == 0;
    }
    ~Timeline() {
        unsetenv("AMS_MEL_TEST_TIMELINE");
        if (complete) unlink(path);
        else std::cerr << "measurement unavailable; timeline=" << path << '\n';
    }
    bool report() {
        struct Entry { std::int64_t ns; std::string name; int family; };
        std::vector<Entry> entries;
        std::ifstream input{path};
        Entry entry{};
        while (input >> entry.ns >> entry.name >> entry.family) entries.push_back(entry);
        if (!input.eof()) return false;
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return a.ns < b.ns; });
        std::map<std::string, std::vector<std::int64_t>> events;
        std::array<unsigned, 6> gets{}, graphs{}, owners{};
        for (const auto& e : entries) {
            events[e.name].push_back(e.ns);
            if (e.family >= 0 && e.family < 6) {
                if (e.name == "get_returned") ++gets[static_cast<unsigned>(e.family)];
                if (e.name == "completion_graph_released") ++graphs[static_cast<unsigned>(e.family)];
                if (e.name == "safe_owner_destroyed") ++owners[static_cast<unsigned>(e.family)];
            }
            if (std::getenv("AMS_MEL_BENCH_DUMP_TIMELINE"))
                std::cout << "event_ns=" << e.ns << " event=" << e.name << " family=" << e.family << '\n';
        }
        constexpr std::array<unsigned, 6> expected{15, 15, 14, 14, 14, 28};
        if (gets != expected || graphs != expected || owners != expected) return false;
        for (const auto *name : {"all_workers_pending", "provider_release_begin",
                 "benchmark_handle_closed", "parent_teardown_begin", "c2_channel_destroyed",
                 "channel_destroyed", "instrumentation_channel_destroyed", "track_channel_destroyed",
                 "control_destroyed", "manager_destroyed", "library_unloaded"})
            if (events[name].size() != 1) return false;
        for (const auto *name : {"get_returned", "future_consumed", "provider_result_scope_exited",
                 "completion_graph_released", "safe_owner_destroyed", "completion_owner_destroyed"})
            if (events[name].size() != 100) return false;
        auto last = [&](const char *name) { return events.at(name).back(); };
        const auto unload = last("library_unloaded");
        if (last("benchmark_handle_closed") > last("parent_teardown_begin") ||
            last("provider_release_begin") > last("benchmark_handle_closed") ||
            last("all_workers_pending") > last("provider_release_begin")) return false;
        for (const auto *name : {"future_consumed", "provider_result_scope_exited", "completion_graph_released"})
            if (last(name) > unload) return false;
        const auto channels = std::max({last("c2_channel_destroyed"), last("channel_destroyed"),
            last("instrumentation_channel_destroyed"), last("track_channel_destroyed")});
        const auto release = last("provider_release_begin");
        const auto get = last("get_returned");
        const auto graph = last("completion_graph_released");
        const auto owner = last("completion_owner_destroyed");
        auto delta = [](std::int64_t a, std::int64_t b) { return static_cast<double>(b - a) / 1000.0; };
        std::cout << "mode=mixed requests=100 timeline=available channels=4"
            << " release_to_gets_us=" << delta(release, get)
            << " release_to_graphs_us=" << delta(release, graph)
            << " release_to_workers_us=" << delta(release, owner)
            << " release_to_channels_us=" << delta(release, channels)
            << " release_to_control_us=" << delta(release, last("control_destroyed"))
            << " release_to_manager_us=" << delta(release, last("manager_destroyed"))
            << " release_to_unload_us=" << delta(release, unload)
            << " gets_to_graphs_us=" << delta(get, graph)
            << " graphs_to_unload_us=" << delta(graph, unload)
            << " unload_to_workers_us=" << delta(unload, owner)
            << " channels_to_unload_us=" << delta(channels, unload);
        for (const auto *name : {"provider_release_begin", "benchmark_handle_closed", "parent_teardown_begin",
                 "c2_channel_destroyed", "channel_destroyed", "instrumentation_channel_destroyed",
                 "track_channel_destroyed", "control_destroyed", "manager_destroyed", "library_unloaded"})
            std::cout << ' ' << name << "_ns=" << last(name);
        std::cout << " all_gets_ns=" << get << " all_graphs_ns=" << graph << " all_workers_ns=" << owner << '\n';
        return true;
    }
};

static int mixed(Gate gate, void *&handle)
{
    constexpr std::array<unsigned, 6> counts{15, 15, 14, 14, 14, 28};
    std::array<std::array<std::uint64_t, 6>, 6> base{}, held{};
    std::array<std::uint64_t, 6> owners{};
    ams_mel_session *session{};
    ams_mel_ir_c2 *c2{};
    ams_mel_ir_stream *image{};
    ams_mel_ir_instrumentation *instr{};
    ams_mel_ir_track *track{};
    std::array<ams_mel_ir_return_request *, 15> returns{};
    std::array<ams_mel_ir_mode_request *, 15> modes{};
    std::array<ams_mel_ir_channel_comms_request *, 14> comms{};
    std::array<ams_mel_ir_navigation_request *, 14> navigation{};
    std::array<ams_mel_ir_instrumentation_request *, 14> levels{};
    std::array<ams_mel_ir_track_update_request *, 14> updates{};
    std::array<ams_mel_ir_track_system_response_request *, 14> responses{};
    struct Guard {
        Gate &gate;
        ams_mel_session *&session;
        ams_mel_ir_c2 *&c2;
        ams_mel_ir_stream *&image;
        ams_mel_ir_instrumentation *&instr;
        ams_mel_ir_track *&track;
        decltype(returns)& returns_ref;
        decltype(modes)& modes_ref;
        decltype(comms)& comms_ref;
        decltype(navigation)& navigation_ref;
        decltype(levels)& levels_ref;
        decltype(updates)& updates_ref;
        decltype(responses)& responses_ref;
        ~Guard() {
            std::uint64_t sent{}, released{};
            if (gate && gate(0, 0, &sent, &released) && sent > released)
                (void)gate(2, sent - released, nullptr, nullptr);
            for (unsigned f = 0; f < 6; ++f)
                (void)ams_mel_test_completion_boundary(f, 2, 0, nullptr);
            for (auto &r : returns_ref) if (r) (void)ams_mel_ir_return_request_close(&r, nullptr, 0, nullptr);
            for (auto &r : modes_ref) if (r) (void)ams_mel_ir_mode_request_close(&r, nullptr, 0, nullptr);
            for (auto &r : comms_ref) if (r) (void)ams_mel_ir_channel_comms_request_close(&r, nullptr, 0, nullptr);
            for (auto &r : navigation_ref) if (r) (void)ams_mel_ir_navigation_request_close(&r, nullptr, 0, nullptr);
            for (auto &r : levels_ref) if (r) (void)ams_mel_ir_instrumentation_request_close(&r, nullptr, 0, nullptr);
            for (auto &r : updates_ref) if (r) (void)ams_mel_ir_track_update_request_close(&r, nullptr, 0, nullptr);
            for (auto &r : responses_ref) if (r) (void)ams_mel_ir_track_system_response_request_close(&r, nullptr, 0, nullptr);
            if (c2) (void)ams_mel_ir_c2_close(&c2, nullptr, 0, nullptr);
            if (image) (void)ams_mel_ir_stream_close(&image, nullptr, 0, nullptr);
            if (instr) (void)ams_mel_ir_instrumentation_close(&instr, nullptr, 0, nullptr);
            if (track) (void)ams_mel_ir_track_close(&track, nullptr, 0, nullptr);
            if (session) (void)ams_mel_session_close(&session, nullptr, 0, nullptr);
            for (unsigned f = 0; f < 6; ++f) {
                std::uint64_t evidence[4]{};
                if (ams_mel_test_completion_boundary(f, 3, 0, evidence))
                    (void)ams_mel_test_completion_owner(f, evidence[2], &sent);
            }
        }
    } guard{gate, session, c2, image, instr, track, returns, modes, comms, navigation, levels, updates, responses};
    auto view = [](const char *s) { return ams_mel_string_view_v1{s, std::char_traits<char>::length(s)}; };
    ams_mel_ir_c2_config_v1 cc{};
    ams_mel_ir_stream_config_v1 ic{};
    ams_mel_ir_instrumentation_config_v1 lc{};
    ams_mel_ir_track_config_v1 tc{};
    cc.channel_type = AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL;
    ic.channel_type = AMS_MEL_IR_CHANNEL_IRST_IMAGE;
    lc.channel_type = AMS_MEL_IR_CHANNEL_INSTRUMENTATION;
    tc.channel_type = AMS_MEL_IR_CHANNEL_IRST_TRACK;
    auto fill = [&](auto& c) {
        c.channel_id.descriptive_label = view("scale");
        c.platform_id.descriptive_label = view("test");
        c.sensor_location.key = view("key");
        c.sensor_location.system_name = view("system");
    };
    fill(cc); fill(ic); fill(lc); fill(tc);
    ic.buffer_count = 2; ic.buffer_size = 64; ic.queue_capacity = 2;
    const auto benchmark_begin = Clock::now();
    const auto baseline = os_state();
    for (unsigned f = 0; f < 6; ++f)
        if (!ams_mel_test_completion_snapshot(f, base[f].data()) ||
            !ams_mel_test_completion_owner(f, 0, &owners[f])) return 1;
    std::uint64_t sent{}, released{};
    if (!gate(0, 0, &sent, &released) || sent != released ||
        ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "completion-scale", "", &session, nullptr, 0, nullptr) != AMS_MEL_OK ||
        ams_mel_ir_c2_open(session, &cc, &c2, nullptr, 0, nullptr) != AMS_MEL_OK ||
        ams_mel_ir_stream_open(session, &ic, &image, nullptr, 0, nullptr) != AMS_MEL_OK ||
        ams_mel_ir_instrumentation_open(session, &lc, &instr, nullptr, 0, nullptr) != AMS_MEL_OK ||
        ams_mel_ir_track_open(session, &tc, &track, nullptr, 0, nullptr) != AMS_MEL_OK ||
        ams_mel_ir_c2_enable(c2, nullptr, 0, nullptr) != AMS_MEL_OK ||
        ams_mel_ir_instrumentation_enable(instr, nullptr, 0, nullptr) != AMS_MEL_OK ||
        ams_mel_ir_track_enable(track, nullptr, 0, nullptr) != AMS_MEL_OK) return 1;
    ams_mel_ir_channel_comms_test_request_v1 cr{};
    ams_mel_navigation_report_v1 nr{};
    ams_mel_ir_instrumentation_level_command_v1 level{};
    ams_mel_ir_track_data_update_v1 update{};
    ams_mel_ir_system_track_data_response_v1 response{};
    nr.state = AMS_MEL_POSITION_SOLUTION_NOT_SET;
    level.priority = AMS_MEL_IR_PRIORITY_DEBUG;
    update.track_status = AMS_MEL_IR_TRACK_STATUS_UPDATE;
    cr.command_id = 0x80000001U; cr.request_id = 0xe0000003U;
    const auto submission_begin = Clock::now();
    for (auto& r : returns) if (ams_mel_ir_c2_send_keepalive(c2, &r, nullptr, 0, nullptr) != AMS_MEL_OK) return 1;
    for (auto& r : modes) if (ams_mel_ir_c2_submit_operate(c2, 1, &r, nullptr, 0, nullptr) != AMS_MEL_OK) return 1;
    for (auto& r : comms) if (ams_mel_ir_c2_submit_comms_test(c2, &cr, &r, nullptr, 0, nullptr) != AMS_MEL_OK) return 1;
    for (auto& r : navigation) if (ams_mel_ir_stream_submit_navigation_report(image, &nr, &r, nullptr, 0, nullptr) != AMS_MEL_OK) return 1;
    for (auto& r : levels) if (ams_mel_ir_instrumentation_submit_level(instr, &level, &r, nullptr, 0, nullptr) != AMS_MEL_OK) return 1;
    for (auto& r : updates) if (ams_mel_ir_track_submit_update(track, &update, &r, nullptr, 0, nullptr) != AMS_MEL_OK) return 1;
    for (auto& r : responses) if (ams_mel_ir_track_submit_system_track_data_response(track, &response, &r, nullptr, 0, nullptr) != AMS_MEL_OK) return 1;
    const auto submitted = Clock::now();
    if (!gate(1, sent + 100, nullptr, nullptr)) return 1;
    unsigned active{}, peak{};
    for (unsigned f = 0; f < 6; ++f) {
        if (!ams_mel_test_completion_wait(f, 4, base[f][4] + counts[f]) ||
            !ams_mel_test_completion_snapshot(f, held[f].data()) || held[f][1] != counts[f]) return 1;
        active += static_cast<unsigned>(held[f][1]);
        peak += static_cast<unsigned>(held[f][2]);
    }
    const auto all_pending = Clock::now();
    ams_mel_test_timeline::record("all_workers_pending");
    const auto pending = os_state();
    const auto release_begin = Clock::now();
    ams_mel_test_timeline::record("provider_release_begin");
    if (!gate(2, 100, nullptr, nullptr)) return 1;
    // Last direct provider call above. Phase C must never invoke this address.
    gate = nullptr;
    if (dlclose(handle) != 0) return 1;
    handle = nullptr;
    ams_mel_test_timeline::record("benchmark_handle_closed");
    for (unsigned f = 0; f < 6; ++f)
        if (!ams_mel_test_completion_boundary(f, 1, counts[f], nullptr)) return 1;
    const auto gets_exited = Clock::now();
    for (auto &r : returns) (void)ams_mel_ir_return_request_close(&r, nullptr, 0, nullptr);
    for (auto &r : modes) (void)ams_mel_ir_mode_request_close(&r, nullptr, 0, nullptr);
    for (auto &r : comms) (void)ams_mel_ir_channel_comms_request_close(&r, nullptr, 0, nullptr);
    for (auto &r : navigation) (void)ams_mel_ir_navigation_request_close(&r, nullptr, 0, nullptr);
    for (auto &r : levels) (void)ams_mel_ir_instrumentation_request_close(&r, nullptr, 0, nullptr);
    for (auto &r : updates) (void)ams_mel_ir_track_update_request_close(&r, nullptr, 0, nullptr);
    for (auto &r : responses) (void)ams_mel_ir_track_system_response_request_close(&r, nullptr, 0, nullptr);
    ams_mel_test_timeline::record("parent_teardown_begin");
    if (ams_mel_ir_c2_close(&c2, nullptr, 0, nullptr) != AMS_MEL_OK ||
        ams_mel_ir_stream_close(&image, nullptr, 0, nullptr) != AMS_MEL_OK ||
        ams_mel_ir_instrumentation_close(&instr, nullptr, 0, nullptr) != AMS_MEL_OK ||
        ams_mel_ir_track_close(&track, nullptr, 0, nullptr) != AMS_MEL_OK ||
        ams_mel_session_close(&session, nullptr, 0, nullptr) != AMS_MEL_OK) return 1;
    const auto parents_closed = Clock::now();
    for (unsigned f = 0; f < 6; ++f)
        if (!ams_mel_test_completion_boundary(f, 2, 0, nullptr)) return 1;
    for (unsigned f = 0; f < 6; ++f)
        if (!ams_mel_test_completion_wait(f, 3, base[f][3] + counts[f])) return 1;
    const auto graphs_released = Clock::now();
    for (unsigned f = 0; f < 6; ++f) {
        std::uint64_t observed{}, boundary[4]{};
        if (!ams_mel_test_completion_owner(f, owners[f] + counts[f], &observed) ||
            !ams_mel_test_completion_boundary(f, 3, 0, boundary) ||
            boundary[1] != counts[f] || boundary[3] != owners[f] + counts[f]) return 1;
    }
    const auto inputs_destroyed = Clock::now();
    std::cout << "mixed_n=100 submitted=100 active=" << active << " peak=" << peak
              << " gets=100 graphs=100 inputs=100 threads_base=" << baseline.threads
              << " threads_held=" << pending.threads << " rss_base_kb=" << baseline.rss
              << " rss_held_kb=" << pending.rss << " vm_base_kb=" << baseline.virtual_kb
              << " vm_held_kb=" << pending.virtual_kb
              << " submit_ms=" << ms(submission_begin, submitted)
              << " pending_ms=" << ms(submission_begin, all_pending)
              << " get_ms=" << ms(release_begin, gets_exited)
              << " graph_after_get_ms=" << ms(gets_exited, graphs_released)
              << " graph_ms=" << ms(release_begin, graphs_released)
              << " input_ms=" << ms(release_begin, inputs_destroyed)
              << " parents_close_ms=" << ms(gets_exited, parents_closed)
              << " terminal_ms=" << ms(release_begin, inputs_destroyed)
              << " duration_ms=" << ms(benchmark_begin, inputs_destroyed) << '\n';
    return 0;
}

OsState os_state()
{
    OsState state;
    std::error_code error;
    if (std::filesystem::exists("/proc/self/task", error)) {
        state.threads = 0;
        for (auto it = std::filesystem::directory_iterator{"/proc/self/task", error};
             !error && it != std::filesystem::directory_iterator{}; it.increment(error))
            ++state.threads;
        if (error) state.threads = -1;
    }
    std::ifstream input{"/proc/self/status"};
    std::string line;
    while (std::getline(input, line)) {
        if (line.starts_with("VmRSS:")) state.rss = std::stol(line.substr(6));
        if (line.starts_with("VmSize:")) state.virtual_kb = std::stol(line.substr(7));
    }
    return state;
}
double ms(Clock::time_point start, Clock::time_point end)
{ return std::chrono::duration<double, std::milli>{end - start}.count(); }

int main(int argc, char **argv)
{
    auto *handle = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW);
    if (!handle) return 1;
    auto gate = reinterpret_cast<Gate>(dlsym(handle, "mock_completion_gate"));
    if (!gate) return 1;
    if (argc == 2 && std::string{argv[1]} == "mixed") {
        Timeline timeline;
        if (!timeline.open()) { dlclose(handle); return 1; }
        const int status = mixed(gate, handle);
        gate = nullptr;
        // On failure only: release the control pin after mixed's cleanup guard.
        if (handle) dlclose(handle);
        if (status != 0 || !timeline.report()) return 1;
        timeline.complete = true;
        return 0;
    }
    if (argc != 1) return 1;
    struct Cleanup {
        Gate gate;
        ams_mel_session *&session;
        ams_mel_ir_c2 *&channel;
        std::vector<ams_mel_ir_return_request *>& requests;
        ~Cleanup()
        {
            std::uint64_t sent{}, released{};
            if (gate(0, 0, &sent, &released) && sent > released)
                (void)gate(2, sent - released, nullptr, nullptr);
            for (auto& request : requests) if (request) {
                ams_mel_ir_return_result_v1 result{};
                (void)ams_mel_ir_return_request_wait(request, 15000, &result, nullptr, 0, nullptr);
                (void)ams_mel_ir_return_request_close(&request, nullptr, 0, nullptr);
            }
            if (channel) (void)ams_mel_ir_c2_close(&channel, nullptr, 0, nullptr);
            if (session) (void)ams_mel_session_close(&session, nullptr, 0, nullptr);
            std::uint64_t evidence[4]{}, observed{};
            if (ams_mel_test_completion_boundary(1, 3, 0, evidence))
                (void)ams_mel_test_completion_owner(1, evidence[2], &observed);
        }
    };
    for (unsigned n : {1U, 10U, 100U}) {
        std::uint64_t before[6]{}, pending[6]{}, after[6]{}, sent{}, released{};
        if (!ams_mel_test_completion_snapshot(1, before) || !gate(0, 0, &sent, &released)) return 1;
        const auto base = os_state();
        ams_mel_session *session{};
        ams_mel_ir_c2 *channel{};
        std::vector<ams_mel_ir_return_request *> requests(n, nullptr);
        Cleanup cleanup{gate, session, channel, requests};
        ams_mel_ir_c2_config_v1 cfg{};
        cfg.channel_type = AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL;
        cfg.channel_id.descriptive_label = {"scale", 5};
        cfg.platform_id.descriptive_label = {"test", 4};
        cfg.sensor_location.key = {"key", 3};
        cfg.sensor_location.system_name = {"system", 6};
        if (ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "completion-scale", "",
                &session, nullptr, 0, nullptr) != AMS_MEL_OK ||
            ams_mel_ir_c2_open(session, &cfg, &channel, nullptr, 0, nullptr) != AMS_MEL_OK)
            return 1;
        const auto start = Clock::now();
        for (auto& request : requests)
            if (ams_mel_ir_c2_send_keepalive(channel, &request, nullptr, 0, nullptr) != AMS_MEL_OK)
                return 1;
        const auto submitted = Clock::now();
        if (!gate(1, sent + n, nullptr, nullptr) ||
            !ams_mel_test_completion_wait(1, 4, before[4] + n) ||
            !ams_mel_test_completion_snapshot(1, pending) || pending[1] != n) return 1;
        const auto held = os_state();
        std::condition_variable idle_cv;
        std::mutex idle_mutex;
        std::unique_lock idle_lock{idle_mutex};
        const auto idle_cpu_start = std::clock();
        /* The workers are already proven blocked above. This timed idle
         * observation is a measurement interval, not a concurrency barrier. */
        idle_cv.wait_for(idle_lock, std::chrono::milliseconds{100});
        const auto idle_cpu_end = std::clock();
        const auto burst_cpu_start = std::clock();
        const auto release_start = Clock::now();
        if (!gate(2, n, nullptr, nullptr) ||
            !ams_mel_test_completion_wait(1, 3, before[3] + n)) return 1;
        for (auto& request : requests) {
            ams_mel_ir_return_result_v1 result{};
            if (ams_mel_ir_return_request_wait(request, 15000, &result, nullptr, 0, nullptr) != AMS_MEL_OK)
                return 1;
            (void)ams_mel_ir_return_request_close(&request, nullptr, 0, nullptr);
        }
        const auto drained = Clock::now();
        const auto burst_cpu_end = std::clock();
        if (ams_mel_ir_c2_close(&channel, nullptr, 0, nullptr) != AMS_MEL_OK ||
            ams_mel_session_close(&session, nullptr, 0, nullptr) != AMS_MEL_OK ||
            !ams_mel_test_completion_snapshot(1, after) || after[1] != 0 ||
            after[5] != before[5] + n) return 1;
        const auto teardown = Clock::now();
        std::cout << "n=" << n << " workers=" << pending[1]
                  << " threads_base=" << base.threads << " threads_held=" << held.threads
                  << " rss_base_kb=" << base.rss << " rss_held_kb=" << held.rss
                  << " vm_base_kb=" << base.virtual_kb << " vm_held_kb=" << held.virtual_kb
                  << " submit_ms=" << ms(start, submitted)
                  << " submit_per_request_ms=" << ms(start, submitted) / n
                  << " drain_ms=" << ms(release_start, drained)
                  << " teardown_ms=" << ms(drained, teardown)
                  << " idle_100ms_cpu_ms=" << 1000.0 * (idle_cpu_end - idle_cpu_start) / CLOCKS_PER_SEC
                  << " burst_cpu_ms=" << 1000.0 * (burst_cpu_end - burst_cpu_start) / CLOCKS_PER_SEC
                  << '\n';
    }
    dlclose(handle);
}
