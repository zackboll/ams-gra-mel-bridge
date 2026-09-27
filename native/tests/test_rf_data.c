/* Task 033B RF DataMEL foundation contract (C11).
 *
 * One process per CTest case so lifetime and permanent-retention evidence
 * cannot leak between cases. A TEST-owned provider pin (dlopen RTLD_NOLOAD)
 * resolves every mock observation symbol before it is dropped (Task 032A
 * rule): no dlsym ever runs after the pin is released. */
#define _POSIX_C_SOURCE 200809L

#include <ams_mel/abi.h>

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef AMS_MEL_TEST_MOCK_RF_PROVIDER
#error "mock RF provider path is required"
#endif
#ifndef AMS_MEL_TEST_MISSING_SYMBOL_RF_PROVIDER
#error "missing-symbol RF provider path is required"
#endif

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

typedef unsigned (*counter_fn)(void);

static char log_path[] = "/tmp/ams-mel-rf-data-XXXXXX";

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

static void read_log(char *buffer, size_t capacity)
{
    FILE *file = fopen(log_path, "rb");
    size_t count;
    CHECK(file != NULL);
    count = fread(buffer, 1, capacity - 1U, file);
    CHECK(!ferror(file) && fclose(file) == 0);
    buffer[count] = '\0';
}

static void expect_log(const char *expected)
{
    char contents[1024];
    read_log(contents, sizeof contents);
    if (strcmp(contents, expected) != 0)
        fprintf(stderr, "lifetime log:\n%s--- expected:\n%s", contents, expected);
    CHECK(strcmp(contents, expected) == 0);
}

static unsigned occurrences(const char *line)
{
    char contents[4096];
    const size_t length = strlen(line);
    unsigned count = 0;
    const char *cursor;
    read_log(contents, sizeof contents);
    for (cursor = contents; *cursor != '\0';) {
        const char *end = strchr(cursor, '\n');
        const size_t size = end ? (size_t)(end - cursor) : strlen(cursor);
        if (size == length && strncmp(cursor, line, length) == 0) ++count;
        cursor += size + (end ? 1U : 0U);
    }
    return count;
}

static int provider_loaded(void)
{
    void *probe = dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    if (probe == NULL) return 0;
    CHECK(dlclose(probe) == 0);
    return 1;
}

/* The TEST-owned pin and every observation symbol, resolved together. */
typedef struct pin {
    void *handle;
    counter_fn shutdown_calls;
    counter_fn forbidden_calls;
    counter_fn getter_calls;
} pin;

static pin take_pin(void)
{
    pin value;
    value.handle = dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(value.handle != NULL);
    *(void **)(&value.shutdown_calls) = dlsym(value.handle, "mock_rf_shutdown_calls");
    *(void **)(&value.forbidden_calls) = dlsym(value.handle, "mock_rf_forbidden_calls");
    *(void **)(&value.getter_calls) = dlsym(value.handle, "mock_rf_getter_calls");
    CHECK(value.shutdown_calls && value.forbidden_calls && value.getter_calls);
    return value;
}

static void drop_pin(pin *value)
{
    CHECK(value->handle != NULL && dlclose(value->handle) == 0);
    value->handle = NULL;
    value->shutdown_calls = value->forbidden_calls = value->getter_calls = NULL;
}

static ams_mel_rf_data *open_mock(const char *scenario)
{
    ams_mel_rf_data *data = NULL;
    char diagnostic[128] = "not cleared";
    size_t required = 0;
    CHECK(ams_mel_rf_data_open(AMS_MEL_TEST_MOCK_RF_PROVIDER, scenario, &data,
          diagnostic, sizeof diagnostic, &required) == AMS_MEL_OK);
    CHECK(data != NULL && diagnostic[0] == '\0' && required == 1U);
    return data;
}

