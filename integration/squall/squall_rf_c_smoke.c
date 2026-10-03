/* Task 033B opt-in real Squall RF DataMEL smoke (C11 only).
 *
 * Uses only the public C ABI: open RF DataMEL, VersionInfo, one RFMFAInfo
 * snapshot, RF Close, then reads the snapshot after RF Close. It never
 * creates a ProductRxEndpoint, listens for UDP IQ, requests jobs, or touches
 * VADB, and it is not part of ordinary CTest or hosted CI.
 *
 * usage: squall_rf_c_smoke PROVIDER_SO PROFILE_JSON */
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

static int failed(const char *step, ams_mel_status_t status, const char *diagnostic)
{
    fprintf(stderr, "FAIL: %s (status %d): %s\n", step, (int)status, diagnostic);
    return EXIT_FAILURE;
}

static void print_ranges(const char *name, ams_mel_rf_frequency_range_span_v1 span)
{
    printf("  %s: %zu\n", name, span.size);
    for (size_t index = 0; index < span.size; ++index)
        printf("    [%zu] %.3f Hz .. %.3f Hz\n", index, span.data[index].min_hz,
               span.data[index].max_hz);
}

static int check_ranges(ams_mel_rf_frequency_range_span_v1 span)
{
    REQUIRE(span.size > 0U && span.data != NULL);
    for (size_t index = 0; index < span.size; ++index)
        REQUIRE(span.data[index].min_hz <= span.data[index].max_hz);
    return EXIT_SUCCESS;
}

/* The pinned Squall contract recorded by Task 033A. Live frequency ranges come
 * from the RF backend's GetStatus, so only their shape is asserted. */
static int check_snapshot(const ams_mel_rf_mfa_info_v1 *view)
{
    const ams_mel_rf_face_info_v1 *face;
    REQUIRE(view->reported_num_faces == 1U);
    REQUIRE(view->contains_open_additions == 0U);
    REQUIRE(view->scheduler_resolution_fs > 0);
    /* Pinned SquallRFMFAInfo publishes exactly {ComplexINT16}. */
    REQUIRE(view->supported_data_formats.size == 1U);
    REQUIRE(view->supported_data_formats.data[0] ==
            AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT16);
    REQUIRE(view->faces.size == 1U && view->faces.data != NULL);
    face = &view->faces.data[0];
    REQUIRE(face->face_id == 0U);
    REQUIRE(face->supports_receive == 1U);
    REQUIRE(face->supports_transmit == 0U);
    REQUIRE(face->requires_endpoint_association == 0U);
    REQUIRE(check_ranges(face->rx_frequency_ranges) == EXIT_SUCCESS);
    REQUIRE(check_ranges(face->sample_frequency_ranges) == EXIT_SUCCESS);
    REQUIRE(face->tx_frequency_ranges.size == 0U);
    return EXIT_SUCCESS;
}

static void print_snapshot(const ams_mel_rf_mfa_info_v1 *view)
{
    printf("RFMFAInfo: reported_num_faces=%llu faces=%zu open_additions=%u "
           "scheduler_resolution_fs=%lld max_context_bytes=%llu formats=%zu\n",
           (unsigned long long)view->reported_num_faces, view->faces.size,
           (unsigned)view->contains_open_additions,
           (long long)view->scheduler_resolution_fs,
           (unsigned long long)view->max_user_defined_context_bytes,
           view->supported_data_formats.size);
    for (size_t index = 0; index < view->supported_data_formats.size; ++index)
        printf("  format[%zu]=%u\n", index,
               (unsigned)view->supported_data_formats.data[index]);
    for (size_t index = 0; index < view->faces.size; ++index) {
        const ams_mel_rf_face_info_v1 *face = &view->faces.data[index];
        printf("face %u: rx=%u tx=%u assoc=%u agc=%lld min_req=%lld max_req=%lld "
               "min_detail=%lld txrx=%lld rxtx=%lld txtx=%lld rxrx=%lld (fs)\n",
               (unsigned)face->face_id, (unsigned)face->supports_receive,
               (unsigned)face->supports_transmit,
               (unsigned)face->requires_endpoint_association,
               (long long)face->agc_processing_time_fs,
               (long long)face->min_job_request_lead_time_fs,
               (long long)face->max_job_request_lead_time_fs,
               (long long)face->min_job_detail_lead_time_fs,
               (long long)face->tx_rx_switching_time_fs,
               (long long)face->rx_tx_switching_time_fs,
               (long long)face->tx_tx_switching_time_fs,
               (long long)face->rx_rx_switching_time_fs);
        print_ranges("rx_frequency_ranges", face->rx_frequency_ranges);
        print_ranges("tx_frequency_ranges", face->tx_frequency_ranges);
        print_ranges("sample_frequency_ranges", face->sample_frequency_ranges);
    }
}

