/* Task 033D opt-in real Squall RF ProductRxEndpoint ComplexINT16 receive
 * (C11). The ProductRxEndpoint, every event, every counter, and every
 * assertion go through the production C facade. The only non-facade code is
 * the separately built TEST-ONLY job helper (squall_rf_job_helper.cpp),
 * loaded with dlopen, because pinned Squall drops ProductRx data unless an RX
 * job is active and the production facade implements no RF C2/Job API.
 *
 * Not part of make check, ordinary CTest, or hosted CI.
 *
 * usage: squall_rf_rx_c PROVIDER_SO PROFILE_JSON JOB_HELPER_SO */
#define _POSIX_C_SOURCE 200809L

#include <ams_mel/abi.h>

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return EXIT_FAILURE; \
    } \
} while (0)

#define EVENTS 8U

typedef int (*job_start_fn)(const char *, const char *, char *, size_t);
typedef void (*job_stop_fn)(void);

static int failed(const char *step, ams_mel_status_t status, const char *diagnostic)
{
    fprintf(stderr, "FAIL: %s (status %d): %s\n", step, (int)status, diagnostic);
    return EXIT_FAILURE;
}

/* Pinned Squall publishes a default-constructed ProductRxMetadata. */
static int check_sparse_metadata(const ams_mel_rf_product_rx_metadata_v1 *m)
{
    REQUIRE(m->mel_protocol_version_id == 0U && m->va_definition_id == 0U);
    REQUIRE(m->va_instance_id == 0U && m->job_details_id == 0U);
    REQUIRE(m->job_interval_id == 0U && m->lf_type_id == 0U && m->lf_instance_id == 0U);
    REQUIRE(m->phase_coherence_with_prior == 0U);
    REQUIRE(m->first_rx_event_start_s == 0 && m->first_rx_event_start_fs == 0);
    REQUIRE(m->rx_stream_ids.size == 0U && m->rx_stream_ids.data == NULL);
    return EXIT_SUCCESS;
}

