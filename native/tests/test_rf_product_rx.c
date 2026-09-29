/* Task 033D RF ProductRxEndpoint ComplexINT16 receive contract (C11).
 *
 * One process per CTest case, so permanent-registration and lifetime
 * evidence can never leak between cases.
 *
 * TEST-owned provider pin discipline: take_pin() dlopens the mock provider
 * with RTLD_NOLOAD and resolves EVERY mock control/observation symbol at
 * once. The late-callback cases call the arm function as the LAST provider
 * function, then drop_pin() clears every function pointer and dlcloses the
 * handle BEFORE endpoint Close and BEFORE the late callback is released. No
 * stale provider function pointer is invoked after that point.
 *
 * DSO mapping evidence is reference-neutral: /proc/self/maps (no dlopen and
 * never RTLD_NOLOAD) plus the mock's library_unloaded lifetime-log record.
 * Facade-side observations after the public endpoint is gone use the
 * test-only ams_mel_test_rf_rx_* seams (test-exports.map only). */
#define _XOPEN_SOURCE 700 /* realpath */

#include <ams_mel/abi.h>

#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef AMS_MEL_TEST_MOCK_RF_PROVIDER
#error "mock RF provider path is required"
#endif

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

/* ---------------------------------------------------------- test seams */
typedef struct ams_mel_test_rf_rx_observer ams_mel_test_rf_rx_observer;
extern int ams_mel_test_rf_rx_observer_from(const ams_mel_rf_product_rx *,
                                            ams_mel_test_rf_rx_observer **);
extern int ams_mel_test_rf_rx_observer_from_last_registration(ams_mel_test_rf_rx_observer **);
extern int ams_mel_test_rf_rx_observe(const ams_mel_test_rf_rx_observer *, size_t *,
                                      uint64_t *, uint64_t *, size_t *, int *);
extern int ams_mel_test_rf_rx_waiters(const ams_mel_test_rf_rx_observer *, size_t *);
extern int ams_mel_test_rf_rx_drain_waiters(const ams_mel_test_rf_rx_observer *, size_t *);
extern int ams_mel_test_rf_rx_preset_counters(const ams_mel_test_rf_rx_observer *, uint64_t);
extern void ams_mel_test_rf_rx_observer_close(ams_mel_test_rf_rx_observer **);
extern void ams_mel_test_rf_rx_failpoint(unsigned);
extern void ams_mel_test_rf_rx_negative_control(unsigned);
extern void ams_mel_test_rf_rx_hold_next(int, int);
extern size_t ams_mel_test_rf_rx_permanent_registrations(void);

/* ---------------------------------------------------------- lifetime log */
static char log_path[] = "/tmp/ams-mel-rf-rx-XXXXXX";
static void remove_log(void) { (void)unlink(log_path); }

static void setup_log(void)
{
    const int fd = mkstemp(log_path);
    CHECK(fd >= 0 && close(fd) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", log_path, 1) == 0);
    CHECK(atexit(remove_log) == 0);
}

static void reset_log(void)
{
    FILE *file = fopen(log_path, "w");
    CHECK(file != NULL && fclose(file) == 0);
}

static unsigned occurrences(const char *line)
{
    char contents[16384];
    const size_t length = strlen(line);
    unsigned count = 0;
    size_t size;
    const char *cursor;
    FILE *file = fopen(log_path, "rb");
    CHECK(file != NULL);
    size = fread(contents, 1, sizeof contents - 1U, file);
    CHECK(!ferror(file) && fclose(file) == 0);
    contents[size] = '\0';
    for (cursor = contents; *cursor != '\0';) {
        const char *end = strchr(cursor, '\n');
        const size_t item = end ? (size_t)(end - cursor) : strlen(cursor);
        if (item == length && strncmp(cursor, line, length) == 0) ++count;
        cursor += item + (end ? 1U : 0U);
    }
    return count;
}

/* 1-based line number of the first `line` in the lifetime log, 0 if absent.
 * Used for ordering evidence between bridge and provider records. */
static unsigned position(const char *line)
{
    char contents[16384];
    const size_t length = strlen(line);
    unsigned number = 0;
    size_t size;
    const char *cursor;
    FILE *file = fopen(log_path, "rb");
    CHECK(file != NULL);
    size = fread(contents, 1, sizeof contents - 1U, file);
    CHECK(!ferror(file) && fclose(file) == 0);
    contents[size] = '\0';
    for (cursor = contents; *cursor != '\0';) {
        const char *end = strchr(cursor, '\n');
        const size_t item = end ? (size_t)(end - cursor) : strlen(cursor);
        ++number;
        if (item == length && strncmp(cursor, line, length) == 0) return number;
        cursor += item + (end ? 1U : 0U);
    }
    return 0U;
}

/* Reference-neutral: reads /proc/self/maps, never touches the loader. */
static int provider_mapped(void)
{
    char resolved[PATH_MAX];
    char line[PATH_MAX + 256];
    int found = 0;
    FILE *maps;
    CHECK(realpath(AMS_MEL_TEST_MOCK_RF_PROVIDER, resolved) != NULL);
    maps = fopen("/proc/self/maps", "r");
    CHECK(maps != NULL);
    while (!found && fgets(line, sizeof line, maps) != NULL) {
        const char *path = strchr(line, '/');
        if (path != NULL) {
            const size_t length = strlen(resolved);
            if (strncmp(path, resolved, length) == 0 &&
                (path[length] == '\n' || path[length] == '\0'))
                found = 1;
        }
    }
    CHECK(fclose(maps) == 0);
    return found;
}

/* ---------------------------------------------------------- provider pin */
typedef unsigned (*counter_fn)(void);
typedef size_t (*size_fn)(void);
typedef uint64_t (*u64_fn)(void);
typedef int (*emit_fn)(uint64_t, int, size_t);
typedef int (*arm_fn)(uint64_t, int, int);
typedef int (*emit_owned_fn)(uint64_t);
typedef void (*active_throw_fn)(int, int);

typedef struct pin {
    void *handle;
    counter_fn shutdown_calls, forbidden_calls, rdma_calls, registrations;
    counter_fn endpoints_destroyed, creates, release, empty_invocations;
    size_fn last_region_size;
    u64_fn last_endpoint_id;
    emit_fn emit;
    arm_fn arm_late;
    emit_owned_fn emit_owned;
    active_throw_fn set_active_throw;
} pin;

#define RESOLVE(field, name) do { \
    *(void **)(&value.field) = dlsym(value.handle, name); \
    CHECK(value.field != NULL); \
} while (0)

/* Only after ams_mel_rf_data_open has loaded the provider. */
static pin take_pin(void)
{
    pin value;
    memset(&value, 0, sizeof value);
    value.handle = dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(value.handle != NULL);
    RESOLVE(shutdown_calls, "mock_rf_shutdown_calls");
    RESOLVE(forbidden_calls, "mock_rf_forbidden_calls");
    RESOLVE(rdma_calls, "mock_rf_rx_rdma_calls");
    RESOLVE(registrations, "mock_rf_rx_registrations");
    RESOLVE(endpoints_destroyed, "mock_rf_rx_endpoints_destroyed");
    RESOLVE(creates, "mock_rf_rx_creates");
    RESOLVE(release, "mock_rf_rx_release");
    RESOLVE(empty_invocations, "mock_rf_rx_empty_invocations");
    RESOLVE(last_region_size, "mock_rf_rx_last_region_size");
    RESOLVE(last_endpoint_id, "mock_rf_rx_last_endpoint_id");
    RESOLVE(emit, "mock_rf_rx_emit");
    RESOLVE(arm_late, "mock_rf_rx_arm_late");
    RESOLVE(emit_owned, "mock_rf_rx_emit_owned");
    RESOLVE(set_active_throw, "mock_rf_rx_set_active_throw");
    return value;
}

/* Clears every provider function pointer, then drops the TEST-owned pin. */
static void drop_pin(pin *value)
{
    void *const handle = value->handle;
    memset(value, 0, sizeof *value);
    CHECK(handle != NULL && dlclose(handle) == 0);
}

/* Contract-wide invariant: production never calls RDMA or a forbidden API. */
static void check_no_forbidden(const pin *p)
{
    CHECK(p->rdma_calls() == 0U);
    CHECK(p->forbidden_calls() == 0U);
    CHECK(p->empty_invocations() == 0U);
}

enum {
    EMIT_RICH = 0, EMIT_NULL_METADATA = 1, EMIT_WRONG_VARIANT = 2, EMIT_NULL_POINTER = 3,
    EMIT_COUNT = 4, EMIT_ZERO_NULL = 5, EMIT_USER_DATA = 6, EMIT_STAB_POINTS = 7,
    EMIT_RECEIVE_EVENTS = 8, EMIT_ASSOCIATIONS = 9, EMIT_EMPTY_ASSOCIATIONS = 10,
    EMIT_SPARSE = 11, EMIT_RICH_ALTERNATE = 12
};

/* ---------------------------------------------------------- facade helpers */
static ams_mel_rf_data *open_rx(const char *scenario)
{
    ams_mel_rf_data *data = NULL;
    char diagnostic[128];
    CHECK(ams_mel_rf_data_open(AMS_MEL_TEST_MOCK_RF_PROVIDER, scenario, &data, diagnostic,
                               sizeof diagnostic, NULL) == AMS_MEL_OK);
    CHECK(data != NULL);
    return data;
}

static ams_mel_rf_product_rx_config_v1 config_of(size_t capacity, size_t max_samples)
{
    ams_mel_rf_product_rx_config_v1 config;
    memset(&config, 0, sizeof config);
    config.data_format = AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT16;
    config.region_size_bytes = UINT64_C(65536);
    config.queue_capacity = capacity;
    config.max_samples_per_event = max_samples;
    return config;
}

static ams_mel_rf_product_rx_request *submit(ams_mel_rf_data *data, size_t capacity,
                                             size_t max_samples)
{
    ams_mel_rf_product_rx_request *request = NULL;
    const ams_mel_rf_product_rx_config_v1 config = config_of(capacity, max_samples);
    char diagnostic[128];
    CHECK(ams_mel_rf_data_submit_product_rx(data, &config, &request, diagnostic,
                                            sizeof diagnostic, NULL) == AMS_MEL_OK);
    CHECK(request != NULL && diagnostic[0] == '\0');
    return request;
}