static int check_physical(const ams_mel_rf_physical_data_v1 *v)
{
    REQUIRE(v->antenna_height_m == 0.25 && v->antenna_width_m == 0.25);
    REQUIRE(v->lattice_angle_rad == 0.0);
    REQUIRE(v->location.offset_x_m == 0.0 && v->location.offset_y_m == 0.0 && v->location.offset_z_m == 0.0);
    REQUIRE(v->location.key.size == 0 && v->location.key.data != NULL);
    REQUIRE(v->location.system_name.size == 0 && v->location.system_name.data != NULL);
    REQUIRE(v->orientation.roll_rad == 0.0 && v->orientation.pitch_rad == 0.0 && v->orientation.yaw_rad == 0.0);
    REQUIRE(v->boresight.roll_rad == 0.0 && v->boresight.pitch_rad == 0.0 && v->boresight.yaw_rad == 0.0);
    return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
    ams_mel_rf_data *data = NULL;
    ams_mel_rf_physical_data *physical = NULL;
    const ams_mel_rf_physical_data_v1 *physical_view = NULL;
    ams_mel_rf_mfa_info *info = NULL;
    const ams_mel_rf_mfa_info_v1 *view = NULL;
    const ams_mel_rf_mfa_info_v1 *after = NULL;
    char diagnostic[512] = {0};
    char vendor[128] = {0};
    char description[256] = {0};
    ams_mel_status_t status;
    ams_mel_provider_version_v1 version = {0, 0, vendor, sizeof vendor, 0,
                                           description, sizeof description, 0};
    void *probe;

    if (argc != 3) {
        fprintf(stderr, "usage: %s PROVIDER_SO PROFILE_JSON\n", argv[0]);
        return EXIT_FAILURE;
    }
    status = ams_mel_rf_data_open(argv[1], argv[2], &data, diagnostic,
                                  sizeof diagnostic, NULL);
    if (status != AMS_MEL_OK) return failed("ams_mel_rf_data_open", status, diagnostic);
    {
        const int64_t inputs[] = {0, 1, INT64_C(123456789), -INT64_C(123456789)};
        for (size_t i = 0; i < sizeof inputs / sizeof inputs[0]; ++i) {
            int64_t output = 0;
            status = ams_mel_rf_data_quantize_duration(data, inputs[i], &output,
                                                       diagnostic, sizeof diagnostic, NULL);
            if (status != AMS_MEL_OK) return failed("quantize_duration", status, diagnostic);
            REQUIRE(output == inputs[i]); /* Pinned Squall identity, not bridge policy. */
        }
    }

    status = ams_mel_rf_data_get_provider_version(data, &version, diagnostic,
                                                  sizeof diagnostic, NULL);
    if (status != AMS_MEL_OK) return failed("get_provider_version", status, diagnostic);
    printf("RF provider: api=%u library=%u vendor=\"%s\" description=\"%s\"\n",
           (unsigned)version.api_version, (unsigned)version.library_version,
           vendor, description);
    REQUIRE(version.api_version == 1U && version.library_version == 1U);
    REQUIRE(strcmp(vendor, "Squall") == 0);
    REQUIRE(strcmp(description, "Squall Simulator RF MEL") == 0);

    status = ams_mel_rf_data_get_mfa_info(data, &info, diagnostic, sizeof diagnostic,
                                          NULL);
    if (status != AMS_MEL_OK) return failed("get_mfa_info", status, diagnostic);
    status = ams_mel_rf_mfa_info_view(info, &view, diagnostic, sizeof diagnostic, NULL);
    if (status != AMS_MEL_OK) return failed("mfa_info_view", status, diagnostic);
    print_snapshot(view);
    REQUIRE(check_snapshot(view) == EXIT_SUCCESS);

    REQUIRE(ams_mel_rf_data_get_physical_data(data, 0, &physical, diagnostic, sizeof diagnostic, NULL) == AMS_MEL_OK);
    REQUIRE(ams_mel_rf_physical_data_view(physical, &physical_view, NULL, 0, NULL) == AMS_MEL_OK);
    REQUIRE(check_physical(physical_view) == EXIT_SUCCESS);

    /* RF Close: shutdown once, destroy DataMEL, then unload the provider. */
    status = ams_mel_rf_data_close(&data, diagnostic, sizeof diagnostic, NULL);
    if (status != AMS_MEL_OK) return failed("ams_mel_rf_data_close", status, diagnostic);
    REQUIRE(data == NULL);
    REQUIRE(ams_mel_rf_data_close(&data, NULL, 0, NULL) == AMS_MEL_OK && data == NULL);
    /* Informational: a provider may pin itself (for example via static
     * gRPC state); the bridge contract is only that it released its own. */
    probe = dlopen(argv[1], RTLD_NOW | RTLD_NOLOAD);
    printf("provider still mapped after RF Close: %s\n", probe ? "yes" : "no");
    if (probe) (void)dlclose(probe);

    /* The snapshot owns everything it references. */
    REQUIRE(ams_mel_rf_mfa_info_view(info, &after, NULL, 0, NULL) == AMS_MEL_OK);
    REQUIRE(after == view);
    REQUIRE(check_snapshot(after) == EXIT_SUCCESS);
    REQUIRE(ams_mel_rf_mfa_info_close(&info, NULL, 0, NULL) == AMS_MEL_OK && info == NULL);
    REQUIRE(ams_mel_rf_physical_data_view(physical, &physical_view, NULL, 0, NULL) == AMS_MEL_OK);
    REQUIRE(check_physical(physical_view) == EXIT_SUCCESS);
    REQUIRE(ams_mel_rf_physical_data_close(&physical, NULL, 0, NULL) == AMS_MEL_OK && physical == NULL);
    puts("PASS: real Squall RF DataMEL C smoke (version, MFA snapshot, close, "
         "snapshot after close)");
    return EXIT_SUCCESS;
}
