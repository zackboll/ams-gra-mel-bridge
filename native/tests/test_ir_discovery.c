#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); return EXIT_FAILURE; } } while (0)

static int text_is(ams_mel_string_view_v1 value, const char *expected)
{ return value.size == strlen(expected) && memcmp(value.data, expected, value.size) == 0; }

static int discover(const char *scenario, int rich, int empty)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_capability_list *list = NULL, *again = NULL;
    ams_mel_ir_channel_capability *entry = NULL;
    const ams_mel_ir_channel_capability_v1 *value = NULL;
    size_t count = 99;
    const uint32_t types[] = {AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL,
        AMS_MEL_IR_CHANNEL_HEALTH_AND_STATUS, AMS_MEL_IR_CHANNEL_IRST_IMAGE,
        AMS_MEL_IR_CHANNEL_STACKED_IMAGE};
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "", &session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_capability_list_create(session, &list, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_capability_list_create(session, &list, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT && list != NULL);
    CHECK(ams_mel_ir_capability_list_count(list, &count, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_capability_list_close(&list, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT && list != NULL);
    CHECK(ams_mel_ir_capability_list_create(session, &again, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_capability_list_close(&again, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    /* No dlopen observer pins the DSO. Lists must not retain provider code. */
    void *loaded = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    if (loaded) dlclose(loaded);
    CHECK(loaded == NULL);
    CHECK(ams_mel_ir_capability_list_count(list, &count, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(count == (empty ? 0U : 4U));
    CHECK(ams_mel_ir_capability_list_get(list, count, &entry, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT && entry == NULL);
    CHECK(ams_mel_ir_capability_list_get(list, SIZE_MAX, &entry, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_capability_list_get(list, 0, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_capability_list_count(list, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    for (size_t i = 0; i < count; ++i) {
        char label[64];
        CHECK(snprintf(label, sizeof label, "discovery-%zu-\xCE\xB1", i) > 0);
        CHECK(ams_mel_ir_capability_list_get(list, i, &entry, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_channel_capability_view(entry, &value, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(text_is(value->channel_id.descriptive_label, label));
        for (size_t j = 0; j < 16; ++j) CHECK(value->channel_id.uuid[j] == 0x10U + i + j * 7U);
        CHECK(value->channel_types.size == (rich ? 3U : 1U) && value->channel_types.data[0] == types[i]);
        CHECK(ams_mel_ir_capability_list_get(list, i, &entry, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT && entry != NULL);
        if (rich) {
            CHECK(value->channel_types.data[1] == AMS_MEL_IR_CHANNEL_INSTRUMENTATION && value->channel_types.data[2] == AMS_MEL_IR_CHANNEL_RESERVED_2);
            CHECK(value->height == 1080 && value->width == 1920 && value->bit_depth == 12);
            CHECK(value->row_pitch == 4096 && value->buffer_size == 8388608 && value->image_size == 4147200);
            CHECK(value->number_of_bands == 3 && value->pixel_format == AMS_MEL_IR_PIXEL_RGB);
            CHECK(value->sensor_types.size == 2 && value->sensor_types.data[0] == AMS_MEL_IR_SENSOR_GIMBAL_HORIZONTAL);
            CHECK(value->metadata_capabilities.size == 4 && value->metadata_capabilities.data[3] == AMS_MEL_IR_METADATA_RESERVED_10);
            CHECK(value->metadata_capabilities.data[0] == 0 && value->metadata_capabilities.data[1] == 9 && value->metadata_capabilities.data[2] == 17);
            CHECK(value->image_bands.size == 2 && value->image_bands.data[0].band_index == 2);
            CHECK(value->image_bands.data[0].bands.size == 2);
            CHECK(value->image_bands.data[0].bands.data[1].min_wavelength_m == 3.0e-6);
            CHECK(value->image_bands.data[1].band_index == 9);
            CHECK(value->nav_frames.size == 2 && value->nav_frames.data[0] == AMS_MEL_IR_COORDINATE_NED_SENSOR);
            CHECK(text_is(value->platform_id.descriptive_label, "platform-\xE2\x82\xAC"));
            CHECK(value->sensor_location.offset_y_m == -2.5 && value->task_schedule_depth == 17);
            CHECK(value->sensor_location.offset_x_m == 1.25 && value->sensor_location.offset_z_m == 3.75);
            CHECK(text_is(value->sensor_location.key, "sensor-key") && text_is(value->sensor_location.system_name, "system-\xCE\xB2"));
            CHECK(value->odc_available == 1 && value->nuc_available == 1);
        } else {
            CHECK(value->height == 0 && value->width == 0 && value->bit_depth == 0);
            CHECK(value->row_pitch == 0 && value->buffer_size == 0 && value->image_size == 0 && value->number_of_bands == 0);
        }
        if (i + 1 == count) {
            CHECK(ams_mel_ir_capability_list_close(&list, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(text_is(value->channel_id.descriptive_label, label));
            if (rich) CHECK(value->image_bands.data[0].bands.data[1].max_wavelength_m == 5.0e-6);
        }
        CHECK(ams_mel_ir_channel_capability_close(&entry, NULL, 0, NULL) == AMS_MEL_OK);
    }
    CHECK(ams_mel_ir_capability_list_close(&list, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_capability_list_close(&list, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

int main(void)
{
    const char *failures[] = {"discovery-malformed", "discovery-bad-label", "discovery-throw",
        "discovery-unknown-throw", "discovery-bad-alloc"};
    ams_mel_ir_capability_list *list = NULL;
    size_t count = 99;
    CHECK(ams_mel_ir_capability_list_create(NULL, &list, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT && list == NULL);
    CHECK(ams_mel_ir_capability_list_create(NULL, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_capability_list_count(NULL, &count, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT && count == 0);
    CHECK(ams_mel_ir_capability_list_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(discover("discovery-zero", 0, 0) == EXIT_SUCCESS);
    CHECK(discover("discovery-rich", 1, 0) == EXIT_SUCCESS);
    CHECK(discover("discovery-empty", 0, 1) == EXIT_SUCCESS);
    for (size_t i = 0; i < sizeof failures / sizeof failures[0]; ++i) {
        ams_mel_session *session = NULL;
        char diagnostic[128];
        size_t required = 0;
        CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, failures[i], "", &session, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_capability_list_create(session, &list, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT && list == NULL);
        CHECK(ams_mel_ir_capability_list_create(session, &list, diagnostic, sizeof diagnostic, &required) ==
            (i < 2 ? AMS_MEL_PROVIDER_FAILED : i < 4 ? AMS_MEL_PROVIDER_EXCEPTION : AMS_MEL_INTERNAL_ERROR));
        CHECK(list == NULL && diagnostic[0] != '\0' && required > 1);
        CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    }
    return EXIT_SUCCESS;
}