static void close_ok(ams_mel_rf_data **data)
{
    CHECK(ams_mel_rf_data_close(data, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(*data == NULL);
}

static const ams_mel_rf_mfa_info_v1 *view_of(const ams_mel_rf_mfa_info *info)
{
    const ams_mel_rf_mfa_info_v1 *view = NULL;
    CHECK(ams_mel_rf_mfa_info_view(info, &view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(view != NULL);
    return view;
}

static void check_range(ams_mel_rf_frequency_range_span_v1 span, size_t index,
                        double min_hz, double max_hz)
{
    CHECK(span.data != NULL && index < span.size);
    CHECK(span.data[index].min_hz == min_hz);
    CHECK(span.data[index].max_hz == max_hz);
}

/* Every Task 033B field of the deterministic mock snapshot, exactly. */
static void verify_mock_snapshot(const ams_mel_rf_mfa_info_v1 *view,
                                 uint64_t reported_faces, int future_format)
{
    const ams_mel_rf_face_info_v1 *seven;
    const ams_mel_rf_face_info_v1 *forty_two;
    const uint32_t *formats;
    size_t first = 0;
    CHECK(view->reported_num_faces == reported_faces);
    CHECK(view->contains_open_additions == 1U);
    CHECK(view->scheduler_resolution_fs == INT64_C(12345));
    CHECK(view->max_user_defined_context_bytes == UINT64_C(9876));

    /* std::set<JobDataFormat> order == underlying enum order. */
    formats = view->supported_data_formats.data;
    CHECK(formats != NULL);
    CHECK(view->supported_data_formats.size == (future_format ? 5U : 4U));
    if (future_format) {
        CHECK(formats[0] == UINT32_C(0xF0000001));
        first = 1;
    }
    CHECK(formats[first + 0] == AMS_MEL_RF_JOB_DATA_FORMAT_DIRECT_INT8);
    CHECK(formats[first + 1] == AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT16);
    CHECK(formats[first + 2] == AMS_MEL_RF_JOB_DATA_FORMAT_AMS_VITA_LARGE);
    CHECK(formats[first + 3] == AMS_MEL_RF_JOB_DATA_FORMAT_LF_TYPE3);

    /* Non-contiguous IDs from getFaceIDs(), not 0..getNumFaces()-1. */
    CHECK(view->faces.size == 2U && view->faces.data != NULL);
    seven = &view->faces.data[0];
    forty_two = &view->faces.data[1];

    CHECK(seven->face_id == 7U);
    CHECK(seven->supports_receive == 1U);
    CHECK(seven->supports_transmit == 0U);
    CHECK(seven->requires_endpoint_association == 1U);
    CHECK(seven->agc_processing_time_fs == INT64_C(1001));
    CHECK(seven->min_job_request_lead_time_fs == INT64_C(1002));
    CHECK(seven->max_job_request_lead_time_fs == INT64_C(1003000000000000));
    CHECK(seven->min_job_detail_lead_time_fs == INT64_C(1004));
    CHECK(seven->tx_rx_switching_time_fs == INT64_C(1005));
    CHECK(seven->rx_tx_switching_time_fs == INT64_C(1006));
    CHECK(seven->tx_tx_switching_time_fs == INT64_C(1007));
    CHECK(seven->rx_rx_switching_time_fs == INT64_C(1008));
    CHECK(seven->rx_frequency_ranges.size == 2U);
    check_range(seven->rx_frequency_ranges, 0, 100250000.125, 200500000.5);
    check_range(seven->rx_frequency_ranges, 1, 1500000000.25, 2750000000.75);
    CHECK(seven->tx_frequency_ranges.size == 0U);
    CHECK(seven->tx_frequency_ranges.data == NULL);
    CHECK(seven->sample_frequency_ranges.size == 1U);
    check_range(seven->sample_frequency_ranges, 0, 1024000.5, 2048000.25);

    CHECK(forty_two->face_id == 42U);
    CHECK(forty_two->supports_receive == 0U);
    CHECK(forty_two->supports_transmit == 1U);
    CHECK(forty_two->requires_endpoint_association == 0U);
    CHECK(forty_two->agc_processing_time_fs == INT64_C(0));
    CHECK(forty_two->min_job_request_lead_time_fs == INT64_C(42001));
    CHECK(forty_two->max_job_request_lead_time_fs == INT64_MAX);
    CHECK(forty_two->min_job_detail_lead_time_fs == INT64_C(42003));
    CHECK(forty_two->tx_rx_switching_time_fs == INT64_C(42004));
    CHECK(forty_two->rx_tx_switching_time_fs == INT64_C(42005));
    CHECK(forty_two->tx_tx_switching_time_fs == INT64_C(42006));
    CHECK(forty_two->rx_rx_switching_time_fs == INT64_C(-42007));
    CHECK(forty_two->rx_frequency_ranges.size == 0U);
    CHECK(forty_two->rx_frequency_ranges.data == NULL);
    CHECK(forty_two->tx_frequency_ranges.size == 1U);
    check_range(forty_two->tx_frequency_ranges, 0, 433125000.5, 434875000.25);
    /* Provider order preserved; inverted ranges are not normalized. */
    CHECK(forty_two->sample_frequency_ranges.size == 2U);
    check_range(forty_two->sample_frequency_ranges, 0, 3000000.75, 1000000.125);
    check_range(forty_two->sample_frequency_ranges, 1, 0.5, 0.25);

    /* No aliasing between distinct backing vectors. */
    CHECK(seven->rx_frequency_ranges.data != seven->sample_frequency_ranges.data);
    CHECK(forty_two->tx_frequency_ranges.data !=
          forty_two->sample_frequency_ranges.data);
    CHECK(seven->sample_frequency_ranges.data !=
          forty_two->sample_frequency_ranges.data);
}

static ams_mel_rf_mfa_info *snapshot(const ams_mel_rf_data *data)
{
    ams_mel_rf_mfa_info *info = NULL;
    char diagnostic[128] = "not cleared";
    size_t required = 0;
    CHECK(ams_mel_rf_data_get_mfa_info(data, &info, diagnostic, sizeof diagnostic,
          &required) == AMS_MEL_OK);
    CHECK(info != NULL && diagnostic[0] == '\0' && required == 1U);
    return info;
}

static void close_info(ams_mel_rf_mfa_info **info)
{
    CHECK(ams_mel_rf_mfa_info_close(info, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(*info == NULL);
    CHECK(ams_mel_rf_mfa_info_close(info, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(*info == NULL);
}

static const char normal_close_log[] =
    "rf_factory_called\nrf_shutdown\nrf_data_destroyed\n"
    "rf_mfa_info_destroyed\nlibrary_unloaded\n";

/* No out-of-scope, post-shutdown, repeated-shutdown or unknown-face use. */
static void no_violations(void)
{
    CHECK(occurrences("rf_forbidden_call") == 0U);
    CHECK(occurrences("rf_call_after_shutdown") == 0U);
    CHECK(occurrences("rf_shutdown_repeated") == 0U);
    CHECK(occurrences("rf_unknown_face") == 0U);
}

static void case_open(void)
{
    ams_mel_rf_data *data = open_mock("success");
    CHECK(provider_loaded());
    close_ok(&data);
    CHECK(!provider_loaded());
    expect_log(normal_close_log);
    /* Repeated independent open/close cycles reload and unload cleanly. */
    for (int cycle = 0; cycle < 10; ++cycle) {
        reset_log();
        data = open_mock("success");
        close_ok(&data);
        expect_log(normal_close_log);
        CHECK(!provider_loaded());
    }
}

static void expect_open_failure(const char *path, const char *configuration,
                                ams_mel_status_t expected,
                                const char *expected_diagnostic,
                                const char *expected_log)
{
    ams_mel_rf_data *data = NULL;
    char diagnostic[256] = {0};
    size_t required = 0;
    reset_log();
    CHECK(ams_mel_rf_data_open(path, configuration, &data, diagnostic,
          sizeof diagnostic, &required) == expected);
    CHECK(data == NULL);
    CHECK(diagnostic[0] != '\0' && required == strlen(diagnostic) + 1U);
    if (expected_diagnostic) CHECK(strcmp(diagnostic, expected_diagnostic) == 0);
    if (expected_log) expect_log(expected_log);
    /* The same failure without any diagnostic buffer. */
    reset_log();
    CHECK(ams_mel_rf_data_open(path, configuration, &data, NULL, 0, NULL) == expected);
    CHECK(data == NULL);
    CHECK(ams_mel_rf_data_close(&data, NULL, 0, NULL) == AMS_MEL_OK && data == NULL);
    CHECK(!provider_loaded());
}

static void case_missing_symbol(void)
{
    void *probe;
    expect_open_failure(AMS_MEL_TEST_MISSING_SYMBOL_RF_PROVIDER, "success",
                        AMS_MEL_SYMBOL_NOT_FOUND, NULL, "");
    probe = dlopen(AMS_MEL_TEST_MISSING_SYMBOL_RF_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(probe == NULL);
    expect_open_failure("/definitely/missing/librfmel.so", "success",
                        AMS_MEL_LIBRARY_LOAD_FAILED, NULL, "");
}

static void case_null_factory(void)
{
    expect_open_failure(AMS_MEL_TEST_MOCK_RF_PROVIDER, "factory-null",
                        AMS_MEL_FACTORY_FAILED, "createDataMEL returned null",
                        "rf_factory_called\nlibrary_unloaded\n");
}

static void case_factory_throw(void)
{
    /* No provider object escaped: the temporary SharedLibrary unloads after
     * the exception was translated, with no retained graph. */
    expect_open_failure(AMS_MEL_TEST_MOCK_RF_PROVIDER, "factory-throw",
                        AMS_MEL_PROVIDER_EXCEPTION, "mock RF factory exception",
                        "rf_factory_called\nlibrary_unloaded\n");
    expect_open_failure(AMS_MEL_TEST_MOCK_RF_PROVIDER, "factory-throw-unknown",
                        AMS_MEL_PROVIDER_EXCEPTION,
                        "unknown provider factory exception",
                        "rf_factory_called\nlibrary_unloaded\n");
    expect_open_failure(AMS_MEL_TEST_MOCK_RF_PROVIDER, "factory-bad-alloc",
                        AMS_MEL_INTERNAL_ERROR, "allocation failed",
                        "rf_factory_called\nlibrary_unloaded\n");
}

static void check_success_version(const ams_mel_rf_data *data)
{
    char vendor[64] = "unchanged";
    char description[64] = "unchanged";
    char diagnostic[128] = {0};
    size_t required = 0;
    ams_mel_provider_version_v1 version = {UINT32_MAX, UINT32_MAX, NULL, 0, 0, NULL, 0, 0};
    const char *expected_vendor = "Mock RF \xC2\xB5Vendor";
    const char *expected_description = "Deterministic Task 033B RF DataMEL";

    /* First call: required sizes only, numeric fields untouched. */
    CHECK(ams_mel_rf_data_get_provider_version(data, &version, diagnostic,
          sizeof diagnostic, &required) == AMS_MEL_BUFFER_TOO_SMALL);
    CHECK(strcmp(diagnostic, "provider version buffer too small") == 0);
    CHECK(version.api_version == UINT32_MAX && version.library_version == UINT32_MAX);
    CHECK(version.vendor_required == strlen(expected_vendor) + 1U);
    CHECK(version.description_required == strlen(expected_description) + 1U);

    /* One byte short: nothing partially published. */
    version.vendor = vendor;
    version.vendor_capacity = version.vendor_required - 1U;
    version.description = description;
    version.description_capacity = sizeof description;
    CHECK(ams_mel_rf_data_get_provider_version(data, &version, NULL, 0, NULL) ==
          AMS_MEL_BUFFER_TOO_SMALL);
    CHECK(strcmp(vendor, "unchanged") == 0 && strcmp(description, "unchanged") == 0);
    CHECK(version.api_version == UINT32_MAX && version.library_version == UINT32_MAX);

    /* Exact capacity: complete publication. */
    version.vendor_capacity = version.vendor_required;
    version.description_capacity = version.description_required;
    CHECK(ams_mel_rf_data_get_provider_version(data, &version, diagnostic,
          sizeof diagnostic, &required) == AMS_MEL_OK);
    CHECK(diagnostic[0] == '\0' && required == 1U);
    CHECK(version.api_version == UINT32_C(0x0000A5A5));
    CHECK(version.library_version == UINT32_C(0x5A5A0000));
    CHECK(strcmp(vendor, expected_vendor) == 0);
    CHECK(strcmp(description, expected_description) == 0);
}

static void expect_version_failure(const char *scenario, ams_mel_status_t expected,
                                   const char *expected_diagnostic)
{
    ams_mel_rf_data *data = open_mock(scenario);
    char vendor[32] = "vendor unchanged";
    char description[32] = "description unchanged";
    char diagnostic[128] = {0};
    size_t required = 0;
    ams_mel_provider_version_v1 version = {
        UINT32_C(0xaaaaaaaa), UINT32_C(0xbbbbbbbb), vendor, sizeof vendor, 123,
        description, sizeof description, 456};
    CHECK(ams_mel_rf_data_get_provider_version(data, &version, diagnostic,
          sizeof diagnostic, &required) == expected);
    CHECK(strcmp(diagnostic, expected_diagnostic) == 0);
    CHECK(required == strlen(expected_diagnostic) + 1U);
    CHECK(version.api_version == UINT32_C(0xaaaaaaaa));
    CHECK(version.library_version == UINT32_C(0xbbbbbbbb));
    CHECK(strcmp(vendor, "vendor unchanged") == 0);
    CHECK(strcmp(description, "description unchanged") == 0);
    /* The owner stays open and closable after a contained failure. */
    close_ok(&data);
}

static void case_version(void)
{
    ams_mel_rf_data *data = open_mock("success");
    check_success_version(data);
    check_success_version(data);
    close_ok(&data);
    expect_log(normal_close_log);
    expect_version_failure("version-invalid-utf8", AMS_MEL_PROVIDER_EXCEPTION,
                           "provider version contains invalid UTF-8 or NUL");
    expect_version_failure("version-nul", AMS_MEL_PROVIDER_EXCEPTION,
                           "provider version contains invalid UTF-8 or NUL");
    expect_version_failure("version-throw", AMS_MEL_PROVIDER_EXCEPTION,
                           "mock RF version exception");
    expect_version_failure("version-throw-unknown", AMS_MEL_PROVIDER_EXCEPTION,
                           "unknown provider exception");
    expect_version_failure("version-bad-alloc", AMS_MEL_INTERNAL_ERROR,
                           "allocation failed");
    no_violations();
}

static void case_version_long(void)
{
    ams_mel_rf_data *data = open_mock("version-long");
    ams_mel_provider_version_v1 version = {0, 0, NULL, 0, 0, NULL, 0, 0};
    char *vendor;
    char *description;
    CHECK(ams_mel_rf_data_get_provider_version(data, &version, NULL, 0, NULL) ==
          AMS_MEL_BUFFER_TOO_SMALL);
    CHECK(version.vendor_required == 300U + 3U + 1U);
    CHECK(version.description_required == 1000U + 4U + 1U);
    CHECK(version.api_version == 0U && version.library_version == 0U);
    vendor = malloc(version.vendor_required);
    description = malloc(version.description_required);
    CHECK(vendor != NULL && description != NULL);
    version.vendor = vendor;
    version.vendor_capacity = version.vendor_required;
    version.description = description;
    version.description_capacity = version.description_required;
    CHECK(ams_mel_rf_data_get_provider_version(data, &version, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(version.api_version == UINT32_C(0xfedcba98));
    CHECK(version.library_version == UINT32_C(0x01234567));
    CHECK(strlen(vendor) == 303U && vendor[0] == 'v' && vendor[299] == 'v');
    CHECK(strcmp(vendor + 300, "\xE2\x82\xAC") == 0);
    CHECK(strlen(description) == 1004U && description[999] == 'd');
    CHECK(strcmp(description + 1000, "\xF0\x9F\x93\xA1") == 0);
    free(vendor);
    free(description);
    close_ok(&data);
    expect_log(normal_close_log);
}

static void case_snapshot(void)
{
    ams_mel_rf_data *data = open_mock("success");
    pin provider = take_pin();
    ams_mel_rf_mfa_info *info = snapshot(data);
    const ams_mel_rf_mfa_info_v1 *view = view_of(info);
    const ams_mel_rf_face_info_v1 *faces = view->faces.data;
    const uint32_t *formats = view->supported_data_formats.data;
    const ams_mel_rf_frequency_range_v1 *spans[6];
    const unsigned calls = provider.getter_calls();
    CHECK(calls > 0U);
    verify_mock_snapshot(view, 2U, 0);
    spans[0] = faces[0].rx_frequency_ranges.data;
    spans[1] = faces[0].sample_frequency_ranges.data;
    spans[2] = faces[1].tx_frequency_ranges.data;
    spans[3] = faces[1].sample_frequency_ranges.data;
    spans[4] = faces[0].tx_frequency_ranges.data;
    spans[5] = faces[1].rx_frequency_ranges.data;
    /* Repeated inspection: identical addresses and contents, no provider call. */
    for (int round = 0; round < 100; ++round) {
        const ams_mel_rf_mfa_info_v1 *again = view_of(info);
        CHECK(again == view);
        CHECK(again->faces.data == faces);
        CHECK(again->supported_data_formats.data == formats);
        CHECK(again->faces.data[0].rx_frequency_ranges.data == spans[0]);
        CHECK(again->faces.data[0].sample_frequency_ranges.data == spans[1]);
        CHECK(again->faces.data[1].tx_frequency_ranges.data == spans[2]);
        CHECK(again->faces.data[1].sample_frequency_ranges.data == spans[3]);
        CHECK(again->faces.data[0].tx_frequency_ranges.data == spans[4]);
        CHECK(again->faces.data[1].rx_frequency_ranges.data == spans[5]);
        verify_mock_snapshot(again, 2U, 0);
    }
    CHECK(provider.getter_calls() == calls);
    CHECK(provider.forbidden_calls() == 0U);
    close_info(&info);
    CHECK(provider.getter_calls() == calls);
    close_ok(&data);
    CHECK(provider.shutdown_calls() == 1U);
    drop_pin(&provider);
    CHECK(!provider_loaded());
    expect_log(normal_close_log);
}

static void case_two_snapshots(void)
{
    ams_mel_rf_data *data = open_mock("success");
    ams_mel_rf_mfa_info *first = snapshot(data);
    ams_mel_rf_mfa_info *second = snapshot(data);
    const ams_mel_rf_mfa_info_v1 *first_view = view_of(first);
    const ams_mel_rf_mfa_info_v1 *second_view = view_of(second);
    CHECK(first != second && first_view != second_view);
    CHECK(first_view->faces.data != second_view->faces.data);
    CHECK(first_view->supported_data_formats.data !=
          second_view->supported_data_formats.data);
    CHECK(first_view->faces.data[0].rx_frequency_ranges.data !=
          second_view->faces.data[0].rx_frequency_ranges.data);
    verify_mock_snapshot(first_view, 2U, 0);
    verify_mock_snapshot(second_view, 2U, 0);
    close_info(&first);
    verify_mock_snapshot(second_view, 2U, 0);
    CHECK(view_of(second) == second_view);
    close_ok(&data);
    CHECK(!provider_loaded());
    verify_mock_snapshot(second_view, 2U, 0);
    close_info(&second);
    expect_log(normal_close_log);
}

static void case_snapshot_independent(void)
{
    ams_mel_rf_data *data = open_mock("success");
    ams_mel_rf_mfa_info *info = snapshot(data);
    const ams_mel_rf_mfa_info_v1 *before = view_of(info);
    close_ok(&data);
    /* The provider DSO is really gone: no test pin, no retained graph. */
    CHECK(!provider_loaded());
    expect_log(normal_close_log);
    /* View and read the entire snapshot after unload. */
    CHECK(view_of(info) == before);
    verify_mock_snapshot(view_of(info), 2U, 0);
    close_info(&info);
    expect_log(normal_close_log);
}

static void expect_snapshot_failure(const ams_mel_rf_data *data,
                                    ams_mel_status_t expected,
                                    const char *expected_diagnostic)
{
    ams_mel_rf_mfa_info *info = NULL;
    char diagnostic[128] = {0};
    size_t required = 0;
    CHECK(ams_mel_rf_data_get_mfa_info(data, &info, diagnostic, sizeof diagnostic,
          &required) == expected);
    CHECK(info == NULL);
    CHECK(strcmp(diagnostic, expected_diagnostic) == 0);
    CHECK(required == strlen(expected_diagnostic) + 1U);
}

static void case_snapshot_throw(void)
{
    /* A getter throws after face 7 was completely copied: no partial
     * snapshot is published and the owner remains usable. */
    ams_mel_rf_data *data = open_mock("mfa-throw-once");
    ams_mel_rf_mfa_info *info;
    expect_snapshot_failure(data, AMS_MEL_PROVIDER_EXCEPTION, "mock RFMFAInfo exception");
    check_success_version(data);
    info = snapshot(data);
    verify_mock_snapshot(view_of(info), 2U, 0);
    close_ok(&data);
    verify_mock_snapshot(view_of(info), 2U, 0);
    close_info(&info);
    expect_log(normal_close_log);

    reset_log();
    data = open_mock("mfa-throw-unknown-once");
    expect_snapshot_failure(data, AMS_MEL_PROVIDER_EXCEPTION,
                            "unknown provider RFMFAInfo exception");
    info = snapshot(data);
    close_info(&info);
    close_ok(&data);
    expect_log(normal_close_log);

    reset_log();
    data = open_mock("mfa-reference-throw");
    expect_snapshot_failure(data, AMS_MEL_PROVIDER_EXCEPTION, "mock getRFMFAInfo exception");
    check_success_version(data);
    close_ok(&data);
    expect_log(normal_close_log);
    CHECK(!provider_loaded());
    no_violations();
}

static void case_inconsistent_faces(void)
{
    /* getNumFaces() == 5 while getFaceIDs() == {7, 42}: both preserved. */
    ams_mel_rf_data *data = open_mock("inconsistent-faces");
    ams_mel_rf_mfa_info *info = snapshot(data);
    verify_mock_snapshot(view_of(info), 5U, 0);
    close_ok(&data);
    close_info(&info);
    no_violations();
}

static void case_future_format(void)
{
    /* JobDataFormat has the fixed underlying type int, so a provider value
     * beyond the published enumerators is well defined and must survive as
     * its raw uint32_t bits. */
    ams_mel_rf_data *data = open_mock("future-format");
    ams_mel_rf_mfa_info *info = snapshot(data);
    verify_mock_snapshot(view_of(info), 2U, 1);
    close_ok(&data);
    close_info(&info);
    no_violations();
}

static void case_close(void)
{
    ams_mel_rf_data *data = open_mock("success");
    pin provider = take_pin();
    char diagnostic[64] = "not cleared";
    size_t required = 0;
    CHECK(provider.shutdown_calls() == 0U);
    CHECK(ams_mel_rf_data_close(&data, diagnostic, sizeof diagnostic, &required) ==
          AMS_MEL_OK);
    CHECK(data == NULL && diagnostic[0] == '\0' && required == 1U);
    CHECK(provider.shutdown_calls() == 1U);
    /* DataMEL destroyed at Close; only the test pin keeps the DSO mapped. */
    expect_log("rf_factory_called\nrf_shutdown\nrf_data_destroyed\n"
               "rf_mfa_info_destroyed\n");
    for (int repeat = 0; repeat < 3; ++repeat) {
        CHECK(ams_mel_rf_data_close(&data, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(data == NULL);
    }
    CHECK(provider.shutdown_calls() == 1U);
    CHECK(provider.forbidden_calls() == 0U);
    drop_pin(&provider);
    CHECK(!provider_loaded());
    expect_log(normal_close_log);
    no_violations();
}

static void shutdown_throw(const char *scenario, const char *expected_diagnostic)
{
    ams_mel_rf_data *data = open_mock(scenario);
    ams_mel_rf_mfa_info *info = snapshot(data);
    pin provider = take_pin();
    char diagnostic[128] = {0};
    size_t required = 0;
    CHECK(ams_mel_rf_data_close(&data, diagnostic, sizeof diagnostic, &required) ==
          AMS_MEL_PROVIDER_EXCEPTION);
    CHECK(data == NULL);
    CHECK(strcmp(diagnostic, expected_diagnostic) == 0);
    CHECK(required == strlen(expected_diagnostic) + 1U);
    CHECK(provider.shutdown_calls() == 1U);
    /* No retry and no rescue through the consumed public owner. */
    CHECK(ams_mel_rf_data_close(&data, NULL, 0, NULL) == AMS_MEL_OK && data == NULL);
    CHECK(provider.shutdown_calls() == 1U);
    CHECK(provider.forbidden_calls() == 0U);
    drop_pin(&provider);
    /* The provider graph is permanently retained: DSO still mapped. */
    CHECK(provider_loaded());
    CHECK(occurrences("rf_shutdown") == 1U);
    CHECK(occurrences("rf_data_destroyed") == 0U);
    CHECK(occurrences("rf_mfa_info_destroyed") == 0U);
    CHECK(occurrences("library_unloaded") == 0U);
    /* The snapshot never depended on the provider. */
    verify_mock_snapshot(view_of(info), 2U, 0);
    close_info(&info);
    no_violations();
    /* Process exit, not a rescue, is the cleanup boundary. */
}

static void case_shutdown_throw(void)
{
    shutdown_throw("shutdown-throw", "mock RF shutdown exception");
}

static void case_shutdown_throw_unknown(void)
{
    shutdown_throw("shutdown-throw-unknown", "unknown provider shutdown exception");
}

static void case_invalid(void)
{
    ams_mel_rf_data *data = NULL;
    ams_mel_rf_data *sentinel = (ams_mel_rf_data *)(uintptr_t)1;
    ams_mel_rf_mfa_info *info = NULL;
    ams_mel_rf_mfa_info *info_sentinel = (ams_mel_rf_mfa_info *)(uintptr_t)1;
    const ams_mel_rf_mfa_info_v1 *view = NULL;
    ams_mel_provider_version_v1 version = {0, 0, NULL, 0, 0, NULL, 0, 0};
    char diagnostic[64] = {0};
    size_t required = 0;
    const char *mock = AMS_MEL_TEST_MOCK_RF_PROVIDER;

    CHECK(ams_mel_rf_data_open(NULL, "success", &data, diagnostic, sizeof diagnostic,
          &required) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(data == NULL && strcmp(diagnostic, "invalid argument") == 0);
    CHECK(required == strlen("invalid argument") + 1U);
    CHECK(ams_mel_rf_data_open(mock, NULL, &data, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_data_open(mock, "success", NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_data_open(mock, "success", &sentinel, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(sentinel == (ams_mel_rf_data *)(uintptr_t)1);
    CHECK(ams_mel_rf_data_open(mock, "success", &data, NULL, 1, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(data == NULL);
    /* No argument failure loaded the provider. */
    expect_log("");
    CHECK(!provider_loaded());

    CHECK(ams_mel_rf_data_get_provider_version(NULL, &version, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_data_get_mfa_info(NULL, &info, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(info == NULL);
    CHECK(ams_mel_rf_mfa_info_view(NULL, &view, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(view == NULL);
    CHECK(ams_mel_rf_mfa_info_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_mfa_info_close(&info, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_data_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_data_close(&data, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT);

    data = open_mock("success");
    CHECK(ams_mel_rf_data_get_provider_version(data, NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    version.vendor_capacity = 1U; /* NULL buffer with nonzero capacity */
    CHECK(ams_mel_rf_data_get_provider_version(data, &version, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    version.vendor_capacity = 0U;
    version.description_capacity = 1U;
    CHECK(ams_mel_rf_data_get_provider_version(data, &version, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    version.description_capacity = 0U;
    CHECK(ams_mel_rf_data_get_provider_version(data, &version, NULL, 1, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_data_get_mfa_info(data, NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_data_get_mfa_info(data, &info_sentinel, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(info_sentinel == (ams_mel_rf_mfa_info *)(uintptr_t)1);
    CHECK(ams_mel_rf_data_get_mfa_info(data, &info, NULL, 1, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(info == NULL);
    info = snapshot(data);
    CHECK(ams_mel_rf_mfa_info_view(info, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_mfa_info_view(info, &view, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(view == NULL);
    close_info(&info);
    /* The live owner is untouched by argument failures. */
    check_success_version(data);
    close_ok(&data);
    expect_log(normal_close_log);
    no_violations();
}

static void case_allocation(void)
{
    ams_mel_rf_data *data = NULL;
    ams_mel_rf_mfa_info *info = NULL;
    char diagnostic[64] = {0};

    /* Public owner allocation: fails before the factory, no provider object. */
    CHECK(setenv("AMS_MEL_TEST_RF_ALLOCATION_FAILURE", "open", 1) == 0);
    CHECK(ams_mel_rf_data_open(AMS_MEL_TEST_MOCK_RF_PROVIDER, "success", &data,
          diagnostic, sizeof diagnostic, NULL) == AMS_MEL_INTERNAL_ERROR);
    CHECK(data == NULL && strcmp(diagnostic, "allocation failed") == 0);
    CHECK(!provider_loaded());
    expect_log("library_unloaded\n");
    CHECK(unsetenv("AMS_MEL_TEST_RF_ALLOCATION_FAILURE") == 0);

    reset_log();
    data = open_mock("success");
    /* Snapshot owner allocation. */
    CHECK(setenv("AMS_MEL_TEST_RF_ALLOCATION_FAILURE", "snapshot-owner", 1) == 0);
    CHECK(ams_mel_rf_data_get_mfa_info(data, &info, diagnostic, sizeof diagnostic,
          NULL) == AMS_MEL_INTERNAL_ERROR);
    CHECK(info == NULL && strcmp(diagnostic, "allocation failed") == 0);
    /* Nested OOM after face 7's ranges and face 42's Rx ranges were copied. */
    CHECK(setenv("AMS_MEL_TEST_RF_ALLOCATION_FAILURE", "snapshot-range", 1) == 0);
    CHECK(ams_mel_rf_data_get_mfa_info(data, &info, diagnostic, sizeof diagnostic,
          NULL) == AMS_MEL_INTERNAL_ERROR);
    CHECK(info == NULL && strcmp(diagnostic, "allocation failed") == 0);
    CHECK(unsetenv("AMS_MEL_TEST_RF_ALLOCATION_FAILURE") == 0);
    /* Strong guarantee: the owner is still fully usable and closable. */
    info = snapshot(data);
    verify_mock_snapshot(view_of(info), 2U, 0);
    close_ok(&data);
    close_info(&info);
    CHECK(!provider_loaded());
    expect_log(normal_close_log);
    no_violations();
}

int main(int argc, char **argv)
{
    static const struct {
        const char *name;
        void (*run)(void);
    } cases[] = {
        {"open", case_open},
        {"version", case_version},
        {"version-long", case_version_long},
        {"snapshot", case_snapshot},
        {"two-snapshots", case_two_snapshots},
        {"snapshot-independent", case_snapshot_independent},
        {"snapshot-throw", case_snapshot_throw},
        {"inconsistent-faces", case_inconsistent_faces},
        {"future-format", case_future_format},
        {"missing-symbol", case_missing_symbol},
        {"null-factory", case_null_factory},
        {"factory-throw", case_factory_throw},
        {"close", case_close},
        {"shutdown-throw", case_shutdown_throw},
        {"shutdown-throw-unknown", case_shutdown_throw_unknown},
        {"invalid", case_invalid},
        {"allocation", case_allocation},
    };
    CHECK(argc == 2);
    setup_log();
    CHECK(!provider_loaded());
    for (size_t index = 0; index < sizeof cases / sizeof cases[0]; ++index) {
        if (strcmp(argv[1], cases[index].name) == 0) {
            cases[index].run();
            printf("PASS: rf-data-%s\n", cases[index].name);
            return EXIT_SUCCESS;
        }
    }
    fprintf(stderr, "unknown case %s\n", argv[1]);
    return EXIT_FAILURE;
}
