#define _GNU_SOURCE
#include <ams_mel/abi.h>
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "TxPower line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static ams_mel_rf_data *open_mock(const char *scenario)
{
    ams_mel_rf_data *data = NULL;
    CHECK(ams_mel_rf_data_open(AMS_MEL_TEST_MOCK_RF_PROVIDER, scenario, &data, NULL, 0, NULL) == AMS_MEL_OK);
    return data;
}
static void verify(ams_mel_rf_tx_power_mode_span_v1 v, double delta)
{
    CHECK(v.size == 2 && v.data != NULL);
    const ams_mel_rf_tx_power_mode_v1 *a = &v.data[0], *b = &v.data[1];
    CHECK(a->tx_power_mode_id == UINT32_C(0x80000001) && b->tx_power_mode_id == UINT32_MAX);
    CHECK(a->is_linear_operation == 1 && b->is_linear_operation == 0);
    CHECK(a->tx_power_level == UINT32_C(0xF0E1D2C3) && b->tx_power_level == 0);
    CHECK(a->tx_frequency_ranges.size == 2 && a->tx_frequency_ranges.data != NULL);
    CHECK(b->tx_frequency_ranges.size == 0 && b->tx_frequency_ranges.data == NULL);
    CHECK(a->tx_frequency_ranges.data[0].min_hz == 1000000.25 + delta);
    CHECK(a->tx_frequency_ranges.data[0].max_hz == 2000000.5 + delta);
    CHECK(a->tx_frequency_ranges.data[1].min_hz == 987654321.125 + delta);
    CHECK(a->tx_frequency_ranges.data[1].max_hz == 987654322.875 + delta);
    CHECK(a->max_tx_duty_factor == 0.625 + delta && b->max_tx_duty_factor == 0.375 + delta);
    CHECK(a->max_tx_pulse_width_ns == -123456789 && b->max_tx_pulse_width_ns == INT64_C(9876543210));
    CHECK(a->max_tx_atten == 63.5 && b->max_tx_atten == 12.25);
    CHECK(a->tx_atten_step_size == 0.125 && b->tx_atten_step_size == 0.5);
}
int main(void)
{
    ams_mel_rf_data *data = open_mock("tx-changing");
    ams_mel_rf_tx_power_mode_snapshot *a = NULL, *b = NULL;
    ams_mel_rf_tx_power_mode_span_v1 av = {0}, bv = {0};
    void *pin = dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    unsigned (*collections)(void), (*directs)(void);
    uint32_t (*face)(void), (*id)(void);
    void *symbol = dlsym(pin, "mock_rf_tx_collection_calls"); memcpy(&collections, &symbol, sizeof collections);
    symbol = dlsym(pin, "mock_rf_tx_direct_calls"); memcpy(&directs, &symbol, sizeof directs);
    symbol = dlsym(pin, "mock_rf_tx_face"); memcpy(&face, &symbol, sizeof face);
    symbol = dlsym(pin, "mock_rf_tx_requested_id"); memcpy(&id, &symbol, sizeof id);
    CHECK(pin && collections && directs && face && id);
    CHECK(ams_mel_rf_data_get_tx_power_modes(NULL, 0, &a, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_data_get_tx_power_modes(data, 0, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_data_get_tx_power_modes(data, 0, &a, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_data_get_tx_power_mode(NULL, 0, 0, &a, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_data_get_tx_power_mode(data, 0, 0, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_data_get_tx_power_mode(data, 0, 0, &a, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(collections() == 0 && directs() == 0);
    CHECK(ams_mel_rf_data_get_tx_power_modes(data, UINT32_C(0xFEDCBA98), &a, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(collections() == 1 && directs() == 0 && face() == UINT32_C(0xFEDCBA98));
    CHECK(ams_mel_rf_data_get_tx_power_modes(data, 0, &a, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(collections() == 1);
    CHECK(ams_mel_rf_tx_power_mode_snapshot_view(a, &av, NULL, 0, NULL) == AMS_MEL_OK); verify(av, 0);
    CHECK(ams_mel_rf_data_get_tx_power_modes(data, 0, &b, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(collections() == 2 && face() == 0);
    CHECK(ams_mel_rf_tx_power_mode_snapshot_view(b, &bv, NULL, 0, NULL) == AMS_MEL_OK); verify(bv, 10); verify(av, 0);
    CHECK(ams_mel_rf_tx_power_mode_snapshot_close(&b, NULL, 0, NULL) == AMS_MEL_OK && b == NULL); verify(av, 0);
    CHECK(ams_mel_rf_data_get_tx_power_mode(data, UINT32_C(0xFEDCBA98), UINT32_C(0xDEADBEEF), &b, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(directs() == 1 && collections() == 2 && face() == UINT32_C(0xFEDCBA98) && id() == UINT32_C(0xDEADBEEF));
    CHECK(ams_mel_rf_tx_power_mode_snapshot_view(b, &bv, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(bv.size == 1 && bv.data[0].tx_power_mode_id == UINT32_C(0xDEADBEEF));
    CHECK(ams_mel_rf_data_get_tx_power_mode(data, 0, 0, &b, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(directs() == 1);
    CHECK(ams_mel_rf_tx_power_mode_snapshot_close(&b, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_rf_data_close(&data, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(dlclose(pin) == 0);
    CHECK(dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER, RTLD_NOW | RTLD_NOLOAD) == NULL);
    CHECK(ams_mel_rf_tx_power_mode_snapshot_view(a, &av, NULL, 0, NULL) == AMS_MEL_OK); verify(av, 0);
    CHECK(ams_mel_rf_tx_power_mode_snapshot_view(NULL, &av, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_tx_power_mode_snapshot_view(a, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_tx_power_mode_snapshot_view(a, &av, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_tx_power_mode_snapshot_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_tx_power_mode_snapshot_close(&a, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_tx_power_mode_snapshot_close(&a, NULL, 0, NULL) == AMS_MEL_OK && a == NULL);
    CHECK(ams_mel_rf_tx_power_mode_snapshot_close(&a, NULL, 0, NULL) == AMS_MEL_OK);
    const char *scenarios[] = {"tx-throw", "tx-unknown", "tx-alloc"};
    for (size_t i = 0; i < 3; ++i) {
        char diagnostic[128]; size_t required = 0;
        data = open_mock(scenarios[i]);
        ams_mel_status_t expected = i == 2 ? AMS_MEL_INTERNAL_ERROR : AMS_MEL_PROVIDER_EXCEPTION;
        CHECK(ams_mel_rf_data_get_tx_power_modes(data, 0, &a, diagnostic, sizeof diagnostic, &required) == expected);
        CHECK(a == NULL && diagnostic[0] && required == strlen(diagnostic) + 1);
        CHECK(ams_mel_rf_data_get_tx_power_mode(data, 0, 123, &a, diagnostic, sizeof diagnostic, &required) == expected);
        CHECK(a == NULL && diagnostic[0] && required == strlen(diagnostic) + 1);
        CHECK(ams_mel_rf_data_close(&data, NULL, 0, NULL) == AMS_MEL_OK);
    }
    data = open_mock("tx-empty");
    CHECK(ams_mel_rf_data_get_tx_power_modes(data, 0, &a, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_rf_tx_power_mode_snapshot_view(a, &av, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(av.size == 0 && av.data == NULL);
    CHECK(ams_mel_rf_tx_power_mode_snapshot_close(&a, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_rf_data_close(&data, NULL, 0, NULL) == AMS_MEL_OK);
    data = open_mock("tx-mismatch");
    CHECK(ams_mel_rf_data_get_tx_power_mode(data, UINT32_C(0xFEDCBA98), UINT32_C(0xDEADBEEF), &a, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_rf_tx_power_mode_snapshot_view(a, &av, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(av.size == 1 && av.data[0].tx_power_mode_id == UINT32_C(0x80000001));
    CHECK(ams_mel_rf_tx_power_mode_snapshot_close(&a, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_rf_data_close(&data, NULL, 0, NULL) == AMS_MEL_OK);
    data = open_mock("tx-special");
    CHECK(ams_mel_rf_data_get_tx_power_modes(data, 0, &a, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_rf_tx_power_mode_snapshot_view(a, &av, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(signbit(av.data[0].max_tx_duty_factor) && av.data[0].max_tx_duty_factor == 0);
    CHECK(isinf(av.data[0].max_tx_atten) && av.data[0].max_tx_atten > 0 && isnan(av.data[0].tx_atten_step_size));
    CHECK(ams_mel_rf_tx_power_mode_snapshot_close(&a, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_rf_data_close(&data, NULL, 0, NULL) == AMS_MEL_OK);
    puts("PASS: RF TxPowerModeData contract");
    return 0;
}