static int check_event(const ams_mel_rf_product_rx_event_v1 *view, uint64_t endpoint_id)
{
    REQUIRE(view->endpoint_id == endpoint_id);
    REQUIRE(view->data_format == AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT16);
    REQUIRE(view->samples.size > 0U && view->samples.data != NULL);
    REQUIRE(check_sparse_metadata(&view->metadata) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static size_t nonzero(const ams_mel_rf_product_rx_event_v1 *view)
{
    size_t index, count = 0;
    for (index = 0; index < view->samples.size; ++index)
        if (view->samples.data[index].real != 0 || view->samples.data[index].imag != 0) ++count;
    return count;
}

static uint64_t fingerprint(const ams_mel_rf_product_rx_event_v1 *view)
{
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t index;
    for (index = 0; index < view->samples.size; ++index) {
        hash = (hash ^ (uint16_t)view->samples.data[index].real) * UINT64_C(1099511628211);
        hash = (hash ^ (uint16_t)view->samples.data[index].imag) * UINT64_C(1099511628211);
    }
    return hash;
}

int main(int argc, char **argv)
{
    ams_mel_rf_data *data = NULL;
    ams_mel_rf_product_rx_request *request = NULL;
    ams_mel_rf_product_rx *endpoint = NULL;
    ams_mel_rf_product_rx_event *events[EVENTS] = {NULL};
    const ams_mel_rf_product_rx_event_v1 *first = NULL;
    ams_mel_rf_product_rx_config_v1 config;
    ams_mel_rf_product_rx_request_result_v1 result = {UINT32_C(0xFFFF)};
    ams_mel_rf_product_rx_info_v1 info = {0, 0};
    ams_mel_rf_product_rx_counters_v1 counters;
    ams_mel_rf_complex_i16_v1 *copy_a;
    char diagnostic[512] = {0};
    char job_error[512] = {0};
    ams_mel_status_t status;
    void *helper;
    job_start_fn job_start;
    job_stop_fn job_stop;
    uint64_t hash_a, distinct = 0;
    size_t count_a, index, received = 0;

    if (argc != 4) {
        fprintf(stderr, "usage: %s PROVIDER_SO PROFILE_JSON JOB_HELPER_SO\n", argv[0]);
        return EXIT_FAILURE;
    }
    helper = dlopen(argv[3], RTLD_NOW | RTLD_LOCAL);
    REQUIRE(helper != NULL);
    *(void **)(&job_start) = dlsym(helper, "squall_rf_test_job_start");
    *(void **)(&job_stop) = dlsym(helper, "squall_rf_test_job_stop");
    REQUIRE(job_start != NULL && job_stop != NULL);

    status = ams_mel_rf_data_open(argv[1], argv[2], &data, diagnostic, sizeof diagnostic, NULL);
    if (status != AMS_MEL_OK) return failed("ams_mel_rf_data_open", status, diagnostic);

    memset(&config, 0, sizeof config);
    config.data_format = AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT16;
    config.region_size_bytes = UINT64_C(1024) * 4U; /* Squall's 1024 ComplexINT16 */
    config.queue_capacity = 256U;                   /* adequate: no overflow */
    config.max_samples_per_event = 65536U;
    status = ams_mel_rf_data_submit_product_rx(data, &config, &request, diagnostic,
                                               sizeof diagnostic, NULL);
    if (status != AMS_MEL_OK) return failed("submit_product_rx", status, diagnostic);
    status = ams_mel_rf_product_rx_request_wait(request, 20000U, &result, diagnostic,
                                                sizeof diagnostic, NULL);
    if (status != AMS_MEL_OK) return failed("request_wait", status, diagnostic);
    REQUIRE(result.error_code == AMS_MEL_ERROR_NONE);
    status = ams_mel_rf_product_rx_request_claim(request, &endpoint, &info, diagnostic,
                                                 sizeof diagnostic, NULL);
    if (status != AMS_MEL_OK) return failed("request_claim", status, diagnostic);
    REQUIRE(ams_mel_rf_product_rx_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    printf("ProductRxEndpoint: id=%llu assigned_format=%u\n",
           (unsigned long long)info.endpoint_id, (unsigned)info.assigned_data_format);
    REQUIRE(info.assigned_data_format == AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT16);
    REQUIRE(info.endpoint_id != 0U);

    /* Test-only: activate a pinned-Squall RX job (production has no C2). */
    if (!job_start(argv[1], argv[2], job_error, sizeof job_error)) {
        fprintf(stderr, "FAIL: test-only job activation: %s\n", job_error);
        return EXIT_FAILURE;
    }

    status = ams_mel_rf_product_rx_receive(endpoint, 30000U, &events[0], diagnostic,
                                           sizeof diagnostic, NULL);
    if (status != AMS_MEL_OK) return failed("receive event A", status, diagnostic);
    received = 1U;
    REQUIRE(ams_mel_rf_product_rx_event_view(events[0], &first, NULL, 0, NULL) == AMS_MEL_OK);
    REQUIRE(check_event(first, info.endpoint_id) == EXIT_SUCCESS);
    count_a = first->samples.size;
    hash_a = fingerprint(first);
    copy_a = (ams_mel_rf_complex_i16_v1 *)malloc(count_a * sizeof *copy_a);
    REQUIRE(copy_a != NULL);
    memcpy(copy_a, first->samples.data, count_a * sizeof *copy_a);
    printf("event A: %zu ComplexINT16 elements, %zu nonzero, first=(%d,%d)\n", count_a,
           nonzero(first), first->samples.data[0].real, first->samples.data[0].imag);
    /* Non-vacuous copy-boundary evidence needs non-trivial sample content. */
    REQUIRE(nonzero(first) > 0U);

    /* Later events: Squall reuses its internal IQ vector for each one. */
    for (index = 1U; index < EVENTS; ++index) {
        const ams_mel_rf_product_rx_event_v1 *view = NULL;
        status = ams_mel_rf_product_rx_receive(endpoint, 30000U, &events[index], diagnostic,
                                               sizeof diagnostic, NULL);
        if (status != AMS_MEL_OK) return failed("receive later event", status, diagnostic);
        ++received;
        REQUIRE(ams_mel_rf_product_rx_event_view(events[index], &view, NULL, 0, NULL) ==
                AMS_MEL_OK);
        REQUIRE(check_event(view, info.endpoint_id) == EXIT_SUCCESS);
        REQUIRE(view->samples.data != first->samples.data);
        if (fingerprint(view) != hash_a) ++distinct;
    }
    printf("received %zu events; %llu later events differ from event A\n", received,
           (unsigned long long)distinct);
    /* Squall reuses one IQ vector per endpoint; later products must differ
     * from A, or the unchanged-A check below would prove nothing. */
    REQUIRE(distinct == EVENTS - 1U);
    /* Event A is unchanged after later callbacks reused the provider buffer. */
    REQUIRE(first->samples.size == count_a && fingerprint(first) == hash_a);
    REQUIRE(memcmp(first->samples.data, copy_a, count_a * sizeof *copy_a) == 0);

    counters = (ams_mel_rf_product_rx_counters_v1){0, 0, 0, 0, 0, 0};
    REQUIRE(ams_mel_rf_product_rx_get_counters(endpoint, &counters, NULL, 0, NULL) == AMS_MEL_OK);
    printf("counters: received=%llu queued=%llu dropped=%llu malformed=%llu alloc=%llu "
           "after_close=%llu\n",
           (unsigned long long)counters.callbacks_received,
           (unsigned long long)counters.products_queued,
           (unsigned long long)counters.products_dropped_queue_full,
           (unsigned long long)counters.malformed_or_unsupported,
           (unsigned long long)counters.allocation_failures,
           (unsigned long long)counters.callbacks_after_close);
    REQUIRE(counters.products_queued >= EVENTS);
    REQUIRE(counters.malformed_or_unsupported == 0U);
    REQUIRE(counters.products_dropped_queue_full == 0U);
    REQUIRE(counters.allocation_failures == 0U && counters.callbacks_after_close == 0U);

    /* Endpoint Close, then RF Data Close (the final child ran shutdown). */
    status = ams_mel_rf_product_rx_close(&endpoint, diagnostic, sizeof diagnostic, NULL);
    if (status != AMS_MEL_OK) return failed("product_rx_close", status, diagnostic);
    status = ams_mel_rf_data_close(&data, diagnostic, sizeof diagnostic, NULL);
    if (status != AMS_MEL_OK) return failed("rf_data_close", status, diagnostic);
    job_stop();

    /* Event A survives endpoint Close, DataMEL destruction, and later
     * provider activity. The DSO staying mapped after registration is
     * intentional generic bridge policy, not a Squall leak. */
    REQUIRE(first->samples.size == count_a && fingerprint(first) == hash_a);
    REQUIRE(memcmp(first->samples.data, copy_a, count_a * sizeof *copy_a) == 0);
    REQUIRE(check_sparse_metadata(&first->metadata) == EXIT_SUCCESS);
    for (index = 0; index < EVENTS; ++index)
        REQUIRE(ams_mel_rf_product_rx_event_close(&events[index], NULL, 0, NULL) == AMS_MEL_OK);
    free(copy_a);
    puts("PASS: real Squall RF ProductRxEndpoint ComplexINT16 receive (creation, "
         "assigned format, events, sparse metadata, buffer-reuse copy boundary, "
         "event survives endpoint/DataMEL Close)");
    return EXIT_SUCCESS;
}