static void wait_ok(const ams_mel_rf_product_rx_request *request)
{
    ams_mel_rf_product_rx_request_result_v1 result = {UINT32_C(0xFFFF)};
    CHECK(ams_mel_rf_product_rx_request_wait(request, 10000U, &result, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(result.error_code == AMS_MEL_ERROR_NONE);
}

static ams_mel_rf_product_rx *claim(ams_mel_rf_product_rx_request *request,
                                    ams_mel_rf_product_rx_info_v1 *info)
{
    ams_mel_rf_product_rx *endpoint = NULL;
    char diagnostic[128];
    CHECK(ams_mel_rf_product_rx_request_claim(request, &endpoint, info, diagnostic,
                                              sizeof diagnostic, NULL) == AMS_MEL_OK);
    CHECK(endpoint != NULL && diagnostic[0] == '\0');
    CHECK(info->assigned_data_format == AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT16);
    return endpoint;
}

/* submit + wait + claim + request close. */
static ams_mel_rf_product_rx *open_endpoint(ams_mel_rf_data *data, size_t capacity,
                                            size_t max_samples,
                                            ams_mel_rf_product_rx_info_v1 *info)
{
    ams_mel_rf_product_rx_request *request = submit(data, capacity, max_samples);
    ams_mel_rf_product_rx *endpoint;
    wait_ok(request);
    endpoint = claim(request, info);
    CHECK(ams_mel_rf_product_rx_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(request == NULL);
    return endpoint;
}

static ams_mel_rf_product_rx_counters_v1 counters_of(const ams_mel_rf_product_rx *endpoint)
{
    ams_mel_rf_product_rx_counters_v1 counters;
    memset(&counters, 0xA5, sizeof counters);
    CHECK(ams_mel_rf_product_rx_get_counters(endpoint, &counters, NULL, 0, NULL) == AMS_MEL_OK);
    return counters;
}

static void expect_counters(const ams_mel_rf_product_rx *endpoint, uint64_t received,
                            uint64_t queued, uint64_t dropped, uint64_t malformed,
                            uint64_t allocation, uint64_t after_close)
{
    const ams_mel_rf_product_rx_counters_v1 c = counters_of(endpoint);
    if (c.callbacks_received != received || c.products_queued != queued ||
        c.products_dropped_queue_full != dropped || c.malformed_or_unsupported != malformed ||
        c.allocation_failures != allocation || c.callbacks_after_close != after_close)
        fprintf(stderr, "counters: %llu %llu %llu %llu %llu %llu\n",
                (unsigned long long)c.callbacks_received, (unsigned long long)c.products_queued,
                (unsigned long long)c.products_dropped_queue_full,
                (unsigned long long)c.malformed_or_unsupported,
                (unsigned long long)c.allocation_failures,
                (unsigned long long)c.callbacks_after_close);
    CHECK(c.callbacks_received == received && c.products_queued == queued);
    CHECK(c.products_dropped_queue_full == dropped && c.malformed_or_unsupported == malformed);
    CHECK(c.allocation_failures == allocation && c.callbacks_after_close == after_close);
}

static ams_mel_rf_product_rx_event *receive_ok(ams_mel_rf_product_rx *endpoint)
{
    ams_mel_rf_product_rx_event *event = NULL;
    CHECK(ams_mel_rf_product_rx_receive(endpoint, 0U, &event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(event != NULL);
    return event;
}

static void expect_empty(ams_mel_rf_product_rx *endpoint)
{
    ams_mel_rf_product_rx_event *event = NULL;
    CHECK(ams_mel_rf_product_rx_receive(endpoint, 0U, &event, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(event == NULL);
}

static const ams_mel_rf_product_rx_event_v1 *view_of(const ams_mel_rf_product_rx_event *event)
{
    const ams_mel_rf_product_rx_event_v1 *view = NULL;
    CHECK(ams_mel_rf_product_rx_event_view(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(view != NULL);
    return view;
}

static void close_event(ams_mel_rf_product_rx_event **event)
{
    CHECK(ams_mel_rf_product_rx_event_close(event, NULL, 0, NULL) == AMS_MEL_OK && !*event);
    CHECK(ams_mel_rf_product_rx_event_close(event, NULL, 0, NULL) == AMS_MEL_OK && !*event);
}

static void close_endpoint(ams_mel_rf_product_rx **endpoint)
{
    char diagnostic[128];
    CHECK(ams_mel_rf_product_rx_close(endpoint, diagnostic, sizeof diagnostic, NULL) ==
          AMS_MEL_OK);
    CHECK(*endpoint == NULL);
    CHECK(ams_mel_rf_product_rx_close(endpoint, NULL, 0, NULL) == AMS_MEL_OK);
}

static void close_data(ams_mel_rf_data **data)
{
    CHECK(ams_mel_rf_data_close(data, NULL, 0, NULL) == AMS_MEL_OK && *data == NULL);
}

/* ---------------------------------------------------------- fidelity */
static void expect_rich_metadata(const ams_mel_rf_product_rx_metadata_v1 *m)
{
    CHECK(m->mel_protocol_version_id == UINT32_C(0xFEDCBA98));
    CHECK(m->va_definition_id == UINT32_C(0x80000001));
    CHECK(m->va_instance_id == UINT32_C(0x7FFFFFFE));
    CHECK(m->job_details_id == UINT32_C(0xDEADBEEF));
    CHECK(m->job_interval_id == UINT32_C(0x00010002));
    CHECK(m->lf_type_id == UINT32_C(0xFFFFFFFF));
    CHECK(m->lf_instance_id == UINT32_C(0x12345678));
    /* Verbatim UTCTime components: no ns conversion, no renormalization. */
    CHECK(m->first_rx_event_start_s == INT64_C(-4102444801));
    CHECK(m->first_rx_event_start_fs == INT64_C(987654321098765));
    CHECK(m->rx_stream_ids.size == 4U && m->rx_stream_ids.data != NULL);
    CHECK(m->rx_stream_ids.data[0] == UINT32_C(0xFFFFFFFF));
    CHECK(m->rx_stream_ids.data[1] == 0U);
    CHECK(m->rx_stream_ids.data[2] == UINT32_C(0x80000000));
    CHECK(m->rx_stream_ids.data[3] == 42U);
}

static void expect_rich(const ams_mel_rf_product_rx_event_v1 *view, uint64_t endpoint_id)
{
    static const int16_t expected[6][2] = {{10, 20}, {30, 40}, {-5, 6},
        {INT16_MIN, INT16_MAX}, {INT16_MAX, INT16_MIN}, {-1, 0}};
    size_t index;
    CHECK(view->endpoint_id == endpoint_id);
    CHECK(view->data_format == AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT16);
    CHECK(view->samples.size == 6U && view->samples.data != NULL);
    for (index = 0; index < 6U; ++index) {
        CHECK(view->samples.data[index].real == expected[index][0]);
        CHECK(view->samples.data[index].imag == expected[index][1]);
    }
    CHECK(view->metadata.phase_coherence_with_prior == 1U);
    expect_rich_metadata(&view->metadata);
}

static void expect_alternate(const ams_mel_rf_product_rx_event_v1 *view)
{
    size_t index;
    CHECK(view->samples.size == 3U);
    for (index = 0; index < 3U; ++index) {
        CHECK(view->samples.data[index].real == (int16_t)(1000 + (int)index));
        CHECK(view->samples.data[index].imag == (int16_t)(-1000 - (int)index));
    }
    CHECK(view->metadata.phase_coherence_with_prior == 0U);
    expect_rich_metadata(&view->metadata);
}

/* Full create/claim/receive/close path plus copy-boundary and fidelity. */
static void test_receive(void)
{
    ams_mel_rf_data *data = open_rx("rx:ok");
    pin p = take_pin();
    ams_mel_rf_product_rx_info_v1 info = {0, 0};
    ams_mel_rf_product_rx *endpoint = open_endpoint(data, 8U, 64U, &info);
    ams_mel_rf_product_rx_event *a, *b;
    const ams_mel_rf_product_rx_event_v1 *view_a, *view_b;

    CHECK(info.endpoint_id == p.last_endpoint_id() && info.endpoint_id != 0U);
    CHECK(p.last_region_size() == 65536U);
    CHECK(p.registrations() == 1U);
    CHECK(ams_mel_test_rf_rx_permanent_registrations() == 1U);
    expect_empty(endpoint);

    CHECK(p.emit(info.endpoint_id, EMIT_RICH, 0U) == 1);
    a = receive_ok(endpoint);
    view_a = view_of(a);
    /* The mock has ALREADY overwritten its buffer and metadata. */
    expect_rich(view_a, info.endpoint_id);

    /* Reuse of the provider buffer for a later product. */
    CHECK(p.emit(info.endpoint_id, EMIT_RICH_ALTERNATE, 0U) == 1);
    b = receive_ok(endpoint);
    view_b = view_of(b);
    expect_alternate(view_b);
    CHECK(view_of(a) == view_a);
    expect_rich(view_a, info.endpoint_id);
    CHECK(view_a->samples.data != view_b->samples.data);
    expect_counters(endpoint, 2U, 2U, 0U, 0U, 0U, 0U);

    close_event(&b);
    close_endpoint(&endpoint);
    CHECK(p.endpoints_destroyed() == 1U);
    close_data(&data);
    CHECK(p.shutdown_calls() == 1U && occurrences("rf_shutdown") == 1U);
    CHECK(occurrences("rf_data_destroyed") == 1U);
    check_no_forbidden(&p);
    drop_pin(&p);
    /* The callback was registered: the DSO is intentionally retained. */
    CHECK(provider_mapped() && occurrences("library_unloaded") == 0U);
    expect_rich(view_a, info.endpoint_id);
    close_event(&a);
}

/* Four fail-closed metadata profiles; empty associations are accepted. */
static void test_fail_closed(void)
{
    ams_mel_rf_data *data = open_rx("rx:ok");
    pin p = take_pin();
    ams_mel_rf_product_rx_info_v1 info;
    ams_mel_rf_product_rx *endpoint = open_endpoint(data, 8U, 64U, &info);
    ams_mel_rf_product_rx_event *event;
    const ams_mel_rf_product_rx_event_v1 *view;
    const int rejected[4] = {EMIT_USER_DATA, EMIT_STAB_POINTS, EMIT_RECEIVE_EVENTS,
                             EMIT_ASSOCIATIONS};
    unsigned index;
    for (index = 0; index < 4U; ++index) {
        CHECK(p.emit(info.endpoint_id, rejected[index], 0U) == 1);
        expect_empty(endpoint);
        expect_counters(endpoint, index + 1U, 0U, 0U, index + 1U, 0U, 0U);
    }
    CHECK(p.emit(info.endpoint_id, EMIT_EMPTY_ASSOCIATIONS, 0U) == 1);
    event = receive_ok(endpoint);
    view = view_of(event);
    CHECK(view->samples.size == 1U && view->samples.data[0].real == 3 &&
          view->samples.data[0].imag == 4);
    expect_rich_metadata(&view->metadata);
    close_event(&event);
    /* The default (sparse) metadata profile: all zero, no stream IDs. */
    CHECK(p.emit(info.endpoint_id, EMIT_SPARSE, 0U) == 1);
    event = receive_ok(endpoint);
    view = view_of(event);
    CHECK(view->metadata.mel_protocol_version_id == 0U && view->metadata.lf_type_id == 0U);
    CHECK(view->metadata.phase_coherence_with_prior == 0U);
    CHECK(view->metadata.first_rx_event_start_s == 0 && view->metadata.first_rx_event_start_fs == 0);
    CHECK(view->metadata.rx_stream_ids.size == 0U && view->metadata.rx_stream_ids.data == NULL);
    close_event(&event);
    expect_counters(endpoint, 6U, 2U, 0U, 4U, 0U, 0U);
    close_endpoint(&endpoint);
    close_data(&data);
    check_no_forbidden(&p);
    drop_pin(&p);
}

static void test_malformed(void)
{
    ams_mel_rf_data *data = open_rx("rx:ok");
    pin p = take_pin();
    ams_mel_rf_product_rx_info_v1 info;
    ams_mel_rf_product_rx *endpoint = open_endpoint(data, 8U, 4U, &info);
    ams_mel_rf_product_rx_event *event;
    const ams_mel_rf_product_rx_event_v1 *view;
    CHECK(p.emit(info.endpoint_id, EMIT_NULL_METADATA, 0U) == 1);
    CHECK(p.emit(info.endpoint_id, EMIT_WRONG_VARIANT, 0U) == 1);
    CHECK(p.emit(info.endpoint_id, EMIT_NULL_POINTER, 3U) == 1);
    CHECK(p.emit(info.endpoint_id, EMIT_COUNT, 5U) == 1); /* > max 4 */
    expect_empty(endpoint);
    expect_counters(endpoint, 4U, 0U, 0U, 4U, 0U, 0U);
    /* Exactly max is accepted. */
    CHECK(p.emit(info.endpoint_id, EMIT_COUNT, 4U) == 1);
    event = receive_ok(endpoint);
    view = view_of(event);
    CHECK(view->samples.size == 4U && view->samples.data[3].real == 3 &&
          view->samples.data[3].imag == -3);
    close_event(&event);
    /* count 0 with a NULL ComplexINT16 pointer: an empty owned event. */
    CHECK(p.emit(info.endpoint_id, EMIT_ZERO_NULL, 0U) == 1);
    event = receive_ok(endpoint);
    view = view_of(event);
    CHECK(view->samples.size == 0U && view->samples.data == NULL);
    CHECK(view->data_format == AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT16);
    expect_rich_metadata(&view->metadata);
    close_event(&event);
    /* Allocation failure while building the event, then while copying IDs. */
    ams_mel_test_rf_rx_failpoint(1U);
    CHECK(p.emit(info.endpoint_id, EMIT_COUNT, 2U) == 1);
    ams_mel_test_rf_rx_failpoint(2U);
    CHECK(p.emit(info.endpoint_id, EMIT_COUNT, 2U) == 1);
    expect_empty(endpoint);
    expect_counters(endpoint, 8U, 2U, 0U, 4U, 2U, 0U);
    close_endpoint(&endpoint);
    close_data(&data);
    check_no_forbidden(&p);
    drop_pin(&p);
}

/* Capacity 1: A is kept, B is dropped (DROP-INCOMING, never replace). */
static void test_queue_full(void)
{
    ams_mel_rf_data *data = open_rx("rx:ok");
    pin p = take_pin();
    ams_mel_rf_product_rx_info_v1 info;
    ams_mel_rf_product_rx *endpoint = open_endpoint(data, 1U, 64U, &info);
    ams_mel_rf_product_rx_event *event;
    CHECK(p.emit(info.endpoint_id, EMIT_RICH, 0U) == 1);
    CHECK(p.emit(info.endpoint_id, EMIT_RICH_ALTERNATE, 0U) == 1);
    expect_counters(endpoint, 2U, 1U, 1U, 0U, 0U, 0U);
    event = receive_ok(endpoint);
    expect_rich(view_of(event), info.endpoint_id);
    close_event(&event);
    expect_empty(endpoint);
    close_endpoint(&endpoint);
    close_data(&data);
    drop_pin(&p);
}

/* ---------------------------------------------------------- create path */
static void expect_submit(const char *scenario, ams_mel_status_t status, const char *text)
{
    ams_mel_rf_data *data;
    reset_log();
    data = open_rx(scenario);
    pin p = take_pin();
    ams_mel_rf_product_rx_request *request = NULL;
    const ams_mel_rf_product_rx_config_v1 config = config_of(4U, 16U);
    char diagnostic[256];
    CHECK(ams_mel_rf_data_submit_product_rx(data, &config, &request, diagnostic,
                                            sizeof diagnostic, NULL) == status);
    CHECK(request == NULL && strstr(diagnostic, text) != NULL);
    CHECK(p.registrations() == 0U && occurrences("rf_rx_create") == 1U);
    close_data(&data);
    CHECK(occurrences("rf_shutdown") == 1U);
    drop_pin(&p);
    CHECK(!provider_mapped() && occurrences("library_unloaded") == 1U);
}

/* Terminal failure cached; repeated Wait and Claim return the same. */
static void expect_failure(const char *scenario, ams_mel_status_t status,
                           ams_mel_error_code_t code, const char *text, int writes_result)
{
    ams_mel_rf_data *data;
    pin p;
    ams_mel_rf_product_rx_request *request;
    ams_mel_rf_product_rx_request_result_v1 result;
    ams_mel_rf_product_rx *endpoint = NULL;
    ams_mel_rf_product_rx_info_v1 info = {7U, 7U};
    char first[1024], second[1024];
    size_t required = 0;
    int round;
    reset_log();
    data = open_rx(scenario);
    p = take_pin();
    request = submit(data, 4U, 16U);
    for (round = 0; round < 2; ++round) {
        char *const diagnostic = round == 0 ? first : second;
        result.error_code = UINT32_C(0xABCD);
        CHECK(ams_mel_rf_product_rx_request_wait(request, 10000U, &result, diagnostic,
                                                 sizeof first, &required) == status);
        CHECK(result.error_code == (writes_result ? code : UINT32_C(0xABCD)));
        CHECK(strstr(diagnostic, text) != NULL && required == strlen(diagnostic) + 1U);
    }
    CHECK(strcmp(first, second) == 0);
    CHECK(ams_mel_rf_product_rx_request_claim(request, &endpoint, &info, second, sizeof second,
                                              NULL) == status);
    CHECK(endpoint == NULL && info.endpoint_id == 7U && strcmp(first, second) == 0);
    CHECK(p.registrations() == 0U);
    CHECK(ams_mel_rf_product_rx_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    /* The failed worker released its child claim before publishing, so
     * this Close is the synchronous 033B path. */
    close_data(&data);
    CHECK(occurrences("rf_shutdown") == 1U && occurrences("rf_data_destroyed") == 1U);
    check_no_forbidden(&p);
    drop_pin(&p);
    CHECK(!provider_mapped() && occurrences("library_unloaded") == 1U);
}

static void test_create_failures(void)
{
    char long_text[700];
    memset(long_text, 'r', 600U);
    memcpy(long_text + 600U, "\xE2\x82\xAC" "zzz", 7U);
    expect_failure("rx:error-known", AMS_MEL_PROVIDER_FAILED,
                   AMS_MEL_ERROR_INSUFFICIENT_RESOURCES, "mock ProductRx resources exhausted", 1);
    expect_failure("rx:error-long", AMS_MEL_PROVIDER_FAILED, AMS_MEL_ERROR_UNSUPPORTED,
                   long_text, 1);
    expect_failure("rx:error-unknown-code", AMS_MEL_PROVIDER_FAILED, AMS_MEL_ERROR_NONE,
                   "unknown MEL ErrorCode 77", 1);
    expect_failure("rx:null-endpoint", AMS_MEL_PROVIDER_FAILED, AMS_MEL_ERROR_NONE,
                   "null endpoint", 1);
    expect_failure("rx:future-throw", AMS_MEL_PROVIDER_EXCEPTION, 0U,
                   "mock ProductRx future exception", 0);
    expect_failure("rx:getter-throw", AMS_MEL_PROVIDER_EXCEPTION, 0U,
                   "mock getEndpointID exception", 0);
    /* Assigned-format mismatch: no callback registered, endpoint destroyed. */
    expect_failure("rx:format-mismatch", AMS_MEL_PROVIDER_FAILED, AMS_MEL_ERROR_NONE,
                   "assigned JobDataFormat 1", 1);
    expect_submit("rx:invalid-future", AMS_MEL_PROVIDER_FAILED, "invalid ProductRxEndpoint future");
    expect_submit("rx:sync-throw", AMS_MEL_PROVIDER_EXCEPTION,
                  "mock createProductRxEndpoint exception");
}

/* Diagnostic truncation keeps a complete UTF-8 prefix of the long text. */
static void test_long_diagnostic(void)
{
    ams_mel_rf_data *data = open_rx("rx:error-long");
    ams_mel_rf_product_rx_request *request = submit(data, 4U, 16U);
    ams_mel_rf_product_rx_request_result_v1 result;
    char small[602];
    size_t required = 0;
    CHECK(ams_mel_rf_product_rx_request_wait(request, 10000U, &result, small, sizeof small,
                                             &required) == AMS_MEL_PROVIDER_FAILED);
    CHECK(required == 607U && strlen(small) == 600U);
    CHECK(ams_mel_rf_product_rx_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    close_data(&data);
}

static void test_invalid(void)
{
    ams_mel_rf_data *data = open_rx("rx:ok");
    pin p = take_pin();
    ams_mel_rf_product_rx_request *request = NULL;
    ams_mel_rf_product_rx_request *sentinel = (ams_mel_rf_product_rx_request *)&request;
    ams_mel_rf_product_rx_config_v1 config = config_of(4U, 16U);
    ams_mel_rf_job_data_format_t format;
    for (format = 0; format < 16U; ++format) {
        if (format == AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT16) continue;
        config.data_format = format;
        CHECK(ams_mel_rf_data_submit_product_rx(data, &config, &request, NULL, 0, NULL) ==
              AMS_MEL_INVALID_ARGUMENT);
    }
    config = config_of(0U, 16U);
    CHECK(ams_mel_rf_data_submit_product_rx(data, &config, &request, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    config = config_of(4U, 0U);
    CHECK(ams_mel_rf_data_submit_product_rx(data, &config, &request, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    config = config_of(4U, 16U);
    CHECK(ams_mel_rf_data_submit_product_rx(NULL, &config, &request, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_data_submit_product_rx(data, NULL, &request, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_data_submit_product_rx(data, &config, NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    request = sentinel;
    CHECK(ams_mel_rf_data_submit_product_rx(data, &config, &request, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    request = NULL;
    CHECK(ams_mel_rf_data_submit_product_rx(data, &config, &request, NULL, 8U, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(request == NULL && p.creates() == 0U); /* before any provider call */

    /* region_size_bytes passes verbatim, 0 included. */
    config.region_size_bytes = 0U;
    CHECK(ams_mel_rf_data_submit_product_rx(data, &config, &request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(p.last_region_size() == 0U);
    wait_ok(request);
    {
        ams_mel_rf_product_rx *endpoint = NULL;
        ams_mel_rf_product_rx_info_v1 info;
        ams_mel_rf_product_rx_request_result_v1 result;
        ams_mel_rf_product_rx_counters_v1 counters;
        ams_mel_rf_product_rx_event *event = NULL;
        CHECK(ams_mel_rf_product_rx_request_wait(NULL, 0U, &result, NULL, 0, NULL) ==
              AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_rf_product_rx_request_wait(request, 0U, NULL, NULL, 0, NULL) ==
              AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_rf_product_rx_request_claim(request, NULL, &info, NULL, 0, NULL) ==
              AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_rf_product_rx_request_claim(request, &endpoint, NULL, NULL, 0, NULL) ==
              AMS_MEL_INVALID_ARGUMENT);
        endpoint = (ams_mel_rf_product_rx *)&endpoint;
        CHECK(ams_mel_rf_product_rx_request_claim(request, &endpoint, &info, NULL, 0, NULL) ==
              AMS_MEL_INVALID_ARGUMENT);
        endpoint = NULL;
        CHECK(p.registrations() == 0U);
        endpoint = claim(request, &info);
        CHECK(ams_mel_rf_product_rx_receive(NULL, 0U, &event, NULL, 0, NULL) ==
              AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_rf_product_rx_receive(endpoint, 0U, NULL, NULL, 0, NULL) ==
              AMS_MEL_INVALID_ARGUMENT);
        event = (ams_mel_rf_product_rx_event *)&event;
        CHECK(ams_mel_rf_product_rx_receive(endpoint, 0U, &event, NULL, 0, NULL) ==
              AMS_MEL_INVALID_ARGUMENT);
        event = NULL;
        CHECK(ams_mel_rf_product_rx_get_counters(endpoint, NULL, NULL, 0, NULL) ==
              AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_rf_product_rx_get_counters(NULL, &counters, NULL, 0, NULL) ==
              AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_rf_product_rx_event_view(NULL, NULL, NULL, 0, NULL) ==
              AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_rf_product_rx_event_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_rf_product_rx_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_rf_product_rx_request_close(NULL, NULL, 0, NULL) ==
              AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_rf_product_rx_close(&endpoint, NULL, 0, NULL) == AMS_MEL_OK);
    }
    CHECK(ams_mel_rf_product_rx_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_rf_product_rx_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    close_data(&data);
    check_no_forbidden(&p);
    drop_pin(&p);
}

/* Bounded observation of an asynchronous worker's effect (never used to
 * order a race; only to await a state that must eventually hold). */
static int eventually(int (*predicate)(void *), void *context)
{
    const struct timespec step = {0, 2000000L};
    int attempt;
    for (attempt = 0; attempt < 5000; ++attempt) {
        if (predicate(context)) return 1;
        (void)nanosleep(&step, NULL);
    }
    return predicate(context);
}

static int log_has(void *line) { return occurrences((const char *)line) != 0U; }
static int endpoint_destroyed(void *p) { return ((pin *)p)->endpoints_destroyed() != 0U; }
static int provider_unmapped(void *unused) { (void)unused; return !provider_mapped(); }

/* ---------------------------------------------------------- lifecycle */
static void test_claim_unique(void)
{
    ams_mel_rf_data *data = open_rx("rx:delayed");
    pin p = take_pin();
    ams_mel_rf_product_rx_request *request = submit(data, 4U, 16U);
    ams_mel_rf_product_rx_request_result_v1 result = {UINT32_C(0xABCD)};
    ams_mel_rf_product_rx *endpoint = NULL, *second = NULL;
    ams_mel_rf_product_rx_info_v1 info = {9U, 9U};
    char diagnostic[128];
    /* Pending: poll, Wait and Claim do not block and write nothing. */
    CHECK(ams_mel_rf_product_rx_request_wait(request, 0U, &result, NULL, 0, NULL) ==
          AMS_MEL_TIMEOUT);
    CHECK(ams_mel_rf_product_rx_request_wait(request, 5U, &result, NULL, 0, NULL) ==
          AMS_MEL_TIMEOUT);
    CHECK(result.error_code == UINT32_C(0xABCD));
    CHECK(ams_mel_rf_product_rx_request_claim(request, &endpoint, &info, NULL, 0, NULL) ==
          AMS_MEL_TIMEOUT);
    CHECK(endpoint == NULL && info.endpoint_id == 9U);
    CHECK(p.release() == 1U);
    wait_ok(request);
    wait_ok(request);
    endpoint = claim(request, &info);
    CHECK(info.endpoint_id == p.last_endpoint_id());
    CHECK(ams_mel_rf_product_rx_request_claim(request, &second, &info, diagnostic,
                                              sizeof diagnostic, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(second == NULL && strcmp(diagnostic, "RF ProductRx endpoint already claimed") == 0);
    wait_ok(request); /* the cached create result is unchanged */
    CHECK(p.registrations() == 1U);
    CHECK(ams_mel_rf_product_rx_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(p.endpoints_destroyed() == 0U);
    close_endpoint(&endpoint);
    CHECK(p.endpoints_destroyed() == 1U);
    close_data(&data);
    drop_pin(&p);
}

/* Parent-first with a claimed endpoint. */
static void test_parent_first(void)
{
    ams_mel_rf_data *data = open_rx("rx:ok");
    pin p = take_pin();
    ams_mel_rf_product_rx_info_v1 info;
    ams_mel_rf_product_rx *endpoint = open_endpoint(data, 4U, 16U, &info);
    ams_mel_rf_product_rx_event *event;
    close_data(&data);
    CHECK(p.shutdown_calls() == 0U && occurrences("rf_data_destroyed") == 0U);
    CHECK(p.emit(info.endpoint_id, EMIT_RICH, 0U) == 1);
    event = receive_ok(endpoint);
    expect_rich(view_of(event), info.endpoint_id);
    close_endpoint(&endpoint);
    /* The final child ran the deferred shutdown, exactly once. */
    CHECK(p.shutdown_calls() == 1U && occurrences("rf_shutdown") == 1U);
    CHECK(occurrences("rf_shutdown_repeated") == 0U && occurrences("rf_data_destroyed") == 1U);
    CHECK(occurrences("rf_call_after_shutdown") == 0U);
    check_no_forbidden(&p);
    drop_pin(&p);
    CHECK(provider_mapped() && occurrences("library_unloaded") == 0U);
    expect_rich(view_of(event), info.endpoint_id);
    close_event(&event);
}

/* Parent Close while the create future is still pending. */
static void test_pending_parent_first(void)
{
    ams_mel_rf_data *data = open_rx("rx:delayed");
    pin p = take_pin();
    ams_mel_rf_product_rx_request *request = submit(data, 4U, 16U);
    ams_mel_rf_product_rx_info_v1 info;
    ams_mel_rf_product_rx *endpoint;
    ams_mel_rf_product_rx_event *event;
    close_data(&data);
    CHECK(p.shutdown_calls() == 0U);
    CHECK(p.release() == 1U);
    wait_ok(request);
    endpoint = claim(request, &info);
    CHECK(ams_mel_rf_product_rx_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(p.shutdown_calls() == 0U);
    CHECK(p.emit(info.endpoint_id, EMIT_RICH, 0U) == 1);
    event = receive_ok(endpoint);
    expect_rich(view_of(event), info.endpoint_id);
    close_event(&event);
    close_endpoint(&endpoint);
    CHECK(p.shutdown_calls() == 1U && occurrences("rf_data_destroyed") == 1U);
    check_no_forbidden(&p);
    drop_pin(&p);
}

/* Future succeeded, never claimed, then request Close: the WORKER destroys
 * the endpoint without registering a callback and releases the claim. */
static void test_unclaimed(int parent_first)
{
    ams_mel_rf_data *data = open_rx("rx:ok");
    pin p = take_pin();
    ams_mel_rf_product_rx_request *request = submit(data, 4U, 16U);
    wait_ok(request);
    if (parent_first) close_data(&data);
    CHECK(p.shutdown_calls() == 0U && p.endpoints_destroyed() == 0U);
    CHECK(ams_mel_rf_product_rx_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(request == NULL);
    CHECK(eventually(endpoint_destroyed, &p));
    CHECK(p.registrations() == 0U);
    CHECK(ams_mel_test_rf_rx_permanent_registrations() == 0U);
    /* The worker destroys the endpoint BEFORE releasing its child claim, so
     * a parent Close issued right now may still be deferred to the worker.
     * Either way shutdown happens exactly once. */
    if (!parent_first) close_data(&data);
    CHECK(eventually(log_has, "rf_data_destroyed"));
    CHECK(p.shutdown_calls() == 1U && occurrences("rf_shutdown") == 1U);
    check_no_forbidden(&p);
    drop_pin(&p);
    /* No registration ever happened, so the DSO unloads normally (possibly on
     * the worker: library_unloaded is logged inside dlclose before munmap). */
    CHECK(eventually(log_has, "library_unloaded"));
    CHECK(eventually(provider_unmapped, NULL));
}

static void test_never_ready(void)
{
    ams_mel_rf_data *data = open_rx("rx:never");
    pin p = take_pin();
    ams_mel_rf_product_rx_request *request = submit(data, 4U, 16U);
    ams_mel_rf_product_rx_request_result_v1 result;
    CHECK(ams_mel_rf_product_rx_request_wait(request, 0U, &result, NULL, 0, NULL) ==
          AMS_MEL_TIMEOUT);
    CHECK(ams_mel_rf_product_rx_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    close_data(&data);
    /* The worker still owns the future and the child claim: no shutdown, no
     * DataMEL destruction, no unload; nothing claims cancellation. */
    CHECK(p.shutdown_calls() == 0U && occurrences("rf_data_destroyed") == 0U);
    drop_pin(&p);
    CHECK(provider_mapped() && occurrences("library_unloaded") == 0U);
}

/* A callback delivered synchronously inside setDataReadyCallback. */
static void test_sync_callback(void)
{
    ams_mel_rf_data *data = open_rx("rx:ok:sync");
    pin p = take_pin();
    ams_mel_rf_product_rx_info_v1 info;
    ams_mel_rf_product_rx *endpoint = open_endpoint(data, 4U, 16U, &info);
    ams_mel_rf_product_rx_event *event = receive_ok(endpoint);
    const ams_mel_rf_product_rx_event_v1 *view = view_of(event);
    CHECK(view->endpoint_id == info.endpoint_id && view->samples.size == 2U);
    CHECK(view->samples.data[0].real == 77 && view->samples.data[0].imag == -77);
    CHECK(view->samples.data[1].real == -1 && view->samples.data[1].imag == 1);
    expect_rich_metadata(&view->metadata);
    expect_counters(endpoint, 1U, 1U, 0U, 0U, 0U, 0U);
    close_event(&event);
    close_endpoint(&endpoint);
    close_data(&data);
    drop_pin(&p);
}

/* Registration throws, before or after retaining a REFERENCE. Both take the
 * permanent-retention path; no endpoint is published; the claim failure is
 * cached and registration is never attempted twice. */
static void test_registration_throw(const char *scenario)
{
    ams_mel_rf_data *data = open_rx(scenario);
    pin p = take_pin();
    ams_mel_rf_product_rx_request *request = submit(data, 4U, 16U);
    ams_mel_rf_product_rx *endpoint = NULL;
    ams_mel_rf_product_rx_info_v1 info = {5U, 5U};
    ams_mel_test_rf_rx_observer *observer = NULL;
    char first[128], second[128];
    size_t in_flight, queue;
    uint64_t received, after;
    int closed;
    uint64_t id;
    wait_ok(request);
    id = p.last_endpoint_id();
    CHECK(ams_mel_rf_product_rx_request_claim(request, &endpoint, &info, first, sizeof first,
                                              NULL) == AMS_MEL_PROVIDER_EXCEPTION);
    CHECK(endpoint == NULL && info.endpoint_id == 5U);
    CHECK(strstr(first, "mock registration exception") != NULL);
    CHECK(ams_mel_rf_product_rx_request_claim(request, &endpoint, &info, second, sizeof second,
                                              NULL) == AMS_MEL_PROVIDER_EXCEPTION);
    CHECK(endpoint == NULL && strcmp(first, second) == 0);
    CHECK(p.registrations() == 1U);
    CHECK(ams_mel_test_rf_rx_permanent_registrations() == 1U);
    CHECK(p.endpoints_destroyed() == 1U);
    CHECK(ams_mel_test_rf_rx_observer_from_last_registration(&observer) == 1);
    CHECK(ams_mel_rf_product_rx_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    close_data(&data);
    CHECK(p.shutdown_calls() == 1U && occurrences("rf_data_destroyed") == 1U);
    /* A provider that kept the callback may still call it later: safe. */
    if (strstr(scenario, "throw-after") != NULL) {
        int gate[2], done[2];
        char byte = 0;
        CHECK(pipe(gate) == 0 && pipe(done) == 0);
        CHECK(p.arm_late(id, gate[0], done[1]) == 1);
        drop_pin(&p);
        CHECK(write(gate[1], "g", 1U) == 1 && read(done[0], &byte, 1U) == 1 && byte == 'K');
        CHECK(ams_mel_test_rf_rx_observe(observer, &in_flight, &received, &after, &queue,
                                         &closed) == 1);
        CHECK(received == 1U && after == 1U && queue == 0U && in_flight == 0U && closed == 1);
    } else {
        drop_pin(&p);
    }
    ams_mel_test_rf_rx_observer_close(&observer);
    CHECK(provider_mapped() && occurrences("library_unloaded") == 0U);
}

/* Deterministic mid-callback Close via pipes. */
typedef struct close_args {
    ams_mel_rf_product_rx *endpoint;
    int done_fd;
    ams_mel_status_t status;
} close_args;

static void *close_thread(void *raw)
{
    close_args *args = (close_args *)raw;
    args->status = ams_mel_rf_product_rx_close(&args->endpoint, NULL, 0, NULL);
    CHECK(write(args->done_fd, "c", 1U) == 1);
    return NULL;
}

typedef struct emit_args {
    pin *p;
    uint64_t id;
} emit_args;

static void *emit_owned_thread(void *raw)
{
    emit_args *args = (emit_args *)raw;
    CHECK(args->p->emit_owned(args->id) == 1);
    return NULL;
}

/* Bounded wait for a STATE (a Close/cleanup thread blocked in the drain);
 * never ordering evidence by itself. */
static int one_drain_waiter(void *observer)
{
    size_t waiters = 0;
    CHECK(ams_mel_test_rf_rx_drain_waiters((ams_mel_test_rf_rx_observer *)observer,
                                           &waiters) == 1);
    return waiters == 1U;
}

static void expect_order(const char *first, const char *second)
{
    const unsigned a = position(first), b = position(second);
    if (a == 0U || b == 0U || a >= b)
        fprintf(stderr, "order: %s@%u must precede %s@%u\n", first, a, second, b);
    CHECK(a != 0U && b != 0U && a < b);
}

enum { MID_PASS = 0, MID_REVOKED_EARLY = 20 };

/* The defining drain-before-destroy test. A provider thread invokes the
 * callback with ENDPOINT-OWNED samples after dropping its own endpoint
 * reference, so only the bridge endpoint owner keeps that page valid. The
 * bridge holds the callback after it observed Receiving and before any
 * sample read. Returns MID_REVOKED_EARLY (without releasing the callback
 * into a PROT_NONE page) if the endpoint-owned storage was revoked while
 * that callback was still in flight: the old, unsafe Close order. */
static int run_mid_callback(void)
{
    ams_mel_rf_data *data = open_rx("rx:ok:owned");
    pin p = take_pin();
    ams_mel_rf_product_rx_info_v1 info;
    ams_mel_rf_product_rx *endpoint = open_endpoint(data, 4U, 16U, &info);
    ams_mel_test_rf_rx_observer *observer = NULL;
    int entered[2], release[2], closed_pipe[2];
    pthread_t emitter, closer;
    emit_args emit = {NULL, 0U};
    close_args closing;
    char byte;
    size_t in_flight, queue;
    uint64_t received, after;
    int closed;
    CHECK(pipe(entered) == 0 && pipe(release) == 0 && pipe(closed_pipe) == 0);
    CHECK(ams_mel_test_rf_rx_observer_from(endpoint, &observer) == 1);
    ams_mel_test_rf_rx_hold_next(entered[1], release[0]);
    emit.p = &p;
    emit.id = info.endpoint_id;
    CHECK(pthread_create(&emitter, NULL, emit_owned_thread, &emit) == 0);
    /* 4. The callback is in_flight, observed Receiving, and is held before
     *    build_event reads the endpoint-owned samples. */
    CHECK(read(entered[0], &byte, 1U) == 1);
    CHECK(ams_mel_test_rf_rx_observe(observer, &in_flight, &received, &after, &queue,
                                     &closed) == 1);
    CHECK(in_flight == 1U && received == 1U && closed == 0);

    /* 5-6. Close: Closed, Receive woken, then waiting in the drain. */
    closing.endpoint = endpoint;
    closing.done_fd = closed_pipe[1];
    closing.status = AMS_MEL_INTERNAL_ERROR;
    endpoint = NULL;
    CHECK(pthread_create(&closer, NULL, close_thread, &closing) == 0);
    CHECK(eventually(one_drain_waiter, observer));

    /* 7. Close is blocked in the drain while the callback is held. Everything
     *    Close does before the drain has already happened, so under the old
     *    order the endpoint-owned page is already revoked here. */
    CHECK(ams_mel_test_rf_rx_observe(observer, &in_flight, &received, &after, &queue,
                                     &closed) == 1);
    CHECK(in_flight == 1U && closed == 1 && queue == 0U);
    CHECK(occurrences("rf_rx_held_callback_released") == 0U);
    if (occurrences("rf_rx_endpoint_buffer_revoked") != 0U ||
        occurrences("rf_rx_endpoint_destroyed") != 0U)
        return MID_REVOKED_EARLY; /* the held callback is never released */
    CHECK(p.endpoints_destroyed() == 0U);

    /* 8-13. Release: the callback copies from the still-valid page, sees
     *       Closed, publishes nothing, leaves in_flight; only then is the
     *       endpoint destroyed and its page revoked; then Close returns. */
    CHECK(write(release[1], "r", 1U) == 1);
    CHECK(read(closed_pipe[0], &byte, 1U) == 1);
    CHECK(pthread_join(closer, NULL) == 0 && pthread_join(emitter, NULL) == 0);
    CHECK(closing.status == AMS_MEL_OK && closing.endpoint == NULL);
    CHECK(ams_mel_test_rf_rx_observe(observer, &in_flight, &received, &after, &queue,
                                     &closed) == 1);
    CHECK(in_flight == 0U && queue == 0U && received == 1U && after == 1U && closed == 1);
    CHECK(occurrences("rf_rx_endpoint_buffer_revoked") == 1U);
    CHECK(occurrences("rf_rx_endpoint_destroyed") == 1U && p.endpoints_destroyed() == 1U);
    expect_order("rf_rx_held_callback_released", "rf_rx_held_callback_left_in_flight");
    expect_order("rf_rx_held_callback_left_in_flight", "rf_rx_endpoint_buffer_revoked");
    expect_order("rf_rx_endpoint_buffer_revoked", "rf_rx_endpoint_destroyed");
    ams_mel_test_rf_rx_observer_close(&observer);
    close_data(&data);
    CHECK(occurrences("rf_data_destroyed") == 1U);
    check_no_forbidden(&p);
    drop_pin(&p);
    CHECK(provider_mapped() && occurrences("library_unloaded") == 0U);
    return MID_PASS;
}

static void test_mid_callback_close(void) { CHECK(run_mid_callback() == MID_PASS); }

/* TEST-build destructive control 3 restores the old order (Closed, destroy
 * ProductRxEndpoint, then drain) in a forked child. The child must detect
 * endpoint-owned storage revoked while the held pre-Close callback is still
 * in flight and before its release. */
static void test_negative_old_close_order(void)
{
    pid_t child;
    int status = 0;
    fflush(NULL);
    child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        ams_mel_test_rf_rx_negative_control(3U);
        _exit(run_mid_callback());
    }
    CHECK(waitpid(child, &status, 0) == child);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != MID_REVOKED_EARLY)
        fprintf(stderr, "negative control 3: exited=%d code=%d signaled=%d signal=%d\n",
                WIFEXITED(status), WIFEXITED(status) ? WEXITSTATUS(status) : -1,
                WIFSIGNALED(status), WIFSIGNALED(status) ? WTERMSIG(status) : -1);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == MID_REVOKED_EARLY);
    CHECK(occurrences("rf_rx_endpoint_buffer_revoked") == 1U);
    CHECK(occurrences("rf_rx_endpoint_destroyed") == 1U);
    CHECK(occurrences("rf_rx_held_callback_released") == 0U);
    CHECK(occurrences("rf_rx_held_callback_left_in_flight") == 0U);
    printf("negative control 3 detected: endpoint-owned provider storage revoked while "
           "a pre-Close callback was still in flight\n");
}

/* Registration throws while a provider-started callback is inside the
 * bridge Receiving body reading ENDPOINT-OWNED samples. */
typedef struct claim_args {
    ams_mel_rf_product_rx_request *request;
    ams_mel_rf_product_rx *endpoint;
    ams_mel_rf_product_rx_info_v1 info;
    char diagnostic[128];
    ams_mel_status_t status;
    int done_fd;
} claim_args;

static void *claim_thread(void *raw)
{
    claim_args *args = (claim_args *)raw;
    args->status = ams_mel_rf_product_rx_request_claim(args->request, &args->endpoint,
                                                       &args->info, args->diagnostic,
                                                       sizeof args->diagnostic, NULL);
    CHECK(write(args->done_fd, "c", 1U) == 1);
    return NULL;
}

static int one_registration(void *unused)
{
    (void)unused;
    return ams_mel_test_rf_rx_permanent_registrations() == 1U;
}

static void test_registration_throw_active(void)
{
    ams_mel_rf_data *data = open_rx("rx:ok:throw-active");
    pin p = take_pin();
    ams_mel_rf_product_rx_request *request = submit(data, 4U, 16U);
    ams_mel_test_rf_rx_observer *observer = NULL;
    int entered[2], release[2], gate[2], done[2], claimed[2], late_gate[2], late_done[2];
    pthread_t claimer;
    claim_args args;
    char byte = 0;
    size_t in_flight, queue;
    uint64_t received, after;
    int closed;
    uint64_t id;
    wait_ok(request);
    id = p.last_endpoint_id();
    CHECK(pipe(entered) == 0 && pipe(release) == 0 && pipe(gate) == 0 && pipe(done) == 0);
    CHECK(pipe(claimed) == 0 && pipe(late_gate) == 0 && pipe(late_done) == 0);
    ams_mel_test_rf_rx_hold_next(entered[1], release[0]);
    p.set_active_throw(gate[0], done[1]);
    memset(&args, 0, sizeof args);
    args.request = request;
    args.info.endpoint_id = 5U;
    args.status = AMS_MEL_OK;
    args.done_fd = claimed[1];
    CHECK(pthread_create(&claimer, NULL, claim_thread, &args) == 0);

    /* The provider callback is inside the bridge (in_flight, saw Receiving)
     * and held before reading the endpoint-owned samples. */
    CHECK(read(entered[0], &byte, 1U) == 1);
    /* Only now does setDataReadyCallback throw. */
    CHECK(write(gate[1], "t", 1U) == 1);
    CHECK(eventually(one_registration, NULL));
    CHECK(ams_mel_test_rf_rx_observer_from_last_registration(&observer) == 1);
    CHECK(eventually(one_drain_waiter, observer));
    CHECK(ams_mel_test_rf_rx_observe(observer, &in_flight, &received, &after, &queue,
                                     &closed) == 1);
    CHECK(in_flight == 1U && received == 1U && closed == 1 && queue == 0U);
    /* The provider endpoint is NOT destroyed while the callback is held. */
    CHECK(occurrences("rf_rx_endpoint_buffer_revoked") == 0U);
    CHECK(occurrences("rf_rx_endpoint_destroyed") == 0U && p.endpoints_destroyed() == 0U);

    CHECK(write(release[1], "r", 1U) == 1);
    CHECK(read(done[0], &byte, 1U) == 1 && byte == 'K'); /* provider callback returned */
    CHECK(read(claimed[0], &byte, 1U) == 1);
    CHECK(pthread_join(claimer, NULL) == 0);
    CHECK(args.status == AMS_MEL_PROVIDER_EXCEPTION);
    CHECK(args.endpoint == NULL && args.info.endpoint_id == 5U);
    CHECK(strstr(args.diagnostic, "mock registration exception with an active callback") !=
          NULL);
    CHECK(ams_mel_test_rf_rx_observe(observer, &in_flight, &received, &after, &queue,
                                     &closed) == 1);
    CHECK(in_flight == 0U && received == 1U && after == 1U && queue == 0U && closed == 1);
    CHECK(occurrences("rf_rx_endpoint_buffer_revoked") == 1U && p.endpoints_destroyed() == 1U);
    expect_order("rf_rx_held_callback_released", "rf_rx_held_callback_left_in_flight");
    expect_order("rf_rx_held_callback_left_in_flight", "rf_rx_endpoint_buffer_revoked");
    expect_order("rf_rx_endpoint_buffer_revoked", "rf_rx_endpoint_destroyed");
    CHECK(p.registrations() == 1U && ams_mel_test_rf_rx_permanent_registrations() == 1U);

    /* The child claim was released: the parent closes normally. */
    CHECK(ams_mel_rf_product_rx_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    close_data(&data);
    CHECK(p.shutdown_calls() == 1U && occurrences("rf_data_destroyed") == 1U);

    /* A late callback after endpoint AND DataMEL destruction takes the
     * Closed fast path (metadata NULL, PROT_NONE samples, never read). */
    CHECK(p.arm_late(id, late_gate[0], late_done[1]) == 1);
    drop_pin(&p);
    CHECK(write(late_gate[1], "g", 1U) == 1);
    CHECK(read(late_done[0], &byte, 1U) == 1 && byte == 'K');
    CHECK(ams_mel_test_rf_rx_observe(observer, &in_flight, &received, &after, &queue,
                                     &closed) == 1);
    CHECK(in_flight == 0U && received == 2U && after == 2U && queue == 0U && closed == 1);
    ams_mel_test_rf_rx_observer_close(&observer);
    CHECK(provider_mapped() && occurrences("library_unloaded") == 0U);
}

/* ---------------------------------------------------------- late start */
enum { LATE_PASS = 0, LATE_UNMAPPED = 10, LATE_EMPTY_CALLBACK = 11, LATE_BAD_RESULT = 12 };

/* The defining 033C/033D test. Returns a detection code instead of CHECK
 * for the lifetime facts so destructive negative controls are measurable. */
static int run_late(const char *scenario, int pre_emit)
{
    ams_mel_rf_data *data = open_rx(scenario);
    pin p = take_pin();
    ams_mel_rf_product_rx_info_v1 info;
    ams_mel_rf_product_rx *endpoint = open_endpoint(data, 4U, 16U, &info);
    ams_mel_test_rf_rx_observer *observer = NULL;
    ams_mel_rf_product_rx_event *event;
    int gate[2], done[2];
    char byte = 0;
    size_t in_flight, queue;
    uint64_t received, after;
    int closed;

    event = NULL;
    if (pre_emit) {
        CHECK(p.emit(info.endpoint_id, EMIT_RICH, 0U) == 1);
        event = receive_ok(endpoint); /* event A, must survive everything below */
    }
    CHECK(ams_mel_test_rf_rx_observer_from(endpoint, &observer) == 1);
    CHECK(pipe(gate) == 0 && pipe(done) == 0);
    /* A provider late thread keeps the callback and waits on a test pipe.
     * Arming is the LAST provider function this test calls. */
    check_no_forbidden(&p);
    CHECK(p.arm_late(info.endpoint_id, gate[0], done[1]) == 1);
    drop_pin(&p); /* pointers cleared; test-owned handle closed */

    close_endpoint(&endpoint);
    CHECK(occurrences("rf_rx_endpoint_destroyed") == 1U);
    CHECK(ams_mel_test_rf_rx_observe(observer, &in_flight, &received, &after, &queue,
                                     &closed) == 1);
    CHECK(in_flight == 0U && closed == 1 && queue == 0U && after == 0U);
    CHECK(received == (pre_emit ? 1U : 0U));

    close_data(&data);
    CHECK(occurrences("rf_shutdown") == 1U && occurrences("rf_data_destroyed") == 1U);
    if (!provider_mapped() || occurrences("library_unloaded") != 0U) return LATE_UNMAPPED;

    /* Only now start the late callback: metadata NULL, PROT_NONE samples. */
    CHECK(write(gate[1], "g", 1U) == 1);
    if (read(done[0], &byte, 1U) != 1) return LATE_BAD_RESULT;
    if (byte == 'E') return LATE_EMPTY_CALLBACK;
    if (byte != 'K') return LATE_BAD_RESULT;
    CHECK(ams_mel_test_rf_rx_observe(observer, &in_flight, &received, &after, &queue,
                                     &closed) == 1);
    CHECK(received == (pre_emit ? 2U : 1U) && after == 1U && queue == 0U && in_flight == 0U);
    CHECK(closed == 1);
    ams_mel_test_rf_rx_observer_close(&observer);
    if (!provider_mapped() || occurrences("library_unloaded") != 0U) return LATE_UNMAPPED;
    /* Event A is unchanged after endpoint Close, DataMEL destruction, and a
     * later callback. */
    if (event != NULL) {
        expect_rich(view_of(event), info.endpoint_id);
        close_event(&event);
    }
    return LATE_PASS;
}

static void test_late(const char *scenario) { CHECK(run_late(scenario, 1) == LATE_PASS); }

/* Destructive negative control in a forked child. The parent never loads
 * the provider. The child _exits so it cannot remove the shared log. */
static void test_negative_control(unsigned control, const char *scenario, int expected)
{
    pid_t child;
    int status = 0;
    fflush(NULL);
    child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        ams_mel_test_rf_rx_negative_control(control);
        _exit(run_late(scenario, 0));
    }
    CHECK(waitpid(child, &status, 0) == child);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != expected)
        fprintf(stderr, "negative control %u: exited=%d code=%d signaled=%d signal=%d\n",
                control, WIFEXITED(status), WIFEXITED(status) ? WEXITSTATUS(status) : -1,
                WIFSIGNALED(status), WIFSIGNALED(status) ? WTERMSIG(status) : -1);
    if (expected == LATE_UNMAPPED && WIFSIGNALED(status) &&
        (WTERMSIG(status) == SIGSEGV || WTERMSIG(status) == SIGBUS)) {
        /* Without the pin the DSO is unmapped under the provider's own late
         * thread, which may still be executing provider code between the arm
         * handshake and its blocking read(). That crash IS the defect
         * (section 58A: "DSO unmapped / library_unloaded / child crash"). It
         * counts only if the log proves the unload happened first
         * (library_unloaded is recorded inside dlclose, before munmap). */
        CHECK(occurrences("library_unloaded") == 1U);
        CHECK(occurrences("rf_data_destroyed") == 1U);
        printf("negative control %u detected: child crashed (signal %d) after the provider "
               "DSO was unloaded (library_unloaded)\n", control, WTERMSIG(status));
        return;
    }
    /* Non-vacuous: the child reached the lifetime check and detected exactly
     * the defect the control introduces. */
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == expected);
    printf("negative control %u detected: %s\n", control,
           expected == LATE_UNMAPPED ? "provider DSO unmapped (library_unloaded) before the "
                                       "late callback"
                                     : "retained exact callback object was empty at the "
                                       "late callback");
    if (expected == LATE_UNMAPPED) CHECK(occurrences("library_unloaded") == 1U);
    CHECK(occurrences("rf_data_destroyed") == 1U);
}

/* 033B regression in its own process: RF support alone never pins. */
static void test_no_callback_unload(void)
{
    ams_mel_rf_data *data = open_rx("rx:ok");
    ams_mel_rf_mfa_info *info = NULL;
    const ams_mel_rf_mfa_info_v1 *view = NULL;
    CHECK(ams_mel_rf_data_get_mfa_info(data, &info, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_rf_mfa_info_view(info, &view, NULL, 0, NULL) == AMS_MEL_OK);
    close_data(&data);
    CHECK(ams_mel_test_rf_rx_permanent_registrations() == 0U);
    CHECK(occurrences("rf_data_destroyed") == 1U && occurrences("library_unloaded") == 1U);
    CHECK(!provider_mapped());
    CHECK(ams_mel_rf_mfa_info_close(&info, NULL, 0, NULL) == AMS_MEL_OK);
}

/* ---------------------------------------------------------- concurrency */
static void test_multiple_endpoints(void)
{
    ams_mel_rf_data *data = open_rx("rx:ok");
    pin p = take_pin();
    ams_mel_rf_product_rx_info_v1 first_info, second_info;
    ams_mel_rf_product_rx *first = open_endpoint(data, 4U, 16U, &first_info);
    ams_mel_rf_product_rx *second = open_endpoint(data, 1U, 16U, &second_info);
    ams_mel_rf_product_rx_event *event;
    CHECK(first_info.endpoint_id != second_info.endpoint_id);
    CHECK(ams_mel_test_rf_rx_permanent_registrations() == 2U);
    CHECK(p.emit(first_info.endpoint_id, EMIT_RICH, 0U) == 1);
    CHECK(p.emit(second_info.endpoint_id, EMIT_RICH_ALTERNATE, 0U) == 1);
    CHECK(p.emit(second_info.endpoint_id, EMIT_RICH_ALTERNATE, 0U) == 1);
    expect_counters(first, 1U, 1U, 0U, 0U, 0U, 0U);
    expect_counters(second, 2U, 1U, 1U, 0U, 0U, 0U);
    close_endpoint(&first);
    CHECK(p.endpoints_destroyed() == 1U);
    close_data(&data);
    CHECK(p.shutdown_calls() == 0U); /* second is still a child */
    event = receive_ok(second);
    CHECK(view_of(event)->endpoint_id == second_info.endpoint_id);
    expect_alternate(view_of(event));
    close_event(&event);
    CHECK(p.emit(second_info.endpoint_id, EMIT_RICH, 0U) == 1);
    event = receive_ok(second);
    expect_rich(view_of(event), second_info.endpoint_id);
    close_event(&event);
    close_endpoint(&second);
    CHECK(p.shutdown_calls() == 1U && p.endpoints_destroyed() == 2U);
    drop_pin(&p);
}

typedef struct burst_args {
    pin *p;
    uint64_t id;
    size_t count;
} burst_args;

static void *burst_thread(void *raw)
{
    burst_args *args = (burst_args *)raw;
    size_t index;
    for (index = 0; index < args->count; ++index)
        CHECK(args->p->emit(args->id, EMIT_COUNT, 1U + index % 7U) == 1);
    return NULL;
}

static void test_concurrent(void)
{
    ams_mel_rf_data *data = open_rx("rx:ok");
    pin p = take_pin();
    ams_mel_rf_product_rx_info_v1 info;
    ams_mel_rf_product_rx *endpoint = open_endpoint(data, 1U, 16U, &info);
    ams_mel_test_rf_rx_observer *observer = NULL;
    int entered[2], release[2];
    pthread_t held, bursts[4];
    burst_args held_args, burst[4];
    ams_mel_rf_product_rx_event *event;
    const ams_mel_rf_product_rx_event_v1 *view;
    size_t in_flight, queue;
    uint64_t received, after;
    int closed, index;
    char byte;
    CHECK(ams_mel_test_rf_rx_observer_from(endpoint, &observer) == 1);

    /* Deterministic overlap: callback A (2 samples) is held inside the
     * bridge while callback B (3 samples) runs to completion concurrently. */
    CHECK(pipe(entered) == 0 && pipe(release) == 0);
    ams_mel_test_rf_rx_hold_next(entered[1], release[0]);
    held_args.p = &p;
    held_args.id = info.endpoint_id;
    held_args.count = 2U; /* emits count 1 then 2; the first is held */
    CHECK(pthread_create(&held, NULL, burst_thread, &held_args) == 0);
    CHECK(read(entered[0], &byte, 1U) == 1);
    CHECK(p.emit(info.endpoint_id, EMIT_COUNT, 3U) == 1);
    CHECK(ams_mel_test_rf_rx_observe(observer, &in_flight, &received, &after, &queue,
                                     &closed) == 1);
    CHECK(in_flight == 1U && received == 2U && queue == 1U);
    CHECK(write(release[1], "r", 1U) == 1);
    CHECK(pthread_join(held, NULL) == 0);
    /* Capacity 1: B was queued first; A and the later one are dropped. */
    expect_counters(endpoint, 3U, 1U, 2U, 0U, 0U, 0U);
    event = receive_ok(endpoint);
    view = view_of(event);
    CHECK(view->samples.size == 3U && view->samples.data[2].real == 2 &&
          view->samples.data[2].imag == -2);
    close_event(&event);
    close_endpoint(&endpoint);
    ams_mel_test_rf_rx_observer_close(&observer);

    /* Unserialized bursts from four provider threads: exact accounting. */
    endpoint = open_endpoint(data, 100000U, 16U, &info);
    for (index = 0; index < 4; ++index) {
        burst[index].p = &p;
        burst[index].id = info.endpoint_id;
        burst[index].count = 500U;
        CHECK(pthread_create(&bursts[index], NULL, burst_thread, &burst[index]) == 0);
    }
    for (index = 0; index < 4; ++index) CHECK(pthread_join(bursts[index], NULL) == 0);
    expect_counters(endpoint, 2000U, 2000U, 0U, 0U, 0U, 0U);
    for (index = 0; index < 2000; ++index) {
        size_t sample;
        event = receive_ok(endpoint);
        view = view_of(event);
        CHECK(view->samples.size >= 1U && view->samples.size <= 7U);
        for (sample = 0; sample < view->samples.size; ++sample)
            CHECK(view->samples.data[sample].real == (int16_t)sample &&
                  view->samples.data[sample].imag == (int16_t)-(int)sample);
        close_event(&event);
    }
    expect_empty(endpoint);
    close_endpoint(&endpoint);
    close_data(&data);
    drop_pin(&p);
}

typedef struct receive_args {
    ams_mel_rf_product_rx *endpoint;
    ams_mel_status_t status;
    ams_mel_rf_product_rx_event *event;
} receive_args;

static void *receive_thread(void *raw)
{
    receive_args *args = (receive_args *)raw;
    args->status = ams_mel_rf_product_rx_receive(args->endpoint, 60000U, &args->event, NULL, 0,
                                                 NULL);
    return NULL;
}

static int one_waiter(void *observer)
{
    size_t waiters = 0;
    CHECK(ams_mel_test_rf_rx_waiters((ams_mel_test_rf_rx_observer *)observer, &waiters) == 1);
    return waiters == 1U;
}

/* Close wakes a blocked Receive; queued events are discarded by Close. */
static void test_receive_close(void)
{
    ams_mel_rf_data *data = open_rx("rx:ok");
    pin p = take_pin();
    ams_mel_rf_product_rx_info_v1 info;
    ams_mel_rf_product_rx *endpoint = open_endpoint(data, 4U, 16U, &info);
    ams_mel_test_rf_rx_observer *observer = NULL;
    receive_args args = {NULL, AMS_MEL_INTERNAL_ERROR, NULL};
    pthread_t receiver;
    size_t in_flight, queue;
    uint64_t received, after;
    int closed;
    CHECK(ams_mel_test_rf_rx_observer_from(endpoint, &observer) == 1);
    args.endpoint = endpoint;
    CHECK(pthread_create(&receiver, NULL, receive_thread, &args) == 0);
    CHECK(eventually(one_waiter, observer));
    close_endpoint(&endpoint);
    CHECK(pthread_join(receiver, NULL) == 0);
    CHECK(args.status == AMS_MEL_STREAM_STOPPED && args.event == NULL);
    ams_mel_test_rf_rx_observer_close(&observer);

    endpoint = open_endpoint(data, 4U, 16U, &info);
    CHECK(ams_mel_test_rf_rx_observer_from(endpoint, &observer) == 1);
    CHECK(p.emit(info.endpoint_id, EMIT_RICH, 0U) == 1);
    CHECK(p.emit(info.endpoint_id, EMIT_RICH, 0U) == 1);
    close_endpoint(&endpoint);
    /* Closing cleared the queue: permanent retention holds no payload. */
    CHECK(ams_mel_test_rf_rx_observe(observer, &in_flight, &received, &after, &queue,
                                     &closed) == 1);
    CHECK(queue == 0U && closed == 1 && received == 2U);
    ams_mel_test_rf_rx_observer_close(&observer);
    close_data(&data);
    drop_pin(&p);
}

static void test_saturation(void)
{
    ams_mel_rf_data *data = open_rx("rx:ok");
    pin p = take_pin();
    ams_mel_rf_product_rx_info_v1 info;
    ams_mel_rf_product_rx *endpoint = open_endpoint(data, 1U, 16U, &info);
    ams_mel_test_rf_rx_observer *observer = NULL;
    ams_mel_rf_product_rx_counters_v1 c;
    CHECK(ams_mel_test_rf_rx_observer_from(endpoint, &observer) == 1);
    CHECK(ams_mel_test_rf_rx_preset_counters(observer, UINT64_MAX - 1U) == 1);
    CHECK(p.emit(info.endpoint_id, EMIT_RICH, 0U) == 1);          /* queued */
    CHECK(p.emit(info.endpoint_id, EMIT_RICH, 0U) == 1);          /* dropped */
    CHECK(p.emit(info.endpoint_id, EMIT_NULL_METADATA, 0U) == 1); /* malformed */
    CHECK(p.emit(info.endpoint_id, EMIT_NULL_METADATA, 0U) == 1);
    ams_mel_test_rf_rx_failpoint(1U);
    CHECK(p.emit(info.endpoint_id, EMIT_COUNT, 1U) == 1);
    ams_mel_test_rf_rx_failpoint(1U);
    CHECK(p.emit(info.endpoint_id, EMIT_COUNT, 1U) == 1);
    c = counters_of(endpoint);
    CHECK(c.callbacks_received == UINT64_MAX && c.products_queued == UINT64_MAX);
    CHECK(c.products_dropped_queue_full == UINT64_MAX);
    CHECK(c.malformed_or_unsupported == UINT64_MAX && c.allocation_failures == UINT64_MAX);
    CHECK(c.callbacks_after_close == UINT64_MAX - 1U);
    ams_mel_test_rf_rx_observer_close(&observer);
    close_endpoint(&endpoint);
    close_data(&data);
    drop_pin(&p);
}

/* ---------------------------------------------------------- shutdown throw */
/* Deferred shutdown throws on the final child's PUBLIC Close. */
static void test_deferred_shutdown_throw(void)
{
    ams_mel_rf_data *data = open_rx("rx:ok:shutdown-throw");
    pin p = take_pin();
    ams_mel_rf_product_rx_info_v1 info;
    ams_mel_rf_product_rx *endpoint = open_endpoint(data, 4U, 16U, &info);
    char diagnostic[128];
    close_data(&data);
    CHECK(p.shutdown_calls() == 0U);
    CHECK(ams_mel_rf_product_rx_close(&endpoint, diagnostic, sizeof diagnostic, NULL) ==
          AMS_MEL_PROVIDER_EXCEPTION);
    CHECK(endpoint == NULL && strcmp(diagnostic, "mock RF shutdown exception") == 0);
    CHECK(p.shutdown_calls() == 1U && occurrences("rf_shutdown") == 1U);
    /* The COMPLETE unproven graph is retained: no destruction, no unload,
     * no retry. */
    CHECK(occurrences("rf_data_destroyed") == 0U);
    drop_pin(&p);
    CHECK(provider_mapped() && occurrences("library_unloaded") == 0U);
}

/* Deferred shutdown throws on the WORKER (no public caller). */
static void test_worker_shutdown_throw(void)
{
    ams_mel_rf_data *data = open_rx("rx:ok:shutdown-throw");
    pin p = take_pin();
    ams_mel_rf_product_rx_request *request = submit(data, 4U, 16U);
    wait_ok(request);
    close_data(&data);
    CHECK(ams_mel_rf_product_rx_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(eventually(log_has, "rf_shutdown"));
    CHECK(eventually(endpoint_destroyed, &p));
    CHECK(p.shutdown_calls() == 1U && p.registrations() == 0U);
    CHECK(occurrences("rf_data_destroyed") == 0U);
    drop_pin(&p);
    CHECK(provider_mapped() && occurrences("library_unloaded") == 0U);
}

/* Worker launch fails around a valid future: nothing published, the future
 * and its child claim are retained forever (so the parent never unloads). */
static void test_worker_launch_failure(void)
{
    ams_mel_rf_data *data = open_rx("rx:ok");
    pin p = take_pin();
    ams_mel_rf_product_rx_request *request = NULL;
    const ams_mel_rf_product_rx_config_v1 config = config_of(4U, 16U);
    char diagnostic[128];
    ams_mel_test_rf_rx_failpoint(3U);
    CHECK(ams_mel_rf_data_submit_product_rx(data, &config, &request, diagnostic,
                                            sizeof diagnostic, NULL) == AMS_MEL_INTERNAL_ERROR);
    CHECK(request == NULL && strstr(diagnostic, "worker launch failed") != NULL);
    close_data(&data);
    CHECK(p.shutdown_calls() == 0U && occurrences("rf_data_destroyed") == 0U);
    drop_pin(&p);
    CHECK(provider_mapped());
}

/* ---------------------------------------------------------- dispatcher */
int main(int argc, char **argv)
{
    const char *name;
    if (argc != 2) {
        fprintf(stderr, "usage: %s CASE\n", argv[0]);
        return EXIT_FAILURE;
    }
    name = argv[1];
    setup_log();
    if (strcmp(name, "receive") == 0) test_receive();
    else if (strcmp(name, "fail-closed") == 0) test_fail_closed();
    else if (strcmp(name, "malformed") == 0) test_malformed();
    else if (strcmp(name, "queue-full") == 0) test_queue_full();
    else if (strcmp(name, "create-failures") == 0) test_create_failures();
    else if (strcmp(name, "long-diagnostic") == 0) test_long_diagnostic();
    else if (strcmp(name, "invalid") == 0) test_invalid();
    else if (strcmp(name, "claim-unique") == 0) test_claim_unique();
    else if (strcmp(name, "parent-first") == 0) test_parent_first();
    else if (strcmp(name, "pending-parent-first") == 0) test_pending_parent_first();
    else if (strcmp(name, "unclaimed") == 0) test_unclaimed(0);
    else if (strcmp(name, "unclaimed-parent-first") == 0) test_unclaimed(1);
    else if (strcmp(name, "never-ready") == 0) test_never_ready();
    else if (strcmp(name, "sync-callback") == 0) test_sync_callback();
    else if (strcmp(name, "registration-throw-before") == 0)
        test_registration_throw("rx:ok:throw-before");
    else if (strcmp(name, "registration-throw-after") == 0)
        test_registration_throw("rx:ok:throw-after");
    else if (strcmp(name, "registration-throw-active") == 0) test_registration_throw_active();
    else if (strcmp(name, "mid-callback-close") == 0) test_mid_callback_close();
    else if (strcmp(name, "negative-old-close-order") == 0) test_negative_old_close_order();
    else if (strcmp(name, "late-copy") == 0) test_late("rx:ok:copy");
    else if (strcmp(name, "late-reference") == 0) test_late("rx:ok:reference");
    else if (strcmp(name, "late-move") == 0) test_late("rx:ok:move");
    else if (strcmp(name, "negative-no-library-pin") == 0)
        test_negative_control(1U, "rx:ok:copy", LATE_UNMAPPED);
    else if (strcmp(name, "negative-no-exact-lvalue") == 0)
        test_negative_control(2U, "rx:ok:reference", LATE_EMPTY_CALLBACK);
    else if (strcmp(name, "no-callback-unload") == 0) test_no_callback_unload();
    else if (strcmp(name, "multiple-endpoints") == 0) test_multiple_endpoints();
    else if (strcmp(name, "concurrent") == 0) test_concurrent();
    else if (strcmp(name, "receive-close") == 0) test_receive_close();
    else if (strcmp(name, "saturation") == 0) test_saturation();
    else if (strcmp(name, "deferred-shutdown-throw") == 0) test_deferred_shutdown_throw();
    else if (strcmp(name, "worker-shutdown-throw") == 0) test_worker_shutdown_throw();
    else if (strcmp(name, "worker-launch-failure") == 0) test_worker_launch_failure();
    else {
        fprintf(stderr, "unknown case %s\n", name);
        return EXIT_FAILURE;
    }
    printf("PASS: rf-product-rx-%s\n", name);
    return EXIT_SUCCESS;
}
