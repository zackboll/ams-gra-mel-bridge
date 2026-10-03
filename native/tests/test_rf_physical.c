#include <ams_mel/abi.h>
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "PhysicalData: line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static ams_mel_rf_data *open_mock(const char *scenario)
{
    ams_mel_rf_data *data = NULL;
    CHECK(ams_mel_rf_data_open(AMS_MEL_TEST_MOCK_RF_PROVIDER, scenario, &data, NULL, 0, NULL) == AMS_MEL_OK);
    return data;
}
static void verify(const ams_mel_rf_physical_data_v1 *v, double d)
{
    const char *key = d == 0 ? "bay-µ-17" : "second-bay-µ-18";
    const char *system = d == 0 ? "mock/β-installation" : "second/β-installation";
    CHECK(v->antenna_height_m == 1.25 + d);
    CHECK(v->antenna_width_m == 2.5 + d);
    CHECK(v->lattice_angle_rad == -0.375 + d);
    CHECK(v->location.offset_x_m == 10.125 + d);
    CHECK(v->location.offset_y_m == -20.25 + d);
    CHECK(v->location.offset_z_m == 30.5 + d);
    CHECK(v->orientation.roll_rad == 0.125 + d);
    CHECK(v->orientation.pitch_rad == -0.25 + d);
    CHECK(v->orientation.yaw_rad == 0.5 + d);
    CHECK(v->boresight.roll_rad == -0.75 + d);
    CHECK(v->boresight.pitch_rad == 1.0 + d);
    CHECK(v->boresight.yaw_rad == -1.25 + d);
    CHECK(v->location.key.size == strlen(key) && memcmp(v->location.key.data, key, strlen(key)) == 0);
    CHECK(v->location.system_name.size == strlen(system) && memcmp(v->location.system_name.data, system, strlen(system)) == 0);
}
int main(void)
{
    ams_mel_rf_data *data = open_mock("physical-changing");
    ams_mel_rf_physical_data *a = NULL, *b = NULL;
    const ams_mel_rf_physical_data_v1 *av = NULL, *bv = NULL;
    void *pin = dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    unsigned (*calls)(void);
    uint32_t (*face)(void);
    CHECK(pin != NULL);
    *(void **)(&calls) = dlsym(pin, "mock_rf_physical_calls");
    *(void **)(&face) = dlsym(pin, "mock_rf_physical_face");
    CHECK(calls != NULL && face != NULL);
    CHECK(ams_mel_rf_data_get_physical_data(NULL, 0, &a, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_data_get_physical_data(data, 0, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_data_get_physical_data(data, 0, &a, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(calls() == 0);
    CHECK(ams_mel_rf_data_get_physical_data(data, UINT32_C(0xFEDCBA98), &a, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(calls() == 1 && face() == UINT32_C(0xFEDCBA98));
    CHECK(ams_mel_rf_data_get_physical_data(data, 0, &a, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(calls() == 1);
    CHECK(ams_mel_rf_physical_data_view(a, &av, NULL, 0, NULL) == AMS_MEL_OK);
    verify(av, 0);
    CHECK(ams_mel_rf_data_get_physical_data(data, 0, &b, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(calls() == 2 && face() == 0);
    CHECK(ams_mel_rf_physical_data_view(b, &bv, NULL, 0, NULL) == AMS_MEL_OK);
    verify(bv, 1); verify(av, 0);
    CHECK(av->location.key.data != bv->location.key.data);
    CHECK(av->location.system_name.data != bv->location.system_name.data);
    CHECK(ams_mel_rf_physical_data_close(&b, NULL, 0, NULL) == AMS_MEL_OK && b == NULL);
    verify(av, 0);
    CHECK(ams_mel_rf_data_close(&data, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(dlclose(pin) == 0);
    pin = dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(pin == NULL);
    CHECK(ams_mel_rf_physical_data_view(a, &av, NULL, 0, NULL) == AMS_MEL_OK);
    verify(av, 0);
    CHECK(ams_mel_rf_physical_data_view(NULL, &av, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_physical_data_view(a, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_physical_data_view(a, &av, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_physical_data_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_physical_data_close(&a, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_physical_data_close(&a, NULL, 0, NULL) == AMS_MEL_OK && a == NULL);
    CHECK(ams_mel_rf_physical_data_close(&a, NULL, 0, NULL) == AMS_MEL_OK);
    {
        const char *scenarios[] = {"physical-key-utf8", "physical-key-nul", "physical-system-utf8", "physical-system-nul", "physical-throw", "physical-unknown", "physical-alloc"};
        for (size_t i = 0; i < sizeof scenarios / sizeof scenarios[0]; ++i) {
            char diagnostic[128] = {0};
            data = open_mock(scenarios[i]);
            CHECK(ams_mel_rf_data_get_physical_data(data, 0, &a, diagnostic, sizeof diagnostic, NULL) ==
                  (i < 4 ? AMS_MEL_PROVIDER_FAILED : i == 6 ? AMS_MEL_INTERNAL_ERROR : AMS_MEL_PROVIDER_EXCEPTION));
            CHECK(a == NULL && diagnostic[0] != '\0');
            CHECK(ams_mel_rf_data_close(&data, NULL, 0, NULL) == AMS_MEL_OK);
        }
    }
    data = open_mock("physical-empty");
    CHECK(ams_mel_rf_data_get_physical_data(data, 0, &a, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_rf_physical_data_view(a, &av, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(av->location.key.size == 0 && av->location.key.data != NULL);
    CHECK(av->location.system_name.size == 0 && av->location.system_name.data != NULL);
    CHECK(ams_mel_rf_physical_data_close(&a, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_rf_data_close(&data, NULL, 0, NULL) == AMS_MEL_OK);
    data = open_mock("physical-special");
    CHECK(ams_mel_rf_data_get_physical_data(data, 0, &a, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_rf_physical_data_view(a, &av, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(signbit(av->antenna_height_m) && av->antenna_height_m == 0);
    CHECK(isinf(av->antenna_width_m) && isnan(av->lattice_angle_rad));
    CHECK(ams_mel_rf_physical_data_close(&a, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_rf_data_close(&data, NULL, 0, NULL) == AMS_MEL_OK);
    puts("PASS: RF PhysicalData contract");
    return 0;
}
