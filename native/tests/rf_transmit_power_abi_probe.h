#ifndef AMS_MEL_RF_TRANSMIT_POWER_ABI_PROBE_H
#define AMS_MEL_RF_TRANSMIT_POWER_ABI_PROBE_H
#include <ams_mel/abi.h>
static void check_rf_transmit_power_signatures(void)
{
    ams_mel_status_t (*radiated)(const ams_mel_rf_virtual_aperture *, uint64_t,
        uint32_t, double, uint64_t, double, double, double, uint32_t, double *,
        char *, size_t, size_t *) = ams_mel_rf_virtual_aperture_get_tx_radiated_power;
    ams_mel_status_t (*peak)(const ams_mel_rf_virtual_aperture *, uint64_t,
        uint32_t, double, double, uint32_t, double *, char *, size_t, size_t *) =
        ams_mel_rf_virtual_aperture_get_tx_peak_radiated_power;
    ams_mel_status_t (*gain)(const ams_mel_rf_virtual_aperture *, uint64_t,
        uint32_t, uint64_t, double, double, double, uint32_t, double *, char *,
        size_t, size_t *) = ams_mel_rf_virtual_aperture_get_tx_aperture_gain;
    ams_mel_status_t (*attenuation)(const ams_mel_rf_virtual_aperture *, uint64_t,
        uint32_t, uint32_t, double *, char *, size_t, size_t *) =
        ams_mel_rf_virtual_aperture_get_max_tx_attenuation;
    (void)radiated; (void)peak; (void)gain; (void)attenuation;
}
#endif
