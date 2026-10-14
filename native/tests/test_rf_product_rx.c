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
#include <math.h>
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

static int selected_vita;
static int selected_medium;
static int selected_large;
static int selected_extra_large;
static int selected_i8;
static int selected_direct = -1;

static ams_mel_rf_product_rx_request *submit(ams_mel_rf_data *data, size_t capacity,
                                             size_t max_samples)
{
    ams_mel_rf_product_rx_request *request = NULL;
    ams_mel_rf_product_rx_config_v1 config = config_of(capacity, max_samples);
    char diagnostic[128];
    if (selected_vita) config.data_format = 4U;
    if (selected_medium) config.data_format = 5U;
    if (selected_large) config.data_format = 6U;
    if (selected_extra_large) config.data_format = 7U;
    if (selected_i8) config.data_format = AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT8;
    if (selected_direct >= 0) config.data_format = (unsigned)selected_direct;
    CHECK((selected_extra_large ? ams_mel_rf_data_submit_product_rx_v7 : selected_large ? ams_mel_rf_data_submit_product_rx_v6 : selected_medium ? ams_mel_rf_data_submit_product_rx_v5 : selected_vita ? ams_mel_rf_data_submit_product_rx_v4 :
           selected_direct >= 0 ? ams_mel_rf_data_submit_product_rx_v3 :
           selected_i8 ? ams_mel_rf_data_submit_product_rx_v2 :
           ams_mel_rf_data_submit_product_rx)(data, &config, &request, diagnostic,
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
    CHECK(info->assigned_data_format == (selected_extra_large ? 7U : selected_large ? 6U : selected_medium ? 5U : selected_vita ? 4U : selected_direct >= 0 ? (unsigned)selected_direct : selected_i8 ?
          AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT8 : AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT16));
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
    if (selected_extra_large) {
        const ams_mel_rf_product_rx_event_v7 *large = NULL;
        CHECK(ams_mel_rf_product_rx_event_view_v7(event, &large, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(large->event.event.event.event.data_format == 7U && large->vita_extra_large_packets.size > 0U);
        CHECK(large->vita_extra_large_packets.data[0].serialized_bytes.size >= 32U);
    } else if (selected_large) {
        const ams_mel_rf_product_rx_event_v6 *large = NULL;
        CHECK(ams_mel_rf_product_rx_event_view_v6(event, &large, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(large->event.event.event.data_format == 6U && large->vita_large_packets.size > 0U);
        CHECK(large->vita_large_packets.data[0].serialized_bytes.size >= 32U);
    } else if (selected_medium) {
        const ams_mel_rf_product_rx_event_v5 *medium = NULL;
        CHECK(ams_mel_rf_product_rx_event_view_v5(event, &medium, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(medium->event.event.data_format == 5U && medium->vita_medium_packets.size > 0U);
        CHECK(medium->vita_medium_packets.data[0].serialized_bytes.size >= 32U);
    } else if (selected_vita) {
        const ams_mel_rf_product_rx_event_v4 *view = NULL;
        CHECK(ams_mel_rf_product_rx_event_view_v4(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(view->event.data_format == 4U && view->vita_small_packets.size > 0U);
        CHECK(view->vita_small_packets.data[0].serialized_bytes.size >= 32U);
    } else if (selected_direct >= 0) {
        const ams_mel_rf_product_rx_event_v3 *view = NULL;
        CHECK(ams_mel_rf_product_rx_event_view_v3(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(view->data_format == (unsigned)selected_direct);
        CHECK(selected_direct == 0 ? view->direct_samples_i8.data[0] == -128 :
              view->direct_samples_i16.data[0] == -32768);
    } else if (selected_i8) {
        const ams_mel_rf_product_rx_event_v2 *view = NULL;
        CHECK(ams_mel_rf_product_rx_event_view_v2(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(view->endpoint_id == info.endpoint_id && view->samples_i8.size == 6U);
        CHECK(view->samples_i8.data[0].real == -128 && view->samples_i8.data[0].imag == 127);
    } else expect_rich(view_of(event), info.endpoint_id);
    close_endpoint(&endpoint);
    /* The final child ran the deferred shutdown, exactly once. */
    CHECK(p.shutdown_calls() == 1U && occurrences("rf_shutdown") == 1U);
    CHECK(occurrences("rf_shutdown_repeated") == 0U && occurrences("rf_data_destroyed") == 1U);
    CHECK(occurrences("rf_call_after_shutdown") == 0U);
    check_no_forbidden(&p);
    drop_pin(&p);
    CHECK(provider_mapped() && occurrences("library_unloaded") == 0U);
    if (!selected_extra_large && !selected_large && !selected_medium && !selected_vita && selected_direct < 0) expect_rich(view_of(event), info.endpoint_id);
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
    if (selected_extra_large) {
        const ams_mel_rf_product_rx_event_v7 *large = NULL;
        CHECK(ams_mel_rf_product_rx_event_view_v7(event, &large, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(large->event.event.event.event.data_format == 7U && large->vita_extra_large_packets.size > 0U);
        CHECK(large->vita_extra_large_packets.data[0].serialized_bytes.size >= 32U);
    } else if (selected_large) {
        const ams_mel_rf_product_rx_event_v6 *large = NULL;
        CHECK(ams_mel_rf_product_rx_event_view_v6(event, &large, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(large->event.event.event.data_format == 6U && large->vita_large_packets.size > 0U);
        CHECK(large->vita_large_packets.data[0].serialized_bytes.size >= 32U);
    } else if (selected_medium) {
        const ams_mel_rf_product_rx_event_v5 *medium = NULL;
        CHECK(ams_mel_rf_product_rx_event_view_v5(event, &medium, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(medium->event.event.data_format == 5U && medium->vita_medium_packets.size > 0U);
        CHECK(medium->vita_medium_packets.data[0].serialized_bytes.size >= 32U);
    } else if (selected_vita) {
        const ams_mel_rf_product_rx_event_v4 *view = NULL;
        CHECK(ams_mel_rf_product_rx_event_view_v4(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(view->event.data_format == 4U && view->vita_small_packets.size > 0U);
        CHECK(view->vita_small_packets.data[0].serialized_bytes.size >= 32U);
    } else if (selected_direct >= 0) {
        const ams_mel_rf_product_rx_event_v3 *view = NULL;
        CHECK(ams_mel_rf_product_rx_event_view_v3(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(view->data_format == (unsigned)selected_direct);
        CHECK(selected_direct == 0 ? view->direct_samples_i8.data[0] == -128 :
              view->direct_samples_i16.data[0] == -32768);
    } else if (selected_i8) {
        const ams_mel_rf_product_rx_event_v2 *view = NULL;
        CHECK(ams_mel_rf_product_rx_event_view_v2(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(view->endpoint_id == info.endpoint_id && view->samples_i8.size == 6U);
        CHECK(view->samples_i8.data[0].real == -128 && view->samples_i8.data[0].imag == 127);
    } else expect_rich(view_of(event), info.endpoint_id);
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
    if (selected_extra_large) {
        const ams_mel_rf_product_rx_event_v7 *large = NULL;
        CHECK(ams_mel_rf_product_rx_event_view_v7(event, &large, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(large->event.event.event.event.data_format == 7U && large->vita_extra_large_packets.size > 0U);
        CHECK(large->vita_extra_large_packets.data[0].serialized_bytes.size >= 32U);
    } else if (selected_large) {
        const ams_mel_rf_product_rx_event_v6 *large = NULL;
        CHECK(ams_mel_rf_product_rx_event_view_v6(event, &large, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(large->event.event.event.data_format == 6U && large->vita_large_packets.size > 0U);
        CHECK(large->vita_large_packets.data[0].serialized_bytes.size >= 32U);
    } else if (selected_medium) {
        const ams_mel_rf_product_rx_event_v5 *medium = NULL;
        CHECK(ams_mel_rf_product_rx_event_view_v5(event, &medium, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(medium->event.event.data_format == 5U && medium->vita_medium_packets.size > 0U);
        CHECK(medium->vita_medium_packets.data[0].serialized_bytes.size >= 32U);
    } else if (selected_vita) {
        const ams_mel_rf_product_rx_event_v4 *view = NULL;
        CHECK(ams_mel_rf_product_rx_event_view_v4(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(view->event.data_format == 4U && view->vita_small_packets.size > 0U);
        CHECK(view->vita_small_packets.data[0].serialized_bytes.size >= 32U);
    } else if (selected_direct >= 0) {
        const ams_mel_rf_product_rx_event_v3 *view = NULL;
        CHECK(ams_mel_rf_product_rx_event_view_v3(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(selected_direct == 0 ? view->direct_samples_i8.size == 2U &&
              view->direct_samples_i8.data[0] == 77 && view->direct_samples_i8.data[1] == -1 :
              view->direct_samples_i16.size == 2U && view->direct_samples_i16.data[0] == 77 &&
              view->direct_samples_i16.data[1] == -1);
    } else {
    const ams_mel_rf_product_rx_event_v1 *view = view_of(event);
    CHECK(view->endpoint_id == info.endpoint_id && view->samples.size == 2U);
    CHECK(view->samples.data[0].real == 77 && view->samples.data[0].imag == -77);
    CHECK(view->samples.data[1].real == -1 && view->samples.data[1].imag == 1);
    expect_rich_metadata(&view->metadata);
    }
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
    if (selected_extra_large) {
        CHECK(occurrences("rf_rx_extra_large_guarded_full_extent") == 1U);
        CHECK(occurrences("rf_rx_extra_large_full_extent_revoked") == 1U);
    } else if (selected_large) {
        CHECK(occurrences("rf_rx_large_guarded_24576_bytes") == 1U);
        CHECK(occurrences("rf_rx_large_all_writable_pages_revoked_guard_preserved") == 1U);
    }
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
        if (selected_extra_large) {
            const ams_mel_rf_product_rx_event_v7 *large = NULL;
            CHECK(ams_mel_rf_product_rx_event_view_v7(event, &large, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(large->event.event.event.event.data_format == 7U && large->vita_extra_large_packets.size > 0U);
        } else if (selected_large) {
            const ams_mel_rf_product_rx_event_v6 *large = NULL;
            CHECK(ams_mel_rf_product_rx_event_view_v6(event, &large, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(large->event.event.event.data_format == 6U && large->vita_large_packets.size > 0U);
        } else if (selected_medium) {
            const ams_mel_rf_product_rx_event_v5 *medium = NULL;
            CHECK(ams_mel_rf_product_rx_event_view_v5(event, &medium, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(medium->event.event.data_format == 5U && medium->vita_medium_packets.size > 0U);
        } else if (selected_vita) {
            const ams_mel_rf_product_rx_event_v4 *view = NULL;
            CHECK(ams_mel_rf_product_rx_event_view_v4(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(view->event.data_format == 4U && view->vita_small_packets.size > 0U);
            CHECK(view->vita_small_packets.data[0].serialized_bytes.size >= 32U);
        } else if (selected_direct >= 0) {
            const ams_mel_rf_product_rx_event_v3 *view = NULL;
            CHECK(ams_mel_rf_product_rx_event_view_v3(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(view->data_format == (unsigned)selected_direct);
            CHECK(selected_direct == 0 ? view->direct_samples_i8.data[0] == -128 :
                  view->direct_samples_i16.data[0] == -32768);
        } else if (selected_i8) {
            const ams_mel_rf_product_rx_event_v2 *view = NULL;
            CHECK(ams_mel_rf_product_rx_event_view_v2(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(view->endpoint_id == info.endpoint_id && view->samples_i8.size == 6U);
            CHECK(view->samples_i8.data[0].real == -128 && view->samples_i8.data[0].imag == 127);
            CHECK(view->samples_i8.data[5].real == 23 && view->samples_i8.data[5].imag == 42);
        } else expect_rich(view_of(event), info.endpoint_id);
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
    ams_mel_rf_product_rx_config_v1 config = config_of(4U, 16U);
    char diagnostic[128];
    if (selected_vita) config.data_format = 4U;
    if (selected_medium) config.data_format = 5U;
    if (selected_large) config.data_format = 6U;
    if (selected_extra_large) config.data_format = 7U;
    if (selected_direct >= 0) config.data_format = (unsigned)selected_direct;
    ams_mel_test_rf_rx_failpoint(3U);
    CHECK((selected_extra_large ? ams_mel_rf_data_submit_product_rx_v7 : selected_large ? ams_mel_rf_data_submit_product_rx_v6 : selected_medium ? ams_mel_rf_data_submit_product_rx_v5 : selected_vita ? ams_mel_rf_data_submit_product_rx_v4 : selected_direct >= 0 ? ams_mel_rf_data_submit_product_rx_v3 : ams_mel_rf_data_submit_product_rx)(data, &config, &request, diagnostic,
                                            sizeof diagnostic, NULL) == AMS_MEL_INTERNAL_ERROR);
    CHECK(request == NULL && strstr(diagnostic, "worker launch failed") != NULL);
    close_data(&data);
    CHECK(p.shutdown_calls() == 0U && occurrences("rf_data_destroyed") == 0U);
    drop_pin(&p);
    CHECK(provider_mapped());
}

static void test_i8_creation(void)
{
    const char *scenarios[] = {"rx:format-mismatch", "rx:getter-throw", "rx:future-throw",
        "rx:invalid-future", "rx:sync-throw", "rx:error-unknown-code", "rx:null-endpoint",
        "rx:width-mismatch"};
    size_t index;
    for (index = 0; index < sizeof scenarios / sizeof scenarios[0]; ++index) {
        ams_mel_rf_data *data = open_rx(scenarios[index]);
        pin p = take_pin();
        ams_mel_rf_product_rx_config_v1 config = config_of(1U, 6U);
        ams_mel_rf_product_rx_request *request = NULL;
        ams_mel_rf_product_rx_request_result_v1 result;
        ams_mel_status_t status;
        config.data_format = AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT8;
        status = ams_mel_rf_data_submit_product_rx_v2(data, &config, &request, NULL, 0, NULL);
        if (index == 3U) CHECK(status == AMS_MEL_PROVIDER_FAILED && request == NULL);
        else if (index == 4U) CHECK(status == AMS_MEL_PROVIDER_EXCEPTION && request == NULL);
        else {
            CHECK(status == AMS_MEL_OK);
            status = ams_mel_rf_product_rx_request_wait(request, 10000U, &result, NULL, 0, NULL);
            CHECK(status == ((index == 1U || index == 2U) ?
                  AMS_MEL_PROVIDER_EXCEPTION : AMS_MEL_PROVIDER_FAILED));
            CHECK(ams_mel_rf_product_rx_request_wait(request, 0U, &result, NULL, 0, NULL) == status);
            CHECK(ams_mel_rf_product_rx_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
        }
        CHECK(p.registrations() == 0U);
        close_data(&data);
        drop_pin(&p);
    }
    {
        ams_mel_rf_data *data = open_rx("rx:ok:sync");
        pin p = take_pin();
        ams_mel_rf_product_rx_info_v1 info;
        ams_mel_rf_product_rx *endpoint;
        ams_mel_rf_product_rx_event *event;
        const ams_mel_rf_product_rx_event_v2 *view = NULL;
        selected_i8 = 1;
        endpoint = open_endpoint(data, 1U, 6U, &info);
        event = receive_ok(endpoint);
        CHECK(ams_mel_rf_product_rx_event_view_v2(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(view->samples_i8.size == 2U);
        CHECK(view->samples_i8.data[0].real == 77 && view->samples_i8.data[0].imag == -77);
        CHECK(view->samples_i8.data[1].real == -1 && view->samples_i8.data[1].imag == 1);
        close_event(&event);
        close_endpoint(&endpoint);
        close_data(&data);
        drop_pin(&p);
        selected_i8 = 0;
    }
}

static void test_direct_creation(void)
{
    const char *scenarios[] = {"rx:format-mismatch", "rx:width-mismatch",
        "rx:representation-mismatch", "rx:getter-throw", "rx:future-throw",
        "rx:invalid-future", "rx:sync-throw", "rx:error-unknown-code", "rx:null-endpoint"};
    unsigned format;
    size_t index;
    for (format = 0U; format < 2U; ++format) {
        for (index = 0U; index < sizeof scenarios / sizeof scenarios[0]; ++index) {
            ams_mel_rf_data *data = open_rx(scenarios[index]);
            pin p = take_pin();
            const unsigned registrations_before = p.registrations();
            ams_mel_rf_product_rx_config_v1 config = config_of(1U, 8U);
            ams_mel_rf_product_rx_request *request = NULL;
            ams_mel_rf_product_rx_request_result_v1 result;
            ams_mel_status_t status;
            config.data_format = format;
            status = ams_mel_rf_data_submit_product_rx_v3(data, &config, &request, NULL, 0, NULL);
            if (index == 5U) CHECK(status == AMS_MEL_PROVIDER_FAILED && request == NULL);
            else if (index == 6U) CHECK(status == AMS_MEL_PROVIDER_EXCEPTION && request == NULL);
            else {
                ams_mel_rf_product_rx *endpoint = NULL;
                ams_mel_rf_product_rx_info_v1 info;
                CHECK(status == AMS_MEL_OK);
                status = ams_mel_rf_product_rx_request_wait(request, 10000U, &result, NULL, 0, NULL);
                CHECK(status == ((index == 3U || index == 4U) ?
                      AMS_MEL_PROVIDER_EXCEPTION : AMS_MEL_PROVIDER_FAILED));
                CHECK(ams_mel_rf_product_rx_request_wait(request, 0U, &result, NULL, 0, NULL) == status);
                CHECK(ams_mel_rf_product_rx_request_claim(request, &endpoint, &info, NULL, 0, NULL)
                      == status && endpoint == NULL);
                CHECK(ams_mel_rf_product_rx_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
            }
            CHECK(p.registrations() == registrations_before);
            close_data(&data);
            drop_pin(&p);
        }
    }
}

static void test_direct(void)
{
    const int8_t values_i8[8] = {-128, 127, -1, 0, 7, -11, 23, 42};
    const int16_t values_i16[8] = {-32768, 32767, -1, 0, 12345, -23456, 17, -19};
    unsigned iteration;
    for (iteration = 0; iteration < 50U; ++iteration) {
        ams_mel_rf_data *data = open_rx("rx:ok:reference");
        pin p = take_pin();
        ams_mel_rf_product_rx *endpoints[4] = {NULL, NULL, NULL, NULL};
        ams_mel_rf_product_rx_info_v1 info[4];
        ams_mel_rf_product_rx_event *events[4] = {NULL, NULL, NULL, NULL};
        const ams_mel_rf_product_rx_event_v3 *views[4];
        ams_mel_rf_product_rx_config_v1 config = config_of(1U, 8U);
        unsigned format, index;
        const unsigned before = p.creates();
        for (format = 4U; format < 16U; ++format) {
            ams_mel_rf_product_rx_request *request = NULL;
            config.data_format = format;
            CHECK(ams_mel_rf_data_submit_product_rx_v3(data, &config, &request, NULL, 0, NULL)
                  == AMS_MEL_INVALID_ARGUMENT);
        }
        config.data_format = UINT32_MAX;
        {
            ams_mel_rf_product_rx_request *request = NULL;
            CHECK(ams_mel_rf_data_submit_product_rx_v3(data, &config, &request, NULL, 0, NULL)
                  == AMS_MEL_INVALID_ARGUMENT);
        }
        CHECK(p.creates() == before);
        for (format = 0U; format < 4U; ++format) {
            ams_mel_rf_product_rx_request *request = NULL;
            config.data_format = format;
            if (format < 2U) {
                CHECK(ams_mel_rf_data_submit_product_rx(data, &config, &request, NULL, 0, NULL)
                      == AMS_MEL_INVALID_ARGUMENT);
                CHECK(ams_mel_rf_data_submit_product_rx_v2(data, &config, &request, NULL, 0, NULL)
                      == AMS_MEL_INVALID_ARGUMENT);
            }
            CHECK(ams_mel_rf_data_submit_product_rx_v3(data, &config, &request, NULL, 0, NULL)
                  == AMS_MEL_OK);
            wait_ok(request);
            CHECK(ams_mel_rf_product_rx_request_claim(request, &endpoints[format],
                  &info[format], NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(info[format].assigned_data_format == format);
            CHECK(ams_mel_rf_product_rx_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(p.emit(info[format].endpoint_id, EMIT_RICH, 0U) == 1);
            events[format] = receive_ok(endpoints[format]);
            CHECK(ams_mel_rf_product_rx_event_view_v3(events[format], &views[format], NULL, 0, NULL)
                  == AMS_MEL_OK);
            CHECK(views[format]->data_format == format);
            CHECK(views[format]->endpoint_id == info[format].endpoint_id);
            if (format != 0U) CHECK(views[format]->direct_samples_i8.data == NULL &&
                                    views[format]->direct_samples_i8.size == 0U);
            if (format != 1U) CHECK(views[format]->direct_samples_i16.data == NULL &&
                                    views[format]->direct_samples_i16.size == 0U);
            if (format != 2U) CHECK(views[format]->complex_samples_i8.data == NULL &&
                                    views[format]->complex_samples_i8.size == 0U);
            if (format != 3U) CHECK(views[format]->complex_samples_i16.data == NULL &&
                                    views[format]->complex_samples_i16.size == 0U);
            if (format < 2U) {
                const ams_mel_rf_product_rx_event_v1 *old = (const void *)(uintptr_t)1U;
                const ams_mel_rf_product_rx_event_v2 *v2 = (const void *)(uintptr_t)1U;
                CHECK(ams_mel_rf_product_rx_event_view(events[format], &old, NULL, 0, NULL)
                      == AMS_MEL_INVALID_ARGUMENT && old == (const void *)(uintptr_t)1U);
                CHECK(ams_mel_rf_product_rx_event_view_v2(events[format], &v2, NULL, 0, NULL)
                      == AMS_MEL_INVALID_ARGUMENT && v2 == (const void *)(uintptr_t)1U);
                CHECK(views[format]->metadata.job_interval_id == 19U + format);
                CHECK(views[format]->metadata.rx_stream_ids.size == 3U);
                CHECK(views[format]->metadata.rx_stream_ids.data[0] == 19U + format);
                CHECK(views[format]->metadata.rx_stream_ids.data[1] == 0U);
                CHECK(views[format]->metadata.rx_stream_ids.data[2] == 42U);
            } else {
                const ams_mel_rf_product_rx_event_v2 *v2 = NULL;
                CHECK(ams_mel_rf_product_rx_event_view_v2(events[format], &v2, NULL, 0, NULL)
                      == AMS_MEL_OK);
                CHECK(v2->samples_i8.data == views[format]->complex_samples_i8.data);
                CHECK(v2->samples_i16.data == views[format]->complex_samples_i16.data);
                CHECK(v2->metadata.rx_stream_ids.data == views[format]->metadata.rx_stream_ids.data);
            }
        }
        CHECK(views[0]->direct_samples_i8.size == 8U);
        CHECK(views[1]->direct_samples_i16.size == 8U);
        for (format = 0U; format < 2U; ++format) {
            const int bad[] = {EMIT_NULL_METADATA, EMIT_WRONG_VARIANT, EMIT_NULL_POINTER,
                EMIT_USER_DATA, EMIT_STAB_POINTS, EMIT_RECEIVE_EVENTS, EMIT_ASSOCIATIONS, 13, 14};
            size_t n;
            for (n = 0; n < sizeof bad / sizeof bad[0]; ++n)
                CHECK(p.emit(info[format].endpoint_id, bad[n], 0U) == 1);
            CHECK(p.emit(info[format].endpoint_id, EMIT_COUNT, 9U) == 1);
            CHECK(p.emit(info[format].endpoint_id, EMIT_COUNT, SIZE_MAX) == 1);
            CHECK(counters_of(endpoints[format]).malformed_or_unsupported == 11U);
            CHECK(p.emit(info[format].endpoint_id, EMIT_ZERO_NULL, 0U) == 1);
            {
                ams_mel_rf_product_rx_event *empty = receive_ok(endpoints[format]);
                const ams_mel_rf_product_rx_event_v3 *v = NULL;
                CHECK(ams_mel_rf_product_rx_event_view_v3(empty, &v, NULL, 0, NULL) == AMS_MEL_OK);
                CHECK(v->data_format == format);
                CHECK(v->direct_samples_i8.data == NULL && v->direct_samples_i8.size == 0U);
                CHECK(v->direct_samples_i16.data == NULL && v->direct_samples_i16.size == 0U);
                CHECK(v->complex_samples_i8.data == NULL && v->complex_samples_i8.size == 0U);
                CHECK(v->complex_samples_i16.data == NULL && v->complex_samples_i16.size == 0U);
                close_event(&empty);
            }
            ams_mel_test_rf_rx_failpoint(1U);
            CHECK(p.emit(info[format].endpoint_id, EMIT_COUNT, 1U) == 1);
            CHECK(counters_of(endpoints[format]).allocation_failures == 1U);
            CHECK(p.emit(info[format].endpoint_id, EMIT_COUNT, 1U) == 1);
            CHECK(p.emit(info[format].endpoint_id, EMIT_RICH, 0U) == 1);
            CHECK(counters_of(endpoints[format]).products_dropped_queue_full == 1U);
            {
                ams_mel_rf_product_rx_event *one = receive_ok(endpoints[format]);
                const ams_mel_rf_product_rx_event_v3 *v = NULL;
                CHECK(ams_mel_rf_product_rx_event_view_v3(one, &v, NULL, 0, NULL) == AMS_MEL_OK);
                CHECK(format == 0U ? v->direct_samples_i8.size == 1U && v->direct_samples_i8.data[0] == -128 :
                      v->direct_samples_i16.size == 1U && v->direct_samples_i16.data[0] == -32768);
                close_event(&one);
            }
        }
        close_data(&data);
        for (format = 0U; format < 4U; ++format) close_endpoint(&endpoints[format]);
        for (index = 0U; index < 8U; ++index) {
            CHECK(views[0]->direct_samples_i8.data[index] == values_i8[index]);
            CHECK(views[1]->direct_samples_i16.data[index] == values_i16[index]);
        }
        CHECK(views[2]->complex_samples_i8.data[0].real == -128);
        CHECK(views[3]->complex_samples_i16.data[0].real == 10);
        for (format = 0U; format < 4U; ++format) close_event(&events[format]);
        drop_pin(&p);
    }
}

static void test_i8(void)
{
    unsigned iteration;
    for (iteration = 0; iteration < 50U; ++iteration) {
        ams_mel_rf_data *data = open_rx("rx:ok:reference");
        pin p = take_pin();
        ams_mel_rf_product_rx_config_v1 config = config_of(1U, 6U);
        const unsigned creates_before = p.creates();
        ams_mel_rf_product_rx_request *request = NULL;
        ams_mel_rf_product_rx *endpoint = NULL;
        ams_mel_rf_product_rx *i16;
        ams_mel_rf_product_rx_info_v1 info, info16;
        ams_mel_rf_product_rx_event *event = NULL, *event16 = NULL;
        const ams_mel_rf_product_rx_event_v2 *view = NULL, *view16 = NULL;
        const ams_mel_rf_product_rx_event_v1 *old = NULL;
        const int8_t values[6][2] = {{-128, 127}, {127, -128}, {-1, 0},
                                    {0, -1}, {7, -11}, {23, 42}};
        unsigned format;
        config.data_format = AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT8;
        CHECK(ams_mel_rf_data_submit_product_rx(data, &config, &request, NULL, 0, NULL)
              == AMS_MEL_INVALID_ARGUMENT);
        CHECK(p.creates() == creates_before);
        for (format = 0; format < 16U; ++format) {
            if (format == 2U || format == 3U) continue;
            config.data_format = format;
            CHECK(ams_mel_rf_data_submit_product_rx_v2(data, &config, &request, NULL, 0, NULL)
                  == AMS_MEL_INVALID_ARGUMENT);
        }
        config.data_format = UINT32_MAX;
        CHECK(ams_mel_rf_data_submit_product_rx_v2(data, &config, &request, NULL, 0, NULL)
              == AMS_MEL_INVALID_ARGUMENT);
        CHECK(p.creates() == creates_before);
        config.data_format = AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT8;
        CHECK(ams_mel_rf_data_submit_product_rx_v2(data, &config, &request, NULL, 0, NULL)
              == AMS_MEL_OK);
        wait_ok(request);
        CHECK(ams_mel_rf_product_rx_request_claim(request, &endpoint, &info, NULL, 0, NULL)
              == AMS_MEL_OK);
        CHECK(info.assigned_data_format == AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT8);
        CHECK(ams_mel_rf_product_rx_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
        config.data_format = AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT16;
        CHECK(ams_mel_rf_data_submit_product_rx_v2(data, &config, &request, NULL, 0, NULL)
              == AMS_MEL_OK);
        wait_ok(request);
        i16 = claim(request, &info16);
        CHECK(ams_mel_rf_product_rx_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(p.emit(info.endpoint_id, EMIT_RICH, 0) == 1);
        CHECK(p.emit(info16.endpoint_id, EMIT_RICH, 0) == 1);
        CHECK(p.emit(info.endpoint_id, EMIT_RICH, 0) == 1);
        CHECK(ams_mel_rf_product_rx_receive(endpoint, 0, &event, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_rf_product_rx_event_view_v2(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(view->data_format == 2U && view->samples_i8.size == 6U);
        CHECK(view->samples_i16.data == NULL && view->samples_i16.size == 0U);
        CHECK(view->metadata.job_interval_id == 18U);
        CHECK(view->metadata.rx_stream_ids.size == 3U);
        CHECK(view->metadata.rx_stream_ids.data[0] == 18U &&
              view->metadata.rx_stream_ids.data[1] == 0U &&
              view->metadata.rx_stream_ids.data[2] == 18U);
        CHECK(ams_mel_rf_product_rx_receive(i16, 0, &event16, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_rf_product_rx_event_view(event16, &old, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_rf_product_rx_event_view_v2(event16, &view16, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(view16->samples_i8.data == NULL && view16->samples_i8.size == 0U);
        CHECK(view16->samples_i16.data == old->samples.data &&
              view16->samples_i16.size == old->samples.size);
        CHECK(memcmp(&view16->metadata, &old->metadata, sizeof old->metadata) == 0);
        {
            const ams_mel_rf_product_rx_event_v1 *sentinel = old;
            CHECK(ams_mel_rf_product_rx_event_view(event, &sentinel, NULL, 0, NULL)
                  == AMS_MEL_INVALID_ARGUMENT);
            CHECK(sentinel == old);
        }
        CHECK(p.emit(info.endpoint_id, EMIT_WRONG_VARIANT, 1) == 1);
        CHECK(p.emit(info.endpoint_id, EMIT_WRONG_VARIANT, 0) == 1);
        CHECK(p.emit(info.endpoint_id, EMIT_NULL_POINTER, 1) == 1);
        CHECK(p.emit(info.endpoint_id, EMIT_COUNT, SIZE_MAX) == 1);
        CHECK(p.emit(info.endpoint_id, EMIT_NULL_METADATA, 0) == 1);
        CHECK(p.emit(info.endpoint_id, EMIT_USER_DATA, 0) == 1);
        CHECK(p.emit(info.endpoint_id, EMIT_STAB_POINTS, 0) == 1);
        CHECK(p.emit(info.endpoint_id, EMIT_RECEIVE_EVENTS, 0) == 1);
        CHECK(p.emit(info.endpoint_id, EMIT_ASSOCIATIONS, 0) == 1);
        CHECK(p.emit(info.endpoint_id, 13, 0) == 1);
        ams_mel_test_rf_rx_failpoint(1U);
        CHECK(p.emit(info.endpoint_id, EMIT_RICH, 0) == 1);
        expect_counters(endpoint, 13U, 1U, 1U, 10U, 1U, 0U);
        CHECK(p.emit(info.endpoint_id, EMIT_ZERO_NULL, 0) == 1);
        {
            ams_mel_rf_product_rx_event *empty = NULL;
            const ams_mel_rf_product_rx_event_v2 *empty_view = NULL;
            CHECK(ams_mel_rf_product_rx_receive(endpoint, 0, &empty, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_rf_product_rx_event_view_v2(empty, &empty_view, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(empty_view->samples_i8.data == NULL && empty_view->samples_i8.size == 0U);
            CHECK(ams_mel_rf_product_rx_event_close(&empty, NULL, 0, NULL) == AMS_MEL_OK);
        }
        CHECK(p.emit(info.endpoint_id, EMIT_COUNT, 1) == 1);
        {
            ams_mel_rf_product_rx_event *one = NULL;
            const ams_mel_rf_product_rx_event_v2 *one_view = NULL;
            CHECK(ams_mel_rf_product_rx_receive(endpoint, 0, &one, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_rf_product_rx_event_view_v2(one, &one_view, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(one_view->samples_i8.size == 1U && one_view->samples_i8.data[0].real == -128);
            CHECK(ams_mel_rf_product_rx_event_close(&one, NULL, 0, NULL) == AMS_MEL_OK);
        }
        close_endpoint(&endpoint);
        close_endpoint(&i16);
        close_data(&data);
        for (format = 0; format < 6U; ++format) {
            CHECK(view->samples_i8.data[format].real == values[format][0]);
            CHECK(view->samples_i8.data[format].imag == values[format][1]);
        }
        CHECK(ams_mel_rf_product_rx_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_rf_product_rx_event_close(&event16, NULL, 0, NULL) == AMS_MEL_OK);
        check_no_forbidden(&p);
        drop_pin(&p);
    }
}

static void check_vita_packet(const ams_mel_rf_vita_small_packet_v1 *packet,
                              unsigned payload_words, unsigned seed)
{
    const uint8_t *bytes = packet->serialized_bytes.data;
    const unsigned words = payload_words + 8U;
    uint32_t expected[128];
    unsigned i, j;
    expected[0] = UINT32_C(0x1ce00000) | ((seed & 15U) << 16) | words;
    expected[1] = UINT32_C(0x12345600) + seed;
    expected[2] = UINT32_C(0x00aaaaaa);
    expected[3] = UINT32_C(0x05000d05);
    expected[4] = UINT32_C(0x23456700) + seed;
    expected[5] = UINT32_C(0x34567800) + seed;
    expected[6] = UINT32_C(0x45678900) + seed;
    for (i = 0; i < payload_words; ++i)
        expected[7U + i] = UINT32_C(0x01020304) + i * UINT32_C(0x070b0d11) + seed;
    expected[words - 1U] = UINT32_C(0x00c00000) | ((seed & 3U) << 10);
    CHECK(bytes != NULL && packet->serialized_bytes.size == words * 4U);
    for (i = 0; i < words; ++i)
        for (j = 0; j < 4U; ++j)
            CHECK(bytes[i * 4U + j] == ((expected[i] >> (24U - j * 8U)) & 255U));
}

static void test_vita(void)
{
    unsigned iteration;
    for (iteration = 0; iteration < 50U; ++iteration) {
        ams_mel_rf_data *data = open_rx("rx:ok:reference");
        pin p = take_pin();
        ams_mel_rf_product_rx_config_v1 config = config_of(1U, 3U);
        ams_mel_rf_product_rx_request *request = NULL;
        ams_mel_rf_product_rx *endpoint = NULL;
        ams_mel_rf_product_rx_info_v1 info;
        ams_mel_rf_product_rx_event *event, *empty;
        const ams_mel_rf_product_rx_event_v4 *view = NULL;
        unsigned format;
        const unsigned creates = p.creates();
        config.data_format = 4U;
        CHECK(ams_mel_rf_data_submit_product_rx(data, &config, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_rf_data_submit_product_rx_v2(data, &config, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_rf_data_submit_product_rx_v3(data, &config, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        for (format = 5U; format <= 14U; ++format) {
            config.data_format = format == 14U ? UINT32_MAX : format;
            CHECK(ams_mel_rf_data_submit_product_rx_v4(data, &config, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        }
        CHECK(p.creates() == creates && request == NULL);
        for (format = 0U; format < 5U; ++format) {
            config.data_format = format;
            CHECK(ams_mel_rf_data_submit_product_rx_v4(data, &config, &request, NULL, 0, NULL) == AMS_MEL_OK);
            wait_ok(request);
            CHECK(ams_mel_rf_product_rx_request_claim(request, &endpoint, &info, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(info.assigned_data_format == format);
            CHECK(ams_mel_rf_product_rx_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
            if (format < 4U) {
                const ams_mel_rf_product_rx_event_v3 *v3 = NULL;
                CHECK(p.emit(info.endpoint_id, EMIT_COUNT, 1U) == 1);
                event = receive_ok(endpoint);
                CHECK(ams_mel_rf_product_rx_event_view_v4(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
                CHECK(ams_mel_rf_product_rx_event_view_v3(event, &v3, NULL, 0, NULL) == AMS_MEL_OK);
                CHECK(memcmp(&view->event, v3, sizeof *v3) == 0);
                CHECK(view->vita_small_packets.data == NULL && view->vita_small_packets.size == 0U);
                close_event(&event);
                close_endpoint(&endpoint);
            }
        }
        CHECK(p.emit(info.endpoint_id, EMIT_RICH, 0U) == 1);
        event = receive_ok(endpoint);
        CHECK(ams_mel_rf_product_rx_event_view_v4(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(view->event.data_format == 4U && view->vita_small_packets.size == 3U);
        CHECK(view->event.direct_samples_i8.data == NULL && view->event.direct_samples_i8.size == 0U);
        CHECK(view->event.direct_samples_i16.data == NULL && view->event.direct_samples_i16.size == 0U);
        CHECK(view->event.complex_samples_i8.data == NULL && view->event.complex_samples_i8.size == 0U);
        CHECK(view->event.complex_samples_i16.data == NULL && view->event.complex_samples_i16.size == 0U);
        CHECK(view->event.metadata.job_interval_id == 21U);
        CHECK(view->event.metadata.rx_stream_ids.size == 3U);
        CHECK(view->event.metadata.rx_stream_ids.data[0] == 21U);
        {
            const ams_mel_rf_product_rx_event_v1 *v1 = (const void *)(uintptr_t)1;
            const ams_mel_rf_product_rx_event_v2 *v2 = (const void *)(uintptr_t)1;
            const ams_mel_rf_product_rx_event_v3 *v3 = (const void *)(uintptr_t)1;
            CHECK(ams_mel_rf_product_rx_event_view(event, &v1, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT && v1 == (const void *)(uintptr_t)1);
            CHECK(ams_mel_rf_product_rx_event_view_v2(event, &v2, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT && v2 == (const void *)(uintptr_t)1);
            CHECK(ams_mel_rf_product_rx_event_view_v3(event, &v3, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT && v3 == (const void *)(uintptr_t)1);
        }
        {
            const int bad[] = {1, 2, 3, 6, 7, 8, 9, 13, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
            size_t i;
            for (i = 0; i < sizeof bad / sizeof bad[0]; ++i)
                CHECK(p.emit(info.endpoint_id, bad[i], 0U) == 1);
            CHECK(p.emit(info.endpoint_id, EMIT_COUNT, 4U) == 1);
            CHECK(p.emit(info.endpoint_id, EMIT_COUNT, SIZE_MAX) == 1);
            CHECK(counters_of(endpoint).malformed_or_unsupported == 22U);
        }
        empty = NULL;
        {
            ams_mel_rf_product_rx_request *large_request = NULL;
            ams_mel_rf_product_rx *large_endpoint = NULL;
            ams_mel_rf_product_rx_info_v1 large_info;
            config.data_format = 4U;
            config.max_samples_per_event = SIZE_MAX;
            CHECK(ams_mel_rf_data_submit_product_rx_v4(data, &config, &large_request, NULL, 0, NULL) == AMS_MEL_OK);
            wait_ok(large_request);
            CHECK(ams_mel_rf_product_rx_request_claim(large_request, &large_endpoint, &large_info, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_rf_product_rx_request_close(&large_request, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(p.emit(large_info.endpoint_id, EMIT_COUNT, SIZE_MAX / 512U + 1U) == 1);
            CHECK(counters_of(large_endpoint).malformed_or_unsupported == 1U);
            close_endpoint(&large_endpoint);
        }
        ams_mel_test_rf_rx_failpoint(4U);
        CHECK(p.emit(info.endpoint_id, EMIT_RICH, 0U) == 1);
        CHECK(counters_of(endpoint).allocation_failures == 1U);
        CHECK(ams_mel_rf_product_rx_receive(endpoint, 0U, &empty, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
        CHECK(p.emit(info.endpoint_id, EMIT_ZERO_NULL, 0U) == 1);
        CHECK(p.emit(info.endpoint_id, EMIT_RICH, 0U) == 1);
        CHECK(counters_of(endpoint).products_dropped_queue_full == 1U);
        empty = receive_ok(endpoint);
        {
            const ams_mel_rf_product_rx_event_v4 *v = NULL;
            CHECK(ams_mel_rf_product_rx_event_view_v4(empty, &v, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(v->event.data_format == 4U && v->vita_small_packets.size == 0U && v->vita_small_packets.data == NULL);
        }
        close_event(&empty);
        CHECK(p.emit(info.endpoint_id, EMIT_RICH, 0U) == 1);
        empty = receive_ok(endpoint);
        close_event(&empty);
        close_data(&data);
        close_endpoint(&endpoint);
        check_vita_packet(&view->vita_small_packets.data[0], 0U, 1U);
        check_vita_packet(&view->vita_small_packets.data[1], 9U, 10U);
        check_vita_packet(&view->vita_small_packets.data[2], 120U, 121U);
        close_event(&event);
        drop_pin(&p);
    }
}

static void check_medium_packet(const ams_mel_rf_vita_medium_packet_v1 *packet,
                              unsigned payload_words, unsigned seed)
{
    const uint8_t *bytes = packet->serialized_bytes.data;
    const unsigned words = payload_words + 8U;
    uint32_t expected[256];
    unsigned i, j;
    expected[0] = UINT32_C(0x1ce00000) | ((seed & 15U) << 16) | words;
    expected[1] = UINT32_C(0x12345600) + seed;
    expected[2] = UINT32_C(0x00aaaaaa);
    expected[3] = UINT32_C(0x05000d05);
    expected[4] = UINT32_C(0x23456700) + seed;
    expected[5] = UINT32_C(0x34567800) + seed;
    expected[6] = UINT32_C(0x45678900) + seed;
    for (i = 0; i < payload_words; ++i)
        expected[7U + i] = UINT32_C(0x01020304) + i * UINT32_C(0x070b0d11) + seed;
    expected[words - 1U] = UINT32_C(0x00c00000) | ((seed & 3U) << 10);
    CHECK(bytes != NULL && packet->serialized_bytes.size == words * 4U);
    for (i = 0; i < words; ++i)
        for (j = 0; j < 4U; ++j)
            CHECK(bytes[i * 4U + j] == ((expected[i] >> (24U - j * 8U)) & 255U));
}

static void test_medium(void)
{
    unsigned iteration;
    for (iteration = 0; iteration < 50U; ++iteration) {
        ams_mel_rf_data *data = open_rx("rx:ok:reference");
        pin p = take_pin();
        ams_mel_rf_product_rx_config_v1 config = config_of(1U, 3U);
        ams_mel_rf_product_rx_request *request = NULL;
        ams_mel_rf_product_rx *endpoint = NULL;
        ams_mel_rf_product_rx_info_v1 info;
        ams_mel_rf_product_rx_event *event, *empty;
        const ams_mel_rf_product_rx_event_v5 *view = NULL;
        unsigned format;
        const unsigned creates = p.creates();
        config.data_format = 5U;
        CHECK(ams_mel_rf_data_submit_product_rx(data, &config, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_rf_data_submit_product_rx_v2(data, &config, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_rf_data_submit_product_rx_v3(data, &config, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_rf_data_submit_product_rx_v4(data, &config, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        for (format = 6U; format <= 14U; ++format) {
            config.data_format = format == 14U ? UINT32_MAX : format;
            CHECK(ams_mel_rf_data_submit_product_rx_v5(data, &config, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        }
        CHECK(p.creates() == creates && request == NULL);
        for (format = 0U; format < 6U; ++format) {
            config.data_format = format;
            CHECK(ams_mel_rf_data_submit_product_rx_v5(data, &config, &request, NULL, 0, NULL) == AMS_MEL_OK);
            wait_ok(request);
            CHECK(ams_mel_rf_product_rx_request_claim(request, &endpoint, &info, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(info.assigned_data_format == format);
            CHECK(ams_mel_rf_product_rx_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
            if (format < 5U) {
                const ams_mel_rf_product_rx_event_v4 *v3 = NULL;
                CHECK(p.emit(info.endpoint_id, EMIT_COUNT, 1U) == 1);
                event = receive_ok(endpoint);
                CHECK(ams_mel_rf_product_rx_event_view_v5(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
                CHECK(ams_mel_rf_product_rx_event_view_v4(event, &v3, NULL, 0, NULL) == AMS_MEL_OK);
                CHECK(memcmp(&view->event, v3, sizeof *v3) == 0);
                CHECK(view->vita_medium_packets.data == NULL && view->vita_medium_packets.size == 0U);
                close_event(&event);
                close_endpoint(&endpoint);
            }
        }
        CHECK(p.emit(info.endpoint_id, EMIT_RICH, 0U) == 1);
        event = receive_ok(endpoint);
        CHECK(ams_mel_rf_product_rx_event_view_v5(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(view->event.event.data_format == 5U && view->vita_medium_packets.size == 3U);
        CHECK(view->event.event.direct_samples_i8.data == NULL && view->event.event.direct_samples_i8.size == 0U);
        CHECK(view->event.event.direct_samples_i16.data == NULL && view->event.event.direct_samples_i16.size == 0U);
        CHECK(view->event.event.complex_samples_i8.data == NULL && view->event.event.complex_samples_i8.size == 0U);
        CHECK(view->event.event.complex_samples_i16.data == NULL && view->event.event.complex_samples_i16.size == 0U);
        CHECK(view->event.vita_small_packets.data == NULL && view->event.vita_small_packets.size == 0U);
        CHECK(view->event.event.metadata.job_interval_id == 22U);
        CHECK(view->event.event.metadata.rx_stream_ids.size == 3U);
        CHECK(view->event.event.metadata.rx_stream_ids.data[0] == 22U);
        {
            const ams_mel_rf_product_rx_event_v1 *v1 = (const void *)(uintptr_t)1;
            const ams_mel_rf_product_rx_event_v2 *v2 = (const void *)(uintptr_t)1;
            const ams_mel_rf_product_rx_event_v3 *v3 = (const void *)(uintptr_t)1;
            const ams_mel_rf_product_rx_event_v4 *v4 = (const void *)(uintptr_t)1;
            CHECK(ams_mel_rf_product_rx_event_view_v4(event, &v4, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT && v4 == (const void *)(uintptr_t)1);
            CHECK(ams_mel_rf_product_rx_event_view(event, &v1, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT && v1 == (const void *)(uintptr_t)1);
            CHECK(ams_mel_rf_product_rx_event_view_v2(event, &v2, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT && v2 == (const void *)(uintptr_t)1);
            CHECK(ams_mel_rf_product_rx_event_view_v3(event, &v3, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT && v3 == (const void *)(uintptr_t)1);
        }
        {
            const int bad[] = {1, 2, 3, 6, 7, 8, 9, 13, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
            size_t i;
            for (i = 0; i < sizeof bad / sizeof bad[0]; ++i)
                CHECK(p.emit(info.endpoint_id, bad[i], 0U) == 1);
            CHECK(p.emit(info.endpoint_id, EMIT_COUNT, 4U) == 1);
            CHECK(p.emit(info.endpoint_id, EMIT_COUNT, SIZE_MAX) == 1);
            CHECK(counters_of(endpoint).malformed_or_unsupported == 23U);
        }
        empty = NULL;
        {
            ams_mel_rf_product_rx_request *large_request = NULL;
            ams_mel_rf_product_rx *large_endpoint = NULL;
            ams_mel_rf_product_rx_info_v1 large_info;
            config.data_format = 5U;
            config.max_samples_per_event = SIZE_MAX;
            CHECK(ams_mel_rf_data_submit_product_rx_v5(data, &config, &large_request, NULL, 0, NULL) == AMS_MEL_OK);
            wait_ok(large_request);
            CHECK(ams_mel_rf_product_rx_request_claim(large_request, &large_endpoint, &large_info, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_rf_product_rx_request_close(&large_request, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(p.emit(large_info.endpoint_id, EMIT_COUNT, SIZE_MAX / 1024U + 1U) == 1);
            CHECK(counters_of(large_endpoint).malformed_or_unsupported == 1U);
            close_endpoint(&large_endpoint);
        }
        ams_mel_test_rf_rx_failpoint(4U);
        CHECK(p.emit(info.endpoint_id, EMIT_RICH, 0U) == 1);
        CHECK(counters_of(endpoint).allocation_failures == 1U);
        CHECK(ams_mel_rf_product_rx_receive(endpoint, 0U, &empty, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
        CHECK(p.emit(info.endpoint_id, EMIT_ZERO_NULL, 0U) == 1);
        CHECK(p.emit(info.endpoint_id, EMIT_RICH, 0U) == 1);
        CHECK(counters_of(endpoint).products_dropped_queue_full == 1U);
        empty = receive_ok(endpoint);
        {
            const ams_mel_rf_product_rx_event_v5 *v = NULL;
            CHECK(ams_mel_rf_product_rx_event_view_v5(empty, &v, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(v->event.event.data_format == 5U && v->vita_medium_packets.size == 0U && v->vita_medium_packets.data == NULL);
        }
        close_event(&empty);
        CHECK(p.emit(info.endpoint_id, EMIT_RICH, 0U) == 1);
        empty = receive_ok(endpoint);
        close_event(&empty);
        close_data(&data);
        close_endpoint(&endpoint);
        check_medium_packet(&view->vita_medium_packets.data[0], 0U, 1U);
        check_medium_packet(&view->vita_medium_packets.data[1], 121U, 122U);
        check_medium_packet(&view->vita_medium_packets.data[2], 248U, 249U);
        close_event(&event);
        drop_pin(&p);
    }
}

static void check_large_packet(const ams_mel_rf_vita_large_packet_v1 *packet,
                              unsigned payload_words, unsigned seed)
{
    const uint8_t *bytes = packet->serialized_bytes.data;
    const unsigned words = payload_words + 8U;
    uint32_t expected[2048];
    unsigned i, j;
    expected[0] = UINT32_C(0x1ce00000) | ((seed & 15U) << 16) | words;
    expected[1] = UINT32_C(0x12345600) + seed;
    expected[2] = UINT32_C(0x00aaaaaa);
    expected[3] = UINT32_C(0x05000d05);
    expected[4] = UINT32_C(0x23456700) + seed;
    expected[5] = UINT32_C(0x34567800) + seed;
    expected[6] = UINT32_C(0x45678900) + seed;
    for (i = 0; i < payload_words; ++i)
        expected[7U + i] = UINT32_C(0x01020304) + i * UINT32_C(0x070b0d11) + seed;
    expected[words - 1U] = UINT32_C(0x00c00000) | ((seed & 3U) << 10);
    CHECK(bytes != NULL && packet->serialized_bytes.size == words * 4U);
    for (i = 0; i < words; ++i)
        for (j = 0; j < 4U; ++j)
            CHECK(bytes[i * 4U + j] == ((expected[i] >> (24U - j * 8U)) & 255U));
}

static void test_large(void)
{
    unsigned iteration;
    for (iteration = 0; iteration < 50U; ++iteration) {
        ams_mel_rf_data *data = open_rx("rx:ok:reference");
        pin p = take_pin();
        ams_mel_rf_product_rx_config_v1 config = config_of(1U, 3U);
        ams_mel_rf_product_rx_request *request = NULL;
        ams_mel_rf_product_rx *endpoint = NULL;
        ams_mel_rf_product_rx_info_v1 info;
        ams_mel_rf_product_rx_event *event, *empty;
        const ams_mel_rf_product_rx_event_v6 *view = NULL;
        unsigned format;
        const unsigned creates = p.creates();
        config.data_format = 6U;
        CHECK(ams_mel_rf_data_submit_product_rx(data, &config, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_rf_data_submit_product_rx_v2(data, &config, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_rf_data_submit_product_rx_v3(data, &config, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_rf_data_submit_product_rx_v4(data, &config, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_rf_data_submit_product_rx_v5(data, &config, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        for (format = 7U; format <= 14U; ++format) {
            config.data_format = format == 14U ? UINT32_MAX : format;
            CHECK(ams_mel_rf_data_submit_product_rx_v6(data, &config, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        }
        CHECK(p.creates() == creates && request == NULL);
        for (format = 0U; format < 7U; ++format) {
            config.data_format = format;
            CHECK(ams_mel_rf_data_submit_product_rx_v6(data, &config, &request, NULL, 0, NULL) == AMS_MEL_OK);
            wait_ok(request);
            CHECK(ams_mel_rf_product_rx_request_claim(request, &endpoint, &info, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(info.assigned_data_format == format);
            CHECK(ams_mel_rf_product_rx_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
            if (format < 6U) {
                const ams_mel_rf_product_rx_event_v5 *v3 = NULL;
                CHECK(p.emit(info.endpoint_id, EMIT_COUNT, 1U) == 1);
                event = receive_ok(endpoint);
                CHECK(ams_mel_rf_product_rx_event_view_v6(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
                CHECK(ams_mel_rf_product_rx_event_view_v5(event, &v3, NULL, 0, NULL) == AMS_MEL_OK);
                CHECK(memcmp(&view->event, v3, sizeof *v3) == 0);
                CHECK(view->vita_large_packets.data == NULL && view->vita_large_packets.size == 0U);
                close_event(&event);
                close_endpoint(&endpoint);
            }
        }
        CHECK(p.emit(info.endpoint_id, EMIT_COUNT, 1U) == 1);
        event = receive_ok(endpoint);
        CHECK(ams_mel_rf_product_rx_event_view_v6(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(view->vita_large_packets.size == 1U);
        check_large_packet(&view->vita_large_packets.data[0], 0U, 1U);
        close_event(&event);
        CHECK(p.emit(info.endpoint_id, EMIT_RICH, 0U) == 1);
        event = receive_ok(endpoint);
        CHECK(ams_mel_rf_product_rx_event_view_v6(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(view->event.event.event.data_format == 6U && view->vita_large_packets.size == 3U);
        CHECK(view->event.event.event.direct_samples_i8.data == NULL && view->event.event.event.direct_samples_i8.size == 0U);
        CHECK(view->event.event.event.direct_samples_i16.data == NULL && view->event.event.event.direct_samples_i16.size == 0U);
        CHECK(view->event.event.event.complex_samples_i8.data == NULL && view->event.event.event.complex_samples_i8.size == 0U);
        CHECK(view->event.event.event.complex_samples_i16.data == NULL && view->event.event.event.complex_samples_i16.size == 0U);
        CHECK(view->event.event.vita_small_packets.data == NULL && view->event.event.vita_small_packets.size == 0U);
        CHECK(view->event.event.event.metadata.job_interval_id == 23U);
        CHECK(view->event.event.event.metadata.rx_stream_ids.size == 3U);
        CHECK(view->event.event.event.metadata.rx_stream_ids.data[0] == 23U);
        {
            const ams_mel_rf_product_rx_event_v1 *v1 = (const void *)(uintptr_t)1;
            const ams_mel_rf_product_rx_event_v2 *v2 = (const void *)(uintptr_t)1;
            const ams_mel_rf_product_rx_event_v3 *v3 = (const void *)(uintptr_t)1;
            const ams_mel_rf_product_rx_event_v4 *v4 = (const void *)(uintptr_t)1;
            CHECK(ams_mel_rf_product_rx_event_view_v4(event, &v4, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT && v4 == (const void *)(uintptr_t)1);
            CHECK(ams_mel_rf_product_rx_event_view(event, &v1, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT && v1 == (const void *)(uintptr_t)1);
            CHECK(ams_mel_rf_product_rx_event_view_v2(event, &v2, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT && v2 == (const void *)(uintptr_t)1);
            CHECK(ams_mel_rf_product_rx_event_view_v3(event, &v3, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT && v3 == (const void *)(uintptr_t)1);
        }
        {
            const ams_mel_rf_product_rx_event_v5 *old = (const void *)(uintptr_t)1;
            CHECK(ams_mel_rf_product_rx_event_view_v5(event, &old, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
            CHECK(old == (const void *)(uintptr_t)1);
            CHECK(view->event.vita_medium_packets.data == NULL && view->event.vita_medium_packets.size == 0U);
            CHECK(view->event.event.vita_small_packets.data == NULL && view->event.event.vita_small_packets.size == 0U);
        }
        {
            const int bad[] = {33, 34,1, 2, 3, 6, 7, 8, 9, 13, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
            size_t i;
            for (i = 0; i < sizeof bad / sizeof bad[0]; ++i)
                CHECK(p.emit(info.endpoint_id, bad[i], 0U) == 1);
            CHECK(p.emit(info.endpoint_id, EMIT_COUNT, 4U) == 1);
            CHECK(p.emit(info.endpoint_id, EMIT_COUNT, SIZE_MAX) == 1);
            CHECK(counters_of(endpoint).malformed_or_unsupported == 25U);
        }
        empty = NULL;
        {
            ams_mel_rf_product_rx_request *large_request = NULL;
            ams_mel_rf_product_rx *large_endpoint = NULL;
            ams_mel_rf_product_rx_info_v1 large_info;
            config.data_format = 6U;
            config.max_samples_per_event = SIZE_MAX;
            CHECK(ams_mel_rf_data_submit_product_rx_v6(data, &config, &large_request, NULL, 0, NULL) == AMS_MEL_OK);
            wait_ok(large_request);
            CHECK(ams_mel_rf_product_rx_request_claim(large_request, &large_endpoint, &large_info, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_rf_product_rx_request_close(&large_request, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(p.emit(large_info.endpoint_id, EMIT_COUNT, SIZE_MAX / 8192U + 1U) == 1);
            CHECK(counters_of(large_endpoint).malformed_or_unsupported == 1U);
            close_endpoint(&large_endpoint);
        }
        ams_mel_test_rf_rx_failpoint(4U);
        CHECK(p.emit(info.endpoint_id, EMIT_RICH, 0U) == 1);
        CHECK(counters_of(endpoint).allocation_failures == 1U);
        CHECK(ams_mel_rf_product_rx_receive(endpoint, 0U, &empty, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
        CHECK(p.emit(info.endpoint_id, EMIT_ZERO_NULL, 0U) == 1);
        CHECK(p.emit(info.endpoint_id, EMIT_RICH, 0U) == 1);
        CHECK(counters_of(endpoint).products_dropped_queue_full == 1U);
        empty = receive_ok(endpoint);
        {
            const ams_mel_rf_product_rx_event_v6 *v = NULL;
            CHECK(ams_mel_rf_product_rx_event_view_v6(empty, &v, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(v->event.event.event.data_format == 6U && v->vita_large_packets.size == 0U && v->vita_large_packets.data == NULL);
        }
        close_event(&empty);
        CHECK(p.emit(info.endpoint_id, EMIT_RICH, 0U) == 1);
        empty = receive_ok(endpoint);
        {
            const ams_mel_rf_product_rx_event_v6 *recovered = NULL;
            CHECK(ams_mel_rf_product_rx_event_view_v6(empty, &recovered, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(recovered->vita_large_packets.size == 3U);
            check_large_packet(&recovered->vita_large_packets.data[0], 0U, 1U);
            check_large_packet(&recovered->vita_large_packets.data[1], 249U, 2U);
            check_large_packet(&recovered->vita_large_packets.data[2], 2040U, 3U);
        }
        close_event(&empty);
        close_data(&data);
        close_endpoint(&endpoint);
        check_large_packet(&view->vita_large_packets.data[0], 0U, 1U);
        check_large_packet(&view->vita_large_packets.data[1], 249U, 2U);
        check_large_packet(&view->vita_large_packets.data[2], 2040U, 3U);
        close_event(&event);
        drop_pin(&p);
    }
}

static void check_extra_large_packet(const ams_mel_rf_vita_extra_large_packet_v1 *packet,
                              unsigned payload_words, unsigned seed)
{
    const uint8_t *bytes = packet->serialized_bytes.data;
    const unsigned words = payload_words + 8U;
    uint32_t *expected = malloc((size_t)words * sizeof *expected);
    CHECK(expected != NULL);
    unsigned i, j;
    expected[0] = UINT32_C(0x1ce00000) | ((seed & 15U) << 16) | words;
    expected[1] = UINT32_C(0x12345600) + seed;
    expected[2] = UINT32_C(0x00aaaaaa);
    expected[3] = UINT32_C(0x05000d05);
    expected[4] = UINT32_C(0x23456700) + seed;
    expected[5] = UINT32_C(0x34567800) + seed;
    expected[6] = UINT32_C(0x45678900) + seed;
    for (i = 0; i < payload_words; ++i)
        expected[7U + i] = UINT32_C(0x01020304) + i * UINT32_C(0x070b0d11) + seed;
    expected[words - 1U] = UINT32_C(0x00c00000) | ((seed & 3U) << 10);
    CHECK(bytes != NULL && packet->serialized_bytes.size == words * 4U);
    for (i = 0; i < words; ++i)
        for (j = 0; j < 4U; ++j)
            CHECK(bytes[i * 4U + j] == ((expected[i] >> (24U - j * 8U)) & 255U));
    free(expected);
}

static void test_extra_large(void)
{
    unsigned iteration;
    for (iteration = 0; iteration < 50U; ++iteration) {
        ams_mel_rf_data *data = open_rx("rx:ok:reference");
        pin p = take_pin();
        ams_mel_rf_product_rx_config_v1 config = config_of(1U, 3U);
        ams_mel_rf_product_rx_request *request = NULL;
        ams_mel_rf_product_rx *endpoint = NULL;
        ams_mel_rf_product_rx_info_v1 info;
        ams_mel_rf_product_rx_event *event, *empty;
        const ams_mel_rf_product_rx_event_v7 *view = NULL;
        unsigned format;
        const unsigned creates = p.creates();
        config.data_format = 7U;
        CHECK(ams_mel_rf_data_submit_product_rx(data, &config, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_rf_data_submit_product_rx_v2(data, &config, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_rf_data_submit_product_rx_v3(data, &config, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_rf_data_submit_product_rx_v4(data, &config, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_rf_data_submit_product_rx_v5(data, &config, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_rf_data_submit_product_rx_v6(data, &config, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        for (format = 8U; format <= 14U; ++format) {
            config.data_format = format == 14U ? UINT32_MAX : format;
            CHECK(ams_mel_rf_data_submit_product_rx_v7(data, &config, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        }
        CHECK(p.creates() == creates && request == NULL);
        for (format = 0U; format < 8U; ++format) {
            config.data_format = format;
            CHECK(ams_mel_rf_data_submit_product_rx_v7(data, &config, &request, NULL, 0, NULL) == AMS_MEL_OK);
            wait_ok(request);
            CHECK(ams_mel_rf_product_rx_request_claim(request, &endpoint, &info, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(info.assigned_data_format == format);
            CHECK(ams_mel_rf_product_rx_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
            if (format < 7U) {
                const ams_mel_rf_product_rx_event_v6 *v3 = NULL;
                CHECK(p.emit(info.endpoint_id, EMIT_COUNT, 1U) == 1);
                event = receive_ok(endpoint);
                CHECK(ams_mel_rf_product_rx_event_view_v7(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
                CHECK(ams_mel_rf_product_rx_event_view_v6(event, &v3, NULL, 0, NULL) == AMS_MEL_OK);
                CHECK(memcmp(&view->event, v3, sizeof *v3) == 0);
                CHECK(view->vita_extra_large_packets.data == NULL && view->vita_extra_large_packets.size == 0U);
                close_event(&event);
                close_endpoint(&endpoint);
            }
        }
        CHECK(p.emit(info.endpoint_id, EMIT_COUNT, 1U) == 1);
        event = receive_ok(endpoint);
        CHECK(ams_mel_rf_product_rx_event_view_v7(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(view->vita_extra_large_packets.size == 1U);
        check_extra_large_packet(&view->vita_extra_large_packets.data[0], 0U, 1U);
        close_event(&event);
        CHECK(p.emit(info.endpoint_id, EMIT_RICH, 0U) == 1);
        event = receive_ok(endpoint);
        CHECK(ams_mel_rf_product_rx_event_view_v7(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(view->event.event.event.event.data_format == 7U && view->vita_extra_large_packets.size == 3U);
        CHECK(view->event.event.event.event.direct_samples_i8.data == NULL && view->event.event.event.event.direct_samples_i8.size == 0U);
        CHECK(view->event.event.event.event.direct_samples_i16.data == NULL && view->event.event.event.event.direct_samples_i16.size == 0U);
        CHECK(view->event.event.event.event.complex_samples_i8.data == NULL && view->event.event.event.event.complex_samples_i8.size == 0U);
        CHECK(view->event.event.event.event.complex_samples_i16.data == NULL && view->event.event.event.event.complex_samples_i16.size == 0U);
        CHECK(view->event.event.event.vita_small_packets.data == NULL && view->event.event.event.vita_small_packets.size == 0U);
        CHECK(view->event.event.event.event.metadata.job_interval_id == 23U);
        CHECK(view->event.event.event.event.metadata.rx_stream_ids.size == 3U);
        CHECK(view->event.event.event.event.metadata.rx_stream_ids.data[0] == 23U);
        {
            const ams_mel_rf_product_rx_event_v1 *v1 = (const void *)(uintptr_t)1;
            const ams_mel_rf_product_rx_event_v2 *v2 = (const void *)(uintptr_t)1;
            const ams_mel_rf_product_rx_event_v3 *v3 = (const void *)(uintptr_t)1;
            const ams_mel_rf_product_rx_event_v4 *v4 = (const void *)(uintptr_t)1;
            CHECK(ams_mel_rf_product_rx_event_view_v4(event, &v4, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT && v4 == (const void *)(uintptr_t)1);
            CHECK(ams_mel_rf_product_rx_event_view(event, &v1, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT && v1 == (const void *)(uintptr_t)1);
            CHECK(ams_mel_rf_product_rx_event_view_v2(event, &v2, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT && v2 == (const void *)(uintptr_t)1);
            CHECK(ams_mel_rf_product_rx_event_view_v3(event, &v3, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT && v3 == (const void *)(uintptr_t)1);
        }
        {
            const ams_mel_rf_product_rx_event_v6 *v6 = (const void *)(uintptr_t)1;
            CHECK(ams_mel_rf_product_rx_event_view_v6(event, &v6, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT && v6 == (const void *)(uintptr_t)1);
            CHECK(view->event.vita_large_packets.data == NULL && view->event.vita_large_packets.size == 0U);
            const ams_mel_rf_product_rx_event_v5 *old = (const void *)(uintptr_t)1;
            CHECK(ams_mel_rf_product_rx_event_view_v5(event, &old, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
            CHECK(old == (const void *)(uintptr_t)1);
            CHECK(view->event.event.vita_medium_packets.data == NULL && view->event.event.vita_medium_packets.size == 0U);
            CHECK(view->event.event.event.vita_small_packets.data == NULL && view->event.event.event.vita_small_packets.size == 0U);
        }
        {
            const int bad[] = {35, 36, 33, 34,1, 2, 3, 6, 7, 8, 9, 13, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
            size_t i;
            for (i = 0; i < sizeof bad / sizeof bad[0]; ++i)
                CHECK(p.emit(info.endpoint_id, bad[i], 0U) == 1);
            CHECK(p.emit(info.endpoint_id, EMIT_COUNT, 4U) == 1);
            CHECK(p.emit(info.endpoint_id, EMIT_COUNT, SIZE_MAX) == 1);
            CHECK(counters_of(endpoint).malformed_or_unsupported == 27U);
        }
        empty = NULL;
        {
            ams_mel_rf_product_rx_request *extra_large_request = NULL;
            ams_mel_rf_product_rx *extra_large_endpoint = NULL;
            ams_mel_rf_product_rx_info_v1 extra_large_info;
            config.data_format = 7U;
            config.max_samples_per_event = SIZE_MAX;
            CHECK(ams_mel_rf_data_submit_product_rx_v7(data, &config, &extra_large_request, NULL, 0, NULL) == AMS_MEL_OK);
            wait_ok(extra_large_request);
            CHECK(ams_mel_rf_product_rx_request_claim(extra_large_request, &extra_large_endpoint, &extra_large_info, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_rf_product_rx_request_close(&extra_large_request, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(p.emit(extra_large_info.endpoint_id, EMIT_COUNT, SIZE_MAX / 32768U + 1U) == 1);
            CHECK(counters_of(extra_large_endpoint).malformed_or_unsupported == 1U);
            close_endpoint(&extra_large_endpoint);
        }
        ams_mel_test_rf_rx_failpoint(4U);
        CHECK(p.emit(info.endpoint_id, EMIT_RICH, 0U) == 1);
        CHECK(counters_of(endpoint).allocation_failures == 1U);
        CHECK(ams_mel_rf_product_rx_receive(endpoint, 0U, &empty, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
        CHECK(p.emit(info.endpoint_id, EMIT_ZERO_NULL, 0U) == 1);
        CHECK(p.emit(info.endpoint_id, EMIT_RICH, 0U) == 1);
        CHECK(counters_of(endpoint).products_dropped_queue_full == 1U);
        empty = receive_ok(endpoint);
        {
            const ams_mel_rf_product_rx_event_v7 *v = NULL;
            CHECK(ams_mel_rf_product_rx_event_view_v7(empty, &v, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(v->event.event.event.event.data_format == 7U && v->vita_extra_large_packets.size == 0U && v->vita_extra_large_packets.data == NULL);
        }
        close_event(&empty);
        CHECK(p.emit(info.endpoint_id, EMIT_RICH, 0U) == 1);
        empty = receive_ok(endpoint);
        {
            const ams_mel_rf_product_rx_event_v7 *recovered = NULL;
            CHECK(ams_mel_rf_product_rx_event_view_v7(empty, &recovered, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(recovered->vita_extra_large_packets.size == 3U);
            check_extra_large_packet(&recovered->vita_extra_large_packets.data[0], 0U, 1U);
            check_extra_large_packet(&recovered->vita_extra_large_packets.data[1], 2041U, 2U);
            check_extra_large_packet(&recovered->vita_extra_large_packets.data[2], 8184U, 3U);
        }
        close_event(&empty);
        close_data(&data);
        close_endpoint(&endpoint);
        check_extra_large_packet(&view->vita_extra_large_packets.data[0], 0U, 1U);
        check_extra_large_packet(&view->vita_extra_large_packets.data[1], 2041U, 2U);
        check_extra_large_packet(&view->vita_extra_large_packets.data[2], 8184U, 3U);
        close_event(&event);
        drop_pin(&p);
    }
}

static void test_extra_large_pages(void)
{
    selected_extra_large = 1;
    ams_mel_rf_data *data = open_rx("rx:ok:owned-exhaustive");
    pin p = take_pin();
    ams_mel_rf_product_rx_info_v1 info;
    ams_mel_rf_product_rx *endpoint = open_endpoint(data, 2U, 3U, &info);
    CHECK(p.emit_owned(info.endpoint_id) == 1);
    ams_mel_rf_product_rx_event *event = receive_ok(endpoint);
    close_endpoint(&endpoint);
    CHECK(occurrences("rf_rx_extra_large_all_writable_pages_revoked_guard_preserved") == 1U);
    close_event(&event);
    close_data(&data);
    drop_pin(&p);
}

/* ---------------------------------------------------------- dispatcher */

typedef ams_mel_status_t (*rx_submit_fn)(ams_mel_rf_data *,
    const ams_mel_rf_product_rx_config_v1 *, ams_mel_rf_product_rx_request **,
    char *, size_t, size_t *);
static const rx_submit_fn association_submits[] = {
    ams_mel_rf_data_submit_product_rx, ams_mel_rf_data_submit_product_rx_v2,
    ams_mel_rf_data_submit_product_rx_v3, ams_mel_rf_data_submit_product_rx_v4,
    ams_mel_rf_data_submit_product_rx_v5, ams_mel_rf_data_submit_product_rx_v6,
    ams_mel_rf_data_submit_product_rx_v7, ams_mel_rf_data_submit_product_rx_v8};

static void association_old_views(ams_mel_rf_product_rx_event *event, unsigned format,
                                  int nonempty)
{
#define OLD_VIEW(V, FN, ALLOWED) do { \
    const ams_mel_rf_product_rx_event_v##V *sentinel = \
        (const ams_mel_rf_product_rx_event_v##V *)(uintptr_t)1U; \
    const ams_mel_status_t status = FN(event, &sentinel, NULL, 0, NULL); \
    if (nonempty || !(ALLOWED)) { \
        CHECK(status == AMS_MEL_INVALID_ARGUMENT); \
        CHECK(sentinel == (const ams_mel_rf_product_rx_event_v##V *)(uintptr_t)1U); \
    } else CHECK(status == AMS_MEL_OK && sentinel != NULL); \
} while (0)
    OLD_VIEW(1, ams_mel_rf_product_rx_event_view, format == 3U);
    OLD_VIEW(2, ams_mel_rf_product_rx_event_view_v2, format == 2U || format == 3U);
    OLD_VIEW(3, ams_mel_rf_product_rx_event_view_v3, format <= 3U);
    OLD_VIEW(4, ams_mel_rf_product_rx_event_view_v4, format <= 4U);
    OLD_VIEW(5, ams_mel_rf_product_rx_event_view_v5, format <= 5U);
    OLD_VIEW(6, ams_mel_rf_product_rx_event_view_v6, format <= 6U);
    OLD_VIEW(7, ams_mel_rf_product_rx_event_view_v7, format <= 7U);
#undef OLD_VIEW
}

static void *association_emit_thread(void *raw)
{
    emit_args *args = (emit_args *)raw;
    CHECK(args->p->emit(args->id, 43, 3U) == 1);
    return NULL;
}

static void test_association_close(void)
{
    unsigned format;
    for (format = 4U; format <= 7U; ++format) {
        ams_mel_rf_data *data = open_rx("rx:ok:reference");
        pin p = take_pin();
        ams_mel_rf_product_rx_config_v1 config = config_of(1U, 3U);
        ams_mel_rf_product_rx_request *request = NULL;
        ams_mel_rf_product_rx *endpoint = NULL;
        ams_mel_rf_product_rx_info_v1 info;
        ams_mel_test_rf_rx_observer *observer = NULL;
        int entered[2], release[2], done[2];
        pthread_t emitter, closer;
        emit_args args;
        close_args closing;
        char byte;
        config.data_format = format;
        CHECK(ams_mel_rf_data_submit_product_rx_v8(data, &config, &request, NULL, 0, NULL) == AMS_MEL_OK);
        wait_ok(request);
        CHECK(ams_mel_rf_product_rx_request_claim(request, &endpoint, &info, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_rf_product_rx_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_test_rf_rx_observer_from(endpoint, &observer) == 1);
        CHECK(pipe(entered) == 0 && pipe(release) == 0 && pipe(done) == 0);
        ams_mel_test_rf_rx_hold_next(entered[1], release[0]);
        args.p = &p; args.id = info.endpoint_id;
        CHECK(pthread_create(&emitter, NULL, association_emit_thread, &args) == 0);
        CHECK(read(entered[0], &byte, 1U) == 1);
        close_data(&data);
        closing.endpoint = endpoint; closing.done_fd = done[1];
        closing.status = AMS_MEL_INTERNAL_ERROR; endpoint = NULL;
        CHECK(pthread_create(&closer, NULL, close_thread, &closing) == 0);
        CHECK(eventually(one_drain_waiter, observer));
        CHECK(write(release[1], "r", 1U) == 1);
        CHECK(pthread_join(emitter, NULL) == 0 && pthread_join(closer, NULL) == 0);
        CHECK(closing.status == AMS_MEL_OK && closing.endpoint == NULL);
        CHECK(read(done[0], &byte, 1U) == 1);
        {
            size_t in_flight, queue; uint64_t received, after; int closed;
            CHECK(ams_mel_test_rf_rx_observe(observer, &in_flight, &received, &after, &queue, &closed) == 1);
            CHECK(in_flight == 0U && queue == 0U && closed == 1 && received == 1U && after == 1U);
        }
        CHECK(close(entered[0]) == 0 && close(entered[1]) == 0);
        CHECK(close(release[0]) == 0 && close(release[1]) == 0);
        CHECK(close(done[0]) == 0 && close(done[1]) == 0);
        ams_mel_test_rf_rx_observer_close(&observer);
        drop_pin(&p);
        /* Profile initialized before a synchronous association-bearing callback. */
        data = open_rx("rx:ok:sync-associations");
        CHECK(ams_mel_rf_data_submit_product_rx_v8(data, &config, &request, NULL, 0, NULL) == AMS_MEL_OK);
        wait_ok(request);
        CHECK(ams_mel_rf_product_rx_request_claim(request, &endpoint, &info, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_rf_product_rx_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
        {
            ams_mel_rf_product_rx_event *event = receive_ok(endpoint);
            const ams_mel_rf_product_rx_event_v8 *view = NULL;
            CHECK(ams_mel_rf_product_rx_event_view_v8(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
            close_data(&data); close_endpoint(&endpoint);
            CHECK(view->receive_event_associations.present == 1U && view->receive_event_associations.event_ids.size == 2U);
            CHECK(view->receive_event_associations.event_ids.data[0] == 17U && view->receive_event_associations.event_ids.data[1] == 17U);
            close_event(&event);
        }
    }
}

static void *stabilization_emit_thread(void *raw)
{
    emit_args *args = (emit_args *)raw;
    CHECK(args->p->emit(args->id, 56, 3U) == 1);
    return NULL;
}

static void test_stabilization_close(void)
{
    unsigned format;
    for (format = 4U; format <= 7U; ++format) {
        ams_mel_rf_data *data = open_rx("rx:ok:reference");
        pin p = take_pin();
        ams_mel_rf_product_rx_config_v1 config = config_of(1U, 3U);
        ams_mel_rf_product_rx_request *request = NULL;
        ams_mel_rf_product_rx *endpoint = NULL;
        ams_mel_rf_product_rx_info_v1 info;
        ams_mel_test_rf_rx_observer *observer = NULL;
        int entered[2], release[2], done[2];
        pthread_t emitter, closer;
        emit_args args;
        close_args closing;
        char byte;
        config.data_format = format;
        CHECK(ams_mel_rf_data_submit_product_rx_v9(data, &config, &request, NULL, 0, NULL) == AMS_MEL_OK);
        wait_ok(request);
        CHECK(ams_mel_rf_product_rx_request_claim(request, &endpoint, &info, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_rf_product_rx_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_test_rf_rx_observer_from(endpoint, &observer) == 1);
        CHECK(pipe(entered) == 0 && pipe(release) == 0 && pipe(done) == 0);
        ams_mel_test_rf_rx_hold_next(entered[1], release[0]);
        args.p = &p; args.id = info.endpoint_id;
        CHECK(pthread_create(&emitter, NULL, stabilization_emit_thread, &args) == 0);
        CHECK(read(entered[0], &byte, 1U) == 1);
        close_data(&data);
        closing.endpoint = endpoint; closing.done_fd = done[1];
        closing.status = AMS_MEL_INTERNAL_ERROR; endpoint = NULL;
        CHECK(pthread_create(&closer, NULL, close_thread, &closing) == 0);
        CHECK(eventually(one_drain_waiter, observer));
        CHECK(write(release[1], "r", 1U) == 1);
        CHECK(pthread_join(emitter, NULL) == 0 && pthread_join(closer, NULL) == 0);
        CHECK(closing.status == AMS_MEL_OK && closing.endpoint == NULL);
        CHECK(read(done[0], &byte, 1U) == 1);
        {
            size_t in_flight, queue; uint64_t received, after; int closed;
            CHECK(ams_mel_test_rf_rx_observe(observer, &in_flight, &received, &after, &queue, &closed) == 1);
            CHECK(in_flight == 0U && queue == 0U && closed == 1 && received == 1U && after == 1U);
        }
        CHECK(close(entered[0]) == 0 && close(entered[1]) == 0);
        CHECK(close(release[0]) == 0 && close(release[1]) == 0);
        CHECK(close(done[0]) == 0 && close(done[1]) == 0);
        ams_mel_test_rf_rx_observer_close(&observer);
        drop_pin(&p);
        /* Profile initialized before a synchronous association-bearing callback. */
        data = open_rx("rx:ok:sync-stabilization");
        CHECK(ams_mel_rf_data_submit_product_rx_v9(data, &config, &request, NULL, 0, NULL) == AMS_MEL_OK);
        wait_ok(request);
        CHECK(ams_mel_rf_product_rx_request_claim(request, &endpoint, &info, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_rf_product_rx_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
        {
            ams_mel_rf_product_rx_event *event = receive_ok(endpoint);
            const ams_mel_rf_product_rx_event_v9 *view = NULL;
            CHECK(ams_mel_rf_product_rx_event_view_v9(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
            close_data(&data); close_endpoint(&endpoint);
            CHECK(view->stabilization_points.size == 1U && view->stabilization_points.data[0].kind == 3U);
            CHECK(view->stabilization_points.data[0].face_relative.azimuth_rad == -12.5);
            close_event(&event);
        }
    }
}

static void test_associations(void)
{
    unsigned iteration, version, format, scenario;
    for (iteration = 0; iteration < 50U; ++iteration) {
        ams_mel_rf_data *data = open_rx("rx:ok:reference");
        pin p = take_pin();
        for (version = 1U; version <= 8U; ++version) {
            for (format = 0U; format <= 14U; ++format) {
                const unsigned minimum = version == 1U ? 3U : version == 2U ? 2U : 0U;
                const unsigned maximum = version < 4U ? 3U : version < 8U ? version : 7U;
                const unsigned raw_format = format == 14U ? UINT32_MAX : format;
                const int accepted = raw_format >= minimum && raw_format <= maximum;
                ams_mel_rf_product_rx_config_v1 config = config_of(1U, 8U);
                ams_mel_rf_product_rx_request *request = NULL;
                ams_mel_rf_product_rx *endpoint = NULL;
                ams_mel_rf_product_rx_info_v1 info;
                unsigned creates = p.creates();
                config.data_format = raw_format;
                CHECK(association_submits[version - 1U](data, &config, &request, NULL, 0, NULL)
                      == (accepted ? AMS_MEL_OK : AMS_MEL_INVALID_ARGUMENT));
                if (!accepted) { CHECK(request == NULL && p.creates() == creates); continue; }
                wait_ok(request);
                CHECK(ams_mel_rf_product_rx_request_claim(request, &endpoint, &info, NULL, 0, NULL) == AMS_MEL_OK);
                CHECK(info.assigned_data_format == format);
                CHECK(ams_mel_rf_product_rx_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
                for (scenario = 40U; scenario <= 47U; ++scenario) {
                    const size_t count = scenario == 42U ? 1U : scenario == 47U ? 0U : 3U;
                    const int present = scenario != 40U;
                    const int nonempty = scenario >= 42U;
                    const int valid = !nonempty ? (version < 8U || format < 4U || !present) :
                        version == 8U && format >= 4U && scenario <= 44U;
                    ams_mel_rf_product_rx_counters_v1 before = counters_of(endpoint);
                    ams_mel_rf_product_rx_event *event = NULL;
                    const ams_mel_rf_product_rx_event_v8 *view = NULL;
                    CHECK(p.emit(info.endpoint_id, (int)scenario, count) == 1);
                    if (!valid) {
                        CHECK(ams_mel_rf_product_rx_receive(endpoint, 0U, &event, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
                        CHECK(event == NULL);
                        CHECK(counters_of(endpoint).malformed_or_unsupported == before.malformed_or_unsupported + 1U);
                        continue;
                    }
                    event = receive_ok(endpoint);
                    CHECK(ams_mel_rf_product_rx_event_view_v8(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
                    CHECK(view->event.event.event.event.event.data_format == format);
                    CHECK(view->receive_event_associations.present == (unsigned)present);
                    CHECK(view->receive_event_associations.event_ids.size == (nonempty ? count : 0U));
                    if (!nonempty) {
                        const ams_mel_rf_product_rx_event_v7 *old = NULL;
                        CHECK(view->receive_event_associations.event_ids.data == NULL);
                        CHECK(ams_mel_rf_product_rx_event_view_v7(event, &old, NULL, 0, NULL) == AMS_MEL_OK);
                        CHECK(memcmp(&view->event, old, sizeof *old) == 0);
                    } else {
                        const uint32_t *ids = view->receive_event_associations.event_ids.data;
                        CHECK(ids != NULL);
                        CHECK(ids[0] == (scenario == 42U ? UINT32_MAX : scenario == 43U ? 17U : 0U));
                        if (count == 3U) {
                            CHECK(ids[1] == (scenario == 43U ? 17U : UINT32_MAX));
                            CHECK(ids[2] == (scenario == 43U ? 23U : UINT32_C(0x81234567)));
                        }
                    }
                    association_old_views(event, format, nonempty);
                    close_event(&event);
                }
                /* Present-empty zero-packet callback, including legacy domains. */
                CHECK(p.emit(info.endpoint_id, 41, 0U) == 1);
                {
                    ams_mel_rf_product_rx_event *event = receive_ok(endpoint);
                    const ams_mel_rf_product_rx_event_v8 *view = NULL;
                    CHECK(ams_mel_rf_product_rx_event_view_v8(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
                    CHECK(view->receive_event_associations.present == 1U);
                    CHECK(view->receive_event_associations.event_ids.data == NULL && view->receive_event_associations.event_ids.size == 0U);
                    association_old_views(event, format, 0);
                    close_event(&event);
                }
                if (version == 8U && format >= 4U) {
                    ams_mel_rf_product_rx_event *event = NULL;
                    const ams_mel_rf_product_rx_event_v8 *view = NULL;
                    ams_mel_rf_product_rx_counters_v1 before = counters_of(endpoint);
                    ams_mel_test_rf_rx_failpoint(5U);
                    CHECK(p.emit(info.endpoint_id, 43, 3U) == 1);
                    CHECK(ams_mel_rf_product_rx_receive(endpoint, 0U, &event, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
                    CHECK(event == NULL && counters_of(endpoint).allocation_failures == before.allocation_failures + 1U);
                    CHECK(p.emit(info.endpoint_id, 43, 3U) == 1);
                    CHECK(p.emit(info.endpoint_id, 44, 3U) == 1); /* DROP-INCOMING */
                    CHECK(counters_of(endpoint).products_dropped_queue_full == before.products_dropped_queue_full + 1U);
                    event = receive_ok(endpoint);
                    CHECK(ams_mel_rf_product_rx_event_view_v8(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
                    close_endpoint(&endpoint);
                    CHECK(view->receive_event_associations.event_ids.data[0] == 17U);
                    CHECK(view->receive_event_associations.event_ids.data[1] == 17U);
                    CHECK(view->receive_event_associations.event_ids.data[2] == 23U);
                    close_event(&event);
                } else close_endpoint(&endpoint);
            }
        }
        close_data(&data);
        check_no_forbidden(&p);
        drop_pin(&p);
    }
}

static void check_stabilization(const ams_mel_rf_pointing_span_v1 *span, unsigned scenario)
{
    const size_t expected = scenario == 50U ? 0U : scenario <= 55U ? 1U : scenario == 57U ? 5U : 7U;
    size_t i;
    CHECK(span->size == expected);
    CHECK((span->data == NULL) == (expected == 0U));
    for (i = 0; i < expected; ++i) {
        const unsigned kind = scenario <= 55U ? scenario - 51U : i < 5U ? (unsigned)i : i == 5U ? 3U : 0U;
        const ams_mel_rf_pointing_v1 *p = &span->data[i];
        CHECK(p->kind == kind);
        switch (kind) {
        case 0:
            CHECK(p->ecef.location_m.x == 0.0 && signbit(p->ecef.location_m.x));
            CHECK(p->ecef.time_of_validity.seconds == -17 && p->ecef.time_of_validity.fractional_femtoseconds == INT64_C(999999999999999));
            if (scenario == 57U) {
                CHECK(isinf(p->ecef.location_m.y) && p->ecef.location_m.y > 0.0);
                CHECK(isnan(p->ecef.location_m.z));
                CHECK(isinf(p->ecef.velocity_mps.x) && p->ecef.velocity_mps.x < 0.0);
                CHECK(p->ecef.velocity_mps.y == 0.0 && !signbit(p->ecef.velocity_mps.y));
                CHECK(p->ecef.velocity_mps.z == 0.0 && signbit(p->ecef.velocity_mps.z));
            } else {
                CHECK(p->ecef.location_m.y == 123.5 && p->ecef.location_m.z == -456.25);
                CHECK(p->ecef.velocity_mps.x == 1.25 && p->ecef.velocity_mps.y == -2.5 && p->ecef.velocity_mps.z == 3.75);
            }
            CHECK(p->lla.time_of_validity.seconds == 0 && p->platform_relative.azimuth_rad == 0.0);
            break;
        case 1:
            CHECK(p->lla.time_of_validity.seconds == 23 && p->lla.time_of_validity.fractional_femtoseconds == 0);
            if (scenario == 57U) {
                CHECK(isnan(p->lla.latitude_rad) && isinf(p->lla.longitude_rad) && p->lla.longitude_rad > 0.0);
                CHECK(isinf(p->lla.altitude_m) && p->lla.altitude_m < 0.0);
                CHECK(p->lla.velocity_north_mps == 0.0 && signbit(p->lla.velocity_north_mps));
                CHECK(isinf(p->lla.velocity_east_mps) && isnan(p->lla.velocity_down_mps));
            } else {
                CHECK(p->lla.latitude_rad == 12.5 && p->lla.longitude_rad == -18.25 && p->lla.altitude_m == -37.5);
                CHECK(p->lla.velocity_north_mps == -4.5 && p->lla.velocity_east_mps == 5.25 && p->lla.velocity_down_mps == -6.75);
            }
            CHECK(p->ecef.time_of_validity.seconds == 0 && p->face_relative.elevation_rad == 0.0);
            break;
        case 2:
            if (scenario == 57U) CHECK(signbit(p->platform_relative.azimuth_rad) && isinf(p->platform_relative.elevation_rad));
            else CHECK(p->platform_relative.azimuth_rad == -12.5 && p->platform_relative.elevation_rad == 8.25);
            CHECK(p->ecef.time_of_validity.seconds == 0 && p->lla.time_of_validity.seconds == 0);
            break;
        case 3:
            if (scenario == 57U) CHECK(isnan(p->face_relative.azimuth_rad) && isinf(p->face_relative.elevation_rad) && p->face_relative.elevation_rad < 0.0);
            else CHECK(p->face_relative.azimuth_rad == 18.5 && p->face_relative.elevation_rad == -9.75);
            break;
        case 4:
            if (scenario == 57U) CHECK(isnan(p->baseline_relative_conic_rad));
            else CHECK(p->baseline_relative_conic_rad == -15.5);
            break;
        default: CHECK(0);
        }
    }
}

static void stabilization_old_views(ams_mel_rf_product_rx_event *event)
{
#define OLD(V, F) do { \
    const ams_mel_rf_product_rx_event_v##V *out = (const void *)(uintptr_t)1; \
    CHECK(F(event, &out, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT); \
    CHECK(out == (const void *)(uintptr_t)1); \
} while (0)
    OLD(1, ams_mel_rf_product_rx_event_view);
    OLD(2, ams_mel_rf_product_rx_event_view_v2);
    OLD(3, ams_mel_rf_product_rx_event_view_v3);
    OLD(4, ams_mel_rf_product_rx_event_view_v4);
    OLD(5, ams_mel_rf_product_rx_event_view_v5);
    OLD(6, ams_mel_rf_product_rx_event_view_v6);
    OLD(7, ams_mel_rf_product_rx_event_view_v7);
    OLD(8, ams_mel_rf_product_rx_event_view_v8);
#undef OLD
}

static void test_stabilization(void)
{
    unsigned iteration, version, format, scenario;
    for (iteration = 0; iteration < 50U; ++iteration) {
        ams_mel_rf_data *data = open_rx("rx:ok:reference");
        pin p = take_pin();
        for (version = 1; version <= 9; ++version) {
            for (format = 0; format <= 14; ++format) {
                ams_mel_rf_product_rx_config_v1 config = config_of(1U, 8U);
                ams_mel_rf_product_rx_request *request = NULL;
                ams_mel_rf_product_rx *endpoint = NULL;
                ams_mel_rf_product_rx_info_v1 info;
                ams_mel_status_t result;
                const unsigned creates_before = p.creates();
                config.data_format = format == 14U ? UINT32_MAX : format;
                result = version == 9U ? ams_mel_rf_data_submit_product_rx_v9(data, &config, &request, NULL, 0, NULL) :
                    association_submits[version - 1U](data, &config, &request, NULL, 0, NULL);
                if (format < (version == 1U ? 3U : version == 2U ? 2U : 0U) ||
                    format > (version < 4U ? 3U : version < 7U ? version : 7U)) {
                    CHECK(result == AMS_MEL_INVALID_ARGUMENT && request == NULL);
                    CHECK(p.creates() == creates_before);
                    continue;
                }
                CHECK(result == AMS_MEL_OK);
                wait_ok(request);
                CHECK(ams_mel_rf_product_rx_request_claim(request, &endpoint, &info, NULL, 0, NULL) == AMS_MEL_OK);
                CHECK(ams_mel_rf_product_rx_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
                /* No event publication can conceal discarded nonempty points. */
                CHECK(p.emit(info.endpoint_id, EMIT_STAB_POINTS, 0U) == 1);
                if (version < 9U) {
                    ams_mel_rf_product_rx_event *none = NULL;
                    CHECK(ams_mel_rf_product_rx_receive(endpoint, 0U, &none, NULL, 0, NULL) == AMS_MEL_TIMEOUT && none == NULL);
                    CHECK(counters_of(endpoint).malformed_or_unsupported == 1U);
                    CHECK(p.emit(info.endpoint_id, 50, 1U) == 1);
                    none = receive_ok(endpoint);
                    {
                        const ams_mel_rf_product_rx_event_v9 *view = NULL;
                        CHECK(ams_mel_rf_product_rx_event_view_v9(none, &view, NULL, 0, NULL) == AMS_MEL_OK);
                        check_stabilization(&view->stabilization_points, 50U);
                    }
                    close_event(&none);
                } else {
                    ams_mel_rf_product_rx_event *event = receive_ok(endpoint);
                    const ams_mel_rf_product_rx_event_v9 *view = NULL;
                    CHECK(ams_mel_rf_product_rx_event_view_v9(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
                    CHECK(view->stabilization_points.size == 1U && view->stabilization_points.data[0].kind == 3U);
                    stabilization_old_views(event);
                    close_event(&event);
                    for (scenario = 50; scenario <= 59; ++scenario) {
                        const size_t count = scenario == 59U ? 0U : 3U;
                        CHECK(p.emit(info.endpoint_id, (int)scenario, count) == 1);
                        if (scenario == 58U) {
                            event = NULL;
                            CHECK(ams_mel_rf_product_rx_receive(endpoint, 0U, &event, NULL, 0, NULL) == AMS_MEL_TIMEOUT && event == NULL);
                            continue;
                        }
                        event = receive_ok(endpoint);
                        CHECK(ams_mel_rf_product_rx_event_view_v9(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
                        check_stabilization(&view->stabilization_points, scenario);
                        if (scenario == 50U) {
                            const ams_mel_rf_product_rx_event_v8 *old = NULL;
                            CHECK(ams_mel_rf_product_rx_event_view_v8(event, &old, NULL, 0, NULL) == AMS_MEL_OK);
                            association_old_views(event, format, 0);
                        }
                        if (scenario != 50U) stabilization_old_views(event);
                        if (scenario == 59U && format >= 4U) CHECK(view->event.receive_event_associations.present == 1U && view->event.receive_event_associations.event_ids.size == 0U);
                        close_event(&event);
                    }
                    CHECK(p.emit(info.endpoint_id, 59, 3U) == 1);
                    event = receive_ok(endpoint);
                    CHECK(ams_mel_rf_product_rx_event_view_v9(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
                    check_stabilization(&view->stabilization_points, 59U);
                    if (format >= 4U) CHECK(view->event.receive_event_associations.event_ids.size == 3U && view->event.receive_event_associations.event_ids.data[2] == 17U);
                    close_event(&event);
                    ams_mel_test_rf_rx_failpoint(6U);
                    CHECK(p.emit(info.endpoint_id, 56, 1U) == 1);
                    event = NULL;
                    CHECK(ams_mel_rf_product_rx_receive(endpoint, 0U, &event, NULL, 0, NULL) == AMS_MEL_TIMEOUT && event == NULL);
                    CHECK(counters_of(endpoint).allocation_failures == 1U);
                    CHECK(p.emit(info.endpoint_id, EMIT_USER_DATA, 0U) == 1);
                    CHECK(p.emit(info.endpoint_id, EMIT_RECEIVE_EVENTS, 0U) == 1);
                    CHECK(counters_of(endpoint).malformed_or_unsupported == 3U);
                    for (scenario = 60U; scenario <= 63U; ++scenario) {
                        CHECK(p.emit(info.endpoint_id, (int)scenario, 3U) == 1);
                        event = NULL;
                        CHECK(ams_mel_rf_product_rx_receive(endpoint, 0U, &event, NULL, 0, NULL) == AMS_MEL_TIMEOUT && event == NULL);
                    }
                    CHECK(counters_of(endpoint).malformed_or_unsupported == 7U);
                    CHECK(p.emit(info.endpoint_id, 56, 1U) == 1);
                    CHECK(p.emit(info.endpoint_id, 57, 1U) == 1);
                    CHECK(counters_of(endpoint).products_dropped_queue_full == 1U);
                    event = receive_ok(endpoint);
                    CHECK(ams_mel_rf_product_rx_event_view_v9(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
                    close_endpoint(&endpoint);
                    check_stabilization(&view->stabilization_points, 56U);
                    close_event(&event);
                }
                close_endpoint(&endpoint);
            }
        }
        close_data(&data); check_no_forbidden(&p); drop_pin(&p);
    }
    /* Immutable capability must exist before synchronous registration callback. */
    for (format = 0; format <= 7; ++format) {
        ams_mel_rf_data *data = open_rx("rx:ok:sync-stabilization");
        ams_mel_rf_product_rx_config_v1 config = config_of(1U, 8U);
        ams_mel_rf_product_rx_request *request = NULL;
        ams_mel_rf_product_rx *endpoint = NULL;
        ams_mel_rf_product_rx_info_v1 info;
        ams_mel_rf_product_rx_event *event;
        const ams_mel_rf_product_rx_event_v9 *view = NULL;
        config.data_format = format;
        CHECK(ams_mel_rf_data_submit_product_rx_v9(data, &config, &request, NULL, 0, NULL) == AMS_MEL_OK);
        wait_ok(request);
        CHECK(ams_mel_rf_product_rx_request_claim(request, &endpoint, &info, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_rf_product_rx_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
        event = receive_ok(endpoint);
        CHECK(ams_mel_rf_product_rx_event_view_v9(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
        close_data(&data); close_endpoint(&endpoint);
        CHECK(view->stabilization_points.size == 1U && view->stabilization_points.data[0].kind == 3U);
        CHECK(view->stabilization_points.data[0].face_relative.azimuth_rad == -12.5);
        close_event(&event);
    }
}
int main(int argc, char **argv)
{
    const char *name;
    if (argc != 2) {
        fprintf(stderr, "usage: %s CASE\n", argv[0]);
        return EXIT_FAILURE;
    }
    name = argv[1];
    if (strncmp(name, "extra-large-", 12U) == 0) { selected_extra_large = 1; name += 12; }
    if (strncmp(name, "large-", 6U) == 0) { selected_large = 1; name += 6; }
    if (strncmp(name, "medium-", 7U) == 0) { selected_medium = 1; name += 7; }
    if (strncmp(name, "vita-", 5U) == 0) { selected_vita = 1; name += 5; }
    if (strncmp(name, "direct8-", 8U) == 0 || strncmp(name, "direct16-", 9U) == 0) {
        selected_direct = name[6] == '8' ? 0 : 1;
        name += selected_direct == 0 ? 8 : 9;
    }
    if (strncmp(name, "i8-", 3U) == 0) {
        selected_i8 = 1;
        name += 3;
    }
    setup_log();
    if (strcmp(name, "pages") == 0 && selected_extra_large) test_extra_large_pages();
    else if (strcmp(name, "extra-large") == 0) test_extra_large();
    else if (strcmp(name, "large") == 0) test_large();
    else if (strcmp(name, "medium") == 0) test_medium();
    else if (strcmp(name, "association-close") == 0) test_association_close();
    else if (strcmp(name, "stabilization-close") == 0) test_stabilization_close();
    else if (strcmp(name, "stabilization") == 0) test_stabilization();
    else if (strcmp(name, "associations") == 0) test_associations();
    else if (strcmp(name, "vita") == 0) test_vita();
    else if (strcmp(name, "creation") == 0 && selected_i8) test_i8_creation();
    else if (strcmp(name, "direct-creation") == 0) test_direct_creation();
    else if (strcmp(name, "direct") == 0) test_direct();
    else if (strcmp(name, "i8") == 0) test_i8();
    else if (strcmp(name, "format-mismatch") == 0)
        expect_failure("rx:format-mismatch", AMS_MEL_PROVIDER_FAILED, AMS_MEL_ERROR_NONE,
                       "assigned", 1);
    else if (strcmp(name, "receive") == 0) test_receive();
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
